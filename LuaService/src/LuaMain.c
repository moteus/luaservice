/** \file LuaMain.c
 *  \brief Wrap up access to Lua interpretor states
 */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <windows.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <assert.h>

#include "luaservice.h"

#include <time.h>

/** Implement the Lua function sleep(ms).
 * 
 * Call the Windows Sleep() API to delay thread execution for 
 * approximately \a ms ms.
 * 
 * \param L Lua state context for the function.
 * \returns The number of values on the Lua stack to be returned
 * to the Lua caller.
 */
static int dbgSleep(lua_State *L)
{
    int t;
    t = luaL_checkint(L,1);
    if (t < 0) t = 0;
    Sleep((DWORD)t);
    return 0;
}

/** Implement the Lua function print(...).
 * 
 * Construct a message from all the arguments to print(), passing
 * each through the global function tostring() make certain they 
 * are strings, and separating them with tab characters. The message
 * is ultimately passed to the Windows service OutputDebugString()
 * for display in a debugger or debug message logger.
 * 
 * \param L Lua state context for the function.
 * \returns The number of values on the Lua stack to be returned
 * to the Lua caller.
 */
static int dbgPrint(lua_State *L) 
{
    luaL_Buffer b;
    int n = lua_gettop(L); /* number of arguments */
    int i;
    lua_getglobal(L, "tostring");
    luaL_buffinit(L, &b);
    for (i=1; i<=n; i++) {
        lua_pushvalue(L, n+1); /* b tostring */
        lua_pushvalue(L, i);   /* b tostring argi */
        lua_call(L, 1, 1);     /* b tostring(argi) */
        luaL_addvalue(&b);     /* b */
        if (i<n)
            luaL_addchar(&b, '\t');
    }
    luaL_pushresult(&b);
    OutputDebugStringA(lua_tostring(L, -1)); 		//fputs(s, stdout);
    lua_pop(L,1);
    return 0;
}

/** Implement the Lua function GetCurrentDirectory().
 * 
 * Discover the current directory name and return it to the caller.
 * 
 * \todo There is a low-probability memory leak here. The buffer used 
 * to hold the current directory string came from malloc() and is held 
 * across a call to lua_pushlstring() which can potentially throw an
 * error, which will leak the allocated buffer. The other bits of Win32
 * API wrappers could have similar issues, and should be inspected.
 * 
 * \param L Lua state context for the function.
 * \returns The number of values on the Lua stack to be returned
 * to the Lua caller.
 */
static int dbgGetCurrentDirectory(lua_State *L)
{
    char *buf = 0;
    DWORD len = 0;
    len = GetCurrentDirectoryA(0,NULL);
    if (!len)
        return luaL_error(L, "GetCurrentDirectory failed (%d)", GetLastError());
    buf = malloc(len+1);
    if (!buf)
        return luaL_error(L,"GetCurrentDirectory can't allocate %ld chars", len);
    GetCurrentDirectoryA(len+1, buf);
    lua_pushlstring(L, buf, len);
    free(buf);
    return 1;
}

/** Implement the Lua function SetCurrentDirectory().
 * 
 * Set current work directory for process.
 * 
 * \param L Lua state context for the function.
 * \param path string represent new work directory.
 * \returns true or rize error
 */
static int dbgSetCurrentDirectory(lua_State *L)
{
    BOOL ret = SetCurrentDirectoryA(luaL_checkstring(L, 1));
    if (!ret)
        return luaL_error(L, "SetCurrentDirectory failed (%d)", GetLastError());
    lua_pushboolean(L, 1);
    return 1;
}

/** Implement the Lua function stopping().
 * 
 * Poll the flag used by the request handler thread to signal that
 * the service should politely halt soon. Returns true if it should
 * shut down, false otherwise.
 * 
 * \param L Lua state context for the function.
 * \returns The number of values on the Lua stack to be returned
 * to the Lua caller.
 */
