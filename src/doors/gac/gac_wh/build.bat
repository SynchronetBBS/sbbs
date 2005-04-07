@echo off
if exist err.out del err.out
ECHO ** Compiling
make %1 %2 %3 -a -fgac_wh.mak >>err.out
if errorlevel 1 goto error
ECHO ** Linking
bcc @turboc.cfg -eGAC_WH.EXE @link.lst >>err.out
if errorlevel 1 goto error
ECHO *****************  SUCCESS  ******************
goto end
:error
ECHO ERRORS!!!
ECHO View ERR.OUT for the list...
REM if exist gac_wh.exe del gac_wh.exe
:end



