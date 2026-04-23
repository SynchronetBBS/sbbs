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


# xp_crypt and xp_tls backend selection. XP_CRYPTO_BACKEND is OpenSSL,
# Botan, or none. "none" selects stub implementations (xp_crypt_none.c,
# xp_tls_none.c) that fail all open calls — used when WITHOUT_CRYPTO is
# set by the consumer.  SyncTERM picks the same backend as DeuceSSH when
# both are in the build; Synchronet server currently falls through to
# the default OpenSSL.
#
ifdef WITHOUT_CRYPTO
 XP_CRYPTO_BACKEND := none
 # WITHOUT_CRYPTLIB is the legacy knob name that Synchronet server
 # code still checks to suppress crypto-dependent paths.  Turning off
 # crypto entirely should imply turning off Cryptlib too, so alias it
 # forward.
 WITHOUT_CRYPTLIB := 1
 CFLAGS += -DWITHOUT_CRYPTLIB
endif
XP_CRYPTO_BACKEND ?= OpenSSL
ifeq ($(XP_CRYPTO_BACKEND),OpenSSL)
 XP_CRYPTO_BACKEND_SUFFIX := openssl
 # Pick up the system OpenSSL's include path explicitly.  On systems
 # where OpenSSL 3.0+ lives in a versioned prefix (e.g. libcrypto3-dev
 # in /usr/include/openssl3/), the default compiler search path still
 # resolves <openssl/*.h> to an older OpenSSL whose core_names.h
 # doesn't exist.  pkg-config points at the right one.
 OPENSSL_CFLAGS := $(shell pkg-config --cflags libcrypto libssl 2>/dev/null)
 CFLAGS         += $(OPENSSL_CFLAGS)
 XPDEV_CFLAGS   += $(OPENSSL_CFLAGS)
 XPDEV-MT_CFLAGS += $(OPENSSL_CFLAGS)
endif
ifeq ($(XP_CRYPTO_BACKEND),none)
 XP_CRYPTO_BACKEND_SUFFIX := none
 CFLAGS += -DWITHOUT_CRYPTO
endif
ifeq ($(XP_CRYPTO_BACKEND),Botan)
 XP_CRYPTO_BACKEND_SUFFIX := botan3
 # Botan 3 public headers require C++20 (std::span). Override the
 # tree-default -std=c++11 for just the Botan shim objects.
 # BOTAN_CXXFLAGS is resolved lazily ($(eval ...)) so it's evaluated
 # at rule-invocation time — after the botan prerequisite
 # has generated the .pc file the probe consults.
 BOTAN_CXXFLAGS = $(shell pkg-config --cflags botan-3)
 $(OBJODIR)/xp_tls_botan3$(OFILE): xp_tls_botan3.cpp botan | $(OBJODIR)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CXX) -std=c++20 $(CFLAGS) $(CXXFLAGS) $(BOTAN_CXXFLAGS) -o $@ -c $<
 $(MTOBJODIR)/xp_tls_botan3$(OFILE): xp_tls_botan3.cpp botan | $(MTOBJODIR)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CXX) -std=c++20 $(CFLAGS) $(CXXFLAGS) $(MT_CFLAGS) $(BOTAN_CXXFLAGS) -o $@ -c $<
 $(OBJODIR)/xp_crypt_botan3$(OFILE): xp_crypt_botan3.cpp botan | $(OBJODIR)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CXX) -std=c++20 $(CFLAGS) $(CXXFLAGS) $(BOTAN_CXXFLAGS) -o $@ -c $<
 $(MTOBJODIR)/xp_crypt_botan3$(OFILE): xp_crypt_botan3.cpp botan | $(MTOBJODIR)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CXX) -std=c++20 $(CFLAGS) $(CXXFLAGS) $(MT_CFLAGS) $(BOTAN_CXXFLAGS) -o $@ -c $<
endif

# ini_crypt, xp_crypt_<backend>, xp_tls_<backend>: kept out of
# objects.mk (which is shared with Common.bmake / Borland) because
# XP_CRYPTO_BACKEND_SUFFIX is a gmake-only variable and the backends
# expect a modern TLS stack (Botan C++ or OpenSSL) that an ancient
# Borland build can't parse.
OBJS   += $(OBJODIR)$(DIRSEP)xp_crypt_$(XP_CRYPTO_BACKEND_SUFFIX)$(OFILE)
OBJS   += $(OBJODIR)$(DIRSEP)xp_tls_$(XP_CRYPTO_BACKEND_SUFFIX)$(OFILE)
MTOBJS += $(MTOBJODIR)$(DIRSEP)xp_crypt_$(XP_CRYPTO_BACKEND_SUFFIX)$(OFILE)
MTOBJS += $(MTOBJODIR)$(DIRSEP)xp_tls_$(XP_CRYPTO_BACKEND_SUFFIX)$(OFILE)
ifneq ($(XP_CRYPTO_BACKEND),none)
 OBJS   += $(OBJODIR)$(DIRSEP)ini_crypt$(OFILE)
 MTOBJS += $(MTOBJODIR)$(DIRSEP)ini_crypt$(OFILE)
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
