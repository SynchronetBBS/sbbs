# GNUmakefile

#########################################################################
# Makefile for Synchronet User Editor for Unix								#
# For use with GNU make and GNU C Compiler or Borland Kylix C++			#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# gcc: gmake															#
# Borland (still in testing/debuging stage): gmake bcc=1				#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id: GNUmakefile,v 1.17 2019/08/24 19:52:01 rswindell Exp $

SRC_ROOT	=	../..
include $(SRC_ROOT)/build/Common.gmake

ifeq ($(os),sunos)
 LDFLAGS	+=	-lnsl
endif

vpath %.c ..

CFLAGS	+=	-DHAS_CURSES -I.. -I../../comio $(SMBLIB_CFLAGS) $(HASH_CFLAGS) $(ENCODE_CFLAGS) $(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
LDFLAGS	+=	$(SMBLIB_LDFLAGS) $(ENCODE_LDFLAGS) $(HASH_LDFLAGS) $(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(MT_LDFLAGS)

$(UEDIT): $(OBJS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(OBJS) -o $@ $(SMBLIB_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)
