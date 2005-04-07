cd\gac_cs\act_0000\art

REM Get the newest REGISTER.DOC file
copy o:\gac_cs\problems.doc ..\release
copy d:\gac_cs\register.doc ..\release
copy d:\gac_cs\register.doc d:\gac_cs\internet\register.txt
copy d:\gac_cs\register.doc d:\gac_cs\demodisk\register.doc
copy d:\gac_cs\catalog.doc ..\release
copy d:\gac_cs\catalog.doc d:\gac_cs\internet\catalog.txt
copy d:\gac_cs\catalog.doc d:\gac_cs\demodisk\catalog.doc


REM Make the .ART archives
copy \gac_cs\gamesdk\reqdart\*.* .
call \gac_cs\makeart GAC_
copy *.art ..\release
copy *.art ..

REM Executable
copy ..\GAC_ID.exe ..\release
REM 1/97 Update the 32 bit archive
copy \gac_cs\gamesdk\template\release\win95nt.zip ..\release
pkzip -u ..\release\win95nt.zip ..\gac_ID32.exe


REM REM The next line out at final release time
REM goto end

REM Arj it up
cd..\release
del archives\*.zip
del archives\*.arj
del \gac_cs\demodisk\GACID*.arj
del \gac_cs\internet\GACID*.zip

arj u -jm archives\GACID%1.arj
pkzip -ex GACID%1.zip
move *.zip archives
REM Copy Archives to were they should be
cd archives
copy *.arj \gac_cs\demodisk
copy *.zip \gac_cs\internet

REM backup the programs
cd\gac_cs\act_0000
call backups 0000
:end
