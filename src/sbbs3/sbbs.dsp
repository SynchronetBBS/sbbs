# Microsoft Developer Studio Project File - Name="sbbs" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=sbbs - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "sbbs.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "sbbs.mak" CFG="sbbs - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "sbbs - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "sbbs - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "sbbs - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "msvc.win32.dll.release"
# PROP Intermediate_Dir "msvc.win32.dll.release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SBBS_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\include\mozilla\js" /I "..\xpdev" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SBBS" /D "SBBS_EXPORTS" /D "WRAPPER_EXPORTS" /D "SMB_EXPORTS" /D "JAVASCRIPT" /D "RINGBUF_SEM" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib winmm.lib ../../lib/mozilla/js/win32.debug/js32.lib /nologo /dll /map /machine:I386

!ELSEIF  "$(CFG)" == "sbbs - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "sbbs___Win32_Debug"
# PROP BASE Intermediate_Dir "sbbs___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "msvc.win32.dll.debug"
# PROP Intermediate_Dir "msvc.win32.dll.debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SBBS_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\..\include\mozilla\js" /I "..\xpdev" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SBBS" /D "SBBS_EXPORTS" /D "WRAPPER_EXPORTS" /D "SMB_EXPORTS" /D "JAVASCRIPT" /D "RINGBUF_SEM" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib winmm.lib ../../lib/mozilla/js/win32.debug/js32.lib /nologo /dll /map /debug /debugtype:both /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /profile

!ENDIF 

# Begin Target

# Name "sbbs - Win32 Release"
# Name "sbbs - Win32 Debug"
# Begin Source File

SOURCE=.\ansiterm.cpp
# End Source File
# Begin Source File

SOURCE=.\answer.cpp
# End Source File
# Begin Source File

SOURCE=.\ars.c
# End Source File
# Begin Source File

SOURCE=.\atcodes.cpp
# End Source File
# Begin Source File

SOURCE=.\base64.c
# End Source File
# Begin Source File

SOURCE=.\bat_xfer.cpp
# End Source File
# Begin Source File

SOURCE=.\bulkmail.cpp
# End Source File
# Begin Source File

SOURCE=.\chat.cpp
# End Source File
# Begin Source File

SOURCE=.\chk_ar.cpp
# End Source File
# Begin Source File

SOURCE=.\con_hi.cpp
# End Source File
# Begin Source File

SOURCE=.\con_out.cpp
# End Source File
# Begin Source File

SOURCE=.\crc16.c
# End Source File
# Begin Source File

SOURCE=.\crc32.c
# End Source File
# Begin Source File

SOURCE=.\dat_rec.c
# End Source File
# Begin Source File

SOURCE=.\data.cpp
# End Source File
# Begin Source File

SOURCE=.\data_ovl.cpp
# End Source File
# Begin Source File

SOURCE=.\date_str.c
# End Source File
# Begin Source File

SOURCE=..\xpdev\dirwrap.c
# End Source File
# Begin Source File

SOURCE=.\download.cpp
# End Source File
# Begin Source File

SOURCE=.\email.cpp
# End Source File
# Begin Source File

SOURCE=.\exec.cpp
# End Source File
# Begin Source File

SOURCE=.\execfile.cpp
# End Source File
# Begin Source File

SOURCE=.\execfunc.cpp
# End Source File
# Begin Source File

SOURCE=.\execmisc.cpp
# End Source File
# Begin Source File

SOURCE=.\execmsg.cpp
# End Source File
# Begin Source File

SOURCE=.\execnet.cpp
# End Source File
# Begin Source File

SOURCE=.\fido.cpp
# End Source File
# Begin Source File

SOURCE=.\file.cpp
# End Source File
# Begin Source File

SOURCE=.\filedat.c
# End Source File
# Begin Source File

SOURCE=..\xpdev\filewrap.c
# End Source File
# Begin Source File

SOURCE=..\xpdev\genwrap.c
# End Source File
# Begin Source File

SOURCE=.\getkey.cpp
# End Source File
# Begin Source File

SOURCE=.\getmail.c
# End Source File
# Begin Source File

SOURCE=.\getmsg.cpp
# End Source File
# Begin Source File

SOURCE=.\getnode.cpp
# End Source File
# Begin Source File

SOURCE=.\getstr.cpp
# End Source File
# Begin Source File

SOURCE=.\ident.c
# End Source File
# Begin Source File

SOURCE=..\xpdev\ini_file.c
# End Source File
# Begin Source File

SOURCE=.\inkey.cpp
# End Source File
# Begin Source File

SOURCE=.\js_bbs.cpp
# End Source File
# Begin Source File

SOURCE=.\js_client.c
# End Source File
# Begin Source File

SOURCE=.\js_console.cpp
# End Source File
# Begin Source File

