PREFIX	?=	/opt
override PREFIX := $(abspath $(PREFIX))/
SRC	?=	.
override SRC := $(abspath $(SRC))/
INSTALL	?=	install
ifdef INSTALLDIR
 override INSTALLDIR := $(abspath $(INSTALLDIR))/
else
 override INSTALLDIR := $(abspath $(DESTDIR)$(PREFIX)clans)/
endif
INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA ?=	$(INSTALL) -m 644

include $(SRC)mk/Platform.gmake
include $(SRC)mk/Paths.gmake

.PHONY: build
build:	data binaries devkit-binaries

.PHONY: game
game:	data binaries

.PHONY: devkit
devkit: devkit-binaries

all: binaries data devkit-binaries installer devkit-installer

.PHONY: binaries
binaries:
	$(MAKE) SRC=$(SRC) -C $(SRC)src default

.PHONY: devkit-binaries
devkit-binaries:
	$(MAKE) SRC=$(SRC) -C $(SRC)src devkit

.PHONY: data
data:
	$(MAKE) SRC=$(SRC) -C $(SRC)data all

.PHONY: test
test:
	$(MAKE) SRC=$(SRC) -C $(SRC)src/tests test
	$(MAKE) SRC=$(SRC) -C $(SRC)tests test

.PHONY: native-devkit
native-devkit:
	env - PATH="$(PATH)" $(MAKE) -C $(SRC)src devkit

