# objects.mk

# Make 'include file' listing object files for Synchronet SBBSINST

# $Id$

# LIBODIR, SBBSLIBODIR, SLASH, and OFILE must be pre-defined


OBJS	=	$(LIBODIR)$(SLASH)sbbsinst.$(OFILE)\
			$(LIBODIR)$(SLASH)sockwrap.$(OFILE)\
			$(LIBODIR)$(SLASH)genwrap.$(OFILE)\
			$(LIBODIR)$(SLASH)dirwrap.$(OFILE)\
			$(LIBODIR)$(SLASH)filewrap.$(OFILE)\
			$(LIBODIR)$(SLASH)httpio.$(OFILE)\
			$(LIBODIR)$(SLASH)uifcx.$(OFILE)
