# smblib/objects.mk

# Make 'include file' listing object files for SMBLIB

# $Id$

# OBJODIR, DIRSEP, and OFILE must be pre-defined

OBJS	=	$(OBJODIR)$(DIRSEP)ansi_cio$(OFILE)\
			$(OBJODIR)$(DIRSEP)ciolib$(OFILE)\
			$(OBJODIR)$(DIRSEP)cterm$(OFILE)\
			$(OBJODIR)$(DIRSEP)mouse$(OFILE)
