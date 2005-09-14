# Makefile

#########################################################################
# Makefile for Synchronet BBS for Win32									#
# For use with Borland make and Borland C++	(or C++Builder)				#
# @format.tab-size 4, @format.use-tabs true								#
#########################################################################

# $Id$

SRC_ROOT	=	..
!include $(SRC_ROOT)\build\Common.bmake

UTIL_LDFLAGS	=	-q $(SMBLIB_LDFLAGS) $(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV_LDFLAGS)
CFLAGS	=	$(CFLAGS) -w-csu -w-par -w-aus
MKSHLIB	=	$(CC) -WD
WILDARGS=	$(MAKEDIR)\..\lib\wildargs.obj

# JS and NSPR setup stuff...
CFLAGS = $(CFLAGS) -DJAVASCRIPT
!ifdef JSINCLUDE
	CFLAGS = $(CFLAGS) -I$(JSINCLUDE)
!else
	CFLAGS = $(CFLAGS) -I$(SRC_ROOT)$(DIRSEP)..$(DIRSEP)include$(DIRSEP)mozilla$(DIRSEP)js
!endif
!ifdef NSPRINCLUDE
	CFLAGS = $(CFLAGS) -I$(NSPRINCLUDE)
!else
	CFLAGS = $(CFLAGS) -I$(SRC_ROOT)$(DIRSEP)..$(DIRSEP)include$(DIRSEP)mozilla$(DIRSEP)nspr
!endif

!ifndef JSLIBDIR
	JSLIBDIR = $(SRC_ROOT)$(DIRSEP)..$(DIRSEP)lib$(DIRSEP)mozilla$(DIRSEP)js$(DIRSEP)win32.$(BUILDPATH)
!endif
!ifndef JSLIB
	JSLIB	=	js32omf
!endif
!ifndef NSPRDIR
	# There *IS* no debug build in CVS
	# That's ok, looks like it doesn't need NSPR4
	#NSPRDIR = $(SRC_ROOT)$(DIRSEP)..$(DIRSEP)lib$(DIRSEP)mozilla$(DIRSEP)nspr$(DIRSEP)win32.$(BUILDPATH)
	#NSPRDIR = $(SRC_ROOT)$(DIRSEP)..$(DIRSEP)lib$(DIRSEP)mozilla$(DIRSEP)nspr$(DIRSEP)win32.release
!endif
JS_LDFLAGS = $(JS_LDFLAGS) $(JSLIBDIR)$(DIRSEP)$(UL_PRE)$(JSLIB)$(UL_SUF)
# Looks like it doesn't need NSPR4
#JS_LDFLAGS = $(JS_LDFLAGS) -L$(NSPRDIR) $(UL_PRE)nspr4$(UL_SUF)

CFLAGS	=	$(CFLAGS) $(JS_CFLAGS)
LDFLAGS	=	$(LDFLAGS) $(JS_LDFLAGS)

!include sbbsdefs.mk
MT_CFLAGS	=	$(MT_CFLAGS) $(SBBSDEFS)
CFLAGS	=	$(CFLAGS) -DMD5_IMPORTS

CON_LIBS	= $(UL_PRE)sbbs$(UL_SUF) $(UL_PRE)ftpsrvr$(UL_SUF) $(UL_PRE)websrvr$(UL_SUF) $(UL_PRE)mailsrvr$(UL_SUF) $(UL_PRE)services$(UL_SUF)

