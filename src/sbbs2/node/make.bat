@echo off
bcc -w-pro -I.. -C -mt -nDOS node.c
if errorlevel 1 goto end
exe2bin dos\node.exe dos\node.com
:end