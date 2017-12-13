UEDIT	=	$(EXEODIR)$(DIRSEP)uedit$(EXEFILE)

all: smblib uifc-mt ciolib-mt xpdev-mt $(MTOBJODIR) $(EXEODIR) $(UEDIT)

ifdef SBBSEXEC
.PHONY: install
install:
	install $(EXEODIR)/* $(SBBSEXEC)

.PHONY: symlinks
symlinks:
	ln -sfr $(EXEODIR)/* $(SBBSEXEC)
endif
$(UEDIT):	$(SMBLIB) $(UIFCLIB-MT) $(CIOLIB-MT) $(XPDEV-MT_LIB)
