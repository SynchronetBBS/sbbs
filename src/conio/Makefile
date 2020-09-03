# conio/Makefile

#########################################################################
# Makefile for Synchronet Message Base Library (SMBLIB)					#
# For use with Borland C++ Builder 5+ or Borland C++ 5.5 for Win32      #
# @format.tab-size 4													#
#																		#
# usage: make															#
#########################################################################

# $Id: Makefile,v 1.24 2020/04/30 01:57:06 deuce Exp $

# To build a DLL, use "make [OPTIONS] DLLIBS=1 dl-mtlib"
# You *MUST* do a clean between a static and dynamic lib on Win32!

# Macros
#DEBUG	=	1				# Comment out for release (non-debug) version

SRC_ROOT = ..
# Cross platform/compiler definitions
!include ..\build\Common.bmake	# defines clean and output directory rules

!ifdef USE_SDL
OBJS		= $(OBJS) $(MTOBJODIR)$(DIRSEP)sdl_con$(OFILE)
OBJS		= $(OBJS) $(MTOBJODIR)$(DIRSEP)SDL_win32_main$(OFILE)
OBJS		= $(OBJS) $(MTOBJODIR)$(DIRSEP)sdlfuncs$(OFILE)
NEED_BITMAP	= 1
!endif

!ifdef NEED_BITMAP
OBJS		= $(OBJS) $(MTOBJODIR)$(DIRSEP)bitmap_con$(OFILE)
!endif

#The following is necessary only when DLL-exporting wrapper functions
!ifdef DLLIBS
CFLAGS  =       $(CFLAGS) -DCIOLIB_EXPORTS=1 -DWRAPPER_IMPORTS=1
LDFLAGS =	$(LDFLAGS) -L$(LIBODIR)
!endif

CFLAGS = -w-par -w-csu $(CFLAGS) $(XPDEV-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(MT_CFLAGS) -I$(CIOLIB_SRC)
OBJS = $(OBJS) $(MTOBJODIR)$(DIRSEP)win32cio$(OFILE)

mtlib: $(LIBODIR)$(DIRSEP)ciolib.res

dl-mtlib: $(LIBODIR)$(DIRSEP)ciolib.res

$(LIBODIR)$(DIRSEP)ciolib.res: ciolib.rc syncicon64.ico
	@echo Creating $< ...
	$(QUIET)brcc32 -fo$@ -32 ciolib.rc

$(CIOLIB-MT_BUILD): $(OBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
	&$(QUIET)tlib $@ +$**

$(CIOLIB-MT_SHLIB_BUILD): $(OBJS)
	@echo Linking $@
	$(QUIET)$(MKSHLIB) $(MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) -lGi -e$@ $(LDFLAGS) $(SHLIBOPTS) $(XPDEV-MT_LIBS) @&&|
	$**
	cw32mt.lib
|

test:
	$(QUIET)$(CC) $(CFLAGS) $(MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) keytest.c $(LDFLAGS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)
