NEED_DATEWARP=1
MAKEFLAGS	=	$(MAKEFLAGS) -DNEED_DATEWRAP=1
EXE_SUFFIX	=	32

SRC_ROOT	= ..\..\..
!include ${SRC_ROOT}\build\Common.bmake

CFLAGS	=	$(CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) -I../sbbs3
LDFLAGS	=	$(LDFLAGS) -tW $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)
CFLAGS  =      $(CFLAGS) -I$(SRC_ROOT)/odoors -I../gamesdk -I.
LDFLAGS =      $(LDFLAGS) -L$(SRC_ROOT)/odoors

.path.c = .;../gamesdk

$(GAC_FC): $(OBJS)
	@echo Linking $@
        ${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -e$@ $(OBJS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS) ODoors.lib
