@echo off
bcc -ml -N -lm -I..;..\smb baja.c ..\ars.c
if errorlevel 1 goto end
bcc32 -WX -N -lm -I..;..\smb -ebaja32.exe baja.c ..\ars.c
:end
