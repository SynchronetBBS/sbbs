@echo off
setlocal
rem *** Requires Microsoft Visual C++ 2013 ***
call "%VS120COMNTOOLS%\vsvars32.bat"
msbuild sbbs3.sln %1 %2 %3 %4 %5
if errorlevel 1 echo. & echo !ERROR(s) occurred & exit /b 1