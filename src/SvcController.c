/*! \file SvcController.c
 *  \brief Functions to configure and control a service.
 * 
 * \todo This mechanism was copied from sample code, and lacks
 * friendly handling of command line arguments. Future versions
 * will likely move some of the less critical details into a Lua
 * script, with the actual service control methods exposed via
 * a built-in module.
 */
#include <windows.h>
#include <process.h> 
#include <stdio.h>

#include "luaservice.h"

void ErrorHandler(char *s, int err);
void GetStatus(SC_HANDLE service);
void ShowUsage();

// service config program tasks
int InstallService();
int UninstallService();
int GetConfiguration();
int ChangeConfig();

// service control program tasks
int ServiceRun();
int ServiceControl(char* CONTROL);

/** Entry point for service control and configuration.
 * 
 * Called from main() if the program is not running as a service
 * and responsible to the SCM.
 * 
 * \param argc Count of arguments in \a argv.
 * \param argv Array of arguments.
 * \return Exit status, as from main().
 */
int SvcControlMain(int argc, char *argv[]) {
    if (argc == 2) {
        if (stricmp("-i", argv[1]) == 0)
            InstallService();
        else if (stricmp("-u", argv[1]) == 0)
            UninstallService();
        else if (stricmp("-r", argv[1]) == 0)
            ServiceRun();
        else if (stricmp("-s", argv[1]) == 0)
            ServiceControl("STOP");
#ifdef LUASERVICE_CAN_PAUSE_CONTINUE
        else if (stricmp("-p", argv[1]) == 0)
        ServiceControl("PAUSE");
        else if (stricmp("-c", argv[1]) == 0)
        ServiceControl("RESUME");
#endif
        else if (stricmp("status", argv[1]) == 0) {
            SC_HANDLE scm, service;
            //Open connection to SCM
            scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
            if (!scm)
                ErrorHandler("OpenSCManager", GetLastError());
            //Get service's handle
            service = OpenService(scm, ServiceName, SERVICE_ALL_ACCESS);
            if (!service)
                ErrorHandler("OpenService", GetLastError());
            fputs("STATUS: ", stdout);
            GetStatus(service);
        } else if (stricmp("config", argv[1]) == 0)
            GetConfiguration();
        else if (stricmp("help", argv[1]) == 0)
            ShowUsage();
        //add other custom commands here and 
        //update ShowUsage function
        else {
            ShowUsage();
            return EXIT_FAILURE;
        }
    } else {
        ShowUsage();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#if 0
//this function consolidates the activities of updating
//the service status with SetServiceStatus
BOOL SendStatusToSCM(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwServiceSpecificExitCode, DWORD dwCheckPoint, DWORD dwWaitHint)
{
    BOOL success;
    SERVICE_STATUS serviceStatus;

    //fill in all of the SERVICE_STATUS fields
    serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    serviceStatus.dwCurrentState = dwCurrentState;

    //if in the process of something, then accept
    //no control events, else accept anything
    if (dwCurrentState == SERVICE_START_PENDING)
    serviceStatus.dwControlsAccepted = 0;
    else
    serviceStatus.dwControlsAccepted =
    SERVICE_ACCEPT_STOP |
    SERVICE_ACCEPT_PAUSE_CONTINUE |
    SERVICE_ACCEPT_SHUTDOWN;

    //if a specific exit code is defines, set up the win32 exit code properly
    if (dwServiceSpecificExitCode == 0)
    serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
    else
    serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;

    serviceStatus.dwServiceSpecificExitCode = dwServiceSpecificExitCode;
    serviceStatus.dwCheckPoint = dwCheckPoint;
    serviceStatus.dwWaitHint = dwWaitHint;

    success = SetServiceStatus (serviceStatusHandle, &serviceStatus);
    if (!success)
    StopService();

    return success;
}

//handle an error from ServiceMain by cleaning up and tell SCM service didn't start.
void terminate(DWORD error)
{
    //close event handle
    if (terminateEvent)
    CloseHandle(terminateEvent);

    //notify SCM service stopped
    if (serviceStatusHandle)
    SendStatusToSCM(SERVICE_STOPPED, error, 0, 0, 0);

    //close thread handle
    if (threadHandle)
    CloseHandle(threadHandle);
}
#endif

/** Handle a Windows error code.
 * 
 * Looks up the error code \a err in the system table of error messages.
 * If the message exists, it is printed to stderr and to a log file. 
 * Otherwise, the printed message and the log entry both indicate that
 * FormatMessage() couldn't identify the error code.
 * 
 * \note In the current version, this is only called after a 
 * service-related bit of the Windows API fails, so it is highly 
 * unlikely that any error code will cause FormatMessage() to fail.
 * However, if a future version allows this function to be called 
 * in a wider context or even from Lua code, then it is possible 
 * that a provision should be made to look for error messages from 
 * additional sources such as any DLLs loaded into the process.
 * 
 * \param s Name of function or other context that caused the failure.
 * \param err Windows error code returned from GetLastError().
 * \return This function calls exit() and does not return to its 
 * caller.
 */
void ErrorHandler(char *s, int err) {
    LPVOID lpMsgBuf;
    FILE* pLog;
    size_t n;

    n = (size_t)FormatMessageA((0 | FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM | 75 ), 
    NULL, err, 0, //MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf, 0, NULL);
    pLog = fopen("LuaService.log", "a");
    if (n != 0) {
        fprintf(stderr,
        "%s failed\n"
        "Error (%d): %s\n", s, err, (char *)lpMsgBuf);
        LocalFree(lpMsgBuf);
    } else {
        fprintf(stderr,
        "%s failed\n"
        "Error (%d): <<not known to FormatMessage(): %ld>>\n",
        s, err, GetLastError());
    }
    fprintf(pLog, "%s failed, error code = %d\n", s, err);
    fclose(pLog);
    exit(EXIT_FAILURE);
}

/** Print a command-line usage message.
 */
void ShowUsage() {
    printf(
            "Usage:\n"
            "LuaService -i\tInstall service\n"
            "LuaService -u\tUninstall service\n"
            "LuaService -r\tRun service\n"
            "LuaService -s\tStop service\n"
#ifdef LUASERVICE_CAN_PAUSE_CONTINUE
            "LuaService -p\tPause service\n"
            "LuaService -c\tResume service\n"
#endif
            "LuaService status\tCurrent status\n"
            "LuaService help\tDisplay this text\n"
            );
}

/** Install the service.
 * 
 * Asks the \ref ssSCM to install the service on the local machine.
 * 
 * The service name and display name are both derived from the
 * name field returned by init.lua.
 * 
 * The service is installed as auto-start, using the LocalSystem
 * authority.
 * 
 * Once installed, the SCM is told to start the service.
 * 
 * \returns	Returns TRUE on success. The current implementation 
 * calls ErrorHandler() for all significant errors which exits
 * the process and does not return. There appear to be no 
 * insignificant errors.
 */
int InstallService() {
    SC_HANDLE newService;
    SC_HANDLE scm;
    char szPath[MAX_PATH+3];

    //get file path
    szPath[0] = '\"';
    GetModuleFileName(GetModuleHandle(NULL), szPath+1, MAX_PATH);
    strcat(szPath, "\"");

    //open connection to SCM
    scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm)
        ErrorHandler("OpenSCManager", GetLastError());

    if(ServiceDisplayName == NULL)
        ServiceDisplayName = ServiceName;

    //install service
    newService = CreateService(
            scm, //scm database
            ServiceName, //service name
            ServiceDisplayName, //display name
            SERVICE_ALL_ACCESS, //access rights to the service
            SERVICE_WIN32_OWN_PROCESS, //service type
            SERVICE_AUTO_START, //service start type
            SERVICE_ERROR_NORMAL, //error control type
            szPath, //service path
            NULL, //no load ordering group 
            NULL, //no tag identifier
            NULL, //no dependencies	
            NULL, //LocalSystem account
            NULL); //no password
    if (!newService) {
        ErrorHandler("CreateService", GetLastError());
        return FALSE;
    } else {
        puts("Service Installed");
        ServiceRun();
    }

    //clean up
    CloseServiceHandle(newService);
    CloseServiceHandle(scm);
    return TRUE;
}

/** Uninstall the service.
 * 
 * Asks the \ref ssSCM to uninstall the service from the local
 * machine.
 * 
 * The service name and display name are both derived from the
 * name field returned by init.lua.
 * 
 * If the service is currently running, the SCM is told to stop
 * the service and before the uninstall can continue.
 * 
 * \returns	Returns TRUE on success. The current implementation 
 * calls ErrorHandler() for all significant errors which exits
 * the process and does not return. There appear to be no 
 * insignificant errors.
 */
int UninstallService() {
    SC_HANDLE service;
    SC_HANDLE scm;
    int SUCCESS;
    SERVICE_STATUS status;

    //Open connection to SCM
    scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm)
        ErrorHandler("OpenSCManager", GetLastError());

    //Get service's handle
    service = OpenService(scm, ServiceName, SERVICE_ALL_ACCESS | DELETE);
    if (!service)
        ErrorHandler("OpenService", GetLastError());

    //Get service status
    SUCCESS = QueryServiceStatus(service, &status);
    if (!SUCCESS)
        ErrorHandler("QueryServiceStatus", GetLastError());

    //Stop service if necessary		
    if (status.dwCurrentState != SERVICE_STOPPED) {
        puts("Stopping service...");
        SUCCESS = ControlService(service, SERVICE_CONTROL_STOP, &status);
        if (!SUCCESS)
            ErrorHandler("ControlService", GetLastError());
        Sleep(500);
    }

    //Delete service
    SUCCESS = DeleteService(service);
    if (SUCCESS)
        puts("Service Uninstalled");
    else
        ErrorHandler("DeleteService", GetLastError());

    //Clean up
    CloseServiceHandle(service);
    CloseServiceHandle(scm);

    return TRUE;
}

/** Start the service running.
 * 
 * Asks the \ref ssSCM to run the service on the local
 * machine.
 * 
 * The service name and display name are both derived from the
 * name field returned by init.lua.
 * 
 * \returns	Returns TRUE on success. The current implementation 
 * calls ErrorHandler() for all significant errors which exits
 * the process and does not return. There appear to be no 
 * insignificant errors.
 */
int ServiceRun() {
    SC_HANDLE scm, Service;
    SERVICE_STATUS ssStatus;
    DWORD dwOldCheckPoint;
    DWORD dwStartTickCount;
    DWORD dwWaitTime;
    DWORD dwStatus;

    //open connection to SCM
    scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm)
        ErrorHandler("OpenSCManager", GetLastError());

    //open service
    Service = OpenService(scm, ServiceName, SERVICE_ALL_ACCESS);
    if (!Service) {
        ErrorHandler("OpenService", GetLastError());
        return FALSE;
    } else {
        //start service
        StartService(Service, 0, NULL);
        GetStatus(Service);

        // Check the status until the service is no longer start pending. 
        if (!QueryServiceStatus(Service, &ssStatus) )
            ErrorHandler("QueryServiceStatus", GetLastError());
        // Save the tick count and initial checkpoint.
        dwStartTickCount = GetTickCount();
        dwOldCheckPoint = ssStatus.dwCheckPoint;

        while (ssStatus.dwCurrentState == SERVICE_START_PENDING) {
            // Do not wait longer than the wait hint. A good interval is 
            // one tenth the wait hint, but no less than 1 second and no 
            // more than 10 seconds. 
            dwWaitTime = ssStatus.dwWaitHint / 10;

            if (dwWaitTime < 1000)
                dwWaitTime = 1000;
            else if (dwWaitTime > 10000)
                dwWaitTime = 10000;

            Sleep(dwWaitTime);

            // Check the status again. 
            if (!QueryServiceStatus(Service, &ssStatus) )
                break;

            if (ssStatus.dwCheckPoint > dwOldCheckPoint) {
                // The service is making progress.
                dwStartTickCount = GetTickCount();
                dwOldCheckPoint = ssStatus.dwCheckPoint;
            } else {
                if (GetTickCount()-dwStartTickCount > ssStatus.dwWaitHint) {
                    // No progress made within the wait hint
                    break;
                }
            }
        }

        if (ssStatus.dwCurrentState == SERVICE_RUNNING) {
            GetStatus(Service);
            dwStatus = NO_ERROR;
        } else {

            puts("\nService not started.");
            printf("  Current State: %ld\n", ssStatus.dwCurrentState);
            printf("  Exit Code: %ld\n", ssStatus.dwWin32ExitCode);
            printf("  Service Specific Exit Code: %ld\n",
                    ssStatus.dwServiceSpecificExitCode);
            printf("  Check Point: %ld\n", ssStatus.dwCheckPoint);
            printf("  Wait Hint: %ld\n", ssStatus.dwWaitHint);
            dwStatus = GetLastError();
        }
    }

    CloseServiceHandle(scm);
    CloseServiceHandle(Service);
    return TRUE;
}

