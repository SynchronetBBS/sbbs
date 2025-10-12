GAMESDK_ROOT	:=	../../gamesdk
SRC_ROOT	:=	../../../..
include ${SRC_ROOT}/build/Common.gmake
include ${GAMESDK_ROOT}/Common.gmake

CFLAGS	+=	-DOD_WIN32_STATIC $(XPDEV-MT_CFLAGS)
CFLAGS	+=	-I$(SRC_ROOT)/odoors -I../../gamesdk -I.
LDFLAGS	+=	$(XPDEV-MT_LDFLAGS) $(MT_LDFLAGS)
LIBS :=		 $(XPDEV-MT_LIBS)
ifdef win
 LIBS	+=	-lgdi32 -lcomctl32
endif

vpath %.c ../../gamesdk

ifdef RELEASE
all: $(GAC_BJ_DST) $(ENCRYPT_DST)
endif

$(GAC_BJ): $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) $(OBJS) -o $@ $(XPDEV-MT_LIBS) $(ODOORS_LIB) $(LIBS)

$(GAC_BJ_DST): $(GAC_BJ)
	@echo Copying release $@
	$(QUIET)cp -f $(GAC_BJ) $@

$(ENCRYPT_DST): $(ENCRYPT)
	@echo Copying release $@
	$(QUIET)cp -f $(ENCRYPT) $@
