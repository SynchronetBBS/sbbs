@echo off
setlocal
rem *** Requires Microsoft Visual C++ 2019 ***
call "%VS160COMNTOOLS%\VsMSBuildCmd.bat"
msbuild syncterm.sln /p:Platform="Win32" %*
if errorlevel 1 echo. & echo !ERROR(s) occurred & exit /b 1