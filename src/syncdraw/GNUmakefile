SRC_ROOT        = ..
include ${SRC_ROOT}/build/Common.gmake

CFLAGS  +=       $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
LDFLAGS +=       $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)

$(SYNCDRAW): $(XPDEV_DEPS) $(OBJS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -o $@ $(OBJS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)

$(FONTUTIL): $(XPDEV_DEPS) $(FU-OBJS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -o $@ $(FU-OBJS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)

$(BUILDFONT): $(XPDEV_DEPS) $(BF-OBJS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) -o $@ $(BF-OBJS) $(XPDEV_LIBS)
