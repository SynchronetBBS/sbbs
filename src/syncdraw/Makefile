SRC_ROOT        = ..
!include ${SRC_ROOT}\build\Common.bmake

CFLAGS  =       $(CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
LDFLAGS =       $(LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)

$(SYNCDRAW): $(OBJS)
        @echo Linking $@
        ${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -e$@ $(OBJS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)

$(FONTUTIL): $(FU-OBJS)
        @echo Linking $@
        ${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -e$@ $(FU-OBJS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)

$(BUILDFONT): $(BF-OBJS)
        @echo Linking $@
        ${QUIET}$(CC) $(LDFLAGS) -e$@ $(BF-OBJS) $(XPDEV_LIBS)
