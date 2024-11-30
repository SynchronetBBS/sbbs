SRC_ROOT = ..
# Cross platform/compiler definitions
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

CFLAGS += $(XPDEV-MT_CFLAGS) $(HASH_CFLAGS) $(ENCODE_CFLAGS) $(CIOLIB-MT_CFLAGS)
VPATH = $(SRCODIR)

ifdef win
 ifndef WITHOUT_GDI
  OBJS	+=	$(MTOBJODIR)$(DIRSEP)win32gdi$(OFILE)
  NEED_BITMAP := 1
 endif
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)SDL_win32_main$(OFILE)
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)win32cio$(OFILE)
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)ciolib_res$(OFILE)
else
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)curs_cio$(OFILE)
endif
ifndef NO_X
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)x_events$(OFILE)
 OBJS	+=      $(MTOBJODIR)$(DIRSEP)x_cio$(OFILE)
 NEED_BITMAP := 1
endif

ifdef WITH_SDL
 NEED_BITMAP := 1
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)sdl_con$(OFILE)
 OBJS	+=      $(MTOBJODIR)$(DIRSEP)sdlfuncs$(OFILE)
endif

ifeq ($(os),netbsd)
 ifndef USE_SYSTEM_CURSES
  CFLAGS	+=	-DN_CURSES_LIB
 endif
endif

ifeq ($(shell if [ -e /usr/include/ncursesw/ncurses.h ] ; then echo YES ; fi),YES)
 CFLAGS += -DDEBIAN_HATES_YOU
endif

ifdef NEED_BITMAP
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)bitmap_con$(OFILE)
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)scale$(OFILE)
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)xbr$(OFILE)
endif



# CIOLIB Library Link Rule
$(CIOLIB-MT_BUILD): $(MTOBJODIR) $(OBJS) $(CIOLIB_INTERPOLATE_TARGET)
	@echo Creating $@ ...
ifdef FAT
	$(QUIET)$(DELETE) $@
endif
	$(QUIET)$(AR) rc $@ $(OBJS)
	$(QUIET)$(RANLIB) $@

$(CIOLIB-MT_SHLIB_BUILD): $(MTOBJODIR) $(OBJS)
	@echo Creating $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(OBJS) $(SHLIBOPTS) -o $@

ifdef win
mtlib: $(MTOBJODIR)$(DIRSEP)ciolib_res${OFILE}

dl-mtlib: $(MTOBJODIR)$(DIRSEP)ciolib_res${OFILE}

$(MTOBJODIR)$(DIRSEP)ciolib_res${OFILE}: ciolib.rc syncicon64.ico ciolib.manifest
	@echo Creating $< ...
	$(QUIET)${WINDRES} $(WINDRESFLAGS) -O coff -i ciolib.rc -o $@
endif
