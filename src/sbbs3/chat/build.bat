@echo off
bpr2mak chat.bpr
echo.
make %1 %2 %3 %4 %5 %6 -f chat.mak