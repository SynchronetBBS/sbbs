SRC	?=	..
override SRC := $(abspath $(SRC))/
include $(SRC)/mk/Platform.gmake
include $(SRC)/mk/Paths.gmake

PAKFILES :=	$(shell awk '{print $$1}' pak.lst)
NATIVEEXEFILE :=$(shell env - PATH="$(PATH)" $(MAKE) -C ../src native-exefile | awk '/^NATIVE EXEFILE:/ {print $$3}')

pak: clans.pak

.PHONY: all
all: pak

.PHONY: native-devkit
native-devkit:
	env - PATH="$(PATH)" $(MAKE) -C ../src devkit

%.e: %.evt $(BINDIR)ecomp $(BINDIR)ecomp$(NATIVEEXEFILE) | native-devkit
	$(BINDIR)ecomp $< $@

%.q: %.txt $(BINDIR)ecomp $(BINDIR)ecomp$(NATIVEEXEFILE) | native-devkit
	$(BINDIR)ecomp $< $@

%.xl: %.txt $(BINDIR)langcomp $(BINDIR)langcomp$(NATIVEEXEFILE) | native-devkit
	$(BINDIR)langcomp $< $@

%.mon: %.txt $(BINDIR)mcomp $(BINDIR)mcomp$(NATIVEEXEFILE) | native-devkit
	$(BINDIR)mcomp $< $@

%.spl: %.txt $(BINDIR)mspells $(BINDIR)mspells$(NATIVEEXEFILE) | native-devkit
	$(BINDIR)mspells $< $@

%.itm: %.txt $(BINDIR)mitems $(BINDIR)mitems$(NATIVEEXEFILE) | native-devkit
	$(BINDIR)mitems $< $@

%.cls: %.txt $(BINDIR)mclass $(BINDIR)mclass$(NATIVEEXEFILE) | native-devkit
	$(BINDIR)mclass $< $@

%.npc: %.txt $(BINDIR)makenpc $(BINDIR)makenpc$(NATIVEEXEFILE) | native-devkit
	$(BINDIR)makenpc $< $@

clans.pak: pak.lst $(PAKFILES) $(BINDIR)makepak$(NATIVEEXEFILE)
	$(BINDIR)makepak $@ $<

clean:
	rm -f *.e *.q *.xl *.mon *.spl *.itm *.cls *.npc clans.pak
