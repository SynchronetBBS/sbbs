# smblib/objects.mk

# Make 'include file' listing object files for SMBLIB

# $Id$

# OBJODIR, DIRSEP, and OFILE must be pre-defined

OBJS	=	$(OBJODIR)$(DIRSEP)smbadd$(OFILE)\
			$(OBJODIR)$(DIRSEP)smballoc$(OFILE)\
			$(OBJODIR)$(DIRSEP)smbdump$(OFILE)\
			$(OBJODIR)$(DIRSEP)smbfile$(OFILE)\
			$(OBJODIR)$(DIRSEP)smbhash$(OFILE)\
			$(OBJODIR)$(DIRSEP)smblib$(OFILE)\
			$(OBJODIR)$(DIRSEP)smbstr$(OFILE)\
			$(OBJODIR)$(DIRSEP)smbtxt$(OFILE)\
			$(OBJODIR)$(DIRSEP)crc16$(OFILE)\
			$(OBJODIR)$(DIRSEP)crc32$(OFILE)\
			$(OBJODIR)$(DIRSEP)md5$(OFILE)\

MTOBJS	=	$(MTOBJODIR)$(DIRSEP)smbadd$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smballoc$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smbdump$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smbfile$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smbhash$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smblib$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smbstr$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smbtxt$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)crc16$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)crc32$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)md5$(OFILE)\
