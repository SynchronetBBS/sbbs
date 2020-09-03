GTKCHAT	=	$(EXEODIR)$(DIRSEP)gtkchat$(EXEFILE)

all: xpdev-mt smblib $(MTOBJODIR) $(EXEODIR) $(GTKCHAT)

ifdef SBBSEXEC
.PHONY: install
install: all
	install $(EXEODIR)/* $(SBBSEXEC)

.PHONY: symlinks
symlinks: all
	ln -sfr $(EXEODIR)/* $(SBBSEXEC)
endif

$(GTKCHAT):	$(XPDEV-MT_LIB) $(SMBLIB)
