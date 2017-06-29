@echo off
rd/s/q ship
mkdir ship
copy Readme.txt ship
copy Release\LuaService.exe ship
copy doc\LuaService.chm ship
if exist doc\LuaService.pdf copy doc\LuaService.pdf ship
mkdir ship\Ticker
copy Samples\Ticker\*.lua ship\Ticker
mkdir ship\Rot13
copy Samples\Rot13\*.lua ship\Rot13
zip -r LuaService%1.zip ship\*.*
