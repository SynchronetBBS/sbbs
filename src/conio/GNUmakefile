SRC_ROOT = ..
# Cross platform/compiler definitions
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

CFLAGS += $(XPDEV-MT_CFLAGS) $(CIOLIB-MT_CFLAGS)

ifeq ($(os),win32)
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)SDL_win32_main$(OFILE)
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)win32cio$(OFILE)
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)ciolib_res$(OFILE)
else
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)curs_cio$(OFILE)
endif
ifdef NO_X
 CFLAGS	+=	-DNO_X
else
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)x_events$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)x_cio$(OFILE)
 NEED_BITMAP := 1
endif

ifdef WITH_SDL_AUDIO
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)sdl_con$(OFILE)
 ifeq ($(os),darwin)
  OBJS 	+=      $(MTOBJODIR)$(DIRSEP)pasteboard$(OFILE)
 endif
 NEED_BITMAP := 1
else
 ifdef WITH_SDL
  NEED_BITMAP := 1
  OBJS	+=	$(MTOBJODIR)$(DIRSEP)sdl_con$(OFILE)
  OBJS	+=      $(MTOBJODIR)$(DIRSEP)sdlfuncs$(OFILE)
  ifeq ($(os),darwin)
   OBJS 	+=      $(MTOBJODIR)$(DIRSEP)SDLMain$(OFILE)
   OBJS 	+=      $(MTOBJODIR)$(DIRSEP)pasteboard$(OFILE)
  endif
 endif
endif

ifeq ($(os),netbsd)
 ifndef USE_SYSTEM_CURSES
  CFLAGS	+=	-DN_CURSES_LIB
 endif
endif

ifdef NEED_BITMAP
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)bitmap_con$(OFILE)
endif

# CIOLIB Library Link Rule
$(CIOLIB-MT_BUILD): $(MTOBJODIR) $(OBJS)
	@echo Creating $@ ...
	$(QUIET)$(AR) rc $@ $(OBJS)
	$(QUIET)$(RANLIB) $@

$(CIOLIB-MT_SHLIB_BUILD): $(MTOBJODIR) $(OBJS)
	@echo Creating $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(OBJS) $(SHLIBOPTS) -o $@

ifeq ($(os),darwin)
$(MTOBJODIR)$(DIRSEP)SDLMain$(OFILE): SDLMain.m
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) $(CCFLAGS) -o $@ -c $<

$(MTOBJODIR)$(DIRSEP)pasteboard$(OFILE): pasteboard.m
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) $(CCFLAGS) -o $@ -c $<
endif

ifeq ($(os),win32)
mtlib: $(MTOBJODIR)$(DIRSEP)ciolib_res${OFILE}

dl-mtlib: $(MTOBJODIR)$(DIRSEP)ciolib_res${OFILE}

$(MTOBJODIR)$(DIRSEP)ciolib_res${OFILE}: ciolib.rc syncicon64.ico
	@echo Creating $< ...
	$(QUIET)${WINDRES} -O coff -i ciolib.rc -o $@
endif
