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
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

CFLAGS += -I$(XPDEV_SRC) -I$(CIOLIB_SRC)
OBJS	+=	$(OBJODIR)$(DIRSEP)curs_cio$(OFILE)
ifndef NO_X
 CFLAGS	+=	-I/usr/X11R6/include
 OBJS	+=	$(OBJODIR)$(DIRSEP)console$(OFILE) \
			$(OBJODIR)$(DIRSEP)x_cio$(OFILE)
endif

# CIOLIB Library Link Rule
$(CIOLIB-MT): $(MTOBJODIR) $(OBJS)
	@echo Creating $@ ...
	$(QUIET)ar rc $@ $(OBJS)
	$(QUIET)ranlib $@
