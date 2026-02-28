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

ifdef WITHOUT_CRYPTLIB
 CFLAGS += -DWITHOUT_CRYPTLIB
 CRYPT_LDFLAGS :=
 CRYPT_DEPS :=
 CRYPT_LIBS :=
else
 OBJS += $(MTOBJODIR)$(DIRSEP)ssh$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)telnets$(OFILE)
 CFLAGS	+=	$(CRYPT_CFLAGS) $(SFTP-MT_CFLAGS)
 CRYPT_LDFLAGS += $(SFTP-MT_LDFLAGS)
 EXTRA_LIBS	+=	$(CRYPT_LIBS) $(SFTP-MT_LIBS)
 STATIC_CRYPTLIB	:= true
 CRYPT_LIBS += $(CRYPT_LIB)
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
	${QUIET}dpkg-deb --root-owner-group -b dpkg syncterm_1.8rc1-${shell date -u +%Y%m%d}_${shell dpkg-architecture -q DEB_BUILD_ARCH}.deb
	${QUIET}rm -rf dpkg

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
