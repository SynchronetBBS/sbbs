NEED_DATEWARP=1
MAKEFLAGS	+=	-DNEED_DATEWRAP=1

SRC_ROOT	= ..
!include ${SRC_ROOT}\build\Common.bmake

CFLAGS	=	$(CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) -I../sbbs3
LDFLAGS	=	$(LDFLAGS) -tW $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)
CFLAGS  =      $(CFLAGS) -I../od6 -I../gamesdk -I../mylibs -I.
LDFLAGS =      $(LDFLAGS) -L../od6


.path.c = .;../gamesdk

$(GAC_FC): $(OBJS)
	@echo Linking $@
        ${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -e$@ $(OBJS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS) ODoors.lib