static int dbgStopping(lua_State *L)
{
    lua_pushboolean(L,ServiceStopping);
    return 1;
}


/** Implement the Lua function tracelevel(level).
 * 
 * Control the verbosity of trace output to the debug console.
 * 
 * If level is passed, sets the trace level accordingly. Regardless,
 * it returns the current trace level.
 * 
 * \param L Lua state context for the function.
 * \returns The number of values on the Lua stack to be returned
 * to the Lua caller.
 */
static int dbgTracelevel(lua_State *L)
{
    SvcDebugTraceLevel = luaL_optint(L,-1,SvcDebugTraceLevel);
    lua_pushinteger(L,SvcDebugTraceLevel);
    return 1;
}

/** Utility macro to add a field to the table at the top of stack.
 * Note this assumes that a table is currently at the top of the
 * stack. If that is not true, then this macro probably causes 
 * Lua to throw an error.
 * 
 * \param f String literal field name.
 * \param n Integer field value
 */
#define fieldint(f,n) do{			\
        lua_pushinteger(L,(n));			\
        lua_setfield(L,-2,f);			\
        SvcDebugTrace("  " f ": 0x%x\n", (n));	\
    }while(0)

/** Utility macro to add a field to the table at the top of stack.
 * Note this assumes that a table is currently at the top of the
 * stack. If that is not true, then this macro probably causes 
 * Lua to throw an error.
 * 
 * \param f String literal field name.
 * \param s String field value
 */
#define fieldstr(f,s) do{			\
        lua_pushstring(L,(s));			\
        lua_setfield(L,-2,f);			\
    SvcDebugTraceStr("  " f ": %s\n", (s));	\
    }while(0)

/** Implement the Lua function GetCurrentConfiguration().
 * 
 * Discover some details about the service's configuration as 
 * known to the \ref ssSCM and report them to the debug trace
 * while building a table from them to return.
 * 
 * \param L Lua state context for the function.
 * \returns The number of values on the Lua stack to be returned
 * to the Lua caller.
 */
