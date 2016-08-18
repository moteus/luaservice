/*! 
 * \file LuaService.c
 * \brief Windows Service framework and startup.
 * 
 * \author Ross Berteig
 * \author Cheshire Engineering Corp.
 * 
 * Copyright (c) 2007, Ross Berteig, Cheshire Engineering Corp.
 * Licensed under the MIT license, see \ref license for the details.
 * 
 * \todo Supporting service PAUSE and CONTINUE control request will
 * require some effort beyond the bare framework guarded by the 
 * undefined macro LUASERVICE_CAN_PAUSE_CONTINUE. At minimum, 
 * some mechanism must be provided for the Lua side to become
 * aware of the request and actually pause; presumably an Event
 * could be waited on to implement the pause, and signaled to 
 * implement continue. However, since we assume that the Lua 
 * interpreter itself is not built for threading, we don't have
 * a good means to asynchronously notify the Lua code of the 
 * pause request in the first place, which would imply that the
 * Lua code is constantly polling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "luaservice.h"

/** Service name.
 * 
 * This string must be unique in the installed system because it 
 * is used to identify the service to the SCM. It will appear in
 * the Service control panel, and in other spots where an end-user
 * might see it.
 * 
 * \note This value may be configured for a specific installation 
 * of this framework by writing a lua script named init.lua that
 * returns a table with a field <code>name</code>. The init.lua 
 * script must be located in the same folder as LuaService.exe.
 */
const char *ServiceName = "LuaService";

/** Display service name.
 *
 * \note This value may be configured for a specific installation 
 * of this framework by writing a lua script named init.lua that
 * returns a table with a field <code>display_name</code>. The init.lua 
 * script must be located in the same folder as LuaService.exe.
 */
const char *ServiceDisplayName = NULL;

/** Service launcher script.
 * 
 * This string names the Lua script that acts as the main entry
 * point of the service worker thread. This script must be located
 * inside the service's folder or a sub-folder.
 *
 * \note This value may be configured for a specific installation 
 * of this framework by writing a lua script named init.lua that
 * returns a table with a field <code>script</code>. The init.lua 
 * script must be located in the same folder as LuaService.exe.
 */
const char *ServiceScript = "service.lua";

/** Additional info for package.path
 *
 * \note This value may be configured for a specific installation 
 * of this framework by writing a lua script named init.lua that
 * returns a table with a field <code>lua_path</code>. The init.lua 
 * script must be located in the same folder as LuaService.exe.
 */
const char *LuaPackagePath = NULL;

/** Additional info for package.cpath
 *
 * \note This value may be configured for a specific installation 
 * of this framework by writing a lua script named init.lua that
 * returns a table with a field <code>lua_cpath</code>. The init.lua 
 * script must be located in the same folder as LuaService.exe.
 */
const char *LuaPackageCPath = NULL;

/** Current service status.
 * 
 * \context 
 * Service main and worker threads
 */
SERVICE_STATUS LuaServiceStatus;

/** Handle to the SCM for the running service to report status.
 * 
 * This is global because it is discovered by the worker thread,
 * and needed by the thread in which the control request handler
 * executes, which is apparently (but not particularly documented) 
 * the main thread.
 * 
 * \context 
 * Service main and worker threads
 */
SERVICE_STATUS_HANDLE LuaServiceStatusHandle;

/** Handle to the thread executing ServiceMain().
 * 
 * This will be initialized by ServiceMain() as a duplicate of
 * the thread handle, for use in the main() thread so that it has
 * a kernel object on which it can wait for the service thread 
 * to have exited when stopping the service.
 * 
 * Since it was created as a duplicate, it must be closed.
 */
HANDLE ServiceWorkerThread;

/** Trace level.
 * Controls the verbosity of the trace output. The level is tested
 * before any work has been done to format the output, so it is
 * reasonably effective to turn tracing off by setting the level
 * to zero.
 * 
 * Values range from zero (no tracing) and up.
 */
