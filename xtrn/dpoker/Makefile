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

# $Id$

XSDK		=	..\sdk
SRC_ROOT	=	..\..\src
!include $(SRC_ROOT)\build\Common.bmake

.path.c = .;$(XSDK)

CFLAGS	=	$(CFLAGS) -I$(XSDK) -DUSE_XPDEV $(XPDEV_CFLAGS)

$(DPOKER): $(OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(LDFLAGS) $** -e$@ $(XPDEV-MT_LIBS)

$(DPCLEAN): $(DPCLEAN_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) $(MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(LDFLAGS) $** -e$@ $(XPDEV-MT_LIBS)
