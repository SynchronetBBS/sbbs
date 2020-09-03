# GNUmakefile

#########################################################################
# Makefile for Synchronet monitor for Unix								#
# For use with GNU make and GNU C Compiler or Borland Kylix C++			#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# gcc: gmake															#
# Borland (still in testing/debuging stage): gmake bcc=1				#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id: GNUmakefile,v 1.33 2019/06/29 01:30:42 rswindell Exp $

SRC_ROOT	=	../..
include $(SRC_ROOT)/build/Common.gmake

ifeq ($(os),sunos)
 LDFLAGS	+=	-lnsl
endif

vpath %.c ..

CFLAGS	+=	-I.. -I../../comio $(SMBLIB_CFLAGS) $(ENCODE_CFLAGS) $(HASH_CFLAGS) $(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
LDFLAGS	+=	$(SMBLIB_LDFLAGS) $(ENCODE_LDFLAGS) $(HASH_LDFLAGS) $(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(MT_LDFLAGS)

$(UMONITOR): $(OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(LDFLAGS) $(OBJS) -o $@ $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(SMBLIB_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(XPDEV-MT_LIBS)
