@echo off
rem ===========================================================================
rem build.bat -- Win32 / MSVC (Release) build of the SyncRPG door (syncrpg.exe:
rem              the vendored EasyRPG Player engine behind our termgfx BaseUi
rem              backend). The *nix counterpart is build.sh, and unlike the
rem              SyncSCUMM door -- whose engine ships a bespoke MSVC project
rem              generator -- EasyRPG Player builds with CMake on every
rem              platform, so this is the SAME two-stage build as build.sh,
rem              just driven through the MSVC toolchain:
rem
rem   1. Enter the VS 2022 x86 developer environment (x64-hosted tools).
rem   2. Install/verify the vcpkg dependencies (x86-windows-static-md).
rem   3. Stage 1 -- termgfx + xpdev + inih static libs (door/CMakeLists.txt).
rem   4. Stage 2 -- the vendored EasyRPG Player as a CMake OBJECT library plus
rem      the door's own sources (CMakeLists.txt), linked against Stage 1.
rem
rem   Usage:  build.bat            (Win32 Release)
rem           build.bat clean      (delete the build tree, then build)
rem
rem The binary is left at build-msvc\Release\syncrpg.exe (NOT installed). Run
rem `jsexec deploy.js` afterwards to copy it into the installed SyncRPG
rem package. Win32 (x86) only -- matches the sibling termgfx doors' triplet.
rem
rem See COMPILING.md for the full story: what each stage does, why the
rem Ninja generator, and how to diagnose a failure in either stage.
rem ===========================================================================
setlocal enabledelayedexpansion

set "SRCDIR=%~dp0"
set "SRCDIR=%SRCDIR:~0,-1%"
set "BUILDDIR=%SRCDIR%\build-msvc"
set "CONFIG=Release"
set "TRIPLET=x86-windows-static-md"
rem Stash any caller-supplied VCPKG_ROOT: VsDevCmd.bat (below) OVERWRITES the
rem variable with VS 2022's own bundled vcpkg
rem (VC\vcpkg\installed\, which has none of our ports), and a stale value there
rem is silent -- CMake configures fine and just records libjxl/libsndfile as
rem NOTFOUND, degrading the door to the sixel + raw-PCM tiers. Restored after
rem the call.
set "USER_VCPKG_ROOT=%VCPKG_ROOT%"

rem --- Parse args ------------------------------------------------------------
set "DOCLEAN="
:parseargs
if "%~1"=="" goto argsdone
if /I "%~1"=="clean" set "DOCLEAN=1"
if /I "%~1"=="x64"   goto no_x64
shift
goto parseargs
:argsdone

rem --- 1. VS 2022 developer environment: x86 target, x64-hosted compiler -----
rem -host_arch=x64 picks HostX64\x86\cl.exe. The 32-bit hosted compiler runs
rem out of its ~3 GB address space optimizing the larger EasyRPG/liblcf
rem translation units under a parallel build (the same reason SyncSCUMM's
rem Directory.Build.props sets PreferredToolArchitecture=x64).
set "VSDEV="
for %%E in (Professional Enterprise Community BuildTools) do (
    if not defined VSDEV (
        set "_V=C:\Program Files\Microsoft Visual Studio\2022\%%E\Common7\Tools\VsDevCmd.bat"
        if exist "!_V!" (
            set "VSDEV=!_V!"
            set "VSROOT=C:\Program Files\Microsoft Visual Studio\2022\%%E"
        )
    )
)
if not defined VSDEV (
    echo [build] ERROR: VS 2022 VsDevCmd.bat not found.
    goto error
)
echo [build] Entering the VS 2022 x86 build environment ...
call "%VSDEV%" -arch=x86 -host_arch=x64 -no_logo
if errorlevel 1 goto error

rem Undo VsDevCmd's VCPKG_ROOT (see the stash above).
if defined USER_VCPKG_ROOT (set "VCPKG_ROOT=%USER_VCPKG_ROOT%") else (set "VCPKG_ROOT=C:\vcpkg")
set "VCPKG_PREFIX=%VCPKG_ROOT%\installed\%TRIPLET%"
echo [build] vcpkg root: %VCPKG_ROOT%

rem CMake and Ninja: on PATH after VsDevCmd when the "C++ CMake tools for
rem Windows" component is installed; else fall back to the copies VS ships.
set "CMAKE=cmake"
where cmake >nul 2>nul || set "CMAKE=%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA=ninja"
where ninja >nul 2>nul || set "NINJA=%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not exist "%CMAKE%" if /I not "%CMAKE%"=="cmake" (
    echo [build] ERROR: cmake.exe not found ^(install the "C++ CMake tools for Windows" VS component^).
    goto error
)

rem --- Clean -----------------------------------------------------------------
if defined DOCLEAN (
    echo [build] Removing build tree %BUILDDIR%
    if exist "%BUILDDIR%" rmdir /S /Q "%BUILDDIR%"
)

