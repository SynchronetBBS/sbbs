Synchronet v3.0 Windows NT/2000 Instructions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Rob Swindell
www.synchro.net

Note
~~~~
Windows 2000 is actually Windows NT 5 (renamed for marketing reasons), not
to be confused with Windows Millennium Edition (the successor to Windows 98).

Issues
~~~~~~
1. Your C:\WINNT\SYSTEM32\CONFIG.NT file should NOT include Microsoft's 
   ANSI.SYS if you want DOS I/O Interception to function properly (required 
   for 16-bit XSDK/WWIV programs, DOS shell, viewing zip files, etc.). 

2. Add C:\SBBS\EXEC\SVDMANSI to the end of your C:\WINNT\SYSTEM32\AUTOEXEC.NT
   to use the Synchronet Virtual DOS Machine ANSI driver (originally created
   as a replacement for OS/2's ANSI driver, but it functions as an ANSI.SYS 
   replacement for Windows NT/2000 just as well - thanks again Steve!).
   
3. At this time, only the output of 16-bit DOS programs can be intercepted, 
   so the output of internal NT commands and executables (dir, copy, rename, 
   more, etc) won't be intercepted for the remote sysop. This is a very 
   unfortunate problem (that does not exist on Windows 95/98) and I'm still 
   trying to find a solution. In the mean-time, the remote DOS shell is of 
   only limited usefulness. Perhaps a third-party command shell (e.g. 4DOS) 
   or console/ANSI driver will solve this problem (this is potentially related 
   to issue #1). Any feedback on this issue is appreciated.

4. If you need more conventional memory for your external DOS programs and 
   do not need DPMI support, remove (or REM-out) the DOSX line of your 
   C:\WINNT\SYSTEM32\AUTOEXEC.NT. This will give you about 40K additional 
   conventional memory for your external DOS programs.


Debugging
~~~~~~~~~
If you have any problems with programs that worked under Windows 95/98,
but aren't under NT/2000: find the sbbsexec#.log (where # is the node 
number) that was generated after the program in question was run and 
send it to me for debugging purposes. The sbbsexec#.log files are created
in the current directory at the time the program is run. This will
usually be either the Synchronet EXEC directory or the startup directory
for an online external program (e.g. C:\SBBS\XTRN\DOORNAME).

/* End of File */
