# smblib/objects.mk

# Make 'include file' listing object files for SMBLIB

# $Id$

# OBJODIR, DIRSEP, and OFILE must be pre-defined

OBJS	=	$(MTOBJODIR)$(DIRSEP)ansi_cio$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)ciolib$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)cterm$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)mouse$(OFILE)
