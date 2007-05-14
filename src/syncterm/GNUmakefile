SRC_ROOT	:=	..
include ${SRC_ROOT}/build/Common.gmake

CFLAGS	+=	-I/usr/local/include	# cryptlib.h lives here by default

ifeq ($(os),sunos)    # Solaris
 LDFLAGS += -lnsl -lrt -lcurses -ldl
 CFLAGS	+=	-DNEED_CFMAKERAW
endif

ifdef USE_GUTS
 CFLAGS +=	-I../guts -DGUTS_BUILTIN
 OBJS	+=	$(MTOBJODIR)$(DIRSEP)gutsz$(OFILE)
endif

ifdef PREFIX
 CFLAGS	+=	-DPREFIX=\"${PREFIX}\"
endif

CFLAGS	+=	$(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) -I../sbbs3 -I../smblib -I../comio
LDFLAGS	+=	$(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)

vpath %.c ../sbbs3 ../smblib ../uifc ../guts ../comio

INSTALL_EXE	?=	install
INSTALL_DATA	?=	install -m 0444

$(SYNCTERM): $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) $(OBJS) -o $@ $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)

install: $(SYNCTERM)
	@echo Installing...
	${INSTALL_EXE} ${SYNCTERM} ${PREFIX}/bin
	${INSTALL_DATA} syncterm.png ${PREFIX}/share/icons/hicolor/64x64/apps
	${INSTALL_DATA} syncterm.desktop ${PREFIX}/share/applications

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
