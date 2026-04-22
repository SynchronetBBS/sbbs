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

# SSH / TelnetS / HTTPS / encrypted-INI crypto stack.
#
# ssh.c uses DeuceSSH (src/ssh/) built with cmake as libdeucessh.a.
# telnets.c and webget.c use xpdev's xp_tls shim.  ini_file.c uses
# xpdev's xp_crypt shim.  All three shims link against OpenSSL (libssl
# + libcrypto).  The SFTP-MT library is still used for the pubkey
# upload bridge in ssh.c; it has no Cryptlib dependency itself.
OBJS += $(MTOBJODIR)$(DIRSEP)ssh$(OFILE)
OBJS += $(MTOBJODIR)$(DIRSEP)telnets$(OFILE)
CFLAGS += -I$(SRC_ROOT)/ssh $(SFTP-MT_CFLAGS)
CRYPT_LDFLAGS += $(SFTP-MT_LDFLAGS)

# DeuceSSH sub-build.  The library only ships a CMake build system, so
# we shell out to cmake for it the same way the 3rdp builds do.  The
# configure step is idempotent; the build step is a no-op when the
# .a is already up-to-date.
DEUCESSH_DIR    := $(SRC_ROOT)/ssh
DEUCESSH_BUILD  := $(DEUCESSH_DIR)/build
DEUCESSH_LIB    := $(DEUCESSH_BUILD)/libdeucessh.a
DEUCESSH_PC     := $(DEUCESSH_BUILD)/deucessh.pc

# DeuceSSH is the source of truth for crypto-backend choice.  If the
# caller explicitly set DEUCESSH_BACKEND on the make command line we
# honour that; otherwise we let DeuceSSH's own auto-detect pick
# (preferring Botan over OpenSSL when both are present).  The
# configure step below runs at parse time so deucessh.pc exists
# before we need to read it further down.
ifdef DEUCESSH_BACKEND
 DEUCESSH_CMAKE_BACKEND_ARG := -DDEUCESSH_CRYPTO_BACKEND=$(DEUCESSH_BACKEND)
else
 DEUCESSH_CMAKE_BACKEND_ARG :=
endif
$(shell cmake -S $(DEUCESSH_DIR) -B $(DEUCESSH_BUILD) \
              -DCMAKE_BUILD_TYPE=Release $(DEUCESSH_CMAKE_BACKEND_ARG) \
              >/dev/null 2>&1)

# Derive XP_CRYPTO_BACKEND from DeuceSSH's deucessh.pc so SyncTERM's
# xp_crypt / xp_tls shims link against the same crypto backend.
# Requires.private names libcrypto (OpenSSL) or botan-3 (Botan).
DEUCESSH_REQUIRES := $(shell awk '/^Requires.private:/{print $$2}' $(DEUCESSH_PC) 2>/dev/null)
ifeq ($(DEUCESSH_REQUIRES),libcrypto)
 XP_CRYPTO_BACKEND := OpenSSL
else ifeq ($(DEUCESSH_REQUIRES),botan-3)
 XP_CRYPTO_BACKEND := Botan
else
 $(error Cannot determine DeuceSSH crypto backend from $(DEUCESSH_PC))
endif
export XP_CRYPTO_BACKEND

.PHONY: deucessh
deucessh $(DEUCESSH_LIB):
	@echo "Building DeuceSSH ($(XP_CRYPTO_BACKEND) backend)"
	cmake --build $(DEUCESSH_BUILD) --target deucessh_static

DEPS += $(DEUCESSH_LIB)
EXTRA_LIBS += $(DEUCESSH_LIB) $(SFTP-MT_LIBS)

ifeq ($(XP_CRYPTO_BACKEND),OpenSSL)
 EXTRA_LIBS += -lssl -lcrypto
endif
ifeq ($(XP_CRYPTO_BACKEND),Botan)
 EXTRA_LIBS += $(shell pkg-config --libs botan-3)
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

OBJS += $(MTOBJODIR)$(DIRSEP)sndfile$(OFILE)
OBJS += $(MTOBJODIR)$(DIRSEP)audio_apc$(OFILE)

# libsndfile: three build modes — WITHOUT_SNDFILE (stubs only),
# STATIC_SNDFILE (direct linkage — used for macOS / Win32 release
# shape), or the default dynamic WITH_SNDFILE (runtime xp_dlopen, no
# link-time dep).  sndfile.c references <sndfile.h> in the last two
# modes, so include-path detection has to resolve before the
# stub-only case can be overridden.
ifdef WITHOUT_SNDFILE
 CFLAGS += -DWITHOUT_SNDFILE
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

$(MTOBJODIR)$(DIRSEP)conn$(OFILE): $(CRYPT_LIBS)
$(MTOBJODIR)$(DIRSEP)ssh$(OFILE): $(CRYPT_LIBS)
$(MTOBJODIR)$(DIRSEP)syncterm$(OFILE): $(CRYPT_LIBS) $(CIOLIB-MT)
$(MTOBJODIR)$(DIRSEP)telnets$(OFILE): $(CRYPT_LIBS)
$(MTOBJODIR)$(DIRSEP)webget$(OFILE): $(CRYPT_LIBS)
$(CIOLIB_INTERPOLATE_OBJS): $(CIOLIB-MT_BUILD)

ifeq ($(os),darwin)
 OBJS += $(MTOBJODIR)$(DIRSEP)DarwinWrappers$(OFILE)
 EXTRA_LIBS += -framework Foundation
 STATIC_CRYPTLIB ?= 1
endif

ifdef STATIC_CRYPTLIB
 CFLAGS += -DSTATIC_CRYPTLIB
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

CFLAGS	+=	$(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) $(ENCODE_CFLAGS) $(HASH_CFLAGS) -I../sbbs3 -I../comio
LDFLAGS	+=	$(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(ENCODE_LDFLAGS) $(HASH_LDFLAGS) $(CRYPT_LDFLAGS)

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

$(SYNCTERM): $(CRYPT_DEPS) $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	${QUIET}$(CXX) $(LDFLAGS) $(MT_LDFLAGS) $(OBJS) -o $@ $(UIFC-MT_LIBS) $(EXTRA_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS) $(ENCODE_LIBS) $(HASH_LIBS)
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
	${QUIET}dpkg-deb --root-owner-group -b dpkg syncterm_1.9b-${shell date -u +%Y%m%d}_${shell dpkg-architecture -q DEB_BUILD_ARCH}.deb
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
