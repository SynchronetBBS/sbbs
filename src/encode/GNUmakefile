# $Id: GNUmakefile,v 1.3 2019/07/10 22:24:51 deuce Exp $

# Macros
#DEBUG	=	1				# Comment out for release (non-debug) version

SRC_ROOT ?= ..
# Cross platform/compiler definitions
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

CFLAGS += -I$(ENCODE_SRC) $(XPDEV_CFLAGS)

# ENCODE Library Link Rule
$(ENCODE_BUILD): $(OBJODIR) $(OBJS)
	@echo Creating $@ ...
	$(QUIET)$(AR) rc $@ $(OBJS)
	$(QUIET)$(RANLIB) $@

