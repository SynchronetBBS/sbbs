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

# $Id: GNUmakefile,v 1.12 2020/04/30 21:10:55 deuce Exp $

SRC_ROOT	=	../..
include $(SRC_ROOT)/build/Common.gmake

ifeq ($(os),sunos)
 EXTRA_LIBS	+=	-lnsl
endif

vpath %.c ..

CFLAGS	+=	-I.. -I../../comio $(SMBLIB_CFLAGS) $(HASH_CFLAGS) $(ENCODE_CFLAGS) $(XPDEV-MT_CFLAGS) $(JS_CFLAGS) $(CRYPT_CFLAGS) `pkg-config gtk+-3.0 --cflags`
LDFLAGS	+=	$(SMBLIB_LDFLAGS) $(ENCODE_LDFLAGS) $(HASH_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(MT_LDFLAGS)
EXTRA_LIBS += `pkg-config gtk+-3.0 --libs`

$(GTKCHAT): $(OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(LDFLAGS) $(OBJS) -o $@ $(EXTRA_LIBS) $(SMBLIB_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(XPDEV-MT_LIBS)
