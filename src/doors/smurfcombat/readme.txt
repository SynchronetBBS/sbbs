 ----------------------------------------------------------------------------

 SYNCHRONET UPDATE: If you have been using CHAIN.TXT, change your Drop File
 to DOOR.SYS.  This error which originates in SYCNHRONET causes the program
 to think you're calling in remotely,  and thus drops the sysop when he/she
 tries to logon.

 ----------------------------------------------------------------------------

 NOTICE! Smurf Combat requires a fossil driver to operate, as noted by the
 correct bath file example shown below. Included in this version or
 upgrade of Smurf Combat is the fossil driver BNUxxxxxx.ZIP This must
 either be installed as a device driver or run in the batch file as
 shown below or else your system may crash or simply not work.

 ----------------------------------------------------------------------------

 >> Synchronet SHOULD NOT use the following BATCH file example

 SAMPLE.BAT      <--- Sample batch file to run Smurf Combat via WWiV
 cd\bbs\smurf         [Goto Smurf Combat Dir]
 copy \bbs\chain.txt  [Copy User Information File]

 bnu                  [Fossil Driver Required]
	 ***OR***     [Fossil Driver Required]
 x00                  [Fossil Driver Required]

 smurf                [Execute Program]
 cd\bbs               [Return to System]
 ^Z                   [Control + Z, Release, Enter]

 >> This batch file is created in the 'BBS Main Directory' specified by
    running 'C:\> SMURF SETUP' and is saved as SMURF.BAT

 ----------------------------------------------------------------------------

>> Synchronet Setup

Name			Smurf Combat 1.5
Startup Dir		C:\BBS\SMURF          (your choice - Dir w/ SMURF.EXE)
Command Line (Normal)   SMURF                 (Run Executive ONLY)

  NOTE: Insure that you have a fossil drive already installed, or to
	install one, simply edit you autoexec.bat to run 'BNU'. A copy
	of 'BNU' is included and should be in this TAZ directory.
  SYNCHRONET: Synchronet functions best with X00.SYS installed as a
	device in your CONFIG.SYS with LOCKED BAUD RATES.

Clean-Up                [None]
ANSI Req		No
Multiuser		No
I/O			No
Shrink			Yes
Modify			No
WWiV Color Codes	No
BBS Drop File		GAP -  DOOR.SYS		(Synchronet MUST use DOOR.SYS)
Place Drop File		Start-up Directory

 ----------------------------------------------------------------------------

 Registration is NOTHING and can be sent to:

 Laurence Maar			(909)861-1616 or (818)792-9988
 1415 Valeview Drive
 Diamond Bar, CA 91765 USA (Etats-Unis)


 Please PRINT the file ORDER.FRM or SMURF.ORD by typing:

 C:\BBS\SMURF>  copy order.frm prn

 from the DOS prompt...


 -- Laurence Maar

 ----------------------------------------------------------------------------








 NOTE: If the above file just scrolled by your screen, type:

 C:\BBS\SMURF\>  type README.DOC | more

 to pause in between screen-fulls...










