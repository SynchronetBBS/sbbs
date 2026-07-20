@echo off
rem ===========================================================================
rem build.bat -- Win32 / MSVC (Release) build of the SyncSCUMM door (syncscumm.exe:
rem              the vendored ScummVM engine collection with our Synchronet
rem              OSystem backend). The *nix counterpart is build.sh; unlike it,
rem              Windows has no gcc/configure path, so this drives ScummVM's own
rem              MSVC project generator (devtools/create_project) instead:
rem
rem   1. Junction backends/platform/synchronet -> door (git stores it as a POSIX
rem      symlink that Windows checks out as a plain text file; the build needs a
rem      real directory there so create_project and MSBuild see the door sources).
rem   2. Build create_project.exe (patched for our --synchronet backend).
rem   3. Build the door's own C libraries -- termgfx + xpdev -- as MSVC static
rem      libs via door/CMakeLists.txt (the same libs build.sh stages first).
rem   4. Generate scummvm.sln with the curated engines and OUR backend (no SDL),
rem      the SDL/network/OpenGL features trimmed to a headless door.
rem   5. Wire up vcpkg locally (media libraries) and MSBuild the solution.
rem
rem   Usage:  build.bat            (Win32 Release)
rem           build.bat clean      (delete the build tree + junction, then build)
rem
rem The binary is left at build-msvc\Release\syncscumm.exe (NOT installed). Run
rem `jsexec deploy.js` afterwards to copy it into every installed SyncSCUMM
rem package (deploy.js auto-discovers them). Win32 (x86) only -- matches the
rem sibling termgfx doors' triplet.
rem ===========================================================================
setlocal enabledelayedexpansion

set "SRCDIR=%~dp0"
set "SRCDIR=%SRCDIR:~0,-1%"
set "SVM=%SRCDIR%\scummvm"
set "DOOR=%SRCDIR%\door"
set "BUILDDIR=%SRCDIR%\build-msvc"
set "CONFIG=Release"
set "PLATFORM=Win32"
set "TRIPLET=x86-windows-static-md"
if not defined VCPKG_ROOT set "VCPKG_ROOT=C:\vcpkg"

rem --- Parse args ------------------------------------------------------------
set "DOCLEAN="
:parseargs
if "%~1"=="" goto argsdone
if /I "%~1"=="clean" set "DOCLEAN=1"
if /I "%~1"=="x64"   goto no_x64
shift
goto parseargs
:argsdone

rem --- Locate MSBuild --------------------------------------------------------
set "MSBUILD="
for %%E in (Professional Enterprise Community BuildTools) do (
    if not defined MSBUILD (
        set "_M=C:\Program Files\Microsoft Visual Studio\2022\%%E\MSBuild\Current\Bin\MSBuild.exe"
        if exist "!_M!" set "MSBUILD=!_M!"
    )
)
if not defined MSBUILD (
    echo [build] ERROR: MSBuild.exe from VS 2022 not found.
    goto error
)

rem --- Locate cmake (PATH, else the VS 2022 bundled copy) --------------------
set "CMAKE=cmake"
where cmake >nul 2>nul
if errorlevel 1 (
    for %%E in (Professional Enterprise Community BuildTools) do (
        set "_C=C:\Program Files\Microsoft Visual Studio\2022\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if exist "!_C!" set "CMAKE=!_C!"
    )
)

rem --- Clean -----------------------------------------------------------------
if defined DOCLEAN (
    echo [build] Removing build tree %BUILDDIR%
    call :drop_junction
    if exist "%BUILDDIR%" rmdir /S /Q "%BUILDDIR%"
)

rem --- 1. Junction backends\platform\synchronet -> door ----------------------
set "BACKLINK=%SVM%\backends\platform\synchronet"
if not exist "%BACKLINK%\module.mk" (
    if exist "%BACKLINK%" del /Q "%BACKLINK%" 2>nul
    echo [build] Creating junction to door backend
    mklink /J "%BACKLINK%" "%DOOR%" >nul
    if errorlevel 1 goto error
)
rem Keep git from reporting the junction (which replaces the tracked symlink).
git -C "%SRCDIR%" update-index --skip-worktree src/doors/syncscumm/scummvm/backends/platform/synchronet >nul 2>nul

