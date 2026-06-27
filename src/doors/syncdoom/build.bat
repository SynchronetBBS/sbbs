@echo off
rem ===========================================================================
rem build.bat - Configure and build the Win32 / MSVC (Release) build of SyncDOOM.
rem
rem   Usage:  build.bat            (Win32 release, JPEG-XL via vcpkg if present)
rem           build.bat x64        (64-bit release)
rem           build.bat clean      (delete the build tree, then build Win32)
rem           build.bat x64 clean  (combine)
rem
rem Mirrors the configuration documented in COMPILING.md: Visual Studio 2022,
rem classic-mode vcpkg for the static libjxl (JPEG-XL graphics tier).  When the
rem vcpkg prefix is absent the configure step falls back to the sixel/text
rem tiers with a warning - the door still builds.
rem
rem Building does NOT install the binary -- it is left at
rem build-msvc\Release\syncdoom.exe.  Run deploy.bat afterwards when you actually
rem want the running door updated, so you can rebuild and test first.
rem ===========================================================================
setlocal enabledelayedexpansion

rem --- Source dir = location of this script (no trailing backslash) ----------
set "SRCDIR=%~dp0"
set "SRCDIR=%SRCDIR:~0,-1%"

rem --- Defaults --------------------------------------------------------------
set "GENERATOR=Visual Studio 17 2022"
set "PLATFORM=Win32"
set "CONFIG=Release"
set "TRIPLET=x86-windows-static-md"
set "DOCLEAN="

rem --- Parse arguments (order-independent: "x64" and/or "clean") -------------
:parseargs
if "%~1"=="" goto argsdone
if /I "%~1"=="x64"   ( set "PLATFORM=x64"  & set "TRIPLET=x64-windows-static-md" )
if /I "%~1"=="Win32" ( set "PLATFORM=Win32" & set "TRIPLET=x86-windows-static-md" )
if /I "%~1"=="clean" ( set "DOCLEAN=1" )
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

rem --- Optional JPEG-XL prefix (classic vcpkg mode) --------------------------
set "PREFIXARG="
if exist "%VCPKG_PREFIX%" (
    set "PREFIXARG=-DCMAKE_PREFIX_PATH=%VCPKG_PREFIX%"
    echo [build] JPEG-XL: using vcpkg prefix %VCPKG_PREFIX%
) else (
    echo [build] JPEG-XL: vcpkg prefix not found ^(%VCPKG_PREFIX%^); building sixel/text tiers only
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
set "EXE=%BUILDDIR%\%CONFIG%\syncdoom.exe"
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
