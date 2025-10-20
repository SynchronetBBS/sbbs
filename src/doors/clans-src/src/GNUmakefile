# External paths we need... default is "what's on Deuce's system"
ODOORS ?= ../../../odoors

include ../mk/Platform.gmake

CFLAGS	+=	-I$(ODOORS)
LDFLAGS	+=	"-L$(ODOORS)/libs-$(OS)"
CLANS_LIBS +=	$(ODOORS)/libs-$(OS)/libODoors.a

LDFLAGS	+=	-L$(ODOORS)/libs-$(OS)

ifeq ($(os),linux)
 WRAPPER_LIBS += -lbsd
endif

# Some platform-specific things...
ifdef UNIXY
 # NetBSD and Darwin don't define this themselves
 CFLAGS	+=	-D__unix__
 CFLAGS	+=	-DNCURSES_WIDECHAR
endif

ifdef WIN
 # We won't be using the DLL
 CFLAGS	+=	-DOD_WIN32_STATIC
 CLANS_LIBS +=	-lws2_32 -lgdi32 -lcomctl32
 CLANS_LDFLAGS+=-mwindows $(ODOORS)/libs-$(OS)/ODoor.res
 WRAPEXT :=	.cmd
else
 # Oh, the things we put up with to use curses...
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
 else ifeq ($(os),openbsd)
  CONSOLE_LIBS := -lncursesw
 else ifeq ($(os),netbsd)
  CONSOLE_LIBS := -lcurses
 else
  CONSOLE_LIBS := -lcursesw
 endif
 WRAPEXT :=
endif

# Build types, debug and opt
ifdef DEBUG
 CFLAGS	+=	-g -O0 -Wall -pedantic
 LDFLAGS +=	-g
 PPATH	?=	$(os).$(mach).debug
 FLAVOUR ?=	debug
else
 CFLAGS	+=	-O3 -DNDEBUG
 PPATH	?=	$(os).$(mach).opt
 FLAVOUR ?=	opt
endif

CFLAGS	+=	-std=c11 -MMD -MP

EXEFILE ?=	.$(os).$(mach).$(FLAVOUR)$(EXE_EXT)
OBJPREFIX ?=	../obj/
OBJDIR	?=	$(OBJPREFIX)$(os).$(mach).$(FLAVOUR)/
BINDIR	?=	../bin/

#
# See convert.txt for more suggestions.

CLANS_OBJS :=	$(OBJDIR)alliance.o $(OBJDIR)clans.o $(OBJDIR)clansini.o \
		$(OBJDIR)class.o $(OBJDIR)cmdline.o $(OBJDIR)crc.o \
		$(OBJDIR)deserialize.o $(OBJDIR)door.o $(OBJDIR)empire.o \
		$(OBJDIR)fight.o $(OBJDIR)game.o $(OBJDIR)help.o \
		$(OBJDIR)ibbs.o $(OBJDIR)input.o $(OBJDIR)items.o \
		$(OBJDIR)language.o $(OBJDIR)mail.o $(OBJDIR)maint.o \
		$(OBJDIR)menus.o $(OBJDIR)menus2.o $(OBJDIR)misc.o \
		$(OBJDIR)myibbs.o $(OBJDIR)myopen.o $(OBJDIR)news.o \
		$(OBJDIR)npc.o $(OBJDIR)parsing.o $(OBJDIR)pawn.o \
		$(OBJDIR)quests.o $(OBJDIR)reg.o $(OBJDIR)scores.o \
		$(OBJDIR)serialize.o $(OBJDIR)spells.o $(OBJDIR)system.o \
		$(OBJDIR)trades.o $(OBJDIR)unix_wrappers.o \
		$(OBJDIR)user.o $(OBJDIR)video.o $(OBJDIR)village.o \
		$(OBJDIR)voting.o $(OBJDIR)wb_fapnd.o

CHEW_OBJS :=	$(OBJDIR)chew.o $(OBJDIR)gum.o $(OBJDIR)parsing.o

