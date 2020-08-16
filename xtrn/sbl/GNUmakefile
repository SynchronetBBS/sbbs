# Makefile.gnu

#########################################################################
# Makefile for Synchronet BBS List										#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: make -f Makefile.gnu											#
# Win32: make -f Makefile.gnu os=win32									#
# FreeBSD: make -f Makefile.gnu os=freebsd								#
#########################################################################

# $Id: GNUmakefile,v 1.28 2019/08/06 17:06:32 deuce Exp $

# Macros
XSDK		=	../sdk
SRC_ROOT	?=	../../src
include $(SRC_ROOT)/build/Common.gmake

ifeq ($(os),sunos)
 LDFLAGS	+=	-lnsl -lrt
endif

LDFLAGS	+=	$(XPDEV_LDFLAGS)

vpath %.c $(XSDK)

CFLAGS	+=	-I$(SRC_ROOT)$(DIRSEP)sbbs3 -I$(XSDK) -DUSE_XPDEV $(XPDEV_CFLAGS) $(SMBLIB_CFLAGS) $(HASH_CFLAGS) $(ENCODE_CFLAGS)

$(SBL): $(OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(MT_LDFLAGS) $(LDFLAGS) $^ -o $@ $(XPDEV-MT_LIBS)

$(SBL2SMB): $(OBJODIR)$(DIRSEP)sbl2smb$(OFILE) $(HASH_LIB) $(ENCODE_LIB)
	@echo Linking $@
	$(QUIET)$(CC) $(LDFLAGS) $(SMBLIB_LDFLAGS) $(HASH_LDFLAGS) $(ENCODE_LDFLAGS) $^ -o $@ $(LIBS) $(SMBLIB_LIBS) $(XPDEV_LIBS) $(HASH_LIBS) $(ENCODE_LIBS)

$(SMB2SBL): $(OBJODIR)$(DIRSEP)smb2sbl$(OFILE) $(HASH_LIB) $(ENCODE_LIB)
	@echo Linking $@
	$(QUIET)$(CC) $(LDFLAGS) $(SMBLIB_LDFLAGS) $(HASH_LDFLAGS) $(ENCODE_LDFLAGS) $^ -o $@  $(SMBLIB_LIBS) $(XPDEV_LIBS) $(HASH_LIBS) $(ENCODE_LIBS)

$(SBLPACK): $(OBJODIR)$(DIRSEP)sblpack$(OFILE)
	@echo Linking $@
	$(QUIET)$(CC) $(LDFLAGS) $(SMBLIB_LDFLAGS) $^ -o $@ $(XPDEV_LIBS)

$(SBBSLIST): $(OBJODIR)$(DIRSEP)sbbslist$(OFILE)
	@echo Linking $@
	$(QUIET)$(CC) $(LDFLAGS) $(SMBLIB_LDFLAGS) $^ -o $@ $(XPDEV_LIBS)
