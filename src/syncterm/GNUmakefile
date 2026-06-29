SRC_ROOT	:=	..
WINVER ?= 0xA00
WINNT ?= 0xA00
WIN32_IE ?= 0xA00
NTDDI ?= 0x0A000006
include ${SRC_ROOT}/build/Common.gmake

ifdef NEED_BITMAP
 OBJS += $(CIOLIB_INTERPOLATE_OBJS)
endif

ifdef WITHOUT_OOII
 CFLAGS	+= -DWITHOUT_OOII=1
else
 OBJS += $(MTOBJODIR)$(DIRSEP)ooii$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)ooii_logons$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)ooii_cmenus$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)ooii_bmenus$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)ooii_sounds$(OFILE)
endif

ifeq ($(os),sunos)    # Solaris
 LDFLAGS += -lnsl -lrt -lcurses
 CFLAGS	+=	-DNEED_CFMAKERAW
endif

# SSH (DeuceSSH) and crypto (TLS + encrypted-INI via xpdev's
# xp_tls / xp_crypt, backed by OpenSSL or Botan 3) are independently
# optional; the default is both enabled.
#
#   WITHOUT_DEUCESSH — drops ssh.c, CONN_TYPE_SSH / _SSHNA, the SFTP
#                      pubkey-upload bridge, and the DeuceSSH sub-build.
#
#   WITHOUT_CRYPTO   — drops telnets.c, CONN_TYPE_TELNETS, the encrypted
#                      bbslist.ini menu + dispatch, and the bundled
#                      decrypt-only reference ciphers in legacy_ciphers/.
#                      webget.c still compiles; its HTTPS paths no-op.

# Two independent decisions: which xp_crypt shim xpdev compiles
# (XP_CRYPTO_BACKEND) and which crypto runtime we link against at
# program-link time.  They usually match, but DeuceSSH needs its
# backend at link time even when WITHOUT_CRYPTO pins xpdev to the
# "none" shim.  NEED_OPENSSL / NEED_BOTAN are set by each side that
# needs those libs; ifdef at the bottom pulls them in exactly once.

# XP_CRYPTO_BACKEND, DEUCESSH_BACKEND, USE_VENDORED_BOTAN are resolved
# once in build/Common.gmake.  Keeping the decision there ensures every
# consumer of the same source tree (xpdev, syncterm, and Synchronet
# server jobs in the same pipeline) arrives at identical values — a
# cached libxpdev_mt.a stays compatible with its downstream linkers.
# build/botan.gmake (included from Common.gmake) also handles the
# PKG_CONFIG_PATH prepend + botan target.

