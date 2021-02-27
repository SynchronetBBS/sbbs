@echo off
wmake OS=DOS %1 %2 %3 %4 %5
if errorlevel 1 goto end
wmake OS=DOS4G %1 %2 %3 %4 %5
if errorlevel 1 goto end
wmake OS=OS2 %1 %2 %3 %4 %5
if errorlevel 1 goto end
wmake OS=NT %1 %2 %3 %4 %5
if errorlevel 1 goto end
:end

