# objects.mk

# Make 'include file' listing object files for Synchronet SCFG

# $Id$

# LIBODIR, SBBSLIBODIR, SLASH, and OFILE must be pre-defined


OBJS	=	$(LIBODIR)$(SLASH)scfg.$(OFILE)\
			$(LIBODIR)$(SLASH)scfgxtrn.$(OFILE)\
			$(LIBODIR)$(SLASH)scfgmsg.$(OFILE)\
			$(LIBODIR)$(SLASH)scfgnet.$(OFILE)\
			$(LIBODIR)$(SLASH)scfgnode.$(OFILE)\
			$(LIBODIR)$(SLASH)scfgsub.$(OFILE)\
			$(LIBODIR)$(SLASH)scfgsys.$(OFILE)\
			$(LIBODIR)$(SLASH)scfgxfr1.$(OFILE)\
			$(LIBODIR)$(SLASH)scfgxfr2.$(OFILE)\
			$(LIBODIR)$(SLASH)scfgchat.$(OFILE)\
            $(LIBODIR)$(SLASH)scfgsave.$(OFILE)\
            $(LIBODIR)$(SLASH)scfglib1.$(OFILE)\
            $(LIBODIR)$(SLASH)smblib.$(OFILE)\
            $(LIBODIR)$(SLASH)scfglib2.$(OFILE)\
            $(LIBODIR)$(SLASH)load_cfg.$(OFILE)\
            $(LIBODIR)$(SLASH)nopen.$(OFILE)\
            $(LIBODIR)$(SLASH)crc16.$(OFILE)\
            $(LIBODIR)$(SLASH)crc32.$(OFILE)\
            $(LIBODIR)$(SLASH)md5.$(OFILE)\
            $(LIBODIR)$(SLASH)lzh.$(OFILE)\
            $(LIBODIR)$(SLASH)dat_rec.$(OFILE)\
            $(LIBODIR)$(SLASH)userdat.$(OFILE)\
            $(LIBODIR)$(SLASH)date_str.$(OFILE)\
			$(LIBODIR)$(SLASH)str_util.$(OFILE)\
			$(LIBODIR)$(SLASH)genwrap.$(OFILE)\
			$(LIBODIR)$(SLASH)dirwrap.$(OFILE)\
			$(LIBODIR)$(SLASH)filewrap.$(OFILE)\
			$(LIBODIR)$(SLASH)uifcx.$(OFILE)
