SRC_ROOT	:=	..
include ${SRC_ROOT}/build/Common.gmake

CFLAGS	+=	-I/usr/local/include	# cryptlib.h lives here by default
CFLAGS	+=	-I../../include/cryptlib	# cryptlib.h lives here in CVS
ifdef WITHOUT_OOII
 CFLAGS	+= -DWITHOUT_OOII=1
else
 OBJS += $(MTOBJODIR)$(DIRSEP)ooii$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)ooii_logons$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)ooii_cmenus$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)ooii_bmenus$(OFILE)
 OBJS += $(MTOBJODIR)$(DIRSEP)ooii_sounds$(OFILE)
endif

ifdef USE_WXWIDGETS

 ifdef WX_CONFIG
  ifeq ($(shell ${WX_CONFIG} --version > /dev/null 2>&1 && echo YES),YES)
       WITH_WXWIDGETS	:=	1
  endif
 else
  ifeq ($(shell wx-config --version > /dev/null 2>&1 && echo YES),YES)
   WX_CONFIG := wx-config
   WITH_WXWIDGETS	:=	1
      else
   ifeq ($(shell wxgtk2-2.8-config --version > /dev/null 2>&1 && echo YES),YES)
    WX_CONFIG := wxgtk2-2.8-config
    WITH_WXWIDGETS	:=	1
   else
    ifeq ($(shell wxgtk2u-2.8-config --version > /dev/null 2>&1 && echo YES),YES)
     WX_CONFIG := wxgtk2u-2.8-config
     WITH_WXWIDGETS	:=	1
    else
     ifeq ($(shell wxgtk2-config --version > /dev/null 2>&1 && echo YES),YES)
      WX_CONFIG := wxgtk2-config
      WITH_WXWIDGETS	:=	1
     else
      ifeq ($(shell wxgtk2u-config --version > /dev/null 2>&1 && echo YES),YES)
       WX_CONFIG := wxgtk2u-config
       WITH_WXWIDGETS	:=	1
      endif
     endif
    endif
   endif
  endif
 endif
endif

ifdef WITH_WXWIDGETS
 CXXFLAGS += `$(WX_CONFIG) --cxxflags`
 WXLIBS += `$(WX_CONFIG) --libs`
 CFLAGS += -DWITH_WXWIDGETS
 OBJS += $(MTOBJODIR)$(DIRSEP)htmlwin$(OFILE)
endif

ifeq ($(os),sunos)    # Solaris
 LDFLAGS += -lnsl -lrt -lcurses -ldl
 CFLAGS	+=	-DNEED_CFMAKERAW
endif

ifdef WITHOUT_CRYPTLIB
 CFLAGS += -DWITHOUT_CRYPTLIB
else
 OBJS += $(MTOBJODIR)$(DIRSEP)ssh$(OFILE)
endif

ifdef STATIC
 STATIC_CRYPTLIB	:= true
endif

ifdef STATIC_CRYPTLIB
 CFLAGS += -DSTATIC_CRYPTLIB
 LDFLAGS += -lz
 EXTRA_LIBS += $(SRC_ROOT)/../lib/cryptlib/$(machine).release/libcl.a
endif

ifeq ($(os),darwin)
 ifeq ($(shell uname -r),6.8)
  EXTRA_LIBS += /usr/local/lib/libdl.a
 endif
 STATIC_CRYPTLIB ?= 1
endif

ifeq ($(os),linux)
 ifeq ($(shell uname -m),x86_64)
  LDFLAGS += -ldl
 endif
endif

ifdef USE_GUTS
 CFLAGS +=	-I../guts -DGUTS_BUILTIN
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)gutsz$(OFILE)
endif

PREFIX	?= /usr/local
DESKTOPDIR ?= $(PREFIX)/share/applications

CFLAGS	+=	-DPREFIX=\"${PREFIX}\"
ifeq ($(PREFIX),/usr)
 MANPREFIX ?= /usr/share
 SYSTEM_LIST_DIR ?= /etc
else
 MANPREFIX ?= $(PREFIX)
 SYSTEM_LIST_DIR ?= ${PREFIX}/etc
endif
CFLAGS	+=	-DSYSTEM_LIST_DIR=\"${SYSTEM_LIST_DIR}\"

CFLAGS	+=	$(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) -I../sbbs3 -I../smblib -I../comio
LDFLAGS	+=	$(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)

vpath %.c ../sbbs3 ../smblib ../uifc ../guts ../comio

ifdef DEBUG
 INSTALL_EXE	?=	install
else
 INSTALL_EXE	?=	install -s
endif
INSTALL_DATA	?=	install -m 0444

OBJS	+= $(MTOBJODIR)$(DIRSEP)comio_nix$(OFILE)
OBJS	+= $(MTOBJODIR)$(DIRSEP)conn_pty$(OFILE)
ifndef bcc
 ifneq ($(os),sunos)
  ifneq ($(os),darwin)
   LDFLAGS   +=  -lutil
  endif
 endif
endif


$(SYNCTERM): $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) $(OBJS) -o $@ $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(WXLIBS) $(XPDEV-MT_LIBS) $(EXTRA_LIBS)
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
	-${QUIET}rm -rf ${EXEODIR}/SyncTERM.app/Contents/Frameworks/SDL.framework
	${QUIET}cp -R ${SDL_FRAMEWORK_PATH}/SDL.framework ${EXEODIR}/SyncTERM.app/Contents/Frameworks
	${QUIET}rm -rf ${EXEODIR}/SyncTERM.app/Contents/Frameworks/SDL.framework/Headers/*
endif

syncterm.1.gz: syncterm.man
	gzip < syncterm.man > syncterm.1.gz

installdirs:
	-mkdir -p ${PREFIX}/bin
	-mkdir -p ${DESKTOPDIR}
	-mkdir -p ${MANPREFIX}/man/man1
	-mkdir -p ${PREFIX}/share/icons/hicolor/64x64/apps

install: $(SYNCTERM) syncterm.1.gz installdirs
	@echo Installing...
	${INSTALL_EXE} ${SYNCTERM} ${PREFIX}/bin
	${INSTALL_DATA} syncterm.png ${PREFIX}/share/icons/hicolor/64x64/apps
	${INSTALL_DATA} syncterm.desktop ${DESKTOPDIR}
	${INSTALL_DATA} syncterm.1.gz ${MANPREFIX}/man/man1

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
	echo ../smblib/crc16.c >> cscope.files
	echo ../smblib/crc16.h >> cscope.files
	echo ../smblib/crc32.c >> cscope.files
	echo ../smblib/crc32.h >> cscope.files
