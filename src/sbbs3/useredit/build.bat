@echo off
setlocal
if exist ..\bcc.win32.lib.release 	set dllsrc=..\bcc.win32.lib.release
if exist ..\bcc.win32.lib.debug 	set dllsrc=..\bcc.win32.lib.debug
if exist ..\msvc.win32.dll.release	set dllsrc=..\msvc.win32.dll.release
if exist ..\msvc.win32.dll.debug	set dllsrc=..\msvc.win32.dll.debug
if '%1'=='' goto implib
set dllsrc=%1
:implib
coff2omf %dllsrc%\sbbs.lib sbbs.lib
bpr2mak useredit.bpr
echo.
make %1 %2 %3 %4 %5 %6 -f useredit.mak
