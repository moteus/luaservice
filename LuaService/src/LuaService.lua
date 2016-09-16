local service = service

local string = require "string"

local function remove_dir_end(str)
  return (string.gsub(str, '[\\/]+$', ''))
end

local function prequire(mod)
  local ok, err = pcall(require, mod)
  if not ok then return nil, err end
  return err, mod
end

local Service = {} do

Service.RUN_AS_SERVICE = not not service

Service.print = service and service.print or print

Service.name  = service and service.name or "LuaService console"

Service.PATH  = service and service.path

if not Service.PATH then
  local lfs = require "lfs"
  Service.PATH = lfs.currentdir()
end

Service.sleep = service and service.sleep

if not Service.sleep then repeat
  local m
  m = prequire "socket"
  if m then Service.sleep = function(s) m.sleep(s/1000) end; break; end
  m = prequire "lzmq.timer"
  if m then Service.sleep = m.sleep; break; end
  m = prequire "winapi"
  if m then Service.sleep = m.sleep; break; end
until true end

assert(Service.sleep, 'can not load sleep function\nCurrently supports: lua-socket/lzmq.timer/winapi modules')

-------------------------------------------------------------------------------
-- Implement basic main loop
do

local STOP_FLAG = false

function Service.check_stop(stime, scount)
  if stime == 0 then
    scount = 1
  end

  stime  = stime  or lsrv.stime  or 1000
  scount = scount or lsrv.scount or 1

  for i = 1, scount do
    if STOP_FLAG or (service and service.stopping()) then 
      STOP_FLAG = true
      return STOP_FLAG
    end
    if stime > 0 then
      Service.sleep(stime)
    end
  end

  return false
end

function Service.stop()
  STOP_FLAG = true
end

function Service.run(main, stime, scount)
  stime  = Service.stime  or stime  or 5000
  scount = Service.scount or scount or 10*2
  while true do
    if Service.check_stop(stime, scount) then
      break
    end
    main()
  end
end

end
-------------------------------------------------------------------------------

end

return Service