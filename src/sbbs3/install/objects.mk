# objects.mk

# Make 'include file' listing object files for Synchronet SBBSINST

# $Id$

# LIBODIR, SBBSLIBODIR, SLASH, and OFILE must be pre-defined


OBJS	=	$(ODIR)$(SLASH)sbbsinst.$(OFILE)\
			$(ODIR)$(SLASH)sockwrap.$(OFILE)\
			$(ODIR)$(SLASH)genwrap.$(OFILE)\
			$(ODIR)$(SLASH)dirwrap.$(OFILE)\
			$(ODIR)$(SLASH)filewrap.$(OFILE)\
			$(ODIR)$(SLASH)ciowrap.$(OFILE)\
			$(ODIR)$(SLASH)httpio.$(OFILE)\
			$(ODIR)$(SLASH)uifcx.$(OFILE)
