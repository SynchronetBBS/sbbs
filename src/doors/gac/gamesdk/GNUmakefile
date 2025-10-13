SRC_ROOT	:=	../../..
include ${SRC_ROOT}/build/Common.gmake
include Common.gmake

CFLAGS	+=	$(XPDEV_CFLAGS)
LDFLAGS	+=	$(XPDEV_LDFLAGS)

all: $(ENCRYPT) $(PACKART) $(UNPACKART)

$(ENCRYPT): $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(OBJS) -o $@ $(XPDEV_LIBS)
