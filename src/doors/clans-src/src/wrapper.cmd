setlocal
reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "x86" > NUL && set PLAT=.win32.x86.|| set PLAT=.win64.amd64.
IF "%CLANS_DEBUG%"=="" (set TYPE=opt) ELSE (set TYPE=debug)
start "The Clans" %~dpn0%PLAT%%TYPE%.exe %*
endlocal
