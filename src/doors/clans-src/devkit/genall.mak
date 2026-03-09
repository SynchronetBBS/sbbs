# genall.mak -- Makefile equivalent of genall.bat
#
# Compiles every default data source file using the devkit tools
# and packs the results into clans.pak.
#
# Usage:  gmake -f genall.mak
#         gmake -f genall.mak LANGCOMP=/usr/local/bin/langcomp
# Clean:  gmake -f genall.mak clean

LANGCOMP = ./langcomp
MCOMP    = ./mcomp
MSPELLS  = ./mspells
MITEMS   = ./mitems
MCLASS   = ./mclass
MAKENPC  = ./makenpc
ECOMP    = ./ecomp
MAKEPAK  = ./makepak

LOG = TMP.FIL

all: clans.pak

strings.xl: strings.txt
	$(LANGCOMP) strings.txt > $(LOG)

event.mon: eventmon.txt
	$(MCOMP) eventmon.txt event.mon >> $(LOG)

output.mon: monsters.txt
	$(MCOMP) monsters.txt output.mon >> $(LOG)

spells.dat: spells.txt
	$(MSPELLS) spells.txt spells.dat >> $(LOG)

items.dat: items.txt
	$(MITEMS) items.txt items.dat >> $(LOG)

classes.cls: classes.txt
	$(MCLASS) classes.txt classes.cls >> $(LOG)

races.cls: races.txt
	$(MCLASS) races.txt races.cls >> $(LOG)

clans.npc: npcs.txt
	$(MAKENPC) npcs.txt clans.npc >> $(LOG)

pray.e: pray.evt
	$(ECOMP) pray.evt pray.e >> $(LOG)

eva.e: eva.txt
	$(ECOMP) eva.txt eva.e >> $(LOG)

quests.e: quests.evt
	$(ECOMP) quests.evt quests.e >> $(LOG)

church.e: church.evt
	$(ECOMP) church.evt church.e >> $(LOG)

secret.e: secret.evt
	$(ECOMP) secret.evt secret.e >> $(LOG)

COMPILED = strings.xl event.mon output.mon spells.dat items.dat \
           classes.cls races.cls clans.npc \
           pray.e eva.e quests.e church.e secret.e

clans.pak: $(COMPILED) pak.lst
	$(MAKEPAK) clans.pak pak.lst >> $(LOG)

clean:
	rm -f $(COMPILED) clans.pak $(LOG)

.PHONY: all clean
