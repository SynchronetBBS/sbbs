@echo off
setlocal
if exist ..\bcc.win32.dll.debug 	set dllsrc=..\bcc.win32.dll.debug
if exist ..\bcc.win32.dll.release 	set dllsrc=..\bcc.win32.dll.release
if exist ..\msvc.win32.dll.debug	set dllsrc=..\msvc.win32.dll.debug
if exist ..\msvc.win32.dll.release	set dllsrc=..\msvc.win32.dll.release
implib sbbs.lib 	%dllsrc%\sbbs.dll
implib mailsrvr.lib 	%dllsrc%\mailsrvr.dll
implib ftpsrvr.lib 	%dllsrc%\ftpsrvr.dll
implib services.lib	%dllsrc%\services.dll