# Makefile

#########################################################################
# Makefile for Synchronet BBS 											#
# For use with Borland C++ Builder 5+ or Borland C++ 5.5 for Win32      #
# @format.tab-size 4													#
#																		#
# usage: make															#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id$

# Macros
#DEBUG	=	1				# Comment out for release (non-debug) version
JS		=	1				# Comment out for non-JavaScript (v3.00) build
CC		=	bcc32
LD		=	ilink32
SLASH	=	\\
OFILE	=	obj
LIBFILE	=	.dll
EXEFILE	=	.exe
LIBODIR	=	bcc.win32.dll	# Library output directory
EXEODIR =	bcc.win32.exe	# Executable output directory
UIFC	=	..\uifc\		# Path to User Interfce library
XPDEV	=	..\xpdev\		# Path to Cross-platform wrappers
CFLAGS	=	-M -I$(XPDEV) -I$(UIFC)
LFLAGS  =	-m -s -c -Tpd -Gi -I$(LIBODIR)
DELETE	=	echo y | del 
WILDARGS=	$(MAKEDIR)\..\lib\wildargs.obj

# Enable auto-dependency checking
.autodepend
.cacheautodepend	

# Optional compile flags (disable banner, warnings and such)
CFLAGS	=	$(CFLAGS) -q -d -H -X- -w-csu -w-pch -w-ccc -w-rch -w-par -w-8004
CFLAGS	=	$(CFLAGS) -DWRAPPER_EXPORTS

# Debug or release build?
!ifdef DEBUG
CFLAGS	=	$(CFLAGS) -v -Od -D_DEBUG 
LFLAGS	=	$(LFLAGS) -v
BUILD	=	debug
!else
CFLAGS	=	$(CFLAGS)
BUILD	=	release
!endif

LIBODIR	=	$(LIBODIR).$(BUILD)
EXEODIR	=	$(EXEODIR).$(BUILD)

# JavaScript Support
!ifdef JS
CFLAGS	= 	$(CFLAGS) -DJAVASCRIPT -I../../include/mozilla/js
LIBS	=	..\..\lib\mozilla\js\win32.$(BUILD)\js32omf.lib
!endif

!ifndef VERBOSE
QUIET	=	@
!endif

default: dlls mono utils

# Cross platform/compiler definitions
!include targets.mk		# defines all targets
!include objects.mk		# defines $(OBJS)
!include sbbsdefs.mk	# defines $(SBBSDEFS)

SBBSLIB	=	$(LIBODIR)\sbbs.lib

$(SBBSLIB):	$(SBBS)

.path.c = .;$(XPDEV)
.path.cpp = .;$(XPDEV)

# Implicit C Compile Rule for SBBS.DLL
.c.obj:
	$(QUIET)$(CC) $(CFLAGS) -WD -WM -n$(LIBODIR) -c $(SBBSDEFS) $<

# Implicit C++ Compile Rule for SBBS.DLL
.cpp.obj:
	$(QUIET)$(CC) $(CFLAGS) -WD -WM -n$(LIBODIR) -c $(SBBSDEFS) $<

# Create output directories if they don't exist
$(LIBODIR):
	$(QUIET)if not exist $(LIBODIR) mkdir $(LIBODIR)
$(EXEODIR):
	$(QUIET)if not exist $(EXEODIR) mkdir $(EXEODIR)

# Monolithic Synchronet executable Build Rule
$(SBBSMONO): $(OBJS) \
	$(LIBODIR)\sbbscon.obj \
	$(LIBODIR)\sbbs_ini.obj \
	$(LIBODIR)\ftpsrvr.obj \
	$(LIBODIR)\websrvr.obj \
	$(LIBODIR)\mailsrvr.obj $(LIBODIR)\mxlookup.obj $(LIBODIR)\mime.obj $(LIBODIR)\base64.obj \
	$(LIBODIR)\services.obj
	$(QUIET)$(CC) $(CFLAGS) -WM -e$(SBBSMONO) $(LIBS) @&&|
	$**
|

# SBBS DLL Link Rule
$(SBBS): $(OBJS)
    @echo Linking $< ...
	$(QUIET)$(LD) $(LFLAGS) c0d32.obj $(LIBS) $(OBJS), $*, $*, \
		import32.lib cw32mt.lib ws2_32.lib

# Mail Server DLL Link Rule
$(MAILSRVR): mailsrvr.c mxlookup.c mime.c base64.c md5.c crc32.c ini_file.c $(SBBSLIB)
    @echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -WD -WM -lGi -n$(LIBODIR) \
		-DMAILSRVR_EXPORTS -DSMB_IMPORTS -DWRAPPER_IMPORTS $** $(LIBS)

# FTP Server DLL Link Rule
$(FTPSRVR): ftpsrvr.c nopen.c $(SBBSLIB)
    @echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -WD -WM -lGi -n$(LIBODIR) \
		-DFTPSRVR_EXPORTS -DWRAPPER_IMPORTS $** $(LIBS)

