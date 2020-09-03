GTKUSEREDIT	=	$(EXEODIR)$(DIRSEP)gtkuseredit$(EXEFILE)

all: xpdev-mt smblib $(MTOBJODIR) $(EXEODIR) $(GTKUSEREDIT)

ifdef SBBSEXEC
.PHONY: install
install: all
	install $(EXEODIR)/* $(SBBSEXEC)

.PHONY: symlinks
symlinks: all
	ln -sfr $(EXEODIR)/* $(SBBSEXEC)
endif

$(GTKUSEREDIT):	$(XPDEV-MT_LIB) $(SMBLIB)
