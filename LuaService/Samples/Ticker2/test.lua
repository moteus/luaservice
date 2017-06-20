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
local Service = require "LuaService"

Service.print(
  "Ticker service started, named " .. Service.name .. 
  " run as " .. (Service.RUN_AS_SERVICE and 'service' or 'application')
)

local counter = 0

-- Check service stop flag each second,
-- but call run function each five seconds.
Service.run(function()
  counter = counter + 1
  Service.print("tick", counter)
end, 1000, 5)

Service.print("Ticker service stopped.")
