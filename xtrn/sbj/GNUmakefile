# Makefile.gnu

#########################################################################
# Makefile for Synchronet Blackjack										#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: make -f Makefile.gnu											#
# Win32: make -f Makefile.gnu os=win32									#
# FreeBSD: make -f Makefile.gnu os=freebsd								#
#########################################################################

# $Id: GNUmakefile,v 1.19 2006/01/11 02:18:57 deuce Exp $

XSDK		=	../sdk
SRC_ROOT	?=	../../src
include $(SRC_ROOT)/build/Common.gmake

vpath %.c $(XSDK)

CFLAGS	+=	-I$(XSDK) -DUSE_XPDEV $(XPDEV-MT_CFLAGS)

ifeq ($(os),sunos)
 LDFLAGS	+=	-lnsl -lrt
endif

$(SBJ): $(OBJS)
   ifndef bcc
	@echo Linking $@
   endif
	$(QUIET)$(CC) $(MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(LDFLAGS) $^ -o $@ $(XPDEV-MT_LIBS)

$(SBJCLEAN): sbjclean.c
   ifndef bcc
	@echo Linking $@
   endif
	$(QUIET)$(CC) $(CFLAGS) $(XPDEV_LDFLAGS) $(LDFLAGS) $^ -o $@ $(XPDEV_LIBS)
