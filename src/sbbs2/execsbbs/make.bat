@echo off
bcc -w-pro -C -mt -ndos execsbbs.c
if errorlevel 1 goto end
exe2bin dos\execsbbs.exe dos\execsbbs.com
:end
