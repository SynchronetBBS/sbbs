# objects.mk

# Make 'include file' listing object files for xpdev "wrappers"

# $Id$

# LIBODIR, SLASH, and OFILE must be pre-defined

OBJS = \
	$(LIBODIR)$(SLASH)genwrap.$(OFILE) \
	$(LIBODIR)$(SLASH)conwrap.$(OFILE) \
	$(LIBODIR)$(SLASH)dirwrap.$(OFILE) \
	$(LIBODIR)$(SLASH)filewrap.$(OFILE) \
	$(LIBODIR)$(SLASH)threadwrap.$(OFILE) \
	$(LIBODIR)$(SLASH)semwrap.$(OFILE)
