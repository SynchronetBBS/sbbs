@echo off
rem *** Requires Microsoft Visual C++ v6.0 ***
"C:\Program Files\Microsoft Visual Studio\Common\MSDev98\Bin\msdev" scfg.dsw /MAKE ALL %1 %2 %3 %4 %5
if errorlevel 1 echo. & echo !ERROR(s) occurred