int SvcDebugTraceLevel = 0;

/** Service Stopping Flag.
 * 
 * Set in the service control request handler to indicate that 
 * a STOP request has been received and that the SCM is being
 * informed that the service is now SERVICE_STOP_PENDING.
 * 
 * About 25 seconds after setting this flag, the service will
 * forcefully die with or without cooperation from the worker
 * thread.
 * 
 * The worker can test this flag from Lua by calling the 
 * function service.stopping().
 */
volatile int ServiceStopping = 0;

/** Output a debug string.
 * 
 * The string is formatted and output only if SvcDebugTraceLevel is 
 * greater than zero.
 * 
 * If SvcDebugTraceLevel is 2 or greater, the name of the service will
 * be included in the output.
 * 
 * If SvcDebugTraceLevel is 3 or greater, the current process and thread
 * ids will be included in the output in addition to the service name.
 * 
 * \context 
 * Service, Configuration, Control
 *  
 * \param fmt	A printf()-like format string with an optional reference to
 * 		a single DWORD value.
 * \param dw	A DWORD value to substitute in the message.
 */
void SvcDebugTrace(LPCSTR fmt, DWORD dw)
{
    char Buffer[1024];
    char *cp = Buffer;

    if (SvcDebugTraceLevel <= 0)
        return;
    if (SvcDebugTraceLevel == 2)
        cp += sprintf(Buffer, "[%s] ", ServiceName);
    else if (SvcDebugTraceLevel >= 3)
        cp += sprintf(Buffer, "[%s:%ld/%ld] ", ServiceName,
                GetCurrentProcessId(), GetCurrentThreadId());
    if (fmt == NULL) {
        strcpy(cp, "-nil-");
        OutputDebugStringA(Buffer);
    } else if ((strlen(fmt)+12) < (sizeof(Buffer) - (cp - Buffer))) {
        sprintf(cp, fmt, dw);
        OutputDebugStringA(Buffer);
    } else
        OutputDebugStringA("--buffer overflow--");
}

/** Output a debug string.
 * 
 * The string is formatted and output only if SvcDebugTraceLevel is 
 * greater than zero.
 * 
 * If SvcDebugTraceLevel is 2 or greater, the name of the service will
 * be included in the output.
 * 
 * If SvcDebugTraceLevel is 3 or greater, the current process and thread
 * ids will be included in the output in addition to the service name.
 * 
 * \context 
 * Service, Configuration, Control
 *  
 * \param fmt   A printf()-like format string with an optional reference to
 *              a single string value.
 * \param s     A string value to substitute in the message.
 */
void SvcDebugTraceStr(LPCSTR fmt, LPCSTR s)
{
    char Buffer[1024];
    char *cp = Buffer;

    if (SvcDebugTraceLevel <= 0)
        return;
    if (SvcDebugTraceLevel == 2)
        cp += sprintf(Buffer, "[%s] ", ServiceName);
    else if (SvcDebugTraceLevel >= 3)
        cp += sprintf(Buffer, "[%s:%ld/%ld] ", ServiceName,
                GetCurrentProcessId(), GetCurrentThreadId());
    if (s == NULL)
        s = "-nil-";
    if (fmt == NULL)
        fmt = "-nil-";
    if ((strlen(fmt)+strlen(s)) < sizeof(Buffer) - (cp - Buffer)) {
        sprintf(cp, fmt, s);
        OutputDebugStringA(Buffer);
    } else
        OutputDebugStringA("--buffer overflow--");
}

/** Service Control Handler.
 * 
 * Called in the main thread when the SCM needs to deliver a
 * status or control request to the service.
 * 
 * \context
 * Service main thread
 * 
 * \param Opcode The control operation to handle.
 * 
 * \see ssSvc
 */