.PHONY: installer
installer: binaries data native-devkit
	rm -rf $(SRC)stage
	mkdir -p $(SRC)stage/gum
	cp $(DOOR_BINARIES) $(SRC)stage/gum/
	cp $(DATA_FILES) $(SRC)stage/gum/
	cp $(SRC)release/* $(SRC)stage/gum/
	cp $(SRC)LICENSE $(SRC)stage/gum/
	mkdir -p $(SRC)stage/gum/outbound
	cd $(SRC)stage/gum && ls -p1 | grep -v /$ > ../files.lst
	echo /outbound >> $(SRC)stage/files.lst
	cd $(SRC)stage/gum && $(BINDIR)chew$(NATIVEEXEFILE) ../clans.gum ../files.lst
	rm -rf $(SRC)stage/gum
	rm -f $(SRC)stage/files.lst
	cp $(SRC)installer/* $(SRC)stage/
	cp $(BINDIR)install$(EXEFILE) $(SRC)stage/
	cp $(BINDIR)install$(WRAPEXT) $(SRC)stage/
	zip -j Clans-$(os)-$(MACH)-0_97b2.zip $(SRC)stage/*

.PHONY: devkit-installer
devkit-installer: devkit-binaries native-devkit
	rm -rf $(SRC)stagedk
	mkdir -p $(SRC)stagedk/gum
	cp $(DEVKIT_BINARIES) $(SRC)stagedk/gum/
	cp $(SRC)devkit/* $(SRC)stagedk/gum/
	cp $(SRC)data/classes.txt $(SRC)stagedk/gum/
	cp $(SRC)data/items.txt $(SRC)stagedk/gum/
	cp $(SRC)data/monsters.txt $(SRC)stagedk/gum/
	cp $(SRC)data/npc-pc.txt $(SRC)stagedk/gum/
	cp $(SRC)data/npcquote.u8.txt $(SRC)stagedk/gum/
	cp $(SRC)data/races.txt $(SRC)stagedk/gum/
	cp $(SRC)data/spells.u8.txt $(SRC)stagedk/gum/
	cp $(SRC)data/clans.u8.txt $(SRC)stagedk/gum/npcs.u8.txt
	cp $(SRC)LICENSE $(SRC)stagedk/gum/
	cd $(SRC)stagedk/gum && ls -p1 | grep -v /$ > ../files.lst
	cd $(SRC)stagedk/gum && $(BINDIR)chew$(NATIVEEXEFILE) ../clandev.gum ../files.lst
	rm -rf $(SRC)stagedk/gum
	rm -f $(SRC)stagedk/files.lst
	cp $(SRC)installerdk/* $(SRC)stagedk/
	cp $(BINDIR)install$(EXEFILE) $(SRC)stagedk/
	cp $(BINDIR)install$(WRAPEXT) $(SRC)stagedk/
	zip -j ClansDevKit-$(os)-$(MACH)-0_97b2.zip $(SRC)stagedk/*

.PHONY: dist
dist:
	rm -rf clans-0.97b2
	mkdir clans-0.97b2
	git archive HEAD -- . | (cd clans-0.97b2 && tar -xf -)
	rm clans-0.97b2/odoors
	(cd ../.. && git archive HEAD -- odoors) | (cd clans-0.97b2 && tar -xf -)
	tar -cf clans-0.97b2.tar clans-0.97b2
	gzip clans-0.97b2.tar
	rm -rf clans-0.97b2

.PHONY: clean
clean:
	$(MAKE) SRC=$(SRC) -C $(SRC)src clean
	$(MAKE) SRC=$(SRC) -C $(SRC)data clean
	rm -f ClansDevKit-$(os)-$(MACH)-0_97b2.zip
	rm -f Clans-$(os)-$(MACH)-0_97b2.zip
	rm -rf $(SRC)stage
	rm -rf $(SRC)stagedk

.PHONY: deepclean
deepclean:
	$(MAKE) SRC=$(SRC) -C $(SRC)data clean
	$(MAKE) SRC=$(SRC) -C $(SRC)src/tests clean
	$(MAKE) SRC=$(SRC) -C $(SRC)src deepclean
	$(MAKE) SRC=$(SRC) -C $(SRC)tests clean
	-rm -f ClansDevKit-$(os)-$(MACH)-0_97b2.zip
	-rm -f Clans-$(os)-$(MACH)-0_97b2.zip
	-rm -f clans-0.97b2.tar.gz
	-rm -f clans-0.97b2.tar
	-rm -rf $(SRC)stage
	-rm -rf $(SRC)stagedk

.PHONY: installdirs
installdirs:
	mkdir -p $(INSTALLDIR)outbound

.PHONY: install
install: installdirs data binaries
	for srcfile in $(DOOR_BINARIES); do $(INSTALL_PROGRAM) $${srcfile} $(INSTALLDIR); done
	for srcfile in $(INSTALL_BINARIES); do $(INSTALL_PROGRAM) $${srcfile} $(INSTALLDIR); done
	for srcfile in $(DATA_FILES); do $(INSTALL_DATA) $${srcfile} $(INSTALLDIR); done
	for srcfile in $(SRC)release/*; do $(INSTALL_DATA) $${srcfile} $(INSTALLDIR); done

.PHONY: install-devkit
install-devkit: installdirs devkit-binaries
	for srcfile in $(DEVKIT_BINARIES); do $(INSTALL_PROGRAM) $${srcfile} $(INSTALLDIR); done
	for srcfile in $(INSTALL_BINARIES); do $(INSTALL_PROGRAM) $${srcfile} $(INSTALLDIR); done
	for srcfile in $(SRC)devkit/*; do $(INSTALL_DATA) $${srcfile} $(INSTALLDIR); done

.PHONY: uninstall
uninstall:
	-for srcfile in $(DOOR_BINARIES); do rm $(INSTALLDIR)$$(basename $${srcfile}); done
	-for srcfile in $(INSTALL_BINARIES); do rm $(INSTALLDIR)$$(basename $${srcfile}); done
	-for srcfile in $(DATA_FILES); do rm $(INSTALLDIR)$$(basename $${srcfile}); done
	-for srcfile in $(SRC)release/*; do rm $(INSTALLDIR)$$(basename $${srcfile}); done
	-rm -r $(INSTALLDIR)outbound
	-rm -r $(INSTALLDIR)

.PHONY: uninstall-devkit
uninstall-devkit:
	-for srcfile in $(DEVKIT_BINARIES); do rm $(INSTALLDIR)$$(basename $${srcfile}); done
	-for srcfile in $(INSTALL_BINARIES); do rm $(INSTALLDIR)$$(basename $${srcfile}); done
	-for srcfile in $(SRC)devkit/*; do rm $(INSTALLDIR)$$(basename $${srcfile}); done
	-rm -r $(INSTALLDIR)outbound
	-rm -r $(INSTALLDIR)
