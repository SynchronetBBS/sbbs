# smblib/objects.mk

# Make 'include file' listing object files for SMBLIB

# $Id$

# LIBODIR, DIRSEP, and OFILE must be pre-defined

OBJS	=	$(LIBODIR)$(DIRSEP)smbadd$(OFILE)\
			$(LIBODIR)$(DIRSEP)smballoc$(OFILE)\
			$(LIBODIR)$(DIRSEP)smbdump$(OFILE)\
			$(LIBODIR)$(DIRSEP)smbfile$(OFILE)\
			$(LIBODIR)$(DIRSEP)smbhash$(OFILE)\
			$(LIBODIR)$(DIRSEP)smblib$(OFILE)\
			$(LIBODIR)$(DIRSEP)smbstr$(OFILE)\
			$(LIBODIR)$(DIRSEP)smbtxt$(OFILE)\
			$(LIBODIR)$(DIRSEP)crc16$(OFILE)\
			$(LIBODIR)$(DIRSEP)crc32$(OFILE)\
			$(LIBODIR)$(DIRSEP)md5$(OFILE)\
			$(LIBODIR)$(DIRSEP)lzh$(OFILE)