void WINAPI LuaServiceCtrlHandler(DWORD Opcode)
{
    DWORD status;

    SvcDebugTrace("Entered LuaServiceCtrlHandler(%d)\n", Opcode);
    switch (Opcode) {
#ifdef LUASERVICE_CAN_PAUSE_CONTINUE
    case SERVICE_CONTROL_PAUSE:
    // Do whatever it takes to pause here. 
    LuaServiceStatus.dwCurrentState = SERVICE_PAUSED;
    break;

    case SERVICE_CONTROL_CONTINUE:
    // Do whatever it takes to continue here. 
    LuaServiceStatus.dwCurrentState = SERVICE_RUNNING;
    break;
#endif
    case SERVICE_CONTROL_STOP:
        // Do whatever it takes to stop here. 
        SvcDebugTrace("Telling service to stop\n", 0);
        ServiceStopping = 1;
        LuaServiceStatus.dwWin32ExitCode = 0;
        LuaServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        LuaServiceStatus.dwCheckPoint = 0;
        LuaServiceStatus.dwWaitHint = 25250;

        if (!SetServiceStatus(LuaServiceStatusHandle, &LuaServiceStatus)) {
            status = GetLastError();
            SvcDebugTrace("SetServiceStatus error %ld\n", status);
        }
        if (ServiceWorkerThread != NULL) {
            SvcDebugTrace("Waiting 25 s for worker to stop\n", 0);
            WaitForSingleObject(ServiceWorkerThread, 25000);
            CloseHandle(ServiceWorkerThread);
        }
        LuaServiceStatus.dwCurrentState = SERVICE_STOPPED;
        if (!SetServiceStatus(LuaServiceStatusHandle, &LuaServiceStatus)) {
            status = GetLastError();
            SvcDebugTrace("SetServiceStatus error %ld\n", status);
        }

        SvcDebugTrace("Leaving Service\n", 0);
        return;

    case SERVICE_CONTROL_INTERROGATE:
        // Fall through to send current status. 
        break;

    default:
        SvcDebugTrace("Unrecognized opcode %ld\n", Opcode);
    }

    // Send current status. 
    if (!SetServiceStatus(LuaServiceStatusHandle, &LuaServiceStatus)) {
        status = GetLastError();
        SvcDebugTrace("SetServiceStatus error %ld\n", status);
    }
    return;
}

/** Stup initialization function.
 * 
 * Initialize Lua state then load and compile our script. The script to run
 * is specified by the <code>script</code> field in the table returned by
 * init.lua.
 * 
 * \param argc Count of arguments passed to the service program by the SCM.
 * \param argv Array of argument strings passed to the service program by the SCM.
 * \param ph   Pointer to a LUAHANDLE that will be written with the handle 
 *             of an initialized Lua state that has all globals loaded and 
 *             the service's main script parsed and loaded.
 * \param perror Pointer to a DWORD to fill with the Win32 error code that
 *             relates to initialization failure, if initialization failed.
 * 			   This value will be passed to the SCM for logging on failure.
 * \returns    Zero on success, non-zero exit status on failure.
 * 			   This value will be passed to the SCM for logging on failure.
 */
DWORD LuaServiceInitialization(DWORD argc, LPTSTR *argv, LUAHANDLE *ph,
        DWORD *perror)
{
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
            GetCurrentProcess(), &ServiceWorkerThread, 0, 
            FALSE, 
            DUPLICATE_SAME_ACCESS)) {
        *perror = GetLastError();
        *ph = NULL;
        return TRUE;
    }
    SvcDebugTraceStr("Load LuaService script %s\n", ServiceScript);
    *ph = LuaWorkerLoad(NULL, ServiceScript);
    if(!*ph){
        *perror = -1;
        return TRUE;
    }

    //LuaWorkerSetArgs(argc, argv);
    *perror = 0;
    return NO_ERROR;
}

