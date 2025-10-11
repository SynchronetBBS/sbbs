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

SRC_ROOT        = ../..
include ${SRC_ROOT}/build/Common.gmake

# OpenDoors
LDFLAGS	+=	-L$(SRC_ROOT)/odoors/libs-$(OS)
CFLAGS	+=	-I$(SRC_ROOT)/odoors -Wall -pedantic $(XPDEV_CFLAGS)

ifeq ($(os),darwin)
 LIB_RPATH := -Wl,$(RPATH),@executable_path
else ifdef win
 LIB_RPATH :=
else
 LIB_RPATH += '-Wl,-rpath,$$ORIGIN/lib.$(machine),-z,origin,--enable-new-dtags'
endif

OBJS	:=	$(OBJODIR)$(DIRSEP)dirwrap$(OFILE) \
		$(OBJODIR)$(DIRSEP)filewrap$(OFILE) \
		$(OBJODIR)$(DIRSEP)freevote$(OFILE) \
		$(OBJODIR)$(DIRSEP)genwrap$(OFILE) \

vpath %.c ../../xpdev

all : freevote.$(machine)

freevote.$(machine): $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -lODoors $(LIB_RPATH) -o freevote.$(machine)

fv-clean:
	rm -f freevote.$(machine)

clean: fv-clean
