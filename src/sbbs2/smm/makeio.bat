@echo off
bcc -w-pro -N -ml -C -n. -DSMB_GETMSGTXT -I..;..\smb smb2smm.c ..\smb\smblib.c ..\smb\lzh.c
if errorlevel 1 goto end
bcc -w-pro -N -ml -C -n. -I..;..\smb smm2smb.c ..\smb\smblib.c
if errorlevel 1 goto end
bcc -w-pro -N -ml -C -n. -I..;..\smb outphoto.c ..\smb\smblib.c
:end
