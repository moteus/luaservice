/*!
 * \file luaservice.h
 * \brief Public declarations.
 */
#ifndef LUASERVICE_H_
#define LUASERVICE_H_

// From LuaMain.c
/** An opaque pointer to a Lua state. */
typedef void *LUAHANDLE;
extern LUAHANDLE LuaWorkerLoad(LUAHANDLE h, const char *cmd);
extern LUAHANDLE LuaWorkerRun(LUAHANDLE h);
extern void LuaWorkerCleanup(LUAHANDLE h);
extern char *LuaResultString(LUAHANDLE h, int item);
extern int LuaResultInt(LUAHANDLE h, int item);
extern char *LuaResultFieldString(LUAHANDLE h, int item, const char *field);
extern int LuaResultFieldInt(LUAHANDLE h, int item, const char *field);


// From LuaService.c
extern void SvcDebugTrace(LPCSTR fmt, DWORD Status);
extern void SvcDebugTraceStr(LPCSTR fmt, LPCSTR s);
extern int SvcDebugTraceLevel;
extern const char *ServiceName;
extern const char *ServiceScript;
extern const char *LuaPackagePath;
extern const char *LuaPackageCPath;
extern volatile int ServiceStopping;

// From SvcController.c
extern int SvcControlMain(int argc, char *argv[]);

#endif /*LUASERVICE_H_*/
