/* sbbs_ini.c */

/* Synchronet console configuration (.ini) file routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs_ini.h"
#include "sbbsdefs.h"	/* JAVASCRIPT_* macros */
#include "ini_opts.h"	/* bbs_options, ftp_options, etc. */
#include "dirwrap.h"	/* backslash */

static const char*	nulstr="";
static const char*	strOptions="Options";
static const char*	strInterface="Interface";
static const char*	strHostName="HostName";
static const char*	strLogMask="LogMask";

#define DEFAULT_LOG_MASK		0x1f	/* EMERG|ALERT|CRIT|ERR|WARNING */
#define DEFAULT_MAX_MSG_SIZE    (10*1024*1024)	/* 10MB */

void sbbs_get_ini_fname(char* ini_file, char* ctrl_dir, char* pHostName)
{
    char host_name[128];
    
    if(pHostName==NULL) {
#if defined(_WINSOCKAPI_)
        WSADATA WSAData;
        WSAStartup(MAKEWORD(1,1), &WSAData); /* req'd for gethostname */
#endif
    	gethostname(host_name,sizeof(host_name)-1);
#if defined(_WINSOCKAPI_)
        WSACleanup();
#endif
        pHostName=host_name;
    }
	sprintf(ini_file,"%s%c%s.ini",ctrl_dir,PATH_DELIM,pHostName);
#if defined(__unix__) && defined(PREFIX)
	if(!fexistcase(ini_file))
		sprintf(ini_file,PREFIX"/etc/sbbs.ini");
#endif
	if(!fexistcase(ini_file))
		sprintf(ini_file,"%s%csbbs.ini",ctrl_dir,PATH_DELIM);
}

static void read_ini_globals(FILE* fp, global_startup_t* global)
{
	const char* section = "Global";
	char		value[INI_MAX_VALUE_LEN];

	if(*iniGetString(fp,section,"CtrlDirectory",nulstr,value)) {
	    SAFECOPY(global->ctrl_dir,value);
		backslash(global->ctrl_dir);
    }

	if(*iniGetString(fp,section,"TempDirectory",nulstr,value)) {
	    SAFECOPY(global->temp_dir,value);
		backslash(global->temp_dir);
    }

	if(*iniGetString(fp,section,strHostName,nulstr,value))
        SAFECOPY(global->host_name,value);

	global->sem_chk_freq=iniGetShortInt(fp,section,strSemFileCheckFrequency,0);
	global->interface_addr=iniGetIpAddress(fp,section,strInterface,INADDR_ANY);
	global->log_mask=iniGetBitField(fp,section,strLogMask,log_mask_bits,DEFAULT_LOG_MASK);

	global->js.max_bytes		= iniGetInteger(fp,section,strJavaScriptMaxBytes		,JAVASCRIPT_MAX_BYTES);
	global->js.cx_stack			= iniGetInteger(fp,section,strJavaScriptContextStack	,JAVASCRIPT_CONTEXT_STACK);
	global->js.branch_limit		= iniGetInteger(fp,section,strJavaScriptBranchLimit		,JAVASCRIPT_BRANCH_LIMIT);
	global->js.gc_interval		= iniGetInteger(fp,section,strJavaScriptGcInterval		,JAVASCRIPT_GC_INTERVAL);
	global->js.yield_interval	= iniGetInteger(fp,section,strJavaScriptYieldInterval	,JAVASCRIPT_YIELD_INTERVAL);
}


