all: clans.pak

clans.pak: stats.hlp strategy.hlp village.hlp war.hlp wizard.hlp \
    quests.e eva.e church.e pray.e secret.e npcquote.q \
    event.mon output.mon npc.mon clans.npc \
    items.dat spells.dat races.dat classes.dat \
    schemes.txt strings.xl list.asc pxtit.asc pg.asc pxnews.asc
	../tools/makepak clans.pak pak.lst

event.mon: eventmon.txt
	../tools/mcomp eventmon.txt event.mon

strings.xl: strings.txt
	../tools/langcomp strings.txt

output.mon: monsters.txt
	../tools/mcomp monsters.txt output.mon

spells.dat: spells.txt
	../tools/mspells spells.txt spells.dat

classes.dat: classes.txt
	../tools/mclass

races.dat: races.txt
	../tools/mclass

items.dat: items.txt
	../tools/mitems items.txt items.dat

clans.npc: npcs.txt
	../tools/makenpc npcs.txt clans.npc

pray.e: pray.evt
	../tools/ecomp pray.evt pray.e

eva.e: eva.txt
	../tools/ecomp eva.txt eva.e

quests.e: quests.evt
	../tools/ecomp quests.evt quests.e

church.e: church.evt
	../tools/ecomp church.evt church.e

secret.e: secret.evt
	../tools/ecomp secret.evt secret.e

npcquote.q: npcquote.txt
	../tools/ecomp npcquote.txt npcquote.q

npc.mon: npc-pc.txt
	../tools/mcomp npc-pc.txt npc.mon

clean:
	rm *.q *.xl *.dat *.e *.mon
