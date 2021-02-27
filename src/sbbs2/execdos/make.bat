@echo off
bcc -w-pro -I..;..\rio;..\smb -ml execdos.c ..\dos\rciol.obj
if errorlevel 1 goto end
rem exe2bin execdos.exe execdos.com
:end
