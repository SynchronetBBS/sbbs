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
ifeq ($(os),darwin)
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

DATA_DIR	?=	../data

#all: clans.exe reset.exe pcedit.exe langcomp.exe mcomp.exe
all: clans$(EXEFILE) pcedit$(EXEFILE) reset$(EXEFILE)

.PHONY: devkit
devkit: chew$(EXEFILE) ecomp$(EXEFILE) install$(EXEFILE) langcomp$(EXEFILE) makenpc$(EXEFILE) makepak$(EXEFILE) mcomp$(EXEFILE) mclass$(EXEFILE) mitems$(EXEFILE) mspells$(EXEFILE)

.PHONY: data
data: $(DATA_DIR)/clans.pak

clean:
	$(DELETE) *.o
	$(DELETE) chew$(EXEFILE)
	$(DELETE) ecomp$(EXEFILE)
	$(DELETE) clans$(EXEFILE)
	$(DELETE) install$(EXEFILE)
	$(DELETE) langcomp$(EXEFILE)
	$(DELETE) makenpc$(EXEFILE)
	$(DELETE) makepak$(EXEFILE)
	$(DELETE) mcomp$(EXEFILE)
	$(DELETE) mclass$(EXEFILE)
	$(DELETE) mitems$(EXEFILE)
	$(DELETE) mspells$(EXEFILE)
	$(DELETE) reset$(EXEFILE)
	$(DELETE) pcedit$(EXEFILE)
	$(DELETE) $(DATA_DIR)/church.e
	$(DELETE) $(DATA_DIR)/clans.npc
	$(DELETE) $(DATA_DIR)/clans.pak
	$(DELETE) $(DATA_DIR)/classes.dat
	$(DELETE) $(DATA_DIR)/eva.e
	$(DELETE) $(DATA_DIR)/event.mon
	$(DELETE) $(DATA_DIR)/items.dat
	$(DELETE) $(DATA_DIR)/npc.mon
	$(DELETE) $(DATA_DIR)/npcquote.q
	$(DELETE) $(DATA_DIR)/output.mon
	$(DELETE) $(DATA_DIR)/pray.e
	$(DELETE) $(DATA_DIR)/quests.e
	$(DELETE) $(DATA_DIR)/races.dat
	$(DELETE) $(DATA_DIR)/secret.e
	$(DELETE) $(DATA_DIR)/spells.dat
	$(DELETE) $(DATA_DIR)/strings.xl

clans$(EXEFILE):      $(OBJS) unix_wrappers.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) unix_wrappers.o -o clans$(EXEFILE) $(LIBS)

%.e: %.evt ecomp$(EXEFILE)
	./ecomp$(EXEFILE) $< $@

%.q: %.txt ecomp$(EXEFILE)
	./ecomp$(EXEFILE) $< $@

%.xl: %.txt langcomp$(EXEFILE)
	./langcomp$(EXEFILE) $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

chew$(EXEFILE):      chew.c
	$(CC) $(CFLAGS) $(LDFLAGS) chew.c -o $@

ecomp$(EXEFILE):      ecomp.c serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) ecomp.c serialize.o crc.o -o $@

install$(EXEFILE):      install.c
	$(CC) $(CFLAGS) $(LDFLAGS) install.c -o $@ $(CONSOLE_LIBS)

langcomp$(EXEFILE):      langcomp.c serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) langcomp.c serialize.o crc.o -o $@

reset$(EXEFILE): reset.c myopen.h parsing.h crc.h structs.h k_config.h unix_wrappers.o serialize.o deserialize.o
	$(CC) $(CFLAGS) $(LDFLAGS) reset.c myopen.o parsing.o crc.o unix_wrappers.o serialize.o deserialize.o -o $@ $(CONSOLE_LIBS)

makenpc$(EXEFILE):      makenpc.c serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) makenpc.c serialize.o crc.o -o $@

makepak$(EXEFILE):      makepak.c serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) makepak.c serialize.o crc.o -o $@

