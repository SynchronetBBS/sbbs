@echo off
wmake
if errorlevel 1 goto end
wmake OS=DOSX
if errorlevel 1 goto end
wmake OS=OS2
:end
