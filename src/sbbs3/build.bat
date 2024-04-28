@echo off
setlocal
rem *** Requires Microsoft Visual C++ 2022 ***
call "%VS170COMNTOOLS%\VsMSBuildCmd.bat"
msbuild sbbs3.sln /p:Platform="Win32" /p:XPDeprecationWarning=false %*
if errorlevel 1 echo. & echo !ERROR(s) occurred & exit /b 1