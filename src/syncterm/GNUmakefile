SRC_ROOT	:=	..
include ${SRC_ROOT}/build/Common.gmake
include extdeps.mk

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
 LDFLAGS += -lnsl -lrt -lcurses -ldl
 CFLAGS	+=	-DNEED_CFMAKERAW
endif

ifdef WITHOUT_CRYPTLIB
 CFLAGS += -DWITHOUT_CRYPTLIB
 CRYPT_LDFLAGS :=
 CRYPT_DEPS :=
else
 OBJS += $(MTOBJODIR)$(DIRSEP)st_crypt$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)ssh$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)telnets$(OFILE)
 CFLAGS	+=	$(CRYPT_CFLAGS) $(SFTP-MT_CFLAGS)
 CRYPT_LDFLAGS += $(SFTP-MT_LDFLAGS)
 EXTRA_LIBS	+=	$(CRYPT_LIBS) $(SFTP-MT_LIBS)
 STATIC_CRYPTLIB	:= true
endif

ifeq ($(os),darwin)
 ifeq ($(shell uname -r),6.8)
  EXTRA_LIBS += /usr/local/lib/libdl.a
 endif
 EXTRA_LIBS += -framework CoreServices
 STATIC_CRYPTLIB ?= 1
endif

ifdef STATIC_CRYPTLIB
 CFLAGS += -DSTATIC_CRYPTLIB
endif

ifeq ($(os),linux)
 ifeq ($(shell uname -m),x86_64)
  LDFLAGS += -ldl
 endif
endif

DESTDIR	?=
PREFIX	?= /usr/local
DESKTOPDIR ?= $(PREFIX)/share/applications

CFLAGS	+=	-DPREFIX=\"${DESTDIR}${PREFIX}\"
ifeq ($(PREFIX),/usr)
 MANPREFIX ?= /usr/share
 SYSTEM_LIST_DIR ?= /etc
else
 MANPREFIX ?= $(PREFIX)
 SYSTEM_LIST_DIR ?= ${PREFIX}/etc
endif
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

ifeq ($(os),win32)
 OBJS	+= $(MTOBJODIR)$(DIRSEP)comio_win32$(OFILE) \
           $(MTOBJODIR)$(DIRSEP)modem$(OFILE) \
           $(MTOBJODIR)$(DIRSEP)comio$(OFILE) \
           $(MTOBJODIR)$(DIRSEP)ciolib_res$(OFILE)
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
    ifneq ($(os),win32)
      EXTRA_LIBS   +=  -lutil
    endif
   endif
  endif
 endif
endif

ifeq ($(os),win32)
 EXTRA_LIBS += -luuid
endif

$(MTOBJODIR)$(DIRSEP)ciolib_res$(OFILE):
	cd ${MTOBJODIR} && $(AR) -x ../${CIOLIB-MT} ciolib_res$(OFILE)

$(SYNCTERM): $(CRYPT_DEPS) $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) $(OBJS) -o $@ $(UIFC-MT_LIBS) $(EXTRA_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS) $(ENCODE_LIBS) $(HASH_LIBS)
ifeq ($(os),darwin)
	-${QUIET}mkdir $(EXEODIR)/SyncTERM.app
	-${QUIET}mkdir $(EXEODIR)/SyncTERM.app/Contents
	-${QUIET}mkdir $(EXEODIR)/SyncTERM.app/Contents/MacOS
	-${QUIET}mkdir $(EXEODIR)/SyncTERM.app/Contents/Resources
	-${QUIET}mkdir $(EXEODIR)/SyncTERM.app/Contents/Frameworks
	${QUIET}echo "APPL????" > $(EXEODIR)/SyncTERM.app/Contents/PkgInfo
	${QUIET}${INSTALL_EXE} ${SYNCTERM} ${EXEODIR}/SyncTERM.app/Contents/MacOS
	${QUIET}${INSTALL_DATA} Info.plist ${EXEODIR}/SyncTERM.app/Contents
	${QUIET}${INSTALL_DATA} SyncTERM.icns ${EXEODIR}/SyncTERM.app/Contents/Resources
	-${QUIET}rm -rf ${EXEODIR}/SyncTERM.app/Contents/Frameworks/SDL2.framework
	${QUIET}cp -R ${SDL_FRAMEWORK_PATH}/SDL2.framework ${EXEODIR}/SyncTERM.app/Contents/Frameworks
	${QUIET}rm -rf ${EXEODIR}/SyncTERM.app/Contents/Frameworks/SDL2.framework/Headers/*
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
	-mkdir -p ${DESTDIR}${PREFIX}/share/icons/hicolor/64x64/apps

install: $(SYNCTERM) syncterm.1.gz installdirs
	@echo Installing...
	${INSTALL_EXE} ${SYNCTERM} ${DESTDIR}${PREFIX}/bin
	${INSTALL_DATA} syncterm.png ${DESTDIR}${PREFIX}/share/icons/hicolor/64x64/apps
	${INSTALL_DATA} syncterm.desktop ${DESTDIR}${DESKTOPDIR}
	${INSTALL_DATA} syncterm.1.gz ${DESTDIR}${MANPREFIX}/man/man1

dpkgdirs:
	@echo Creating dpkg directories
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/bin
	${QUIET}-mkdir -p dpkg${DESTDIR}${DESKTOPDIR}
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/share/doc/syncterm
	${QUIET}-mkdir -p dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/64x64/apps
	${QUIET}-mkdir -p dpkg${DESTDIR}${MANPREFIX}/share/man/man1
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
	${QUIET}${INSTALL_DATA} syncterm.png dpkg${DESTDIR}${PREFIX}/share/icons/hicolor/64x64/apps
	${QUIET}${INSTALL_DATA} syncterm.desktop dpkg${DESTDIR}${DESKTOPDIR}
	${QUIET}${INSTALL_DATA} syncterm.1.gz dpkg${DESTDIR}${MANPREFIX}/share/man/man1
	${QUIET}${INSTALL_DATA} LICENCE dpkg${DESTDIR}${PREFIX}/share/doc/syncterm/copyright
	${QUIET}${INSTALL_DATA} changelog.gz dpkg${DESTDIR}${PREFIX}/share/doc/syncterm
	${QUIET}${INSTALL_DATA} dpkg-control dpkg/DEBIAN/control
	${QUIET}dpkg-deb --root-owner-group -b dpkg syncterm_1.2a-${shell date -u +%Y%m%d}_${shell dpkg-architecture -q DEB_BUILD_ARCH}.deb
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
