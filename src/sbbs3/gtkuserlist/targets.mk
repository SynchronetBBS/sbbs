GTKUSERLIST	=	$(EXEODIR)$(DIRSEP)gtkuserlist$(EXEFILE)

all: xpdev-mt $(MTOBJODIR) $(EXEODIR) $(GTKUSERLIST)

ifdef SBBSEXEC
.PHONY: install
install: all
	install $(EXEODIR)/* $(SBBSEXEC)

.PHONY: symlinks
symlinks: all
	ln -sfr $(EXEODIR)/* $(SBBSEXEC)
endif

$(GTKUSERLIST):	$(XPDEV-MT_LIB) $(SMBLIB)