BOOL LuaServiceSetStatus(DWORD dwCurrentState, DWORD dwCheckPoint,
        DWORD dwWaitHint)
{
    LuaServiceStatus.dwCurrentState = dwCurrentState;
    LuaServiceStatus.dwCheckPoint = dwCheckPoint;
    LuaServiceStatus.dwWaitHint = dwWaitHint;
    return SetServiceStatus(LuaServiceStatusHandle, &LuaServiceStatus);
}

/** Service Main function.
 * 
 * The entry point of the service's primary worker thread. Since
 * this thread was created by system library code, it apparently 
 * has not had the CRT completely initialized. 
 * 
 * \todo Should LuaService push its Lua implementation into a second
 * worker thread that has its CRT properly initialized by using 
 * _beginthreadex() to create it instead of CreateThread()?
 * 
 * \context
 * Service worker thread
 * 
 * \param argc The count of arguments.
 * \param argv The list of arguments.
 * 
 * \see \ref ssSvc
 */
void WINAPI LuaServiceMain(DWORD argc, LPTSTR *argv)
{
    DWORD status = 0;
    DWORD specificError = 0;
    LUAHANDLE wk = 0;

    SvcDebugTrace("Entered LuaServiceMain\n", 0);

    LuaServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; // SERVICE_WIN32; 
    LuaServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    LuaServiceStatus.dwControlsAccepted = (0 | SERVICE_ACCEPT_STOP 
                //| SERVICE_ACCEPT_SHUTDOWN
#ifdef LUASERVICE_CAN_PAUSE_CONTINUE
                | SERVICE_ACCEPT_PAUSE_CONTINUE
#endif
                );
    LuaServiceStatus.dwWin32ExitCode = 0;
    LuaServiceStatus.dwServiceSpecificExitCode = 0;
    LuaServiceStatus.dwCheckPoint = 0;
    LuaServiceStatus.dwWaitHint = 0;

    LuaServiceStatusHandle = RegisterServiceCtrlHandler(
            ServiceName,
            LuaServiceCtrlHandler);

    if (LuaServiceStatusHandle == (SERVICE_STATUS_HANDLE)0) {
        SvcDebugTrace("RegisterServiceCtrlHandler failed %d\n",
                GetLastError());
        return;
    }

    // Initialization code goes here. 
    LuaServiceSetStatus(SERVICE_START_PENDING, 0, 5000);
    status = LuaServiceInitialization(argc, (char **)argv, &wk,
            &specificError);
    if (status != NO_ERROR) {
        // Handle error condition 
        LuaServiceStatus.dwCurrentState = SERVICE_STOPPED;
        LuaServiceStatus.dwCheckPoint = 0;
        LuaServiceStatus.dwWaitHint = 0;
        LuaServiceStatus.dwWin32ExitCode = status;
        LuaServiceStatus.dwServiceSpecificExitCode = specificError;

        SvcDebugTrace("LuaServiceInitialization exitCode %ld\n", status);
        SvcDebugTrace("LuaServiceInitialization specificError %ld\n",
                specificError);

        SetServiceStatus(LuaServiceStatusHandle, &LuaServiceStatus);
        return;
    }

    // Initialization complete - report running status. 
    if (!LuaServiceSetStatus(SERVICE_RUNNING, 0, 0)) {
        status = GetLastError();
        SvcDebugTrace("SetServiceStatus error %ld\n", status);
    }

    // do the work of the service by running the loaded script.
    LuaWorkerRun(wk);
    LuaWorkerCleanup(wk);

    if(!ServiceStopping){
        SvcDebugTrace("Service main script exit. Stopping service... \n", 0);
        LuaServiceStatus.dwCurrentState = SERVICE_STOPPED;
        LuaServiceStatus.dwCheckPoint = 0;
        LuaServiceStatus.dwWaitHint = 0;
        LuaServiceStatus.dwWin32ExitCode = 1;
        LuaServiceStatus.dwServiceSpecificExitCode = -1;
        SetServiceStatus(LuaServiceStatusHandle, &LuaServiceStatus);
    }

    // we get here only if the script itself returned.
    SvcDebugTrace("Returning to the Main Thread \n", 0);
    return;
}

