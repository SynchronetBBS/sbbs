@echo off
rem
rem Makes all Watcom compiled Synchronet projects - April 1997 Rob Swindell
rem
cd sbbs\addfiles
call makeall.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\delfiles
call makeall.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\dupefind
call makeall.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\echo
wmake -f sbbsecho.wat OS=OS2  %1 %2 %3 %4 %5
if errorlevel 1 goto err
wmake -f sbbsecho.wat OS=DOS4G  %1 %2 %3 %4 %5
if errorlevel 1 goto err
wmake -f sbbsecho.wat OS=NT  %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\filelist
call makeall.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\smb\smbutil
call makeall.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\..\smbactiv
call makeall.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
rem End of makes
cd ..\..
goto end
:err
rem Error occurred
echo.
echo There was an error!
echo.
:end
