@echo off
rem ===========================================================================
rem build.bat - Configure and build the Win32 / MSVC (Release) build of
rem             SyncConquer (the syncalert door).
rem
rem   Usage:  build.bat            (Win32 release, JPEG-XL via vcpkg if present)
rem           build.bat clean      (delete the build tree, then build Win32)
rem
rem Mirrors src/doors/syncduke/build.bat and src/doors/syncdoom/build.bat:
rem Visual Studio 2022, classic-mode vcpkg for the static libjxl (JPEG-XL
rem graphics tier) and libsndfile (OGG music tier).  When the vcpkg prefix is
rem absent the configure step falls back to the sixel/text tiers and raw-PCM
rem music with a warning - the door still builds.
rem
rem Building does NOT install the binary -- it is left at
rem build-msvc\Release\syncalert.exe (the project is syncconquer; the door's
rem binary is syncalert).  Run deploy.bat afterwards when you actually want the
rem running door updated, so you can rebuild and test first.
rem
rem Win32 (x86) ONLY -- unlike the siblings, this door has no x64 option.  The
rem vendored Vanilla Conquer engine's Windows-without-SDL path (the one the
rem headless door build selects) is full of Win16-era 32-bit assumptions that
rem upstream never compiles, because upstream's Windows build always defines
rem SDL_BUILD and skips them: wwmouse.cpp passes a DWORD-argument callback to
rem timeSetEvent (whose LPTIMECALLBACK takes DWORD_PTR), winstub.cpp assigns a
rem `long(HWND,UINT,UINT,LONG)` to a WNDPROC, and so on.  Each is a hard MSVC
rem error at x64 and harmless at x86, where DWORD/LONG are pointer-width.
rem Porting that path is an upstream job, so x64 is refused up front rather
rem than failing deep into the build.  See COMPILING.md.
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

rem --- Parse arguments ("clean"; "x64" is explicitly refused, see header) ----
:parseargs
if "%~1"=="" goto argsdone
if /I "%~1"=="x64"   goto no_x64
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

rem --- Optional vcpkg prefix (classic mode): libjxl + libsndfile -------------
rem   vcpkg install libjxl:%TRIPLET% libsndfile:%TRIPLET%
set "PREFIXARG="
if exist "%VCPKG_PREFIX%" (
    set "PREFIXARG=-DCMAKE_PREFIX_PATH=%VCPKG_PREFIX%"
    echo [build] vcpkg prefix %VCPKG_PREFIX% ^(libjxl / libsndfile if installed^)
) else (
    echo [build] vcpkg prefix not found ^(%VCPKG_PREFIX%^); sixel/text tiers, raw-PCM music
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
set "EXE=%BUILDDIR%\%CONFIG%\syncalert.exe"
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

:no_x64
echo [build] ERROR: x64 is not supported by this door.
echo [build] The vendored Vanilla Conquer engine's Windows-without-SDL path has
echo [build] 32-bit assumptions (timeSetEvent/WNDPROC callback signatures) that
echo [build] are hard MSVC errors at x64. Build Win32: build.bat
echo [build] See COMPILING.md.
endlocal
exit /b 1

:error
echo.
echo [build] FAILED (errorlevel %errorlevel%)
endlocal
exit /b 1
