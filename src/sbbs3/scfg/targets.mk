# targets.mk

# Make 'include file' defining targets for Synchronet SCFG project

# $Id$

# ODIR, SLASH, LIBFILE, EXEFILE, and DELETE must be pre-defined

SCFG	=	$(ODIR)$(SLASH)scfg$(EXEFILE) 
MAKEHELP=	$(ODIR)$(SLASH)makehelp$(EXEFILE) 
SCFGHELP=	$(ODIR)$(SLASH)scfghelp.dat

all:	$(ODIR) \
		$(SCFG) $(SCFGHELP)

clean:
	@$(DELETE) $(ODIR)$(SLASH)*