static int dbgGetCurrentConfiguration(lua_State *L)
{
    SC_HANDLE schService;
    SC_HANDLE schManager;
    LPQUERY_SERVICE_CONFIG lpqscBuf;
    LPSERVICE_DESCRIPTION lpqscBuf2;
    DWORD dwBytesNeeded;
    const char *name;

    name = luaL_optstring(L, 1, ServiceName);
    SvcDebugTraceStr("Get service configuration for %s:\n", name);
    
    // Open a handle to the service. 
    schManager = OpenSCManagerA(NULL, NULL, (0
        |GENERIC_READ
        |SC_MANAGER_CONNECT
        |SC_MANAGER_CREATE_SERVICE
        |SC_MANAGER_ENUMERATE_SERVICE
    ));
    if (schManager == NULL)
    return luaL_error(L, "OpenSCManager failed (%d)", GetLastError());
    schService = OpenServiceA(schManager, // SCManager database 
        name, // name of service 
        SERVICE_QUERY_CONFIG); // need QUERY access 
    if (schService == NULL) {
        CloseServiceHandle(schManager);
        return luaL_error(L, "OpenService failed (%d)", GetLastError());
    }

    // Allocate buffers for the configuration information.
    lpqscBuf = (LPQUERY_SERVICE_CONFIG) LocalAlloc(LPTR, 8192);
    if (lpqscBuf == NULL) {
        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);
        return luaL_error(L, "Can't allocate lpqscBuf");
    }
    lpqscBuf2 = (LPSERVICE_DESCRIPTION) LocalAlloc(LPTR, 8192);
    if (lpqscBuf2 == NULL) {
        LocalFree(lpqscBuf);
        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);
        return luaL_error(L, "Can't allocate lpqscBuf2");
    }
    
    // Get the configuration information. 
    if (! QueryServiceConfig(
        schService,
        lpqscBuf,
        8192,
        &dwBytesNeeded)) {
        LocalFree(lpqscBuf);
        LocalFree(lpqscBuf2);
        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);
        return luaL_error(L, "QueryServiceConfig failed (%d)",
        GetLastError());
    }
    if (! QueryServiceConfig2(
        schService,
        SERVICE_CONFIG_DESCRIPTION,
        (LPBYTE)lpqscBuf2,
        8192,
        &dwBytesNeeded)) {
        LocalFree(lpqscBuf);
        LocalFree(lpqscBuf2);
        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);
        return luaL_error(L, "QueryServiceConfig2 failed (%d)",
        GetLastError());
    }

    // Build a table of configuration details, 
    // passing them to the trace log along the way

    lua_newtable(L);
    fieldstr("name", name);
    fieldint("ServiceType", lpqscBuf->dwServiceType);
    fieldint("StartType", lpqscBuf->dwStartType);
    fieldint("ErrorControl", lpqscBuf->dwErrorControl);
    fieldstr("BinaryPathName", lpqscBuf->lpBinaryPathName);
    if (lpqscBuf->lpLoadOrderGroup != NULL)
        fieldstr("LoadOrderGroup", lpqscBuf->lpLoadOrderGroup);
    if (lpqscBuf->dwTagId != 0)
        fieldint("TagId", lpqscBuf->dwTagId);
    if (lpqscBuf->lpDependencies != NULL)
        fieldstr("Dependencies", lpqscBuf->lpDependencies);
    if (lpqscBuf->lpServiceStartName != NULL)
        fieldstr("ServiceStartName", lpqscBuf->lpServiceStartName);
    if (lpqscBuf2->lpDescription != NULL)
        fieldstr("Description", lpqscBuf2->lpDescription);
    
    LocalFree(lpqscBuf);
    LocalFree(lpqscBuf2);
    CloseServiceHandle(schService);
    CloseServiceHandle(schManager);
    return 1;
}

/** Private key for a pending compiled but unexecuted Lua chunk. */
static const char *PENDING_WORK = "Pending Work";
/** Private key for results from a lua chunk. */
static const char *WORK_RESULTS= "Work Results";

/** Put a private-keyed registry item on the top of stack.
 * 
 * Retrives the item from the registry keyed by a light user data
 * made from the pointer \a key and pushes it on the stack.
 * 
 * \param L The Lua state.
 * \param key A private key in the form of a pointer to something.
 */
#define local_getreg(L,key) do {			\
        lua_pushlightuserdata(L,(void*)key);	\
        lua_gettable(L,LUA_REGISTRYINDEX);	\
    } while(0)

/** Store the top of stack in a private key in the registry.
 * 
 * Stores the top of the stack in the registry keyed by a light
 * user data made from the pointer \a key.
 * 
 * \param L The Lua state.
 * \param key A private key in the form of a pointer to something.
 */
#define local_setreg(L,key) do {			\
        lua_pushlightuserdata(L,(void*)key);	\
        lua_insert(L,-2);			\
        lua_settable(L,LUA_REGISTRYINDEX);	\
    } while(0)


/** List of Lua callable functions for the service object.
 * 
 * Each entry creates a single function in the service object.
 */
static const struct luaL_Reg dbgFunctions[] = {
        {"sleep", dbgSleep },
        {"print", dbgPrint },
        {"stopping", dbgStopping },
        {"tracelevel", dbgTracelevel },
        {"GetCurrentDirectory", dbgGetCurrentDirectory},
        {"SetCurrentDirectory", dbgSetCurrentDirectory},
        {"GetCurrentConfiguration", dbgGetCurrentConfiguration},
        {NULL, NULL},
};

