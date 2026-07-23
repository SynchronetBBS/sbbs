@echo off
rem ===========================================================================
rem build.bat - Build every Win32-buildable door under src\doors from one
rem             command.  The *nix counterpart is build.sh.
rem
rem   Usage:  build.bat                    (build every door, keep going on error)
rem           build.bat syncdoom syncduke  (build only the named doors)
rem           build.bat clean              (delete the build trees, then exit)
rem           build.bat clean all          (delete the build trees, then build)
rem           build.bat -l                 (list the targets and exit)
rem           build.bat -q                 (quiet: log to .build-logs\, show failures)
rem
rem This is an umbrella over the per-door build systems -- it does NOT replace
rem them, and it passes no argument a door's own build.bat wouldn't accept.
rem
rem Two kinds of door build on Windows.  The modern C/C++ doors (syncdoom,
rem syncduke, ...) each ship a build.bat that drives MSVC -- CMake + Visual
rem Studio 2022, or the engine's own solution generator for SyncSCUMM -- and
rem vendor their own copy of libtermgfx.  The Clans instead ships an MSVC
rem solution, built here with MSBuild; it is the one legacy door with a Windows
rem project.  Nothing has a build order against anything else.
rem
rem The remaining legacy doors are plain make(1) trees that link ..\odoors and
rem assume a Unix toolchain, so --list reports them as unavailable instead of
rem failing one by one.  Use build.sh for those.
rem
rem Unlike build.sh there is no libODoors prerequisite step: the Win32 OpenDoors
rem import library (src\odoors\ODoorW.lib) is prebuilt and tracked in git, and
rem The Clans links it straight from there.
rem
rem Win32 (x86) is the only target, deliberately.  A Win32 door runs on a Win32
rem and a Win64 Synchronet host alike -- the DOOR32.SYS comm handle is 32-bit-
rem significant -- which is why each door's own build.bat defaults to, and in
rem three cases accepts nothing but, Win32.  "x64" is refused here rather than
rem passed down.  Release-only for the same reason: no per-door build.bat takes
rem a "debug" argument.  (clans.sln does carry a Debug configuration, but one
rem target out of eight is not enough to make "debug" mean anything here.)
rem
rem Building does NOT touch any live install -- the doors that ship a deploy.js
rem still need `jsexec deploy.js` run in their own directory afterwards.  Keeping
rem deploy separate means a sysop can rebuild and test before pushing a new
rem binary to a live BBS.
rem
rem Unlike a per-door build.bat, a failure here is not fatal: every remaining
rem target is still attempted and a pass/fail summary is printed at the end.
rem The exit status is non-zero if any target failed.
rem ===========================================================================
setlocal enabledelayedexpansion

rem --- Source dir = location of this script (no trailing backslash) ----------
set "SRCDIR=%~dp0"
set "SRCDIR=%SRCDIR:~0,-1%"
set "LOGDIR=%SRCDIR%\.build-logs"
set "CONFIG=Release"
set "PLATFORM=Win32"
set "DOCLEAN="
set "DOALL="
set "DOLIST="
set "QUIET="
set "SELECTED="

call :targets

