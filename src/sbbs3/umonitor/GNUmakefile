# GNUmakefile

#########################################################################
# Makefile for Synchronet monitor for Unix								#
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

UIFC	=	../../uifc/
XPDEV	=	../../xpdev/
SBBS_SRC	=   ../

NEED_UIFC	:=	1
NEED_CTERM	:=	1

include $(XPDEV)Common.gmake
include $(SBBS_SRC)Common.gmake
include $(UIFC)Common.gmake

ifeq ($(os),sunos)
 LDFLAGS	+=	-lsocket -lnsl
endif

ifeq ($(os),netbsd)
 CFLAGS		+=	$(PTHREAD_CFLAGS)
 LDFLAGS	+=	$(PTHREAD_LDFLAGS)
endif

FORCE$(EXEODIR)/umonitor: $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)

$(EXEODIR)/umonitor: $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	@$(CC) $(LDFLAGS) $(OBJS) -o $@

-include $(LIBODIR)/.depend
-include $(EXEODIR)/.depend
-include $(LIBODIR)/*.d
-include $(EXEODIR)/*.d
