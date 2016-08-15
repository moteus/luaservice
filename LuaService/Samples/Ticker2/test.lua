-- Ticker service implementation
--[[----------------------------
This service simply emits a string on the debug console once 
every 5 seconds until it is stopped. Not necessarily the most
useful service, but it demonstrates how to construct a service
in the LuaService framework without too many other system
assumptions.

Copy the LuaService executable to this folder.

To use sc.exe to create, start, and stop the service:

sc create TickService binPath= "C:\path\to\this\folder\LuaService.exe"
sc start TickService
sc stop TickService

To use LuaService to create, start, and stop the service:

LuaService -i		Create and start the ticker service
LuaService -r 		Start the service
LuaService -s		Stop the service
LuaService -u		Uninstall the service

You will want a debug console that is listening to OutputDebugString() to 
see this service do anything at all. DebugView from www.sysinternals.com
is a good choice.
--]]----------------------------
local SERVICE = require "SERVICE"

SERVICE.print("Ticker service started, named ", SERVICE.name)

local i = 0						-- counter

SERVICE.run(function()
  i = i + 1						-- count
  SERVICE.print("tick", i)		-- OutputDebugString
end, 1000, 5)

SERVICE.print("Ticker service stopped.")

SERVICE.exit(0)
