# encode/objects.mk

# Make 'include file' listing object files for ENCODE LIB

# $Id$

# OBJODIR, DIRSEP, and OFILE must be pre-defined

OBJS	=	$(OBJODIR)$(DIRSEP)unicode$(OFILE) \
        $(OBJODIR)$(DIRSEP)utf8$(OFILE) \
		$(OBJODIR)$(DIRSEP)uucode$(OFILE) \
		$(OBJODIR)$(DIRSEP)yenc$(OFILE) \
		$(OBJODIR)$(DIRSEP)lzh$(OFILE) \
		$(OBJODIR)$(DIRSEP)base64$(OFILE)


