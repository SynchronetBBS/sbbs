# External paths we need... default is "what's on Deuce's system"
ODOORS ?= ../../../odoors/
override ODOORS := $(abspath $(ODOORS))/
SRC	?=	..

include $(SRC)/mk/Platform.gmake
include $(SRC)/mk/Paths.gmake

CFLAGS	+=	-I$(ODOORS)
LDFLAGS	+=	"-L$(ODOORS)libs-$(OS)"
CLANS_LIBS +=	$(ODOORS)libs-$(OS)/libODoors.a

LDFLAGS	+=	-L$(ODOORS)libs-$(OS)

# Some platform-specific things...
ifdef UNIXY
 # NetBSD and Darwin don't define this themselves
 CFLAGS	+=	-D__unix__
endif

ifdef WIN
 # We won't be using the DLL
 CFLAGS	+=	-DOD_WIN32_STATIC
 CLANS_LIBS +=	-lws2_32 -lgdi32 -lcomctl32
 CLANS_LDFLAGS+=-mwindows $(ODOORS)libs-$(OS)/ODoor.res
else
 CFLAGS	+=	-D_POSIX_C_SOURCE=200809L
endif

# Build types, debug and opt
ifdef DEBUG
 CFLAGS	+=	-g -O0 -Wall -pedantic
 LDFLAGS +=	-g
else
 CFLAGS	+=	-O3 -DNDEBUG
endif

CFLAGS	+=	-std=c11 -MMD -MP

#
# See convert.txt for more suggestions.

CLANS_OBJS :=	$(OBJDIR)alliance.o $(OBJDIR)alliancem.o \
		$(OBJDIR)clans.o $(OBJDIR)clansini.o $(OBJDIR)class.o \
		$(OBJDIR)cmdline.o $(OBJDIR)crc.o $(OBJDIR)deserialize.o \
		$(OBJDIR)door.o $(OBJDIR)empire.o $(OBJDIR)fight.o \
		$(OBJDIR)game.o $(OBJDIR)help.o $(OBJDIR)ibbs.o \
		$(OBJDIR)input.o $(OBJDIR)items.o $(OBJDIR)language.o \
		$(OBJDIR)mail.o $(OBJDIR)maint.o $(OBJDIR)menus.o \
		$(OBJDIR)menus2.o $(OBJDIR)misc.o $(OBJDIR)myibbs.o \
		$(OBJDIR)myopen.o $(OBJDIR)news.o $(OBJDIR)npc.o \
		$(OBJDIR)parsing.o $(OBJDIR)pawn.o $(OBJDIR)quests.o \
		$(OBJDIR)random.o $(OBJDIR)readcfg.o $(OBJDIR)reg.o \
		$(OBJDIR)scores.o $(OBJDIR)semfile.o \
		$(OBJDIR)serialize.o $(OBJDIR)spells.o $(OBJDIR)system.o \
		$(OBJDIR)trades.o $(OBJDIR)unix_wrappers.o \
		$(OBJDIR)user.o $(OBJDIR)video.o $(OBJDIR)village.o \
		$(OBJDIR)voting.o $(OBJDIR)wb_fapnd.o \
		$(OBJDIR)win_wrappers.o

CHEW_OBJS :=	$(OBJDIR)chew.o $(OBJDIR)gum.o $(OBJDIR)parsing.o \
		$(OBJDIR)win_wrappers.o

CONFIG_OBJS :=	$(OBJDIR)config.o $(OBJDIR)cmdline.o $(OBJDIR)console.o \
		$(OBJDIR)parsing.o $(OBJDIR)readcfg.o \
		$(OBJDIR)unix_wrappers.o $(OBJDIR)video.o \
		$(OBJDIR)win_wrappers.o

ECOMP_OBJS :=	$(OBJDIR)ecomp.o $(OBJDIR)parsing.o $(OBJDIR)serialize.o \
		$(OBJDIR)crc.o $(OBJDIR)win_wrappers.o

