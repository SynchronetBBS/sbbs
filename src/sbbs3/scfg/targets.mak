# targets.mak

# Make 'include file' defining targets for Synchronet project

# $Id$

# LIBODIR, EXEODIR, SLASH, LIBFILE, EXEFILE, and DELETE must be pre-defined

SCFG	=	$(EXEODIR)$(SLASH)scfgx$(EXEFILE) 
SCFGOBJ	=	$(OBJODIR)$(SLASH)scfgx$(OBJFILE) 

all:	$(LIBODIR) $(EXEODIR) \
		$(OBJS)\
		$(SCFG)

clean:
	$(DELETE) $(LIBODIR)/*
	$(DELETE) $(EXEODIR)/*