/** Send other controls to the service.
 * 
 * Asks the \ref ssSCM to control the service on the local
 * machine based on the parameter.
 * 
 * The service name and display name are both derived from the
 * name field returned by init.lua.
 * 
 * \param CONTROL The name of the control message to send. The 
 * following controls are understood:
 * - "STOP"
 * 
 * \returns	Returns TRUE on success. The current implementation 
 * calls ErrorHandler() for all significant errors which exits
 * the process and does not return. There appear to be no 
 * insignificant errors.
 */
int ServiceControl(char* CONTROL) {
    SC_HANDLE service;
    SC_HANDLE scm;
    BOOL SUCCESS= FALSE;
    SERVICE_STATUS status;

    //Open connection to SCM
    scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm)
        ErrorHandler("OpenSCManager", GetLastError());

    //Get service's handle
    service = OpenService(scm, ServiceName, SERVICE_ALL_ACCESS);
    if (!service)
        ErrorHandler("OpenService", GetLastError());

    //stop the service
    if (stricmp(CONTROL, "STOP") == 0) {
        puts("Service is stopping...");
        SUCCESS = ControlService(service, SERVICE_CONTROL_STOP, &status);
    }
#ifdef LUASERVICE_CAN_PAUSE_CONTINUE
    //pause the service
    else if (stricmp(CONTROL, "PAUSE") == 0)
    {
        puts("Service is pausing...");
        SUCCESS = ControlService(service, SERVICE_CONTROL_PAUSE, &status);
    }
    //continue the service
    else if (stricmp(CONTROL, "RESUME") == 0)
    {
        puts("Service is resuming...");
        SUCCESS = ControlService(service, SERVICE_CONTROL_CONTINUE, &status);
    }
