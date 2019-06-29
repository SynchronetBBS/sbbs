# encode/objects.mk

# Make 'include file' listing object files for ENCODE LIB

# $Id$

# OBJODIR, DIRSEP, and OFILE must be pre-defined

OBJS	=	$(OBJODIR)$(DIRSEP)cp437_utf8_tbl$(OFILE) \
		$(OBJODIR)$(DIRSEP)uucode$(OFILE) \
		$(OBJODIR)$(DIRSEP)yenc$(OFILE)


