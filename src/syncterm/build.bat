@echo off
setlocal
rem Windows MSVC build driver.  Argument 1 is the config (Release or
rem Debug); defaults to Release.
rem
rem Uses CMake with the Visual Studio 2022 generator.  The CMake path
rem already handles DeuceSSH + vendored Botan (from
rem 3rdp/dist/Botan.tar.xz) the same way the MinGW-w64 cross-compile
rem does, so no MSBuild .sln / .vcxproj edits are needed.  Requires
rem CMake + Python 3 + the MSVC C/C++ build tools on PATH.
rem
rem Artifacts land in msvc.build\%CONFIG%\syncterm.exe.

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

rem Bring the MSVC toolset environment in scope for downstream tools
rem that rely on it (Botan's configure.py + Nmake, ExternalProject).
if exist "%VS170COMNTOOLS%\VsMSBuildCmd.bat" (
    call "%VS170COMNTOOLS%\VsMSBuildCmd.bat"
)

if not exist msvc.build mkdir msvc.build
pushd msvc.build

cmake -G "Visual Studio 17 2022" -A Win32 -S .. -B . -DCMAKE_BUILD_TYPE=%CONFIG%
if errorlevel 1 goto fail

cmake --build . --config %CONFIG%
if errorlevel 1 goto fail

popd
exit /b 0

:fail
popd
echo. & echo !ERROR(s) occurred & exit /b 1