/** Initialize useful Lua globals.
 * 
 * The following globals are created in the Lua state:
 * 
 * - service -- a table for the service
 * - service.name		-- the service name known to the SCM
 * - service.filename	-- a string containing the filename of the service program
 * - service.path		-- the path of the service folder
 * - service.sleep(ms)	-- a function to sleep for \a ms ms
 * - service.print(...) -- like standalone Lua's print(), but with OutputDebugString()
 * - print -- a copy of service.print
 * - sleep -- a copy of service.sleep
 * 
 * The global table package has its path and cpath replaced to reference only 
 * the service folder itself. 
 * 
 * \param L Lua state context to get the globals.
 */
static void initGlobals(lua_State *L)
{
    char szPath[MAX_PATH + 1];
    char *cp;
    
    lua_newtable(L);
    GetModuleFileName(GetModuleHandle(NULL), szPath, MAX_PATH);
    lua_pushstring(L,szPath);
    lua_setfield(L,-2,"filename");
    cp = strrchr(szPath, '\\');
    if (cp) {
        cp[1] = '\0';
        lua_pushstring(L,szPath);
        lua_setfield(L,-2,"path");
    }
    lua_pushstring(L,ServiceName);
    lua_setfield(L,-2,"name");
    // define a few useful utility functions
    luaL_register(L, NULL, dbgFunctions);
    lua_setglobal(L, "service");
#if 0
    luaL_dostring(L,
            "package.path = string.replace([[@?.lua;@?\\init.lua]],'%@',service.path)\n"
            "package.cpath = string.replace([[@?.dll;@loadall.dll]],'%@',service.path)\n"
            );
#endif
    luaL_dostring(L,
            "print = service.print\n"
            "sleep = service.sleep\n");
}

/** Function called in a protected Lua state.
 * 
 * Initialize the Lua state if the global service has not been defined,
 * then do something. This function assumes that the caller is living 
 * within the constraints of lua_cpcall(), meaning that it is passed 
 * exactly one argument on the Lua stack which is a light userdata 
 * wrapping an opaque pointer, and it isn't allowed to return anything.
 * 
 * That pointer must be either NULL or a pointer to a C string naming the
 * script file or code fragment to load and execute in the Lua context.
 * 
 * If a NULL is passed, the private registry value PENDING_WORK is pushed
 * and called. Any results are collected in a table and stored in the
 * registry at the private key WORK_RESULTS, and the value at PENDING_WORK
 * is freed to the the garbage collector.
 * 
 * If a non-null string is passed, it is loaded (but not called) and 
 * stored in the registry at the private key PENDING_WORK. Any prevous
 * work results are released to the the garbage collector.
 * 
 * A string is assumed to be a file name, but a future version probably 
 * should allow for a litteral script as well.
 *  
 * \param L Lua state context for the function.
 * \returns The number of values on the Lua stack to be returned
 * to the Lua caller.
 */ 
