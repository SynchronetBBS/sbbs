@echo off
call ../build/gitinfo.bat
call build.bat "/p:Configuration=Release" %*