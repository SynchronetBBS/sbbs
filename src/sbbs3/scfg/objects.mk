# objects.mk

# Make 'include file' listing object files for Synchronet SCFG

# $Id$

# LIBODIR, SBBSLIBODIR, SLASH, and OFILE must be pre-defined


OBJS	=	$(ODIR)$(SLASH)scfg.$(OFILE)\
			$(ODIR)$(SLASH)scfgxtrn.$(OFILE)\
			$(ODIR)$(SLASH)scfgmsg.$(OFILE)\
			$(ODIR)$(SLASH)scfgnet.$(OFILE)\
			$(ODIR)$(SLASH)scfgnode.$(OFILE)\
			$(ODIR)$(SLASH)scfgsub.$(OFILE)\
			$(ODIR)$(SLASH)scfgsys.$(OFILE)\
			$(ODIR)$(SLASH)scfgxfr1.$(OFILE)\
			$(ODIR)$(SLASH)scfgxfr2.$(OFILE)\
			$(ODIR)$(SLASH)scfgchat.$(OFILE)\
            $(ODIR)$(SLASH)scfgsave.$(OFILE)\
            $(ODIR)$(SLASH)scfglib1.$(OFILE)\
            $(ODIR)$(SLASH)smblib.$(OFILE)\
            $(ODIR)$(SLASH)scfglib2.$(OFILE)\
            $(ODIR)$(SLASH)ars.$(OFILE)\
            $(ODIR)$(SLASH)load_cfg.$(OFILE)\
            $(ODIR)$(SLASH)nopen.$(OFILE)\
            $(ODIR)$(SLASH)crc16.$(OFILE)\
            $(ODIR)$(SLASH)crc32.$(OFILE)\
            $(ODIR)$(SLASH)dat_rec.$(OFILE)\
            $(ODIR)$(SLASH)userdat.$(OFILE)\
            $(ODIR)$(SLASH)date_str.$(OFILE)\
			$(ODIR)$(SLASH)str_util.$(OFILE)\
			$(ODIR)$(SLASH)genwrap.$(OFILE)\
			$(ODIR)$(SLASH)conwrap.$(OFILE)\
			$(ODIR)$(SLASH)dirwrap.$(OFILE)\
			$(ODIR)$(SLASH)filewrap.$(OFILE)\
			$(ODIR)$(SLASH)uifcx.$(OFILE)

