@echo off
if exist err.out del err.out
ECHO ** Compiling
make.exe %1 %2 %3 -a -fgac_bj.mak >>err.out
if errorlevel 1 goto error
ECHO ** Linking
bcc @turboc.cfg -eGAC_BJ.EXE @link.lst >>err.out
if errorlevel 1 goto error
ECHO *****************  SUCCESS  ******************
goto end
:error
ECHO ERRORS!!!
ECHO View ERR.OUT for the list...
if exist gac_bj.exe del gac_bj.exe
:end



