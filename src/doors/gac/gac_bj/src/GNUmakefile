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

ANSI_FILES := $(wildcard ../art/*.ans)
ANSART := ..$(DIRSEP)gac_ans.art
ASCII_FILES := $(wildcard ../art/*.asc)
ASCART := ..$(DIRSEP)gac_asc.art
RIP_FILES := $(wildcard ../art/*.rip)
RIPART := ..$(DIRSEP)gac_rip.art

all: $(ANSART) $(ASCART) $(RIPART)
ifdef RELEASE
all: $(GAC_BJ_DST)
endif

$(GAC_BJ): $(OBJS) $(BUILD_DEPENDS) | $(EXEODIR)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) $(OBJS) -o $@ $(XPDEV-MT_LIBS) $(ODOORS_LIB) $(LIBS)

$(GAC_BJ_DST): $(GAC_BJ)
	@echo Copying release $@
	$(QUIET)cp -f $(GAC_BJ) $@

$(ANSART): $(ANSI_FILES) $(PACKART)
	@echo Packing $@
	$(QUIET)rm -f $(ANSART)
	$(QUIET)$(PACKART) $(ANSART) $(ANSI_FILES)

$(ASCART): $(ASCII_FILES) $(PACKART)
	@echo Packing $@
	$(QUIET)rm -f $(ASCART)
	$(QUIET)$(PACKART) $(ASCART) $(ASCII_FILES)

$(RIPART): $(RIP_FILES) $(PACKART)
	@echo Packing $@
	$(QUIET)rm -f $(RIPART)
	$(QUIET)$(PACKART) $(RIPART) $(RIP_FILES)
