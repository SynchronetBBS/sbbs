@ECHO OFF
REM Do not modify the following area:
REM
REM This file should be used to run Tournament Blackjack. The reason is
REM that the auto update will not work if just the GAC_FC.EXE is run.
REM Check for .EXE file...
if exist IN\GAC_FC32.EXE goto copy
REM
REM Run the game with any supplied parameters.
:run
GAC_FC32.exe %1 %2 %3 %4 %5 %6 %7 %8 %9
goto end
REM
REM This section will copy the new .EXE to the directory (if needed)
:copy
attrib -r GAC_FC32.EXE
copy IN\GAC_FC32.EXE .
del in\GAC_FC32.EXE
attrib +r GAC_FC32.EXE
goto run
REM
REM Exit
:end
