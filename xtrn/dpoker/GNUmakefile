# GNUmakefile

#########################################################################
# Makefile for Domain Poker												#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: make															#
# FreeBSD: gmake														#
#########################################################################

# $Id$

XSDK		=	../sdk
SRC_ROOT	?=	../../src
include $(SRC_ROOT)/build/Common.gmake

vpath %.c $(XSDK)

CFLAGS	+=	-I$(XSDK) -DUSE_XPDEV $(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)

ifeq ($(os),sunos)
 LDFLAGS	+=	-lsocket -lnsl -lrt
endif

$(DPOKER): $(OBJS)
   ifndef bcc
	@echo Linking $@
   endif
	$(QUIET)$(CC) $(MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(LDFLAGS) $^ -o $@ $(XPDEV-MT_LIBS)

$(DPCLEAN): $(DPCLEAN_OBJS)
   ifndef bcc
	@echo Linking $@
   endif
	$(QUIET)$(CC) $(MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(LDFLAGS) $^ -o $@ $(XPDEV-MT_LIBS)

$(DPCONFIG): $(DPCONFIG_OBJS)
   ifndef bcc
	@echo Linking $@
   endif
	$(QUIET)$(CC) $(MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(LDFLAGS) $^ -o $@ $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)
