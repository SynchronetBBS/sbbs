@echo off
setlocal
if exist ..\bcc.win32.lib.release 	set dllsrc=..\bcc.win32.lib.release
if exist ..\bcc.win32.lib.debug 	set dllsrc=..\bcc.win32.lib.debug
if exist ..\msvc.win32.dll.release	set dllsrc=..\msvc.win32.dll.release
if exist ..\msvc.win32.dll.debug	set dllsrc=..\msvc.win32.dll.debug
echo Creating import libraries from %dllsrc%
coff2omf %dllsrc%\sbbs.lib sbbs.lib
coff2omf %dllsrc%\mailsrvr.lib 	mailsrvr.lib
coff2omf %dllsrc%\ftpsrvr.lib 	ftpsrvr.lib
coff2omf %dllsrc%\websrvr.lib 	websrvr.lib
coff2omf %dllsrc%\services.lib	services.lib
bpr2mak sbbsctrl.bpr
echo.
make %1 %2 %3 %4 %5 %6 -f sbbsctrl.mak
