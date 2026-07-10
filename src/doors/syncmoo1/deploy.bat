@echo off
rem ===========================================================================
rem deploy.bat - Install the already-built syncmoo1.exe into the door's xtrn
rem              dir(s).  Run AFTER build.bat, when you actually want the running
rem              door updated -- building does not deploy automatically, so a
rem              sysop can rebuild and test before pushing a new binary live.
rem
rem   Usage:  deploy.bat            (deploy build-msvc\Release\syncmoo1.exe)
rem
rem   Env:    SYNCMOO1_DEST=<dir>   also deploy into this live xtrn\syncmoo1 dir
rem                                 (overrides the %SBBSCTRL% auto-detect)
rem
rem Deploys build-msvc\Release\syncmoo1.exe to two destinations:
rem   1. THIS tree's ..\..\..\xtrn\syncmoo1\ -- the in-tree door bundle, next to
rem      install-xtrn.ini / syncmoo1.example.ini / getdata.js.
rem   2. The live install located via %SBBSCTRL% (root = %SBBSCTRL%\..) or
rem      %SYNCMOO1_DEST% -- for copy-style installs where the in-tree dir is not
rem      the live xtrn.
rem The binary is a git-ignored build artifact, not carried to a live install by
rem `git pull` / `update.js`, so this script is what puts it where the door reads it.
rem
rem The MoO1 LBX data files (not shipped -- see README.md) still need to be
rem supplied separately per install; getdata.js installs them from a copy you
rem place in the door directory.
rem ===========================================================================
setlocal enabledelayedexpansion

rem --- Source dir = location of this script (no trailing backslash) ----------
set "SRCDIR=%~dp0"
set "SRCDIR=%SRCDIR:~0,-1%"

rem --- build.bat builds Win32/Release into build-msvc\Release\ (Win32 is the ---
rem --- one supported Windows target -- see build.bat's header).
set "CONFIG=Release"

set "EXE=%SRCDIR%\build-msvc\%CONFIG%\syncmoo1.exe"
set "DESTDIR=%SRCDIR%\..\..\..\xtrn\syncmoo1"

if not exist "%EXE%" (
    echo [deploy] ERROR: %EXE% not found -- run build.bat first
    goto error
)

call :deploy "%DESTDIR%"
if errorlevel 1 goto error

rem --- Copy-style installs: deploy to the live install too -------------------
set "LIVE=%SYNCMOO1_DEST%"
if not defined LIVE if defined SBBSCTRL set "LIVE=%SBBSCTRL%\..\xtrn\syncmoo1"
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
if exist "%DST%\syncmoo1.exe" (
    fc /b "%EXE%" "%DST%\syncmoo1.exe" >nul 2>nul
    if not errorlevel 1 (
        echo [deploy] %DST%\syncmoo1.exe already up to date -- nothing to copy
        exit /b 0
    )
)
copy /Y "%EXE%" "%DST%\syncmoo1.exe" >nul
if errorlevel 1 exit /b 1
for %%I in ("%DST%\syncmoo1.exe") do echo [deploy] Deployed: %%~fI
exit /b 0

:error
echo.
echo [deploy] FAILED (errorlevel %errorlevel%)
endlocal
exit /b 1
