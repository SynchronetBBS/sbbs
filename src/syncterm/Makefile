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

!ifdef USE_WXWIDGETS
 !ifndef WXWIDGETS_DIR
  WXWIDGETS_DIR = C:\wxWidgets-2.8.4
 !endif
 !ifndef WXWIDGETS_CFG
  WXWIDGETS_CFG=syncterm
 !endif
 CFLAGS = $(CFLAGS) -DWITH_WXWIDGETS -I$(WXWIDGETS_DIR)\include -I$(WXWIDGETS_DIR)\lib\bcc_lib$(WXWIDGETS_CFG)\mswu
 LDFLAGS = $(LDFLAGS)  -L$(WXWIDGETS_DIR)\lib\bcc_lib$(WXWIDGETS_CFG) 
 WXLIBS = $(WXLIBS) wxbase28.lib wxbase28_net.lib wxjpeg.lib wxmsw28_core.lib wxmsw28_html.lib wxpng.lib wxtiff.lib wxzlib.lib
 OBJS = $(OBJS) $(MTOBJODIR)$(DIRSEP)htmlwin$(OFILE)
!endif

$(SYNCTERM): $(OBJS)
	@echo Linking $@
        ${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -e$@ $(OBJS) $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(WXLIBS) $(XPDEV-MT_LIBS)
