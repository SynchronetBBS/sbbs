@echo off
setlocal
rem *** Requires Microsoft Visual C++ 2019 ***
call "%VS160COMNTOOLS%\VsMSBuildCmd.bat"
msbuild sbbs3.sln /p:Platform="Win32" %1 %2 %3 %4 %5
if errorlevel 1 echo. & echo !ERROR(s) occurred & exit /b 1