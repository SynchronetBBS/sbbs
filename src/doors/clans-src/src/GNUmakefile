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

ifeq ($(OS),$(shell uname))
 ifeq ($(MACH),$(shell uname -m | tr "[A-Z ]" "[a-z_]"))
  NATIVE	:= 1
 endif
endif

ifeq ($(os),netbsd)
CFLAGS	+=	-D__unix__
endif
ifeq ($(os),darwin)
CFLAGS	+=	-D__unix__
endif
ifdef win
CFLAGS += -DOD_WIN32_STATIC
endif
# Somebody make this compile under gcc. :-)  I'll get you started:
CC ?= gcc
ODOORS ?= ../../../odoors
CFLAGS += -std=c99 -O2 -Wall -pedantic -I$(ODOORS)
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
DELETEDIR = rm -rf
EXEFILE :=
ifdef WIN
 EXEFILE :=	.exe
 LIBS	+=	-lws2_32 -lgdi32 -lcomctl32
else
 CFLAGS	+=	-DNCURSES_WIDECHAR
endif

ifdef WIN
 CONSOLE_LIBS :=
else
 ifeq ($(os),darwin)
  CONSOLE_LIBS := -lcurses
 else ifeq ($(os),linux)
  ifeq ($(shell pkg-config ncursesw --exists && echo YES), YES)
   CONSOLE_LIBS += $(shell pkg-config ncursesw --libs)
   CFLAGS += $(shell pkg-config ncursesw --cflags)
  else
   # This probobly won't work
   CONSOLE_LIBS := -lncursesw
  endif
 else
  CONSOLE_LIBS := -lcursesw
 endif
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
DOOR_BINARIES	:=	clans$(EXEFILE) config$(EXEFILE) pcedit$(EXEFILE) reset$(EXEFILE)
DEVKIT_BINARIES	:=	chew$(EXEFILE) ecomp$(EXEFILE) install$(EXEFILE) \
			langcomp$(EXEFILE) makenpc$(EXEFILE) makepak$(EXEFILE) \
			mcomp$(EXEFILE) mclass$(EXEFILE) mitems$(EXEFILE) \
			mspells$(EXEFILE)

ifdef NATIVE
NATIVEEXEFILE	:=	$(EXEFILE)
else
NATIVEEXEFILE	:=	$(shell env - PATH="$(PATH)" $(MAKE) NATIVE=1 native-exefile$(NATIVEEXEFILE) | awk '/^NATIVE EXEFILE: / {print $$3}')
endif

#all: clans.exe reset.exe pcedit.exe langcomp.exe mcomp.exe
all: $(DOOR_BINARIES)

.PHONY: devkit
devkit: $(DEVKIT_BINARIES)

.PHONY: data
data: $(DATA_DIR)/clans.pak

