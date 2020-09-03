@echo off
echo Synchronet source code check-out for Win32
echo $Id: checkout.bat,v 1.5 2011/10/27 01:23:59 rswindell Exp $
setlocal
set HOME=c:\
set CVSROOT=:pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs
cvs co src-sbbs3
cvs co lib-win32