@echo off
set dllsrc ..\msvc.win32.dll.debug
implib -a sbbs.lib 	%dllsrc%\sbbs.dll
implib -a mailsrvr.lib 	%dllsrc\mailsrvr.dll
implib -a ftpsrvr.lib 	%dllsrc\ftpsrvr.dll
