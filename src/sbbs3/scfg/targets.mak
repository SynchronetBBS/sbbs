# targets.mak

# Make 'include file' defining targets for Synchronet project

# $Id$

# LIBODIR, EXEODIR, SLASH, LIBFILE, EXEFILE, and DELETE must be pre-defined

SCFG	=	$(EXEODIR)$(SLASH)scfg$(EXEFILE) 
SCFGOBJ	=	$(OBJODIR)$(SLASH)scfg$(OBJFILE) 

all:	$(LIBODIR) $(SBBSLIBODIR) $(UIFCLIBODIR) $(EXEODIR) \
		$(OBJS)\
		$(SCFG)

clean:
	$(DELETE) $(LIBODIR)/*
	$(DELETE) $(EXEODIR)/*
	$(DELETE) $(SBBSLIBODIR)/*
	$(DELETE) $(UIFCLIBODIR)/*