INSTALL_OBJS :=	$(OBJDIR)install.o $(OBJDIR)cmdline.o $(OBJDIR)console.o \
		$(OBJDIR)gum.o $(OBJDIR)parsing.o \
		$(OBJDIR)unix_wrappers.o $(OBJDIR)video.o \
		$(OBJDIR)win_wrappers.o

LANGCOMP_OBJS := $(OBJDIR)langcomp.o $(OBJDIR)serialize.o $(OBJDIR)crc.o \
		$(OBJDIR)win_wrappers.o

MAKENPC_OBJS :=	$(OBJDIR)makenpc.o $(OBJDIR)parsing.o \
		$(OBJDIR)serialize.o $(OBJDIR)crc.o \
		$(OBJDIR)win_wrappers.o

MAKEPAK_OBJS :=	$(OBJDIR)makepak.o $(OBJDIR)parsing.o \
		$(OBJDIR)serialize.o $(OBJDIR)crc.o \
		$(OBJDIR)win_wrappers.o

MCLASS_OBJS :=	$(OBJDIR)mclass.o $(OBJDIR)serialize.o $(OBJDIR)crc.o \
		$(OBJDIR)win_wrappers.o

MCOMP_OBJS :=	$(OBJDIR)mcomp.o $(OBJDIR)serialize.o $(OBJDIR)crc.o \
		$(OBJDIR)win_wrappers.o

MITEMS_OBJS :=	$(OBJDIR)mitems.o $(OBJDIR)serialize.o $(OBJDIR)crc.o \
		$(OBJDIR)win_wrappers.o

MSPELLS_OBJS :=	$(OBJDIR)mspells.o $(OBJDIR)serialize.o $(OBJDIR)crc.o \
		$(OBJDIR)win_wrappers.o

PCEDIT_OBJS :=	$(OBJDIR)pcedit.o $(OBJDIR)alliance.o $(OBJDIR)myopen.o \
		$(OBJDIR)unix_wrappers.o $(OBJDIR)serialize.o \
		$(OBJDIR)deserialize.o $(OBJDIR)crc.o \
		$(OBJDIR)win_wrappers.o

RESET_OBJS :=	$(OBJDIR)reset.o $(OBJDIR)alliance.o $(OBJDIR)cmdline.o \
		$(OBJDIR)console.o $(OBJDIR)crc.o $(OBJDIR)deserialize.o \
		$(OBJDIR)myopen.o $(OBJDIR)parsing.o $(OBJDIR)random.o \
		$(OBJDIR)readcfg.o $(OBJDIR)serialize.o \
		$(OBJDIR)unix_wrappers.o $(OBJDIR)video.o \
		$(OBJDIR)win_wrappers.o

# Default target
default: $(DOOR_BINARIES) $(INSTALL_BINARIES)

# Implicit rules
$(OBJDIR)%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Pull in generated deps
-include $(OBJDIR)*.d

.PHONY: devkit
devkit: $(DEVKIT_BINARIES) $(INSTALL_BINARIES)

.PHONY: all
all: default devkit

.PHONY: native-exefile
native-exefile:
	@echo NATIVE EXEFILE: $(EXEFILE)

deepclean:
	rm -rf $(OBJPREFIX)
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
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)config$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)config$(EXEFILE)
	cp $^ $@

$(BINDIR)ecomp$(EXEFILE): $(ECOMP_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)ecomp$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)ecomp$(EXEFILE)
	cp $^ $@

$(BINDIR)install$(EXEFILE): $(INSTALL_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)install$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)install$(EXEFILE)
	cp $^ $@

$(BINDIR)langcomp$(EXEFILE): $(LANGCOMP_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)langcomp$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)langcomp$(EXEFILE)
	cp $^ $@

$(BINDIR)reset$(EXEFILE): $(RESET_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

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
