# External paths we need... default is "what's on Deuce's system"
SRC	?=	..

include $(SRC)/mk/Platform.gmake
include $(SRC)/mk/Paths.gmake
NATIVEEXEFILE :=$(shell env - PATH="$(PATH)" $(MAKE) -C$(SRC)/mk native-exefile | awk '/^NATIVE EXEFILE:/ {print $$3}')

ODOORS ?= $(SRC)odoors/
override ODOORS := $(abspath $(ODOORS))/
CFLAGS	+=	-I$(ODOORS)
LDFLAGS	+=	"-L$(ODOORS)libs-$(OS)"
ODOORS_LIB ?=	$(ODOORS)libs-$(OS)/libODoors.a
CLANS_LIBS +=	$(ODOORS_LIB)
LDFLAGS	+=	-L$(ODOORS)libs-$(OS)

ifdef WIN
 # We won't be using the DLL
 CFLAGS	+=	-DOD_WIN32_STATIC
 CLANS_LIBS +=	-lws2_32 -lgdi32 -lcomctl32
 CLANS_LDFLAGS+=-mwindows $(ODOORS)libs-$(OS)/ODoor.res
else
 CFLAGS	+=	-D_POSIX_C_SOURCE=200809L
endif

#
# See convert.txt for more suggestions.

CLANS_OBJS :=	$(OBJDIR)alliance.o $(OBJDIR)alliancem.o \
		$(OBJDIR)clans.o $(OBJDIR)clansini.o $(OBJDIR)class.o \
		$(OBJDIR)deserialize.o $(OBJDIR)door.o $(OBJDIR)empire.o \
		$(OBJDIR)fight.o $(OBJDIR)game.o $(OBJDIR)help.o \
		$(OBJDIR)ibbs.o $(OBJDIR)input.o $(OBJDIR)items.o \
		$(OBJDIR)language.o $(OBJDIR)mail.o $(OBJDIR)maint.o \
		$(OBJDIR)menus.o $(OBJDIR)menus2.o $(OBJDIR)misc.o \
		$(OBJDIR)myibbs.o $(OBJDIR)myopen.o $(OBJDIR)news.o \
		$(OBJDIR)npc.o $(OBJDIR)platform.o $(OBJDIR)parsing.o \
		$(OBJDIR)pawn.o $(OBJDIR)quests.o $(OBJDIR)random.o \
		$(OBJDIR)readcfg.o $(OBJDIR)reg.o $(OBJDIR)scores.o \
		$(OBJDIR)semfile.o $(OBJDIR)serialize.o \
		$(OBJDIR)spells.o $(OBJDIR)system.o $(OBJDIR)trades.o \
		$(OBJDIR)user.o $(OBJDIR)video.o $(OBJDIR)village.o \
		$(OBJDIR)voting.o

CHEW_OBJS :=	$(OBJDIR)chew.o $(OBJDIR)gum.o $(OBJDIR)parsing.o \
		$(OBJDIR)platform.o

CONFIG_OBJS :=	$(OBJDIR)config.o $(OBJDIR)console.o \
		$(OBJDIR)misc.o $(OBJDIR)parsing.o $(OBJDIR)platform.o \
		$(OBJDIR)readcfg.o $(OBJDIR)video.o

ECOMP_OBJS :=	$(OBJDIR)ecomp.o $(OBJDIR)parsing.o $(OBJDIR)platform.o \
		$(OBJDIR)serialize.o $(OBJDIR)u8cp437.o

INSTALL_OBJS :=	$(OBJDIR)install.o  $(OBJDIR)console.o \
		$(OBJDIR)gum.o $(OBJDIR)parsing.o \
		$(OBJDIR)platform.o $(OBJDIR)video.o

LANGCOMP_OBJS := $(OBJDIR)langcomp.o $(OBJDIR)platform.o \
		$(OBJDIR)serialize.o $(OBJDIR)u8cp437.o

MAKENPC_OBJS :=	$(OBJDIR)makenpc.o $(OBJDIR)parsing.o \
		$(OBJDIR)platform.o $(OBJDIR)serialize.o \
		$(OBJDIR)u8cp437.o

MAKEPAK_OBJS :=	$(OBJDIR)makepak.o $(OBJDIR)parsing.o \
		$(OBJDIR)platform.o $(OBJDIR)serialize.o $(OBJDIR)u8cp437.o

MCLASS_OBJS :=	$(OBJDIR)mclass.o $(OBJDIR)platform.o \
		$(OBJDIR)serialize.o $(OBJDIR)u8cp437.o

MCOMP_OBJS :=	$(OBJDIR)mcomp.o $(OBJDIR)platform.o \
		$(OBJDIR)serialize.o $(OBJDIR)tools.o $(OBJDIR)u8cp437.o

