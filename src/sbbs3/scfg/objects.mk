# objects.mk

# Make 'include file' listing object files for Synchronet SCFG

# $Id$

# MTLIBODIR, SBBSMTLIBODIR, DIRSEP, and OFILE must be pre-defined


OBJS	=	$(MTOBJODIR)$(DIRSEP)scfg$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfgxtrn$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfgmsg$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfgnet$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfgnode$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfgsub$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfgsys$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfgxfr1$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfgxfr2$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfgchat$(OFILE)\
            $(MTOBJODIR)$(DIRSEP)scfgsave$(OFILE)\
            $(MTOBJODIR)$(DIRSEP)scfglib1$(OFILE)\
            $(MTOBJODIR)$(DIRSEP)scfglib2$(OFILE)\
            $(MTOBJODIR)$(DIRSEP)load_cfg$(OFILE)\
            $(MTOBJODIR)$(DIRSEP)nopen$(OFILE)\
            $(MTOBJODIR)$(DIRSEP)dat_rec$(OFILE)\
            $(MTOBJODIR)$(DIRSEP)userdat$(OFILE)\
            $(MTOBJODIR)$(DIRSEP)msgdate$(OFILE)\
            $(MTOBJODIR)$(DIRSEP)date_str$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)str_util$(OFILE)
