# objects.mak

# Make 'include file' listing object files for SBBS.DLL

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
            $(SBBSLIBODIR)$(SLASH)scfgsave.$(OFILE)\
            $(SBBSLIBODIR)$(SLASH)scfglib1.$(OFILE)\
            $(SBBSLIBODIR)$(SLASH)smblib.$(OFILE)\
            $(SBBSLIBODIR)$(SLASH)scfglib2.$(OFILE)\
            $(SBBSLIBODIR)$(SLASH)smbwrap.$(OFILE)\
            $(SBBSLIBODIR)$(SLASH)ars.$(OFILE)\
            $(SBBSLIBODIR)$(SLASH)load_cfg.$(OFILE)\
            $(SBBSLIBODIR)$(SLASH)misc.$(OFILE)\
            $(SBBSLIBODIR)$(SLASH)crc32.$(OFILE)\
            $(SBBSLIBODIR)$(SLASH)userdat.$(OFILE)\
            $(SBBSLIBODIR)$(SLASH)date_str.$(OFILE)\
            $(SBBSLIBODIR)$(SLASH)wrappers.$(OFILE)\
			$(UIFCLIBODIR)$(SLASH)uifcx.$(OFILE)

ifdef USE_DIALOG
OBJS := $(OBJS)			$(UIFCLIBODIR)$(SLASH)uifcd.$(OFILE)
endif
