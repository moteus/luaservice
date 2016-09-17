:: Build LuaService with MSVC
:: Run this script from MSVC Command Prompt

@ECHO OFF && SETLOCAL enableextensions enabledelayedexpansion

set LUADIR=c:\Luarocks\x86\5.3
set LUALIB=lua53.lib
set OUTNAME=LuaService.exe

set LUA_INCDIR=%LUADIR%\include
set LUA_LIBDIR=%LUADIR%\lib
set OUTDIR=Release

:: Configure LuaService
SET DEFS=
SET DEFS=%DEFS% USE_LUA_ALLOCATOR
::SET DEFS=%DEFS% NO_DEBUG_TRACEBACK
::SET DEFS=%DEFS% USE_ONLY_MALLOC
::SET DEFS=%DEFS% LOG_ALLOCATIONS

SET CFLAGS=/nologo -c /MD /O2 /WX /D_CRT_SECURE_NO_DEPRECATE /DNDEBUG /I%LUA_INCDIR%
SET LFLAGS=/nologo /INCREMENTAL:NO /LIBPATH:%LUA_LIBDIR%
SET RFLAGS=/nologo
SET LIBS=kernel32.lib Advapi32.lib %LUALIB%

SET CFILES=src\LuaMain.c src\LuaService.c src\SvcController.c
SET RFILES=src\LuaService.rc

set DEFS_=
FOR %%S IN (%DEFS%) DO SET DEFS_=!DEFS_! /D%%S

set OFILES=
FOR %%S IN (%CFILES%) DO (
  set SRC_NAME=%%S
  set OBJ_NAME=!SRC_NAME:~0,-2!.obj
  set OFILES=!OFILES! !OBJ_NAME!
  echo cl %CFLAGS% %DEFS_% !SRC_NAME! /Fo!OBJ_NAME!
  cl %CFLAGS% %DEFS_% !SRC_NAME! /Fo!OBJ_NAME!
)

set RESFILE=%RFILES:~0,-3%.res

echo rc %RFLAGS% /fo %RESFILE% %RFILES%
rc %RFLAGS% /fo %RESFILE% %RFILES%

echo link %LFLAGS% %OFILES% %RESFILE% %LIBS% /OUT:%OUTDIR%\%OUTNAME%
link %LFLAGS% %OFILES% %RESFILE% %LIBS% /OUT:%OUTDIR%\%OUTNAME%

del %OFILES%
del %RESFILE%
