cd\gac_cs\act_0029\art

REM Get the newest REGISTER.DOC file
copy \gac_cs\problems.doc ..\release
copy \gac_cs\register.doc ..\release
copy \gac_cs\register.doc \gac_cs\internet\register.txt
copy \gac_cs\register.doc \gac_cs\demodisk\register.doc
copy \gac_cs\catalog.doc ..\release
copy \gac_cs\catalog.doc \gac_cs\internet\catalog.txt
copy \gac_cs\catalog.doc \gac_cs\demodisk\catalog.doc


REM Make the .ART archives
del *.art
del *.tmp

call \gac_cs\makeart GAC_
copy *.art ..\release
copy *.art ..

REM Executable
copy ..\gac_fc.exe ..\release
REM 1/97 Update the 32 bit archive
copy \gac_cs\gamesdk\template\release\win95nt.zip ..\release
pause
pkzip -u ..\release\win95nt.zip ..\gac_fc32.exe
pause

REM REM The next line out at final release time
REM goto end

REM Arj it up
cd..\release
del archives\*.zip
del archives\*.arj
del \gac_cs\demodisk\gacfc*.arj
del \gac_cs\internet\gacfc*.zip

pkzip -ex GACfc%1.zip
move GAC*.zip archives
REM Copy Archives to were they should be
cd archives
copy *.zip \gac_cs\demodisk
copy *.zip \gac_cs\internet

REM backup the programs
cd\gac_cs\act_0029
call backups
:end
