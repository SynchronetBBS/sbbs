GTKMONITOR	=	$(EXEODIR)$(DIRSEP)gtkmonitor$(EXEFILE)

all: xpdev-mt smblib $(MTOBJODIR) $(EXEODIR) $(GTKMONITOR)

ifdef SBBSEXEC
.PHONY: install
install: all
	install $(EXEODIR)/* $(SBBSEXEC)

.PHONY: symlinks
symlinks: all
	ln -sfr $(EXEODIR)/* $(SBBSEXEC)
endif

$(GTKMONITOR):	$(XPDEV-MT_LIB) $(SMBLIB)