SOURCE=.\js_file.c
# End Source File
# Begin Source File

SOURCE=.\js_file_area.c
# End Source File
# Begin Source File

SOURCE=.\js_global.c
# End Source File
# Begin Source File

SOURCE=.\js_msg_area.c
# End Source File
# Begin Source File

SOURCE=.\js_msgbase.c
# End Source File
# Begin Source File

SOURCE=.\js_socket.c
# End Source File
# Begin Source File

SOURCE=.\js_system.c
# End Source File
# Begin Source File

SOURCE=.\js_user.c
# End Source File
# Begin Source File

SOURCE=.\js_xtrn_area.c
# End Source File
# Begin Source File

SOURCE=.\listfile.cpp
# End Source File
# Begin Source File

SOURCE=.\load_cfg.c
# End Source File
# Begin Source File

SOURCE=.\logfile.cpp
# End Source File
# Begin Source File

SOURCE=.\login.cpp
# End Source File
# Begin Source File

SOURCE=.\logon.cpp
# End Source File
# Begin Source File

SOURCE=.\logout.cpp
# End Source File
# Begin Source File

SOURCE=.\lzh.c
# End Source File
# Begin Source File

SOURCE=.\mail.cpp
# End Source File
# Begin Source File

SOURCE=.\main.cpp
# End Source File
# Begin Source File

SOURCE=.\md5.c
# End Source File
# Begin Source File

SOURCE=.\msgtoqwk.cpp
# End Source File
# Begin Source File

SOURCE=.\netmail.cpp
# End Source File
# Begin Source File

SOURCE=.\newuser.cpp
# End Source File
# Begin Source File

SOURCE=.\nopen.c
# End Source File
# Begin Source File

SOURCE=.\pack_qwk.cpp
# End Source File
# Begin Source File

SOURCE=.\pack_rep.cpp
# End Source File
# Begin Source File

SOURCE=.\postmsg.cpp
# End Source File
# Begin Source File

SOURCE=.\prntfile.cpp
# End Source File
# Begin Source File

SOURCE=.\putmsg.cpp
# End Source File
# Begin Source File

SOURCE=.\putnode.cpp
# End Source File
# Begin Source File

SOURCE=.\qwk.cpp
# End Source File
# Begin Source File

SOURCE=.\qwktomsg.cpp
# End Source File
# Begin Source File

SOURCE=.\readmail.cpp
# End Source File
# Begin Source File

SOURCE=.\readmsgs.cpp
# End Source File
# Begin Source File

SOURCE=.\ringbuf.c
# End Source File
# Begin Source File

SOURCE=.\scandirs.cpp
# End Source File
# Begin Source File

SOURCE=.\scansubs.cpp
# End Source File
# Begin Source File

SOURCE=.\scfglib1.c
# End Source File
# Begin Source File

SOURCE=.\scfglib2.c
# End Source File
# Begin Source File

SOURCE=.\scfgsave.c
# End Source File
# Begin Source File

SOURCE=.\smblib.c
# End Source File
# Begin Source File

SOURCE=.\smbtxt.c
# End Source File
# Begin Source File

SOURCE=.\sockopts.c
# End Source File
# Begin Source File

SOURCE=..\xpdev\sockwrap.c
# End Source File
# Begin Source File

SOURCE=.\sortdir.cpp
# End Source File
# Begin Source File

SOURCE=.\str.cpp
# End Source File
# Begin Source File

SOURCE=.\str_util.c
# End Source File
# Begin Source File

SOURCE=.\telgate.cpp
# End Source File
# Begin Source File

SOURCE=.\telnet.c
# End Source File
# Begin Source File

SOURCE=.\text_sec.cpp
# End Source File
# Begin Source File

SOURCE=..\xpdev\threadwrap.c
# End Source File
# Begin Source File

SOURCE=.\tmp_xfer.cpp
# End Source File
# Begin Source File

SOURCE=.\un_qwk.cpp
# End Source File
# Begin Source File

SOURCE=.\un_rep.cpp
# End Source File
# Begin Source File

SOURCE=.\upload.cpp
# End Source File
# Begin Source File

SOURCE=.\userdat.c
# End Source File
# Begin Source File

SOURCE=.\useredit.cpp
# End Source File
# Begin Source File

SOURCE=.\uucode.c
# End Source File
# Begin Source File

SOURCE=.\ver.cpp
# End Source File
# Begin Source File

SOURCE=.\viewfile.cpp
# End Source File
# Begin Source File

SOURCE=.\writemsg.cpp
# End Source File
# Begin Source File

SOURCE=.\xtrn.cpp
# End Source File
# Begin Source File

SOURCE=.\xtrn_sec.cpp
# End Source File
# Begin Source File

SOURCE=.\yenc.c
# End Source File
# End Target
# End Project
