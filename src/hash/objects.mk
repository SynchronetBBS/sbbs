# hash/objects.mk

# Make 'include file' listing object files for HASH LIB

# $Id$

# OBJODIR, DIRSEP, and OFILE must be pre-defined

OBJS	=	$(OBJODIR)$(DIRSEP)crc16$(OFILE) \
		$(OBJODIR)$(DIRSEP)crc32$(OFILE) \
		$(OBJODIR)$(DIRSEP)md5$(OFILE)


