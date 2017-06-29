local service = require "LuaService"
local uv      = require "lluv"

local EOF = uv.error('LIBUV', uv.EOF)

local function logerror(...)
  service.print('[ERROR] ' .. string.format(...))
end

local function loginfo(...)
  service.print('[INFO] ' .. string.format(...))
end

local host, port = service.argv[1] or '*:5678'
if string.find(host, ':') then
  host, port = string.match(host, '^(.-):(.-)$')
end

if not port then
  logerror('Invalid enpoint argument: %s', host)
  return 1
end

local function on_write(cli, err)
  if err then
    if err == EOF then
      loginfo('on write close socket')
    else
      logerror('on write error: %s', tostring(err))
    end
    return cli:close()
  end
end

local function on_read(cli, err, data)
  if err then
    if err == EOF then
      loginfo('on read close socket')
    else
      logerror('on read error: %s', tostring(err))
    end
    return cli:close()
  end
  cli:write(data, on_write)
end

uv.tcp():bind(host, port, function(server, err, host, port)
  if err then
    logerror("Can not bind: %s", tostring(err))
    return server:close()
  end

  loginfo("Bind on: %s:%s", tostring(host), tostring(port))

  server:listen(function(server, err)
    if err then
      logerror('listen error: %s', tostring(err))
      return
    end

    -- create client socket in same loop as server
    local cli, err = server:accept()
    if not cli then
      logerror('accept error: %s', tostring(err))
      return
    end

    loginfo('accepted: %s:%s', cli:getpeername())

    cli:start_read(on_read)
  end)
end)

-- handle service control

local function stop_service()
  loginfo("stopping service...")
  uv.stop()
end

loginfo("running service...")

if service.RUN_AS_SERVICE then

  loginfo("run as service")

  uv.timer():start(1000, 1000, function()
    if service.check_stop(0) then
      stop_service()
    end
  end)

else

  loginfo("run as console")

  uv.signal():start(uv.SIGINT,   stop_service)

  uv.signal():start(uv.SIGBREAK, stop_service)
end

uv.run()

loginfo("service stopped")
