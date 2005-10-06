# smblib/Makefile

#########################################################################
# Makefile for Synchronet Message Base Library (SMBLIB)					#
# For use with Borland C++ Builder 5+ or Borland C++ 5.5 for Win32      #
# @format.tab-size 4													#
#																		#
# usage: make															#
#########################################################################

# $Id$

# Macros
#DEBUG	=	1				# Comment out for release (non-debug) version

SRC_ROOT = ..
# Cross platform/compiler definitions
!include ..\build\Common.bmake	# defines clean and output directory rules

!ifdef USE_SDL
OBJS		= $(OBJS) $(MTOBJODIR)$(DIRSEP)sdl_con$(OFILE) $(MTOBJODIR)$(DIRSEP)SDL_win32_main$(OFILE)
!endif

CFLAGS = -w-par -w-csu $(CFLAGS) $(XPDEV-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(MT_CFLAGS) -I$(CIOLIB_SRC)
OBJS = $(OBJS) $(MTOBJODIR)$(DIRSEP)win32cio$(OFILE)

mtlib: $(LIBODIR)$(DIRSEP)ciolib.res

$(LIBODIR)$(DIRSEP)ciolib.res: ciolib.rc syncicon64.ico
	@echo Creating $< ...
	$(QUIET)brcc32 -fo$@ -32 ciolib.rc

# SBBS DLL Link Rule
$(CIOLIB-MT_BUILD): $(OBJS)
	@echo Creating $< ...
	-$(QUIET)$(DELETE) $@
	&$(QUIET)tlib $@ +$**
