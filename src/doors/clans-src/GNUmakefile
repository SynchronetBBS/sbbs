SRC	?=	.
override SRC := $(abspath $(SRC))/

include $(SRC)mk/Platform.gmake
include $(SRC)mk/Paths.gmake

.PHONY: game
game:	data binaries
	cp $(DOOR_BINARIES) .
	cp $(DATA_FILES) .
	cp $(SRC)release/* .
	mkdir -p outbound

.PHONY: devkit
devkit: devkit-binaries
	cp $(DEVKIT_BINARIES) .
	cp $(SRC)devkit/* .

all: game devkit

.PHONY: binaries
binaries:
	$(MAKE) SRC=$(SRC) -C $(SRC)src default

.PHONY: devkit-binaries
devkit-binaries:
	$(MAKE) SRC=$(SRC) -C $(SRC)src devkit

.PHONY: data
data:
	$(MAKE) SRC=$(SRC) -C $(SRC)data all

.PHONY: installer
installer: binaries devkit-binaries data
	mkdir -p $(SRC)stage/gum
	cd $(SRC)stage/gum && $(MAKE) SRC=$(SRC) -f $(SRC)/GNUmakefile game
	cd $(SRC)stage/gum && ls -p1 | grep -v /$ > files.lst
	echo /outbound >> $(SRC)stage/gum/files.lst
	cd $(SRC)stage/gum && $(BINDIR)chew$(NATIVEEXEFILE) ../clans.gum files.lst
	rm -rf $(SRC)stage/gum
	cp $(SRC)installer/* $(SRC)stage
	cp $(BINDIR)install$(EXEFILE) $(SRC)stage
	cp $(BINDIR)install$(WRAPEXT) $(SRC)stage
	zip -j Clans-$(os)-$(MACH)-0_97b1.zip $(SRC)stage/*

.PHONY: devkit-installer
devkit-installer: $(DEVKIT_BINARIES) $(BINDIR)chew$(NATIVEEXEFILE) $(BINDIR)install$(EXEFILE)
	mkdir -p $(SRC)stagedk/gum
	cd $(SRC)stagedk/gum && $(MAKE) SRC=$(SRC) -f $(SRC)/GNUmakefile devkit
	cd $(SRC)stagedk/gum && ls -p1 | grep -v /$ > files.lst
	cd $(SRC)stagedk/gum && $(BINDIR)chew$(NATIVEEXEFILE) ../clandev.gum files.lst
	rm -rf $(SRC)stagedk/gum
	rm -f $(SRC)stagedk/files.lst
	rm -f $(SRC)stagedk/UnixAttr.DAT
	cp $(SRC)installerdk/* $(SRC)stagedk
	cp $(BINDIR)install$(EXEFILE) $(SRC)stagedk
	zip -j ClansDevKit-$(os)-$(MACH)-0_12.zip $(SRC)stagedk/*

.PHONY: clean
clean:
	$(MAKE) SRC=$(SRC) -C $(SRC)src clean
	$(MAKE) SRC=$(SRC) -C $(SRC)data clean
	for srcfile in $(DOOR_BINARIES); do rm -f ./$$(basename $${srcfile}); done
	for srcfile in $(DEVKIT_BINARIES); do rm -f ./$$(basename $${srcfile}); done
	for srcfile in $(DATA_FILES); do rm -f ./$$(basename $${srcfile}); done
	for srcfile in $(SRC)release/*; do rm -f ./$$(basename $${srcfile}); done
	rm -rf outbound
	rm -f ClansDevKit-$(os)-$(MACH)-0_12.zip
	rm -f Clans-$(os)-$(MACH)-0_97b1.zip
	rm -rf $(SRC)stage
	rm -rf $(SRC)stagedk

.PHONY: deepclean
deepclean:
	$(MAKE) SRC=$(SRC) -C $(SRC)src deepclean
	$(MAKE) SRC=$(SRC) -C $(SRC)data clean
	for srcfile in $(DOOR_BINARIES); do rm -f ./$$(basename $${srcfile}); done
	for srcfile in $(DEVKIT_BINARIES); do rm -f ./$$(basename $${srcfile}); done
	for srcfile in $(DATA_FILES); do rm -f ./$$(basename $${srcfile}); done
	for srcfile in $(SRC)release/*; do rm -f ./$$(basename $${srcfile}); done
	rm -rf outbound
	rm -f ClansDevKit-$(os)-$(MACH)-0_12.zip
	rm -f Clans-$(os)-$(MACH)-0_97b1.zip
	rm -rf $(SRC)stage
	rm -rf $(SRC)stagedk
