# depends.mk

# Make 'include file' defining dependencies for Synchronet SBBS.DLL

# $Id$

# LIBODIR, EXEODIR, SLASH, and OFILE must be pre-defined

$(LIBODIR)$(SLASH)answer.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)ars.$(OFILE):			$(HEADERS) ars_defs.h
$(LIBODIR)$(SLASH)bat_xfer.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)bulkmail.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)chk_ar.$(OFILE):		$(HEADERS) ars_defs.h
$(LIBODIR)$(SLASH)atcodes.$(OFILE):		$(HEADERS) cmdshell.h
$(LIBODIR)$(SLASH)chat.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)comio.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)con_hi.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)con_out.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)data.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)data_ovl.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)date_str.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)download.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)email.$(OFILE):		$(HEADERS) cmdshell.h
$(LIBODIR)$(SLASH)exec.$(OFILE):		$(HEADERS) cmdshell.h
$(LIBODIR)$(SLASH)execfile.$(OFILE):	$(HEADERS) cmdshell.h
$(LIBODIR)$(SLASH)execfunc.$(OFILE):	$(HEADERS) cmdshell.h
$(LIBODIR)$(SLASH)execmisc.$(OFILE):	$(HEADERS) cmdshell.h
$(LIBODIR)$(SLASH)execnet.$(OFILE):	$(HEADERS) cmdshell.h
$(LIBODIR)$(SLASH)execmsg.$(OFILE):		$(HEADERS) cmdshell.h
$(LIBODIR)$(SLASH)fido.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)file.$(OFILE):      	$(HEADERS)
$(LIBODIR)$(SLASH)filedat.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)getkey.$(OFILE):    	$(HEADERS)
$(LIBODIR)$(SLASH)getmsg.$(OFILE):    	$(HEADERS)
$(LIBODIR)$(SLASH)getnode.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)getstr.$(OFILE):    	$(HEADERS)
$(LIBODIR)$(SLASH)ident.$(OFILE):    	$(HEADERS) ident.h
$(LIBODIR)$(SLASH)inkey.$(OFILE):    	$(HEADERS)
$(LIBODIR)$(SLASH)listfile.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)load_cfg.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)logfile.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)login.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)logon.$(OFILE):    	$(HEADERS) cmdshell.h
$(LIBODIR)$(SLASH)logout.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)lzh.$(OFILE):			$(HEADERS)
$(LIBODIR)$(SLASH)mail.$(OFILE):	    $(HEADERS)
$(LIBODIR)$(SLASH)main.$(OFILE):		$(HEADERS) cmdshell.h ident.h
$(LIBODIR)$(SLASH)misc.$(OFILE):		$(HEADERS) ars_defs.h crc32.h
$(LIBODIR)$(SLASH)msgtoqwk.$(OFILE):	$(HEADERS) qwk.h
$(LIBODIR)$(SLASH)netmail.$(OFILE):		$(HEADERS) qwk.h
$(LIBODIR)$(SLASH)newuser.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)pack_qwk.$(OFILE):	$(HEADERS) qwk.h 
$(LIBODIR)$(SLASH)pack_rep.$(OFILE):	$(HEADERS) qwk.h
$(LIBODIR)$(SLASH)postmsg.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)prntfile.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)putmsg.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)putnode.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)qwk.$(OFILE):			$(HEADERS) qwk.h
$(LIBODIR)$(SLASH)qwktomsg.$(OFILE):	$(HEADERS) qwk.h
$(LIBODIR)$(SLASH)readmail.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)readmsgs.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)ringbuf.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)scandirs.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)scansubs.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)scfglib1.$(OFILE):	$(HEADERS) scfglib.h
$(LIBODIR)$(SLASH)scfglib2.$(OFILE):	$(HEADERS) scfglib.h
$(LIBODIR)$(SLASH)smblib.$(OFILE):    	smblib.h smbdefs.h
$(LIBODIR)$(SLASH)smbtxt.$(OFILE):   	smblib.h lzh.h
$(LIBODIR)$(SLASH)sortdir.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)str.$(OFILE):			$(HEADERS)
$(LIBODIR)$(SLASH)telgate.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)telmet.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)text_sec.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)tmp_xfer.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)un_qwk.$(OFILE):		$(HEADERS) qwk.h
$(LIBODIR)$(SLASH)un_rep.$(OFILE):		$(HEADERS) qwk.h
$(LIBODIR)$(SLASH)upload.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)userdat.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)useredit.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)getuser.$(OFILE):		$(HEADERS)
$(LIBODIR)$(SLASH)ver.$(OFILE):			$(HEADERS) $(OBJS)
$(LIBODIR)$(SLASH)viewfile.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)writemsg.$(OFILE):	$(HEADERS)
$(LIBODIR)$(SLASH)xtrn.$(OFILE):		$(HEADERS) cmdshell.h
$(LIBODIR)$(SLASH)xtrn_sec.$(OFILE):	$(HEADERS)
