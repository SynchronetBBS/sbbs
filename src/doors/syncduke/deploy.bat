@echo off
rem ===========================================================================
rem deploy.bat - Install the already-built syncduke.exe into the door's xtrn
rem              dir(s).  Run AFTER build.bat, when you actually want the running
rem              door updated -- building no longer deploys automatically, so a
rem              sysop can rebuild and test before pushing a new binary live.
rem
rem   Usage:  deploy.bat            (deploy build-msvc\Release\syncduke.exe)
rem
rem   Env:    SYNCDUKE_DEST=<dir>   also deploy into this live xtrn\syncduke dir
rem                                 (overrides the %SBBSCTRL% auto-detect)
rem
rem Deploys build-msvc\Release\syncduke.exe to two destinations:
rem   1. THIS tree's ..\..\..\xtrn\syncduke\ -- the in-tree door bundle, next to
rem      install-xtrn.ini / syncduke.example.ini.
rem   2. The live install located via %SBBSCTRL% (root = %SBBSCTRL%\..) or
rem      %SYNCDUKE_DEST% -- for copy-style installs where the in-tree dir is not
rem      the live xtrn.
rem The binary is a git-ignored build artifact, not carried to a live install by
rem `git pull` / `update.js`, so this script is what puts it where the door reads it.
rem ===========================================================================
setlocal enabledelayedexpansion

rem --- Source dir = location of this script (no trailing backslash) ----------
set "SRCDIR=%~dp0"
set "SRCDIR=%SRCDIR:~0,-1%"

rem --- The MSVC multi-config generator puts both x86 and x64 builds under -----
rem --- build-msvc\Release\, so there is nothing to select here.
set "CONFIG=Release"

set "EXE=%SRCDIR%\build-msvc\%CONFIG%\syncduke.exe"
set "DESTDIR=%SRCDIR%\..\..\..\xtrn\syncduke"

if not exist "%EXE%" (
    echo [deploy] ERROR: %EXE% not found -- run build.bat first
    goto error
)

call :deploy "%DESTDIR%"
if errorlevel 1 goto error

rem --- Copy-style installs: deploy to the live install too -------------------
set "LIVE=%SYNCDUKE_DEST%"
if not defined LIVE if defined SBBSCTRL set "LIVE=%SBBSCTRL%\..\xtrn\syncduke"
if defined LIVE (
    call :deploy "%LIVE%"
    if errorlevel 1 goto error
)

endlocal
exit /b 0

rem --- deploy <destdir> : copy the exe unless the dest already matches -------
:deploy
set "DST=%~1"
if not exist "%DST%" mkdir "%DST%"
if exist "%DST%\syncduke.exe" (
    fc /b "%EXE%" "%DST%\syncduke.exe" >nul 2>nul
    if not errorlevel 1 (
        echo [deploy] %DST%\syncduke.exe already up to date -- nothing to copy
        exit /b 0
    )
)
copy /Y "%EXE%" "%DST%\syncduke.exe" >nul
if errorlevel 1 exit /b 1
for %%I in ("%DST%\syncduke.exe") do echo [deploy] Deployed: %%~fI
exit /b 0

:error
echo.
echo [deploy] FAILED (errorlevel %errorlevel%)
endlocal
exit /b 1