CFLAGS	=	$(CFLAGS) $(UIFC-MT_CFLAGS) $(XPDEV-MT_CFLAGS) $(SMBLIB_CFLAGS) $(CIOLIB-MT_CFLAGS) -DNO_SOCKET_SUPPORT
LDFLAGS =	$(LDFLAGS) $(UIFC-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(SMBLIB_LDFLAGS) $(CIOLIB-MT_LDFLAGS)

# Monolithic Synchronet executable Build Rule
$(SBBSMONO): $(MONO_OBJS) $(OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(MT_LDFLAGS) -e$@ $(LDFLAGS) $(SMBLIB) $(XPDEV-MT_LIB)  @&&|
	$** 
|

# Synchronet BBS library Link Rule
$(SBBS): $(OBJS) $(LIBS)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(MT_LDFLAGS) -lGi -e$@ $(LDFLAGS) $(SHLIBOPTS) \
		$(SMBLIB) $(XPDEV-MT_LIB) @&&|
	$**
|

# FTP Server Link Rule
$(FTPSRVR): $(FTP_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(MT_LDFLAGS) -lGi -e$@ $(LDFLAGS) $(SHLIBOPTS) \
		$(XPDEV-MT_LIB) $(LIBODIR)$(DIRSEP)sbbs.lib @&&|
	$**
|

# Mail Server Link Rule

$(MAILSRVR): $(MAIL_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(MT_LDFLAGS) -lGi -e$@ $(LDFLAGS) $(SHLIBOPTS) \
		$(XPDEV-MT_LIB) $(LIBODIR)$(DIRSEP)sbbs.lib @&&|
	$**
|

# Mail Server Link Rule
$(WEBSRVR): $(WEB_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(MT_LDFLAGS) -lGi -e$@ $(LDFLAGS) $(SHLIBOPTS) \
		$(XPDEV-MT_LIB) $(LIBODIR)$(DIRSEP)sbbs.lib @&&|
	$**
|

# Services Link Rule
$(SERVICES): $(SERVICE_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(MT_LDFLAGS) -lGi -e$@ $(LDFLAGS) $(SHLIBOPTS) \
		$(XPDEV-MT_LIB) $(LIBODIR)$(DIRSEP)sbbs.lib @&&|
	$**
|

# Synchronet Console Build Rule
$(SBBSCON): $(CON_OBJS) $(SBBS) $(FTPSRVR) $(WEBSRVR) $(MAILSRVR) $(SERVICES)
	@echo Linking $@
	$(QUIET)$(CC) $(MT_LDFLAGS) -e$@ $(LDFLAGS) $(CON_OBJS) $(CON_LIB) \
		$(XPDEV-MT_LIB) -L$(LIBODIR) $(LIBODIR)$(DIRSEP)sbbs.lib \
		$(LIBODIR)$(DIRSEP)ftpsrvr.lib $(LIBODIR)$(DIRSEP)mailsrvr.lib \
		$(LIBODIR)$(DIRSEP)websrvr.lib $(LIBODIR)$(DIRSEP)services.lib

# Baja Utility
$(BAJA): $(BAJA_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $** $(SMBLIB_LIBS) $(XPDEV_LIBS)

# UnBaja Utility
$(UNBAJA): $(UNBAJA_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $** $(XPDEV_LIBS) $(WILDARGS)

# Node Utility
$(NODE): $(NODE_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $** $(XPDEV_LIBS)

# FIXSMB Utility
$(FIXSMB): $(FIXSMB_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $** $(SMBLIB_LIBS) $(XPDEV_LIBS) $(WILDARGS)

# CHKSMB Utility
$(CHKSMB): $(CHKSMB_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $** $(SMBLIB_LIBS) $(XPDEV_LIBS) $(WILDARGS)

# SMB Utility
$(SMBUTIL): $(SMBUTIL_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $** $(SMBLIB_LIBS) $(XPDEV_LIBS) $(WILDARGS)

# SBBSecho (FidoNet Packet Tosser)
$(SBBSECHO): $(SBBSECHO_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $** $(SMBLIB_LIBS) $(XPDEV_LIBS)

# SBBSecho Configuration Program
$(ECHOCFG): $(ECHOCFG_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) $(MT_LDFLAGS) -e$@ $** \
		$(UIFC-MT_LDFLAGS) $(SMBLIB_LIBS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)

# ADDFILES
$(ADDFILES): $(ADDFILES_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $** $(XPDEV_LIBS)

# FILELIST
$(FILELIST): $(FILELIST_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $** $(XPDEV_LIBS)

# MAKEUSER
$(MAKEUSER): $(MAKEUSER_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $** $(XPDEV_LIBS)

# JSEXEC
$(JSEXEC): $(JSEXEC_OBJS) $(SBBS)
	@echo Linking $@
	$(QUIET)$(CC) -e$@ $(LDFLAGS) $(JSEXEC_OBJS) $(XPDEV-MT_LIB) \
		$(LIBODIR)$(DIRSEP)$(UL_PRE)sbbs$(UL_SUF)

# ANS2ASC
$(ANS2ASC): $(OBJODIR)$(DIRSEP)ans2asc$(OFILE)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $**

# ASC2ANS
$(ASC2ANS): $(OBJODIR)$(DIRSEP)asc2ans$(OFILE)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $**

$(MTOBJODIR)$(DIRSEP)ftpsrvr$(OFILE): ftpsrvr.c
        $(QUIET)$(CC) $(CFLAGS) $(CCFLAGS) -DFTPSRVR_EXPORTS -DSMB_IMPORTS -USBBS_EXPORTS \
			-n$(MTOBJODIR) $(MT_CFLAGS) -c $** $(OUTPUT)$@

$(MTOBJODIR)$(DIRSEP)mailsrvr$(OFILE): mailsrvr.c
        $(QUIET)$(CC) $(CFLAGS) $(CCFLAGS) -DMAILSRVR_EXPORTS -DSMB_IMPORTS -USBBS_EXPORTS \
			-n$(MTOBJODIR) $(MT_CFLAGS) -c $** $(OUTPUT)$@

$(MTOBJODIR)$(DIRSEP)websrvr$(OFILE): websrvr.c
        $(QUIET)$(CC) $(CFLAGS) $(CCFLAGS) -DWEBSRVR_EXPORTS -DSMB_IMPORTS -USBBS_EXPORTS \
			-n$(MTOBJODIR) $(MT_CFLAGS) -c $** $(OUTPUT)$@

$(MTOBJODIR)$(DIRSEP)services$(OFILE): services.c
        $(QUIET)$(CC) $(CFLAGS) $(CCFLAGS) -DSERVICES_EXPORTS -DSMB_IMPORTS -USBBS_EXPORTS \
			-n$(MTOBJODIR) $(MT_CFLAGS) -c $** $(OUTPUT)$@

# QWKNodes
$(QWKNODES): $(QWKNODES_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $** $(SMBLIB_LIBS) $(XPDEV_LIBS)

# SLOG
$(SLOG): $(SLOG_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $** $(XPDEV_LIBS)

# ALLUSERS
$(ALLUSERS): $(ALLUSERS_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -e$@ $** $(XPDEV_LIBS)

