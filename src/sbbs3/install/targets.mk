# targets.mk

# Make 'include file' defining targets for Synchronet SBBSINST project

# $Id$

# ODIR, SLASH, LIBFILE, EXEFILE, and DELETE must be pre-defined

SBBSINST	=	$(ODIR)$(SLASH)sbbsinst$(EXEFILE) 

all:	$(ODIR) \
		$(SBBSINST)

clean:
	@$(DELETE) $(ODIR)$(SLASH)*