ifndef WITHOUT_DEUCESSH
 # DeuceSSH ships only a CMake build system.  Build into a per-target
 # directory under $(3RDPODIR) so native and cross-compile builds don't
 # share a CMakeCache.txt (cmake refuses to change toolchain after the
 # first configure).  Matches the cryptlib / mozjs / botan install
 # layout.  The build step is a no-op when libdeucessh.a is already
 # current.
 DEUCESSH_DIR    := $(SRC_ROOT)/ssh
 DEUCESSH_BUILD  := $(3RDPODIR)/deucessh-build
 DEUCESSH_LIB    := $(DEUCESSH_BUILD)/libdeucessh.a
 DEUCESSH_PC     := $(DEUCESSH_BUILD)/deucessh.pc

 # DEUCESSH_BACKEND (and XP_CRYPTO_BACKEND) are decided in Common.gmake
 # so every consumer of the same source tree agrees.  Pin it into
 # DeuceSSH's cmake configure so it doesn't auto-detect something
 # different (e.g. preferring system OpenSSL when vendored Botan
 # hasn't been built yet at parse time).
 DEUCESSH_CMAKE_BACKEND_ARG := -DDEUCESSH_CRYPTO_BACKEND=$(DEUCESSH_BACKEND)

 # For Win32/Win64 cross-compile, point cmake at the MinGW toolchain
 # and tell it we're cross-compiling to Windows.  cmake caches toolchain
 # decisions, so $(DEUCESSH_BUILD) must be wiped whenever the compiler
 # changes — the deucessh rule below handles that.
 ifdef win
  # Split ccache out of $(CC)/$(CXX) into CMAKE_*_COMPILER_LAUNCHER —
  # cmake wants a single-binary CMAKE_*_COMPILER and treats a two-
  # word value ("ccache /path/to/gcc") as a compiler path + stray arg
  # ("Ignoring extra path from command line" warning, then the test
  # compile runs ccache with no compiler and fails).
  DEUCESSH_CC_WORDS  := $(CC)
  DEUCESSH_CXX_WORDS := $(CXX)
  ifeq ($(firstword $(DEUCESSH_CC_WORDS)),ccache)
   DEUCESSH_CMAKE_LAUNCHER_ARGS := \
       -DCMAKE_C_COMPILER_LAUNCHER=ccache \
       -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
   DEUCESSH_CC_BIN  := $(wordlist 2,$(words $(DEUCESSH_CC_WORDS)),$(DEUCESSH_CC_WORDS))
   DEUCESSH_CXX_BIN := $(wordlist 2,$(words $(DEUCESSH_CXX_WORDS)),$(DEUCESSH_CXX_WORDS))
  else
   DEUCESSH_CMAKE_LAUNCHER_ARGS :=
   DEUCESSH_CC_BIN  := $(CC)
   DEUCESSH_CXX_BIN := $(CXX)
  endif
  DEUCESSH_CMAKE_TOOLCHAIN_ARGS := \
      -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_C_COMPILER=$(DEUCESSH_CC_BIN) \
      -DCMAKE_CXX_COMPILER=$(DEUCESSH_CXX_BIN) \
      $(DEUCESSH_CMAKE_LAUNCHER_ARGS) \
      -DCMAKE_AR=$(shell command -v $(AR)) \
      -DCMAKE_RANLIB=$(shell command -v $(RANLIB)) \
      -DCMAKE_RC_COMPILER=$(shell command -v $(WINDRES))
 else ifeq ($(os),darwin)
  # FAT cascades to SyncTERM's CFLAGS/LDFLAGS via Common.gmake but
  # DeuceSSH is a CMake subproject and doesn't see those — set
  # CMAKE_OSX_ARCHITECTURES so its libraries match the SyncTERM
  # binary's slice list.  MACOS_PORTABLE adds per-slice microarch
  # floors (-Xarch_<arch> ...) plus the deployment target so
  # DeuceSSH's archive matches SyncTERM's redistributable floor.
  DEUCESSH_CMAKE_TOOLCHAIN_ARGS :=
  ifdef FAT
   # Quote the value: the recipe's shell would otherwise split on the
   # `;` in x86_64;arm64 and treat everything after as a separate
   # command, dropping the rest of the cmake args (deployment-target,
   # CXX_FLAGS, ...) on the floor.
   DEUCESSH_CMAKE_TOOLCHAIN_ARGS += "-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64"
   ifdef MACOS_PORTABLE
    DSSH_FAT_FLAGS := -Xarch_x86_64 -march=skylake -Xarch_arm64 -mcpu=apple-m1
    DEUCESSH_CMAKE_TOOLCHAIN_ARGS += "-DCMAKE_C_FLAGS=$(DSSH_FAT_FLAGS)" \
                                     "-DCMAKE_CXX_FLAGS=$(DSSH_FAT_FLAGS)"
   endif
  else ifdef MACOS_PORTABLE
   ifeq ($(machine_uname),x86_64)
    DEUCESSH_CMAKE_TOOLCHAIN_ARGS += -DCMAKE_C_FLAGS=-march=skylake \
                                     -DCMAKE_CXX_FLAGS=-march=skylake
   else ifeq ($(machine_uname),arm64)
    DEUCESSH_CMAKE_TOOLCHAIN_ARGS += -DCMAKE_C_FLAGS=-mcpu=apple-m1 \
                                     -DCMAKE_CXX_FLAGS=-mcpu=apple-m1
   endif
  endif
  ifdef MIN_MAC_OSX_VERSION
   DEUCESSH_CMAKE_TOOLCHAIN_ARGS += -DCMAKE_OSX_DEPLOYMENT_TARGET=$(MIN_MAC_OSX_VERSION)
  endif
 else
  DEUCESSH_CMAKE_TOOLCHAIN_ARGS :=
 endif

 ifeq ($(DEUCESSH_BACKEND),Botan)
  NEED_BOTAN := 1
 else ifeq ($(DEUCESSH_BACKEND),OpenSSL)
  NEED_OPENSSL := 1
 endif

 # Probe + configure DeuceSSH at parse time.  DeuceSSH requires C17;
 # platforms whose compiler can't advertise C17 to cmake (notably
 # AppleClang 10 on macOS 10.13 — see 7d7092472b) fail cmake configure.
 # Detect by checking that BOTH deucessh.pc (configure_file ran) AND a
 # build-system file (Makefile / build.ninja — generation step ran)
 # exist; either one alone means a half-finished configure that would
 # leave `cmake --build` looking for a Makefile that isn't there.
 # Flip to WITHOUT_DEUCESSH so the rest of syncterm still builds.
 #
 # Only probe once some suitable crypto (Botan 3 or OpenSSL 3) is
 # already on disk — DeuceSSH's configure runs pkg_check_modules for
 # it, and a missing crypto backend would look identical to a C17
 # failure.  CI extracts botan.tgz before this file parses; fresh
 # local checkouts with no system crypto let the normal build-order
 # (botan → deucessh → syncterm) run unprobed.
 # Pass PKG_CONFIG_PATH on the command line rather than relying on the
 # `export` from build/botan.gmake — some gmake versions (observed on
 # Ubuntu) don't propagate `export VAR := value` from an included file
 # into $(shell ...) calls in the same parse phase.  $(PKG_CONFIG_PATH)
 # is still set as a make variable, so we can re-emit it explicitly.
 DSSH_CRYPTO_READY := $(shell export PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)"; (pkg-config --atleast-version=3.0 botan-3 2>/dev/null || pkg-config --atleast-version=3.0 libcrypto 2>/dev/null) && echo YES)
 # Sentinel that proves configure ran AND generation produced a
 # buildable tree.  Use `test -f` via $(shell) rather than $(wildcard)
 # — wildcard's expansion is cached across a single make parse phase,
 # so the first call (before cmake runs) would poison the second call.
 DSSH_BUILD_READY = $(shell test -f $(DEUCESSH_PC) \
                          && (test -f $(DEUCESSH_BUILD)/Makefile \
                              || test -f $(DEUCESSH_BUILD)/build.ninja) \
                          && echo YES)
 ifeq ($(DSSH_CRYPTO_READY),YES)
  DSSH_READY := $(DSSH_BUILD_READY)
  ifneq ($(DSSH_READY),YES)
   $(info Configuring DeuceSSH...)
   # mkdir the build-log's parent dir first — on a fresh build,
   # $(3RDPODIR) doesn't exist yet and the >log redirect would fail
   # before cmake could even run, causing a false-negative probe.
   # PKG_CONFIG_PATH explicitly re-exported here for the same reason
   # the DSSH_CRYPTO_READY probe above does it: $(shell ...) doesn't
   # honour the make-level `export` set by build/botan.gmake.  cmake's
   # pkg_check_modules(botan-3) needs to see the vendored copy.
   DSSH_CFG_RC := $(shell mkdir -p $(dir $(DEUCESSH_BUILD)) && \
       export PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" && \
       cmake -S $(DEUCESSH_DIR) -B $(DEUCESSH_BUILD) \
       -DCMAKE_BUILD_TYPE=Release $(DEUCESSH_CMAKE_BACKEND_ARG) \
       $(DEUCESSH_CMAKE_TOOLCHAIN_ARGS) >$(DEUCESSH_BUILD).log 2>&1; echo $$?)
   DSSH_READY := $(DSSH_BUILD_READY)
   ifneq ($(DSSH_READY),YES)
    WITHOUT_DEUCESSH := 1
    $(info DeuceSSH configure failed — disabling SSH support.  cmake output:)
    $(info ----- $(DEUCESSH_BUILD).log -----)
    $(info $(shell cat $(DEUCESSH_BUILD).log 2>/dev/null))
    $(info -----)
   endif
  endif
 endif
endif

ifndef WITHOUT_DEUCESSH
 # ssh.c + SFTP-MT bridge — pulled in only if the probe above decided
 # DeuceSSH is buildable.  Doing this after the probe keeps WITHOUT_DEUCESSH
 # honest: the probe may set it, and then nothing references dssh_* or
 # ssh.o at all.
 OBJS       += $(MTOBJODIR)$(DIRSEP)ssh$(OFILE)
 CFLAGS     += -I$(SRC_ROOT)/ssh $(SFTP-MT_CFLAGS)
 LDFLAGS    += $(SFTP-MT_LDFLAGS)
 EXTRA_LIBS += $(SFTP-MT_LIBS) $(DEUCESSH_LIB)
 DEPS       += $(DEUCESSH_LIB)

 # Build step — configure normally ran at parse time, but the probe at
 # line 127 skips configure when no system crypto is on disk (intent was
 # for the build-order's `botan` recipe to provide vendored Botan first,
 # then leave the recipe to handle it).  Bootstrap configure here if the
 # parse-time probe didn't, so the unprobed path actually works.
 .PHONY: deucessh
 deucessh $(DEUCESSH_LIB):
	@echo "Building DeuceSSH ($(DEUCESSH_BACKEND) backend)"
	@if [ ! -f $(DEUCESSH_PC) ] \
	    || { [ ! -f $(DEUCESSH_BUILD)/Makefile ] \
	         && [ ! -f $(DEUCESSH_BUILD)/build.ninja ]; }; then \
	    echo "Configuring DeuceSSH..."; \
	    PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" \
	    cmake -S $(DEUCESSH_DIR) -B $(DEUCESSH_BUILD) \
	        -DCMAKE_BUILD_TYPE=Release $(DEUCESSH_CMAKE_BACKEND_ARG) \
	        $(DEUCESSH_CMAKE_TOOLCHAIN_ARGS) || exit 1; \
	fi
	cmake --build $(DEUCESSH_BUILD) --target deucessh_static
else
 CFLAGS += -DWITHOUT_DEUCESSH
 # targets.mk captured "deucessh" as a prereq of `all:` before our
 # probe ran (include ordering).  Provide a no-op rule so make doesn't
 # fail with "No rule to make target 'deucessh'" when we've disabled it.
 .PHONY: deucessh
 deucessh:
	@true
endif

# `botan` delegation target lives in build/botan.gmake; targets.mk
# wires it in as a prerequisite ahead of xpdev-mt and deucessh.

# XP_CRYPTO_BACKEND is decided in Common.gmake (shared with xpdev so
# the cached libxpdev_mt.a agrees with this build's link line).
# Just set the NEED_* flags + link-time macros from the chosen value.
ifdef WITHOUT_CRYPTO
 CFLAGS += -DWITHOUT_CRYPTO
endif
ifeq ($(XP_CRYPTO_BACKEND),OpenSSL)
 NEED_OPENSSL := 1
 CFLAGS       += -DXP_CRYPTO_BACKEND_OPENSSL
else ifeq ($(XP_CRYPTO_BACKEND),Botan)
 NEED_BOTAN := 1
 CFLAGS     += -DXP_CRYPTO_BACKEND_BOTAN
else ifeq ($(XP_CRYPTO_BACKEND),none)
 # Stub backend — nothing to link for this side.
else
 $(error Unknown XP_CRYPTO_BACKEND: $(XP_CRYPTO_BACKEND))
endif

ifdef NEED_OPENSSL
 # syncterm's xp_tls_openssl.c / xp_crypt_openssl.c and libdeucessh.a
 # both reference OpenSSL symbols; append to TRAILING_LIBS so they land
 # after the static libraries on the link line (static archives resolve
 # in order).  Also pick up include paths via pkg-config so non-default
 # OpenSSL installs (libcrypto3-dev in /usr/include/openssl3/, Homebrew
 # on macOS, /usr/local on FreeBSD) are found.
 OPENSSL_CFLAGS := $(shell pkg-config --cflags libcrypto libssl 2>/dev/null)
 CFLAGS        += $(OPENSSL_CFLAGS)
 TRAILING_LIBS += -lssl -lcrypto
endif
ifdef NEED_BOTAN
 # Strip -pthread from botan-3's pkg-config output — xpdev already
 # handles thread-library linkage per-platform (Common.gmake adds
 # -pthread / -lwinpthread where appropriate), and on MinGW a bare
 # -pthread here trips "cannot find -lpthread".
 # Append to TRAILING_LIBS — xpdev and deucessh both reference Botan
 # symbols, so Botan must appear after both static archives.
 TRAILING_LIBS += $(filter-out -pthread,$(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config --libs botan-3))
endif

# ini_crypt.c + xp_crypt/xp_tls backend shim are always built, even
# with WITHOUT_CRYPTO — bbslist.c and syncterm.c reference the
# encrypted-INI API unconditionally (iniWriteEncryptedFile is the
# general write path; with INI_CRYPT_ALGO_NONE it falls through to
# plain iniWriteFile).  The "none" backend provides no-op xp_crypt
# and xp_tls so nothing drags in OpenSSL / Botan.
#
# xp_crypt / xp_tls sources live in ../xptls/ (standalone library).
XPTLS_SRC = ../xptls
CFLAGS += -I$(XPTLS_SRC)
OBJS += $(MTOBJODIR)$(DIRSEP)ini_crypt$(OFILE)
ifeq ($(XP_CRYPTO_BACKEND),Botan)
 # Botan 3 public headers require C++20 (std::span).  Override the
 # tree-default -std=c++11 for just the Botan shim objects.
 BOTAN_CXXFLAGS = $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config --cflags botan-3)
 OBJS += $(MTOBJODIR)$(DIRSEP)xp_crypt_botan3$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)xp_tls_botan3$(OFILE)
 $(MTOBJODIR)$(DIRSEP)xp_crypt_botan3$(OFILE): $(XPTLS_SRC)/xp_crypt_botan3.cpp | botan $(MTOBJODIR)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CXX) -std=c++20 $(CFLAGS) $(CXXFLAGS) $(MT_CFLAGS) $(BOTAN_CXXFLAGS) -o $@ -c $<
 $(MTOBJODIR)$(DIRSEP)xp_tls_botan3$(OFILE): $(XPTLS_SRC)/xp_tls_botan3.cpp | botan $(MTOBJODIR)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CXX) -std=c++20 $(CFLAGS) $(CXXFLAGS) $(MT_CFLAGS) $(BOTAN_CXXFLAGS) -o $@ -c $<