/** Process entry point.
 * 
 * Invoked when the process starts either by a user at a command prompt 
 * to setup or control the service, or by the Service Control Manager to
 * start the service.
 * 
 * To Distinguish between the three kinds of service-related programs 
 * (the service program, the service control program, and the service 
 * configuration program) that we can call StartServiceCtrlDispatcher() 
 * early on and use its success or failure to connect to the SCM as an 
 * indication of the calling context. If it succeeds, then the process 
 * was started by the SCM and is the service program. If it fails with
 * the specific error code ERROR_FAILED_SERVICE_CONTROLLER_CONNECT, then
 * it is not the service program, and it can depend on its command line
 * to distinguish control from configuration. If any other error is 
 * returned, then it might have been a service program, but something
 * is so horribly wrong that the service cannot start.
 * 
 * \todo We could also support running our service thread interactively
 * to support easier debugging. If that is done, then we should consider
 * making SvcDebugTrace() and friends, as well as the implementation of
 * service.print() for Lua write to stdout rather than OutputDebugString().
 * 
 * \context 
 * Service, Configuration, Control
 *  
 * \param argc The count of arguments.
 * \param argv The list of arguments.
 * \returns The ANSI C process exit status.
 * 
 * \see ssSvc
 */
int main(int argc, char *argv[])
{
    LUAHANDLE lh;
    SERVICE_TABLE_ENTRY DispatchTable[2]; // note room for terminating record.
    char *cp;
    int n;

    memset(DispatchTable, 0, sizeof(DispatchTable));

    SvcDebugTrace("Entered main\n", 0);
    lh = LuaWorkerLoad(NULL, "init.lua");

    if (!lh) {
        fprintf(stderr, "Can not load `init.lua` file");
        return EXIT_FAILURE;
    }

    SvcDebugTrace("... ran init\n", 0);

    if (!LuaWorkerRun(lh)) {
        LuaWorkerCleanup(lh);
        fprintf(stderr, "Can not execute `init.lua` file");
        return EXIT_FAILURE;
    }

    n = LuaResultFieldInt(lh, 1, "tracelevel");
    SvcDebugTraceLevel = n;
    cp = LuaResultFieldString(lh, 1, "name");
    if (cp)
        ServiceName = cp;
    SvcDebugTraceStr("... got name %s", ServiceName);
    cp = LuaResultFieldString(lh, 1, "display_name");
    if (cp)
        ServiceDisplayName = cp;
    cp = LuaResultFieldString(lh, 1, "script");
    if (cp)
        ServiceScript = cp;
    SvcDebugTraceStr("... got script %s", ServiceScript);
    cp = LuaResultFieldString(lh, 1, "lua_path");
    if (cp)
        LuaPackagePath = cp;
    cp = LuaResultFieldString(lh, 1, "lua_cpath");
    if (cp)
        LuaPackageCPath = cp;
    SvcDebugTrace("Finished pre-init\n", 0);
    LuaWorkerCleanup(lh);

    DispatchTable[0].lpServiceName = (LPSTR)ServiceName;
    DispatchTable[0].lpServiceProc = LuaServiceMain;
    SvcDebugTraceStr("Service name: %s\n", ServiceName);
    if (!StartServiceCtrlDispatcher(DispatchTable)) {
        DWORD err = GetLastError();
        if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            /*
             * A failure to connect to the SCM implies we are not running 
             * under the SCM's control, so we must not be the actual 
             * service application. 
             * 
             * We try being a controller or configurer instead.
             */
            return SvcControlMain(argc, argv);
        } else {
            SvcDebugTrace("StartServiceCtrlDispatcher failed %ld\n", err);
            return EXIT_FAILURE;
        }
    }
    SvcDebugTrace("Leaving main\n", 0);
    return EXIT_SUCCESS;
}
