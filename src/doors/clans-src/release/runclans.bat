@echo off
REM change \TheClans to the directory where you are running The Clans from.

REM cd \TheClans
REM ^Change this to The Clans directory and uncomment it.

REM The %1 simply means using "RUNCLANS.BAT 1" will run Node 1,
REM "RUNCLANS.BAT 2" will run Node 2, etc.
REM
REM You can change the %1 to an actual number if you know what node you're
REM running already.  Just remember to keep the Clans /Nx format where x is
REM the node to run.  InterBBS users may wish to add /F to the command line.

if "%1" == "" GOTO LOCAL

Clans /N%1

REM Not all BBSes need this line.  It just changes back to your BBS
REM directory.

REM cd \bbs
REM ^Change this to your BBS dir and uncomment it.

GOTO DONE

:LOCAL
CLANS /L

:DONE
