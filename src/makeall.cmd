@echo off
rem
rem Makes all Borland for OS/2 Synchronet projects - April 1997 Rob Swindell
rem
cd sbbs
make %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd scfg
make %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\addfiles
make -fmakefile.bc %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\baja
call make.cmd %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\delfiles
make -fmakefile.bc %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\dupefind
make -fmakefile.bc %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\echo
make %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\execsbbs
call make.cmd %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\filelist
make -fmakefile.bc %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\install
make %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\node
call make.cmd %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\slog
call make.cmd %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\smb
cd chksmb
make %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\fixsmb
make %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\smbutil
make -fmakefile.bc %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..
cd ..\smbactiv
make -fmakefile.bc %1 %2 %3 %4 %5
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
