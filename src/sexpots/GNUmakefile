SRC_ROOT        = ..
include ${SRC_ROOT}/build/Common.gmake

CFLAGS  +=       -I${SRC_ROOT}/comio -I${SRC_ROOT}/sbbs3 $(XPDEV-MT_CFLAGS)
LDFLAGS +=       $(XPDEV-MT_LDFLAGS)

vpath %.c ../comio ../sbbs3

$(SEXPOTS): $(OBJS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -o $@ $(OBJS) $(XPDEV-MT_LIBS)