.PHONY: installer
installer: $(DOOR_BINARIES) data native-chew$(NATIVEEXEFILE) install$(EXEFILE)
	mkdir -p ../stage/gum
	cp ../release/* ../stage/gum
	cp $(DATA_DIR)/clans.pak ../stage/gum
	cp $(DOOR_BINARIES) ../stage/gum
	cd ../stage/gum && ls -1 > ../files.lst
	cd ../stage/gum && ../../src/native-chew$(NATIVEEXEFILE) ../clans.gum ../files.lst
	echo /Outbound > ../files.lst
	rm -rf ../stage/gum
	rm -f ../stage/files.lst
	cp ../installer/* ../stage
	cp install$(EXEFILE) ../stage
	zip -j Clans-$(os)-$(MACH)-0_97b1.zip ../stage/*

.PHONY: devkit-installer
devkit-installer: $(DEVKIT_BINARIES) native-chew$(NATIVEEXEFILE) install$(EXEFILE)
	mkdir ../stagedk
	mkdir ../stagedk/gum
	cp ../devkit/* ../stagedk/gum
	cp $(DEVKIT_BINARIES) ../stagedk/gum
	rm ../stagedk/gum/install$(EXEFILE)
	cp $(DATA_DIR)/monsters.txt ../stagedk/gum
	cp $(DATA_DIR)/npc-pc.txt ../stagedk/gum/npc-mon.txt
	cp $(DATA_DIR)/npcquote.txt ../stagedk/gum
	cp $(DATA_DIR)/npcs.txt ../stagedk/gum
	cd ../stagedk/gum && ls -1 > ../files.lst
	cd ../stagedk/gum && ../../src/native-chew$(NATIVEEXEFILE) ../clandev.gum ../files.lst
	rm -rf ../stagedk/gum
	rm -f ../stagedk/files.lst
	rm -f ../stagedk/UnixAttr.DAT
	cp ../installerdk/* ../stagedk
	cp install$(EXEFILE) ../stagedk
	zip -j ClansDevKit-$(os)-$(MACH)-0_12.zip ../stagedk/*

clean:
	$(DELETE) *.o
	$(DELETE) config$(EXEFILE)
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
	$(DELETE) native-*
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
	$(DELETEDIR) ../stage
	$(DELETEDIR) ../stagedk
	$(DELETE) Clans-*.zip
	$(DELETE) ClansDevKit-*.zip

clans$(EXEFILE):      $(OBJS) unix_wrappers.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) unix_wrappers.o -o clans$(EXEFILE) $(LIBS) $(CONSOLE_LIBS)

%.e: %.evt native-ecomp$(NATIVEEXEFILE)
	./native-ecomp$(NATIVEEXEFILE) $< $@

%.q: %.txt native-ecomp$(NATIVEEXEFILE)
	./native-ecomp$(NATIVEEXEFILE) $< $@

%.xl: %.txt native-langcomp$(NATIVEEXEFILE)
	./native-langcomp$(NATIVEEXEFILE) $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

chew$(EXEFILE):      chew.c gum.o parsing.o
	$(CC) $(CFLAGS) $(LDFLAGS) chew.c gum.o parsing.o -o $@

native-chew$(NATIVEEXEFILE):      chew.c gum.c parsing.c
ifdef NATIVE
	$(CC) $(CFLAGS) $(LDFLAGS) chew.c gum.c parsing.c -o $@
else
	env - PATH="$(PATH)" $(MAKE) NATIVE=1 RELEASE=1 $@
endif

.PHONY: native-exefile
native-exefile:
	@echo NATIVE EXEFILE: $(EXEFILE)

config$(EXEFILE):      config.c parsing.o unix_wrappers.o
	$(CC) $(CFLAGS) $(LDFLAGS) config.c parsing.o unix_wrappers.o -o $@ $(CONSOLE_LIBS)

ecomp$(EXEFILE):      ecomp.c parsing.o serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) ecomp.c parsing.o serialize.o crc.o -o $@

native-ecomp$(NATIVEEXEFILE):      ecomp.c parsing.c serialize.c crc.c
ifdef NATIVE
	$(CC) $(CFLAGS) $(LDFLAGS) ecomp.c parsing.c serialize.c crc.c -o $@
else
	env - PATH="$(PATH)" $(MAKE) NATIVE=1 RELEASE=1 $@
endif

install$(EXEFILE):      install.c gum.o parsing.o
	$(CC) $(CFLAGS) $(LDFLAGS) install.c gum.o parsing.o -o $@ $(CONSOLE_LIBS)

langcomp$(EXEFILE):      langcomp.c serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) langcomp.c serialize.o crc.o -o $@

native-langcomp$(NATIVEEXEFILE):      langcomp.c serialize.c crc.c
ifdef NATIVE
	$(CC) $(CFLAGS) $(LDFLAGS) langcomp.c serialize.c crc.c -o $@
else
	env - PATH="$(PATH)" $(MAKE) NATIVE=1 RELEASE=1 $@
endif

reset$(EXEFILE): reset.c myopen.h parsing.h crc.h structs.h k_config.h unix_wrappers.o serialize.o deserialize.o
	$(CC) $(CFLAGS) $(LDFLAGS) reset.c myopen.o parsing.o crc.o unix_wrappers.o serialize.o deserialize.o -o $@ $(CONSOLE_LIBS)

makenpc$(EXEFILE):      makenpc.c parsing.o serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) makenpc.c parsing.o serialize.o crc.o -o $@

native-makenpc$(NATIVEEXEFILE):      makenpc.c parsing.c serialize.c crc.c
ifdef NATIVE
	$(CC) $(CFLAGS) $(LDFLAGS) makenpc.c parsing.c serialize.c crc.c -o $@
else
	env - PATH="$(PATH)" $(MAKE) NATIVE=1 RELEASE=1 $@
endif

makepak$(EXEFILE):      makepak.c parsing.o serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) makepak.c parsing.o serialize.o crc.o -o $@

native-makepak$(NATIVEEXEFILE):      makepak.c parsing.c serialize.c crc.c
ifdef NATIVE
	$(CC) $(CFLAGS) $(LDFLAGS) makepak.c parsing.c serialize.c crc.c -o $@
else
	env - PATH="$(PATH)" $(MAKE) NATIVE=1 RELEASE=1 $@
endif

mcomp$(EXEFILE):      mcomp.c serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) mcomp.c serialize.o crc.o -o $@

native-mcomp$(NATIVEEXEFILE):      mcomp.c serialize.c crc.c
ifdef NATIVE
	$(CC) $(CFLAGS) $(LDFLAGS) mcomp.c serialize.c crc.c -o $@
else
	env - PATH="$(PATH)" $(MAKE) NATIVE=1 RELEASE=1 $@
endif

mclass$(EXEFILE):      mclass.c serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) mclass.c serialize.o crc.o -o $@

native-mclass$(NATIVEEXEFILE):      mclass.c serialize.c crc.c
ifdef NATIVE
	$(CC) $(CFLAGS) $(LDFLAGS) mclass.c serialize.c crc.c -o $@
else
	env - PATH="$(PATH)" $(MAKE) NATIVE=1 RELEASE=1 $@
endif

mitems$(EXEFILE):      mitems.c serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) mitems.c serialize.o crc.o -o $@

native-mitems$(NATIVEEXEFILE):      mitems.c serialize.c crc.c
ifdef NATIVE
	$(CC) $(CFLAGS) $(LDFLAGS) mitems.c serialize.c crc.c -o $@
else
	env - PATH="$(PATH)" $(MAKE) NATIVE=1 RELEASE=1 $@
endif

mspells$(EXEFILE):      mspells.c serialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) mspells.c serialize.o crc.o -o $@

native-mspells$(NATIVEEXEFILE):      mspells.c serialize.c crc.c
ifdef NATIVE
	$(CC) $(CFLAGS) $(LDFLAGS) mspells.c serialize.c crc.c -o $@
else
	env - PATH="$(PATH)" $(MAKE) NATIVE=1 RELEASE=1 $@
endif

pcedit$(EXEFILE):      pcedit.c myopen.o unix_wrappers.o myopen.h unix_wrappers.h serialize.o deserialize.o crc.o
	$(CC) $(CFLAGS) $(LDFLAGS) pcedit.c myopen.o unix_wrappers.o serialize.o deserialize.o crc.o -o $@

$(DATA_DIR)/event.mon: $(DATA_DIR)/eventmon.txt native-mcomp$(NATIVEEXEFILE)
	./native-mcomp$(NATIVEEXEFILE) $(DATA_DIR)/eventmon.txt $@

$(DATA_DIR)/output.mon: $(DATA_DIR)/monsters.txt native-mcomp$(NATIVEEXEFILE)
	./native-mcomp$(NATIVEEXEFILE) $(DATA_DIR)/monsters.txt $@

$(DATA_DIR)/npc.mon: $(DATA_DIR)/npc-pc.txt native-mcomp$(NATIVEEXEFILE)
	./native-mcomp$(NATIVEEXEFILE) $(DATA_DIR)/npc-pc.txt $@

$(DATA_DIR)/spells.dat: $(DATA_DIR)/spells.txt native-mspells$(NATIVEEXEFILE)
	./native-mspells$(NATIVEEXEFILE) $(DATA_DIR)/spells.txt $@

$(DATA_DIR)/items.dat: $(DATA_DIR)/items.txt native-mitems$(NATIVEEXEFILE)
	./native-mitems$(NATIVEEXEFILE) $(DATA_DIR)/items.txt $@

$(DATA_DIR)/classes.dat: $(DATA_DIR)/races.dat

$(DATA_DIR)/races.dat: $(DATA_DIR)/classes.txt $(DATA_DIR)/races.txt native-mclass$(NATIVEEXEFILE)
	cd $(DATA_DIR) && ../src/native-mclass$(NATIVEEXEFILE)

$(DATA_DIR)/clans.npc: $(DATA_DIR)/npcs.txt native-makenpc$(NATIVEEXEFILE)
	./native-makenpc$(NATIVEEXEFILE) $(DATA_DIR)/npcs.txt $@

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
		$(DATA_DIR)/pxnews.asc native-makepak$(NATIVEEXEFILE)
	cd $(DATA_DIR) && ../src/native-makepak$(NATIVEEXEFILE) clans.pak pak.lst
