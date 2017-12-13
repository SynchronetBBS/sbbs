# targets.mk

# Make 'include file' defining targets for Synchronet SCFG project

# $Id$

# LIBODIR, SLASH, LIBFILE, EXEFILE, and DELETE must be pre-defined

SCFG	=	$(EXEODIR)$(DIRSEP)scfg$(EXEFILE) 

all:		xpdev-mt \
		uifc-mt \
		ciolib-mt \
		smblib \
		$(EXEODIR) \
		$(MTOBJODIR) \
		$(SCFG)

ifdef SBBSEXEC
.PHONY: install
install:
	install $(EXEODIR)/* $(SBBSEXEC)

.PHONY: symlinks
symlinks:
	ln -sfr $(EXEODIR)/* $(SBBSEXEC)
endif

$(SCFG):	$(XPDEV-MT_LIB) $(UIFCLIB-MT) $(CIOLIB-MT)
