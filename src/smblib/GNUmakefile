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

CFLAGS += -I$(XPDEV_SRC)

lib:	$(LIBODIR) $(SMBLIB)
mtlib:	$(LIBODIR) $(SMBLIB-MT)

# SMBLIB Library Link Rule
$(SMBLIB): $(OBJODIR) $(OBJS)
	@echo Creating $@ ...
	$(QUIET)ar rc $@ $(OBJS)
	$(QUIET)ranlib $@

# SMBLIB Library Link Rule
$(SMBLIB-MT): $(MTOBJODIR) $(MTOBJS)
	@echo Creating $@ ...
	$(QUIET)ar rc $@ $(MTOBJS)
	$(QUIET)ranlib $@

