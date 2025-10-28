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
	cp $(DOOR_BINARIES) .
	cp $(DATA_FILES) .
	cp $(SRC)release/* .
	mkdir -p outbound

.PHONY: devkit
devkit: devkit-binaries
	cp $(DEVKIT_BINARIES) .
	cp $(SRC)devkit/* .

all: game installer devkit devkit-installer

.PHONY: binaries
binaries:
	$(MAKE) SRC=$(SRC) -C $(SRC)src default

.PHONY: devkit-binaries
devkit-binaries:
	$(MAKE) SRC=$(SRC) -C $(SRC)src devkit

.PHONY: data
data:
	$(MAKE) SRC=$(SRC) -C $(SRC)data all

.PHONY: native-devkit
native-devkit:
	env - PATH="$(PATH)" $(MAKE) -C $(SRC)src devkit

.PHONY: installer
installer: binaries data native-devkit
	rm -rf $(SRC)stage
	mkdir -p $(SRC)stage/gum
	cd $(SRC)stage/gum && $(MAKE) SRC=$(SRC) -f $(SRC)/GNUmakefile game
	cd $(SRC)stage/gum && ls -p1 | grep -v /$ > ../files.lst
	echo /outbound >> $(SRC)stage/files.lst
	cd $(SRC)stage/gum && $(BINDIR)chew$(NATIVEEXEFILE) ../clans.gum ../files.lst
	rm -rf $(SRC)stage/gum
	rm -f $(SRC)stage/files.lst
	cp $(SRC)installer/* $(SRC)stage/
	cp $(BINDIR)install$(EXEFILE) $(SRC)stage/
	cp $(BINDIR)install$(WRAPEXT) $(SRC)stage/
	zip -j Clans-$(os)-$(MACH)-0_97b1.zip $(SRC)stage/*

.PHONY: devkit-installer
devkit-installer: devkit-binaries native-devkit
	rm -rf $(SRC)stagedk
	mkdir -p $(SRC)stagedk/gum
	cd $(SRC)stagedk/gum && $(MAKE) SRC=$(SRC) -f $(SRC)/GNUmakefile devkit
	cd $(SRC)stagedk/gum && ls -p1 | grep -v /$ > ../files.lst
	cd $(SRC)stagedk/gum && $(BINDIR)chew$(NATIVEEXEFILE) ../clandev.gum ../files.lst
	rm -rf $(SRC)stagedk/gum
	rm -f $(SRC)stagedk/files.lst
	cp $(SRC)installerdk/* $(SRC)stagedk/
	cp $(BINDIR)install$(EXEFILE) $(SRC)stagedk/
	cp $(BINDIR)install$(WRAPEXT) $(SRC)stagedk/
	zip -j ClansDevKit-$(os)-$(MACH)-0_12.zip $(SRC)stagedk/*

.PHONY: clean
clean:
	$(MAKE) SRC=$(SRC) -C $(SRC)src clean
	$(MAKE) SRC=$(SRC) -C $(SRC)data clean
	for srcfile in $(DOOR_BINARIES); do rm -f ./$$(basename $${srcfile}); done
	for srcfile in $(DEVKIT_BINARIES); do rm -f ./$$(basename $${srcfile}); done
	for srcfile in $(DATA_FILES); do rm -f ./$$(basename $${srcfile}); done
	for srcfile in $(SRC)release/*; do rm -f ./$$(basename $${srcfile}); done
	for srcfile in $(SRC)devkit/*; do rm -f ./$$(basename $${srcfile}); done
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
	for srcfile in $(SRC)devkit/*; do rm -f ./$$(basename $${srcfile}); done
	rm -rf outbound
	rm -f ClansDevKit-$(os)-$(MACH)-0_12.zip
	rm -f Clans-$(os)-$(MACH)-0_97b1.zip
	rm -rf $(SRC)stage
	rm -rf $(SRC)stagedk

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
	rm -r $(INSTALLDIR)outbound
	rm -r $(INSTALLDIR)

.PHONY: uninstall-devkit
uninstall-devkit:
	-for srcfile in $(DEVKIT_BINARIES); do rm $(INSTALLDIR)$$(basename $${srcfile}); done
	-for srcfile in $(INSTALL_BINARIES); do rm $(INSTALLDIR)$$(basename $${srcfile}); done
	-for srcfile in $(SRC)devkit/*; do rm $(INSTALLDIR)$$(basename $${srcfile}); done
	rm -r $(INSTALLDIR)outbound
	rm -r $(INSTALLDIR)
