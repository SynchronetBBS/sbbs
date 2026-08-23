###############################################################################
# SEXYZ (Synchronet External X/Y/ZMODEM) - stand-alone source release         #
#                                                                             #
# GNU make build for Unix-like systems (Linux, *BSD, macOS, Solaris).         #
#                                                                             #
#   make                 optimized build -> ./sexyz                           #
#   make DEBUG=1         unoptimized build with symbols                       #
#   make clean                                                                #
#                                                                             #
# For Windows/MSVC, use Makefile.vc (see COMPILING.md).                       #
###############################################################################

CC ?= cc

# Every .c in this directory, and the same list Makefile.vc builds: each
# platform-specific source compiles to nothing on the platform that does not
# want it (xpevent.c outside __unix__, xpprintf.c's vasprintf outside MSVC).
# Explicit rather than a wildcard so a stray .c in the extraction directory
# is not picked up.
SRCS = sexyz.c xmodem.c zmodem.c ringbuf.c nopen.c date_str.c telnet.c \
       crc16.c crc32.c \
       datewrap.c dirwrap.c filewrap.c genwrap.c ini_file.c netwrap.c \
       os_info.c sockwrap.c str_list.c threadwrap.c xpdatetime.c xpevent.c \
       xpprintf.c

OBJS = $(SRCS:.c=.o)

# RINGBUF_EVENT and RINGBUF_MUTEX are required: sexyz.c waits on the ring
# buffer's events and the output thread shares it with the protocol thread.
DEFS = -DSBBS_EXPORTS -DRINGBUF_EVENT -DRINGBUF_MUTEX \
       -DXPDEV_THREAD_SAFE -D_THREAD_SAFE -D_REENTRANT \
       -D_FILE_OFFSET_BITS=64 -DPREFER_POLL

LIBS = -lpthread -lm

UNAME := $(shell uname -s)

ifeq ($(UNAME),Linux)
 DEFS += -D_GNU_SOURCE -D_DEFAULT_SOURCE
 LIBS += -ldl
endif
ifeq ($(UNAME),Darwin)
 DEFS += -D__unix__ -D__DARWIN__
endif
ifeq ($(UNAME),SunOS)
 LIBS += -lsocket -lnsl
endif

ifdef DEBUG
 CFLAGS += -g -O0 -D_DEBUG
else
 CFLAGS += -O2 -DNDEBUG
endif

# GCC/Clang warning flags; override with "make WARN=" for other compilers.
WARN ?= -Wall -Wno-unused-result -Wno-unused-value

# -I. is required: a few headers use angle-bracket includes (e.g. md5.h)
CFLAGS += -I. $(WARN) $(DEFS)

sexyz: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(OBJS) sexyz

# There is deliberately no install target: copy the binary into your BBS's
# exec directory yourself (see COMPILING.md).
