@echo off
call makelibs.bat
bpr2mak sbbsctrl.bpr
echo.
make %1 %2 %3 %4 %5 %6 -f sbbsctrl.mak