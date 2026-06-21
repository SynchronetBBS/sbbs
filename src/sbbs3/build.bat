@echo off
setlocal
rem *** Requires Microsoft Visual C++ 2022 ***
call "%VS170COMNTOOLS%\VsMSBuildCmd.bat"
rem PreferredToolArchitecture=x64: use the 64-bit-hosted, x86-targeting CL.exe
rem (HostX64\x86). Emits identical Win32/x86 binaries but the compiler runs as a
rem 64-bit process, removing the 32-bit host's ~2GB address-space ceiling that
rem caused sporadic CL.exe crashes (0xC0000005 / "internal compiler error") on the
rem parallel GitLab build.
msbuild sbbs3.sln /p:Platform="Win32" /p:PreferredToolArchitecture=x64 /p:XPDeprecationWarning=false %*
if errorlevel 1 echo. & echo !ERROR(s) occurred & exit /b 1