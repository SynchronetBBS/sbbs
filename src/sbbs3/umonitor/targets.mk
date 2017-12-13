UMONITOR	=	$(EXEODIR)$(DIRSEP)umonitor$(EXEFILE)

all: ciolib-mt uifc-mt xpdev-mt smblib $(MTOBJODIR) $(EXEODIR) $(UMONITOR)

ifdef SBBSEXEC
.PHONY: install
install:
	install $(EXEODIR)/* $(SBBSEXEC)

.PHONY: symlinks
symlinks:
	ln -sfr $(EXEODIR)/* $(SBBSEXEC)
endif

$(UMONITOR):	$(CIOLIB-MT) $(UIFCLIB-MT) $(XPDEV-MT_LIB) $(SMBLIB)
