@lredir D: linux\fs$NODEDIR >NUL
@lredir E: linux\fs$XTRNDIR >NUL
@lredir F: linux\fs$CTRLDIR >NUL
@lredir G: linux\fs$DATADIR >NUL
@lredir H: linux\fs$EXECDIR >NUL

E:

REM Switch to startup dir, unless its not defined
REM If not defined, go to node dir (external editors use this)
IF "%STARTDIR%"=="" D:
IF NOT "%STARTDIR%"=="" CD %STARTDIR%

REM Optionally call emusetup.bat or put that stuff here for global (in NOEMU)
REM Looks in startup dir, then ctrl dir
IF EXIST EMUSETUP.BAT GOTO EMULOCAL
IF EXIST F:\EMUSETUP.BAT GOTO EMUGLOBAL
IF EXIST E:\DOSUTILS\NUL GOTO NOEMU
IF EXIST H:\DOSUTILS\NUL GOTO NOEMU
ECHO ERROR: No emusetup.bat in E:\%STARTDIR% or F, or DOSUTILS is missing
GOTO EXEC

:EMULOCAL
CALL EMUSETUP.BAT
GOTO EXEC

:EMUGLOBAL
CALL F:\EMUSETUP.BAT
GOTO EXEC

:NOEMU
@set PATH=%PATH%;E:\dosutils;H:\dosutils
REM fossil driver, such as x00, bnu, or dosemu fossil.com
rem IF "$RUNTYPE" == "FOSSIL" @fossil.com >NUL
rem IF "$RUNTYPE" == "FOSSIL" bnu.com /P1 /L0=11520 >NUL
IF "$RUNTYPE" == "FOSSIL" x00.exe eliminate >NUL
REM share.exe for multinode file locking
@share >NUL

GOTO EXEC

:EXEC
$CMDLINE

IF NOT "%1" == "TEST" exitemu


