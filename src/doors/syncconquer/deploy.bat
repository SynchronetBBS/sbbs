@echo off
rem ===========================================================================
rem deploy.bat - Install the already-built syncalert.exe into the door's xtrn
rem              dir(s).  Run AFTER build.bat, when you actually want the running
rem              door updated -- building does not deploy automatically, so a
rem              sysop can rebuild and test before pushing a new binary live.
rem
rem   Usage:  deploy.bat            (deploy build-msvc\Release\syncalert.exe)
rem
rem   Env:    SYNCALERT_DEST=<dir>  also deploy into this live xtrn\syncalert dir
rem                                 (overrides the %SBBSCTRL% auto-detect)
rem
rem Deploys build-msvc\Release\syncalert.exe to two destinations:
rem   1. THIS tree's ..\..\..\xtrn\syncalert\ -- the in-tree door bundle, next to
rem      install-xtrn.ini / fetch-assets.js.
rem   2. The live install located via %SBBSCTRL% (root = %SBBSCTRL%\..) or
rem      %SYNCALERT_DEST% -- for copy-style installs where the in-tree dir is not
rem      the live xtrn.
rem The binary is a git-ignored build artifact, not carried to a live install by
rem `git pull` / `update.js`, so this script is what puts it where the door reads it.
rem
rem The exe is deployed one level ABOVE xtrn\syncalert\assets\ (the REDALERT.MIX/
rem MAIN.MIX dir that fetch-assets.js populates) because door_io.c's default
rem asset-dir resolution is "<the binary's own directory>\assets" -- so a stock
rem install-xtrn.ini launch (no -assets arg) finds the game data with zero extra
rem sysop configuration.
rem
rem For a first-time install, register the door -- the installer offers to fetch
rem the freeware RA95 game data for you:
rem     jsexec install-xtrn ..\xtrn\syncalert
rem
rem Unlike src/doors/syncduke/deploy.bat and src/doors/syncdoom/deploy.bat, an
rem AUTO-DETECTED live dir that doesn't exist is skipped rather than created:
rem %SBBSCTRL% pointing at a BBS with no syncalert installed should not have a
rem stray xtrn\syncalert\ conjured into it.  An explicit %SYNCALERT_DEST% is
rem still created on demand.  This matches this door's own deploy.sh.
rem ===========================================================================
setlocal enabledelayedexpansion

rem --- Source dir = location of this script (no trailing backslash) ----------
set "SRCDIR=%~dp0"
set "SRCDIR=%SRCDIR:~0,-1%"

rem --- The MSVC multi-config generator puts both x86 and x64 builds under -----
rem --- build-msvc\Release\, so there is nothing to select here.
set "CONFIG=Release"

set "EXE=%SRCDIR%\build-msvc\%CONFIG%\syncalert.exe"
set "DESTDIR=%SRCDIR%\..\..\..\xtrn\syncalert"

if not exist "%EXE%" (
    echo [deploy] ERROR: %EXE% not found -- run build.bat first
    goto error
)

rem --- 1. In-tree door bundle ------------------------------------------------
call :deploy "%DESTDIR%"
if errorlevel 1 goto error

rem --- 2. Copy-style installs: deploy to the live install too -----------------
set "LIVE=%SYNCALERT_DEST%"
if not defined LIVE if defined SBBSCTRL set "LIVE=%SBBSCTRL%\..\xtrn\syncalert"
if not defined LIVE goto done

rem Resolve both to full paths so the "same dir as the in-tree bundle" check
rem survives the ..\ segments above (%SBBSCTRL%\..\ and this script's ..\..\..\).
for %%I in ("%DESTDIR%") do set "DESTFULL=%%~fI"
for %%I in ("%LIVE%")    do set "LIVEFULL=%%~fI"
if /I "%DESTFULL%"=="%LIVEFULL%" goto done

if defined SYNCALERT_DEST (
    call :deploy "%LIVEFULL%"
    if errorlevel 1 goto error
) else if exist "%LIVEFULL%\" (
    call :deploy "%LIVEFULL%"
    if errorlevel 1 goto error
) else (
    echo [deploy] Live install dir not found at %LIVEFULL% -- skipping
    echo [deploy] ^(run install-xtrn there for a first install^)
)

:done
endlocal
exit /b 0

rem --- deploy <destdir> : copy the exe unless the dest already matches -------
:deploy
set "DST=%~f1"
if not exist "%DST%\" mkdir "%DST%"
if exist "%DST%\syncalert.exe" (
    fc /b "%EXE%" "%DST%\syncalert.exe" >nul 2>nul
    if not errorlevel 1 (
        echo [deploy] %DST%\syncalert.exe already up to date -- nothing to copy
        exit /b 0
    )
)
copy /Y "%EXE%" "%DST%\syncalert.exe" >nul
if errorlevel 1 exit /b 1
for %%I in ("%DST%\syncalert.exe") do echo [deploy] Deployed: %%~fI
exit /b 0

:error
echo.
echo [deploy] FAILED (errorlevel %errorlevel%)
endlocal
exit /b 1
