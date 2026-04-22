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

# xp_crypt and xp_tls backend selection. XP_CRYPTO_BACKEND is OpenSSL
# or Botan, matching the DeuceSSH (src/ssh/) crypto-backend choice so
# both halves of SyncTERM's SSH/TLS stack share one libcrypto or one
# libbotan-3. Defaults to OpenSSL.
XP_CRYPTO_BACKEND ?= OpenSSL
ifeq ($(XP_CRYPTO_BACKEND),OpenSSL)
 XP_CRYPTO_BACKEND_SUFFIX := openssl
endif
ifeq ($(XP_CRYPTO_BACKEND),Botan)
 XP_CRYPTO_BACKEND_SUFFIX := botan3
 # Botan 3 public headers require C++20 (std::span). Override the
 # tree-default -std=c++11 for just the Botan shim objects.
 BOTAN_CXXFLAGS := $(shell pkg-config --cflags botan-3)
 $(OBJODIR)/xp_tls_botan3$(OFILE): xp_tls_botan3.cpp | $(OBJODIR)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CXX) -std=c++20 $(CFLAGS) $(CXXFLAGS) $(BOTAN_CXXFLAGS) -o $@ -c $<
 $(MTOBJODIR)/xp_tls_botan3$(OFILE): xp_tls_botan3.cpp | $(MTOBJODIR)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CXX) -std=c++20 $(CFLAGS) $(CXXFLAGS) $(MT_CFLAGS) $(BOTAN_CXXFLAGS) -o $@ -c $<
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

$(XPDEV_LIB_BUILD): $(DEPS) $(OBJS) | $(OBJODIR)
	@echo Creating $@
ifdef FAT
	$(QUIET)$(DELETE) $@
endif
	$(QUIET)$(AR) rc $@ $(OBJS)
	$(QUIET)$(RANLIB) $@

$(XPDEV_SHLIB_BUILD): $(DEPS) $(OBJS) | $(OBJODIR)
	@echo Creating $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(OBJS) $(SHLIBOPTS) -o $@

$(XPDEV-MT_LIB_BUILD): $(DEPS) $(MTOBJS) | $(MTOBJODIR)
	@echo Creating $@
ifdef FAT
	$(QUIET)$(DELETE) $@
endif
	$(QUIET)$(AR) rc $@ $(MTOBJS)
	$(QUIET)$(RANLIB) $@

$(XPDEV-MT_SHLIB_BUILD): $(DEPS) $(MTOBJS) | $(MTOBJODIR)
	@echo Creating $@
	$(QUIET)$(MKSHLIB) $(LDFLAGS) $(MTOBJS) $(SHLIBOPTS) -o $@

xpprintf_test: xpprintf.c $(DEPS)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) $(MT_CFLAGS) -DXP_PRINTF_TEST $< -o $@ $(LDFLAGS) $(MT_LDFLAGS) $(XPDEV-MT_LIB_BUILD) -lm $(XPDEV-MT_LIBS)