CONFIG_OBJS :=	$(OBJDIR)config.o $(OBJDIR)parsing.o \
		$(OBJDIR)unix_wrappers.o

ECOMP_OBJS :=	$(OBJDIR)ecomp.o $(OBJDIR)parsing.o $(OBJDIR)serialize.o \
		$(OBJDIR)crc.o

INSTALL_OBJS :=	$(OBJDIR)install.o $(OBJDIR)gum.o $(OBJDIR)parsing.o

LANGCOMP_OBJS := $(OBJDIR)langcomp.o $(OBJDIR)serialize.o $(OBJDIR)crc.o

MAKENPC_OBJS :=	$(OBJDIR)makenpc.o $(OBJDIR)parsing.o \
		$(OBJDIR)serialize.o $(OBJDIR)crc.o

MAKEPAK_OBJS :=	$(OBJDIR)makepak.o $(OBJDIR)parsing.o \
		$(OBJDIR)serialize.o $(OBJDIR)crc.o

MCLASS_OBJS :=	$(OBJDIR)mclass.o $(OBJDIR)serialize.o $(OBJDIR)crc.o

MCOMP_OBJS :=	$(OBJDIR)mcomp.o $(OBJDIR)serialize.o $(OBJDIR)crc.o

MITEMS_OBJS :=	$(OBJDIR)mitems.o $(OBJDIR)serialize.o $(OBJDIR)crc.o

MSPELLS_OBJS :=	$(OBJDIR)mspells.o $(OBJDIR)serialize.o $(OBJDIR)crc.o

PCEDIT_OBJS :=	$(OBJDIR)pcedit.o $(OBJDIR)myopen.o \
		$(OBJDIR)unix_wrappers.o $(OBJDIR)serialize.o \
		$(OBJDIR)deserialize.o $(OBJDIR)crc.o

RESET_OBJS :=	$(OBJDIR)reset.o $(OBJDIR)unix_wrappers.o \
		$(OBJDIR)serialize.o $(OBJDIR)deserialize.o \
		$(OBJDIR)crc.o $(OBJDIR)parsing.o $(OBJDIR)myopen.o

ifdef WIN
 CLANS_OBJS +=	$(OBJDIR)win_wrappers.o
 CHEW_OBJS +=	$(OBJDIR)win_wrappers.o
 CONFIG_OBJS +=	$(OBJDIR)win_wrappers.o
 ECOMP_OBJS +=	$(OBJDIR)win_wrappers.o
 INSTALL_OBJS += $(OBJDIR)win_wrappers.o
 LANGCOMP_OBJS += $(OBJDIR)win_wrappers.o
 MAKENPC_OBJS += $(OBJDIR)win_wrappers.o
 MAKEPAK_OBJS += $(OBJDIR)win_wrappers.o
 MCLASS_OBJS +=	$(OBJDIR)win_wrappers.o
 MCOMP_OBJS +=	$(OBJDIR)win_wrappers.o
 MITEMS_OBJS +=	$(OBJDIR)win_wrappers.o
 MSPELLS_OBJS += $(OBJDIR)win_wrappers.o
 PCEDIT_OBJS +=	$(OBJDIR)win_wrappers.o
 RESET_OBJS +=	$(OBJDIR)win_wrappers.o
endif

DOOR_BINARIES	:=	$(BINDIR)clans$(EXEFILE) \
			$(BINDIR)config$(EXEFILE) \
			$(BINDIR)pcedit$(EXEFILE) \
			$(BINDIR)reset$(EXEFILE) \
			$(BINDIR)clans$(WRAPEXT) \
			$(BINDIR)config$(WRAPEXT) \
			$(BINDIR)pcedit$(WRAPEXT) \
			$(BINDIR)reset$(WRAPEXT)

