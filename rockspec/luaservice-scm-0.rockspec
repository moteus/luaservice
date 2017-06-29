package = "LuaService"
version = "scm-0"

source = {
  url = "https://github.com/moteus/luaservice/archive/master.zip",
  dir = "luaservice-master/LuaService",
}

description = {
  summary = "Library for run LuaService scripts as Lua application",
  homepage = "https://github.com/moteus/luaservice",
  detailed = [[This Library allows run same script as Windows service using LuaService
  framewark and as regular script using Lua binaries
  ]],
  license  = "MIT/X11",
}

dependencies = {
  "lua >= 5.1, < 5.4",
  "luafilesystem",
  "luasocket",
}

build = {
  type = "builtin",
  copy_directories = {},

  modules = {
    ["LuaService" ] = "src/LuaService.lua",
  }
}