void sbbs_read_ini(
	 FILE*					fp
	,global_startup_t*		global
	,BOOL*					run_bbs
	,bbs_startup_t*			bbs
	,BOOL*					run_ftp
	,ftp_startup_t*			ftp
	,BOOL*					run_web
	,web_startup_t*			web
	,BOOL*					run_mail		
	,mail_startup_t*		mail
	,BOOL*					run_services
	,services_startup_t*	services
	)
{
	const char*	section;
	const char* default_term_ansi;
	const char*	default_cgi_temp;
	const char*	default_dosemu_path;
	char		value[INI_MAX_VALUE_LEN];
	global_startup_t global_buf;

	if(global==NULL)
		global=&global_buf;

	read_ini_globals(fp, global);

	if(global->ctrl_dir[0]) {
		if(bbs!=NULL)		SAFECOPY(bbs->ctrl_dir,global->ctrl_dir);
		if(ftp!=NULL)		SAFECOPY(ftp->ctrl_dir,global->ctrl_dir);
		if(mail!=NULL)		SAFECOPY(mail->ctrl_dir,global->ctrl_dir);
		if(services!=NULL)	SAFECOPY(services->ctrl_dir,global->ctrl_dir);
	}
	if(global->temp_dir[0]) {
		if(bbs!=NULL)		SAFECOPY(bbs->temp_dir,global->temp_dir);
		if(ftp!=NULL)		SAFECOPY(ftp->temp_dir,global->temp_dir);
	}
													
	/***********************************************************************/
	if(bbs!=NULL) {

		section = "BBS";

		if(run_bbs!=NULL)
			*run_bbs=iniGetBool(fp,section,"AutoStart",TRUE);

		bbs->telnet_interface
			=iniGetIpAddress(fp,section,"TelnetInterface",global->interface_addr);
		bbs->telnet_port
			=iniGetShortInt(fp,section,"TelnetPort",IPPORT_TELNET);

		bbs->rlogin_interface
			=iniGetIpAddress(fp,section,"RLoginInterface",global->interface_addr);
		bbs->rlogin_port
			=iniGetShortInt(fp,section,"RLoginPort",513);

		bbs->first_node
			=iniGetShortInt(fp,section,"FirstNode",1);
		bbs->last_node
			=iniGetShortInt(fp,section,"LastNode",4);

		bbs->outbuf_highwater_mark
			=iniGetShortInt(fp,section,"OutbufHighwaterMark",1024);
		bbs->outbuf_drain_timeout
			=iniGetShortInt(fp,section,"OutbufDrainTimeout",10);

		bbs->sem_chk_freq
			=iniGetShortInt(fp,section,strSemFileCheckFrequency,global->sem_chk_freq);

		bbs->xtrn_polls_before_yield
			=iniGetInteger(fp,section,"ExternalYield",10);

		/* JavaScript operating parameters */
		bbs->js_max_bytes
			=iniGetInteger(fp,section,strJavaScriptMaxBytes		,global->js.max_bytes);
		bbs->js_cx_stack
			=iniGetInteger(fp,section,strJavaScriptContextStack	,global->js.cx_stack);
		bbs->js_branch_limit
			=iniGetInteger(fp,section,strJavaScriptBranchLimit	,global->js.branch_limit);
		bbs->js_gc_interval
			=iniGetInteger(fp,section,strJavaScriptGcInterval	,global->js.gc_interval);
		bbs->js_yield_interval
			=iniGetInteger(fp,section,strJavaScriptYieldInterval,global->js.yield_interval);

		SAFECOPY(bbs->host_name
			,iniGetString(fp,section,strHostName,global->host_name,value));

		/* Set default terminal type to "stock" termcap closest to "ansi-bbs" */
	#if defined(__FreeBSD__)
		default_term_ansi="cons25";
	#else
		default_term_ansi="pc3";
	#endif

		SAFECOPY(bbs->xtrn_term_ansi
			,iniGetString(fp,section,"ExternalTermANSI",default_term_ansi,value));
		SAFECOPY(bbs->xtrn_term_dumb
			,iniGetString(fp,section,"ExternalTermDumb","dumb",value));

	#if defined(__FreeBSD__)
		default_dosemu_path="/usr/bin/doscmd";
	#else
		default_dosemu_path="/usr/bin/dosemu.bin";
	#endif

		SAFECOPY(bbs->dosemu_path
			,iniGetString(fp,section,"DOSemuPath",default_dosemu_path,value));

		SAFECOPY(bbs->answer_sound
			,iniGetString(fp,section,"AnswerSound",nulstr,value));
		SAFECOPY(bbs->hangup_sound
			,iniGetString(fp,section,"HangupSound",nulstr,value));

		bbs->log_mask
			=iniGetBitField(fp,section,strLogMask,log_mask_bits,global->log_mask);
		bbs->options
			=iniGetBitField(fp,section,strOptions,bbs_options
				,BBS_OPT_XTRN_MINIMIZED|BBS_OPT_SYSOP_AVAILABLE);
	}

	/***********************************************************************/
	if(ftp!=NULL) {

		section = "FTP";

		if(run_ftp!=NULL)
			*run_ftp=iniGetBool(fp,section,"AutoStart",TRUE);

		ftp->interface_addr
			=iniGetIpAddress(fp,section,strInterface,global->interface_addr);
		ftp->port
			=iniGetShortInt(fp,section,"Port",IPPORT_FTP);
		ftp->max_clients
			=iniGetShortInt(fp,section,"MaxClients",10);
		ftp->max_inactivity
			=iniGetShortInt(fp,section,"MaxInactivity",300);	/* seconds */
		ftp->qwk_timeout
			=iniGetShortInt(fp,section,"QwkTimeout",600);		/* seconds */
		ftp->sem_chk_freq
			=iniGetShortInt(fp,section,strSemFileCheckFrequency,global->sem_chk_freq);

		/* JavaScript Operating Parameters */
		ftp->js_max_bytes
			=iniGetInteger(fp,section,strJavaScriptMaxBytes		,global->js.max_bytes);
		ftp->js_cx_stack
			=iniGetInteger(fp,section,strJavaScriptContextStack	,global->js.cx_stack);

		SAFECOPY(ftp->host_name
			,iniGetString(fp,section,strHostName,global->host_name,value));

		SAFECOPY(ftp->index_file_name
			,iniGetString(fp,section,"IndexFileName","00index",value));
		SAFECOPY(ftp->html_index_file
			,iniGetString(fp,section,"HtmlIndexFile","00index.html",value));
		SAFECOPY(ftp->html_index_script
			,iniGetString(fp,section,"HtmlIndexScript","ftp-html.js",value));

		SAFECOPY(ftp->answer_sound
			,iniGetString(fp,section,"AnswerSound",nulstr,value));
		SAFECOPY(ftp->hangup_sound
			,iniGetString(fp,section,"HangupSound",nulstr,value));
		SAFECOPY(ftp->hack_sound
			,iniGetString(fp,section,"HackAttemptSound",nulstr,value));

		ftp->log_mask
			=iniGetBitField(fp,section,strLogMask,log_mask_bits,global->log_mask);
		ftp->options
			=iniGetBitField(fp,section,strOptions,ftp_options
				,FTP_OPT_INDEX_FILE|FTP_OPT_HTML_INDEX_FILE|FTP_OPT_ALLOW_QWK);
	}

	/***********************************************************************/
	if(mail!=NULL) {

		section = "Mail";

		if(run_mail!=NULL)
			*run_mail=iniGetBool(fp,section,"AutoStart",TRUE);

		mail->interface_addr
			=iniGetIpAddress(fp,section,strInterface,global->interface_addr);
		mail->smtp_port
			=iniGetShortInt(fp,section,"SMTPPort",IPPORT_SMTP);
		mail->pop3_port
			=iniGetShortInt(fp,section,"POP3Port",IPPORT_POP3);
		mail->relay_port
			=iniGetShortInt(fp,section,"RelayPort",IPPORT_SMTP);
		mail->max_clients
			=iniGetShortInt(fp,section,"MaxClients",10);
		mail->max_inactivity
			=iniGetShortInt(fp,section,"MaxInactivity",120);		/* seconds */
		mail->max_delivery_attempts
			=iniGetShortInt(fp,section,"MaxDeliveryAttempts",50);
		mail->rescan_frequency
			=iniGetShortInt(fp,section,"RescanFrequency",3600);	/* 60 minutes */
		mail->sem_chk_freq
			=iniGetShortInt(fp,section,strSemFileCheckFrequency,global->sem_chk_freq);
		mail->lines_per_yield
			=iniGetShortInt(fp,section,"LinesPerYield",10);
		mail->max_recipients
			=iniGetShortInt(fp,section,"MaxRecipients",100);
		mail->max_msg_size
			=iniGetInteger(fp,section,"MaxMsgSize",DEFAULT_MAX_MSG_SIZE);

		SAFECOPY(mail->host_name
			,iniGetString(fp,section,strHostName,global->host_name,value));

		SAFECOPY(mail->relay_server
			,iniGetString(fp,section,"RelayServer",mail->relay_server,value));
		SAFECOPY(mail->dns_server
			,iniGetString(fp,section,"DNSServer",mail->dns_server,value));

		SAFECOPY(mail->default_user
			,iniGetString(fp,section,"DefaultUser",nulstr,value));

		SAFECOPY(mail->dnsbl_hdr
			,iniGetString(fp,section,"DNSBlacklistHeader","X-DNSBL",value));
		SAFECOPY(mail->dnsbl_tag
			,iniGetString(fp,section,"DNSBlacklistSubject","SPAM",value));

		SAFECOPY(mail->pop3_sound
			,iniGetString(fp,section,"POP3Sound",nulstr,value));
		SAFECOPY(mail->inbound_sound
			,iniGetString(fp,section,"InboundSound",nulstr,value));
		SAFECOPY(mail->outbound_sound
			,iniGetString(fp,section,"OutboundSound",nulstr,value));

		SAFECOPY(mail->proc_cfg_file
			,iniGetString(fp,section,"ProcessConfigFile","mailproc.cfg",value));

		/* JavaScript Operating Parameters */
		mail->js_max_bytes
			=iniGetInteger(fp,section,strJavaScriptMaxBytes		,global->js.max_bytes);
		mail->js_cx_stack
			=iniGetInteger(fp,section,strJavaScriptContextStack	,global->js.cx_stack);

		mail->log_mask
			=iniGetBitField(fp,section,strLogMask,log_mask_bits,global->log_mask);
		mail->options
			=iniGetBitField(fp,section,strOptions,mail_options
				,MAIL_OPT_ALLOW_POP3);
	}

	/***********************************************************************/
	if(services!=NULL) {

		section = "Services";

		if(run_services!=NULL)
			*run_services=iniGetBool(fp,section,"AutoStart",TRUE);

		services->interface_addr
			=iniGetIpAddress(fp,section,strInterface,global->interface_addr);

		services->sem_chk_freq
			=iniGetShortInt(fp,section,strSemFileCheckFrequency,global->sem_chk_freq);

		/* Configurable JavaScript default parameters */
		services->js_max_bytes
			=iniGetInteger(fp,section,strJavaScriptMaxBytes		,global->js.max_bytes);
		services->js_cx_stack
			=iniGetInteger(fp,section,strJavaScriptContextStack	,global->js.cx_stack);
		services->js_branch_limit
			=iniGetInteger(fp,section,strJavaScriptBranchLimit	,global->js.branch_limit);
		services->js_gc_interval
			=iniGetInteger(fp,section,strJavaScriptGcInterval	,global->js.gc_interval);
		services->js_yield_interval
			=iniGetInteger(fp,section,strJavaScriptYieldInterval,global->js.yield_interval);

		SAFECOPY(services->host_name
			,iniGetString(fp,section,strHostName,global->host_name,value));

		SAFECOPY(services->ini_file
			,iniGetString(fp,section,"iniFile","services.ini",value));

		SAFECOPY(services->cfg_file
			,iniGetString(fp,section,"ConfigFile","services.cfg",value));

		SAFECOPY(services->answer_sound
			,iniGetString(fp,section,"AnswerSound",nulstr,value));
		SAFECOPY(services->hangup_sound
			,iniGetString(fp,section,"HangupSound",nulstr,value));

		services->log_mask
			=iniGetBitField(fp,section,strLogMask,log_mask_bits,global->log_mask);
		services->options
			=iniGetBitField(fp,section,strOptions,service_options
				,BBS_OPT_NO_HOST_LOOKUP);
	}

	/***********************************************************************/
	if(web!=NULL) {

		section = "Web";

		if(run_web!=NULL)
			*run_web=iniGetBool(fp,section,"AutoStart",FALSE);

		web->interface_addr
			=iniGetIpAddress(fp,section,strInterface,global->interface_addr);
		web->port
			=iniGetShortInt(fp,section,"Port",IPPORT_HTTP);
		web->sem_chk_freq
			=iniGetShortInt(fp,section,strSemFileCheckFrequency,global->sem_chk_freq);

		/* JavaScript Operating Parameters */
		web->js_max_bytes
			=iniGetInteger(fp,section,strJavaScriptMaxBytes		,global->js.max_bytes);
		web->js_cx_stack
			=iniGetInteger(fp,section,strJavaScriptContextStack	,global->js.cx_stack);

		SAFECOPY(web->host_name
			,iniGetString(fp,section,strHostName,global->host_name,value));

		SAFECOPY(web->root_dir
			,iniGetString(fp,section,"RootDirectory",WEB_DEFAULT_ROOT_DIR,value));
		SAFECOPY(web->error_dir
			,iniGetString(fp,section,"ErrorDirectory",WEB_DEFAULT_ERROR_DIR,value));
		SAFECOPY(web->cgi_dir
			,iniGetString(fp,section,"CGIDirectory",WEB_DEFAULT_CGI_DIR,value));

		iniFreeStringList(web->index_file_name);
		web->index_file_name
			=iniGetStringList(fp,section,"IndexFileNames", "," ,"index.html,index.ssjs");
		iniFreeStringList(web->cgi_ext);
		web->cgi_ext
			=iniGetStringList(fp,section,"CGIExtensions", "," ,".cgi");
		SAFECOPY(web->ssjs_ext
			,iniGetString(fp,section,"JavaScriptExtension",".ssjs",value));

		web->max_inactivity
			=iniGetShortInt(fp,section,"MaxInactivity",120);		/* seconds */
		web->max_cgi_inactivity
			=iniGetShortInt(fp,section,"MaxCgiInactivity",120);	/* seconds */


		default_cgi_temp = _PATH_TMP;
		SAFECOPY(web->cgi_temp_dir
			,iniGetString(fp,section,"CGITempDirectory",default_cgi_temp,value));

		web->log_mask
			=iniGetBitField(fp,section,strLogMask,log_mask_bits,global->log_mask);
		web->options
			=iniGetBitField(fp,section,strOptions,web_options
				,BBS_OPT_NO_HOST_LOOKUP);
	}
}