rem --- Parse arguments -------------------------------------------------------
rem "clean" and "all" are argument words rather than flags, matching what the
rem per-door build.bat scripts accept.  Anything else is a target name.
:parseargs
if "%~1"=="" goto argsdone
set "ARG=%~1"
if /I "!ARG!"=="-h"       goto usage
if /I "!ARG!"=="--help"   goto usage
if /I "!ARG!"=="/?"       goto usage
if /I "!ARG!"=="-l"       ( set "DOLIST=1" & goto nextarg )
if /I "!ARG!"=="--list"   ( set "DOLIST=1" & goto nextarg )
if /I "!ARG!"=="-q"       ( set "QUIET=1"  & goto nextarg )
if /I "!ARG!"=="--quiet"  ( set "QUIET=1"  & goto nextarg )
if /I "!ARG!"=="clean"    ( set "DOCLEAN=1" & goto nextarg )
if /I "!ARG!"=="all"      ( set "DOALL=1"  & goto nextarg )
if /I "!ARG!"=="release"  goto nextarg
if /I "!ARG!"=="Win32"    goto nextarg
if /I "!ARG!"=="x64"      goto no_x64
if /I "!ARG!"=="debug"    goto no_debug
if "!ARG:~0,1!"=="-" (
    1>&2 echo build.bat: unknown option '!ARG!'
    1>&2 echo build.bat: try 'build.bat --help'
    exit /b 2
)
call :lookup "!ARG!"
if "!FOUND!"=="nowin32" (
    1>&2 echo build.bat: '!ARG!' has no Windows build -- it is a make^(1^) + libODoors
    1>&2 echo build.bat: tree with no MSVC project. Build it with .\build.sh instead.
    exit /b 2
)
if "!FOUND!"=="" (
    1>&2 echo build.bat: unknown target '!ARG!'
    1>&2 echo build.bat: try 'build.bat --list'
    exit /b 2
)
set "SELECTED=!SELECTED! !ARG!"
:nextarg
shift
goto parseargs
:argsdone

if defined DOLIST goto list

rem The targets to act on: every buildable one, or just the ones named.
if not defined SELECTED set "SELECTED=%TARGETS%"

rem Each door's build.bat locates its own cmake (PATH, else the copy bundled
rem with VS 2022), so checking for cmake here would be misleading -- what they
rem all genuinely need is the VS 2022 toolchain.
set "VSED="
for %%E in (Professional Enterprise Community BuildTools) do (
    if not defined VSED (
        if exist "C:\Program Files\Microsoft Visual Studio\2022\%%E\Common7\Tools\VsDevCmd.bat" set "VSED=%%E"
    )
)
if not defined VSED echo [build] WARNING: Visual Studio 2022 not found -- every door will fail 1>&2

if defined QUIET if not exist "%LOGDIR%" mkdir "%LOGDIR%"

rem --- Clean -----------------------------------------------------------------
rem Done here rather than by forwarding "clean" to each door's build.bat: those
rem scripts all clean and then BUILD, so there is no way to ask them for a clean
rem alone.  Only build-msvc is removed; build\ is the *nix tree, which is
rem build.sh's business even when both exist side by side on a shared checkout.
rem
rem One thing this cannot do: syncscumm's own `build.bat clean` additionally
rem drops the backends\platform\synchronet junction and restores the tracked
rem symlink file underneath it.  Run that door's script directly when you want
rem the source tree back the way git checked it out.
if defined DOCLEAN (
    for %%T in (%SELECTED%) do call :clean_target %%T
    if not defined DOALL (
        echo [build] Clean complete.
        exit /b 0
    )
    echo.
)

rem --- Build -----------------------------------------------------------------
rem Each door's own build.bat already parallelizes internally, so targets are
rem built one at a time rather than oversubscribing the machine.
set /a NRES=0
set /a NPASS=0
set /a NFAIL=0

echo [build] Configuration: %CONFIG% ^(%PLATFORM%^)
echo.

for %%T in (%SELECTED%) do call :run_one %%T

rem --- Summary ---------------------------------------------------------------
echo ==========================================================
echo  Build summary ^(%CONFIG% / %PLATFORM%^)
echo ==========================================================
for /L %%I in (1,1,%NRES%) do (
    for /f "tokens=1-3" %%a in ("!RES_%%I!") do (
        call :pad "%%b"
        echo  %%a   !PADDED! %%cs
    )
)
echo ----------------------------------------------------------
echo  %NPASS% passed, %NFAIL% failed
if defined QUIET echo  Logs: %LOGDIR%
echo.

if %NFAIL% gtr 0 exit /b 1
exit /b 0

rem ===========================================================================
rem Subroutines
rem ===========================================================================

rem --- Target table ----------------------------------------------------------
rem KIND_<name> is how the target is built, DIR_<name> where from (relative to
rem this script):
rem
rem   bat      the door ships build.bat -- Win32 Release, "clean" handled above
rem   msbuild  an MSVC solution named by SLN_<name>, built with MSBuild
rem
rem Order does not matter: every door vendors what it needs.
:targets
set "TARGETS=syncconquer syncdoom syncduke syncmoo1 syncretro syncrpg syncscumm clans"
for %%T in (%TARGETS%) do (
    set "KIND_%%T=bat"
    set "DIR_%%T=%%T"
)

