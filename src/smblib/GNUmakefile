# smblib/Makefile

#########################################################################
# Makefile for Synchronet Message Base Library (SMBLIB)					#
# GNU make                                                              #
# @format.tab-size 4													#
#																		#
# usage: make															#
#########################################################################

# $Id: GNUmakefile,v 1.8 2019/06/29 01:30:55 rswindell Exp $

# Macros
#DEBUG	=	1				# Comment out for release (non-debug) version

SRC_ROOT = ..
# Cross platform/compiler definitions
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

CFLAGS += -I$(XPDEV_SRC) $(ENCODE_CFLAGS) $(HASH_CFLAGS)


# SMBLIB Library Link Rule
$(SMBLIB_BUILD): $(OBJODIR) $(OBJS)
	@echo Creating $@ ...
	$(QUIET)$(AR) rc $@ $(OBJS)
	$(QUIET)$(RANLIB) $@

