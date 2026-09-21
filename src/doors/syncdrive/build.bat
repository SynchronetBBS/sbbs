@echo off
rem ===========================================================================
rem build.bat - Configure and build the Windows / MSVC (Release) build of
rem             syncdrive, and run the unit tests.
rem
rem   Usage:  build.bat             (Win32 release + unit tests)
rem           build.bat clean       (delete the build tree first, then build)
rem
rem Win32 (x86) is the one supported Windows target -- deliberately. A Win32
rem door runs on both a Win32 and a Win64 Synchronet host: the DOOR32.SYS comm
rem handle is 32-bit-significant and crosses the process-bitness boundary fine,
rem so one Win32 binary covers every Windows BBS. Test Drive is a 320x200 EGA
rem game emulated a byte at a time; nothing about it wants 64-bit. The code is
rem 64-bit-clean, so `cmake -A x64` still compiles -- we just don't ship it.
rem
rem Uses classic-mode vcpkg for the static libsndfile, which the door's audio
rem REQUIRES to be audible: the PC-speaker PCM is generated in-process, but
rem termgfx streams it to SyncTERM as Ogg/Opus, and that encoder is a no-op stub
rem when termgfx is built without libsndfile -- every chunk is then dropped and
rem the door runs silent (termgfx/audio_stream.c, termgfx_stream_chunk_closed).
rem When the vcpkg prefix is missing this script warns and builds on: the door
rem works, without sound.
rem     vcpkg install libsndfile:x86-windows-static-md
rem No libjxl, unlike the siblings: this door serves the sixel tier only (see
rem DESIGN.md sec. 1).
rem
rem Building does NOT install the binary -- it is left at
rem build-msvc\Release\syncdrive.exe. Run `jsexec deploy.js` afterwards when you
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
rem Resolved to a full path even when it is on PATH, so ctest can be taken from
rem the same bin directory below -- the VS-bundled CMake is on nobody's PATH.
set "CMAKE="
for /f "delims=" %%I in ('where cmake 2^>nul') do if not defined CMAKE set "CMAKE=%%I"
if not defined CMAKE (
    for %%E in (Professional Enterprise Community BuildTools) do (
        set "_C=C:\Program Files\Microsoft Visual Studio\2022\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if not defined CMAKE if exist "!_C!" set "CMAKE=!_C!"
    )
)
if not defined CMAKE (
    echo [build] ERROR: cmake.exe not found on PATH or in Visual Studio 2022.
    goto error
)
for %%I in ("!CMAKE!") do set "CTEST=%%~dpIctest.exe"
if not exist "!CTEST!" set "CTEST=ctest"

rem --- vcpkg prefix (classic mode): libsndfile, i.e. audible sound ------------
set "PREFIXARG="
if exist "%VCPKG_PREFIX%" (
    set "PREFIXARG=-DCMAKE_PREFIX_PATH=%VCPKG_PREFIX%"
    echo [build] vcpkg prefix %VCPKG_PREFIX% ^(libsndfile if installed^)
) else (
    echo [build] WARNING: vcpkg prefix not found ^(%VCPKG_PREFIX%^) -- the door will
    echo [build] WARNING: build but run SILENT. See the header of this script.
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

rem --- Unit tests ------------------------------------------------------------
rem The door's pure modules (frame/keymap/keyscript/speaker/alias), same set as
rem build.sh runs, minus the fork-based lock test (*nix only, see
rem tests\CMakeLists.txt). Cheap enough to be part of every build.
echo [build] Running unit tests ...
pushd "%BUILDDIR%"
"%CTEST%" --build-config %CONFIG% --output-on-failure
set "SB_RC=!errorlevel!"
popd
if not "!SB_RC!"=="0" goto error

rem --- Confirm the build produced the binary ---------------------------------
set "EXE=%BUILDDIR%\%CONFIG%\syncdrive.exe"
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
