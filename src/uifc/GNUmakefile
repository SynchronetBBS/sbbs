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

CFLAGS += -I$(XPDEV_SRC) $(CIOLIB-MT_CFLAGS)

# UIFC Library Link Rule
$(UIFCLIB): $(OBJODIR) $(OBJS)
	@echo Creating $@ ...
	$(QUIET)ar rc $@ $(OBJS)
	$(QUIET)ranlib $@

# MT-UIFC Library Link Rule
$(UIFCLIB-MT): $(MTOBJODIR) $(MT_OBJS)
	@echo Creating $@ ...
	$(QUIET)ar rc $@ $(MT_OBJS)
	$(QUIET)ranlib $@
