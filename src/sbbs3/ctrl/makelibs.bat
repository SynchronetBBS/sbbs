@echo off
setlocal
rem set dllsrc=..\bcc.win32.dll.release
set dllsrc=..\msvc.win32.dll.debug
implib sbbs.lib 	%dllsrc%\sbbs.dll
implib mailsrvr.lib 	%dllsrc%\mailsrvr.dll
implib ftpsrvr.lib 	%dllsrc%\ftpsrvr.dll
implib services.lib	%dllsrc%\services.dll