--[[--------------
Rot13 Service

This service watches a folder for files. When a new file is found, 
it transforms text in the file with a ROT13 encryption. This version
uses LuaFileSystem (available separately from the Keppler Project) 
to scan its folder. A better implementation would be to use a 
Windows folder change event to reduce the impact on the system, but
testing by copying 300 files to the watched folder shows that the
impact of doing it this way isn't as large as might be feared.

Usage:

Copy the LuaService executable to this folder, along with lua5.1.dll
and lfs.dll.

LuaService -i		Create and start the ticker service
LuaService -r 		Start the service
LuaService -s		Stop the service
LuaService -u		Uninstall the service

You will want a debug console that is listening to OutputDebugString() to 
track progress as this service executes. DebugView from www.sysinternals.com
is a good choice.

Once the service is running, copy a file to \tmp\rot and notice that 
about a second later it has had its content scrambled. Rename the file,
and notice that a second scramble restores the content.

A brief history of ROT13 is at http://en.wikipedia.org/wiki/Rot13
--]]--------------
assert(require"lfs")

local watched = [[\tmp\rot]]	-- folder to watch


-- ROT13 a string. Copied from the Lua-L archive, from a 
-- message from Philippe Lhoste on Fri, 22 Oct 2004
local function Rotate13(t)
  local byte_a, byte_A = string.byte('a'), string.byte('A')
  return (string.gsub(t, "[%a]",
      function (char)
        local offset = (char < 'a') and byte_A or byte_a
        local b = string.byte(char) - offset -- 0 to 25
        b = math.mod(b  + 13, 26) + offset -- Rotate
        return string.char(b)
      end
    ))
end


local found = {}	-- memory of files seen in the folder

-- examine a folder for files that are new since the last peek.
local function CheckForFiles()
	-- list of new files
	local r = {}
	for f in lfs.dir(watched) do
		local ff = watched..[[\]]..f
		if lfs.attributes(ff, "mode") == "file" then
			if not found[ff] then 
				found[ff] = "new"
				r[#r+1] = ff
			else
				found[ff] = "newish"
			end
		end
	end
	-- scan for old files that have been deleted
	local d = {}
	for k,f in pairs(found) do
		if f == "old" then 
			d[#d+1] = k
		else
			found[k] = "old"
		end
	end
	-- forget about the deleted files
	for i,k in ipairs(d) do
		found[k] = nil
	end
	return #r>0 and r or nil
end

-- Apply ROT13 to an entire file. If the file
-- has gone missing, delete it from the table
-- of remembered files.
local function Rot13File(file)
	service.print("Rot13: ", file)
	local f,err = io.open(file, "r+")
	if f == nil then 
		found[ff] = nil
		service.print(file,": ", err)
		return
	end
	local txt = f:read("*a")
	f:seek("set",0)
	f:write(Rotate13(txt))
	f:close()
end

-- main service implementation
service.print("ROT13 service started, named ", service.name)
while true do					-- loop forever
	service.sleep(1000)			-- sleep 1 second
	if service.stopping() then	-- Test for STOP request 
  		break					--  " and halt service if requested
	end
	local files = CheckForFiles()
	if type(files)=="table" then 
		for i,f in ipairs(files) do
			Rot13File(f)
		end
	end
end
service.print("ROT13 service stopped.")

