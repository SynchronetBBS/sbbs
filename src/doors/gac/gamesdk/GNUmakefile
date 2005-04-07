SRC_ROOT	:=	../../..
include ${SRC_ROOT}/build/Common.gmake

CFLAGS	+=	$(XPDEV_CFLAGS)
LDFLAGS	+=	$(XPDEV_LDFLAGS)

$(ENCRYPT): $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(OBJS) -o $@ $(XPDEV_LIBS)
