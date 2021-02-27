@ECHO OFF
REM This batch file will run the New York 2008 game
REM USAGE: EXAMPLE.BAT <node number>

C:

CD\BBS\NY2008

NY2008 -N%1 -PC:\BBS\NODE%1
REM           ^^^^^^^^^^^^^ path to your drop files

CD\BBS
