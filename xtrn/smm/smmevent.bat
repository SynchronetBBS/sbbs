@echo off
rem ***************************************************************************
rem		   MATCH MAKER IMPORT/EXPORT and MAINTENANCE
rem ***************************************************************************
smb2smm %sbbsdata%\subs\syncdata smm.dab >> %sbbsdata%\smb2smm.log
smm2smb smm.dab %sbbsdata%\subs\syncdata
smmutil 90 7 > smmstats.txt
outphoto smm.dab %sbbsdata%\subs\syncdata
rem ***************************************************************************

:end
