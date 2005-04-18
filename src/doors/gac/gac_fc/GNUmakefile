NEED_DATEWRAP	:=	1
MAKE	+=	NEED_DATEWRAP=1

ifdef FIXME
 CFLAGS	+=	-Wshadow -Wconversion -Wredundant-decls
endif

SRC_ROOT	:=	../../..
include ${SRC_ROOT}/build/Common.gmake

CFLAGS	+=	$(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
LDFLAGS	+=	$(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)
CFLAGS	+=	-I$(SRC_ROOT)/odoors -I../gamesdk -I../mylibs -I.
LDFLAGS	+=	-L$(SRC_ROOT)/odoors/libs-`uname`

vpath %.c ../gamesdk

$(GAC_FC): $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) $(OBJS) -o $@ $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS) -lODoors