static int pmain(lua_State *L)
{
    char szPath[MAX_PATH+1];
    char *cp;
    char *arg;
    int status;

    arg = (char *)lua_touserdata(L,-1);
    lua_getglobal(L, "service");
    if (!lua_toboolean(L,-1)) {
        lua_gc(L, LUA_GCSTOP, 0); /* stop gc during initialization */
        luaL_openlibs(L); /* open libraries */
        initGlobals(L);
        lua_gc(L, LUA_GCRESTART, 0);
    }
    lua_pop(L,2); /* don't need the light userdata or service objects on the stack */
    if (arg) {
        // load but don't call the code
        
        // first, release any past results
        lua_pushnil(L);
        local_setreg(L,WORK_RESULTS);
        lua_pushnil(L);
        local_setreg(L,PENDING_WORK);
        
        /**
         * \note The script file name is always relative to the 
         * service folder. This protects against substitution of
         * the script by a third party, at least to some degree.
         */
        GetModuleFileName(GetModuleHandle(NULL), szPath, MAX_PATH);
        cp = strrchr(szPath, '\\');
        if (cp) {
            cp[1] = '\0';
            if ((cp - szPath) + strlen(arg) + 1 > MAX_PATH)
                return luaL_error(L, "Script name '%s%s' too long", szPath, arg);
            strcpy(cp+1, arg);
        } else {
            return luaL_error(L, "Module name '%s' isn't fully qualified", szPath);
        }
        SvcDebugTraceStr("Script: %s\n", szPath); 
        status = luaL_loadfile(L,szPath);
        if (status) { 
            return luaL_error(L,"%s\n",lua_tostring(L,-1));
        }
        local_setreg(L,PENDING_WORK);
        return 0;
    } else {
        int n;
        int i;
        int results;
        
        // call pending code and save any results
        
        // first, release any past results
        lua_pushnil(L);
        local_setreg(L,WORK_RESULTS);

        lua_createtable(L, 5, 0);
        results = lua_gettop(L);
//#define NO_DEBUG_TRACEBACK
#ifdef NO_DEBUG_TRACEBACK
        n = lua_gettop(L);
        local_getreg(L,PENDING_WORK);
        status = lua_pcall(L,0,LUA_MULTRET,0);
#else
        lua_getglobal(L,"debug");
        // debug
        lua_getfield(L, -1, "traceback"); // debug debug.traceback
        lua_remove(L, -2); // debug.traceback
        n = lua_gettop(L);
        local_getreg(L,PENDING_WORK); // debug.traceback function
        if (lua_type(L,-1) != LUA_TFUNCTION)
            return luaL_error(L,"No pending work function to run");
        status = lua_pcall(L, 0, LUA_MULTRET, -2); // debug.traceback ...
#endif
        if (status) { 
            return luaL_error(L,"%s\n",lua_tostring(L,-1));
        }
        SvcDebugTrace("Saved work result count: %d", lua_gettop(L) - n);
        for (i=lua_gettop(L); i>n; --i) {
            SvcDebugTraceStr("item: %s", lua_typename(L,lua_type(L,-1)));
            lua_rawseti(L,results,i-n);
        }
        assert(lua_gettop(L) == n);
        if (lua_gettop(L) != results)
            lua_settop(L, results);
        assert(lua_type(L,-1) == LUA_TTABLE);
        local_setreg(L,WORK_RESULTS);
        return 0;
    }
}

/** Lua allocator function.
 * 
 * Borrowed verbatim from the Lua sources, found in lauxlib.c.
 * 
 * Needed to support a direct creation of a Lua state that cannot 
 * refer in any way to stdio handles.
 * 
 * Manage a memory block. If \a nsize is non-zero, it will return a
 * pointer to allocated memory that must be passed back here to be
 * freed. If \a osize is non-zero, \a ptr must be non-null and the 
 * block it points to will be either freed or reallocated depending
 * on the value of \a nsize.
 * 
 * \note This is a good place to introduce memory performance hooks 
 * for a Lua state in some future version.
 * 
 * \param ud	Opaque token provided when the Lua state was created.
 * \param ptr	Pointer to any existing memory for this transaction.
 * \param osize	Size of the existing memory block.
 * \param nsize	Size of the memory block needed.
 */
static void *LuaAlloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    void *retv = NULL;
    (void)ud;
    (void)osize;
    if (nsize == 0) {
        free(ptr);
        retv = NULL;
    }
#ifdef USE_ONLY_MALLOC
    else if (osize >= nsize)
        retv = ptr;
    else {
        void *p = malloc(nsize);
        if (!p) 
            retv = NULL;
        else {
            memcpy(p,ptr,osize);
            retv = p;
        }
    }
#else
    else
        retv = realloc(ptr, nsize);
#endif
//#define LOG_ALLOCATIONS
#ifdef LOG_ALLOCATIONS
    {
        //static clock_t basetime;
        FILE *fp = fopen("alloc.log", "at");
        assert(fp);
        //if (basetime == 0)
        //	basetime = clock();
        fprintf(fp, "%ld %p %d %d %p\n", clock(), ptr, osize, nsize,
                retv);
        fclose(fp);
    }
