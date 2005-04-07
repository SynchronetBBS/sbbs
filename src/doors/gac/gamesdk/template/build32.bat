@echo off
if exist err.out del err.out
ECHO ** Compiling
make.exe %1 %2 %3 -a -fgac_id32.mak >>err.out
if errorlevel 1 goto error
ECHO ** Linking
bcc32 @turbo32.cfg -egac_id32 @link32.lst >>err.out
if errorlevel 1 goto error
ECHO *****************  SUCCESS  ******************
goto end
:error
ECHO ERRORS!!!
ECHO View ERR.OUT for the list...
if exist gac_id32.exe del gac_id32.exe
:end



