@echo off
echo Synchronet source code check-out for Win32
echo $Id$
setlocal
set HOME=c:\
set CVSROOT=:pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs
cvs co src-sbbs3
cvs co lib-win32 
