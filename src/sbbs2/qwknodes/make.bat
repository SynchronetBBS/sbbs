@echo off
bcc -w-pro -N -ml -C -I..;..\smb;..\rio qwknodes.c ..\ars.c ..\scfglib1.c ..\scfgvars.c ..\smb\smblib.c
