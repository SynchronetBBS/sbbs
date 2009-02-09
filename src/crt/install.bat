@echo=off
echo INSTALL.BAT - VIDEO HANDLING FUNCTIONS installation utility
echo Usage: INSTALL (TC/TC++ path)
echo ÿ
if not exist %1\bin\tlib.exe goto error
if exist %1\include\crt.h goto error2
echo INSTALL.BAT will copy the following files in the following directories:
echo CRT.H to %1\INCLUDE
echo VIDEOHLP.TXT to %1\DOC
echo and will also install VIDEO HANDLING FUNCTIONS in TC RTL libraries at %1\LIB
echo ÿ
echo If everything is OK, hit "y" to continue.
echo If the paths above are incorrect or if you wish to make a safe backup
echo of your RTL libraries, hit "n" or "ESC" to exit.
echo For more information read README.1ST.
choice /n /c:yn
if errorlevel 2 goto end
if errorlevel 1 goto install
echo ÿ
echo ERROR: CHOICE.COM not found in path or user hit CTRL-C / CTRL-BREAK
echo ÿ
echo Hit any key to INSTALL or CTRL-C / CTRL-BREAK to stop this batch file and exit
pause
:install
echo Installing functions in RTL libraries in
echo small, medium, compact, large and huge memory models
%1\bin\tlib %1\lib\cs.lib + lib\videos.lib
%1\bin\tlib %1\lib\cm.lib + lib\videom.lib
%1\bin\tlib %1\lib\cc.lib + lib\videoc.lib
%1\bin\tlib %1\lib\cl.lib + lib\videol.lib
%1\bin\tlib %1\lib\ch.lib + lib\videoh.lib
echo Removing unecessary backup files created by TLIB
del %1\lib\cc.bak
del %1\lib\cs.bak
del %1\lib\cm.bak
del %1\lib\cl.bak
del %1\lib\ch.bak
echo Copying header file CRT.H 
copy include\crt.h %1\include
echo Copying help file VIDEOHLP.TXT
copy doc\videohlp.txt %1\doc
echo ÿ
echo Installation complete. Read README.1ST file for information.
goto end
:error
echo ERROR: %1\BIN\TLIB.EXE not found.
echo This problably has happened because you gave wrong path of TC or because
echo your C/C++ compiler is not Turbo C/C++. (in this case edit this batch file)
echo For more information read README.1ST.
goto end
:error2
echo INSTALLATION ABORTED: Include file %1\include\CRT.H found.
echo Probably VIDEO HANDLING FUNCTIONS is already installed (this version or
echo another version).
echo If you are trying to install this version over an older version remove
echo first the previous version with REMOVE.BAT, then run INSTALL.BAT again.
echo For more information read README.1ST.
:end
@echo=on
