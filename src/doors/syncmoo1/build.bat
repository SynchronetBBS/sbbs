@echo off
rem ===========================================================================
rem build.bat - Configure and build the Windows / MSVC (Release) build of syncmoo1.
rem
rem   Usage:  build.bat             (Win32 release, JPEG-XL via vcpkg if present)
rem           build.bat clean       (delete the build tree first, then build)
rem
rem Win32 (x86) is the one supported Windows target -- deliberately. A Win32 door
rem runs on both a Win32 and a (future) Win64 Synchronet host: the DOOR32.SYS comm
rem handle is 32-bit-significant and crosses the process-bitness boundary fine, so
rem one Win32 binary covers every Windows BBS, present and future. MoO1 through
rem 1oom is a 320x200 game that gains nothing from x64, and Win32 gets the JPEG-XL
rem tier for free from the x86-windows-static-md vcpkg triplet. The door's code is
rem 64-bit-clean (1oom is; our glue is), so `cmake -A x64` still compiles -- we
rem just don't ship or test an x64 binary.
rem
rem Uses Visual Studio 2022 and classic-mode vcpkg for the static libjxl. When the
rem vcpkg prefix is absent the configure step falls back to the sixel/text tiers
rem with a warning - the door still builds.
rem
rem Building does NOT install the binary -- it is left at
rem build-msvc\Release\syncmoo1.exe.  Run deploy.bat afterwards when you actually
rem want the running door updated, so you can rebuild and test first.
rem ===========================================================================
setlocal enabledelayedexpansion

rem --- Source dir = location of this script (no trailing backslash) ----------
set "SRCDIR=%~dp0"
set "SRCDIR=%SRCDIR:~0,-1%"

rem --- Fixed target: Win32 (see the header for why) --------------------------
set "GENERATOR=Visual Studio 17 2022"
set "PLATFORM=Win32"
set "CONFIG=Release"
set "TRIPLET=x86-windows-static-md"
set "DOCLEAN="

rem --- Parse arguments (only "clean") ----------------------------------------
:parseargs
if "%~1"=="" goto argsdone
if /I "%~1"=="clean" ( set "DOCLEAN=1" ) else ( echo [build] ignoring unknown argument '%~1' )
shift
goto parseargs
:argsdone

set "BUILDDIR=%SRCDIR%\build-msvc"
set "VCPKG_PREFIX=C:\vcpkg\installed\%TRIPLET%"

rem --- Locate cmake (PATH, else the VS 2022 bundled copy) --------------------
set "CMAKE=cmake"
where cmake >nul 2>nul
if errorlevel 1 (
    for %%E in (Professional Enterprise Community BuildTools) do (
        set "_C=C:\Program Files\Microsoft Visual Studio\2022\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if exist "!_C!" set "CMAKE=!_C!"
    )
)

rem --- Optional vcpkg prefix (classic mode): libjxl --------------------------
rem   vcpkg install libjxl:x86-windows-static-md
set "PREFIXARG="
if exist "%VCPKG_PREFIX%" (
    set "PREFIXARG=-DCMAKE_PREFIX_PATH=%VCPKG_PREFIX%"
    echo [build] vcpkg prefix %VCPKG_PREFIX% ^(libjxl if installed^)
) else (
    echo [build] vcpkg prefix not found ^(%VCPKG_PREFIX%^); sixel/text tiers only
)

rem --- Clean if requested ----------------------------------------------------
if defined DOCLEAN (
    echo [build] Removing build tree %BUILDDIR%
    if exist "%BUILDDIR%" rmdir /S /Q "%BUILDDIR%"
)

rem --- Configure -------------------------------------------------------------
echo [build] Configuring %PLATFORM% (%CONFIG%) ...
"%CMAKE%" -S "%SRCDIR%" -B "%BUILDDIR%" -G "%GENERATOR%" -A %PLATFORM% %PREFIXARG%
if errorlevel 1 goto error

rem --- Build -----------------------------------------------------------------
echo [build] Building ...
"%CMAKE%" --build "%BUILDDIR%" --config %CONFIG%
if errorlevel 1 goto error

rem --- Confirm the build produced the binary --------------------------------
set "EXE=%BUILDDIR%\%CONFIG%\syncmoo1.exe"
if not exist "%EXE%" (
    echo [build] ERROR: expected output not found: %EXE%
    goto error
)

for %%I in ("%EXE%") do set "BUILT=%%~fI"
echo.
echo [build] Built: %BUILT%
echo [build] Run deploy.bat to install it into the door's xtrn dir.
endlocal
exit /b 0

:error
echo.
echo [build] FAILED (errorlevel %errorlevel%)
endlocal
exit /b 1
