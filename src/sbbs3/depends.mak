# depends.mak

# Make 'include file' defining dependencies for Synchronet SBBS.DLL

# $id$

# ODIR, SLASH, and OFILE must be pre-defined

$(ODIR)$(SLASH)answer.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)ars.$(OFILE):			$(HEADERS) ars_defs.h
$(ODIR)$(SLASH)bat_xfer.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)bulkmail.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)chk_ar.$(OFILE):		$(HEADERS) ars_defs.h
$(ODIR)$(SLASH)atcodes.$(OFILE):		$(HEADERS) cmdshell.h
$(ODIR)$(SLASH)chat.$(OFILE):        	$(HEADERS)
$(ODIR)$(SLASH)comio.$(OFILE):       	$(HEADERS)
$(ODIR)$(SLASH)con_hi.$(OFILE):      	$(HEADERS)
$(ODIR)$(SLASH)con_out.$(OFILE):     	$(HEADERS)
$(ODIR)$(SLASH)data.$(OFILE):        	$(HEADERS)
$(ODIR)$(SLASH)data_ovl.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)date_str.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)download.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)email.$(OFILE):			$(HEADERS) cmdshell.h
$(ODIR)$(SLASH)exec.$(OFILE):			$(HEADERS) cmdshell.h
$(ODIR)$(SLASH)execfile.$(OFILE):		$(HEADERS) cmdshell.h
$(ODIR)$(SLASH)execfunc.$(OFILE):		$(HEADERS) cmdshell.h
$(ODIR)$(SLASH)execmisc.$(OFILE):		$(HEADERS) cmdshell.h
$(ODIR)$(SLASH)execmsg.$(OFILE):		$(HEADERS) cmdshell.h
$(ODIR)$(SLASH)fido.$(OFILE):        	$(HEADERS)
$(ODIR)$(SLASH)file.$(OFILE):        	$(HEADERS)
$(ODIR)$(SLASH)filedat.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)getkey.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)getmsg.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)getnode.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)getstr.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)inkey.$(OFILE):    		$(HEADERS)
$(ODIR)$(SLASH)listfile.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)load_cfg.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)logfile.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)login.$(OFILE):			$(HEADERS)
$(ODIR)$(SLASH)logon.$(OFILE):    		$(HEADERS) cmdshell.h
$(ODIR)$(SLASH)logout.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)lzh.$(OFILE):			$(HEADERS)
$(ODIR)$(SLASH)mail.$(OFILE):	       	$(HEADERS)
$(ODIR)$(SLASH)main.$(OFILE):        	$(HEADERS) cmdshell.h
$(ODIR)$(SLASH)misc.$(OFILE):        	$(HEADERS) ars_defs.h crc32.h
$(ODIR)$(SLASH)msgtoqwk.$(OFILE):		$(HEADERS) qwk.h
$(ODIR)$(SLASH)netmail.$(OFILE):     	$(HEADERS) qwk.h
$(ODIR)$(SLASH)newuser.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)pack_qwk.$(OFILE):		$(HEADERS) qwk.h post.h
$(ODIR)$(SLASH)pack_rep.$(OFILE):		$(HEADERS) qwk.h post.h
$(ODIR)$(SLASH)postmsg.$(OFILE):     	$(HEADERS)
$(ODIR)$(SLASH)prntfile.$(OFILE):     	$(HEADERS)
$(ODIR)$(SLASH)putmsg.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)putnode.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)qwk.$(OFILE):			$(HEADERS) qwk.h post.h
$(ODIR)$(SLASH)qwktomsg.$(OFILE):		$(HEADERS) qwk.h
$(ODIR)$(SLASH)readmail.$(OFILE):     	$(HEADERS)
$(ODIR)$(SLASH)readmsgs.$(OFILE):		$(HEADERS) post.h
$(ODIR)$(SLASH)ringbuf.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)scandirs.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)scansubs.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)scfglib1.$(OFILE):    	$(HEADERS) scfglib.h
$(ODIR)$(SLASH)scfglib2.$(OFILE):    	$(HEADERS) scfglib.h
$(ODIR)$(SLASH)smblib.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)sortdir.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)str.$(OFILE):         	$(HEADERS)
$(ODIR)$(SLASH)telgate.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)telmet.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)text_sec.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)tmp_xfer.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)un_qwk.$(OFILE):		$(HEADERS) qwk.h
$(ODIR)$(SLASH)un_rep.$(OFILE):		$(HEADERS) qwk.h
$(ODIR)$(SLASH)upload.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)userdat.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)useredit.$(OFILE):    	$(HEADERS)
$(ODIR)$(SLASH)getuser.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)ver.$(OFILE):			$(HEADERS) $(OBJS)
$(ODIR)$(SLASH)viewfile.$(OFILE):		$(HEADERS)
$(ODIR)$(SLASH)wrappers.$(OFILE):     	$(HEADERS)
$(ODIR)$(SLASH)writemsg.$(OFILE):     	$(HEADERS)
$(ODIR)$(SLASH)xtrn.$(OFILE):        	$(HEADERS) cmdshell.h
$(ODIR)$(SLASH)xtrn_sec.$(OFILE):    	$(HEADERS)
