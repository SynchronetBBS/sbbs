@echo off
echo Synchronet source code check-out for Win32
echo $id: $
setlocal
set HOME=c:\
set CVSROOT=:pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs
cvs co src/uifc
cvs co src/xpdev
cvs co src/sbbs3
cvs co include
cvs co lib/fltk/win32 
cvs co lib/mozilla/js/win32.release
cvs co lib/mozilla/nspr/win32.release