else ifeq ($(XP_CRYPTO_BACKEND),OpenSSL)
 OBJS += $(MTOBJODIR)$(DIRSEP)xp_crypt_openssl$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)xp_tls_openssl$(OFILE)
 $(MTOBJODIR)$(DIRSEP)xp_crypt_openssl$(OFILE): $(XPTLS_SRC)/xp_crypt_openssl.c | $(MTOBJODIR)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) $(MT_CFLAGS) -o $@ -c $<
 $(MTOBJODIR)$(DIRSEP)xp_tls_openssl$(OFILE): $(XPTLS_SRC)/xp_tls_openssl.c | $(MTOBJODIR)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) $(MT_CFLAGS) -o $@ -c $<
else
 OBJS += $(MTOBJODIR)$(DIRSEP)xp_crypt_none$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)xp_tls_none$(OFILE)
 $(MTOBJODIR)$(DIRSEP)xp_crypt_none$(OFILE): $(XPTLS_SRC)/xp_crypt_none.c | $(MTOBJODIR)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) $(MT_CFLAGS) -o $@ -c $<
 $(MTOBJODIR)$(DIRSEP)xp_tls_none$(OFILE): $(XPTLS_SRC)/xp_tls_none.c | $(MTOBJODIR)
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) $(CFLAGS) $(MT_CFLAGS) -o $@ -c $<
endif

