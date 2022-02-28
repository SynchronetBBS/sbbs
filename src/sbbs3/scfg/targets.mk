# targets.mk

# Make 'include file' defining targets for Synchronet SCFG project

SCFG	=	$(EXEODIR)/scfg$(EXEFILE)

all:		xpdev-mt \
		uifc-mt \
		ciolib-mt \
		smblib \
		encode \
		$(EXEODIR) \
		$(MTOBJODIR) \
		$(SCFG)

ifdef SBBSEXEC
.PHONY: install
install: all
	install $(EXEODIR)/* $(SBBSEXEC)

.PHONY: symlinks
symlinks: all
	ln -sfr $(EXEODIR)/* $(SBBSEXEC)
endif

$(SCFG):	$(XPDEV-MT_LIB) $(UIFCLIB-MT) $(CIOLIB-MT)
