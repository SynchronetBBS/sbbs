@echo off
if exist err.out del err.out
ECHO ** Compiling
make %1 %2 %3 -a -fGAC_ID.mak >>err.out
if errorlevel 1 goto error
ECHO ** Linking
bcc @turboc.cfg -eGAC_ID.EXE @link_id.lst >>err.out
if errorlevel 1 goto error
ECHO *****************  SUCCESS  ******************
goto end
:error
ECHO ERRORS!!!
ECHO View ERR.OUT for the list...
if exist GAC_ID.exe del GAC_ID.exe
:end



