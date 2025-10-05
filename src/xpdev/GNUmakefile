SRC_ROOT	?=	..
include $(SRC_ROOT)/build/Common.gmake

ifdef XP_SEM
	MTOBJS	+=	$(MTOBJODIR)$(DIRSEP)xpsem$(OFILE)
endif
ifndef win
 MTOBJS	+=	$(MTOBJODIR)$(DIRSEP)xpevent$(OFILE)
endif

CFLAGS +=  -I. $(XPDEV_CFLAGS)
MT_CFLAGS	+=	$(XPDEV-MT_CFLAGS)

ifdef WITH_SDL_AUDIO
 MTOBJS	+=	$(MTOBJODIR)$(DIRSEP)sdlfuncs$(OFILE)
 OBJS	+=	$(OBJODIR)$(DIRSEP)sdlfuncs$(OFILE)
endif

ifdef WITHOUT_CRYPTLIB
 CFLAGS += -DWITHOUT_CRYPTLIB
else
 DEPS += $(CRYPT_LIB)
endif

# Executable Build Rule
$(WRAPTEST): $(MTOBJODIR)/wraptest.o $(DEPS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $(LDFLAGS) $(MT_LDFLAGS) $^ $(XPDEV-MT_LIB_BUILD) -lm $(XPDEV-MT_LIBS)

$(SOPENFILE): $(MTOBJODIR)/sopenfile.o $(DEPS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $(LDFLAGS) $(MT_LDFLAGS) $^ $(XPDEV-MT_LIB_BUILD) -lm $(XPDEV-MT_LIBS)

$(LOCKFILE): $(OBJODIR)/lockfile.o $(DEPS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $(LDFLAGS) $^ $(XPDEV_LIB_BUILD) -lm $(XPDEV-LIBS)

$(SHOWLOCKS): $(OBJODIR)/showlocks.o $(DEPS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) -o $@ $(LDFLAGS) $^ $(XPDEV_LIB_BUILD) -lm $(XPDEV-LIBS)

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

xpprintf_test: xpprintf.c $(DEPS)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) $(MT_CFLAGS) -DXP_PRINTF_TEST $< -o $@ $(LDFLAGS) $(MT_LDFLAGS) $(XPDEV-MT_LIB_BUILD) -lm $(XPDEV-MT_LIBS)
