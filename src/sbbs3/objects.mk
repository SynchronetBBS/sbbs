# objects.mk

# Make 'include file' listing object files for SBBS.DLL

# $Id: objects.mk,v 1.76 2020/04/03 19:54:31 rswindell Exp $(DIRSEP)09$(DIRSEP)08 07:23:54 deuce Exp $

# OBJODIR, SLASH, and OFILE must be pre-defined

OBJS	=	$(MTOBJODIR)$(DIRSEP)ansiterm$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)answer$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)ars$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)atcodes$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)bat_xfer$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)bulkmail$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)chat$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)chk_ar$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)comio$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)con_hi$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)con_out$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)dat_rec$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)data$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)data_ovl$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)date_str$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)download$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)email$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)exec$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)execfile$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)execfunc$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)execmisc$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)execmsg$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)execnet$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)fido$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)file$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)filedat$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)getkey$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)getmail$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)getmsg$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)getnode$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)getstats$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)getstr$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)inkey$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)ident$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)jsdebug$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_bbs$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_client$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_com$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_console$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_cryptcert$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_cryptcon$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_cryptkeyset$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_file$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_file_area$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_global$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_internal$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_msg_area$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_msgbase$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_queue$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_request$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_rtpool$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_server$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_socket$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_sprintf$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_system$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_user$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_xtrn_area$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)listfile$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)load_cfg$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)logfile$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)login$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)logon$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)logout$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)mail$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)main$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)msg_id$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)msgdate$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)msgtoqwk$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)netmail$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)newuser$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)nopen$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)pack_qwk$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)pack_rep$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)postmsg$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)prntfile$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)putmsg$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)putnode$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)qwk$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)qwktomsg$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)readmail$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)readmsgs$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)readtext$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)ringbuf$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scandirs$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scansubs$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfglib1$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfglib2$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfgsave$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)sockopts$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)sortdir$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)str$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)str_util$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)telgate$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)telnet$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)text_defaults$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)text_sec$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)tmp_xfer$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)un_qwk$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)un_rep$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)upload$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)userdat$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)useredit$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)viewfile$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)wordwrap$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)writemsg$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)xtrn$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)xtrn_sec$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)ver$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)ssl$(OFILE)

# Must add new additions to MONO_OBJS too!
CON_OBJS	= $(MTOBJODIR)$(DIRSEP)sbbscon$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)sbbs_status$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)sbbs_ini$(OFILE)

# Must add new additions to MONO_OBJS too!
FTP_OBJS	= $(MTOBJODIR)$(DIRSEP)ftpsrvr$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)nopen$(OFILE)

# Must add new additions to MONO_OBJS too!
MAIL_OBJS	= $(MTOBJODIR)$(DIRSEP)mailsrvr$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)mxlookup$(OFILE) \
 		  	$(MTOBJODIR)$(DIRSEP)mime$(OFILE) \
 		  	$(MTOBJODIR)$(DIRSEP)nopen$(OFILE) \
 		  	$(MTOBJODIR)$(DIRSEP)ars$(OFILE)

# Must add new additions to MONO_OBJS too!
WEB_OBJS	= $(MTOBJODIR)$(DIRSEP)websrvr$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)ars$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)ringbuf$(OFILE)

# Must add new additions to MONO_OBJS too!
SERVICE_OBJS	= $(MTOBJODIR)$(DIRSEP)services$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)sbbs_ini$(OFILE)

MONO_OBJS	= \
			$(MTOBJODIR)$(DIRSEP)ftpsrvr$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)mailsrvr$(OFILE) \
 		  	$(MTOBJODIR)$(DIRSEP)mime$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)mxlookup$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)sbbs_ini$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)sbbscon$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)services$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)websrvr$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)ssl$(OFILE)

BAJA_OBJS = \
			$(OBJODIR)$(DIRSEP)baja$(OFILE) \
			$(OBJODIR)$(DIRSEP)ars$(OFILE)

UNBAJA_OBJS = \
			$(OBJODIR)$(DIRSEP)unbaja$(OFILE)

NODE_OBJS = $(OBJODIR)$(DIRSEP)node$(OFILE)

FIXSMB_OBJS = \
			$(OBJODIR)$(DIRSEP)fixsmb$(OFILE) \
			$(OBJODIR)$(DIRSEP)str_util$(OFILE)

CHKSMB_OBJS = \
			$(OBJODIR)$(DIRSEP)chksmb$(OFILE)

SMBUTIL_OBJS = \
			$(OBJODIR)$(DIRSEP)smbutil$(OFILE) \
			$(OBJODIR)$(DIRSEP)str_util$(OFILE)

SBBSECHO_OBJS = \
			$(OBJODIR)$(DIRSEP)sbbsecho$(OFILE) \
			$(OBJODIR)$(DIRSEP)ars$(OFILE) \
			$(OBJODIR)$(DIRSEP)date_str$(OFILE) \
			$(OBJODIR)$(DIRSEP)load_cfg$(OFILE) \
			$(OBJODIR)$(DIRSEP)scfglib1$(OFILE) \
			$(OBJODIR)$(DIRSEP)scfglib2$(OFILE) \
			$(OBJODIR)$(DIRSEP)scfgsave$(OFILE) \
			$(OBJODIR)$(DIRSEP)nopen$(OFILE) \
			$(OBJODIR)$(DIRSEP)str_util$(OFILE) \
			$(OBJODIR)$(DIRSEP)dat_rec$(OFILE) \
			$(OBJODIR)$(DIRSEP)userdat$(OFILE) \
			$(OBJODIR)$(DIRSEP)rechocfg$(OFILE) \
			$(OBJODIR)$(DIRSEP)msg_id$(OFILE) \
			$(OBJODIR)$(DIRSEP)msgdate$(OFILE) \
			$(OBJODIR)$(DIRSEP)getmail$(OFILE) \
			$(SMB_OBJS)

ECHOCFG_OBJS = \
			$(MTOBJODIR)$(DIRSEP)echocfg$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)rechocfg$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)str_util$(OFILE) \
			$(UIFC_OBJS) \
			$(MTOBJODIR)$(DIRSEP)nopen$(OFILE)

ADDFILES_OBJS = \
			$(OBJODIR)$(DIRSEP)addfiles$(OFILE) \
			$(OBJODIR)$(DIRSEP)ars$(OFILE) \
			$(OBJODIR)$(DIRSEP)date_str$(OFILE) \
			$(OBJODIR)$(DIRSEP)load_cfg$(OFILE) \
			$(OBJODIR)$(DIRSEP)scfglib1$(OFILE) \
			$(OBJODIR)$(DIRSEP)scfglib2$(OFILE) \
			$(OBJODIR)$(DIRSEP)nopen$(OFILE) \
			$(OBJODIR)$(DIRSEP)str_util$(OFILE) \
			$(OBJODIR)$(DIRSEP)dat_rec$(OFILE) \
			$(OBJODIR)$(DIRSEP)userdat$(OFILE) \
			$(OBJODIR)$(DIRSEP)msgdate$(OFILE) \
			$(OBJODIR)$(DIRSEP)filedat$(OFILE)

FILELIST_OBJS = \
			$(OBJODIR)$(DIRSEP)filelist$(OFILE) \
			$(OBJODIR)$(DIRSEP)ars$(OFILE) \
			$(OBJODIR)$(DIRSEP)date_str$(OFILE) \
			$(OBJODIR)$(DIRSEP)load_cfg$(OFILE) \
			$(OBJODIR)$(DIRSEP)scfglib1$(OFILE) \
			$(OBJODIR)$(DIRSEP)scfglib2$(OFILE) \
			$(OBJODIR)$(DIRSEP)nopen$(OFILE) \
			$(OBJODIR)$(DIRSEP)str_util$(OFILE) \
			$(OBJODIR)$(DIRSEP)dat_rec$(OFILE) \
			$(OBJODIR)$(DIRSEP)filedat$(OFILE)

MAKEUSER_OBJS = \
			$(OBJODIR)$(DIRSEP)makeuser$(OFILE) \
			$(OBJODIR)$(DIRSEP)ars$(OFILE) \
			$(OBJODIR)$(DIRSEP)date_str$(OFILE) \
			$(OBJODIR)$(DIRSEP)load_cfg$(OFILE) \
			$(OBJODIR)$(DIRSEP)scfglib1$(OFILE) \
			$(OBJODIR)$(DIRSEP)scfglib2$(OFILE) \
			$(OBJODIR)$(DIRSEP)nopen$(OFILE) \
			$(OBJODIR)$(DIRSEP)str_util$(OFILE) \
			$(OBJODIR)$(DIRSEP)dat_rec$(OFILE) \
			$(OBJODIR)$(DIRSEP)userdat$(OFILE) \
			$(OBJODIR)$(DIRSEP)msgdate$(OFILE)

JSEXEC_OBJS = \
			$(MTOBJODIR)$(DIRSEP)jsexec$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_uifc$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_conio$(OFILE)

JSDOOR_OBJS = \
			$(MTOBJODIR)$(DIRSEP)ars$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)date_str$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)dat_rec$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)jsdoor$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)jsdebug$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_uifc$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_conio$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_request$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_socket$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)comio$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)js_client$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_com$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_cryptcon$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_cryptcert$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_cryptkeyset$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_global$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_rtpool$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_sprintf$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_file$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_internal$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_queue$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)js_server$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)nopen$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)str_util$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)sockopts$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)ssl$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)load_cfg$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)readtext$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)text_defaults$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfgsave$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfglib1$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)scfglib2$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)wordwrap$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)userdat$(OFILE)\
			$(MTOBJODIR)$(DIRSEP)msgdate$(OFILE)\

SEXYZ_OBJS = \
			$(MTOBJODIR)$(DIRSEP)sexyz$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)xmodem$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)zmodem$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)ringbuf$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)nopen$(OFILE) \
			$(MTOBJODIR)$(DIRSEP)telnet$(OFILE)

QWKNODES_OBJS = \
			$(OBJODIR)$(DIRSEP)qwknodes$(OFILE)\
			$(OBJODIR)$(DIRSEP)date_str$(OFILE)\
			$(OBJODIR)$(DIRSEP)nopen$(OFILE) \
			$(OBJODIR)$(DIRSEP)load_cfg$(OFILE)\
			$(OBJODIR)$(DIRSEP)scfglib1$(OFILE) \
			$(OBJODIR)$(DIRSEP)scfglib2$(OFILE) \
			$(OBJODIR)$(DIRSEP)str_util$(OFILE) \
			$(OBJODIR)$(DIRSEP)ars$(OFILE)

SLOG_OBJS = \
			$(OBJODIR)$(DIRSEP)slog$(OFILE) \
			$(OBJODIR)$(DIRSEP)nopen$(OFILE)

ALLUSERS_OBJS = \
			$(OBJODIR)$(DIRSEP)allusers$(OFILE) \
			$(OBJODIR)$(DIRSEP)str_util$(OFILE) \
			$(OBJODIR)$(DIRSEP)ars$(OFILE)

DELFILES_OBJS = \
			$(OBJODIR)$(DIRSEP)delfiles$(OFILE) \
			$(OBJODIR)$(DIRSEP)load_cfg$(OFILE)\
			$(OBJODIR)$(DIRSEP)scfglib1$(OFILE) \
			$(OBJODIR)$(DIRSEP)scfglib2$(OFILE) \
			$(OBJODIR)$(DIRSEP)str_util$(OFILE) \
			$(OBJODIR)$(DIRSEP)ars$(OFILE) \
			$(OBJODIR)$(DIRSEP)nopen$(OFILE) \
			$(OBJODIR)$(DIRSEP)filedat$(OFILE) \
			$(OBJODIR)$(DIRSEP)dat_rec$(OFILE)

DUPEFIND_OBJS = \
			$(OBJODIR)$(DIRSEP)dupefind$(OFILE) \
			$(OBJODIR)$(DIRSEP)load_cfg$(OFILE)\
			$(OBJODIR)$(DIRSEP)scfglib1$(OFILE) \
			$(OBJODIR)$(DIRSEP)scfglib2$(OFILE) \
			$(OBJODIR)$(DIRSEP)str_util$(OFILE) \
			$(OBJODIR)$(DIRSEP)ars$(OFILE) \
			$(OBJODIR)$(DIRSEP)nopen$(OFILE)

SMBACTIV_OBJS = \
			$(OBJODIR)$(DIRSEP)smbactiv$(OFILE)\
			$(OBJODIR)$(DIRSEP)load_cfg$(OFILE)\
			$(OBJODIR)$(DIRSEP)scfglib1$(OFILE) \
			$(OBJODIR)$(DIRSEP)scfglib2$(OFILE) \
			$(OBJODIR)$(DIRSEP)str_util$(OFILE) \
			$(OBJODIR)$(DIRSEP)ars$(OFILE) \
			$(OBJODIR)$(DIRSEP)nopen$(OFILE)

DSTSEDIT_OBJS = \
			$(OBJODIR)$(DIRSEP)dstsedit$(OFILE)\
			$(OBJODIR)$(DIRSEP)date_str$(OFILE) \
			$(OBJODIR)$(DIRSEP)str_util$(OFILE) \
			$(OBJODIR)$(DIRSEP)nopen$(OFILE)

READSAUCE_OBJS = \
			$(OBJODIR)$(DIRSEP)readsauce$(OFILE)

SHOWSTAT_OBJS = \
			$(OBJODIR)$(DIRSEP)showstat$(OFILE)

PKTDUMP_OBJS =		$(OBJODIR)$(DIRSEP)pktdump$(OFILE)

FMSGDUMP_OBJS = 	$(OBJODIR)$(DIRSEP)fmsgdump$(OFILE)

