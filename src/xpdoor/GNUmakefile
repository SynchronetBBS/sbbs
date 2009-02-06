SRC_ROOT	:=	..
include ${SRC_ROOT}/build/Common.gmake

CFLAGS	+=	$(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) -I../sbbs3 -I../smblib -I../comio
LDFLAGS	+=	$(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)
EXTRA_LIBS	+=	$(XPDOOR)

OBJS	+=	$(MTOBJODIR)$(DIRSEP)comio_nix$(OFILE)

vpath %.c ../sbbs3 ../uifc ../conio ../comio

$(XPDOOR): $(LIBODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Creating $@
	$(QUIET)$(AR) rc $@ $(OBJS)
	$(QUIET)$(RANLIB) $@

$(EXAMPLE): $(XPDOOR) $(EXEODIR) $(EX_OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) $(EX_OBJS) -o $@ $(XPDOOR) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(WXLIBS) $(XPDEV-MT_LIBS)