#endif
    if (!SUCCESS)
        ErrorHandler("ControlService", GetLastError());
    else
        GetStatus(service);

    //Clean up
    CloseServiceHandle(service);
    CloseServiceHandle(scm);

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Purpose	:Get the current status of the service
// Parameter:service handle.
// Returns	:N/A
////////////////////////////////////////////////////////////////////////////////
void GetStatus(SC_HANDLE service) {
    BOOL SUCCESS;
    SERVICE_STATUS status;

    SUCCESS = QueryServiceStatus(service, &status);
    switch (status.dwCurrentState) {
    case SERVICE_RUNNING:
        puts("Service RUNNING.");
        break;
    case SERVICE_STOPPED:
        puts("Service STOPPED.");
        break;
    case SERVICE_PAUSED:
        puts("Service PAUSED.");
        break;
    case SERVICE_CONTINUE_PENDING:
        puts("Service is resuming...");
        break;
    case SERVICE_PAUSE_PENDING:
        puts("Service is pausing...");
        break;
    case SERVICE_START_PENDING:
        puts("Service is starting...");
        break;
    case SERVICE_STOP_PENDING:
        puts("Service is stopping...");
        break;
    default:
        break;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Purpose	:Get configuration of service
// Parameter:N/A
// Returns	:N/A
////////////////////////////////////////////////////////////////////////////////
int GetConfiguration() {
    SC_HANDLE service;
    SC_HANDLE scm;
    BOOL success;
    LPQUERY_SERVICE_CONFIG buffer;
    DWORD sizeNeeded;

    //open connection to SCM
    scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm)
        ErrorHandler("OpenSCManager", GetLastError());

    //get service's handle
    service = OpenService(scm, ServiceName, SERVICE_QUERY_CONFIG);
    if (!service)
        ErrorHandler("OpenService", GetLastError());

    //allocate space for buffer
    buffer = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LPTR, 4096);
    // Get the configuration information. 
    success = QueryServiceConfig(service, buffer, 4096, &sizeNeeded);
    if (!success)
        ErrorHandler("QueryServiceConfig", GetLastError());

    //display the info
    printf("Service name\t: %s\n", buffer->lpDisplayName);
    printf("Service type\t: %ld\n", buffer->dwServiceType);
    printf("Start type\t: %ld\n", buffer->dwStartType);
    printf("Start name\t: %s\n", buffer->lpServiceStartName);
    printf("Path\t\t: %s\n", buffer->lpBinaryPathName);

    LocalFree(buffer);

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return TRUE;
}

