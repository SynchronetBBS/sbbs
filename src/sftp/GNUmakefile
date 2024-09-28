# uifc/Makefile

########################################
# Makefile for Synchronet SFTP library #
# For use with GNU make for *nix       #
########################################

# Macros
#DEBUG	=	1				# Comment out for release (non-debug) version

SRC_ROOT = ..
# Cross platform/compiler definitions
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

CFLAGS += -I$(XPDEV_SRC) $(XPDEV-MT_CFLAGS)

# MT-SFTP Library Link Rule
$(SFTPLIB-MT_BUILD): $(MT_OBJS) | $(LIBODIR)
	@echo Creating $@ ...
ifdef FAT
	$(QUIET)$(DELETE) $@
endif
	$(QUIET)$(AR) rc $@ $(MT_OBJS)
	$(QUIET)$(RANLIB) $@
