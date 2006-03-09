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

# $Id$

SRC_ROOT	=	../..
include $(SRC_ROOT)/build/Common.gmake

ifeq ($(os),sunos)
 LDFLAGS	+=	-lnsl
endif

vpath %.c ..

CFLAGS	+=	-I.. $(SMBLIB_CFLAGS) $(XPDEV-MT_CFLAGS) `pkg-config gtk+-2.0 --cflags`
LDFLAGS	+=	$(SMBLIB_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(MT_LDFLAGS) `pkg-config gtk+-2.0 --libs`

$(GTKCHAT): $(OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(LDFLAGS) $(OBJS) -o $@ $(SMBLIB_LIBS) $(XPDEV-MT_LIBS)
