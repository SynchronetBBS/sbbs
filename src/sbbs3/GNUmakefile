# GNUmakefile

#########################################################################
# Makefile for Synchronet BBS for Unix									#
# For use with GNU make and GNU C Compiler or Borland Kylix C++			#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# gcc: gmake															#
# Borland (still in testing/debuging stage): gmake bcc=1				#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id$

SRC_ROOT	=	..
include $(SRC_ROOT)/build/Common.gmake

UTIL_LDFLAGS	:=	$(LDFLAGS)
UTIL_LDFLAGS	+=	$(SMBLIB_LDFLAGS) $(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV_LDFLAGS)
UTIL_LIBS	+=	$(SMBLIB_LIBS)

ifndef bcc
 ifneq ($(os),sunos)
  LDFLAGS	+=	-lutil
 endif
endif

ifeq ($(os),sunos)    # Solaris
 LDFLAGS += -lnsl -lrt
endif

# So far, only QNX has sem_timedwait()
ifeq ($(os),qnx)
 LDFLAGS += -lsocket
endif

ifdef PREFIX
 CFLAGS += -DPREFIX=$(PREFIX)
endif

ifdef USE_DOSEMU
 CFLAGS += -DUSE_DOSEMU
endif

ifdef DONT_BLAME_SYNCHRONET
 CFLAGS += -DDONT_BLAME_SYNCHRONET
endif

# JS and NSPR setup stuff...
CFLAGS += -DJAVASCRIPT
ifdef JSINCLUDE
 CFLAGS += -I$(JSINCLUDE)
else
 CFLAGS += -I$(SRC_ROOT)$(DIRSEP)..$(DIRSEP)include$(DIRSEP)mozilla$(DIRSEP)js
endif
ifdef NSPRINCLUDE
 CFLAGS += -I$(NSPRINCLUDE)
else
 # Use local NSPR first...
 CFLAGS += -I/usr/local/include -I$(SRC_ROOT)$(DIRSEP)..$(DIRSEP)include$(DIRSEP)mozilla$(DIRSEP)nspr
endif
ifndef JSLIBDIR
 JSLIBDIR := $(SRC_ROOT)$(DIRSEP)..$(DIRSEP)lib$(DIRSEP)mozilla$(DIRSEP)js$(DIRSEP)$(machine).$(BUILD)
endif
ifndef JSLIB
 JSLIB	:=	js
endif
ifndef NSPRDIR
 NSPRDIR := $(SRC_ROOT)$(DIRSEP)..$(DIRSEP)lib$(DIRSEP)mozilla$(DIRSEP)nspr$(DIRSEP)$(machine).$(BUILD)
endif
JS_LDFLAGS += -L$(JSLIBDIR) -l$(JSLIB)
#The following is needed for nspr support on Linux
ifeq ($(os),linux)
 JS_LDFLAGS	+=	-ldl
endif
JS_LDFLAGS	+=	-L/usr/local/lib -L$(NSPRDIR) -lnspr4

CFLAGS	+=	$(JS_CFLAGS)
LDFLAGS	+=	$(JS_LDFLAGS)

include sbbsdefs.mk
MT_CFLAGS	+=	$(SBBSDEFS)

# Set up LD_RUN_PATH for run-time locating of the .so files
PWD	:=	$(shell pwd)
ifdef SBBSDIR
 ifeq ($(os),sunos)
  LD_RUN_PATH	:=	$(SBBSDIR)/exec:$(PWD)/$(LIBODIR):$(PWD)/$(JSLIBDIR):$(PWD)/$(NSPRDIR):/opt/sfw/gcc-3/lib
 else
  LD_RUN_PATH	:=	$(SBBSDIR)/exec:$(PWD)/$(LIBODIR):$(PWD)/$(JSLIBDIR):$(PWD)/$(NSPRDIR)
 endif
else
 ifeq ($(os),sunos)
  LD_RUN_PATH	:=	$(PWD)/$(LIBODIR):$(PWD)/$(JSLIBDIR):$(PWD)/$(NSPRDIR):/opt/sfw/gcc-3/lib
 else
  LD_RUN_PATH	:=	$(PWD)/$(LIBODIR):$(PWD)/$(JSLIBDIR):$(PWD)/$(NSPRDIR)
 endif
endif
export LD_RUN_PATH

CON_LIBS	= -lsbbs -lftpsrvr -lwebsrvr -lmailsrvr -lservices
SHLIBOPTS	:=	-shared
ifeq ($(os),darwin)
 MKSHLIB		:=	libtool -dynamic -framework System -lcc_dynamic
 MKSHPPLIB		:=	libtool -dynamic -framework System -lcc_dynamic -lstdc++
 SHLIBOPTS	:=	
else
 ifeq ($(os),sunos)
  MKSHLIB		:=	/usr/ccs/bin/ld -G
  MKSHPPLIB		:=	/usr/ccs/bin/ld -G
  SHLIBOPTS	:=	
 else
  MKSHLIB		:=	$(CC)
  MKSHPPLIB		:=	$(CXX)
 endif
endif

