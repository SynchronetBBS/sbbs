# Makefile

#########################################################################
# Makefile for Synchronet BBS Menu Editor (menuedit)
# For use with Borland C++ Builder 5+ or Borland C++ 5.5 for Win32      #
# @format.tab-size 4													#
#########################################################################

# $Id$

# Macros
#DEBUG	=	1				# Comment out for release (non-debug) version
#USE_FLTK=	1
USE_UIFC32= 1
NEED_UIFC	:=	1
XPDEV	?=	../../xpdev/
UIFC_SRC	?=	../../uifc/

include ${XPDEV}Common.gmake
include ${UIFC_SRC}Common.gmake

# MenuEdit Build Rule
$(MENUEDIT): $(OBJS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(OBJS) -o $@

clean:
	@echo Deleting $(EXEODIR)
	$(QUIET)$(DELETE) $(EXEODIR)\*
