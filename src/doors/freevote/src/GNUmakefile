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

all:

WITHOUT_CRYPTLIB	:= please
SRC_ROOT        = ../../..
include ${SRC_ROOT}/build/Common.gmake

FREEVOTE :=	$(EXEODIR)$(DIRSEP)freevote$(EXEFILE)
FREEVOTE_DST :=	..$(DIRSEP)freevote.$(machine)$(EXEFILE)
ODOORS_LIB :=	$(SRC_ROOT)$(DIRSEP)odoors$(DIRSEP)libs-${OS}$(DIRSEP)$(LIBPREFIX)ODoors$(LIBFILE)

LDFLAGS	+=	-L$(SRC_ROOT)/odoors/libs-$(OS) $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS) $(MT_LDFLAGS)
CFLAGS	+=	-DOD_WIN32_STATIC -I$(SRC_ROOT)/odoors -Wall -pedantic $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
LIBS	+=	 $(ODOORS_LIB) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)
ifdef win
 LIBS	+=	-lcomctl32
endif

OBJS	:=	$(OBJODIR)$(DIRSEP)freevote$(OFILE)
ifdef win
 OBJS  +=      $(SRC_ROOT)/odoors/libs-$(OS)/ODoor.res
endif

vpath %.c ../../../xpdev

all: $(FREEVOTE)

ifdef RELEASE
all: $(FREEVOTE_DST)
endif

$(FREEVOTE): $(OBJS) $(XPDEV-MT_LIB) $(CIOLIB-MT) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CXX) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(FREEVOTE_DST): $(FREEVOTE)
	@echo Copying release $@
	$(QUIET)cp -f $(FREEVOTE) $@

fv-clean:
ifdef RELEASE
	rm -f freevote.$(machine)
endif

clean: fv-clean