ifndef WITHOUT_CRYPTO
 OBJS += $(MTOBJODIR)$(DIRSEP)telnets$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)conn_mqtt$(OFILE)

 # Bundled decrypt-only reference implementations of legacy ciphers
 # (IDEA, RC2) that neither OpenSSL (modern, w/o legacy provider) nor
 # Botan3 carry.  Registered with xp_crypt at startup so encrypted
 # bbslist.ini files written by older builds still decrypt.  Only
 # meaningful when a real xp_crypt backend is linked — the "none"
 # backend can't decrypt anything anyway.
 OBJS   += $(MTOBJODIR)$(DIRSEP)legacy_idea$(OFILE)
 OBJS   += $(MTOBJODIR)$(DIRSEP)legacy_rc2$(OFILE)
 OBJS   += $(MTOBJODIR)$(DIRSEP)legacy_ciphers_register$(OFILE)
 CFLAGS += -Ilegacy_ciphers

 $(MTOBJODIR)$(DIRSEP)legacy_idea$(OFILE): legacy_ciphers/idea.c
	@echo Compiling $<
	$(QUIET)$(CC) $(CFLAGS) $(MT_CFLAGS) -c $< -o $@

 $(MTOBJODIR)$(DIRSEP)legacy_rc2$(OFILE): legacy_ciphers/rc2.c
	@echo Compiling $<
	$(QUIET)$(CC) $(CFLAGS) $(MT_CFLAGS) -c $< -o $@

 $(MTOBJODIR)$(DIRSEP)legacy_ciphers_register$(OFILE): legacy_ciphers/register.c
	@echo Compiling $<
	$(QUIET)$(CC) $(CFLAGS) $(MT_CFLAGS) -c $< -o $@
endif

ifeq ($(os),freebsd)
 EXTRA_LIBS += -lstdthreads
endif

ifndef WITHOUT_JPEG_XL
 ifdef win
  CFLAGS += -I${3RDP_ROOT}/win32.release/libjxl/include -DJXL_THREADS_STATIC_DEFINE -DJXL_STATIC_DEFINE -DDLLIFY
  LDFLAGS += -L${3RDP_ROOT}/${os}.release/libjxl/lib -static-libstdc++ -static-libgcc
  EXTRA_LIBS += -ljxl
  EXTRA_LIBS += -ljxl_threads
  EXTRA_LIBS += -lbrotlidec
  EXTRA_LIBS += -lbrotlicommon
  EXTRA_LIBS += -lhwy
  EXTRA_LIBS += -mdll
  CFLAGS += -DWITH_JPEG_XL -DWITH_STATIC_JXL
  CFLAGS += -DWITH_JPEG_XL_THREADS
  OBJS += $(MTOBJODIR)$(DIRSEP)libjxl$(OFILE)
  OBJS += $(MTOBJODIR)$(DIRSEP)conn_conpty$(OFILE)
 else
  ifeq ($(shell pkg-config libjxl --exists && echo YES), YES)
   CFLAGS += $(shell pkg-config libjxl --cflags)
   CFLAGS += -DWITH_JPEG_XL
   OBJS += $(MTOBJODIR)$(DIRSEP)libjxl$(OFILE)
   ifeq ($(shell pkg-config libjxl_threads --exists && echo YES), YES)
    CFLAGS += $(shell pkg-config libjxl_threads --cflags)
    CFLAGS += -DWITH_JPEG_XL_THREADS
   endif
  else
   ifeq ($(os),darwin)
    CFLAGS += -I$(3RDP_ROOT)/$(os).release/libjxl/include -DJXL_THREADS_STATIC_DEFINE -DJXL_STATIC_DEFINE
    LDFLAGS += -L$(3RDP_ROOT)/$(os).release/libjxl/lib
    EXTRA_LIBS += -ljxl
    EXTRA_LIBS += -ljxl_threads
    EXTRA_LIBS += -lbrotlidec
    EXTRA_LIBS += -lbrotlicommon
    EXTRA_LIBS += -lhwy
    CFLAGS += -DWITH_JPEG_XL -DWITH_STATIC_JXL
    CFLAGS += -DWITH_JPEG_XL_THREADS
    OBJS += $(MTOBJODIR)$(DIRSEP)libjxl$(OFILE)
   endif
  endif
 endif
endif

OBJS += $(MTOBJODIR)$(DIRSEP)xp_sndfile$(OFILE)
OBJS += $(MTOBJODIR)$(DIRSEP)audio_apc$(OFILE)

# Wren scripting VM (vendored under wren/).  The basenames in wren/vm/
# and wren/optional/ all begin with `wren_` and are unique across the
# two dirs, so vpath + flat MTOBJODIR output works without renaming.
# The implicit MT C rule in Common.gmake handles the actual compile;
# target-specific CFLAGS below add Wren's private include paths and
# undef the SyncTERM PREFIX define that collides with Wren's PREFIX
# function-like macro.  Wren's optional modules default to enabled in
# wren_common.h, so no -D flags are needed.
vpath %.c wren/vm wren/optional