#endif
    return retv;
}

/** The panic function for a Lua state.
 * 
 * This function is called as a last resort if an error is thrown
 * in an unprotected Lua context. It has access to an error message
 * on the top of the Lua stack, but should probably refrain from
 * anything that could throw additional errors, say be attempting
 * to allocate additional memory.
 * 
 * If it returns, Lua is going to call exit(EXIT_FAILURE). Since 
 * this is almost certainly running in a thread other than the main
 * thread and, worse, under the supervision of the \ref ssSCM that
 * could result in the SCM becoming confused about the current state
 * of the service. 
 * 
 * To prevent SCM confusion, this function simply calls ExitThread() 
 * to kill the current thread without necessarily killing the whole
 * process.
 * 
 * \todo Should panic() also tell the SCM SERVICE_STOPPED?
 */
static int panic (lua_State *L) {
  (void)L;  /* to avoid warnings */
  SvcDebugTrace("PANIC: unprotected error in call to Lua API...",0);
  SvcDebugTrace(lua_tostring(L, -1), 0);
  ExitThread(EXIT_FAILURE);
  return 0;
}

/** Create a Lua state with a script loaded.
 * 
 * Creates a new Lua state, initializes it with built-in modules
 * and global variables, then loads the specified script or command.
 * 
 * If all is well, the compiled but as yet unexecuted main block 
 * of the script is cached in the Lua Registry at index PENDING_WORK
 * (a light user data made from the address of this function).
 * 
 * \param h Opaque handle to the Lua state to use, or 
 *          NULL to create a new state.
 * \param cmd Script or statement to load
 * \returns An opaque handle identifying the created Lua state.
 */
LUAHANDLE LuaWorkerLoad(LUAHANDLE h, const char *cmd)
{
    int status;
    lua_State *L=(lua_State*)h;
    
    if (!h) {
#if 0
        L = luaL_newstate();
#else
        L = lua_newstate(LuaAlloc, NULL);
#endif
        assert(L);
        lua_atpanic(L, &panic); 
    }
    status = lua_cpcall(L, &pmain, (void*)cmd);
    if (status) {
        SvcDebugTrace("Load script cpcall status %d", status);
        SvcDebugTrace((char *)lua_tostring(L,-1),0);
        //return NULL; 
    } else {
        SvcDebugTrace("Script loaded ok", 0);
    }
    return (void *)L;
}

/** Run a pending Lua script.
 * 
 * The script must have been previously loaded and saved in the Lua registry.
 * 
 * \param h An opaque handle returned by a previous call to LuaWorkerRun().
 * \returns An opaque handle identifying the created Lua state.
 */
LUAHANDLE LuaWorkerRun(LUAHANDLE h)
{
    int status;
    lua_State *L=(lua_State*)h;
    
    if (!h) {
        SvcDebugTrace("No existing lua state!!!", 0);
        return NULL;
        //L = lua_open(); 
    }
    status = lua_cpcall(L, &pmain, NULL);
    if (status) {
        SvcDebugTrace("Run script cpcall status %d", status);
        SvcDebugTrace((char *)lua_tostring(L,-1),0);
    } else {
        SvcDebugTrace("Script succeeded", 0);
    }
    return (void *)L;
}

/** Clean up after the worker by closing the Lua state.
 * 
 * \param h An opaque handle returned by a previous call to LuaWorkerRun().
 */
void LuaWorkerCleanup(LUAHANDLE h)
{
    lua_State *L=(lua_State*)h;
    if (h)
        lua_close(L);
}

/** Get a cached worker result item as a string.
 * 
 * \note The string returned came from strdup(), and must be 
 * freed by the caller.
 * 
 * \param h An opaque handle returned by a previous call to LuaWorkerRun().
 * \param item The index of the result item to retrieve. 
 * The first result is index 1, consistent with Lua counting.
 * \returns The string value (from strdup()) or NULL if the item
 * doesn't exist or can't be converted to a string.
 */
