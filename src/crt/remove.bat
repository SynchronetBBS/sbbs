@echo=off
echo REMOVE.BAT - VIDEO HANDLING FUNCTIONS uninstallation utility
echo Usage: REMOVE (TC/TC++ path)
echo ÿ
if not exist %1\bin\tlib.exe goto error
if not exist %1\include\crt.h goto warning
echo REMOVE will remove VIDEO HANDLING FUNCTIONS from your RTL libraries and
echo the following VIDEO HANDLING FUNCTIONS files:
echo %1\INCLUDE\CRT.H
echo %1\DOC\VIDEOHLP.TXT
echo Continue?
:jp1
choice /c:yn
if errorlevel 2 goto end
if errorlevel 1 goto remove
echo ÿ
echo ERROR: CHOICE.COM not found in path or user hit CTRL-C/CTRL-BREAK
echo ÿ
echo Hit any key to UNINSTALL or CTRL-C/CTRL-BREAK to stop this batch file and exit
pause
:remove
echo Removing CRT functions from the RTL libraries
echo Processing cs.lib
%1\bin\tlib %1\lib\cs.lib @remove.dat
echo Processing cm.lib
%1\bin\tlib %1\lib\cm.lib @remove.dat
echo Processing cc.lib
%1\bin\tlib %1\lib\cc.lib @remove.dat
echo Processing cl.lib
%1\bin\tlib %1\lib\cl.lib @remove.dat
echo Processing ch.lib
%1\bin\tlib %1\lib\ch.lib @remove.dat
echo Removing unecessary backup files created by TLIB
del %1\lib\cc.bak
del %1\lib\cs.bak
del %1\lib\cm.bak
del %1\lib\cl.bak
del %1\lib\ch.bak
echo Deleting header file CRT.H
del %1\include\crt.h
echo Deleting help file VIDEOHLP.TXT
del %1\doc\videohlp.txt
echo ÿ
echo Uninstallation complete. As some files or directories may have been
echo changed since last installation, it would be good to check if
echo uninstallation has been properly executed.
goto end
:error
echo ERROR: %1\BIN\TLIB.EXE not found.
echo This problably has happened because you gave wrong path of TC or because
echo your C/C++ compiler is not Turbo C/C++. (in this case edit this batch file)
echo For more information read README.1ST.
goto end
:warning
echo It seems that VIDEO HANDLING FUNCTIONS has been already uninstalled, as
echo %1\include\CRT.H is missing.
echo Continue anyway?
goto jp1
:end
@echo=on
