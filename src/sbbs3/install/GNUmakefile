# GNUmakefile

#########################################################################
# Makefile for SBBSINST		 											#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# Linux: gmake															#
# FreeBSD: gmake os=FreeBSD												#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id$

#USE_DIALOG =	1	# Dialog vesrion of UIFC
#USE_CURSES	=	1	# Curses version of UIFC
#USE_FLTK	=	1	# Windowed version of UIFC
USE_UIFC32	:=	1	# New 32-bit version of UIFC
UIFC	=	../../uifc/
XPDEV	=	../../xpdev/

NEED_UIFC	:=	1
include $(XPDEV)/Common.gmake
include $(UIFC)/Common.gmake

ifdef STATIC
 LDFLAGS	+=	-static
endif

$(SBBSINST): $(OBJS)
   ifdef USE_DIALOG
	$(QUIET)$(MAKE) --no-print-directory -C ../../libdialog
   endif
	$(QUIET)echo Linking $@
	$(QUIET)$(CC) -o $@ $(OBJS) $(LDFLAGS)

# Auto-dependency files (should go in output dir, but gcc v2.9.5 puts in cwd)
-include ./*.d
-include $(ODIR)/*.d
