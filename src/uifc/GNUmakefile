# uifc/Makefile

#########################################################################
# Makefile for Synchronet User Interface Library (UIFC)					#
# For use with GNU make for *nix								        #
# @format.tab-size 4													#
#																		#
# usage: make															#
#########################################################################

# $Id: GNUmakefile,v 1.15 2020/08/16 20:37:07 rswindell Exp $

# Macros
#DEBUG	=	1				# Comment out for release (non-debug) version

SRC_ROOT = ..
# Cross platform/compiler definitions
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

CFLAGS += -I$(XPDEV_SRC) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)

# MT-UIFC Library Link Rule
$(UIFCLIB-MT_BUILD): $(MT_OBJS) | $(MTOBJODIR) $(LIBODIR)
	@echo Creating $@ ...
ifdef FAT
	$(QUIET)$(DELETE) $@
endif
	$(QUIET)$(AR) rc $@ $(MT_OBJS)
	$(QUIET)$(RANLIB) $@

$(UIFCTEST): $(MTOBJODIR)$(DIRSEP)uifctest$(OFILE) $(MTOBJODIR)$(DIRSEP)filepick$(OFILE) | $(EXEODIR)
	@echo Creating $@ ...
	$(QUIET)$(CC) $(MT_LDFLAGS) $(UIFC-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(LDFLAGS) -o $@ $(MTOBJODIR)$(DIRSEP)filepick$(OFILE) $(MTOBJODIR)$(DIRSEP)uifctest$(OFILE) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)