# FTP Server DLL Link Rule
$(WEBSRVR): wesrvr.c $(XPDEV)sockwrap.c base64.c
    @echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -WD -WM -lGi -n$(LIBODIR) \
		-DWEBSRVR_EXPORTS -DWRAPPER_IMPORTS $** $(LIBS)

# Services DLL Link Rule
$(SERVICES): services.c $(SBBSLIB) $(LIBODIR)\ini_file.obj
    @echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -WD -WM -lGi -n$(LIBODIR) \
		-DSERVICES_EXPORTS -DWRAPPER_IMPORTS $** $(LIBS)

# Synchronet Console Build Rule
$(SBBSCON): sbbscon.c $(SBBSLIB)
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $**

# Baja Utility
$(BAJA): baja.c ars.c crc32.c
	@echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $** 

# Node Utility
$(NODE): node.c 
	@echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $** 

SMBLIB = $(LIBODIR)\smblib.obj $(LIBODIR)\genwrap.obj $(LIBODIR)\filewrap.obj \
	 $(LIBODIR)\crc16.obj

# FIXSMB Utility
$(FIXSMB): fixsmb.c $(SMBLIB) $(LIBODIR)\str_util.obj $(LIBODIR)\dirwrap.obj
	@echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $**

# CHKSMB Utility
$(CHKSMB): chksmb.c $(SMBLIB) $(XPDEV)dirwrap.c
	@echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $** $(WILDARGS)

# SMB Utility
$(SMBUTIL): smbutil.c smbtxt.c crc32.c lzh.c date_str.c str_util.c $(SMBLIB) $(XPDEV)dirwrap.c
	@echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $** $(WILDARGS)

# SBBSecho (FidoNet Packet Tosser)
$(SBBSECHO): sbbsecho.c rechocfg.c smbtxt.c crc32.c lzh.c $(SMBLIB) \
	$(LIBODIR)\ars.obj \
	$(LIBODIR)\nopen.obj \
	$(LIBODIR)\str_util.obj \
	$(LIBODIR)\date_str.obj \
	userdat.c \
	dat_rec.c \
	dirwrap.c \
	$(LIBODIR)\load_cfg.obj \
	$(LIBODIR)\scfglib1.obj \
	$(LIBODIR)\scfglib2.obj
	@echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $** 

# SBBSecho Configuration Program
$(ECHOCFG): echocfg.c rechocfg.c \
	$(UIFC)uifc.c \
	$(UIFC)uifcx.c \
	$(LIBODIR)\genwrap.obj \
	$(LIBODIR)\dirwrap.obj \
	$(LIBODIR)\nopen.obj \
	$(LIBODIR)\crc16.obj \
	$(LIBODIR)\str_util.obj
	@echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $** 

# ADDFILES
$(ADDFILES): addfiles.c \
	$(LIBODIR)\ars.obj \
	$(LIBODIR)\nopen.obj \
	$(LIBODIR)\crc16.obj \
	$(LIBODIR)\str_util.obj \
	$(LIBODIR)\date_str.obj \
	$(LIBODIR)\userdat.obj \
	dat_rec.c \
	filedat.c \
	genwrap.c \
	dirwrap.c \
	$(LIBODIR)\load_cfg.obj \
	$(LIBODIR)\scfglib1.obj \
	$(LIBODIR)\scfglib2.obj
	@echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $** 

# FILELIST
$(FILELIST): filelist.c \
	$(LIBODIR)\ars.obj \
	$(LIBODIR)\nopen.obj \
	$(LIBODIR)\crc16.obj \
	$(LIBODIR)\str_util.obj \
	$(LIBODIR)\date_str.obj \
	dat_rec.c \
	filedat.c \
	genwrap.c \
	dirwrap.c \
	$(LIBODIR)\load_cfg.obj \
	$(LIBODIR)\scfglib1.obj \
	$(LIBODIR)\scfglib2.obj
	@echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $** 

# MAKEUSER
$(MAKEUSER): makeuser.c \
	$(LIBODIR)\ars.obj \
	$(LIBODIR)\nopen.obj \
	$(LIBODIR)\crc16.obj \
	$(LIBODIR)\str_util.obj \
	$(LIBODIR)\date_str.obj \
	dat_rec.c \
	userdat.c \
	genwrap.c \
	dirwrap.c \
	$(LIBODIR)\load_cfg.obj \
	$(LIBODIR)\scfglib1.obj \
	$(LIBODIR)\scfglib2.obj
	@echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $** 

# JSEXEC
$(JSEXEC): jsexec.c \
	$(LIBS) \
	$(SBBSLIB)
	@echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $**

# ANS2ASC
$(ANS2ASC): ans2asc.c
	@echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $** 

# ASC2ANS
$(ASC2ANS): asc2ans.c
	@echo Creating $@
	$(QUIET)$(CC) $(CFLAGS) -n$(EXEODIR) $** 


