# objects.mk

# Make 'include file' listing object files for Synchronet SBBSINST

# $Id$

# LIBODIR, SBBSLIBODIR, SLASH, and OFILE must be pre-defined


OBJS	=	$(ODIR)$(SLASH)sbbsinst.$(OFILE)\
			$(ODIR)$(SLASH)conwrap.$(OFILE)\
			$(ODIR)$(SLASH)genwrap.$(OFILE)\
			$(ODIR)$(SLASH)dirwrap.$(OFILE)\
			$(ODIR)$(SLASH)ftpio.$(OFILE)\
			$(ODIR)$(SLASH)uifcx.$(OFILE)