DEVKIT_BINARIES	:=	$(BINDIR)chew$(EXEFILE) $(BINDIR)ecomp$(EXEFILE) \
			$(BINDIR)install$(EXEFILE) \
			$(BINDIR)langcomp$(EXEFILE) \
			$(BINDIR)makenpc$(EXEFILE) \
			$(BINDIR)makepak$(EXEFILE) \
			$(BINDIR)mcomp$(EXEFILE) \
			$(BINDIR)mclass$(EXEFILE) \
			$(BINDIR)mitems$(EXEFILE) \
			$(BINDIR)mspells$(EXEFILE) \
			$(BINDIR)chew$(WRAPEXT) $(BINDIR)ecomp$(WRAPEXT) \
			$(BINDIR)install$(WRAPEXT) \
			$(BINDIR)langcomp$(WRAPEXT) \
			$(BINDIR)makenpc$(WRAPEXT) \
			$(BINDIR)makepak$(WRAPEXT) \
			$(BINDIR)mcomp$(WRAPEXT) \
			$(BINDIR)mclass$(WRAPEXT) \
			$(BINDIR)mitems$(WRAPEXT) \
			$(BINDIR)mspells$(WRAPEXT)

# Default target
default: $(DOOR_BINARIES)

# Implicit rules
$(OBJDIR)%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Pull in generated deps
-include $(OBJDIR)*.d

.PHONY: devkit
devkit: $(DEVKIT_BINARIES)

.PHONY: all
all: default devkit

.PHONY: native-exefile
native-exefile:
	@echo NATIVE EXEFILE: $(EXEFILE)

deepclean:
	rm -rf $(OBJPREFIX)*
	rm -rf $(BINDIR)

clean:
	rm -rf $(OBJDIR)
	rm -f $(DOOR_BINARIES)
	rm -f $(DEVKIT_BINARIES)

$(OBJDIR):
	mkdir -p $@

$(BINDIR):
	mkdir -p $@

$(BINDIR)clans$(EXEFILE): $(CLANS_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(CLANS_LDFLAGS) $(CLANS_OBJS) -o $@ $(CLANS_LIBS) $(WRAPPER_LIBS)

$(BINDIR)clans$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)clans$(EXEFILE)
	cp $^ $@

$(BINDIR)chew$(EXEFILE): $(CHEW_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)chew$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)chew$(EXEFILE)
	cp $^ $@

$(BINDIR)config$(EXEFILE): $(CONFIG_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(CONSOLE_LIBS) $(WRAPPER_LIBS)

$(BINDIR)config$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)config$(EXEFILE)
	cp $^ $@

$(BINDIR)ecomp$(EXEFILE): $(ECOMP_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)ecomp$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)ecomp$(EXEFILE)
	cp $^ $@

$(BINDIR)install$(EXEFILE): $(INSTALL_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(CONSOLE_LIBS) $(WRAPPER_LIBS)

$(BINDIR)install$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)install$(EXEFILE)
	cp $^ $@

$(BINDIR)langcomp$(EXEFILE): $(LANGCOMP_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)langcomp$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)langcomp$(EXEFILE)
	cp $^ $@

$(BINDIR)reset$(EXEFILE): $(RESET_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(CONSOLE_LIBS) $(WRAPPER_LIBS)

$(BINDIR)reset$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)reset$(EXEFILE)
	cp $^ $@

$(BINDIR)makenpc$(EXEFILE): $(MAKENPC_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)makenpc$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)makenpc$(EXEFILE)
	cp $^ $@

$(BINDIR)makepak$(EXEFILE): $(MAKEPAK_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)makepak$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)makepak$(EXEFILE)
	cp $^ $@

$(BINDIR)mcomp$(EXEFILE): $(MCOMP_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)mcomp$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)mcomp$(EXEFILE)
	cp $^ $@

$(BINDIR)mclass$(EXEFILE): $(MCLASS_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)mclass$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)mclass$(EXEFILE)
	cp $^ $@

$(BINDIR)mitems$(EXEFILE): $(MITEMS_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)mitems$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)mitems$(EXEFILE)
	cp $^ $@

$(BINDIR)mspells$(EXEFILE): $(MSPELLS_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)mspells$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)mspells$(EXEFILE)
	cp $^ $@

$(BINDIR)pcedit$(EXEFILE): $(PCEDIT_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)pcedit$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)pcedit$(EXEFILE)
	cp $^ $@
