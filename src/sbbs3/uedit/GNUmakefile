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

# $Id$

#USE_CURSES	:=	1	#Use old curses UIFC implementation
USE_UIFC32	:=	1	# Use new uifc32 implementation

XPDEV	=	../../xpdev/
SBBS_SRC	=   ../
UIFC_SRC	= ../../uifc/
NEED_UIFC	:=	1
include $(XPDEV)Common.gmake
include $(SBBS_SRC)Common.gmake
include $(UIFC_SRC)Common.gmake

FORCE$(EXEODIR)/uedit: $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)

$(EXEODIR)/uedit: $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	@$(CC) $(LDFLAGS) $(OBJS) -o $@
