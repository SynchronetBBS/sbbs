BINDIR	?=	../bin/
PAKFILES :=	$(shell awk '{print $$1}' pak.lst)

pak: clans.pak

.PHONY: native-devkit
native-devkit:
	env - PATH="$(PATH)" $(MAKE) -C ../src devkit

%.e: %.evt | native-devkit
	$(BINDIR)ecomp $< $@

%.q: %.txt | native-devkit
	$(BINDIR)ecomp $< $@

%.xl: %.txt | native-devkit
	$(BINDIR)langcomp $< $@

%.mon: %.txt | native-devkit
	$(BINDIR)mcomp $< $@

%.spl: %.txt | native-devkit
	$(BINDIR)mspells $< $@

%.itm: %.txt | native-devkit
	$(BINDIR)mitems $< $@

%.cls: %.txt | native-devkit
	$(BINDIR)mclass $< $@

%.npc: %.txt | native-devkit
	$(BINDIR)makenpc $< $@

clans.pak: pak.lst $(PAKFILES)
	$(BINDIR)makepak $@ $<

clean:
	rm -f *.e *.q *.xl *.mon *.spl *.itm *.cls *.npc clans.pak
