@echo off
setlocal
if exist ..\bcc.win32.lib.release 	set dllsrc=..\bcc.win32.lib.release
if exist ..\bcc.win32.lib.debug 	set dllsrc=..\bcc.win32.lib.debug
if exist ..\msvc.win32.dll.release	set dllsrc=..\msvc.win32.dll.release
if exist ..\msvc.win32.dll.debug	set dllsrc=..\msvc.win32.dll.debug
if '%1'=='' goto implib
set dllsrc=%1
:implib
echo Creating import libraries from %dllsrc%
implib sbbs.lib 	%dllsrc%\sbbs.dll
implib mailsrvr.lib 	%dllsrc%\mailsrvr.dll
implib ftpsrvr.lib 	%dllsrc%\ftpsrvr.dll
implib websrvr.lib 	%dllsrc%\websrvr.dll
implib services.lib	%dllsrc%\services.dll