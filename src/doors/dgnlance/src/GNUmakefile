# Get OS name
gcc_machine := $(findstring mingw32,$(shell ${CC} -dumpmachine))
gcc_w64     := $(findstring w64,$(shell ${CC} -dumpmachine))
gcc_x86_64  := $(findstring x86_64,$(shell ${CC} -dumpmachine))
ifeq ($(gcc_machine),mingw32)
 win	:= yes
 CFLAGS +=      -DMSVCRT_VERSION=0x0800
 ifeq ($(gcc_x86_64),x86_64)
  OS            := Win64
 else
  OS            := Win32
 endif
else
 OS      :=      $(shell uname)
endif

SRC_ROOT        = ../../..
include ${SRC_ROOT}/build/Common.gmake

# OpenDoors
LDFLAGS	+=	-L$(SRC_ROOT)/odoors/libs-$(OS)
CFLAGS	+=	-I$(SRC_ROOT)/odoors -Wall -pedantic

ifdef RELEASE
all: $(DGNLNCE_DST)
endif

ifdef win
OBJS += $(SRC_ROOT)/odoors/libs-$(OS)/ODoor.res
endif

$(DGNLNCE): $(OBJS) $(ODOORS_LIB)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(OBJS) ${ODOORS_LIB} ${LIB_RPATH} -o $@

$(DGNLNCE_DST): $(DGNLNCE)
	${QUIET}cp -f $(DGNLNCE) $(DGNLNCE_DST)

.PHONY: dstclean
dstclean:
ifdef RELEASE
	${DELETE} ${DGNLNCE_DST}
endif

clean: dstclean
