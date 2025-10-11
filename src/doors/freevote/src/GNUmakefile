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

SRC_ROOT        = ../../..
include ${SRC_ROOT}/build/Common.gmake

# OpenDoors
LDFLAGS	+=	-L$(SRC_ROOT)/odoors/libs-$(OS)
CFLAGS	+=	-I$(SRC_ROOT)/odoors -Wall -pedantic $(XPDEV_CFLAGS)

OBJS	:=	$(OBJODIR)$(DIRSEP)dirwrap$(OFILE) \
		$(OBJODIR)$(DIRSEP)filewrap$(OFILE) \
		$(OBJODIR)$(DIRSEP)freevote$(OFILE) \
		$(OBJODIR)$(DIRSEP)genwrap$(OFILE)

FREEVOTE :=	$(EXEODIR)$(DIRSEP)freevote$(EXEFILE)
FREEVOTE_DST :=	..$(DIRSEP)freevote.$(machine)$(EXEFILE)
ODOORS_LIB :=	$(SRC_ROOT)$(DIRSEP)odoors$(DIRSEP)libs-${OS}$(DIRSEP)$(LIBPREFIX)ODoors$(LIBFILE)

vpath %.c ../../../xpdev

all: $(FREEVOTE)

ifdef RELEASE
all: $(FREEVOTE_DST)
endif

$(FREEVOTE): $(OBJS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CXX) $(LDFLAGS) $(OBJS) $(ODOORS_LIB) -o $@

$(FREEVOTE_DST): $(FREEVOTE)
	@echo Copying release $@
	$(QUIET)cp -f $(FREEVOTE) $@

fv-clean:
ifdef RELEASE
	rm -f freevote.$(machine)
endif

clean: fv-clean