int ChangeConfig() {
    SC_HANDLE service;
    SC_HANDLE scm;
    BOOL success;
    SC_LOCK lock;

    //open connection to SCM
    scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS | GENERIC_WRITE);
    if (!scm)
        ErrorHandler("OpenSCManager", GetLastError());

    //lock the database to guarantee exclusive access
    lock = LockServiceDatabase(scm);
    if (lock == 0)
        ErrorHandler("LockServiceDatabase", GetLastError());

    //get service's handle
    service = OpenService(scm, ServiceName, SERVICE_ALL_ACCESS);
    if (!service)
        ErrorHandler("OpenService", GetLastError());

    //	serviceType = SERVICE_NO_CHANGE;
    //	serviceStart = SERVICE_NO_CHANGE;
    //	serviceError = SERVICE_NO_CHANGE;
    //	path = 0;

    //change service config
    success = ChangeServiceConfig(
            service,
            SERVICE_NO_CHANGE,
            SERVICE_NO_CHANGE,
            SERVICE_NO_CHANGE,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL);
    if (!success) {
        UnlockServiceDatabase(lock);
        ErrorHandler("ChangeServiceConfig", GetLastError());
    }

    //unlock database
    success = UnlockServiceDatabase(lock);
    if (!success)
        ErrorHandler("UnlockServiceDatabase", GetLastError());

    //clean up
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return TRUE;
}
