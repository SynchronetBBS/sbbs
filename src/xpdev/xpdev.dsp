# Microsoft Developer Studio Project File - Name="xpdev" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=xpdev - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "xpdev.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "xpdev.mak" CFG="xpdev - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "xpdev - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "xpdev - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "xpdev - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "msvc.win32.release"
# PROP Intermediate_Dir "msvc.win32.release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "../../include/microsoft/psdk" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "_CRT_SECURE_NO_DEPRECATE" /D "_CRT_NONSTDC_NO_DEPRECATE" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "xpdev - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "xpdev___Win32_Debug"
# PROP BASE Intermediate_Dir "xpdev___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "msvc.win32.debug"
# PROP Intermediate_Dir "msvc.win32.debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../../include/microsoft/psdk" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "_CRT_SECURE_NO_DEPRECATE" /D "_CRT_NONSTDC_NO_DEPRECATE" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "xpdev - Win32 Release"
# Name "xpdev - Win32 Debug"
# Begin Source File

SOURCE=.\dat_file.c
# End Source File
# Begin Source File

SOURCE=.\datewrap.c
# End Source File
# Begin Source File

SOURCE=.\dirwrap.c
# End Source File
# Begin Source File

SOURCE=.\filewrap.c
# End Source File
# Begin Source File

SOURCE=.\genwrap.c
# End Source File
# Begin Source File

SOURCE=.\ini_file.c
# End Source File
# Begin Source File

SOURCE=.\link_list.c
# End Source File
# Begin Source File

SOURCE=.\netwrap.c
# End Source File
# Begin Source File

SOURCE=.\semfile.c
# End Source File
# Begin Source File

SOURCE=.\sockwrap.c
# End Source File
# Begin Source File

SOURCE=.\str_list.c
# End Source File
# Begin Source File

SOURCE=.\xp_dl.c
# End Source File
# Begin Source File

SOURCE=.\xpdatetime.c
# End Source File
# Begin Source File

SOURCE=.\xpprintf.c
# End Source File
# End Target
# End Project
