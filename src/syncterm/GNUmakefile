SRC_ROOT	:=	..
include ${SRC_ROOT}/build/Common.gmake

ifeq ($(os),sunos)    # Solaris
 LDFLAGS += -lsocket -lnsl -lrt -lncurses
 CFLAGS	+=	-DNEED_CFMAKERAW
endif

CFLAGS	+=	$(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
LDFLAGS	+=	$(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)

$(SYNCTERM): $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) $(OBJS) -o $@ $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)
