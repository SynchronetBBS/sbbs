@echo off
REM
REM Creates all the Time Port executables
REM
@ECHO ----------------------
@ECHO Compiling CODING.BAS
@ECHO ----------------------
LH bc /E /W /O /T /C:512 coding,,;
REM No need to link "coding" it just needs to be an OBJ.
@ECHO ----------------------
@ECHO Compiling TIMEPORT.BAS
@ECHO ----------------------
LH bc /E /W /O /T /C:512 TIMEPORT,,;
LH link TIMEPORT qbserpdq coding,,,c:\qb\bcom45.lib;
@ECHO ----------------------
@ECHO Compiling INSTALL.BAS
@ECHO ----------------------
LH bc /E /W /O /T /C:512 INSTALL,,;
LH link INSTALL coding,,,c:\qb\bcom45.lib;
@ECHO ----------------------
@ECHO Compiling BADTIMES.BAS
@ECHO ----------------------
LH bc /E /W /O /T /C:512 BADTIMES,,;
LH link BADTIMES qbserpdq coding,,,c:\qb\bcom45.lib;
@ECHO ----------------------
@ECHO Compiling MAKEMOD.BAS
@ECHO ----------------------
LH bc /E /W /O /T /C:512 MAKEMOD,,;
LH link MAKEMOD coding,,,c:\qb\bcom45.lib;
@ECHO ----------------------
@ECHO Compiling OUTSIDE.BAS
@ECHO ----------------------
LH bc /E /W /O /T /C:512 OUTSIDE,,;
LH link OUTSIDE qbserpdq coding,,,c:\qb\bcom45.lib;
@ECHO ----------------------
@ECHO Compiling TWENTIES.BAS
@ECHO ----------------------
LH bc /E /W /O /T /C:512 TWENTIES,,;
LH link TWENTIES qbserpdq coding,,,c:\qb\bcom45.lib;
@ECHO ----------------------
@ECHO Compiling STONEAGE.BAS
@ECHO ----------------------
LH bc /E /W /O /T /C:512 STONEAGE,,;
LH link STONEAGE qbserpdq coding,,,c:\qb\bcom45.lib;
@ECHO ----------------------
@ECHO Compiling TP-EVENT.BAS
@ECHO ----------------------
LH bc /E /W /O /T /C:512 TP-EVENT,,;
LH link TP-EVENT qbserpdq coding,,,c:\qb\bcom45.lib;
