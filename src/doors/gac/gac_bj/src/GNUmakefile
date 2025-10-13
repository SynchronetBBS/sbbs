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
PACKART_PATH := ..$(DIRSEP)..$(DIRSEP)gamesdk$(DIRSEP)$(shell env - PATH="$(PATH)" $(MAKE) -C ../../gamesdk RELEASE=1 packart-path | awk '/^PACKART PATH:/ {print $$3}')
DEBUG := $(shell echo $(PACKART_PATH))

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

.PHONY: $(PACKART_PATH)
$(PACKART_PATH):
	env - PATH="$(PATH)" $(MAKE) -C ../../gamesdk RELEASE=1

$(ANSART): $(ANSI_FILES) $(PACKART_PATH)
	@echo Packing $@
	$(QUIET)rm -f $(ANSART)
	$(QUIET)$(PACKART_PATH) $(ANSART) $(ANSI_FILES)

$(ASCART): $(ASCII_FILES) $(PACKART_PATH)
	@echo Packing $@
	$(QUIET)rm -f $(ASCART)
	$(QUIET)$(PACKART_PATH) $(ASCART) $(ASCII_FILES)

$(RIPART): $(RIP_FILES) $(PACKART_PATH)
	@echo Packing $@
	$(QUIET)rm -f $(RIPART)
	$(QUIET)$(PACKART_PATH) $(RIPART) $(RIP_FILES)

.PHONY: relclean
relclean:
	rm -f $(ANSART) $(ASCART) $(RIPART) $(GAC_BJ_DST)

clean: relclean
