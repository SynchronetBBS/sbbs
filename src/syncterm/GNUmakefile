NEED_UIFC	:=	1
USE_UIFC32	:=	1
UIFC_SRC	:=	../uifc/
XPDEV		:=	../xpdev/
include ${XPDEV}Common.gmake
include ${UIFC_SRC}Common.gmake

ifeq ($(os),sunos)    # Solaris
 LDFLAGS += -lsocket -lnsl -lrt -lncurses
 CFLAGS	+=	-DNEED_CFMAKERAW
endif

$(EXEODIR)/syncterm: $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(OBJS) -o $@
