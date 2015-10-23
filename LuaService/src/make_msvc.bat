rem Makefile for LuaService for MSVC ( I use MSVC 2010 )
rem 7/2015 Helmut Gruber
rem To be run inside a "Visual Studio Command Prompt"
rem
rem Please adjust these 2 lines:
SET LUAINC=c:\Lua\2.2\include
SET LUALIB=c:\Lua\2.2\lua5.1.lib


rc LuaService.rc
cl /I%LUAINC%  LuaMain.c LuaService.c SvcController.c LuaService.res /link %LUALIB% kernel32.lib Advapi32.lib /OUT:../LuaService.exe
del *.obj
