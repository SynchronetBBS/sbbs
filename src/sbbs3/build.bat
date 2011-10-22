@echo off
setlocal
rem *** Requires Microsoft Visual C++ 2010 ***
call "%VS100COMNTOOLS%\vsvars32.bat"
msbuild sbbs3.sln %1 %2 %3 %4 %5
if errorlevel 1 echo. & echo !ERROR(s) occurred