rem --- 2. Build create_project.exe (Debug -- host tool; Release LTCG ICEs) ----
set "CP=%SVM%\devtools\create_project\msvc\Debug\create_project.exe"
if not exist "%CP%" (
    echo [build] Building create_project.exe ...
    "%MSBUILD%" "%SVM%\devtools\create_project\msvc\create_project.sln" /p:Configuration=Debug /p:Platform=Win32 /v:minimal /nologo
    if errorlevel 1 goto error
)

rem --- 3. Build termgfx + xpdev static libs (door/CMakeLists.txt) -------------
rem Point CMake at the classic vcpkg prefix so termgfx finds libjxl (JPEG-XL
rem graphics tier -> WITH_JXL) and libsndfile (Opus/Ogg audio -> the streaming
rem music path). Both are also listed in msvc\vcpkg.json, so the same libraries
rem (and their transitive deps) auto-link into syncscumm.exe. If the prefix
rem lacks them the door still builds -- degraded to the sixel + raw-PCM tiers.
rem   vcpkg install libjxl libsndfile:%TRIPLET%
set "VCPKG_PREFIX=%VCPKG_ROOT%\installed\%TRIPLET%"
where vcpkg >nul 2>nul && vcpkg install libjxl libsndfile:%TRIPLET% >nul 2>nul
if not exist "%VCPKG_PREFIX%\lib\jxl.lib" (
    echo [build] NOTE: %VCPKG_PREFIX% has no libjxl/libsndfile; building sixel/raw-PCM only.
    echo [build]       Run: vcpkg install libjxl libsndfile:%TRIPLET%  for the JXL/audio tiers.
)
echo [build] Building termgfx + xpdev static libraries ...
"%CMAKE%" -S "%DOOR%" -B "%BUILDDIR%\libs" -A %PLATFORM% -DCMAKE_PREFIX_PATH="%VCPKG_PREFIX%"
if errorlevel 1 goto error
"%CMAKE%" --build "%BUILDDIR%\libs" --config %CONFIG% --target termgfx xpdev_static
if errorlevel 1 goto error
rem Collect the three archives into one dir for the linker search path.
if not exist "%BUILDDIR%\lib" mkdir "%BUILDDIR%\lib"
copy /Y "%BUILDDIR%\libs\termgfx\%CONFIG%\termgfx.lib"     "%BUILDDIR%\lib\" >nul
copy /Y "%BUILDDIR%\libs\%CONFIG%\xpdev_static.lib"        "%BUILDDIR%\lib\" >nul
copy /Y "%BUILDDIR%\libs\%CONFIG%\ADLMIDI.lib"             "%BUILDDIR%\lib\" >nul

rem --- 4. Generate the ScummVM solution --------------------------------------
rem Engines: the curated five. Features: keep the media libs the engines need
rem (zlib/mad/ogg/vorbis/flac/png/jpeg/freetype); disable everything SDL-bound
rem (opengl/imgui/sdlnet), network/cloud, MIDI softfonts, i18n, and the video
rem codecs no curated title uses -- the headless-door subset, mirroring build.sh.
if not exist "%BUILDDIR%" mkdir "%BUILDDIR%"
pushd "%BUILDDIR%"
rem Source path is RELATIVE (..\scummvm from build-msvc): create_project bakes
rem it into every source reference, and its ObjectFileName is
rem $(IntDir)dists\msvc\%%(RelativeDir) -- an ABSOLUTE source path there embeds a
rem drive letter mid-path and MSBuild rejects it (MSB3191). Relative keeps the
rem per-object intermediate dirs valid.
echo [build] Generating MSVC project files (create_project --synchronet) ...
"%CP%" "..\scummvm" --msvc --synchronet --vcpkg ^
  --disable-all-engines ^
  --enable-engine=scumm --enable-engine=sky --enable-engine=queen ^
  --enable-engine=lure --enable-engine=drascula ^
  --enable-engine=agi --enable-engine=sci ^
  --disable-detection-full ^
  --disable-opengl --disable-opengl_game_classic --disable-opengl_game_shaders ^
  --disable-tinygl --disable-3d ^
  --disable-sdlnet --disable-libcurl --disable-http --disable-basic-net ^
  --disable-cloud --disable-enet ^
  --disable-fluidsynth --disable-fribidi --disable-mpeg2 --disable-theoradec ^
  --disable-bink --disable-nasm --disable-taskbar --disable-tts ^
  --disable-translation --disable-langdetect --disable-printing --disable-dialogs ^
  --include-dir "%SRCDIR%\..\termgfx" --include-dir "%SRCDIR%\..\..\xpdev" ^
  --library-dir "%BUILDDIR%\lib"
