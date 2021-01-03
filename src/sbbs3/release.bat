@echo off
call gitinfo.bat
call build.bat "/p:Configuration=Release" %*