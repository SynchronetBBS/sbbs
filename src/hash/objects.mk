# hash/objects.mk

# Make 'include file' listing object files for HASH LIB

# $Id: objects.mk,v 1.1 2019/06/29 01:14:55 rswindell Exp $

# OBJODIR, DIRSEP, and OFILE must be pre-defined

OBJS	=	$(OBJODIR)$(DIRSEP)crc16$(OFILE) \
		$(OBJODIR)$(DIRSEP)crc32$(OFILE) \
		$(OBJODIR)$(DIRSEP)md5$(OFILE)


