#  OpenDoors 6.20
#  (C) Copyright 1991 - 1997 by Brian Pirie. All Rights Reserved.
#
#  Oct-2001 door32.sys/socket modifications by Rob Swindell (www.synchro.net)
#
#
#         File: Win32.mak
#
#  Description: Makefile used to build the Win32 OpenDoors libraries from
#               the sources. Usage is described below.
#
#    Revisions: Date          Ver   Who  Change
#               ---------------------------------------------------------------
#               Oct 13, 1994  6.00  BP   New file header format.
#               Oct 13, 1994  6.00  BP   Made directories configurable.
#               Oct 13, 1994  6.00  BP   Erase tlib-created backup file.
#               Oct 14, 1994  6.00  BP   Added ODGen.h dependencies.
#               Oct 14, 1994  6.00  BP   Added ODPlat.c module.
#               Oct 31, 1994  6.00  BP   Added headers dependency constant.
#               Nov 01, 1994  6.00  BP   Added ODUtil.c module.
#               Dec 31, 1994  6.00  BP   Added -B option for Borland Cs.
#               Jan 01, 1995  6.00  BP   Added ODKrnl.c, ODKrnl.h.
#               Jan 29, 1995  6.00  BP   Added ODCmdLn.c.
#               Nov 16, 1995  6.00  BP   Added ODInQue.c, and new headers.
#               Nov 21, 1995  6.00  BP   Created ODInit1.c, ODInit2.c.
#               Dec 02, 1995  6.00  BP   Added ODRes.h
#               Dec 02, 1995  6.00  BP   Added ODFrame.c, ODFrame.h.
#               Dec 02, 1995  6.00  BP   Added ODStat.h, ODSwap.h.
#               Dec 04, 1995  6.00  BP   Changes for building Win32 version.
#               Dec 05, 1995  6.00  BP   Split into makefiles for each platform
#               Dec 07, 1995  6.00  BP   Added ODEdit.c.
#               Dec 21, 1995  6.00  BP   Changes for building as DLL.
#               Jan 04, 1996  6.00  BP   Added ODGetIn.c.
#               Feb 09, 1996  6.00  BP   Renamed ODInit?.* to ODInEx?.*
#               Feb 19, 1996  6.00  BP   Turned off OD_DEBUG
#               Feb 19, 1996  6.00  BP   Changed version number to 6.00.
#               Mar 03, 1996  6.10  BP   Begin version 6.10.
#               Oct 19, 2001  6.20  RS   Added door32.sys and socket support.
#
###############################################################################
#
# USAGE INFORMATION
#
###############################################################################
#
# Command Line:   make -fWin32.mak
#                     or
#                 nmake /f Win32.mak
#
###############################################################################
#
# CONFIGURATION
#
# Customize this section of the makefile to provide the relevant information
# for your compiler, assembler (if any) and build environment.
#
###############################################################################
# Compiler executable file name. Use:
#
#                  tcc - For Borland Turbo C and Turbo C++
#                  bcc - For Borland C++
#                   cl - For Microsoft compilers
#
CC=cl
#
#------------------------------------------------------------------------------
#
# Linker executable file name. Use:
#
#                tlink - For Borland compilers
#                 link - For Microsoft compilers
#
LINK=link
#
#------------------------------------------------------------------------------
#
# Resource compiler exectuable file name.
#
RC=rc
#
#------------------------------------------------------------------------------
#
# Win32 compiler command-line flags. Use:
#
# /c /W3 /D "WIN32" /D "_WINDOWS"  - For Microsoft compilers
#
CFLAGS=/nologo /MT /W3 /GX /O2 /D "WIN32" /D "_WINDOWS" /c
# /MTd /Zi - for debug
#
#------------------------------------------------------------------------------
#
# Link flags.
#
LINKFLAGS=kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib wsock32.lib\
          uuid.lib comctl32.lib /NOLOGO /DLL /INCREMENTAL:no\
	  /MAP\
#          /DEBUG\
          /MACHINE:I386\
          /DEF:$(SOURCEDIR)"OpenDoor.def" /OUT:$(LIBDIR)"ODoors62.dll"\
          /IMPLIB:$(LIBDIR)"ODoorW.lib" /SUBSYSTEM:windows,4.0
