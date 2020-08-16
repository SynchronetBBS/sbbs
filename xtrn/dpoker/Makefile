# Makefile

#########################################################################
# Makefile for Domain Poker												#
# For use with Borland make and Borland Compiler						#
# @format.tab-size 4, @format.use-tabs true								#
#########################################################################

# $Id: Makefile,v 1.6 2007/06/30 03:57:11 rswindell Exp $

XSDK		=	..\sdk
!ifndef SRC_ROOT
	SRC_ROOT=	..\..\src
!endif
!include $(SRC_ROOT)\build\Common.bmake

.path.c = .;$(XSDK)

CFLAGS	=	$(CFLAGS) -I$(XSDK) -DUSE_XPDEV $(XPDEV-MT_CFLAGS) $(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS)

$(DPOKER): $(OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(LDFLAGS) $** -e$@ $(XPDEV-MT_LIBS)

$(DPCONFIG): $(DPCONFIG_OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) $(MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(UIFC-MT_LDFLAGS) $(LDFLAGS) $** -e$@ $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)
