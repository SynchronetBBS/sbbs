SRC_ROOT	= ..
!include ${SRC_ROOT}\build\Common.bmake

CFLAGS	=	$(CFLAGS) $(UIFC-MT_CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) -I..\sbbs3;..\smblib;..\..\3rdp\win32.release\cryptlib\include;..\comio
LDFLAGS	=	$(LDFLAGS) $(UIFC-MT_LDFLAGS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)

.path.c = .;..\sbbs3;..\smblib;..\uifc;..\comio

OBJS = $(OBJS) $(MTOBJODIR)$(DIRSEP)comio$(OFILE)
OBJS = $(OBJS) $(MTOBJODIR)$(DIRSEP)modem$(OFILE)
OBJS = $(OBJS) $(MTOBJODIR)$(DIRSEP)comio_win32$(OFILE) $(MTOBJODIR)$(DIRSEP)ssh$(OFILE)
!ifdef WITHOUT_OOII
 CFLAGS	= $(CFLAGS) -DWITHOUT_OOII=1
!else
 OBJS = $(OBJS) $(MTOBJODIR)$(DIRSEP)ooii$(OFILE)
 OBJS = $(OBJS) $(MTOBJODIR)$(DIRSEP)ooii_logons$(OFILE)
 OBJS = $(OBJS) $(MTOBJODIR)$(DIRSEP)ooii_cmenus$(OFILE)
 OBJS = $(OBJS) $(MTOBJODIR)$(DIRSEP)ooii_bmenus$(OFILE)
 OBJS = $(OBJS) $(MTOBJODIR)$(DIRSEP)ooii_sounds$(OFILE)
!endif

$(SYNCTERM): $(OBJS)
	@echo Linking $@
        ${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -e$@ $(OBJS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)
