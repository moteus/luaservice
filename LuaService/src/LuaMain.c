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

#include "luaservice.h"

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


/** Implement the Lua function GetCurrentConfiguration().
 * 
 * Discover the current directory name and return it to the caller.
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
 
    // Open a handle to the service. 
    schManager = OpenSCManagerA(NULL, NULL, (0
    		|GENERIC_READ
    		|SC_MANAGER_CONNECT 
    		|SC_MANAGER_CREATE_SERVICE 
    		|SC_MANAGER_ENUMERATE_SERVICE 
    		) );
    if (schManager == NULL)
    	return luaL_error(L,"OpenSCManager failed (%d)", GetLastError());
    schService = OpenServiceA(schManager, // SCManager database 
			ServiceName, // name of service 
			SERVICE_QUERY_CONFIG); // need QUERY access 
	if (schService == NULL) {
		CloseServiceHandle(schManager);
		return luaL_error(L,"OpenService failed (%d)", GetLastError());
	}
    
    // Allocate a buffer for the configuration information.
 
    lpqscBuf = (LPQUERY_SERVICE_CONFIG) LocalAlloc( 
        LPTR, 8192); 
    if (lpqscBuf == NULL) {
        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);
        return luaL_error(L,"Can't allocate lpqscBuf");
    } 
    lpqscBuf2 = (LPSERVICE_DESCRIPTION) LocalAlloc( 
        LPTR, 8192); 
    if (lpqscBuf2 == NULL) {
        LocalFree(lpqscBuf); 
        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);
        return luaL_error(L,"Can't allocate lpqscBuf2");
    } 
    // Get the configuration information. 
 
    if (! QueryServiceConfig( 
        schService, 
        lpqscBuf, 
        8192, 
        &dwBytesNeeded) )     {
    	
        LocalFree(lpqscBuf); 
        LocalFree(lpqscBuf2); 
        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);
        return luaL_error(L,"QueryServiceConfig failed (%d)", GetLastError());
    }
 
    if (! QueryServiceConfig2( 
        schService, 
        SERVICE_CONFIG_DESCRIPTION,
        (LPBYTE)lpqscBuf2, 
        8192, 
        &dwBytesNeeded) ) {
    	
        LocalFree(lpqscBuf); 
        LocalFree(lpqscBuf2); 
        CloseServiceHandle(schService);
        CloseServiceHandle(schManager);
        return luaL_error(L,"QueryServiceConfig2 failed (%d)", GetLastError());
    }
 
    // Print the configuration information.
 
    SvcDebugTrace("\nSample_Srv configuration: \n",0);
    SvcDebugTrace(" Type: 0x%x\n", lpqscBuf->dwServiceType);
    SvcDebugTrace(" Start Type: 0x%x\n", lpqscBuf->dwStartType);
    SvcDebugTrace(" Error Control: 0x%x\n", lpqscBuf->dwErrorControl);
    //SvcDebugTrace(" Binary path: %s\n", lpqscBuf->lpBinaryPathName);

#if 0
    if (lpqscBuf->lpLoadOrderGroup != NULL)
        printf(" Load order group: %s\n", lpqscBuf->lpLoadOrderGroup);
    if (lpqscBuf->dwTagId != 0)
        printf(" Tag ID: %d\n", lpqscBuf->dwTagId);
    if (lpqscBuf->lpDependencies != NULL)
        printf(" Dependencies: %s\n", lpqscBuf->lpDependencies);
    if (lpqscBuf->lpServiceStartName != NULL)
        printf(" Start Name: %s\n", lpqscBuf->lpServiceStartName);
    if (lpqscBuf2->lpDescription != NULL)
        printf(" Description: %s\n", lpqscBuf2->lpDescription);
#endif
    
    
    LocalFree(lpqscBuf); 
    LocalFree(lpqscBuf2); 
    CloseServiceHandle(schService);
    CloseServiceHandle(schManager);
    return 0;
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
#define local_getreg(L,key) do { 				\
		lua_pushlightuserdata(L,(void*)key);	\
		lua_gettable(L,LUA_REGISTRYINDEX);		\
	} while(0)

/** Store the top of stack in a private key in the registry.
 * 
 * Stores the top of the stack in the registry keyed by a light
 * user data made from the pointer \a key.
 * 
 * \param L The Lua state.
 * \param key A private key in the form of a pointer to something.
 */
#define local_setreg(L,key) do {				\
		lua_pushlightuserdata(L,(void*)key);	\
		lua_insert(L,-2);						\
		lua_settable(L,LUA_REGISTRYINDEX);		\
    } while(0)


/** List of Lua callable functions for the service object.
 * 
 * Each entry creates a single function in the service object.
 */
static const struct luaL_Reg dbgFunctions[] = {
		{"sleep", dbgSleep },
		{"print", dbgPrint },
		{"stopping", dbgStopping },
		{"GetCurrentDirectory", dbgGetCurrentDirectory},
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
	luaL_dostring(L,
			"package.path = string.replace([[@?.lua;@?\\init.lua]],'%@',service.path)\n"
			"package.cpath = string.replace([[@?.dll;@loadall.dll]],'%@',service.path)\n"
			);
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
		// first, release any past results
		lua_pushnil(L);
		local_setreg(L,PENDING_WORK);

		
		// the script file name is always relative to the service folder
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
		//OutputDebugStringA(szPath); 
		status = luaL_loadfile(L,szPath);
		if (status) { 
			return luaL_error(L,"%s\n",lua_tostring(L,-1));
		}
		local_setreg(L,PENDING_WORK);
		return 0;
	} else {
		int n;
		int i;
		
		// call pending code and save any results
		
		// first, release any past results
		lua_pushnil(L);
		local_setreg(L,WORK_RESULTS);

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
		local_getreg(L,PENDING_WORK);
		status = lua_pcall(L, 0, LUA_MULTRET, -2);
#endif
		if (status) { 
			return luaL_error(L,"%s\n",lua_tostring(L,-1));
		}
		lua_createtable(L, lua_gettop(L)-n, 0);
		for (i=lua_gettop(L); i>n+1; --i)
			lua_rawseti(L,n+1,i);
		local_setreg(L,WORK_RESULTS);
		return 0;
	}
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
LUAHANDLE LuaWorkerLoad(LUAHANDLE h, char *cmd)
{
	int status;
	lua_State *L=(lua_State*)h;
	
	if (!h)
		L = lua_open(); 
	status = lua_cpcall(L, &pmain, (void*)cmd);
	if (status) {
		SvcDebugTrace("Script cpcall status %d", status);
		SvcDebugTrace((char *)lua_tostring(L,-1),0);
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
		SvcDebugTrace("Script cpcall status %d", status);
		SvcDebugTrace((char *)lua_tostring(L,-1),0);
	} else {
		SvcDebugTrace("Script succeeded", 0);
	}
	return (void *)L;
}

/**
 */
void LuaWorkerCleanup(LUAHANDLE h)
{
	lua_State *L=(lua_State*)h;
	if (h)
		lua_close(L);
}