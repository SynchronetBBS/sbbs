@echo off
rem
rem Makes all Borland for DOS Synchronet projects - April 1997 Rob Swindell
rem
cd sbbs
make %1 %2 %3 %4 %5
if errorlevel 1 goto err
make -DW32 %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd scfg
make %1 %2 %3 %4 %5
if errorlevel 1 goto err
make -DDOS32 %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\addfiles
make -fmakefile.bc %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\allusers
call make.bat %1 %2 %3 %4 %5 
if errorlevel 1 goto err
cd ..\baja
call make.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\delfiles
make -fmakefile.bc %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\dstsedit
call make.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\dupefind
make -fmakefile.bc %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\echo
make %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\execdos
call make.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\execsbbs
call make.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\filelist
make -fmakefile.bc %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\install
make %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\mlabels
call make.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\node
call make.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\qwknodes
call make.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\sbj
make %1 %2 %3 %4 %5 
if errorlevel 1 goto err
cd ..\sbl
make %1 %2 %3 %4 %5 
if errorlevel 1 goto err
call makeio.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
cd ..\scb
make %1 %2 %3 %4 %5 
if errorlevel 1 goto err
make -fscbcfg.mak %1 %2 %3 %4 %5 
if errorlevel 1 goto err
cd ..\slog
call make.bat %1 %2 %3 %4 %5
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
cd ..\smm
make %1 %2 %3 %4 %5 
if errorlevel 1 goto err
make -fsmmcfg.mak %1 %2 %3 %4 %5 
if errorlevel 1 goto err
call makeio.bat %1 %2 %3 %4 %5
if errorlevel 1 goto err
call makephot.bat  %1 %2 %3 %4 %5 
if errorlevel 1 goto err
call makeutil.batn %1 %2 %3 %4 %5 
if errorlevel 1 goto err
cd ..\uti
make %1 %2 %3 %4 %5 
if errorlevel 1 goto err
rem
rem Non-Synchronet specific stuff
rem
cd ..\
cd ..\tone
call make.bat %1 %2 %3 %4 %5 
if errorlevel 1 goto err
cd ..\stp
call make.bat %1 %2 %3 %4 %5 
if errorlevel 1 goto err
rem
rem End of makes
rem
cd ..
goto end
:err
rem Error occurred
echo.
echo There was an error!
echo.
:end
