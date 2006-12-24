SRC_ROOT        = ..\..
!include ${SRC_ROOT}\build\Common.bmake

CFLAGS  =       $(CFLAGS) -I.. $(SMBLIB_CFLAGS) $(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
LDFLAGS =       $(LDFLAGS) $(SMBLIB_LDFLAGS) $(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)

.path.c = .;..

$(UEDIT): $(OBJS)
        @echo Linking $@
        ${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -e$@ $(OBJS) $(SMBLIB_LIBS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)