MITEMS_OBJS :=	$(OBJDIR)mitems.o $(OBJDIR)platform.o \
		$(OBJDIR)serialize.o $(OBJDIR)tools.o $(OBJDIR)u8cp437.o

MSPELLS_OBJS :=	$(OBJDIR)mspells.o $(OBJDIR)platform.o \
		$(OBJDIR)serialize.o $(OBJDIR)tools.o $(OBJDIR)u8cp437.o

QTEST_OBJS := $(OBJDIR)qtest.o $(OBJDIR)clansini.o $(OBJDIR)console.o \
		$(OBJDIR)deserialize.o $(OBJDIR)help.o \
		$(OBJDIR)input.o $(OBJDIR)language.o $(OBJDIR)misc.o \
		$(OBJDIR)myopen.o $(OBJDIR)npc.o $(OBJDIR)parsing.o \
		$(OBJDIR)platform.o $(OBJDIR)quests.o \
		$(OBJDIR)serialize.o $(OBJDIR)video.o

PCEDIT_OBJS :=	$(OBJDIR)pcedit.o $(OBJDIR)alliance.o \
		$(OBJDIR)console.o $(OBJDIR)deserialize.o \
		$(OBJDIR)myopen.o $(OBJDIR)platform.o \
		$(OBJDIR)serialize.o $(OBJDIR)video.o

RESET_OBJS :=	$(OBJDIR)reset.o $(OBJDIR)alliance.o \
		$(OBJDIR)console.o $(OBJDIR)deserialize.o \
		$(OBJDIR)misc.o $(OBJDIR)myopen.o $(OBJDIR)platform.o \
		$(OBJDIR)parsing.o $(OBJDIR)random.o $(OBJDIR)readcfg.o \
		$(OBJDIR)serialize.o $(OBJDIR)video.o

# Default target
default: mstrings.h $(DOOR_BINARIES) $(INSTALL_BINARIES)

# Implicit rules
$(OBJDIR)%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Pull in generated deps
-include $(OBJDIR)*.d

# When cross-compiling, build just the native langcomp using the host compiler.
# env - strips the CC override from MAKEFLAGS so the sub-make uses native GCC.
ifndef NATIVE
$(BINDIR)langcomp$(NATIVEEXEFILE): langcomp.c platform.c serialize.c u8cp437.c
	env - PATH="$(PATH)" $(MAKE) SRC=$(SRC) $(BINDIR)langcomp$(NATIVEEXEFILE)
endif

# Generate mstrings.h by calling the native langcomp directly.
# Delegating to data/GNUmakefile caused a recursive-make cycle:
#   src default → mstrings.h → data strings.xl → native-devkit → src devkit → loop.
mstrings.h: $(DATADIR)strings.u8.txt $(BINDIR)langcomp$(NATIVEEXEFILE)
	$(BINDIR)langcomp$(NATIVEEXEFILE) $(DATADIR)strings.u8.txt $(DATADIR)strings.xl $@

# Ensure mstrings.h exists before compiling any source that uses it.
# Objects shared with the langcomp build (serialize.o, platform.o) are
# excluded via filter-out to avoid a circular dependency.
$(filter-out $(LANGCOMP_OBJS), \
    $(CLANS_OBJS) $(CONFIG_OBJS) $(PCEDIT_OBJS) \
    $(RESET_OBJS) $(QTEST_OBJS) $(INSTALL_OBJS)): | mstrings.h

.PHONY: devkit
devkit: $(DEVKIT_BINARIES) $(INSTALL_BINARIES)

.PHONY: all
all: default devkit

.PHONY: native-exefile
native-exefile:
	@echo NATIVE EXEFILE: $(EXEFILE)

.PHONY: test
test:
	$(MAKE) -C tests test
	$(MAKE) -C tests clean

deepclean:
	-rm -rf $(OBJPREFIX)
	-rm -rf $(BINDIR)

clean:
	-rm -rf $(OBJDIR)
	-rm -f $(DOOR_BINARIES)
	-rm -f $(DEVKIT_BINARIES)
	-rm -f $(INSTALL_BINARIES)

$(OBJDIR):
	mkdir -p $@

$(BINDIR):
	mkdir -p $@

$(ODOORS_LIB):
	$(MAKE) -C $(ODOORS) static-lib

$(BINDIR)clans$(EXEFILE): $(CLANS_OBJS) $(ODOORS_LIB) | $(BINDIR)
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

$(BINDIR)qtest$(EXEFILE): $(QTEST_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(QTEST_OBJS) -o $@ $(WRAPPER_LIBS)

$(BINDIR)qtest$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)qtest$(EXEFILE)
	cp $^ $@

$(BINDIR)pcedit$(EXEFILE): $(PCEDIT_OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(WRAPPER_LIBS)

$(BINDIR)pcedit$(WRAPEXT): wrapper$(WRAPEXT) | $(BINDIR)pcedit$(EXEFILE)
	cp $^ $@
