SRC_ROOT	?=	..
include $(SRC_ROOT)/build/Common.gmake

ifdef XP_SEM
	MTOBJS	+=	$(MTOBJODIR)$(DIRSEP)xpsem$(OFILE)
endif
ifndef win
 MTOBJS	+=	$(MTOBJODIR)$(DIRSEP)xpevent$(OFILE)
endif

CFLAGS +=  -I. $(XPDEV_CFLAGS)
ifndef WITHOUT_ALSA_SOUND
	ifeq ($(shell if [ -f /usr/include/alsa/asoundlib.h ] ; then echo YES ; fi),YES)
		CFLAGS	+=	-DUSE_ALSA_SOUND
	endif
endif
MT_CFLAGS	+=	$(XPDEV-MT_CFLAGS)

ifdef WITH_SDL_AUDIO
 MTOBJS	+=	$(MTOBJODIR)$(DIRSEP)sdlfuncs$(OFILE)
 OBJS	+=	$(OBJODIR)$(DIRSEP)sdlfuncs$(OFILE)
endif

# Executable Build Rule
$(WRAPTEST): $(MTOBJODIR)/wraptest.o $(DEPS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $(LDFLAGS) $(MT_LDFLAGS) $^ $(XPDEV-MT_LIB_BUILD) -lm $(XPDEV-MT_LIBS)

$(XPTIME): $(OBJODIR)/xptime.o $(XPDEV_LIB_BUILD)
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $(LDFLAGS) $^ $(XPDEV_LIB_BUILD) -lm

$(XPDEV_LIB_BUILD): $(OBJS) | $(OBJODIR)
	@echo Creating $@
ifdef FAT
	$(QUIET)$(DELETE) $@
endif
	$(QUIET)$(AR) rc $@ $(OBJS)
	$(QUIET)$(RANLIB) $@

$(XPDEV_SHLIB_BUILD): $(OBJS) | $(OBJODIR)
	@echo Creating $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(OBJS) $(SHLIBOPTS) -o $@

$(XPDEV-MT_LIB_BUILD): $(MTOBJS) | $(MTOBJODIR)
	@echo Creating $@
ifdef FAT
	$(QUIET)$(DELETE) $@
endif
	$(QUIET)$(AR) rc $@ $(MTOBJS)
	$(QUIET)$(RANLIB) $@

$(XPDEV-MT_SHLIB_BUILD): $(MTOBJS) | $(MTOBJODIR)
	@echo Creating $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(MTOBJS) $(SHLIBOPTS) -o $@

