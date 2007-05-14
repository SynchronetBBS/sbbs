SRC_ROOT	= ..
!include ${SRC_ROOT}\build\Common.bmake

CFLAGS	=	$(CFLAGS) $(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) -I..\sbbs3;..\smblib;..\..\include\cryptlib;..\comio
LDFLAGS	=	$(LDFLAGS) $(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)

.path.c = .;..\sbbs3;..\smblib;..\uifc;..\comio

OBJS = $(OBJS) $(MTOBJODIR)$(DIRSEP)comio_win32$(OFILE)

$(SYNCTERM): $(OBJS)
	@echo Linking $@
        ${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -e$@ $(OBJS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)