mcomp$(EXEFILE):      mcomp.c serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) mcomp.c serialize.o crc.o -o $@

mclass$(EXEFILE):      mclass.c serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) mclass.c serialize.o crc.o -o $@

mitems$(EXEFILE):      mitems.c serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) mitems.c serialize.o crc.o -o $@

mspells$(EXEFILE):      mspells.c serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) mspells.c serialize.o crc.o -o $@

pcedit$(EXEFILE):      pcedit.c myopen.o unix_wrappers.o myopen.h unix_wrappers.h serialize.o deserialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) pcedit.c myopen.o unix_wrappers.o serialize.o deserialize.o crc.o -o $@

$(DATA_DIR)/event.mon: $(DATA_DIR)/eventmon.txt mcomp$(EXEFILE)
	./mcomp$(EXEFILE) $(DATA_DIR)/eventmon.txt $@

$(DATA_DIR)/output.mon: $(DATA_DIR)/monsters.txt mcomp$(EXEFILE)
	./mcomp$(EXEFILE) $(DATA_DIR)/monsters.txt $@

$(DATA_DIR)/npc.mon: $(DATA_DIR)/npc-pc.txt mcomp$(EXEFILE)
	./mcomp$(EXEFILE) $(DATA_DIR)/npc-pc.txt $@

$(DATA_DIR)/spells.dat: $(DATA_DIR)/spells.txt mspells$(EXEFILE)
	./mspells$(EXEFILE) $(DATA_DIR)/spells.txt $@

$(DATA_DIR)/items.dat: $(DATA_DIR)/items.txt mitems$(EXEFILE)
	./mitems$(EXEFILE) $(DATA_DIR)/items.txt $@

$(DATA_DIR)/classes.dat: $(DATA_DIR)/races.dat

$(DATA_DIR)/races.dat: $(DATA_DIR)/classes.txt $(DATA_DIR)/races.txt mclass$(EXEFILE)
	cd $(DATA_DIR) && ../src/mclass$(EXEFILE)

$(DATA_DIR)/clans.npc: $(DATA_DIR)/npcs.txt makenpc$(EXEFILE)
	./makenpc$(EXEFILE) $(DATA_DIR)/npcs.txt $@

$(DATA_DIR)/clans.pak: $(DATA_DIR)/menus.hlp $(DATA_DIR)/bulletin.hlp \
		$(DATA_DIR)/citizen.hlp $(DATA_DIR)/clans.hlp \
		$(DATA_DIR)/combat.hlp $(DATA_DIR)/empire.hlp \
		$(DATA_DIR)/army.hlp $(DATA_DIR)/items.hlp \
		$(DATA_DIR)/newbie.hlp $(DATA_DIR)/races.hlp \
		$(DATA_DIR)/ruler.hlp $(DATA_DIR)/spells.hlp \
		$(DATA_DIR)/stats.hlp $(DATA_DIR)/strategy.hlp \
		$(DATA_DIR)/village.hlp $(DATA_DIR)/war.hlp \
		$(DATA_DIR)/wizard.hlp $(DATA_DIR)/quests.e \
		$(DATA_DIR)/eva.e $(DATA_DIR)/church.e \
		$(DATA_DIR)/pray.e $(DATA_DIR)/npcquote.q \
		$(DATA_DIR)/event.mon $(DATA_DIR)/output.mon \
		$(DATA_DIR)/npc.mon $(DATA_DIR)/secret.e \
		$(DATA_DIR)/clans.npc $(DATA_DIR)/items.dat \
		$(DATA_DIR)/spells.dat $(DATA_DIR)/races.dat \
		$(DATA_DIR)/classes.dat $(DATA_DIR)/schemes.txt \
		$(DATA_DIR)/strings.xl $(DATA_DIR)/list.asc \
		$(DATA_DIR)/pxtit.asc $(DATA_DIR)/pg.asc \
		$(DATA_DIR)/pxnews.asc makepak$(EXEFILE)
	cd $(DATA_DIR) && ../src/makepak$(EXEFILE) clans.pak pak.lst
