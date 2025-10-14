# CC = tcc
# CFLAGS = -ml -wall -C
# DELETE = del
# EXEFILE = .exe

gcc_machine := $(findstring mingw32,$(shell ${CC} -dumpmachine))
gcc_w64     := $(findstring w64,$(shell ${CC} -dumpmachine))
gcc_x86_64  := $(findstring x86_64,$(shell ${CC} -dumpmachine))
ifeq ($(gcc_machine),mingw32)
 CFLAGS +=  -DMSVCRT_VERSION=0x0800
 ifeq ($(gcc_x86_64),x86_64)
  OS            := Win64
  WIN           := 64
  CFLAGS        += -m64
  LDFLAGS       += -m64
  MACH		:= amd64
 else
  OS            := Win32
  WIN		:= 32
  CFLAGS        += -m32
  LDFLAGS       += -m32
  WINDRESFLAGS  += -Fpe-i386
  MACH		:= x86
 endif
 ifeq ($(gcc_w64),w64)
  CFLAGS += -DDISABLE_MKSTEMP_DEFINE
 endif
else
 OS	:=	$(shell uname)
 MACH	:=	$(shell uname -m | tr "[A-Z ]" "[a-z_]")
endif
os	:=	$(shell echo $(OS)| tr "[A-Z ]" "[a-z_]")

ifeq ($(os),netbsd)
CFLAGS	+=	-D__unix__
endif
# Somebody make this compile under gcc. :-)  I'll get you started:
CC ?= gcc
ODOORS ?= ../../../odoors
CFLAGS += -std=c99 -O2 -Wall -pedantic -I$(ODOORS) -DOD_WIN32_STATIC
ifdef PROFILE
LIBS += $(ODOORS)/libs-$(OS)/libODoors_p.a
else
LIBS += $(ODOORS)/libs-$(OS)/libODoors.a
endif
LDFLAGS += -L$(ODOORS)/libs-$(OS)
ifdef PROFILE
CFLAGS	+=	-pg
endif
ifdef DEBUG
CFLAGS	+=	-g
endif
# CFLAGS = -O -Wall
DELETE = rm -f
EXEFILE := .${os}.${MACH}
ifdef WIN
 EXEFILE := .${os}.${MACH}.exe
 LIBS	+=	-lws2_32 -lgdi32 -lcomctl32
endif

ifdef WIN
 CONSOLE_LIBS :=
else
 CONSOLE_LIBS := -lcurses
endif

#
# See convert.txt for more suggestions.

OBJS = alliance.o clans.o clansini.o class.o crc.o \
	deserialize.o door.o empire.o fight.o game.o \
	help.o ibbs.o input.o items.o language.o \
	mail.o maint.o menus.o menus2.o misc.o \
	myibbs.o myopen.o news.o npc.o parsing.o \
	pawn.o quests.o reg.o scores.o serialize.o \
	spells.o system.o trades.o user.o video.o \
	village.o voting.o wb_fapnd.o

ifdef WIN
 OBJS	+=	cmdline.o
endif

#all: clans.exe reset.exe pcedit.exe langcomp.exe mcomp.exe
all: clans$(EXEFILE) langcomp$(EXEFILE) mcomp$(EXEFILE) reset$(EXEFILE) pcedit$(EXEFILE)

clean:
	$(DELETE) *.o
	$(DELETE) clans$(EXEFILE)
	$(DELETE) langcomp$(EXEFILE)
	$(DELETE) mcomp$(EXEFILE)
	$(DELETE) reset$(EXEFILE)
	$(DELETE) pcedit$(EXEFILE)

clans$(EXEFILE):      $(OBJS) unix_wrappers.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) unix_wrappers.o -o clans$(EXEFILE) $(LIBS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

langcomp$(EXEFILE):      langcomp.c
	$(CC) $(CFLAGS) $(LDFLAGS) langcomp.c -o langcomp$(EXEFILE)

reset$(EXEFILE): reset.c myopen.h parsing.h crc.h structs.h k_config.h unix_wrappers.o serialize.o deserialize.o
	$(CC) $(CFLAGS) $(LDFLAGS) reset.c myopen.o parsing.o crc.o unix_wrappers.o serialize.o deserialize.o -o reset$(EXEFILE) $(CONSOLE_LIBS)


mcomp$(EXEFILE):      mcomp.c
	$(CC) $(CFLAGS) $(LDFLAGS) mcomp.c -o mcomp$(EXEFILE)

pcedit$(EXEFILE):      pcedit.c myopen.o unix_wrappers.o myopen.h unix_wrappers.h serialize.o deserialize.o
	$(CC) $(CFLAGS) $(LDFLAGS) pcedit.c myopen.o unix_wrappers.o serialize.o deserialize.o -o pcedit$(EXEFILE)

