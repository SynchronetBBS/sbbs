# targets.mk

# Make 'include file' defining targets for xpdel wrappers

# $Id$

# ODIR, SLASH, LIBFILE, EXEFILE, and DELETE must be pre-defined

WRAPTEST	= $(ODIR)$(SLASH)wraptest$(EXEFILE) 

all: $(ODIR) $(WRAPTEST)

clean:
	@$(DELETE) $(ODIR)$(SLASH)*