set "GENRC=%errorlevel%"
popd
if not "%GENRC%"=="0" goto error

rem --- 5. Drop the local vcpkg integration next to the solution --------------
copy /Y "%SRCDIR%\msvc\vcpkg.json"              "%BUILDDIR%\vcpkg.json" >nul
copy /Y "%SRCDIR%\msvc\Directory.Build.props"   "%BUILDDIR%\Directory.Build.props" >nul
copy /Y "%SRCDIR%\msvc\Directory.Build.targets" "%BUILDDIR%\Directory.Build.targets" >nul

rem --- 6. Build --------------------------------------------------------------
rem WholeProgramOptimization=false: ScummVM's Release enables LTCG, which ICEs
rem this MSVC toolset (seen building create_project); we don't need it.
echo [build] Building syncscumm.exe (%CONFIG% ^| %PLATFORM%) ...
"%MSBUILD%" "%BUILDDIR%\scummvm.sln" ^
  /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% ^
  /p:WholeProgramOptimization=false /p:LinkTimeCodeGeneration=false ^
  /m /v:minimal /nologo
if errorlevel 1 goto error

rem --- 7. Confirm the output and stage it where deploy.js looks --------------
rem create_project's OutDir is $(Configuration)$(Arch) -- "Releasex86" for
rem Win32 -- but exec/load/door_deploy.js (shared by every door) looks under
rem build-msvc\Release\. Copy the exe there so `jsexec deploy.js` finds it.
set "OUTEXE=%BUILDDIR%\%CONFIG%x86\syncscumm.exe"
if not exist "%OUTEXE%" (
    echo [build] ERROR: expected output not found: %OUTEXE%
    goto error
)
if not exist "%BUILDDIR%\%CONFIG%" mkdir "%BUILDDIR%\%CONFIG%"
copy /Y "%OUTEXE%" "%BUILDDIR%\%CONFIG%\syncscumm.exe" >nul
echo.
echo [build] Built: %BUILDDIR%\%CONFIG%\syncscumm.exe
echo [build] Run 'jsexec deploy.js' to install it into every SyncSCUMM package.
endlocal
exit /b 0

:drop_junction
rem Remove the junction (rmdir, not del) and restore the tracked symlink file.
set "_BL=%SVM%\backends\platform\synchronet"
if exist "%_BL%\module.mk" (
    git -C "%SRCDIR%" update-index --no-skip-worktree src/doors/syncscumm/scummvm/backends/platform/synchronet >nul 2>nul
    rmdir "%_BL%" 2>nul
    git -C "%SRCDIR%" checkout -- src/doors/syncscumm/scummvm/backends/platform/synchronet >nul 2>nul
)
exit /b 0

:no_x64
echo [build] ERROR: x64 is not supported by this door (Win32 only, per the
echo [build] sibling termgfx doors' x86-windows-static-md triplet). Build: build.bat
endlocal
exit /b 1

:error
echo.
echo [build] FAILED (errorlevel %errorlevel%)
endlocal
exit /b 1