rem The Clans is the exception: no build.bat, but src\clans.sln builds the game
rem and its fifteen devkit tools.  Everything lands in clans-src\bin, named
rem <tool>.Win32.Release.exe -- there is no deploy.js for this one.
set "KIND_clans=msbuild"
set "DIR_clans=clans-src\src"
set "SLN_clans=clans.sln"

rem Doors with no Windows build at all: make(1) trees that link ..\odoors and
rem assume a Unix toolchain (uname-suffixed outputs, ..\build\Common.gmake, gcc
rem flags).  vbbs and ny2008 do carry a Makefile.win32 / Makefile.bor, but both
rem name a compiler and directories this tree has not had in twenty years.
set "NOWIN32=dgnlance freevote gac-sdk gac_bj gac_fc gac_wh ny2008 oxgen sde smurfcombat timeport top u32rr vbbs"
exit /b 0

rem --- Look a name up in either table -----------------------------------------
rem Sets FOUND to buildable, nowin32, or empty for an unknown name.
:lookup
set "FOUND="
for %%T in (%TARGETS%) do if /I "%%T"=="%~1" set "FOUND=buildable"
for %%T in (%NOWIN32%) do if /I "%%T"=="%~1" set "FOUND=nowin32"
exit /b 0

rem --- Pad a name out to 12 columns for the tables (or %2 columns) -----------
:pad
set "P=%~1            "
if "%~2"=="" ( set "PADDED=!P:~0,12!" ) else ( set "PADDED=!P:~0,%~2!" )
exit /b 0

rem --- --list ----------------------------------------------------------------
:list
echo Buildable targets:
for %%T in (%TARGETS%) do (
    call :pad "%%T"
    set "COL1=!PADDED!"
    call :pad "!KIND_%%T!" 8
    echo   !COL1! !PADDED! !DIR_%%T!
)
echo.
echo No Windows build ^(make^(1^) + libODoors trees -- use .\build.sh^):
for %%T in (%NOWIN32%) do echo   %%T
exit /b 0

rem --- Clean one target ------------------------------------------------------
:clean_target
set "CT=%~1"
if /I "!KIND_%CT%!"=="msbuild" (
    call :find_msbuild
    if defined MSBUILD (
        echo [build] !CT!: MSBuild /t:Clean in !DIR_%CT%!
        "!MSBUILD!" "%SRCDIR%\!DIR_%CT%!\!SLN_%CT%!" /nologo /v:q /t:Clean /p:Configuration=%CONFIG% /p:Platform=x86 >nul 2>nul
    ) else (
        echo [build] !CT!: MSBuild not found, leaving !DIR_%CT%! alone
    )
    exit /b 0
)
set "CD_=%SRCDIR%\!DIR_%CT%!\build-msvc"
if exist "!CD_!" (
    echo [build] !CT!: removing !CD_!
    rmdir /S /Q "!CD_!"
) else (
    echo [build] !CT!: nothing to clean
)
exit /b 0

rem --- Locate MSBuild (same probe order the per-door scripts use) ------------
:find_msbuild
set "MSBUILD="
for %%E in (Professional Enterprise Community BuildTools) do (
    if not defined MSBUILD (
        set "_M=C:\Program Files\Microsoft Visual Studio\2022\%%E\MSBuild\Current\Bin\MSBuild.exe"
        if exist "!_M!" set "MSBUILD=!_M!"
    )
)
exit /b 0

