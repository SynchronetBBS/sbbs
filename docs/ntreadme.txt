Synchronet v3.10 Windows NT/2000/XP Instructions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Note
~~~~
Windows 2000 is actually Windows NT 5 (renamed for marketing reasons), not
to be confused with Windows Millennium Edition (the successor to Windows 98).
Windows XP is actually Windows NT 5.1 (renamed for marketing reasons).

Issues
~~~~~~
1. Your C:\WINNT\SYSTEM32\CONFIG.NT file should NOT include Microsoft's 
   ANSI.SYS if you want DOS I/O Interception to function properly (required 
   for 16-bit XSDK/WWIV programs, DOS shell, viewing zip files, etc.). 

2. Add C:\SBBS\EXEC\SVDMANSI to the end of your C:\WINNT\SYSTEM32\AUTOEXEC.NT
   to use the Synchronet Virtual DOS Machine ANSI driver (originally created
   as a replacement for OS/2's ANSI driver, but it functions as an ANSI.SYS 
   replacement for Windows NT just as well - thanks again Steve!).
   Use of this driver is *optional*.
   
3. Use ;SHELL instead of ;DOS to remotely shell to the Windows NT 32-bit
   command interpreter (cmd.exe), which has more functional I/O interception.
   The backspace key does not work correctly however (watch those typos!).
   The ;DOS command will shell to the 16-bit command interpreter (command.com)
   which has numerous I/O interception issues on NT.

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
As for v3.10, these files are only created with the DEBUG BUILD of
SBBSEXEC.DLL (which you probaby don't have).

/* End of File */
