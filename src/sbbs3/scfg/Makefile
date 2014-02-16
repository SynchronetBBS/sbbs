SRC_ROOT	= ..\..

!include ${SRC_ROOT}\build\Common.bmake

.path.c=	.;..


CFLAGS	=	-w-csu -w-aus -DSCFG -I.. $(CFLAGS) $(SMBLIB_CFLAGS) $(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
CFLAGS  =       $(CFLAGS) -I..\..\comio
LDFLAGS	=	$(LDFLAGS) $(SMBLIB_LDFLAGS) $(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)


$(SCFG): $(OBJS)
	@echo Linking $@
        ${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -e$@ $(OBJS) $(SMBLIB_LIBS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)

