@echo off
set CLEXE="C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\cl.exe"
set LIBTOOL="C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\lib.exe"

echo Building x86 mozjs_stubs.lib (debug)...
%CLEXE% /nologo /c /std:c++17 C:\next-js\3rdp\mozjs.stubs.cpp /FoC:\next-js\3rdp\win32.debug\mozjs\bin\mozjs_stubs.obj
if errorlevel 1 exit /b 1
%LIBTOOL% /nologo /out:C:\next-js\3rdp\win32.debug\mozjs\bin\mozjs_stubs.lib C:\next-js\3rdp\win32.debug\mozjs\bin\mozjs_stubs.obj
if errorlevel 1 exit /b 1

echo Building x86 mozjs_stubs.lib (release)...
%CLEXE% /nologo /c /std:c++17 C:\next-js\3rdp\mozjs.stubs.cpp /FoC:\next-js\3rdp\win32.release\mozjs\bin\mozjs_stubs.obj
if errorlevel 1 exit /b 1
%LIBTOOL% /nologo /out:C:\next-js\3rdp\win32.release\mozjs\bin\mozjs_stubs.lib C:\next-js\3rdp\win32.release\mozjs\bin\mozjs_stubs.obj
if errorlevel 1 exit /b 1

echo Done.