#
#------------------------------------------------------------------------------
#
# Output directories. customize for your own preferences. Note that trailing
# backslash (\) characters are required.
#
SOURCEDIR=.\                                               # Comments required
ODHEADERDIR=.\                                             # in order to
OBJDIR=.\ 	# was ..\obj                               # avoid line
LIBDIR=.\       # was ..\lib                          	   # concatentation
#
###############################################################################
#
# DEPENDENCIES
#
# You won't normally have to change anything after this point in this makefile.
#
###############################################################################
#
# Define primary target.
#
TARGET=w
all: $(LIBDIR)ODoors62.dll
#
#------------------------------------------------------------------------------
#
# Name of all headers.
#
HEADERS= $(HEADERDIR)ODCom.h\
         $(HEADERDIR)ODCore.h\
         $(HEADERDIR)ODFrame.h\
         $(HEADERDIR)ODGen.h\
         $(HEADERDIR)ODInEx.h\
         $(HEADERDIR)ODInQue.h\
         $(HEADERDIR)ODKrnl.h\
         $(HEADERDIR)ODPlat.h\
         $(HEADERDIR)ODRes.h\
         $(HEADERDIR)ODScrn.h\
         $(HEADERDIR)ODStat.h\
         $(HEADERDIR)ODSwap.h\
         $(HEADERDIR)ODTypes.h\
         $(HEADERDIR)ODUtil.h\
         $(HEADERDIR)OpenDoor.h
#
#------------------------------------------------------------------------------
#
#
DEF_FILE=$(SOURCEDIR)OpenDoor.def
#
#------------------------------------------------------------------------------
#
# Build from C sources.
#
$(OBJDIR)odauto$(TARGET).obj : $(SOURCEDIR)odauto.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odauto.c
   command /c erase $(OBJDIR)odauto$(TARGET).obj
   move odauto.obj $(OBJDIR)odauto$(TARGET).obj

$(OBJDIR)odblock$(TARGET).obj : $(SOURCEDIR)odblock.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odblock.c
   command /c erase $(OBJDIR)odblock$(TARGET).obj
   move odblock.obj $(OBJDIR)odblock$(TARGET).obj

$(OBJDIR)odcfile$(TARGET).obj : $(SOURCEDIR)odcfile.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odcfile.c
   command /c erase $(OBJDIR)odcfile$(TARGET).obj
   move odcfile.obj $(OBJDIR)odcfile$(TARGET).obj

$(OBJDIR)odcmdln$(TARGET).obj : $(SOURCEDIR)odcmdln.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odcmdln.c
   command /c erase $(OBJDIR)odcmdln$(TARGET).obj
   move odcmdln.obj $(OBJDIR)odcmdln$(TARGET).obj

$(OBJDIR)odcom$(TARGET).obj : $(SOURCEDIR)odcom.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odcom.c
   command /c erase $(OBJDIR)odcom$(TARGET).obj
   move odcom.obj $(OBJDIR)odcom$(TARGET).obj

$(OBJDIR)odcore$(TARGET).obj : $(SOURCEDIR)odcore.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odcore.c
   command /c erase $(OBJDIR)odcore$(TARGET).obj
   move odcore.obj $(OBJDIR)odcore$(TARGET).obj

$(OBJDIR)oddrbox$(TARGET).obj : $(SOURCEDIR)oddrbox.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)oddrbox.c
   command /c erase $(OBJDIR)oddrbox$(TARGET).obj
   move oddrbox.obj $(OBJDIR)oddrbox$(TARGET).obj

$(OBJDIR)odedit$(TARGET).obj : $(SOURCEDIR)odedit.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odedit.c
   command /c erase $(OBJDIR)odedit$(TARGET).obj
   move odedit.obj $(OBJDIR)odedit$(TARGET).obj

$(OBJDIR)odedstr$(TARGET).obj : $(SOURCEDIR)odedstr.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odedstr.c
   command /c erase $(OBJDIR)odedstr$(TARGET).obj
   move odedstr.obj $(OBJDIR)odedstr$(TARGET).obj

$(OBJDIR)odemu$(TARGET).obj : $(SOURCEDIR)odemu.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odemu.c
   command /c erase $(OBJDIR)odemu$(TARGET).obj
   move odemu.obj $(OBJDIR)odemu$(TARGET).obj

$(OBJDIR)odframe$(TARGET).obj : $(SOURCEDIR)odframe.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odframe.c
   command /c erase $(OBJDIR)odframe$(TARGET).obj
   move odframe.obj $(OBJDIR)odframe$(TARGET).obj