rem --- Build one target ------------------------------------------------------
rem Returns the door's own exit status, or 2 when there was nothing to run.
:build_target
set "BT=%~1"
set "BD=%SRCDIR%\!DIR_%BT%!"
if not exist "!BD!" (
    1>&2 echo [build] !BT!: SKIP -- !DIR_%BT%! does not exist
    exit /b 2
)
if /I "!KIND_%BT%!"=="bat" (
    if not exist "!BD!\build.bat" (
        1>&2 echo [build] !BT!: SKIP -- !DIR_%BT%!\build.bat does not exist
        exit /b 2
    )
    pushd "!BD!"
    call "!BD!\build.bat"
    set "RC=!errorlevel!"
    popd
    exit /b !RC!
)
if /I "!KIND_%BT%!"=="msbuild" (
    call :find_msbuild
    if not defined MSBUILD (
        1>&2 echo [build] !BT!: SKIP -- MSBuild.exe from VS 2022 not found
        exit /b 2
    )
    pushd "!BD!"
    "!MSBUILD!" "!BD!\!SLN_%BT%!" /nologo /m /v:m /p:Configuration=%CONFIG% /p:Platform=x86
    set "RC=!errorlevel!"
    popd
    exit /b !RC!
)
1>&2 echo [build] !BT!: unknown kind '!KIND_%BT%!'
exit /b 2

rem --- Build one target, timed, and record the result ------------------------
:run_one
set "T=%~1"
echo ==^> !T! ^(!DIR_%T%!^)
call :now T0
if defined QUIET (
    call :build_target "!T!" >"%LOGDIR%\!T!.log" 2>&1
) else (
    call :build_target "!T!"
)
set "RC=!errorlevel!"
call :now T1
set /a SECS=T1-T0
if !SECS! lss 0 set /a SECS+=86400
set /a NRES+=1
if "!RC!"=="0" (
    set /a NPASS+=1
    set "RES_!NRES!=PASS !T! !SECS!"
    echo ^<== !T! OK ^(!SECS!s^)
) else (
    set /a NFAIL+=1
    set "RES_!NRES!=FAIL !T! !SECS!"
    1>&2 echo ^<== !T! FAILED ^(rc=!RC!^)
    if defined QUIET (
        1>&2 echo --- tail of %LOGDIR%\!T!.log ---
        call :tail "%LOGDIR%\!T!.log" 15 1>&2
        1>&2 echo --------------------------------
    )
)
echo.
exit /b 0

rem --- Seconds since midnight (cmd has no seconds-since-epoch) ---------------
rem The hour is space-padded before noon, hence the :TIME: =0: substitution, and
rem every field is read as 1<field> so an 08 or 09 is not taken for octal.
:now
for /f "tokens=1-3 delims=:.," %%a in ("!TIME: =0!") do (
    set /a "%~1=((1%%a-100)*3600)+((1%%b-100)*60)+(1%%c-100)"
)
exit /b 0

rem --- Last N lines of a file ("more +N" skips N lines) ----------------------
:tail
for /f %%N in ('find /c /v "" ^< "%~1"') do set /a "_SKIP=%%N-%~2"
if !_SKIP! lss 0 set "_SKIP=0"
more +!_SKIP! "%~1"
exit /b 0

rem --- Refused arguments and --help ------------------------------------------
:no_x64
1>&2 echo build.bat: x64 is not a supported target for the doors ^(Win32 only: a
1>&2 echo build.bat: Win32 door runs on a Win64 Synchronet host too^). Three of the
1>&2 echo build.bat: modern doors refuse x64 outright; build the two that accept it
1>&2 echo build.bat: with their own script, e.g. syncdoom\build.bat x64.
exit /b 2

:no_debug
1>&2 echo build.bat: no Debug build -- none of the per-door build.bat scripts take
1>&2 echo build.bat: a "debug" argument, they are MSVC Release only.
exit /b 2

:usage
echo build.bat - Build every Win32-buildable door under src\doors from one
echo             command.  The *nix counterpart is build.sh.
echo.
echo   Usage:  build.bat                    ^(build every door, keep going on error^)
echo           build.bat syncdoom syncduke  ^(build only the named doors^)
echo           build.bat clean              ^(delete the build trees, then exit^)
echo           build.bat clean all          ^(delete the build trees, then build^)
echo           build.bat -l                 ^(list the targets and exit^)
echo           build.bat -q                 ^(quiet: log to .build-logs\, show failures^)
exit /b 0
