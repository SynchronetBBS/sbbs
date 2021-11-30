REM Build SBBSEXEC.DLL (Synchronet Virtual UART/FOSSIL Device Driver)
cl /Di386 /Gz /O2 ntvdm.lib netapi32.lib /DRINGBUF_SEM /DRINGBUF_MUTEX /DNO_SOCKET_SUPPORT /I../xpdev /LD sbbsexec.c ringbuf.c ../xpdev/ini_file.c ../xpdev/dirwrap.c ../xpdev/str_list.c ../xpdev/xpdatetime.c ../xpdev/genwrap.c ../xpdev/semwrap.c ../xpdev/datewrap.c ../xpdev/xpprintf.c ../xpdev/threadwrap.c
