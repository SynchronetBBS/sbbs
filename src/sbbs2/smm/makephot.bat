@echo off
bcc -I..;..\smb -w-pro -N -C -ml addphoto.c
if errorlevel 1 goto end
bcc -I..;..\smb -w-pro -N -C -ml delphoto.c
if errorlevel 1 goto end
bcc -I..;..\smb -w-pro -N -C -ml lstphoto.c
:end
