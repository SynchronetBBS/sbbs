# $Id: GNUmakefile,v 1.2 2019/07/10 22:24:28 deuce Exp $

# Macros
#DEBUG	=	1				# Comment out for release (non-debug) version

SRC_ROOT ?= ..
# Cross platform/compiler definitions
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

CFLAGS += -I$(HASH_SRC) $(XPDEV_CFLAGS)

# HASH Library Link Rule
$(HASH_BUILD): $(OBJODIR) $(OBJS)
	@echo Creating $@ ...
	$(QUIET)$(AR) rc $@ $(OBJS)
	$(QUIET)$(RANLIB) $@

