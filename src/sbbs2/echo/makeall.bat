@echo off
make
if errorlevel 1 goto end
wmake -f sbbsecho.wat OS=OS2
if errorlevel 1 goto end
wmake -f sbbsecho.wat OS=DOS4G
if errorlevel 1 goto end
wmake -f sbbsecho.wat OS=NT
:end