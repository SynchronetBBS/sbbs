# Microsoft Developer Studio Project File - Name="syncterm" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=syncterm - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "syncterm.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "syncterm.mak" CFG="syncterm - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "syncterm - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "syncterm - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "syncterm - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "msvc.win32.release"
# PROP Intermediate_Dir "msvc.win32.release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\include\cryptlib" /I "..\comio" /I "..\xpdev" /I "..\conio" /I "..\uifc" /I "..\sbbs3" /I "..\smblib" /I "..\..\include\sdl" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "WITH_SDL" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib winmm.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "syncterm - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "msvc.win32.debug"
# PROP Intermediate_Dir "msvc.win32.debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\..\include\cryptlib" /I "..\comio" /I "..\xpdev" /I "..\conio" /I "..\uifc" /I "..\sbbs3" /I "..\smblib" /I "..\..\include\sdl" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "WITH_SDL" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib winmm.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "syncterm - Win32 Release"
# Name "syncterm - Win32 Debug"
# Begin Source File

SOURCE=.\bbslist.c
# End Source File
# Begin Source File

SOURCE=..\comio\comio.c
# End Source File
# Begin Source File

SOURCE=..\comio\comio_win32.c
# End Source File
# Begin Source File

SOURCE=.\conn.c
# End Source File
# Begin Source File

SOURCE=.\conn_telnet.c
# End Source File
# Begin Source File

SOURCE=..\smblib\crc16.c
# End Source File
# Begin Source File

SOURCE=..\smblib\crc32.c
# End Source File
# Begin Source File

SOURCE=..\uifc\filepick.c
# End Source File
# Begin Source File

SOURCE=.\fonts.c
# End Source File
# Begin Source File

SOURCE=.\menu.c
# End Source File
# Begin Source File

SOURCE=.\modem.c
# End Source File
# Begin Source File

SOURCE=.\ooii.c
# End Source File
# Begin Source File

SOURCE=.\ooii_bmenus.c
# End Source File
# Begin Source File

SOURCE=.\ooii_cmenus.c
# End Source File
# Begin Source File

SOURCE=.\ooii_logons.c
# End Source File
# Begin Source File

SOURCE=.\ooii_sounds.c
# End Source File
# Begin Source File

SOURCE=.\rlogin.c
# End Source File
# Begin Source File

SOURCE=.\ssh.c
# End Source File
# Begin Source File

SOURCE=.\st_crypt.c
# End Source File
# Begin Source File

SOURCE=.\syncterm.c
# End Source File
# Begin Source File

SOURCE=..\sbbs3\telnet.c
# End Source File
# Begin Source File

SOURCE=.\telnet_io.c
# End Source File
# Begin Source File

SOURCE=.\term.c
# End Source File
# Begin Source File

SOURCE=.\uifcinit.c
# End Source File
# Begin Source File

SOURCE=.\window.c
# End Source File
# Begin Source File

SOURCE=..\sbbs3\xmodem.c
# End Source File
# Begin Source File

SOURCE=..\xpdev\xpbeep.c
# End Source File
# Begin Source File

SOURCE=..\sbbs3\zmodem.c
# End Source File
# End Target
# End Project
