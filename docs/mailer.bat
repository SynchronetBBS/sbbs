@echo off
rem ***************************************************************************
rem MAILER.BAT (AKA IMRUN.BAT or FDRUN.BAT) file for use with FrontDoor or
rem InterMail and Synchronet BBS Version 2
rem 
rem If you get OUT OF ENVIRONMENT SPACE messages when using this batch file,
rem increase your DOS environment by adding the following to your CONFIG.SYS:
rem SHELL=C:\COMMAND.COM /E:1024 /P
rem ***************************************************************************

rem ***************************************************************************
rem Set this to IM for InterMail or FD for FrontDoor
rem ************************************************
set fd=FD

rem ***************************************************************************
rem Node Number - needed for NODE utility
rem *************************************
set nodenum=1

rem ***************************************************************************
rem SBBS Directory 
rem **************
set sbbs=c:\sbbs

rem ***************************************************************************
rem SBBS CTRL Dir - needed for NODE utility
rem ***************************************
set sbbsctrl=%sbbs%\ctrl

rem ***************************************************************************
rem SBBS NODE Dir - needed for SBBSECHO (use NODE1 directory)
rem *********************************************************
set sbbsnode=%sbbs%\node1

:top
rem ***************************************************************************
rem Set Node status to WAITING FOR CALL
rem ***********************************
%sbbs%\exec\node status=0 %nodenum% rerun off %nodenum% event off %nodenum%

rem ***************************************************************************
rem Drive letter and directory where Mailer is installed 
rem ****************************************************
c:
cd \%fd%
%fd%

rem ***************************************************************************
rem These error levels must be set in FDSETUP or IMSETUP!
rem *****************************************************
if errorlevel 200 goto fax
if errorlevel 100 goto bbs
if errorlevel  75 goto event
if errorlevel  50 goto inmail
if errorlevel  25 goto outmail
if errorlevel  10 goto userbreak
if errorlevel   1 goto error
goto end

rem ***************************************************************************
rem Mailer received in-bound mail from another system 
rem *************************************************
:inmail
echo Importing in-bound mail!!!
rem ***************************************************************************
rem Set node status to NETWORKING
rem *****************************
%sbbs%\exec\node status=6 %nodenum%
%sbbs%\exec\sbbsecho /les!
goto top

rem ***************************************************************************
rem Semaphore file (xxEXIT.025) was signaled by the BBS - new outbound messages
rem ***************************************************************************
:outmail
echo Exporting out-bound mail!!!
rem ***************************************************************************
rem Set node status to NETWORKING
rem *****************************
%sbbs%\exec\node status=6 %nodenum%
%sbbs%\exec\sbbsecho /lin
rem ***************************************************************************
rem Signal the rescan semaphore so that FD will rescan the netmail folder
rem *********************************************************************
rem > fdrescan.now
goto top

rem ***************************************************************************
rem Received a call from a user wanting access to the BBS - how dare they!
rem This actually executes EXEBBS.BAT with the proper switches for SBBS
rem You will probably also need to edit EXEBBS.BAT for your configuration 
rem *********************************************************************
:bbs
dobbs

rem ***************************************************************************
rem Run any pending events (Remove the 'o' if running SBBS v2.1 or earlier)
rem ***********************************************************************
:event
cd %sbbsnode%
call sbbs o
goto top

rem ***************************************************************************
rem Received a fax call
rem *******************
:fax
cd \zfax
rcvfax 2 /p:1
if errorlevel 0 echo riIncoming FAX Notification! >> \bbs\data\msgs\0001.msg
goto top

rem ***************************************************************************
rem Quit FD
rem *******
:userbreak
echo User Break.
goto end

rem ***************************************************************************
rem Mailer error of some kind
rem *************************
:error
echo %fd% ERROR (1-9)
goto end

rem ***************************************************************************
rem End of the batch file, so display a blank line for prettiness
rem *************************************************************
:end
rem ***************************************************************************
rem Make node status OFFLINE
rem ************************
%sbbs%\exec\node status=5 %nodenum%
echo.