CFLAGS	+=	$(UIFC-MT_CFLAGS) $(XPDEV-MT_CFLAGS) $(SMBLIB_CFLAGS) $(CIOLIB-MT_CFLAGS)
LDFLAGS +=	$(UIFC-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(SMBLIB_LDFLAGS) $(CIOLIB-MT_LDFLAGS)

# Monolithic Synchronet executable Build Rule
$(SBBSMONO): $(MONO_OBJS) $(OBJS)
	@echo Linking $@
	$(QUIET)$(CXX) -o $@ $(LDFLAGS) $(MT_LDFLAGS) $(MONO_OBJS) $(OBJS) $(SMBLIB_LIBS) $(XPDEV-MT_LIBS)

# Synchronet BBS library Link Rule
$(SBBS): $(OBJS) $(LIBS)
	@echo Linking $@
	$(QUIET)$(MKSHPPLIB) $(LDFLAGS) -o $@ $(OBJS) $(LIBS) $(SHLIBOPTS)

# FTP Server Link Rule
$(FTPSRVR): $(MTOBJODIR)/ftpsrvr.o
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(MTOBJODIR)/ftpsrvr.o $(SHLIBOPTS) -o $@

# Mail Server Link Rule
$(MAILSRVR): $(MAIL_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(MAIL_OBJS) $(SHLIBOPTS) -o $@

# Web Server Link Rule
$(WEBSRVR): $(WEB_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(WEB_OBJS) $(SHLIBOPTS) -o $@

# Services Link Rule
$(SERVICES): $(SERVICE_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(SERVICE_OBJS) $(SHLIBOPTS) -o $@

# Synchronet Console Build Rule
$(SBBSCON): $(CON_OBJS) $(SBBS) $(FTPSRVR) $(WEBSRVR) $(MAILSRVR) $(SERVICES)
	@echo Linking $@
	$(QUIET)$(CXX) $(LDFLAGS) $(MT_LDFLAGS) -o $@ $(CON_OBJS) $(CON_LIBS) $(SMBLIB_LIBS) $(XPDEV-MT_LIBS)

# Baja Utility
$(BAJA): $(BAJA_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(BAJA_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS)

# UnBaja Utility
$(UNBAJA): $(OBJODIR) $(EXEODIR) $(UNBAJA_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(UNBAJA_OBJS) $(XPDEV_LIBS) $(UTIL_LIBS)

# Node Utility
$(NODE): $(NODE_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(NODE_OBJS) $(XPDEV_LIBS)

# FIXSMB Utility
$(FIXSMB): $(FIXSMB_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(FIXSMB_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS)

# CHKSMB Utility
$(CHKSMB): $(CHKSMB_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(CHKSMB_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS)

# SMB Utility
$(SMBUTIL): $(SMBUTIL_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(SMBUTIL_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS)

# SBBSecho (FidoNet Packet Tosser)
$(SBBSECHO): $(SBBSECHO_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(SBBSECHO_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS)

# SBBSecho Configuration Program
$(ECHOCFG): $(ECHOCFG_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) $(MT_LDFLAGS) -o $@ $(ECHOCFG_OBJS) $(UIFC-MT_LDFLAGS) $(SMBLIB_LIBS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)

# ADDFILES
$(ADDFILES): $(ADDFILES_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(ADDFILES_OBJS) $(XPDEV_LIBS)

# FILELIST
$(FILELIST): $(FILELIST_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(FILELIST_OBJS) $(XPDEV_LIBS)

# MAKEUSER
$(MAKEUSER): $(MAKEUSER_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(MAKEUSER_OBJS) $(XPDEV_LIBS)

# JSEXEC
$(JSEXEC): $(JSEXEC_OBJS) $(SBBS)
	@echo Linking $@
	$(QUIET)$(CXX) $(LDFLAGS) $(MT_LDFLAGS) -o $@ $(JSEXEC_OBJS) -lsbbs $(SMBLIB_LIBS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)

# ANS2ASC
$(ANS2ASC): $(OBJODIR)/ans2asc.o
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(OBJODIR)/ans2asc.o

# ASC2ANS
$(ASC2ANS): $(OBJODIR)/asc2ans.o
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(OBJODIR)/asc2ans.o

# SEXYZ
$(SEXYZ): $(SEXYZ_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) $(MT_LDFLAGS) -o $@ $(SEXYZ_OBJS) $(SMBLIB_LIBS) $(XPDEV-MT_LIBS)

# QWKNODES
$(QWKNODES): $(QWKNODES_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(QWKNODES_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS)

# SLOG
$(SLOG): $(SLOG_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(SLOG_OBJS) $(XPDEV_LIBS)

# ALLUSERS
$(ALLUSERS): $(ALLUSERS_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(ALLUSERS_OBJS) $(XPDEV_LIBS)

# DELFILES
$(DELFILES): $(DELFILES_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(DELFILES_OBJS) $(XPDEV_LIBS)

# DUPEFIND
$(DUPEFIND): $(DUPEFIND_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(DUPEFIND_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS)

# SMBACTIV
$(SMBACTIV): $(SMBACTIV_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $(SMBACTIV_OBJS) $(SMBLIB_LIBS) $(XPDEV_LIBS)
