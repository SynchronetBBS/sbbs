@echo off
REM
REM For creating a single EXE file... pass it the source file name in the
REM form:
REM            BAS [filename]
REM            where [filename] is something like TIMEPORT(.bas) or whatever
REM
REM For BC7 users, QBSERPDQ may need changed to BC7SER instead.
REM
LH bc /E /W /O /T /C:512 %1,,;
LH link %1 qbserpdq coding,,,c:\qb\bcom45.lib;