$(OBJDIR)odgetin$(TARGET).obj : $(SOURCEDIR)odgetin.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odgetin.c
   command /c erase $(OBJDIR)odgetin$(TARGET).obj
   move odgetin.obj $(OBJDIR)odgetin$(TARGET).obj

$(OBJDIR)odgraph$(TARGET).obj : $(SOURCEDIR)odgraph.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odgraph.c
   command /c erase $(OBJDIR)odgraph$(TARGET).obj
   move odgraph.obj $(OBJDIR)odgraph$(TARGET).obj

$(OBJDIR)odinex1$(TARGET).obj : $(SOURCEDIR)odinex1.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odinex1.c
   command /c erase $(OBJDIR)odinex1$(TARGET).obj
   move odinex1.obj $(OBJDIR)odinex1$(TARGET).obj

$(OBJDIR)odinex2$(TARGET).obj : $(SOURCEDIR)odinex2.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odinex2.c
   command /c erase $(OBJDIR)odinex2$(TARGET).obj
   move odinex2.obj $(OBJDIR)odinex2$(TARGET).obj

$(OBJDIR)odinque$(TARGET).obj : $(SOURCEDIR)odinque.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odinque.c
   command /c erase $(OBJDIR)odinque$(TARGET).obj
   move odinque.obj $(OBJDIR)odinque$(TARGET).obj

$(OBJDIR)odkrnl$(TARGET).obj : $(SOURCEDIR)odkrnl.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odkrnl.c
   command /c erase $(OBJDIR)odkrnl$(TARGET).obj
   move odkrnl.obj $(OBJDIR)odkrnl$(TARGET).obj

$(OBJDIR)odlist$(TARGET).obj : $(SOURCEDIR)odlist.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odlist.c
   command /c erase $(OBJDIR)odlist$(TARGET).obj
   move odlist.obj $(OBJDIR)odlist$(TARGET).obj

$(OBJDIR)odlog$(TARGET).obj : $(SOURCEDIR)odlog.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odlog.c
   command /c erase $(OBJDIR)odlog$(TARGET).obj
   move odlog.obj $(OBJDIR)odlog$(TARGET).obj

$(OBJDIR)odmulti$(TARGET).obj : $(SOURCEDIR)odmulti.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odmulti.c
   command /c erase $(OBJDIR)odmulti$(TARGET).obj
   move odmulti.obj $(OBJDIR)odmulti$(TARGET).obj

$(OBJDIR)odplat$(TARGET).obj : $(SOURCEDIR)odplat.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odplat.c
   command /c erase $(OBJDIR)odplat$(TARGET).obj
   move odplat.obj $(OBJDIR)odplat$(TARGET).obj

$(OBJDIR)odpcb$(TARGET).obj : $(SOURCEDIR)odpcb.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odpcb.c
   command /c erase $(OBJDIR)odpcb$(TARGET).obj
   move odpcb.obj $(OBJDIR)odpcb$(TARGET).obj

$(OBJDIR)odpopup$(TARGET).obj : $(SOURCEDIR)odpopup.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odpopup.c
   command /c erase $(OBJDIR)odpopup$(TARGET).obj
   move odpopup.obj $(OBJDIR)odpopup$(TARGET).obj

$(OBJDIR)odprntf$(TARGET).obj : $(SOURCEDIR)odprntf.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odprntf.c
   command /c erase $(OBJDIR)odprntf$(TARGET).obj
   move odprntf.obj $(OBJDIR)odprntf$(TARGET).obj

$(OBJDIR)odra$(TARGET).obj : $(SOURCEDIR)odra.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odra.c
   command /c erase $(OBJDIR)odra$(TARGET).obj
   move odra.obj $(OBJDIR)odra$(TARGET).obj

$(OBJDIR)odscrn$(TARGET).obj : $(SOURCEDIR)odscrn.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odscrn.c
   command /c erase $(OBJDIR)odscrn$(TARGET).obj
   move odscrn.obj $(OBJDIR)odscrn$(TARGET).obj

$(OBJDIR)odspawn$(TARGET).obj : $(SOURCEDIR)odspawn.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odspawn.c
   command /c erase $(OBJDIR)odspawn$(TARGET).obj
   move odspawn.obj $(OBJDIR)odspawn$(TARGET).obj

$(OBJDIR)odstand$(TARGET).obj : $(SOURCEDIR)odstand.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odstand.c
   command /c erase $(OBJDIR)odstand$(TARGET).obj
   move odstand.obj $(OBJDIR)odstand$(TARGET).obj

