cd\gac_cs\act_0013\art

REM Get the newest REGISTER.DOC file
copy \gac_cs\problems.doc ..\release
copy \gac_cs\register.doc ..\release
copy \gac_cs\register.doc \gac_cs\internet\register.txt
copy \gac_cs\catalog.doc ..\release
copy \gac_cs\catalog.doc \gac_cs\internet\catalog.txt
copy \gac_cs\bug_rprt.doc ..\release
copy \gac_cs\bug_rprt.doc \gac_cs\internet\bug_rprt.txt
copy \gac_cs\bug_rprt.doc \gac_cs\internet


REM Make the .ART archives
copy \gac_cs\gamesdk\reqdart\*.* .
call \gac_cs\makeart GAC_
copy *.art ..\release
copy *.art ..

REM Executable
copy ..\gac_bj.exe ..\release
REM 1/97 Update the 32 bit archive
copy \gac_cs\gamesdk\template\release\win95nt.zip ..\release
pkzip -u ..\release\win95nt.zip ..\gac_bj32.exe

REM REM The next line out at final release time
REM goto end

REM Arj it up
cd..\release
del archives\*.*
del \gac_cs\internet\gacbj*.zip
\gac_cs\fd *.*
pkzip -ex GACBJ%1.zip
move GAC*.zip archives
REM Copy Archives to were they should be
cd archives
copy *.zip \gac_cs\internet
copy *.zip \gac_cs\demodisk

REM backup the programs
cd\gac_cs\act_0013
call backups
:end
