# targets.mk

# Make 'include file' defining targets for Synchronet SCFG project

# $Id$

# LIBODIR, SLASH, LIBFILE, EXEFILE, and DELETE must be pre-defined

SCFG	=	$(EXEODIR)$(SLASH)scfg$(EXEFILE) 
MAKEHELP=	$(EXEODIR)$(SLASH)makehelp$(EXEFILE) 
SCFGHELP=	$(EXEODIR)$(SLASH)scfghelp.dat

all:	$(EXEODIR) \
		$(LIBODIR) \
		$(SCFG) $(SCFGHELP)
