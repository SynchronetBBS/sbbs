@echo off
rem ===========================================================================
rem build.bat - Configure and build the Windows / MSVC (Release) build of syncretro.
rem
rem   Usage:  build.bat             (Win32 release, JPEG-XL via vcpkg if present)
rem           build.bat clean       (delete the build tree first, then build)
rem
rem Win32 (x86) is the one supported Windows target -- deliberately. A Win32 door
rem runs on both a Win32 and a (future) Win64 Synchronet host: the DOOR32.SYS comm
rem handle is 32-bit-significant and crosses the process-bitness boundary fine, so
rem one Win32 binary covers every Windows BBS, present and future. The frontend is
rem 64-bit-clean (our glue is; the libretro cores are separate .dll content the
rem sysop supplies to match), so `cmake -A x64` still compiles -- we just don't
rem ship or test an x64 binary, and a Win32 door must load a Win32 core .dll.
rem
rem Uses Visual Studio 2022 and classic-mode vcpkg for the static libjxl (the
rem JPEG-XL / SyncTERM graphics tier). When the vcpkg prefix is absent the
rem configure step falls back to the sixel/text tiers with a warning - the door
rem still builds. libsndfile (Ogg audio) is likewise optional: without it the
rem door is simply silent, a first-class state the audio module already handles.
rem
rem Building does NOT install the binary -- it is left at
rem build-msvc\Release\syncretro.exe.  Run `jsexec deploy.js` afterwards when you
rem actually want the running door updated, so you can rebuild and test first.
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
set "EXE=%BUILDDIR%\%CONFIG%\syncretro.exe"
if not exist "%EXE%" (
    echo [build] ERROR: expected output not found: %EXE%
    goto error
)

for %%I in ("%EXE%") do set "BUILT=%%~fI"
echo.
echo [build] Built: %BUILT%
echo [build] Run 'jsexec deploy.js' to install it into the door's xtrn dir.
endlocal
exit /b 0

:error
echo.
echo [build] FAILED (errorlevel %errorlevel%)
endlocal
exit /b 1
