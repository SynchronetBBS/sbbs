# targets.mak

# Make 'include file' defining targets for Synchronet SCFG project

# $Id$

# ODIR, SLASH, LIBFILE, EXEFILE, and DELETE must be pre-defined

SCFG	=	$(ODIR)$(SLASH)scfg$(EXEFILE) 
SCFGHELP=	$(ODIR)$(SLASH)scfghelp.dat

all:	$(ODIR) \
		$(SCFG) $(SCFGHELP)

clean:
	@$(DELETE) $(ODIR)$(SLASH)*
