# targets.mk

# Make 'include file' defining targets for xpdel wrappers

# $Id$

# ODIR, SLASH, LIBFILE, EXEFILE, and DELETE must be pre-defined

WRAPTEST	= $(EXEODIR)$(SLASH)wraptest$(EXEFILE) 

all: $(EXEODIR) $(LIBODIR) $(WRAPTEST)