rem --- 2. vcpkg dependencies -------------------------------------------------
rem EasyRPG Player's hard requirements (liblcf's too) plus the optional media
rem decoders it and termgfx use. SDL2 is required by CMake even though no
rem Sdl2Ui is ever constructed: PLAYER_TARGET_PLATFORM has no "none" option and
rem the only alternative (libretro) needs builds/libretro, which the vendoring
rem pass pruned. See COMPILING.md.
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo [build] ERROR: vcpkg not found at %VCPKG_ROOT% ^(set VCPKG_ROOT^).
    goto error
)
echo [build] Verifying vcpkg dependencies ^(%TRIPLET%^) ...
"%VCPKG_ROOT%\vcpkg.exe" install --triplet %TRIPLET% ^
  fmt pixman harfbuzz sdl2 expat icu nlohmann-json ^
  opusfile speexdsp libsamplerate mpg123 libsndfile libvorbis ^
  libpng freetype zlib libjxl
if errorlevel 1 goto error

rem --- 3. Stage 1: termgfx + xpdev + inih static libraries -------------------
echo [build] Stage 1: termgfx + xpdev + inih ...
"%CMAKE%" -S "%SRCDIR%\door" -B "%BUILDDIR%\libs" -G Ninja ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_BUILD_TYPE=%CONFIG% ^
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=%TRIPLET%
if errorlevel 1 goto error
"%CMAKE%" --build "%BUILDDIR%\libs" --target termgfx xpdev_static inih
if errorlevel 1 goto error

rem libtermgfx's own externals (the JPEG XL graphics tier, libsndfile Opus/Ogg
rem encode) do not survive the Stage-1-CMake -> Stage-2-CMake seam, because
rem Stage 2 links the archive by absolute path rather than through its CMake
rem target. Stage 1 recorded them at configure time; transcribe them here, the
rem same handoff build.sh does.
set "JXL_LIBS="
set "SNDFILE_LIBS="
if exist "%BUILDDIR%\libs\jxl_libs.txt"     set /p JXL_LIBS=<"%BUILDDIR%\libs\jxl_libs.txt"
if exist "%BUILDDIR%\libs\sndfile_libs.txt" set /p SNDFILE_LIBS=<"%BUILDDIR%\libs\sndfile_libs.txt"
if not defined JXL_LIBS echo [build] NOTE: no libjxl in %VCPKG_PREFIX% -- building the sixel graphics tier only.

rem --- 4. Stage 2: EasyRPG Player + the door ---------------------------------
rem The -D flags mirror build.sh's Stage 2b one for one; see that file and
rem COMPILING.md for why each is set. PLAYER_WITH_LHASA=OFF is Windows-only:
rem there is no vcpkg lhasa port, and .lzh-archived games are not something a
rem sysop-installed door package ships.
echo [build] Stage 2: EasyRPG Player + syncrpg.exe ...
"%CMAKE%" -S "%SRCDIR%" -B "%BUILDDIR%\rpg" -G Ninja ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_BUILD_TYPE=%CONFIG% ^
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=%TRIPLET% ^
  -DPLAYER_TARGET_PLATFORM=SDL2 ^
  -DPLAYER_BUILD_LIBLCF=ON ^
  -DPLAYER_WITH_NATIVE_MIDI=OFF ^
  -DPLAYER_WITH_LHASA=OFF ^
  -DINIH_INCLUDE_DIR="%SRCDIR%\easyrpg\lib\inih" ^
  -DINIH_LIBRARY="%BUILDDIR%\libs\inih.lib" ^
  -DTERMGFX_LIB="%BUILDDIR%\libs\termgfx\termgfx.lib" ^
  -DXPDEV_LIB="%BUILDDIR%\libs\xpdev_static.lib" ^
  -DJXL_LIBS="%JXL_LIBS%" ^
  -DSNDFILE_LIBS="%SNDFILE_LIBS%"
if errorlevel 1 goto error
"%CMAKE%" --build "%BUILDDIR%\rpg" --target syncrpg
if errorlevel 1 goto error

rem --- 5. Stage the binary where deploy.js looks -----------------------------
rem exec/load/door_deploy.js (shared by every door) looks under
rem build-msvc\Release\ on Windows; the Ninja build is single-config and drops
rem the exe straight in its binary dir, so copy it into place.
if not exist "%BUILDDIR%\rpg\syncrpg.exe" (
    echo [build] ERROR: expected output not found: %BUILDDIR%\rpg\syncrpg.exe
    goto error
)
if not exist "%BUILDDIR%\%CONFIG%" mkdir "%BUILDDIR%\%CONFIG%"
copy /Y "%BUILDDIR%\rpg\syncrpg.exe" "%BUILDDIR%\%CONFIG%\syncrpg.exe" >nul
echo.
echo [build] Built: %BUILDDIR%\%CONFIG%\syncrpg.exe
echo [build] Run 'jsexec deploy.js' to install it into the SyncRPG package.
endlocal
exit /b 0

:no_x64
echo [build] ERROR: x64 is not supported by this door ^(Win32 only, per the
echo [build] sibling termgfx doors' x86-windows-static-md triplet^). Build: build.bat
endlocal
exit /b 1

:error
echo.
echo [build] FAILED ^(errorlevel %errorlevel%^)
endlocal
exit /b 1
