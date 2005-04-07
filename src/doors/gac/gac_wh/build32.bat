@echo off
if exist err.out del err.out
ECHO ** Compiling
make.exe %1 %2 %3 -a -fgac_wh32.mak >>err.out
if errorlevel 1 goto error
ECHO ** Linking
bcc32 @turbo32.cfg -egac_wh32 @link32.lst >>err.out
if errorlevel 1 goto error
ECHO *****************  SUCCESS  ******************
goto end
:error
ECHO ERRORS!!!
ECHO View ERR.OUT for the list...
if exist gac_wh32.exe del gac_wh32.exe
:end



