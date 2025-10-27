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
	cd $(SRC)stage/gum && ls -p1 | grep -v gum/ > ../files.lst
	cd $(SRC)stage/gum && $(BINDIR)chew$(NATIVEEXEFILE) ../clans.gum ../files.lst
	rm -rf $(SRC)stage/gum
	rm -f $(SRC)stage/files.lst
	cp $(SRC)installer/* $(SRC)stage
	cp $(BINDIR)install$(EXEFILE) $(SRC)stage
	cp $(BINDIR)install$(WRAPEXT) $(SRC)stage
	zip -j Clans-$(os)-$(MACH)-0_97b1.zip ../stage/*

.PHONY: devkit-installer
devkit-installer: $(DEVKIT_BINARIES) native-chew$(NATIVEEXEFILE) install$(EXEFILE)
	mkdir -p $(SRC)stage/gum
	cd $(SRC)stage/gum && $(MAKE) SRC=$(SRC) -f $(SRC)/GNUmakefile devkit
	cd $(SRC)stage/gum && ls -p1 | grep -v gum/ > ../files.lst
	cd $(SRC)stage/gum && $(BINDIR)chew$(NATIVEEXEFILE) ../clans.gum ../files.lst
	rm -rf ../stagedk/gum
	rm -f ../stagedk/files.lst
	rm -f ../stagedk/UnixAttr.DAT
	cp ../installerdk/* ../stagedk
	cp install$(EXEFILE) ../stagedk
	zip -j ClansDevKit-$(os)-$(MACH)-0_12.zip ../stagedk/*

.PHONY: clean
clean:
