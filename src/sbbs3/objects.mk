# objects.mk

# [MT]OBJODIR and OFILE must be pre-defined

OBJS	=		$(LOAD_CFG_OBJS) \
			$(MTOBJODIR)/ansiterm$(OFILE) \
			$(MTOBJODIR)/answer$(OFILE)\
			$(MTOBJODIR)/atcodes$(OFILE)\
			$(MTOBJODIR)/bat_xfer$(OFILE)\
			$(MTOBJODIR)/bulkmail$(OFILE)\
			$(MTOBJODIR)/chat$(OFILE)\
			$(MTOBJODIR)/chk_ar$(OFILE)\
			$(MTOBJODIR)/comio$(OFILE)\
			$(MTOBJODIR)/con_hi$(OFILE)\
			$(MTOBJODIR)/con_out$(OFILE)\
			$(MTOBJODIR)/dat_rec$(OFILE)\
			$(MTOBJODIR)/data$(OFILE)\
			$(MTOBJODIR)/data_ovl$(OFILE)\
			$(MTOBJODIR)/date_str$(OFILE)\
			$(MTOBJODIR)/download$(OFILE)\
			$(MTOBJODIR)/email$(OFILE)\
			$(MTOBJODIR)/exec$(OFILE)\
			$(MTOBJODIR)/execfile$(OFILE)\
			$(MTOBJODIR)/execfunc$(OFILE)\
			$(MTOBJODIR)/execmisc$(OFILE)\
			$(MTOBJODIR)/execmsg$(OFILE)\
			$(MTOBJODIR)/execnet$(OFILE)\
			$(MTOBJODIR)/fido$(OFILE)\
			$(MTOBJODIR)/file$(OFILE)\
			$(MTOBJODIR)/filedat$(OFILE)\
			$(MTOBJODIR)/getkey$(OFILE)\
			$(MTOBJODIR)/getmail$(OFILE)\
			$(MTOBJODIR)/getmsg$(OFILE)\
			$(MTOBJODIR)/getnode$(OFILE)\
			$(MTOBJODIR)/getstats$(OFILE)\
			$(MTOBJODIR)/getstr$(OFILE)\
			$(MTOBJODIR)/inkey$(OFILE)\
			$(MTOBJODIR)/ident$(OFILE)\
			$(MTOBJODIR)/jsdebug$(OFILE)\
			$(MTOBJODIR)/js_archive$(OFILE)\
			$(MTOBJODIR)/js_bbs$(OFILE)\
			$(MTOBJODIR)/js_client$(OFILE)\
			$(MTOBJODIR)/js_com$(OFILE)\
			$(MTOBJODIR)/js_console$(OFILE)\
			$(MTOBJODIR)/js_cryptcert$(OFILE)\
			$(MTOBJODIR)/js_cryptcon$(OFILE)\
			$(MTOBJODIR)/js_cryptkeyset$(OFILE)\
			$(MTOBJODIR)/js_file$(OFILE)\
			$(MTOBJODIR)/js_file_area$(OFILE)\
			$(MTOBJODIR)/js_global$(OFILE)\
			$(MTOBJODIR)/js_internal$(OFILE)\
			$(MTOBJODIR)/js_msg_area$(OFILE)\
			$(MTOBJODIR)/js_msgbase$(OFILE)\
			$(MTOBJODIR)/js_filebase$(OFILE)\
			$(MTOBJODIR)/js_queue$(OFILE)\
			$(MTOBJODIR)/js_request$(OFILE)\
			$(MTOBJODIR)/js_rtpool$(OFILE)\
			$(MTOBJODIR)/js_server$(OFILE)\
			$(MTOBJODIR)/js_socket$(OFILE)\
			$(MTOBJODIR)/js_sprintf$(OFILE)\
			$(MTOBJODIR)/js_system$(OFILE)\
			$(MTOBJODIR)/js_user$(OFILE)\
			$(MTOBJODIR)/js_xtrn_area$(OFILE)\
			$(MTOBJODIR)/listfile$(OFILE)\
			$(MTOBJODIR)/logfile$(OFILE)\
			$(MTOBJODIR)/login$(OFILE)\
			$(MTOBJODIR)/logon$(OFILE)\
			$(MTOBJODIR)/logout$(OFILE)\
			$(MTOBJODIR)/mail$(OFILE)\
			$(MTOBJODIR)/main$(OFILE)\
			$(MTOBJODIR)/msg_id$(OFILE)\
			$(MTOBJODIR)/msgdate$(OFILE)\
			$(MTOBJODIR)/msgtoqwk$(OFILE)\
			$(MTOBJODIR)/netmail$(OFILE)\
			$(MTOBJODIR)/newuser$(OFILE)\
			$(MTOBJODIR)/pack_qwk$(OFILE)\
			$(MTOBJODIR)/pack_rep$(OFILE)\
			$(MTOBJODIR)/postmsg$(OFILE)\
			$(MTOBJODIR)/prntfile$(OFILE)\
			$(MTOBJODIR)/putmsg$(OFILE)\
			$(MTOBJODIR)/putnode$(OFILE)\
			$(MTOBJODIR)/qwk$(OFILE)\
			$(MTOBJODIR)/qwktomsg$(OFILE)\
			$(MTOBJODIR)/readmail$(OFILE)\
			$(MTOBJODIR)/readmsgs$(OFILE)\
			$(MTOBJODIR)/ringbuf$(OFILE)\
			$(MTOBJODIR)/sauce$(OFILE)\
			$(MTOBJODIR)/scandirs$(OFILE)\
			$(MTOBJODIR)/scansubs$(OFILE)\
			$(MTOBJODIR)/scfgsave$(OFILE)\
			$(MTOBJODIR)/sockopts$(OFILE)\
			$(MTOBJODIR)/str$(OFILE)\
			$(MTOBJODIR)/telgate$(OFILE)\
			$(MTOBJODIR)/telnet$(OFILE)\
			$(MTOBJODIR)/text_sec$(OFILE)\
			$(MTOBJODIR)/tmp_xfer$(OFILE)\
			$(MTOBJODIR)/un_qwk$(OFILE)\
			$(MTOBJODIR)/un_rep$(OFILE)\
			$(MTOBJODIR)/upload$(OFILE)\
			$(MTOBJODIR)/userdat$(OFILE)\
			$(MTOBJODIR)/useredit$(OFILE)\
			$(MTOBJODIR)/viewfile$(OFILE)\
			$(MTOBJODIR)/wordwrap$(OFILE)\
			$(MTOBJODIR)/writemsg$(OFILE)\
			$(MTOBJODIR)/xtrn$(OFILE)\
			$(MTOBJODIR)/xtrn_sec$(OFILE)\
			$(MTOBJODIR)/ver$(OFILE)\
			$(MTOBJODIR)/ssl$(OFILE)

# Must add new additions to MONO_OBJS too!
CON_OBJS	= $(MTOBJODIR)/sbbscon$(OFILE) \
			$(MTOBJODIR)/sbbs_status$(OFILE) \
			$(MTOBJODIR)/sbbs_ini$(OFILE)

# Must add new additions to MONO_OBJS too!
FTP_OBJS	= $(MTOBJODIR)/ftpsrvr$(OFILE) \
			$(MTOBJODIR)/nopen$(OFILE)

# Must add new additions to MONO_OBJS too!
MAIL_OBJS	= $(MTOBJODIR)/mailsrvr$(OFILE) \
			$(MTOBJODIR)/mxlookup$(OFILE) \
 		  	$(MTOBJODIR)/mime$(OFILE) \
 		  	$(MTOBJODIR)/nopen$(OFILE) \
 		  	$(MTOBJODIR)/ars$(OFILE)

# Must add new additions to MONO_OBJS too!
WEB_OBJS	= $(MTOBJODIR)/websrvr$(OFILE) \
			$(MTOBJODIR)/ars$(OFILE) \
			$(MTOBJODIR)/ringbuf$(OFILE)

# Must add new additions to MONO_OBJS too!
SERVICE_OBJS	= $(MTOBJODIR)/services$(OFILE) \
			$(MTOBJODIR)/sbbs_ini$(OFILE)

MONO_OBJS	= \
			$(MTOBJODIR)/ftpsrvr$(OFILE) \
			$(MTOBJODIR)/mailsrvr$(OFILE) \
 		  	$(MTOBJODIR)/mime$(OFILE) \
			$(MTOBJODIR)/mxlookup$(OFILE) \
			$(MTOBJODIR)/sbbs_ini$(OFILE) \
			$(MTOBJODIR)/sbbscon$(OFILE) \
			$(MTOBJODIR)/services$(OFILE) \
			$(MTOBJODIR)/websrvr$(OFILE) \
			$(MTOBJODIR)/ssl$(OFILE)

BAJA_OBJS = \
			$(OBJODIR)/baja$(OFILE) \
			$(OBJODIR)/ars$(OFILE)

UNBAJA_OBJS = \
			$(OBJODIR)/unbaja$(OFILE)

NODE_OBJS = $(OBJODIR)/node$(OFILE)

FIXSMB_OBJS = \
			$(OBJODIR)/fixsmb$(OFILE) \
			$(OBJODIR)/str_util$(OFILE)

CHKSMB_OBJS = \
			$(OBJODIR)/chksmb$(OFILE)

SMBUTIL_OBJS = \
			$(OBJODIR)/smbutil$(OFILE) \
			$(OBJODIR)/str_util$(OFILE)

SBBSECHO_OBJS = 	$(LOAD_CFG_OBJS) \
			$(OBJODIR)/sbbsecho$(OFILE) \
			$(OBJODIR)/date_str$(OFILE) \
			$(OBJODIR)/scfgsave$(OFILE) \
			$(OBJODIR)/dat_rec$(OFILE) \
			$(OBJODIR)/filedat$(OFILE) \
			$(OBJODIR)/userdat$(OFILE) \
			$(OBJODIR)/rechocfg$(OFILE) \
			$(OBJODIR)/msg_id$(OFILE) \
			$(OBJODIR)/msgdate$(OFILE) \
			$(OBJODIR)/getmail$(OFILE) \
			$(OBJODIR)/sauce$(OFILE) \
			$(SMB_OBJS)

ECHOCFG_OBJS = \
			$(MTOBJODIR)/echocfg$(OFILE) \
			$(MTOBJODIR)/rechocfg$(OFILE) \
			$(MTOBJODIR)/str_util$(OFILE) \
			$(UIFC_OBJS) \
			$(MTOBJODIR)/nopen$(OFILE)

ADDFILES_OBJS = 	$(LOAD_CFG_OBJS) \
			$(OBJODIR)/addfiles$(OFILE) \
			$(OBJODIR)/date_str$(OFILE) \
			$(OBJODIR)/dat_rec$(OFILE) \
			$(OBJODIR)/userdat$(OFILE) \
			$(OBJODIR)/msgdate$(OFILE) \
			$(OBJODIR)/filedat$(OFILE) \
			$(OBJODIR)/sauce$(OFILE)

FILELIST_OBJS = 	$(LOAD_CFG_OBJS) \
			$(OBJODIR)/filelist$(OFILE) \
			$(OBJODIR)/date_str$(OFILE) \
			$(OBJODIR)/dat_rec$(OFILE) \
			$(OBJODIR)/filedat$(OFILE) \
			$(OBJODIR)/sauce$(OFILE) \
			$(OBJODIR)/userdat$(OFILE)

MAKEUSER_OBJS = 	$(LOAD_CFG_OBJS) \
			$(OBJODIR)/makeuser$(OFILE) \
			$(OBJODIR)/date_str$(OFILE) \
			$(OBJODIR)/dat_rec$(OFILE) \
			$(OBJODIR)/userdat$(OFILE) \
			$(OBJODIR)/msgdate$(OFILE)

JSEXEC_OBJS = \
			$(MTOBJODIR)/jsexec$(OFILE) \
			$(MTOBJODIR)/js_uifc$(OFILE) \
			$(MTOBJODIR)/js_conio$(OFILE)

JSDOOR_OBJS = 		$(LOAD_CFG_OBJS) \
			$(MTOBJODIR)/date_str$(OFILE) \
			$(MTOBJODIR)/dat_rec$(OFILE) \
			$(MTOBJODIR)/jsdoor$(OFILE) \
			$(MTOBJODIR)/jsdebug$(OFILE) \
			$(MTOBJODIR)/js_archive$(OFILE) \
			$(MTOBJODIR)/js_uifc$(OFILE) \
			$(MTOBJODIR)/js_conio$(OFILE) \
			$(MTOBJODIR)/js_request$(OFILE) \
			$(MTOBJODIR)/js_socket$(OFILE) \
			$(MTOBJODIR)/comio$(OFILE)\
			$(MTOBJODIR)/js_client$(OFILE) \
			$(MTOBJODIR)/js_com$(OFILE) \
			$(MTOBJODIR)/js_cryptcon$(OFILE) \
			$(MTOBJODIR)/js_cryptcert$(OFILE) \
			$(MTOBJODIR)/js_cryptkeyset$(OFILE) \
			$(MTOBJODIR)/js_global$(OFILE) \
			$(MTOBJODIR)/js_rtpool$(OFILE) \
			$(MTOBJODIR)/js_sprintf$(OFILE) \
			$(MTOBJODIR)/js_file$(OFILE) \
			$(MTOBJODIR)/js_internal$(OFILE) \
			$(MTOBJODIR)/js_queue$(OFILE) \
			$(MTOBJODIR)/js_server$(OFILE) \
			$(MTOBJODIR)/sockopts$(OFILE)\
			$(MTOBJODIR)/ssl$(OFILE)\
			$(MTOBJODIR)/scfgsave$(OFILE)\
			$(MTOBJODIR)/wordwrap$(OFILE)\
			$(MTOBJODIR)/userdat$(OFILE)\
			$(MTOBJODIR)/msgdate$(OFILE)\
			$(MTOBJODIR)/filedat$(OFILE)\
			$(MTOBJODIR)/sauce$(OFILE)\
			$(MTOBJODIR)/getstats$(OFILE)\

SEXYZ_OBJS = \
			$(MTOBJODIR)/sexyz$(OFILE) \
			$(MTOBJODIR)/xmodem$(OFILE) \
			$(MTOBJODIR)/zmodem$(OFILE) \
			$(MTOBJODIR)/ringbuf$(OFILE) \
			$(MTOBJODIR)/nopen$(OFILE) \
			$(MTOBJODIR)/telnet$(OFILE)

QWKNODES_OBJS = 	$(LOAD_CFG_OBJS) \
			$(OBJODIR)/qwknodes$(OFILE) \
			$(OBJODIR)/date_str$(OFILE)

SLOG_OBJS = \
			$(OBJODIR)/slog$(OFILE) \
			$(OBJODIR)/nopen$(OFILE)

ALLUSERS_OBJS = \
			$(OBJODIR)/allusers$(OFILE) \
			$(OBJODIR)/str_util$(OFILE) \
			$(OBJODIR)/ars$(OFILE)

DELFILES_OBJS = 	$(LOAD_CFG_OBJS) \
			$(OBJODIR)/delfiles$(OFILE) \
			$(OBJODIR)/filedat$(OFILE) \
			$(OBJODIR)/sauce$(OFILE) \
			$(OBJODIR)/userdat$(OFILE) \
			$(OBJODIR)/dat_rec$(OFILE)

DUPEFIND_OBJS = 	$(LOAD_CFG_OBJS) \
			$(OBJODIR)/dupefind$(OFILE)

SMBACTIV_OBJS = 	$(LOAD_CFG_OBJS) \
			$(OBJODIR)/smbactiv$(OFILE)

DSTSEDIT_OBJS = \
			$(OBJODIR)/dstsedit$(OFILE)\
			$(OBJODIR)/date_str$(OFILE) \
			$(OBJODIR)/str_util$(OFILE) \
			$(OBJODIR)/nopen$(OFILE)

READSAUCE_OBJS = \
			$(OBJODIR)/readsauce$(OFILE)

SHOWSTAT_OBJS = \
			$(OBJODIR)/showstat$(OFILE)

PKTDUMP_OBJS =		$(OBJODIR)/pktdump$(OFILE)

FMSGDUMP_OBJS = 	$(OBJODIR)/fmsgdump$(OFILE)

UPGRADE_TO_V319_OBJS  =	$(LOAD_CFG_OBJS) \
                        $(OBJODIR)/filedat$(OFILE) \
                        $(OBJODIR)/sauce$(OFILE) \
                        $(OBJODIR)/userdat$(OFILE) \
                        $(OBJODIR)/dat_rec$(OFILE) \
			$(OBJODIR)/upgrade_to_v319$(OFILE)

LOAD_CFG_OBJS = \
			$(OBJODIR)/load_cfg$(OFILE) \
			$(OBJODIR)/readtext$(OFILE) \
			$(OBJODIR)/text_defaults$(OFILE) \
			$(OBJODIR)/scfglib1$(OFILE) \
			$(OBJODIR)/scfglib2$(OFILE) \
			$(OBJODIR)/str_util$(OFILE) \
			$(OBJODIR)/ars$(OFILE) \
			$(OBJODIR)/nopen$(OFILE)

