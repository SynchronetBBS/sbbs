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

# $Id$

SRC_ROOT	= ../..
include $(SRC_ROOT)/build/Common.gmake

CFLAGS	+=	-I../

ifdef STATIC
 LDFLAGS	+=	-static
endif

ifeq ($(os),sunos)
 LDFLAGS	+=	-lnsl
endif

CFLAGS	+=	$(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) $(SMBLIB_CFLAGS)
LDFLAGS	+=	$(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(SMBLIB_LDFLAGS)

$(SBBSINST): $(OBJS)
	$(QUIET)echo Linking $@
	$(QUIET)$(CC) -o $@ $(MT_LDFLAGS) $(OBJS) $(LDFLAGS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)
