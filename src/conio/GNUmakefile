SRC_ROOT = ..
# Cross platform/compiler definitions
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

CFLAGS += $(XPDEV-MT_CFLAGS) $(CIOLIB-MT_CFLAGS)

OBJS	+=	$(MTOBJODIR)$(DIRSEP)curs_cio$(OFILE)
ifdef NO_X
 CFLAGS	+=	-DNO_X
else
 CFLAGS	+=	-I/usr/X11R6/include
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)console$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)x_cio$(OFILE)
endif
ifeq ($(os),netbsd)
 CFLAGS	+=	-DN_CURSES_LIB
endif

# CIOLIB Library Link Rule
$(CIOLIB-MT): $(MTOBJODIR) $(OBJS)
	@echo Creating $@ ...
	$(QUIET)ar rc $@ $(OBJS)
	$(QUIET)ranlib $@
