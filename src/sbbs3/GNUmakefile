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

UTIL_LDFLAGS	+=	$(SMBLIB_LDFLAGS) $(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV_LDFLAGS)
UTIL_LIBS	+=	$(SMBLIB_LIBS)

ifeq ($(os),freebsd)
 BSD	=	1
else
 ifeq ($(os),openbsd)
  BSD	=	1
 endif
endif

ifndef bcc
 LDFLAGS	+=	-lm
 ifneq ($(os),sunos)
  LDFLAGS	+=	-lutil
 endif
endif

ifeq ($(os),sunos)    # Solaris
 LDFLAGS += -lsocket -lnsl -lrt
endif

ifeq ($(os),netbsd)
 LDFLAGS += -L/usr/pkg/lib
 UTIL_LDFLAGS	+=	-lpth -L/usr/pkg/lib
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
JS_LDFLAGS	+=	-L$(NSPRDIR) -lnspr4

CFLAGS	+=	$(JS_CFLAGS)
LDFLAGS	+=	$(JS_LDFLAGS)

include sbbsdefs.mk
CFLAGS	+=	$(SBBSDEFS)

# Set up LD_RUN_PATH for run-time locating of the .so files
PWD	:=	$(shell pwd)
LD_RUN_PATH	:=	.:$(PWD)/$(LIBODIR):$(PWD)/$(JSLIBDIR):$(PWD)/$(NSPRDIR)
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
  MKSHPPLIB		:=	/usr/ccs/bin/ld -G -L/usr/local/lib -lstdc++
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
	$(QUIET)$(CXX) -o $@ $(LDFLAGS) $(MT_LDFLAGS) $^ $(SMBLIB_LIBS) $(XPDEV-MT_LIBS)

# Synchronet BBS library Link Rule
$(SBBS): $(OBJS) $(LIBS)
	@echo Linking $@
	$(QUIET)$(MKSHPPLIB) $(LDFLAGS) -o $@ $^ $(SHLIBOPTS)

# FTP Server Link Rule
$(FTPSRVR): $(MTOBJODIR)/ftpsrvr.o
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $^ $(SHLIBOPTS) -o $@

# Mail Server Link Rule
$(MAILSRVR): $(MAIL_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $^ $(SHLIBOPTS) -o $@

# Mail Server Link Rule
$(WEBSRVR): $(WEB_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $^ $(SHLIBOPTS) -o $@

# Services Link Rule
$(SERVICES): $(SERVICE_OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $^ $(SHLIBOPTS) -o $@

# Synchronet Console Build Rule
$(SBBSCON): $(CON_OBJS) $(SBBS) $(FTPSRVR) $(WEBSRVR) $(MAILSRVR) $(SERVICES)
	@echo Linking $@
	$(QUIET)$(CC) $(LDFLAGS) $(MT_LDFLAGS) -o $@ $(CON_OBJS) $(CON_LIBS) $(SMBLIB_LIBS) $(XPDEV-MT_LIBS)

# Baja Utility
$(BAJA): $(BAJA_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^ $(SMBLIB_LIBS) $(XPDEV_LIBS)

# Node Utility
$(NODE): $(NODE_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^ $(XPDEV_LIBS)

# FIXSMB Utility
$(FIXSMB): $(FIXSMB_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^ $(SMBLIB_LIBS) $(XPDEV_LIBS)

# CHKSMB Utility
$(CHKSMB): $(CHKSMB_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^ $(SMBLIB_LIBS) $(XPDEV_LIBS)

# SMB Utility
$(SMBUTIL): $(SMBUTIL_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^ $(SMBLIB_LIBS) $(XPDEV_LIBS)

# SBBSecho (FidoNet Packet Tosser)
$(SBBSECHO): $(SBBSECHO_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^ $(SMBLIB_LIBS) $(XPDEV_LIBS)

# SBBSecho Configuration Program
$(ECHOCFG): $(ECHOCFG_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) $(MT_LDFLAGS) -o $@ $^ $(UIFC-MT_LDFLAGS) $(SMBLIB_LIBS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)

# ADDFILES
$(ADDFILES): $(ADDFILES_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^ $(XPDEV_LIBS)

# FILELIST
$(FILELIST): $(FILELIST_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^ $(XPDEV_LIBS)

# MAKEUSER
$(MAKEUSER): $(MAKEUSER_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^ $(XPDEV_LIBS)

# JSEXEC
$(JSEXEC): $(JSEXEC_OBJS) $(SBBS)
	@echo Linking $@
	$(QUIET)$(CXX) $(LDFLAGS) $(MT_LDFLAGS) -o $@ $(JSEXEC_OBJS) -lsbbs $(SMBLIB_LIBS) $(XPDEV-MT_LIBS)

# ANS2ASC
$(ANS2ASC): $(OBJODIR)/ans2asc.o
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^

# ASC2ANS
$(ASC2ANS): $(OBJODIR)/asc2ans.o
	@echo Linking $@
	$(QUIET)$(CC) $(UTIL_LDFLAGS) -o $@ $^
