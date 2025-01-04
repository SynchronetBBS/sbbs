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
call build /B
if errorlevel 1 goto fail
cd ..\useredit
call build /B
if errorlevel 1 goto fail
cd ..\chat
call build /B
if errorlevel 1 goto fail
goto end
:fail
echo FAILED: See errors above!
:end
