NEED_DATEWARP=1
MAKEFLAGS	=	$(MAKEFLAGS) -DNEED_DATEWRAP=1

SRC_ROOT	= ..\..
!include ${SRC_ROOT}\build\Common.bmake

CFLAGS	=	-ff- $(CFLAGS) $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
LDFLAGS	=	-ff- $(LDFLAGS) -tW $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)
CFLAGS  =      $(CFLAGS) -I$(SRC_ROOT)/odoors
LDFLAGS =      $(LDFLAGS) -L$(SRC_ROOT)/odoors

$(SMURF): $(OBJS)
	@echo Linking $@
        ${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) -e$@ $(OBJS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS) ODoors.lib

dist:: $(SMURF)
	-md dist
	copy /Y $(SMURF) dist
	copy /Y $(SRC_ROOT)\odoors\ODoors62.dll dist
	copy /Y *.d0* dist
	copy /Y *.doc dist
	copy /Y *.txt dist
	copy /Y install.bat dist