char *LuaResultString(LUAHANDLE h, int item)
{
    char *ret = NULL;
    lua_State *L = (lua_State*)h;
    if (!h) return NULL;
    local_getreg(L,WORK_RESULTS);
    if (lua_type(L,-1) != LUA_TTABLE) {
        lua_pop(L,1);
        return NULL;
    }
    lua_rawgeti(L,-1,item);
    ret = (char *)lua_tostring(L,-1);
    if (ret)
        ret = strdup(ret);
    lua_pop(L,2);
    return ret;
}


/** Get a cached worker result item as an integer.
 * 
 * 
 * \param h An opaque handle returned by a previous call to LuaWorkerRun().
 * \param item The index of the result item to retrieve. 
 * The first result is index 1, consistent with Lua counting.
 * \returns The integer value or 0 if the item
 * doesn't exist or can't be converted to a number.
 */
int LuaResultInt(LUAHANDLE h, int item)
{
    int ret = 0;
    lua_State *L = (lua_State*)h;
    if (!h) return 0;
    local_getreg(L,WORK_RESULTS);
    if (lua_type(L,-1) != LUA_TTABLE) {
        lua_pop(L,1);
        return 0;
    }
    lua_rawgeti(L,-1,item);
    ret = (int)lua_tointeger(L,-1);
    lua_pop(L,2);
    return ret;
}

/** Get a field of a cached worker result item as a string.
 * 
 * \note The string returned came from strdup(), and must be 
 * freed by the caller.
 * 
 * \param h An opaque handle returned by a previous call to LuaWorkerRun().
 * \param item The index of the result item to retrieve. 
 * The first result is index 1, consistent with Lua counting.
 * \param field The name of the field to retrieve.
 * \returns The string value (from strdup()) or NULL if the 
 * field or item doesn't exist or can't be converted to a string.
 */
char *LuaResultFieldString(LUAHANDLE h, int item, const char *field)
{
    char *ret = NULL;
    lua_State *L = (lua_State*)h;
    if (!h) return NULL;
    local_getreg(L,WORK_RESULTS);	// table
    if (lua_type(L,-1) != LUA_TTABLE) {
        lua_pop(L,1);
        return NULL;
    }
    lua_rawgeti(L,-1,item);			// table itemtable
    if (lua_type(L,-1) != LUA_TTABLE) {
        lua_pop(L,2);
        return NULL;
    }
    lua_getfield(L,-1,field);		// table itemtable fieldvalue
    ret = (char *)lua_tostring(L,-1);
    if (ret)
        ret = strdup(ret);
    lua_pop(L,3);
    return ret;
}


/** Get a field of a cached worker result item as an integer.
 * 
 * 
 * \param h An opaque handle returned by a previous call to LuaWorkerRun().
 * \param item The index of the result item to retrieve. 
 * The first result is index 1, consistent with Lua counting.
 * \param field The name of the field to retrieve.
 * \returns The integer value or 0 if the field or item
 * doesn't exist or can't be converted to a number.
 */
int LuaResultFieldInt(LUAHANDLE h, int item, const char *field)
{
    int ret = 0;
    lua_State *L = (lua_State*)h;
    if (!h) return 0;
    local_getreg(L,WORK_RESULTS);	// table
    if (lua_type(L,-1) != LUA_TTABLE) {
        lua_pop(L,1);
        return 0;
    }
    lua_rawgeti(L,-1,item);			// table itemtable
    if (lua_type(L,-1) != LUA_TTABLE) {
        lua_pop(L,2);
        return 0;
    }
    lua_getfield(L,-1,field);		// table itemtable fieldvalue
    ret = (int)lua_tointeger(L,-1);
    lua_pop(L,3);
    return ret;
}
