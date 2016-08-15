-- init.lua for the Ticker service
return {
  tracelevel = 0,              -- Framework trace level
  name = "TickService2",       -- Service name for SCM
  script = "test.lua",         -- Script that runs the service
  lua_path = "!\\lib\\?.lua";  -- Additional paths
  lua_cpath = "!\\lib\\?.dll"; -- Additional cpaths
}
