# smblib/objects.mk

# Make 'include file' listing object files for SMBLIB

# $Id: objects.mk,v 1.6 2019/06/29 01:22:50 rswindell Exp $

# OBJODIR, DIRSEP, and OFILE must be pre-defined

OBJS	=	$(OBJODIR)$(DIRSEP)smbadd$(OFILE)\
			$(OBJODIR)$(DIRSEP)smballoc$(OFILE)\
			$(OBJODIR)$(DIRSEP)smbdump$(OFILE)\
			$(OBJODIR)$(DIRSEP)smbfile$(OFILE)\
			$(OBJODIR)$(DIRSEP)smbhash$(OFILE)\
			$(OBJODIR)$(DIRSEP)smblib$(OFILE)\
			$(OBJODIR)$(DIRSEP)smbstr$(OFILE)\
			$(OBJODIR)$(DIRSEP)smbtxt$(OFILE)\

MTOBJS	=	$(MTOBJODIR)$(DIRSEP)smbadd$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smballoc$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smbdump$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smbfile$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smbhash$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smblib$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smbstr$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)smbtxt$(OFILE)\
