# GNUmakefile

#########################################################################
# Makefile for SCFG			 											#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: gmake															#
# FreeBSD: gmake os=FreeBSD												#
#																		#
# Optional build targets: clean
#########################################################################

# $Id$

#USE_DIALOG =	1	# Dialog vesrion of UIFC
#USE_FLTK =	1	# Use Windowed version
ifndef NO_CURSES
 #USE_CURSES	=	1	# Curses version of UIFC
 USE_UIFC32	=	1	# NEW curses version of uifc
endif

UIFC_SRC	=	../../uifc
XPDEV	=	../../xpdev
SBBS_SRC =	..
NEED_UIFC	:=	1
CFLAGS		=	-DSCFG

include $(XPDEV)/Common.gmake
include $(UIFC_SRC)/Common.gmake
include $(SBBS_SRC)/Common.gmake

ifeq ($(os),netbsd)
 CFLAGS		+=	$(PTHREAD_CFLAGS)
 LDFLAGS	+=	$(PTHREAD_LDFLAGS)
endif

SCFG	:=	$(EXEODIR)$(SLASH)scfg$(EXEFILE)

$(MAKEHELP): makehelp.c
	$(QUIET)$(CC) $(CFLAGS) makehelp.c -o $(MAKEHELP)

$(SCFGHELP): $(OBJS) $(MAKEHELP)
	$(QUIET)$(MAKEHELP) $(EXEODIR)

# SCFG Build Rule
$(SCFG): $(OBJS)
   ifdef USE_DIALOG
	$(QUIET)$(MAKE) --no-print-directory -C ../../libdialog
   endif
	@echo Linking $@
	$(QUIET)$(CXX) -o $@ $(OBJS) $(LDFLAGS)
