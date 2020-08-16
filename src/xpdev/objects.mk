# libobjs.mk

# Make 'include file' listing object files for xpdev "wrappers"

# $Id: objects.mk,v 1.19 2019/07/10 00:25:24 rswindell Exp $

# OBJODIR, SLASH, and OFILE must be pre-defined

OBJS	= \
	$(OBJODIR)$(DIRSEP)conwrap$(OFILE) \
	$(OBJODIR)$(DIRSEP)dat_file$(OFILE) \
	$(OBJODIR)$(DIRSEP)datewrap$(OFILE) \
	$(OBJODIR)$(DIRSEP)dirwrap$(OFILE) \
	$(OBJODIR)$(DIRSEP)filewrap$(OFILE) \
	$(OBJODIR)$(DIRSEP)genwrap$(OFILE) \
	$(OBJODIR)$(DIRSEP)ini_file$(OFILE) \
	$(OBJODIR)$(DIRSEP)link_list$(OFILE) \
	$(OBJODIR)$(DIRSEP)multisock$(OFILE) \
	$(OBJODIR)$(DIRSEP)netwrap$(OFILE) \
	$(OBJODIR)$(DIRSEP)sockwrap$(OFILE) \
	$(OBJODIR)$(DIRSEP)semfile$(OFILE) \
	$(OBJODIR)$(DIRSEP)str_list$(OFILE) \
	$(OBJODIR)$(DIRSEP)strwrap$(OFILE) \
	$(OBJODIR)$(DIRSEP)unicode$(OFILE) \
	$(OBJODIR)$(DIRSEP)xp_dl$(OFILE) \
	$(OBJODIR)$(DIRSEP)xpbeep$(OFILE) \
	$(OBJODIR)$(DIRSEP)xpdatetime$(OFILE) \
	$(OBJODIR)$(DIRSEP)xpmap$(OFILE) \
	$(OBJODIR)$(DIRSEP)xpprintf$(OFILE)


MTOBJS	= \
	$(MTOBJODIR)$(DIRSEP)conwrap$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)dat_file$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)datewrap$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)dirwrap$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)filewrap$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)genwrap$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)ini_file$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)link_list$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)msg_queue$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)multisock$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)semwrap$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)netwrap$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)sockwrap$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)semfile$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)str_list$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)strwrap$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)threadwrap$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)unicode$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)xp_dl$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)xpbeep$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)xpdatetime$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)xpmap$(OFILE) \
	$(MTOBJODIR)$(DIRSEP)xpprintf$(OFILE)

TESTOBJS = \
	$(MTOBJODIR)$(DIRSEP)wraptest$(OFILE)

