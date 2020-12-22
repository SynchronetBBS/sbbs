SEXPOTS		=	$(EXEODIR)$(DIRSEP)sexpots$(EXEFILE)

all: xpdev-mt $(MTOBJODIR) $(EXEODIR) $(SEXPOTS)

ifdef SBBSEXEC
.PHONY: install
install: all
	install $(EXEODIR)/* $(SBBSEXEC)

.PHONY: symlinks
symlinks: all
	ln -sfr $(EXEODIR)/* $(SBBSEXEC)
endif

$(SEXPOTS):	$(XPDEV-MT_LIB)
