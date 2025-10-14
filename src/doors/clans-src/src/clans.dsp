# Microsoft Developer Studio Project File - Name="clans" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=clans - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "clans.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "clans.mak" CFG="clans - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "clans - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "clans - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "clans - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "clans___Win32_Release"
# PROP BASE Intermediate_Dir "clans___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Win32_Release"
# PROP Intermediate_Dir "Win32_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /Zp1 /MD /W3 /Ox /Ot /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_MT" /YX /FD /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ODoorW.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "clans - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Win32_Debug"
# PROP Intermediate_Dir "Win32_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /Zp1 /MDd /W3 /Gm /GX- /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ODoorW.lib /nologo /subsystem:windows /profile /map /debug /debugtype:both /machine:I386

!ENDIF 

# Begin Target

# Name "clans - Win32 Release"
# Name "clans - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\ALLIANCE.C
# End Source File
# Begin Source File

SOURCE=.\CLANS.C
# End Source File
# Begin Source File

SOURCE=.\CLANSINI.C
# End Source File
# Begin Source File

SOURCE=.\CLASS.C
# End Source File
# Begin Source File

SOURCE=.\cmdline.c
# End Source File
# Begin Source File

SOURCE=.\CRC.C
# End Source File
# Begin Source File

SOURCE=.\DOOR.C
# End Source File
# Begin Source File

SOURCE=.\EMPIRE.C
# End Source File
# Begin Source File

SOURCE=.\EVENT.C
# End Source File
# Begin Source File

SOURCE=.\FIGHT.C
# End Source File
# Begin Source File

SOURCE=.\GAME.C
# End Source File
# Begin Source File

SOURCE=.\HELP.C
# End Source File
# Begin Source File

SOURCE=.\IBBS.C
# End Source File
# Begin Source File

SOURCE=.\INPUT.C
# End Source File
# Begin Source File

SOURCE=.\ITEMS.C
# End Source File
# Begin Source File

SOURCE=.\LANGUAGE.C
# End Source File
# Begin Source File

SOURCE=.\MAIL.C
# End Source File
# Begin Source File

SOURCE=.\MAINT.C
# End Source File
# Begin Source File

SOURCE=.\MENUS.C
# End Source File
# Begin Source File

SOURCE=.\MENUS2.C
# End Source File
# Begin Source File

SOURCE=.\MISC.C
# End Source File
# Begin Source File

SOURCE=.\MYIBBS.C
# End Source File
# Begin Source File

SOURCE=.\MYOPEN.C
# End Source File
# Begin Source File

SOURCE=.\NEWS.C
# End Source File
# Begin Source File

SOURCE=.\NPC.C
# End Source File
# Begin Source File

SOURCE=.\PARSING.C
# End Source File
# Begin Source File

SOURCE=.\PAWN.C
# End Source File
# Begin Source File

SOURCE=.\QUESTS.C
# End Source File
# Begin Source File

SOURCE=.\REG.C
# End Source File
# Begin Source File

SOURCE=.\SCORES.C
# End Source File
# Begin Source File

SOURCE=.\SPELLS.C
# End Source File
# Begin Source File

SOURCE=.\system.c
# End Source File
# Begin Source File

SOURCE=.\TRADES.C
# End Source File
# Begin Source File

SOURCE=.\TSLICER.C
# End Source File
# Begin Source File

SOURCE=.\USER.C
# End Source File
# Begin Source File

SOURCE=.\VIDEO.C
# End Source File
# Begin Source File

SOURCE=.\VILLAGE.C
# End Source File
# Begin Source File

SOURCE=.\VOTING.C
# End Source File
# Begin Source File

SOURCE=.\WB_FAPND.C
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\ALLIANCE.H
# End Source File
# Begin Source File

SOURCE=.\CLANS.H
# End Source File
# Begin Source File

SOURCE=.\CLANSF.H
# End Source File
# Begin Source File

SOURCE=.\CLANSINI.H
# End Source File
# Begin Source File

SOURCE=.\CLASS.H
# End Source File
# Begin Source File

SOURCE=.\cmdline.h
# End Source File
# Begin Source File

SOURCE=.\CRC.H
# End Source File
# Begin Source File

SOURCE=.\DEFINES.H
# End Source File
# Begin Source File

SOURCE=.\DOOR.H
# End Source File
# Begin Source File

SOURCE=.\EMPIRE.H
# End Source File
# Begin Source File

SOURCE=.\FIGHT.H
# End Source File
# Begin Source File

SOURCE=.\GAME.H
# End Source File
# Begin Source File

SOURCE=.\HELP.H
# End Source File
# Begin Source File

SOURCE=.\IBBS.H
# End Source File
# Begin Source File

SOURCE=.\INIT.H
# End Source File
# Begin Source File

SOURCE=.\INPUT.H
# End Source File
# Begin Source File

SOURCE=.\INTERBBS.H
# End Source File
# Begin Source File

SOURCE=.\ITEMS.H
# End Source File
# Begin Source File

SOURCE=.\K_CLANSI.H
# End Source File
# Begin Source File

SOURCE=.\K_COMMAN.H
# End Source File
# Begin Source File

SOURCE=.\K_CONFIG.H
# End Source File
# Begin Source File

SOURCE=.\K_IBBS.H
# End Source File
# Begin Source File

SOURCE=.\K_QUESTS.H
# End Source File
# Begin Source File

SOURCE=.\LANGUAGE.H
# End Source File
# Begin Source File

SOURCE=.\MAIL.H
# End Source File
# Begin Source File

SOURCE=.\MAINT.H
# End Source File
# Begin Source File

SOURCE=.\MENUS.H
# End Source File
# Begin Source File

SOURCE=.\MENUS2.H
# End Source File
# Begin Source File

SOURCE=.\MISC.H
# End Source File
# Begin Source File

SOURCE=.\MSTRINGS.H
# End Source File
# Begin Source File

SOURCE=.\MYIBBS.H
# End Source File
# Begin Source File

SOURCE=.\MYOPEN.H
# End Source File
# Begin Source File

SOURCE=.\NEWS.H
# End Source File
# Begin Source File

SOURCE=.\NPC.H
# End Source File
# Begin Source File

SOURCE=.\OPENDOOR.H
# End Source File
# Begin Source File

SOURCE=.\PACKET.H
# End Source File
# Begin Source File

SOURCE=.\PARSING.H
# End Source File
# Begin Source File

SOURCE=.\PAWN.H
# End Source File
# Begin Source File

SOURCE=.\QUESTS.H
# End Source File
# Begin Source File

SOURCE=.\REG.H
# End Source File
# Begin Source File

SOURCE=.\SCORES.H
# End Source File
# Begin Source File

SOURCE=.\SNIPFILE.H
# End Source File
# Begin Source File

SOURCE=.\SPELLS.H
# End Source File
# Begin Source File

SOURCE=.\STRUCTS.H
# End Source File
# Begin Source File

SOURCE=.\SYSTEM.H
# End Source File
# Begin Source File

SOURCE=.\SYSTEMF.H
# End Source File
# Begin Source File

SOURCE=.\TASKER.H
# End Source File
# Begin Source File

SOURCE=.\TRADES.H
# End Source File
# Begin Source File

SOURCE=.\TSLICER.H
# End Source File
# Begin Source File

SOURCE=.\USER.H
# End Source File
# Begin Source File

SOURCE=.\VIDEO.H
# End Source File
# Begin Source File

SOURCE=.\VILLAGE.H
# End Source File
# Begin Source File

SOURCE=.\VOTING.H
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