BOOL sbbs_write_ini(
	 FILE*					fp
    ,scfg_t*                cfg
	,global_startup_t*		global
	,BOOL					run_bbs
	,bbs_startup_t*			bbs
	,BOOL					run_ftp
	,ftp_startup_t*			ftp
	,BOOL					run_web
	,web_startup_t*			web
	,BOOL					run_mail		
	,mail_startup_t*		mail
	,BOOL					run_services
	,services_startup_t*	services
	)
{
	const char*	section;
	BOOL		result=FALSE;
	str_list_t	list;
	str_list_t*	lp;
	ini_style_t style;
	global_startup_t	global_buf;

	if(global==NULL) {
		read_ini_globals(fp, &global_buf);
		global = &global_buf;
	}
	
	memset(&style, 0, sizeof(style));
	style.key_prefix = "\t";
    style.bit_separator = " | ";

	if((list=iniReadFile(fp))==NULL)
		return(FALSE);
	
	lp=&list;

	do { /* try */

	/***********************************************************************/
	if(bbs!=NULL) {

		section = "BBS";

		if(!iniSetBool(lp,section,"AutoStart",run_bbs,&style))
			break;

		if(bbs->telnet_interface==global->interface_addr)
			iniRemoveValue(lp,section,"TelnetInterface");
		else if(!iniSetIpAddress(lp,section,"TelnetInterface",bbs->telnet_interface,&style))
			break;

		if(!iniSetShortInt(lp,section,"TelnetPort",bbs->telnet_port,&style))
			break;

		if(bbs->rlogin_interface==global->interface_addr)
			iniRemoveValue(lp,section,"RLoginInterface");
		else if(!iniSetIpAddress(lp,section,"RLoginInterface",bbs->rlogin_interface,&style))
			break;

		if(!iniSetShortInt(lp,section,"RLoginPort",bbs->rlogin_port,&style))
			break;
		if(!iniSetShortInt(lp,section,"FirstNode",bbs->first_node,&style))
			break;
		if(!iniSetShortInt(lp,section,"LastNode",bbs->last_node,&style))
			break;
		if(!iniSetShortInt(lp,section,"OutbufHighwaterMark",bbs->outbuf_highwater_mark,&style))
			break;
		if(!iniSetShortInt(lp,section,"OutbufDrainTimeout",bbs->outbuf_drain_timeout,&style))
			break;

		if(bbs->sem_chk_freq==global->sem_chk_freq)
			iniRemoveValue(lp,section,strSemFileCheckFrequency);
		else if(!iniSetShortInt(lp,section,strSemFileCheckFrequency,bbs->sem_chk_freq,&style))
			break;

		if(bbs->log_mask==global->log_mask)
			iniRemoveValue(lp,section,strLogMask);
		else if(!iniSetBitField(lp,section,strLogMask,log_mask_bits,bbs->log_mask,&style))
			break;

		if(!iniSetInteger(lp,section,"ExternalYield",bbs->xtrn_polls_before_yield,&style))
			break;

		/* JavaScript operating parameters */
		
		if(bbs->js_max_bytes==global->js.max_bytes)
			iniRemoveValue(lp,section,strJavaScriptMaxBytes);
		else if(!iniSetInteger(lp,section,strJavaScriptMaxBytes		,bbs->js_max_bytes,&style))
			break;

		if(bbs->js_cx_stack==global->js.cx_stack)
			iniRemoveValue(lp,section,strJavaScriptContextStack);
		else if(!iniSetInteger(lp,section,strJavaScriptContextStack	,bbs->js_cx_stack,&style))
			break;

		if(bbs->js_branch_limit==global->js.branch_limit)
			iniRemoveValue(lp,section,strJavaScriptBranchLimit);
		else if(!iniSetInteger(lp,section,strJavaScriptBranchLimit	,bbs->js_branch_limit,&style))
			break;

		if(bbs->js_gc_interval==global->js.gc_interval)
			iniRemoveValue(lp,section,strJavaScriptGcInterval);
		else if(!iniSetInteger(lp,section,strJavaScriptGcInterval	,bbs->js_gc_interval,&style))
			break;

		if(bbs->js_yield_interval==global->js.yield_interval)
			iniRemoveValue(lp,section,strJavaScriptYieldInterval);
		else if(!iniSetInteger(lp,section,strJavaScriptYieldInterval,bbs->js_yield_interval,&style))
			break;

		if(strcmp(bbs->host_name,global->host_name)==0
            || strcmp(bbs->host_name,cfg->sys_inetaddr)==0)
			iniRemoveKey(lp,section,strHostName);
		else if(!iniSetString(lp,section,strHostName,bbs->host_name,&style))
			break;

		if(!iniSetString(lp,section,"ExternalTermANSI",bbs->xtrn_term_ansi,&style))
			break;
		if(!iniSetString(lp,section,"ExternalTermDumb",bbs->xtrn_term_dumb,&style))
			break;
		if(!iniSetString(lp,section,"DOSemuPath",bbs->dosemu_path,&style))
			break;

		if(!iniSetString(lp,section,"AnswerSound",bbs->answer_sound,&style))
			break;
		if(!iniSetString(lp,section,"HangupSound",bbs->hangup_sound,&style))
			break;

		if(!iniSetBitField(lp,section,strOptions,bbs_options,bbs->options,&style))
			break;
	}
	/***********************************************************************/
	if(ftp!=NULL) {

		section = "FTP";

		if(!iniSetBool(lp,section,"AutoStart",run_ftp,&style))
			break;

		if(ftp->interface_addr==global->interface_addr)
			iniRemoveValue(lp,section,strInterface);
		else if(!iniSetIpAddress(lp,section,strInterface,ftp->interface_addr,&style))
			break;

		if(!iniSetShortInt(lp,section,"Port",ftp->port,&style))
			break;
		if(!iniSetShortInt(lp,section,"MaxClients",ftp->max_clients,&style))
			break;
		if(!iniSetShortInt(lp,section,"MaxInactivity",ftp->max_inactivity,&style))
			break;
		if(!iniSetShortInt(lp,section,"QwkTimeout",ftp->qwk_timeout,&style))
			break;

		if(ftp->sem_chk_freq==global->sem_chk_freq)
			iniRemoveValue(lp,section,strSemFileCheckFrequency);
		else if(!iniSetShortInt(lp,section,strSemFileCheckFrequency,ftp->sem_chk_freq,&style))
			break;

		if(ftp->log_mask==global->log_mask)
			iniRemoveValue(lp,section,strLogMask);
		else if(!iniSetBitField(lp,section,strLogMask,log_mask_bits,ftp->log_mask,&style))
			break;

		/* JavaScript Operating Parameters */
		
		if(ftp->js_max_bytes==global->js.max_bytes)
			iniRemoveValue(lp,section,strJavaScriptMaxBytes);
		else if(!iniSetInteger(lp,section,strJavaScriptMaxBytes		,ftp->js_max_bytes,&style))
			break;

		if(ftp->js_cx_stack==global->js.cx_stack)
			iniRemoveValue(lp,section,strJavaScriptContextStack);
		else if(!iniSetInteger(lp,section,strJavaScriptContextStack	,ftp->js_cx_stack,&style))
			break;

		if(strcmp(ftp->host_name,global->host_name)==0
            || strcmp(bbs->host_name,cfg->sys_inetaddr)==0)
			iniRemoveKey(lp,section,strHostName);
		else if(!iniSetString(lp,section,strHostName,ftp->host_name,&style))
			break;

		if(!iniSetString(lp,section,"IndexFileName",ftp->index_file_name,&style))
			break;
		if(!iniSetString(lp,section,"HtmlIndexFile",ftp->html_index_file,&style))
			break;
		if(!iniSetString(lp,section,"HtmlIndexScript",ftp->html_index_script,&style))
			break;

		if(!iniSetString(lp,section,"AnswerSound",ftp->answer_sound,&style))
			break;
		if(!iniSetString(lp,section,"HangupSound",ftp->hangup_sound,&style))
			break;
		if(!iniSetString(lp,section,"HackAttemptSound",ftp->hack_sound,&style))
			break;
	
		if(!iniSetBitField(lp,section,strOptions,ftp_options,ftp->options,&style))
			break;
	}

	/***********************************************************************/
	if(mail!=NULL) {

		section = "Mail";

		if(!iniSetBool(lp,section,"AutoStart",run_mail,&style))
			break;

		if(mail->interface_addr==global->interface_addr)
			iniRemoveValue(lp,section,strInterface);
		else if(!iniSetIpAddress(lp,section,strInterface,mail->interface_addr,&style))
			break;

		if(mail->sem_chk_freq==global->sem_chk_freq)
			iniRemoveValue(lp,section,strSemFileCheckFrequency);
		else if(!iniSetShortInt(lp,section,strSemFileCheckFrequency,mail->sem_chk_freq,&style))
			break;

		if(mail->log_mask==global->log_mask)
			iniRemoveValue(lp,section,strLogMask);
		else if(!iniSetBitField(lp,section,strLogMask,log_mask_bits,mail->log_mask,&style))
			break;

		if(!iniSetShortInt(lp,section,"SMTPPort",mail->smtp_port,&style))
			break;
		if(!iniSetShortInt(lp,section,"POP3Port",mail->pop3_port,&style))
			break;
		if(!iniSetShortInt(lp,section,"RelayPort",mail->relay_port,&style))
			break;
		if(!iniSetShortInt(lp,section,"MaxClients",mail->max_clients,&style))
			break;
		if(!iniSetShortInt(lp,section,"MaxInactivity",mail->max_inactivity,&style))
			break;
		if(!iniSetShortInt(lp,section,"MaxDeliveryAttempts",mail->max_delivery_attempts,&style))
			break;
		if(!iniSetShortInt(lp,section,"RescanFrequency",mail->rescan_frequency,&style))
			break;
		if(!iniSetShortInt(lp,section,"LinesPerYield",mail->lines_per_yield,&style))
			break;
		if(!iniSetShortInt(lp,section,"MaxRecipients",mail->max_recipients,&style))
			break;
		if(!iniSetInteger(lp,section,"MaxMsgSize",mail->max_msg_size,&style))
			break;

		if(strcmp(mail->host_name,global->host_name)==0
            || strcmp(bbs->host_name,cfg->sys_inetaddr)==0)
			iniRemoveKey(lp,section,strHostName);
		else if(!iniSetString(lp,section,strHostName,mail->host_name,&style))
			break;

		if(!iniSetString(lp,section,"RelayServer",mail->relay_server,&style))
			break;
		if(!iniSetString(lp,section,"DNSServer",mail->dns_server,&style))
			break;

		if(!iniSetString(lp,section,"DefaultUser",mail->default_user,&style))
			break;

		if(!iniSetString(lp,section,"DNSBlacklistHeader",mail->dnsbl_hdr,&style))
			break;
		if(!iniSetString(lp,section,"DNSBlacklistSubject",mail->dnsbl_tag,&style))
			break;

		if(!iniSetString(lp,section,"POP3Sound",mail->pop3_sound,&style))
			break;
		if(!iniSetString(lp,section,"InboundSound",mail->inbound_sound,&style))
			break;
		if(!iniSetString(lp,section,"OutboundSound",mail->outbound_sound,&style))
			break;

		if(!iniSetString(lp,section,"ProcessConfigFile",mail->proc_cfg_file,&style))
			break;

		/* JavaScript Operating Parameters */
		if(mail->js_max_bytes==global->js.max_bytes)
			iniRemoveValue(lp,section,strJavaScriptMaxBytes);
		else if(!iniSetInteger(lp,section,strJavaScriptMaxBytes		,mail->js_max_bytes,&style))
			break;

		if(mail->js_cx_stack==global->js.cx_stack)
			iniRemoveValue(lp,section,strJavaScriptContextStack);
		else if(!iniSetInteger(lp,section,strJavaScriptContextStack	,mail->js_cx_stack,&style))
			break;

		if(!iniSetBitField(lp,section,strOptions,mail_options,mail->options,&style))
			break;
	}

	/***********************************************************************/
	if(services!=NULL) {

		section = "Services";

		if(!iniSetBool(lp,section,"AutoStart",run_services,&style))
			break;

		if(services->interface_addr==global->interface_addr)
			iniRemoveValue(lp,section,strInterface);
		else if(!iniSetIpAddress(lp,section,strInterface,services->interface_addr,&style))
			break;

		if(services->sem_chk_freq==global->sem_chk_freq)
			iniRemoveValue(lp,section,strSemFileCheckFrequency);
		else if(!iniSetShortInt(lp,section,strSemFileCheckFrequency,services->sem_chk_freq,&style))
			break;

		if(services->log_mask==global->log_mask)
			iniRemoveValue(lp,section,strLogMask);
		else if(!iniSetBitField(lp,section,strLogMask,log_mask_bits,services->log_mask,&style))
			break;

		/* Configurable JavaScript default parameters */
		if(services->js_max_bytes==global->js.max_bytes)
			iniRemoveValue(lp,section,strJavaScriptMaxBytes);
		else if(!iniSetInteger(lp,section,strJavaScriptMaxBytes		,services->js_max_bytes,&style))
			break;

		if(services->js_cx_stack==global->js.cx_stack)
			iniRemoveValue(lp,section,strJavaScriptContextStack);
		else if(!iniSetInteger(lp,section,strJavaScriptContextStack	,services->js_cx_stack,&style))
			break;

		if(services->js_branch_limit==global->js.branch_limit)
			iniRemoveValue(lp,section,strJavaScriptBranchLimit);
		else if(!iniSetInteger(lp,section,strJavaScriptBranchLimit	,services->js_branch_limit,&style))
			break;

		if(services->js_gc_interval==global->js.gc_interval)
			iniRemoveValue(lp,section,strJavaScriptGcInterval);
		else if(!iniSetInteger(lp,section,strJavaScriptGcInterval	,services->js_gc_interval,&style))
			break;

		if(services->js_yield_interval==global->js.yield_interval)
			iniRemoveValue(lp,section,strJavaScriptYieldInterval);
		else if(!iniSetInteger(lp,section,strJavaScriptYieldInterval	,services->js_yield_interval,&style))
			break;

		if(strcmp(services->host_name,global->host_name)==0
            || strcmp(bbs->host_name,cfg->sys_inetaddr)==0)
			iniRemoveKey(lp,section,strHostName);
		else if(!iniSetString(lp,section,strHostName,services->host_name,&style))
			break;

		if(!iniSetString(lp,section,"iniFile",services->ini_file,&style))
			break;
		if(!iniSetString(lp,section,"ConfigFile",services->cfg_file,&style))
			break;

		if(!iniSetString(lp,section,"AnswerSound",services->answer_sound,&style))
			break;
		if(!iniSetString(lp,section,"HangupSound",services->hangup_sound,&style))
			break;

		if(!iniSetBitField(lp,section,strOptions,service_options,services->options,&style))
			break;
	}

	/***********************************************************************/
	if(web!=NULL) {

		section = "Web";

		if(!iniSetBool(lp,section,"AutoStart",run_web,&style))
			break;

		if(web->interface_addr==global->interface_addr)
			iniRemoveValue(lp,section,strInterface);
		else if(!iniSetIpAddress(lp,section,strInterface,web->interface_addr,&style))
			break;

		if(!iniSetShortInt(lp,section,"Port",web->port,&style))
			break;

		if(web->sem_chk_freq==global->sem_chk_freq)
			iniRemoveValue(lp,section,strSemFileCheckFrequency);
		else if(!iniSetShortInt(lp,section,strSemFileCheckFrequency,web->sem_chk_freq,&style))
			break;

		if(web->log_mask==global->log_mask)
			iniRemoveValue(lp,section,strLogMask);
		else if(!iniSetBitField(lp,section,strLogMask,log_mask_bits,web->log_mask,&style))
			break;

		/* JavaScript Operating Parameters */
		if(web->js_max_bytes==global->js.max_bytes)
			iniRemoveValue(lp,section,strJavaScriptMaxBytes);
		else if(!iniSetInteger(lp,section,strJavaScriptMaxBytes		,web->js_max_bytes,&style))
			break;

		if(web->js_cx_stack==global->js.cx_stack)
			iniRemoveValue(lp,section,strJavaScriptContextStack);
		else if(!iniSetInteger(lp,section,strJavaScriptContextStack	,web->js_cx_stack,&style))
			break;

		if(strcmp(web->host_name,global->host_name)==0
            || strcmp(bbs->host_name,cfg->sys_inetaddr)==0)
			iniRemoveKey(lp,section,strHostName);
		else if(!iniSetString(lp,section,strHostName,web->host_name,&style))
			break;

		if(!iniSetString(lp,section,"RootDirectory",web->root_dir,&style))
			break;
		if(!iniSetString(lp,section,"ErrorDirectory",web->error_dir,&style))
			break;
		if(!iniSetString(lp,section,"CGIDirectory",web->cgi_dir,&style))
			break;

		if(!iniSetStringList(lp,section,"IndexFileNames", "," ,web->index_file_name,&style))
			break;
		if(!iniSetStringList(lp,section,"CGIExtensions", "," ,web->cgi_ext,&style))
			break;

		if(!iniSetString(lp,section,"JavaScriptExtension",web->ssjs_ext,&style))
			break;

		if(!iniSetShortInt(lp,section,"MaxInactivity",web->max_inactivity,&style))
			break;
		if(!iniSetShortInt(lp,section,"MaxCgiInactivity",web->max_cgi_inactivity,&style))
			break;

		if(!iniSetString(lp,section,"CGITempDirectory",web->cgi_temp_dir,&style))
			break;

		if(!iniSetBitField(lp,section,strOptions,web_options,web->options,&style))
			break;
	}

	/***********************************************************************/
	result=iniWriteFile(fp,list);

	} while(0);	/* finally */

	strListFree(&list);

	return(result);
}
