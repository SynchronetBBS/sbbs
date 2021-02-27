# GNUmakefile

#########################################################################
# Makefile for SBBSINST		 											#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: gmake															#
# FreeBSD: gmake os=FreeBSD												#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id: GNUmakefile,v 1.23 2020/03/31 06:48:50 deuce Exp $

SRC_ROOT	= ../..
include $(SRC_ROOT)/build/Common.gmake

CFLAGS	+=	-I../ -I../../comio

ifdef STATIC
 LDFLAGS	+=	-static
endif

ifeq ($(os),sunos)
 LDFLAGS	+=	-lnsl
endif

CFLAGS	+=	$(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) $(SMBLIB_CFLAGS) $(HASH_CFLAGS)
LDFLAGS	+=	$(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(SMBLIB_LDFLAGS) $(HASH_LDFLAGS)

$(SBBSINST): $(OBJS)
	$(QUIET)echo Linking $@
	$(QUIET)$(CC) -o $@ $(MT_LDFLAGS) $(OBJS) $(LDFLAGS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS) $(HASH_LIBS)