WREN_VM_SRCS  := $(wildcard wren/vm/*.c)
WREN_OPT_SRCS := $(wildcard wren/optional/*.c)
WREN_OBJS     := $(addprefix $(MTOBJODIR)$(DIRSEP),\
                   $(notdir $(WREN_VM_SRCS:.c=$(OFILE))) \
                   $(notdir $(WREN_OPT_SRCS:.c=$(OFILE))))

OBJS   += $(WREN_OBJS)
OBJS   += $(MTOBJODIR)$(DIRSEP)wren_host$(OFILE)
OBJS   += $(MTOBJODIR)$(DIRSEP)wren_bind$(OFILE)
OBJS   += $(MTOBJODIR)$(DIRSEP)wren_bind_sftp$(OFILE)
OBJS   += $(MTOBJODIR)$(DIRSEP)wren_bind_fs$(OFILE)
OBJS   += $(MTOBJODIR)$(DIRSEP)wren_bind_screen$(OFILE)
OBJS   += $(MTOBJODIR)$(DIRSEP)wren_bind_conn$(OFILE)
OBJS   += $(MTOBJODIR)$(DIRSEP)wren_bind_hook$(OFILE)
OBJS   += $(MTOBJODIR)$(DIRSEP)wren_bind_won$(OFILE)
OBJS   += $(MTOBJODIR)$(DIRSEP)wren_bind_xfer$(OFILE)
OBJS   += $(MTOBJODIR)$(DIRSEP)wren_token$(OFILE)
CFLAGS += -Iwren/include

# Wren-private include paths and PREFIX undef apply to every TU that
# pulls in wren/vm/* headers (the VM itself, plus wren_host.c and
# wren_bind.c which reach for wren_vm.h / wren_value.h directly).
WREN_ALL_OBJS := $(WREN_OBJS) \
                 $(MTOBJODIR)$(DIRSEP)wren_host$(OFILE) \
                 $(MTOBJODIR)$(DIRSEP)wren_bind$(OFILE)
$(WREN_ALL_OBJS): CFLAGS += -Iwren/vm -Iwren/optional -UPREFIX

# RE1 (Russ Cox's NFA regex engine) — used by Hook.onMatch in wren_bind.c.
# Only the four files we actually use (compile/pike/sub + the yacc-
# generated parser).  The pedagogical sources don't pass our usual
# warning set, so suppress on these TUs only.
vpath %.c re1
RE1_OBJS := $(MTOBJODIR)$(DIRSEP)compile$(OFILE) \
            $(MTOBJODIR)$(DIRSEP)pike$(OFILE) \
            $(MTOBJODIR)$(DIRSEP)sub$(OFILE) \
            $(MTOBJODIR)$(DIRSEP)y.tab$(OFILE)
OBJS    += $(RE1_OBJS)
$(RE1_OBJS): CFLAGS += -Ire1 -w
# wren_bind reaches for re1/regexp.h too.
$(MTOBJODIR)$(DIRSEP)wren_bind$(OFILE): CFLAGS += -Ire1
$(MTOBJODIR)$(DIRSEP)wren_bind_hook$(OFILE): CFLAGS += -Ire1
$(MTOBJODIR)$(DIRSEP)wren_host$(OFILE): CFLAGS += -Ire1

# Embedded-script codegen.  wren_embed_gen is built with $(BUILD_CC) so
# cross-builds (e.g. MinGW) compile the codegen with the *host* compiler
# and run it natively, then the generated embedded_scripts.c compiles
# with the target $(CC) like any other source file.  Mirrors the conio
# genmap pattern (src/conio/genmap.c).
#
# Two source directories feed the embed:
#   * scripts/*.wren           — pure library modules, loaded only when
#                                 something imports them.
#   * scripts/auto/<event>/*.wren — auto-loaded entry scripts.  Each
#                                    subdirectory under auto/ names a
#                                    framework event (connected, startup,
#                                    …); files in it run when that event
#                                    fires.  For now there's just one,
#                                    `connected`, fired per BBS session.
#
# Developer-only files are not embedded:
#   * scripts/auto/connected/runtests.wren — Alt+T hotkey trigger
#   * scripts/wrentest.wren                — Wren self-test suite
#   * scripts/*_test.wren                  — per-library test modules
#   * scripts/*_demo.wren                  — interactive playgrounds
# Devs run syncterm against the source `scripts/` dir to pick them up;
# user builds get a binary without them.
WREN_SCRIPTS    := $(filter-out scripts/wrentest.wren, \
                       $(filter-out scripts/%_test.wren, \
                       $(filter-out scripts/%_demo.wren, \
                                    $(wildcard scripts/*.wren)))) \
                   $(filter-out scripts/auto/connected/runtests.wren, \
                                $(wildcard scripts/auto/connected/*.wren))
WREN_EMBED_GEN  := $(MTOBJODIR)$(DIRSEP)wren_embed_gen$(EXEFILE)
WREN_EMBED_SRC  := $(MTOBJODIR)$(DIRSEP)embedded_scripts.c
WREN_EMBED_OBJ  := $(MTOBJODIR)$(DIRSEP)embedded_scripts$(OFILE)

OBJS += $(WREN_EMBED_OBJ)

$(WREN_EMBED_GEN): wren_embed_gen.c | $(MTOBJODIR)
	@echo Building $@
	$(QUIET)$(BUILD_CC) -O2 -o $@ $<

$(WREN_EMBED_SRC): $(WREN_EMBED_GEN) $(WREN_SCRIPTS)
	@echo Generating $@
	$(QUIET)$(WREN_EMBED_GEN) $(WREN_SCRIPTS) > $@

$(WREN_EMBED_OBJ): $(WREN_EMBED_SRC) wren_host_internal.h
	@echo $(COMPILE_MSG) $<
	$(QUIET)$(CC) -std=c17 $(CFLAGS) $(CCFLAGS) $(MT_CFLAGS) -I. -o $@ -c $<

# libsndfile: three build modes — WITHOUT_SNDFILE (stubs only),
# STATIC_SNDFILE (direct linkage — used for macOS / Win32 release
# shape), or the default dynamic WITH_SNDFILE (runtime xp_dlopen, no
# link-time dep).  sndfile.c references <sndfile.h> in the last two
# modes, so include-path detection has to resolve before the
# stub-only case can be overridden.
ifdef WITHOUT_SNDFILE
 CFLAGS += -DWITHOUT_SNDFILE
else
 ifdef win
  CFLAGS += -I${3RDP_ROOT}/${os}.release/libsndfile/include -DSTATIC_SNDFILE
  LDFLAGS += -L${3RDP_ROOT}/${os}.release/libsndfile/bin -L${3RDP_ROOT}/${os}.release/flac/bin -L${3RDP_ROOT}/${os}.release/lame/bin -L${3RDP_ROOT}/${os}.release/ogg/bin -L${3RDP_ROOT}/${os}.release/opus/bin -L${3RDP_ROOT}/${os}.release/mpg123/bin
  EXTRA_LIBS += -lsndfile
  EXTRA_LIBS += -lFLAC
  EXTRA_LIBS += -lmp3lame
  EXTRA_LIBS += -lmpg123
  EXTRA_LIBS += -lopus
  EXTRA_LIBS += -logg
 else ifeq ($(os),darwin)
  CFLAGS += -I${3RDP_ROOT}/darwin.release/libsndfile/include -DSTATIC_SNDFILE
  LDFLAGS += -L${3RDP_ROOT}/darwin.release/libsndfile/lib -L${3RDP_ROOT}/${os}.release/flac/lib -L${3RDP_ROOT}/${os}.release/lame/lib -L${3RDP_ROOT}/${os}.release/ogg/lib -L${3RDP_ROOT}/${os}.release/opus/lib -L${3RDP_ROOT}/${os}.release/mpg123/lib -L${3RDP_ROOT}/${os}.release/vorbis/lib
  EXTRA_LIBS += -lsndfile
  EXTRA_LIBS += -lFLAC
  EXTRA_LIBS += -lmp3lame
  EXTRA_LIBS += -lmpg123
  EXTRA_LIBS += -lopus
  EXTRA_LIBS += -logg
  EXTRA_LIBS += -lvorbis
  EXTRA_LIBS += -lvorbisenc
 else
  ifdef STATIC_SNDFILE
   # Static link — headers + .a/.lib supplied by the environment.  Use
   # pkg-config if available for correct flags, else assume the lib is
   # on the default search path and the header under /usr/local.
   ifeq ($(shell pkg-config sndfile --exists && echo YES), YES)
    CFLAGS     += $(shell pkg-config sndfile --cflags) -DSTATIC_SNDFILE
    EXTRA_LIBS += $(shell pkg-config sndfile --libs)
   else
    ifneq ($(wildcard /usr/local/include/sndfile.h),)
     CFLAGS += -I/usr/local/include -DSTATIC_SNDFILE
    else
     CFLAGS += -DSTATIC_SNDFILE
    endif
    LDFLAGS    += -L/usr/local/lib
    EXTRA_LIBS += -lsndfile
   endif
  else
   # Dynamic runtime path.  Probe for sndfile.h; if absent, fall back
   # to stubs so a build without the header still links.
   ifeq ($(shell pkg-config sndfile --exists && echo YES), YES)
    CFLAGS += $(shell pkg-config sndfile --cflags)
    CFLAGS += -DWITH_SNDFILE
   else
    ifneq ($(wildcard /usr/local/include/sndfile.h),)
     CFLAGS += -I/usr/local/include -DWITH_SNDFILE
    else
     ifneq ($(wildcard /usr/include/sndfile.h),)
      CFLAGS += -DWITH_SNDFILE
     else
      CFLAGS += -DWITHOUT_SNDFILE
     endif
    endif
   endif
  endif
 endif
endif

$(MTOBJODIR)$(DIRSEP)syncterm$(OFILE): $(CIOLIB-MT)
$(CIOLIB_INTERPOLATE_OBJS): $(CIOLIB-MT_BUILD)

ifeq ($(os),darwin)
 OBJS += $(MTOBJODIR)$(DIRSEP)DarwinWrappers$(OFILE)
 EXTRA_LIBS += -framework Foundation
endif

DESTDIR	?=
PREFIX	?= /usr/local
DESKTOPDIR ?= $(PREFIX)/share/applications

CFLAGS	+=	-DPREFIX=\"${DESTDIR}${PREFIX}\"
CFLAGS	+=	-DTELNET_NO_DLL
ifeq ($(PREFIX),/usr)
 SYSTEM_LIST_DIR ?= /etc
else
 SYSTEM_LIST_DIR ?= ${PREFIX}/etc
endif
MANPREFIX ?= $(PREFIX)/share
CFLAGS	+=	-DSYSTEM_LIST_DIR=\"${SYSTEM_LIST_DIR}\"

CFLAGS	+=	$(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) $(ENCODE_CFLAGS) $(HASH_CFLAGS) -I. -I../sbbs3 -I../comio
LDFLAGS	+=	$(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(ENCODE_LDFLAGS) $(HASH_LDFLAGS)

vpath %.c ../sbbs3 ../uifc ../comio

ifdef DEBUG
 INSTALL_EXE	?=	install
else
 INSTALL_EXE	?=	install -s
endif
INSTALL_DATA	?=	install -m 0644

ifdef win
 OBJS	+= $(MTOBJODIR)$(DIRSEP)comio_win32$(OFILE) \
           $(MTOBJODIR)$(DIRSEP)modem$(OFILE) \
           $(MTOBJODIR)$(DIRSEP)comio$(OFILE) \
           $(MTOBJODIR)$(DIRSEP)syncterm_res$(OFILE)
 BUILD_DEPENDS += $(MTOBJODIR)$(DIRSEP)ciolib_res$(OFILE) \
           $(MTOBJODIR)$(DIRSEP)syncterm_res$(OFILE)
else
 ifneq ($(os),haiku)
  OBJS	+= $(MTOBJODIR)$(DIRSEP)comio_nix$(OFILE) \
           $(MTOBJODIR)$(DIRSEP)modem$(OFILE) \
           $(MTOBJODIR)$(DIRSEP)comio$(OFILE)
 endif
endif
OBJS += $(MTOBJODIR)$(DIRSEP)conn_pty$(OFILE)
ifndef bcc
 ifneq ($(os),sunos)
  ifneq ($(os),darwin)
   ifneq ($(os),haiku)
    ifndef win
     EXTRA_LIBS   +=  -lutil
    endif
   endif
  endif
 endif
endif

ifdef win
 EXTRA_LIBS += -luuid
endif

ifdef WITH_RETRO
 ifndef win
  LDFLAGS += -shared
 endif
endif

$(MTOBJODIR)$(DIRSEP)ciolib_res$(OFILE): ${CIOLIB-MT}
	cd ${MTOBJODIR} && $(AR) -x ../${CIOLIB-MT} ciolib_res$(OFILE)

$(SYNCTERM): $(DEPS) $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	${QUIET}$(CXX) $(LDFLAGS) $(MT_LDFLAGS) $(OBJS) -o $@ $(UIFC-MT_LIBS) $(EXTRA_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(TRAILING_LIBS)
ifdef win
	${QUIET}$(CXX) $(LDFLAGS) $(MT_LDFLAGS) win32_stub.c $(MTOBJODIR)$(DIRSEP)syncterm_res$(OFILE) $(CIOLIB_SRC)/$(MTOBJODIR)/ciolib_res.o -s -o $(EXEODIR)/syncterm.exe -L$(EXEODIR) -lsyncterm -mwindows
	${QUIET}$(CXX) $(LDFLAGS) $(MT_LDFLAGS) win32_stub.c $(MTOBJODIR)$(DIRSEP)syncterm_res$(OFILE) $(CIOLIB_SRC)/$(MTOBJODIR)/ciolib_res.o -s -o $(EXEODIR)/syncterm.com -L$(EXEODIR) -lsyncterm -mconsole
endif
ifeq ($(os),darwin)
	-${QUIET}mkdir $(EXEODIR)/SyncTERM.app
	-${QUIET}mkdir $(EXEODIR)/SyncTERM.app/Contents
	-${QUIET}mkdir $(EXEODIR)/SyncTERM.app/Contents/MacOS
	-${QUIET}mkdir $(EXEODIR)/SyncTERM.app/Contents/Resources
ifdef SDL_FRAMEWORK_PATH
	-${QUIET}mkdir $(EXEODIR)/SyncTERM.app/Contents/Frameworks
endif
	${QUIET}echo "APPL????" > $(EXEODIR)/SyncTERM.app/Contents/PkgInfo
	${QUIET}${INSTALL_EXE} ${SYNCTERM} ${EXEODIR}/SyncTERM.app/Contents/MacOS
	${QUIET}${INSTALL_DATA} Info.plist ${EXEODIR}/SyncTERM.app/Contents
	${QUIET}${INSTALL_DATA} SyncTERM.icns ${EXEODIR}/SyncTERM.app/Contents/Resources
ifdef SDL_FRAMEWORK_PATH
	-${QUIET}rm -rf ${EXEODIR}/SyncTERM.app/Contents/Frameworks/SDL2.framework
	-${QUIET}cp -R ${SDL_FRAMEWORK_PATH}/SDL2.framework ${EXEODIR}/SyncTERM.app/Contents/Frameworks
	-${QUIET}rm -rf ${EXEODIR}/SyncTERM.app/Contents/Frameworks/SDL2.framework/Headers/*
endif
endif

syncterm.man: syncterm.man.in
	@echo Creating $@
	${QUIET}sed -E -e 's|%%DESTDIR%%|${DESTDIR}|g' -e 's|%%PREFIX%%|${PREFIX}|g' < syncterm.man.in > syncterm.man

syncterm.1.gz: syncterm.man
	@echo Creating $@
	${QUIET}gzip -9n < syncterm.man > syncterm.1.gz

installdirs:
	-mkdir -p ${DESTDIR}${PREFIX}/bin
	-mkdir -p ${DESTDIR}${DESKTOPDIR}
	-mkdir -p ${DESTDIR}${MANPREFIX}/man/man1
	-mkdir -p ${DESTDIR}${PREFIX}/share/icons/hicolor/16x16/apps
	-mkdir -p ${DESTDIR}${PREFIX}/share/icons/hicolor/22x22/apps
	-mkdir -p ${DESTDIR}${PREFIX}/share/icons/hicolor/24x24/apps
	-mkdir -p ${DESTDIR}${PREFIX}/share/icons/hicolor/32x32/apps
	-mkdir -p ${DESTDIR}${PREFIX}/share/icons/hicolor/36x36/apps
	-mkdir -p ${DESTDIR}${PREFIX}/share/icons/hicolor/48x48/apps
	-mkdir -p ${DESTDIR}${PREFIX}/share/icons/hicolor/64x64/apps
	-mkdir -p ${DESTDIR}${PREFIX}/share/icons/hicolor/256x256/apps
	-mkdir -p ${DESTDIR}${PREFIX}/share/icons/hicolor/scalable/apps

install: $(SYNCTERM) syncterm.1.gz installdirs
	@echo Installing...
	${INSTALL_EXE} ${SYNCTERM} ${DESTDIR}${PREFIX}/bin
	${INSTALL_DATA} syncterm16.png ${DESTDIR}${PREFIX}/share/icons/hicolor/16x16/apps/syncterm.png
	${INSTALL_DATA} syncterm22.png ${DESTDIR}${PREFIX}/share/icons/hicolor/22x22/apps/syncterm.png
	${INSTALL_DATA} syncterm24.png ${DESTDIR}${PREFIX}/share/icons/hicolor/24x24/apps/syncterm.png
	${INSTALL_DATA} syncterm32.png ${DESTDIR}${PREFIX}/share/icons/hicolor/32x32/apps/syncterm.png
	${INSTALL_DATA} syncterm36.png ${DESTDIR}${PREFIX}/share/icons/hicolor/36x36/apps/syncterm.png
	${INSTALL_DATA} syncterm48.png ${DESTDIR}${PREFIX}/share/icons/hicolor/48x48/apps/syncterm.png
	${INSTALL_DATA} syncterm64.png ${DESTDIR}${PREFIX}/share/icons/hicolor/64x64/apps/syncterm.png
	${INSTALL_DATA} syncterm256.png ${DESTDIR}${PREFIX}/share/icons/hicolor/256x256/apps/syncterm.png
	${INSTALL_DATA} icons/syncterm.svg ${DESTDIR}${PREFIX}/share/icons/hicolor/scalable/apps
	${INSTALL_DATA} icons/syncterm-mini.svg ${DESTDIR}${PREFIX}/share/icons/hicolor/scalable/apps
	${INSTALL_DATA} syncterm.desktop ${DESTDIR}${DESKTOPDIR}
	${INSTALL_DATA} syncterm.1.gz ${DESTDIR}${MANPREFIX}/man/man1

dpkgdirs:
	@echo Creating dpkg directories
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/bin
	${QUIET}-mkdir -p dpkg${DESTDIR}${DESKTOPDIR}
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/share/doc/syncterm
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/16x16/apps
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/22x22/apps
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/24x24/apps
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/32x32/apps
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/36x36/apps
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/48x48/apps
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/64x64/apps
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/256x256/apps
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/scalable/apps
	${QUIET}-mkdir -p dpkg${DESTDIR}${MANPREFIX}/man/man1
	${QUIET}-mkdir -p dpkg/DEBIAN

dpkg-control: dpkg-control.in
	@echo Creating $@
	${QUIET}sed -E -e "s|%%ARCH%%|$(shell dpkg-architecture -q DEB_BUILD_ARCH)|g" < dpkg-control.in > dpkg-control

changelog.gz: CHANGES
	@echo Creating $@
	${QUIET}gzip -9n < CHANGES > changelog.gz

dpkg: $(SYNCTERM) syncterm.1.gz dpkg-control dpkgdirs changelog.gz
	@echo Creating $@
	${QUIET}${INSTALL_EXE} ${SYNCTERM} dpkg${DESTDIR}${PREFIX}/bin
	${QUIET}${INSTALL_DATA} syncterm16.png dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/16x16/apps/syncterm.png
	${QUIET}${INSTALL_DATA} syncterm22.png dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/22x22/apps/syncterm.png
	${QUIET}${INSTALL_DATA} syncterm24.png dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/24x24/apps/syncterm.png
	${QUIET}${INSTALL_DATA} syncterm32.png dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/32x32/apps/syncterm.png
	${QUIET}${INSTALL_DATA} syncterm36.png dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/36x36/apps/syncterm.png
	${QUIET}${INSTALL_DATA} syncterm48.png dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/48x48/apps/syncterm.png
	${QUIET}${INSTALL_DATA} syncterm64.png dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/64x64/apps/syncterm.png
	${QUIET}${INSTALL_DATA} syncterm256.png dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/256x256/apps/syncterm.png
	${QUIET}${INSTALL_DATA} icons/syncterm.svg dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/scalable/apps
	${QUIET}${INSTALL_DATA} icons/syncterm-mini.svg dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/scalable/apps
	${QUIET}${INSTALL_DATA} syncterm.desktop dpkg${DESTDIR}${DESKTOPDIR}
	${QUIET}${INSTALL_DATA} syncterm.1.gz dpkg${DESTDIR}${MANPREFIX}/man/man1
	${QUIET}${INSTALL_DATA} LICENCE dpkg${DESTDIR}${PREFIX}/share/doc/syncterm/copyright
	${QUIET}${INSTALL_DATA} changelog.gz dpkg${DESTDIR}${PREFIX}/share/doc/syncterm
	${QUIET}${INSTALL_DATA} dpkg-control dpkg/DEBIAN/control
	${QUIET}dpkg-deb --root-owner-group -b dpkg syncterm_1.10a-${shell date -u +%Y%m%d}_${shell dpkg-architecture -q DEB_BUILD_ARCH}.deb
	${QUIET}rm -rf dpkg

ifndef WITHOUT_SDL
TERMTEST = $(EXEODIR)$(DIRSEP)termtest

$(TERMTEST): termtest.c | $(EXEODIR)
	@echo Compiling termtest
	${QUIET}$(CC) -o $@ $<

.PHONY: termtest test parser_vectors
termtest: $(TERMTEST)

test: $(SYNCTERM) $(TERMTEST)
	./run_termtest.sh $(SYNCTERM) $(TERMTEST)

# M1 RIP parser vectors.  Drives rip_test/parser_vectors/* through the
# running harness on $RIP_HARNESS_HOST:$RIP_HARNESS_PORT (default
# portable:1516).  Requires the harness and both terminals connected;
# does NOT start them itself.  See rip_test/parser_vectors/README.md.
parser_vectors:
	@echo Running RIP parser_vectors against harness
	${QUIET}python3 rip_test/parser_vectors/run_vectors.py
else
.PHONY: termtest test parser_vectors
termtest:
	@echo "termtest requires SDL (skipped)"
test:
	@echo "termtest requires SDL (skipped)"
parser_vectors:
	@echo "parser_vectors requires SDL (skipped)"
endif

devel: tags cscope.out

cscope.out: cscope.files
	cscope -b

tags: cscope.files
	exctags -VL cscope.files

cscope.files::
	find . ../conio ../uifc ../xpdev -name '*.c' -o -name '*.cpp' -o -name '*.h' > cscope.files
	echo ../sbbs3/telnet.c >> cscope.files
	echo ../sbbs3/telnet.h >> cscope.files
	echo ../sbbs3/zmodem.c >> cscope.files
	echo ../sbbs3/zmodem.h >> cscope.files
	echo ../sbbs3/xmodem.h >> cscope.files
	echo ../hash/crc16.c >> cscope.files
	echo ../hash/crc16.h >> cscope.files
	echo ../hash/crc32.c >> cscope.files
	echo ../hash/crc32.h >> cscope.files

syncterm-clean:
	rm -rf dpkg
	${DELETE} syncterm.deb
	${DELETE} syncterm.man
	${DELETE} syncterm.1.gz
	${DELETE} syncterm.1
	${DELETE} dpkg-control
	${DELETE} changelog.gz

clean: syncterm-clean

ifdef win
$(MTOBJODIR)$(DIRSEP)syncterm_res${OFILE}: syncterm.rc
	@echo Creating $< ...
	$(QUIET)${WINDRES} $(WINDRESFLAGS) -O coff -i syncterm.rc -o $@
endif
