# objects.mk

# Make 'include file' listing object files for Synchronet SBBSINST

# $Id$

# MTOBJODIR, DIRSEP, and OFILE must be pre-defined


OBJS	=	$(MTOBJODIR)$(DIRSEP)sbbsinst$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)httpio$(OFILE)
