@echo off
set dllsrc=..\msvc.win32.dll.debug
implib sbbs.lib 	%dllsrc%\sbbs.dll
implib mailsrvr.lib 	%dllsrc%\mailsrvr.dll
implib ftpsrvr.lib 	%dllsrc%\ftpsrvr.dll
