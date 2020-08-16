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

# $Id: GNUmakefile,v 1.4 2020/04/28 17:50:46 rswindell Exp $

SRC_ROOT	=	../..
include $(SRC_ROOT)/build/Common.gmake
include $(SRC_ROOT)/sbbs3/sbbsdefs.mk

vpath %.c ..

CFLAGS	+=	$(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) $(SMBLIB_CFLAGS)
LDFLAGS	+=	$(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(MT_LDFLAGS) $(ENCODE_LDFLAGS) $(HASH_LDFLAGS)

$(SYNCVIEW): $(OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(LDFLAGS) $(OBJS) -o $@ $(CIOLIB-MT_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(XPDEV-MT_LIBS)
