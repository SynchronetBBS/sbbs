# objects.mak

# Make 'include file' listing object files for SBBS.DLL

# $Id$

# LIBODIR, SLASH, and OFILE must be pre-defined


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
			../../uifc/uifcx.$(OFILE)\
                        ..$(SLASH)$(LIBODIR)$(SLASH)scfgsave.$(OFILE)\
                        ..$(SLASH)$(LIBODIR)$(SLASH)scfglib1.$(OFILE)\
                        ..$(SLASH)$(LIBODIR)$(SLASH)smblib.$(OFILE)\
                        ..$(SLASH)$(LIBODIR)$(SLASH)scfglib2.$(OFILE)\
                        ..$(SLASH)$(LIBODIR)$(SLASH)smbwrap.$(OFILE)\
                        ..$(SLASH)$(LIBODIR)$(SLASH)ars.$(OFILE)\
                        ..$(SLASH)$(LIBODIR)$(SLASH)load_cfg.$(OFILE)\
                        ..$(SLASH)$(LIBODIR)$(SLASH)misc.$(OFILE)\
                        ..$(SLASH)$(LIBODIR)$(SLASH)crc32.$(OFILE)\
                        ..$(SLASH)$(LIBODIR)$(SLASH)userdat.$(OFILE)\
                        ..$(SLASH)$(LIBODIR)$(SLASH)date_str.$(OFILE)