$(OBJDIR)odstat$(TARGET).obj : $(SOURCEDIR)odstat.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odstat.c
   command /c erase $(OBJDIR)odstat$(TARGET).obj
   move odstat.obj $(OBJDIR)odstat$(TARGET).obj

# This file (odsys.c) wasn't included in 6.1.1 source <shrug>
#$(OBJDIR)odsys$(TARGET).obj : $(SOURCEDIR)odsys.c $(HEADERS)
#   $(CC) $(CFLAGS) $(SOURCEDIR)odsys.c
#   command /c erase $(OBJDIR)odsys$(TARGET).obj
#   move odsys.obj $(OBJDIR)odsys$(TARGET).obj

$(OBJDIR)odutil$(TARGET).obj : $(SOURCEDIR)odutil.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odutil.c
   command /c erase $(OBJDIR)odutil$(TARGET).obj
   move odutil.obj $(OBJDIR)odutil$(TARGET).obj

$(OBJDIR)odwcat$(TARGET).obj : $(SOURCEDIR)odwcat.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odwcat.c
   command /c erase $(OBJDIR)odwcat$(TARGET).obj
   move odwcat.obj $(OBJDIR)odwcat$(TARGET).obj

$(OBJDIR)odwin$(TARGET).obj : $(SOURCEDIR)odwin.c $(HEADERS)
   $(CC) $(CFLAGS) $(SOURCEDIR)odwin.c
   command /c erase $(OBJDIR)odwin$(TARGET).obj
   move odwin.obj $(OBJDIR)odwin$(TARGET).obj
#
#------------------------------------------------------------------------------
#
# Build from resource script.
#
$(OBJDIR)ODoor$(TARGET).res: $(SOURCEDIR)ODRes.rc
   $(RC) $(SOURCEDIR)ODRes.rc
   command /c erase $(LIBDIR)ODoor$(TARGET).res
   move $(SOURCEDIR)ODRes.res $(OBJDIR)ODoor$(TARGET).res
#
#------------------------------------------------------------------------------
#
# Build DLL from objects.
#
OBJECTS= $(OBJDIR)odauto$(TARGET).obj\
         $(OBJDIR)odblock$(TARGET).obj\
         $(OBJDIR)odcfile$(TARGET).obj\
         $(OBJDIR)odcmdln$(TARGET).obj\
         $(OBJDIR)odcom$(TARGET).obj\
         $(OBJDIR)odcore$(TARGET).obj\
         $(OBJDIR)oddrbox$(TARGET).obj\
         $(OBJDIR)odedit$(TARGET).obj\
         $(OBJDIR)odedstr$(TARGET).obj\
         $(OBJDIR)odemu$(TARGET).obj\
         $(OBJDIR)odframe$(TARGET).obj\
         $(OBJDIR)odgetin$(TARGET).obj\
         $(OBJDIR)odgraph$(TARGET).obj\
         $(OBJDIR)odinex1$(TARGET).obj\
         $(OBJDIR)odinex2$(TARGET).obj\
         $(OBJDIR)odinque$(TARGET).obj\
         $(OBJDIR)odkrnl$(TARGET).obj\
         $(OBJDIR)odlist$(TARGET).obj\
         $(OBJDIR)odlog$(TARGET).obj\
         $(OBJDIR)odmulti$(TARGET).obj\
         $(OBJDIR)odplat$(TARGET).obj\
         $(OBJDIR)odpcb$(TARGET).obj\
         $(OBJDIR)odpopup$(TARGET).obj\
         $(OBJDIR)odprntf$(TARGET).obj\
         $(OBJDIR)odra$(TARGET).obj\
         $(OBJDIR)odscrn$(TARGET).obj\
         $(OBJDIR)odspawn$(TARGET).obj\
         $(OBJDIR)odstand$(TARGET).obj\
         $(OBJDIR)odstat$(TARGET).obj\
#         $(OBJDIR)odsys$(TARGET).obj\	this file is missing
         $(OBJDIR)odutil$(TARGET).obj\
         $(OBJDIR)odwcat$(TARGET).obj\
         $(OBJDIR)odwin$(TARGET).obj\
         $(OBJDIR)ODoor$(TARGET).res
$(LIBDIR)ODoors62.dll : $(DEF_FILE) $(OBJECTS)
   $(LINK) @<<
   $(LINKFLAGS) $(OBJECTS)
<<
#
#------------------------------------------------------------------------------
