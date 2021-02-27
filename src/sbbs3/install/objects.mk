# objects.mk

# Make 'include file' listing object files for Synchronet SBBSINST

# $Id: objects.mk,v 1.6 2004/09/13 22:57:09 deuce Exp $

# MTOBJODIR, DIRSEP, and OFILE must be pre-defined


OBJS	=	$(MTOBJODIR)$(DIRSEP)sbbsinst$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)httpio$(OFILE)
