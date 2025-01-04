@echo off
setlocal
cd ..\exec
make
if errorlevel 1 goto fail
cd ..\src\sbbs3
call release.bat /t:Clean
call release.bat
if errorlevel 1 goto fail
cd ctrl
build /B
if errorlevel 1 goto fail
cd ..\useredit
build /B
if errorlevel 1 goto fail
cd ..\chat
build /B
:fail
echo FAILED: See errors above!
:end
