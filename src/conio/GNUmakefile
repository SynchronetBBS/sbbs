SRC_ROOT = ..
# Cross platform/compiler definitions
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

CFLAGS += $(XPDEV-MT_CFLAGS) $(CIOLIB-MT_CFLAGS)

OBJS	+=	$(MTOBJODIR)$(DIRSEP)curs_cio$(OFILE)
ifdef NO_X
 CFLAGS	+=	-DNO_X
else
 CFLAGS	+=	-I$(X_HEADERS)
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)console$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)x_cio$(OFILE)
endif

ifdef WITH_SDL
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)sdl_con$(OFILE)
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)sdlfuncs$(OFILE)
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)allfonts$(OFILE)
else
 ifndef NO_X
  OBJS	+=	$(MTOBJODIR)$(DIRSEP)allfonts$(OFILE)
 endif
endif

ifeq ($(os),netbsd)
 CFLAGS	+=	-DN_CURSES_LIB
endif

ifeq ($(os),darwin)
 ifdef WITH_SDL
  OBJS	+=	$(MTOBJODIR)$(DIRSEP)SDLMain$(OFILE)
 endif
endif

ifeq ($(os),darwin)
$(MTOBJODIR)$(DIRSEP)SDLMain$(OFILE): SDLMain.m
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) $(CCFLAGS) -o $@ -c $<
endif

$(MTOBJODIR)$(DIRSEP)console$(OFILE).static:
	$(QUIET)$(DELETE) $(MTOBJODIR)$(DIRSEP)console$(OFILE)*
	$(QUIET)touch $(MTOBJODIR)$(DIRSEP)console$(OFILE).static

$(MTOBJODIR)$(DIRSEP)console$(OFILE).dynamic:
	$(QUIET)$(DELETE) $(MTOBJODIR)$(DIRSEP)console$(OFILE)*
	$(QUIET)touch $(MTOBJODIR)$(DIRSEP)console$(OFILE).dynamic

# CIOLIB Library Link Rule
ifdef STATIC
$(CIOLIB-MT_BUILD): $(MTOBJODIR)$(DIRSEP)console$(OFILE).static $(MTOBJODIR) $(OBJS)
else
$(CIOLIB-MT_BUILD): $(MTOBJODIR)$(DIRSEP)console$(OFILE).dynamic $(MTOBJODIR) $(OBJS)
endif
	@echo Creating $@ ...
	$(QUIET)$(AR) rc $@ $(OBJS)
	$(QUIET)$(RANLIB) $@

ifdef STATIC
$(CIOLIB-MT_SHLIB_BUILD): $(MTOBJODIR)$(DIRSEP)console$(OFILE).static $(MTOBJODIR) $(OBJS)
else
$(CIOLIB-MT_SHLIB_BUILD): $(MTOBJODIR)$(DIRSEP)console$(OFILE).dynamic $(MTOBJODIR) $(OBJS)
endif
	@echo Creating $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(OBJS) $(SHLIBOPTS) -o $@
