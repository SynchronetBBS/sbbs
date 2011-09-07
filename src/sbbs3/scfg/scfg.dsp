# Microsoft Developer Studio Project File - Name="scfg" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=scfg - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "scfg.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "scfg.mak" CFG="scfg - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "scfg - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "scfg - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "scfg - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "msvc.win32.exe.release"
# PROP Intermediate_Dir "msvc.win32.release\scfg"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I ".." /I "..\..\xpdev" /I "..\..\smblib" /I "..\..\uifc" /I "..\..\conio" /I "..\..\comio" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "SCFG" /D "SBBS_EXPORTS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "scfg - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "scfg___Win32_Debug"
# PROP BASE Intermediate_Dir "scfg___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "msvc.win32.exe.debug"
# PROP Intermediate_Dir "msvc.win32.debug\scfg"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I ".." /I "..\..\xpdev" /I "..\..\smblib" /I "..\..\uifc" /I "..\..\conio" /I "..\..\comio" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "SCFG" /D "SBBS_EXPORTS" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "scfg - Win32 Release"
# Name "scfg - Win32 Debug"
# Begin Source File

SOURCE=..\dat_rec.c
# End Source File
# Begin Source File

SOURCE=..\date_str.c
# End Source File
# Begin Source File

SOURCE=..\load_cfg.c
# End Source File
# Begin Source File

SOURCE=..\nopen.c
# End Source File
# Begin Source File

SOURCE=.\scfg.c
# End Source File
# Begin Source File

SOURCE=.\scfgchat.c
# End Source File
# Begin Source File

SOURCE=..\scfglib1.c
# End Source File
# Begin Source File

SOURCE=..\scfglib2.c
# End Source File
# Begin Source File

SOURCE=.\scfgmsg.c
# End Source File
# Begin Source File

SOURCE=.\scfgnet.c
# End Source File
# Begin Source File

SOURCE=.\scfgnode.c
# End Source File
# Begin Source File

SOURCE=..\scfgsave.c
# End Source File
# Begin Source File

SOURCE=.\scfgsub.c
# End Source File
# Begin Source File

SOURCE=.\scfgsys.c
# End Source File
# Begin Source File

SOURCE=.\scfgxfr1.c
# End Source File
# Begin Source File

SOURCE=.\scfgxfr2.c
# End Source File
# Begin Source File

SOURCE=.\scfgxtrn.c
# End Source File
# Begin Source File

SOURCE=..\str_util.c
# End Source File
# Begin Source File

SOURCE=..\userdat.c
# End Source File
# End Target
# End Project
