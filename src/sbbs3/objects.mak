# objects.mak

# Make 'include file' listing object files for SBBS.DLL

# $id$

# ODIR, SLASH, and OFILE must be pre-defined

OBJS	=	$(ODIR)$(SLASH)ansiterm.$(OFILE)\
			$(ODIR)$(SLASH)answer.$(OFILE)\
			$(ODIR)$(SLASH)ars.$(OFILE)\
			$(ODIR)$(SLASH)atcodes.$(OFILE)\
			$(ODIR)$(SLASH)bat_xfer.$(OFILE)\
			$(ODIR)$(SLASH)bulkmail.$(OFILE)\
			$(ODIR)$(SLASH)chat.$(OFILE)\
			$(ODIR)$(SLASH)chk_ar.$(OFILE)\
			$(ODIR)$(SLASH)con_hi.$(OFILE)\
			$(ODIR)$(SLASH)con_out.$(OFILE)\
			$(ODIR)$(SLASH)data.$(OFILE)\
			$(ODIR)$(SLASH)data_ovl.$(OFILE)\
			$(ODIR)$(SLASH)date_str.$(OFILE)\
			$(ODIR)$(SLASH)download.$(OFILE)\
			$(ODIR)$(SLASH)email.$(OFILE)\
			$(ODIR)$(SLASH)exec.$(OFILE)\
			$(ODIR)$(SLASH)execfile.$(OFILE)\
			$(ODIR)$(SLASH)execfunc.$(OFILE)\
			$(ODIR)$(SLASH)execmisc.$(OFILE)\
			$(ODIR)$(SLASH)execmsg.$(OFILE)\
			$(ODIR)$(SLASH)fido.$(OFILE)\
			$(ODIR)$(SLASH)file.$(OFILE)\
			$(ODIR)$(SLASH)filedat.$(OFILE)\
			$(ODIR)$(SLASH)getkey.$(OFILE)\
			$(ODIR)$(SLASH)getmsg.$(OFILE)\
			$(ODIR)$(SLASH)getnode.$(OFILE)\
			$(ODIR)$(SLASH)getstr.$(OFILE)\
			$(ODIR)$(SLASH)inkey.$(OFILE)\
			$(ODIR)$(SLASH)listfile.$(OFILE)\
			$(ODIR)$(SLASH)load_cfg.$(OFILE)\
			$(ODIR)$(SLASH)logfile.$(OFILE)\
			$(ODIR)$(SLASH)login.$(OFILE)\
			$(ODIR)$(SLASH)logon.$(OFILE)\
			$(ODIR)$(SLASH)logout.$(OFILE)\
			$(ODIR)$(SLASH)lzh.$(OFILE)\
			$(ODIR)$(SLASH)mail.$(OFILE)\
			$(ODIR)$(SLASH)main.$(OFILE)\
			$(ODIR)$(SLASH)misc.$(OFILE)\
			$(ODIR)$(SLASH)msgtoqwk.$(OFILE)\
			$(ODIR)$(SLASH)netmail.$(OFILE)\
			$(ODIR)$(SLASH)newuser.$(OFILE)\
			$(ODIR)$(SLASH)pack_qwk.$(OFILE)\
			$(ODIR)$(SLASH)pack_rep.$(OFILE)\
			$(ODIR)$(SLASH)postmsg.$(OFILE)\
			$(ODIR)$(SLASH)prntfile.$(OFILE)\
			$(ODIR)$(SLASH)putmsg.$(OFILE)\
			$(ODIR)$(SLASH)putnode.$(OFILE)\
			$(ODIR)$(SLASH)qwk.$(OFILE)\
			$(ODIR)$(SLASH)qwktomsg.$(OFILE)\
			$(ODIR)$(SLASH)readmail.$(OFILE)\
			$(ODIR)$(SLASH)readmsgs.$(OFILE)\
			$(ODIR)$(SLASH)ringbuf.$(OFILE)\
			$(ODIR)$(SLASH)scandirs.$(OFILE)\
			$(ODIR)$(SLASH)scansubs.$(OFILE)\
			$(ODIR)$(SLASH)scfglib1.$(OFILE)\
			$(ODIR)$(SLASH)scfglib2.$(OFILE)\
			$(ODIR)$(SLASH)smblib.$(OFILE)\
			$(ODIR)$(SLASH)sortdir.$(OFILE)\
			$(ODIR)$(SLASH)str.$(OFILE)\
			$(ODIR)$(SLASH)telgate.$(OFILE)\
			$(ODIR)$(SLASH)telnet.$(OFILE)\
			$(ODIR)$(SLASH)text_sec.$(OFILE)\
			$(ODIR)$(SLASH)tmp_xfer.$(OFILE)\
			$(ODIR)$(SLASH)un_qwk.$(OFILE)\
			$(ODIR)$(SLASH)un_rep.$(OFILE)\
			$(ODIR)$(SLASH)upload.$(OFILE)\
			$(ODIR)$(SLASH)userdat.$(OFILE)\
			$(ODIR)$(SLASH)useredit.$(OFILE)\
			$(ODIR)$(SLASH)viewfile.$(OFILE)\
			$(ODIR)$(SLASH)wrappers.$(OFILE)\
			$(ODIR)$(SLASH)writemsg.$(OFILE)\
			$(ODIR)$(SLASH)xtrn.$(OFILE)\
			$(ODIR)$(SLASH)xtrn_sec.$(OFILE) 
