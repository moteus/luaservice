LuaService
A framework for running a Lua script as a Windows Service.

This is the second alpha release of LuaService.

All the documentation is provided through Doxygen, and drawn from 
source code comments and the files in the dox folder which provide
some overview and background topics. For best readability, have
Doxygen build its document set and start browsing it by opening 
the file doc\html\index.html in your favorite browser.

A sample service implementation is found in the files init.lua 
and test.lua. Put those files together with LuaService.exe in
a folder somewhere on a local disk drive. Get a command prompt
in that folder and say LuaService -i to install the demo ticker 
service.

The following files are included in this release:

ReadMe.txt		This file.
LuaService.exe		The Lua Windows Service framework.
LuaService.chm		Documentation
Ticker			Folder with the Ticker sample
  init.lua		  Ticker service initialization file.
  test.lua		  Ticker service implementation.
Rot13
  init.lua		  Rot13 service initialization file.
  rot13svc.lua		  Rot13 service implementation.
  
Since the Ticker sample service's only visible effect is to call 
OutputDebugString(), you will need a tool like DebugView to see
it work.  Get DebugView from the Sysinternals group at Microsoft,
http://www.microsoft.com/technet/sysinternals/. While there, you
might want to look at ProcessExplorer and ProcessMonitor, both
of which have been valuable for debugging LuaService and other
Windows applications.

The Rot13 sample is more complete. Create the folder \tmp\rot
on the same drive that has \WINDOWS on it. Copy LuaService.exe,
lua5.1.dll, and lfs.dll into the Rot13 sample's folder, and 
say LuaService -i to install and start it. Now, any file copied
into \tmp\rot will be transformed through ROT13. (ROT13 is 
described by http://en.wikipedia.org/wiki/Rot13.) Run DebugView
and see a trace of its activity while it works.

This is an alpha release. Please do not install this on a 
production or mission critical system. It is believed to be 
functional and safe to run, but all the usual disclaimers 
apply. 

It has been tested on Windows XP SP2 with all the maintenance 
patches kept up to date. It might work on Windows 2000, but it is 
extremely unlikely to work at all on Windows NT, 95, 98, or Me. 

I have not tested LuaService on Vista. I am not aware of any 
specific issues with Vista, except that as a system application, 
it is likely to require Administrator access to run at all. I
would love to have feedback from Vista users.

Please visit the LuaService project pages at Luaforge:

	http://luaforge.net/projects/luaservice/
	http://luaservice.luaforge.net/

There you will find the full source code, bug trackers, and so
forth.

	Ross Berteig
	Cheshire Engineering Corp.
	650 Sierra Madre Villa Ave., Suite 201
	Pasadena, CA 91107
	+1.626.351.5493
	Ross@cheshireeng.com
	www.cheshireeng.com

