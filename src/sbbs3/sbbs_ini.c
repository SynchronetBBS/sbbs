/* sbbs_ini.c */

/* Synchronet initialization (.ini) file routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2008 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#define STARTUP_INI_BITDESC_TABLES

#include <string.h>	/* strchr, memset */

#include "dirwrap.h"	/* backslash */
#include "sbbs_ini.h"
#include "sbbsdefs.h"	/* JAVASCRIPT_* macros */

static const char*	nulstr="";
static const char*  strAutoStart="AutoStart";
static const char*  strCtrlDirectory="CtrlDirectory";
static const char*  strTempDirectory="TempDirectory";
static const char*	strOptions="Options";
static const char*	strInterface="Interface";
static const char*	strPort="Port";
static const char*	strMaxClients="MaxClients";
static const char*	strMaxInactivity="MaxInactivity";
static const char*	strHostName="HostName";
static const char*	strLogLevel="LogLevel";
static const char*	strBindRetryCount="BindRetryCount";
static const char*	strBindRetryDelay="BindRetryDelay";
static const char*	strAnswerSound="AnswerSound";
static const char*	strHangupSound="HangupSound";
static const char*	strHackAttemptSound="HackAttemptSound";
static const char*	strJavaScriptMaxBytes		="JavaScriptMaxBytes";
static const char*	strJavaScriptContextStack	="JavaScriptContextStack";
static const char*	strJavaScriptThreadStack	="JavaScriptThreadStack";
static const char*	strJavaScriptBranchLimit	="JavaScriptBranchLimit";
static const char*	strJavaScriptGcInterval		="JavaScriptGcInterval";
static const char*	strJavaScriptYieldInterval	="JavaScriptYieldInterval";
static const char*	strSemFileCheckFrequency	="SemFileCheckFrequency";

#define DEFAULT_LOG_LEVEL		LOG_DEBUG
#define DEFAULT_MAX_MSG_SIZE    (10*1024*1024)	/* 10MB */
#define DEFAULT_BIND_RETRY_COUNT	2
#define DEFAULT_BIND_RETRY_DELAY	15

void sbbs_get_ini_fname(char* ini_file, char* ctrl_dir, char* pHostName)
{
    char	host_name[128];
    char	path[MAX_PATH+1];
	char*	p;

    if(pHostName!=NULL)
		SAFECOPY(host_name,pHostName);
	else {
#if defined(_WINSOCKAPI_)
        WSADATA WSAData;
        WSAStartup(MAKEWORD(1,1), &WSAData); /* req'd for gethostname */
#endif
    	gethostname(host_name,sizeof(host_name)-1);
#if defined(_WINSOCKAPI_)
        WSACleanup();
#endif
    }
	SAFECOPY(path,ctrl_dir);
	backslash(path);
	sprintf(ini_file,"%s%s.ini",path,host_name);
	if(fexistcase(ini_file))
		return;
	if((p=strchr(host_name,'.'))!=NULL) {
		*p=0;
		sprintf(ini_file,"%s%s.ini",path,host_name);
		if(fexistcase(ini_file))
			return;
	}
#if defined(__unix__) && defined(PREFIX)
	sprintf(ini_file,PREFIX"/etc/sbbs.ini");
	if(fexistcase(ini_file))
		return;
#endif
	iniFileName(ini_file,MAX_PATH,ctrl_dir,"sbbs.ini");
	if(fexistcase(ini_file))
		return;
	iniFileName(ini_file,MAX_PATH,ctrl_dir,"startup.ini");
}

static void sbbs_fix_js_settings(js_startup_t* js)
{
	/* Some sanity checking here */
	if(js->max_bytes==0)	js->max_bytes=JAVASCRIPT_MAX_BYTES;
	if(js->cx_stack==0)		js->cx_stack=JAVASCRIPT_CONTEXT_STACK;
}

void sbbs_get_js_settings(
	 str_list_t list
	,const char* section
	,js_startup_t* js
	,js_startup_t* defaults)
{
	js->max_bytes		= iniGetInteger(list,section,strJavaScriptMaxBytes		,defaults->max_bytes);
	js->cx_stack		= iniGetInteger(list,section,strJavaScriptContextStack	,defaults->cx_stack);
	js->thread_stack	= iniGetInteger(list,section,strJavaScriptThreadStack	,defaults->thread_stack);
	js->branch_limit	= iniGetInteger(list,section,strJavaScriptBranchLimit	,defaults->branch_limit);
	js->gc_interval		= iniGetInteger(list,section,strJavaScriptGcInterval	,defaults->gc_interval);
	js->yield_interval	= iniGetInteger(list,section,strJavaScriptYieldInterval	,defaults->yield_interval);

	sbbs_fix_js_settings(js);
}

BOOL sbbs_set_js_settings(
	 str_list_t* lp
	,const char* section
	,js_startup_t* js
	,js_startup_t* defaults
	,ini_style_t* style)
{
	BOOL	failure=FALSE;
	js_startup_t global_defaults = {
			 JAVASCRIPT_MAX_BYTES
			,JAVASCRIPT_CONTEXT_STACK
			,JAVASCRIPT_THREAD_STACK
			,JAVASCRIPT_BRANCH_LIMIT
			,JAVASCRIPT_GC_INTERVAL
			,JAVASCRIPT_YIELD_INTERVAL
		};

	if(defaults==NULL)
		defaults=&global_defaults;

	sbbs_fix_js_settings(js);

	if(js->max_bytes==defaults->max_bytes)
		iniRemoveValue(lp,section,strJavaScriptMaxBytes);
	else
		failure|=iniSetInteger(lp,section,strJavaScriptMaxBytes,js->max_bytes,style)==NULL;

	if(js->cx_stack==defaults->cx_stack)
		iniRemoveValue(lp,section,strJavaScriptContextStack);
	else 
		failure|=iniSetInteger(lp,section,strJavaScriptContextStack,js->cx_stack,style)==NULL;

	if(js->thread_stack==defaults->thread_stack)
		iniRemoveValue(lp,section,strJavaScriptThreadStack);
	else 
		failure|=iniSetInteger(lp,section,strJavaScriptThreadStack,js->thread_stack,style)==NULL;

	if(js->branch_limit==defaults->branch_limit)
		iniRemoveValue(lp,section,strJavaScriptBranchLimit);
	else
		failure|=iniSetInteger(lp,section,strJavaScriptBranchLimit,js->branch_limit,style)==NULL;

	if(js->gc_interval==defaults->gc_interval)
		iniRemoveValue(lp,section,strJavaScriptGcInterval);
	else 
		failure|=iniSetInteger(lp,section,strJavaScriptGcInterval,js->gc_interval,style)==NULL;

	if(js->yield_interval==defaults->yield_interval)
		iniRemoveValue(lp,section,strJavaScriptYieldInterval);
	else 
		failure|=iniSetInteger(lp,section,strJavaScriptYieldInterval,js->yield_interval,style)==NULL;

	return(!failure);
}

static void get_ini_globals(str_list_t list, global_startup_t* global)
{
	const char* section = "Global";
	char		value[INI_MAX_VALUE_LEN];
	char*		p;

	p=iniGetString(list,section,strCtrlDirectory,nulstr,value);
	if(*p) {
	    SAFECOPY(global->ctrl_dir,value);
		backslash(global->ctrl_dir);
    }

	p=iniGetString(list,section,strTempDirectory,nulstr,value);
#if defined(__unix__)
	if(*p==0)
		p=_PATH_TMP;	/* Good idea to use "/tmp" on Unix */
#endif
	if(*p) {
	    SAFECOPY(global->temp_dir,value);
		backslash(global->temp_dir);
    }

	p=iniGetString(list,section,strHostName,nulstr,value);
	if(*p)
        SAFECOPY(global->host_name,value);

	global->sem_chk_freq=iniGetShortInt(list,section,strSemFileCheckFrequency,0);
	global->interface_addr=iniGetIpAddress(list,section,strInterface,INADDR_ANY);
	global->log_level=iniGetLogLevel(list,section,strLogLevel,DEFAULT_LOG_LEVEL);
	global->bind_retry_count=iniGetInteger(list,section,strBindRetryCount,DEFAULT_BIND_RETRY_COUNT);
	global->bind_retry_delay=iniGetInteger(list,section,strBindRetryDelay,DEFAULT_BIND_RETRY_DELAY);

	/* Setup default values here */
	global->js.max_bytes		= JAVASCRIPT_MAX_BYTES;
	global->js.cx_stack			= JAVASCRIPT_CONTEXT_STACK;
	global->js.thread_stack		= JAVASCRIPT_THREAD_STACK;
	global->js.branch_limit		= JAVASCRIPT_BRANCH_LIMIT;
	global->js.gc_interval		= JAVASCRIPT_GC_INTERVAL;
	global->js.yield_interval	= JAVASCRIPT_YIELD_INTERVAL;

	/* Read .ini values here */
	sbbs_get_js_settings(list, section, &global->js, &global->js);
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
	const char*	default_dosemu_path;
	char		value[INI_MAX_VALUE_LEN];
	str_list_t	list;
	global_startup_t global_buf;

	if(global==NULL) {
		memset(&global_buf,0,sizeof(global_buf));
		global=&global_buf;
	}

	list=iniReadFile(fp);

	get_ini_globals(list, global);

	if(global->ctrl_dir[0]) {
		if(bbs!=NULL)		SAFECOPY(bbs->ctrl_dir,global->ctrl_dir);
		if(ftp!=NULL)		SAFECOPY(ftp->ctrl_dir,global->ctrl_dir);
		if(web!=NULL)		SAFECOPY(web->ctrl_dir,global->ctrl_dir);
		if(mail!=NULL)		SAFECOPY(mail->ctrl_dir,global->ctrl_dir);
		if(services!=NULL)	SAFECOPY(services->ctrl_dir,global->ctrl_dir);
	}
													
	/***********************************************************************/
	section = "BBS";

	if(run_bbs!=NULL)
		*run_bbs=iniGetBool(list,section,strAutoStart,TRUE);

	if(bbs!=NULL) {

		bbs->telnet_interface
			=iniGetIpAddress(list,section,"TelnetInterface",global->interface_addr);
		bbs->telnet_port
			=iniGetShortInt(list,section,"TelnetPort",IPPORT_TELNET);

		bbs->rlogin_interface
			=iniGetIpAddress(list,section,"RLoginInterface",global->interface_addr);
		bbs->rlogin_port
			=iniGetShortInt(list,section,"RLoginPort",513);

		bbs->ssh_interface
			=iniGetIpAddress(list,section,"SSHInterface",global->interface_addr);
		bbs->ssh_port
			=iniGetShortInt(list,section,"SSHPort",22);

		bbs->first_node
			=iniGetShortInt(list,section,"FirstNode",1);
		bbs->last_node
			=iniGetShortInt(list,section,"LastNode",4);

		bbs->outbuf_highwater_mark
			=iniGetShortInt(list,section,"OutbufHighwaterMark"
#ifdef TCP_MAXSEG	/* Auto-tune if possible.  Would this be defined here? */
			,0
#else
			,1024
#endif
			);
		bbs->outbuf_drain_timeout
			=iniGetShortInt(list,section,"OutbufDrainTimeout",10);

		bbs->sem_chk_freq
			=iniGetShortInt(list,section,strSemFileCheckFrequency,global->sem_chk_freq);

		/* JavaScript operating parameters */
		sbbs_get_js_settings(list, section, &bbs->js, &global->js);

		SAFECOPY(bbs->host_name
			,iniGetString(list,section,strHostName,global->host_name,value));

		SAFECOPY(bbs->temp_dir
			,iniGetString(list,section,strTempDirectory,global->temp_dir,value));

		/* Set default terminal type to "stock" termcap closest to "ansi-bbs" */
	#if defined(__FreeBSD__)
		default_term_ansi="cons25";
	#else
		default_term_ansi="pc3";
	#endif

		SAFECOPY(bbs->xtrn_term_ansi
			,iniGetString(list,section,"ExternalTermANSI",default_term_ansi,value));
		SAFECOPY(bbs->xtrn_term_dumb
			,iniGetString(list,section,"ExternalTermDumb","dumb",value));

	#if defined(__FreeBSD__)
		default_dosemu_path="/usr/local/bin/doscmd";
	#else
		default_dosemu_path="/usr/bin/dosemu.bin";
	#endif

		SAFECOPY(bbs->dosemu_path
			,iniGetString(list,section,"DOSemuPath",default_dosemu_path,value));

		SAFECOPY(bbs->answer_sound
			,iniGetString(list,section,strAnswerSound,nulstr,value));
		SAFECOPY(bbs->hangup_sound
			,iniGetString(list,section,strHangupSound,nulstr,value));

		bbs->log_level
			=iniGetLogLevel(list,section,strLogLevel,global->log_level);
		bbs->options
			=iniGetBitField(list,section,strOptions,bbs_options
				,BBS_OPT_XTRN_MINIMIZED|BBS_OPT_SYSOP_AVAILABLE);

		bbs->bind_retry_count=iniGetInteger(list,section,strBindRetryCount,global->bind_retry_count);
		bbs->bind_retry_delay=iniGetInteger(list,section,strBindRetryDelay,global->bind_retry_delay);
	}

	/***********************************************************************/
	section = "FTP";

	if(run_ftp!=NULL)
		*run_ftp=iniGetBool(list,section,strAutoStart,TRUE);

	if(ftp!=NULL) {

		ftp->interface_addr
			=iniGetIpAddress(list,section,strInterface,global->interface_addr);
		ftp->port
			=iniGetShortInt(list,section,strPort,IPPORT_FTP);
		ftp->max_clients
			=iniGetShortInt(list,section,strMaxClients,10);
		ftp->max_inactivity
			=iniGetShortInt(list,section,strMaxInactivity,300);	/* seconds */
		ftp->qwk_timeout
			=iniGetShortInt(list,section,"QwkTimeout",600);		/* seconds */
		ftp->sem_chk_freq
			=iniGetShortInt(list,section,strSemFileCheckFrequency,global->sem_chk_freq);

		/* Passive transfer settings (for stupid firewalls/NATs) */
		ftp->pasv_ip_addr
			=iniGetIpAddress(list,section,"PasvIpAddress",0);
		ftp->pasv_port_low
			=iniGetShortInt(list,section,"PasvPortLow",IPPORT_RESERVED);
		ftp->pasv_port_high
			=iniGetShortInt(list,section,"PasvPortHigh",0xffff);


		/* JavaScript Operating Parameters */
		sbbs_get_js_settings(list, section, &ftp->js, &global->js);

		SAFECOPY(ftp->host_name
			,iniGetString(list,section,strHostName,global->host_name,value));

		SAFECOPY(ftp->index_file_name
			,iniGetString(list,section,"IndexFileName","00index",value));
		SAFECOPY(ftp->html_index_file
			,iniGetString(list,section,"HtmlIndexFile","00index.html",value));
		SAFECOPY(ftp->html_index_script
			,iniGetString(list,section,"HtmlIndexScript","ftp-html.js",value));

		SAFECOPY(ftp->answer_sound
			,iniGetString(list,section,strAnswerSound,nulstr,value));
		SAFECOPY(ftp->hangup_sound
			,iniGetString(list,section,strHangupSound,nulstr,value));
		SAFECOPY(ftp->hack_sound
			,iniGetString(list,section,strHackAttemptSound,nulstr,value));

		SAFECOPY(ftp->temp_dir
			,iniGetString(list,section,strTempDirectory,global->temp_dir,value));

		ftp->log_level
			=iniGetLogLevel(list,section,strLogLevel,global->log_level);
		ftp->options
			=iniGetBitField(list,section,strOptions,ftp_options
				,FTP_OPT_INDEX_FILE|FTP_OPT_HTML_INDEX_FILE|FTP_OPT_ALLOW_QWK);

		ftp->bind_retry_count=iniGetInteger(list,section,strBindRetryCount,global->bind_retry_count);
		ftp->bind_retry_delay=iniGetInteger(list,section,strBindRetryDelay,global->bind_retry_delay);
	}

	/***********************************************************************/
	section = "Mail";

	if(run_mail!=NULL)
		*run_mail=iniGetBool(list,section,strAutoStart,TRUE);

	if(mail!=NULL) {

		mail->interface_addr
			=iniGetIpAddress(list,section,strInterface,global->interface_addr);
		mail->smtp_port
			=iniGetShortInt(list,section,"SMTPPort",IPPORT_SMTP);
		mail->submission_port
			=iniGetShortInt(list,section,"SubmissionPort",IPPORT_SUBMISSION);
		mail->pop3_port
			=iniGetShortInt(list,section,"POP3Port",IPPORT_POP3);
		mail->relay_port
			=iniGetShortInt(list,section,"RelayPort",IPPORT_SMTP);
		mail->max_clients
			=iniGetShortInt(list,section,strMaxClients,10);
		mail->max_inactivity
			=iniGetShortInt(list,section,strMaxInactivity,120);		/* seconds */
		mail->max_delivery_attempts
			=iniGetShortInt(list,section,"MaxDeliveryAttempts",50);
		mail->rescan_frequency
			=iniGetShortInt(list,section,"RescanFrequency",3600);	/* 60 minutes */
		mail->sem_chk_freq
			=iniGetShortInt(list,section,strSemFileCheckFrequency,global->sem_chk_freq);
		mail->lines_per_yield
			=iniGetShortInt(list,section,"LinesPerYield",10);
		mail->max_recipients
			=iniGetShortInt(list,section,"MaxRecipients",100);
		mail->max_msg_size
			=iniGetInteger(list,section,"MaxMsgSize",DEFAULT_MAX_MSG_SIZE);

		SAFECOPY(mail->host_name
			,iniGetString(list,section,strHostName,global->host_name,value));

		SAFECOPY(mail->temp_dir
			,iniGetString(list,section,strTempDirectory,global->temp_dir,value));

		SAFECOPY(mail->relay_server
			,iniGetString(list,section,"RelayServer",nulstr,value));
		SAFECOPY(mail->relay_user
			,iniGetString(list,section,"RelayUsername",nulstr,value));
		SAFECOPY(mail->relay_pass
			,iniGetString(list,section,"RelayPassword",nulstr,value));

		SAFECOPY(mail->dns_server
			,iniGetString(list,section,"DNSServer",nulstr,value));

		SAFECOPY(mail->default_user
			,iniGetString(list,section,"DefaultUser",nulstr,value));

		SAFECOPY(mail->default_charset
			,iniGetString(list,section,"DefaultCharset",nulstr,value));

		SAFECOPY(mail->dnsbl_hdr
			,iniGetString(list,section,"DNSBlacklistHeader","X-DNSBL",value));
		SAFECOPY(mail->dnsbl_tag
			,iniGetString(list,section,"DNSBlacklistSubject","SPAM",value));

		SAFECOPY(mail->pop3_sound
			,iniGetString(list,section,"POP3Sound",nulstr,value));
		SAFECOPY(mail->inbound_sound
			,iniGetString(list,section,"InboundSound",nulstr,value));
		SAFECOPY(mail->outbound_sound
			,iniGetString(list,section,"OutboundSound",nulstr,value));

		/* JavaScript Operating Parameters */
		sbbs_get_js_settings(list, section, &mail->js, &global->js);

		mail->log_level
			=iniGetLogLevel(list,section,strLogLevel,global->log_level);
		mail->options
			=iniGetBitField(list,section,strOptions,mail_options
				,MAIL_OPT_ALLOW_POP3);

		mail->bind_retry_count=iniGetInteger(list,section,strBindRetryCount,global->bind_retry_count);
		mail->bind_retry_delay=iniGetInteger(list,section,strBindRetryDelay,global->bind_retry_delay);
	}

	/***********************************************************************/
	section = "Services";

	if(run_services!=NULL)
		*run_services=iniGetBool(list,section,strAutoStart,TRUE);

	if(services!=NULL) {

		services->interface_addr
			=iniGetIpAddress(list,section,strInterface,global->interface_addr);

		services->sem_chk_freq
			=iniGetShortInt(list,section,strSemFileCheckFrequency,global->sem_chk_freq);

		/* JavaScript operating parameters */
		sbbs_get_js_settings(list, section, &services->js, &global->js);

		SAFECOPY(services->host_name
			,iniGetString(list,section,strHostName,global->host_name,value));

		SAFECOPY(services->temp_dir
			,iniGetString(list,section,strTempDirectory,global->temp_dir,value));

		SAFECOPY(services->answer_sound
			,iniGetString(list,section,strAnswerSound,nulstr,value));
		SAFECOPY(services->hangup_sound
			,iniGetString(list,section,strHangupSound,nulstr,value));

		services->log_level
			=iniGetLogLevel(list,section,strLogLevel,global->log_level);
		services->options
			=iniGetBitField(list,section,strOptions,service_options
				,BBS_OPT_NO_HOST_LOOKUP);

		services->bind_retry_count=iniGetInteger(list,section,strBindRetryCount,global->bind_retry_count);
		services->bind_retry_delay=iniGetInteger(list,section,strBindRetryDelay,global->bind_retry_delay);
	}

	/***********************************************************************/
	section = "Web";

	if(run_web!=NULL)
		*run_web=iniGetBool(list,section,strAutoStart,FALSE);

	if(web!=NULL) {

		web->interface_addr
			=iniGetIpAddress(list,section,strInterface,global->interface_addr);
		web->port
			=iniGetShortInt(list,section,strPort,IPPORT_HTTP);
		web->max_clients
			=iniGetShortInt(list,section,strMaxClients,10);
		web->max_inactivity
			=iniGetShortInt(list,section,strMaxInactivity,120);		/* seconds */
		web->sem_chk_freq
			=iniGetShortInt(list,section,strSemFileCheckFrequency,global->sem_chk_freq);

		/* JavaScript operating parameters */
		sbbs_get_js_settings(list, section, &web->js, &global->js);

		SAFECOPY(web->host_name
			,iniGetString(list,section,strHostName,global->host_name,value));

		SAFECOPY(web->temp_dir
			,iniGetString(list,section,strTempDirectory,global->temp_dir,value));

		SAFECOPY(web->root_dir
			,iniGetString(list,section,"RootDirectory",WEB_DEFAULT_ROOT_DIR,value));
		SAFECOPY(web->error_dir
			,iniGetString(list,section,"ErrorDirectory",WEB_DEFAULT_ERROR_DIR,value));
		SAFECOPY(web->cgi_dir
			,iniGetString(list,section,"CGIDirectory",WEB_DEFAULT_CGI_DIR,value));
		SAFECOPY(web->default_auth_list
			,iniGetString(list,section,"Authentication",WEB_DEFAULT_AUTH_LIST,value));
		SAFECOPY(web->logfile_base
			,iniGetString(list,section,"HttpLogFile",nulstr,value));

		SAFECOPY(web->default_cgi_content
			,iniGetString(list,section,"DefaultCGIContent",WEB_DEFAULT_CGI_CONTENT,value));

		iniFreeStringList(web->index_file_name);
		web->index_file_name
			=iniGetStringList(list,section,"IndexFileNames", "," ,"index.html,index.ssjs");
		iniFreeStringList(web->cgi_ext);
		web->cgi_ext
			=iniGetStringList(list,section,"CGIExtensions", "," ,".cgi");
		SAFECOPY(web->ssjs_ext
			,iniGetString(list,section,"JavaScriptExtension",".ssjs",value));
		SAFECOPY(web->js_ext
			,iniGetString(list,section,"EmbJavaScriptExtension",".bbs",value));

		web->max_inactivity
			=iniGetShortInt(list,section,strMaxInactivity,120);		/* seconds */
		web->max_cgi_inactivity
			=iniGetShortInt(list,section,"MaxCgiInactivity",120);	/* seconds */

		SAFECOPY(web->answer_sound
			,iniGetString(list,section,strAnswerSound,nulstr,value));
		SAFECOPY(web->hangup_sound
			,iniGetString(list,section,strHangupSound,nulstr,value));
		SAFECOPY(web->hack_sound
			,iniGetString(list,section,strHackAttemptSound,nulstr,value));

		web->log_level
			=iniGetLogLevel(list,section,strLogLevel,global->log_level);
		web->options
			=iniGetBitField(list,section,strOptions,web_options
				,BBS_OPT_NO_HOST_LOOKUP | WEB_OPT_HTTP_LOGGING);
		web->outbuf_highwater_mark
			=iniGetShortInt(list,section,"OutbufHighwaterMark"
#ifdef TCP_MAXSEG	/* Auto-tune if possible.  Would this be defined here? */
			,0
#else
			,1024
#endif
			);
		web->outbuf_drain_timeout
			=iniGetShortInt(list,section,"OutbufDrainTimeout",10);

		web->bind_retry_count=iniGetInteger(list,section,strBindRetryCount,global->bind_retry_count);
		web->bind_retry_delay=iniGetInteger(list,section,strBindRetryDelay,global->bind_retry_delay);
	}

	iniFreeStringList(list);
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

	memset(&style, 0, sizeof(style));
	style.key_prefix = "\t";
    style.bit_separator = " | ";

	if((list=iniReadFile(fp))==NULL)
		return(FALSE);

	if(global==NULL) {
		get_ini_globals(list, &global_buf);
		global = &global_buf;
	}
	
	lp=&list;

	do { /* try */

	/***********************************************************************/
	if(global!=&global_buf) {
		section = "Global";

		if(global->ctrl_dir[0]==0)
			iniRemoveKey(lp,section,strCtrlDirectory);
		else
			iniSetString(lp,section,strCtrlDirectory,global->ctrl_dir,&style);

		if(global->temp_dir[0]==0)
			iniRemoveKey(lp,section,strTempDirectory);
		else
			iniSetString(lp,section,strTempDirectory,global->temp_dir,&style);

		if(global->host_name[0]==0)
			iniRemoveKey(lp,section,strHostName);
		else
			iniSetString(lp,section,strHostName,global->host_name,&style);
	
		if(global->sem_chk_freq==0)
			iniRemoveKey(lp,section,strSemFileCheckFrequency);
		else
			iniSetShortInt(lp,section,strSemFileCheckFrequency,global->sem_chk_freq,&style);
		if(global->interface_addr==INADDR_ANY)
			iniRemoveKey(lp,section,strInterface);
		else
			iniSetIpAddress(lp,section,strInterface,global->interface_addr,&style);
		if(global->log_level==DEFAULT_LOG_LEVEL)
			iniRemoveKey(lp,section,strLogLevel);
		else
			iniSetLogLevel(lp,section,strLogLevel,global->log_level,&style);

		if(global->bind_retry_count==DEFAULT_BIND_RETRY_COUNT)
			iniRemoveKey(lp,section,strBindRetryCount);
		else
			iniSetInteger(lp,section,strBindRetryCount,global->bind_retry_count,&style);
		if(global->bind_retry_delay==DEFAULT_BIND_RETRY_DELAY)
			iniRemoveKey(lp,section,strBindRetryDelay);
		else
			iniSetInteger(lp,section,strBindRetryDelay,global->bind_retry_delay,&style);

		/* JavaScript operating parameters */
		if(!sbbs_set_js_settings(lp,section,&global->js,NULL,&style))
			break;
	}

	/***********************************************************************/
	if(bbs!=NULL) {

		section = "BBS";

		if(!iniSetBool(lp,section,strAutoStart,run_bbs,&style))
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

		if(bbs->ssh_interface==global->interface_addr)
			iniRemoveValue(lp,section,"SSHInterface");
		else if(!iniSetIpAddress(lp,section,"SSHInterface",bbs->ssh_interface,&style))
			break;
		if(!iniSetShortInt(lp,section,"SSHPort",bbs->ssh_port,&style))
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

		if(bbs->log_level==global->log_level)
			iniRemoveValue(lp,section,strLogLevel);
		else if(!iniSetLogLevel(lp,section,strLogLevel,bbs->log_level,&style))
			break;

		/* JavaScript operating parameters */
		if(!sbbs_set_js_settings(lp,section,&bbs->js,&global->js,&style))
			break;

		if(strcmp(bbs->host_name,global->host_name)==0
            || strcmp(bbs->host_name,cfg->sys_inetaddr)==0)
			iniRemoveKey(lp,section,strHostName);
		else if(!iniSetString(lp,section,strHostName,bbs->host_name,&style))
			break;

		if(stricmp(bbs->temp_dir,global->temp_dir)==0)
			iniRemoveKey(lp,section,strTempDirectory);
		else if(!iniSetString(lp,section,strTempDirectory,bbs->temp_dir,&style))
			break;

		if(!iniSetString(lp,section,"ExternalTermANSI",bbs->xtrn_term_ansi,&style))
			break;
		if(!iniSetString(lp,section,"ExternalTermDumb",bbs->xtrn_term_dumb,&style))
			break;
		if(!iniSetString(lp,section,"DOSemuPath",bbs->dosemu_path,&style))
			break;

		if(!iniSetString(lp,section,strAnswerSound,bbs->answer_sound,&style))
			break;
		if(!iniSetString(lp,section,strHangupSound,bbs->hangup_sound,&style))
			break;

		if(!iniSetBitField(lp,section,strOptions,bbs_options,bbs->options,&style))
			break;

		if(bbs->bind_retry_count==global->bind_retry_count)
			iniRemoveValue(lp,section,strBindRetryCount);
		else if(!iniSetInteger(lp,section,strBindRetryCount,bbs->bind_retry_count,&style))
			break;
		if(bbs->bind_retry_delay==global->bind_retry_delay)
			iniRemoveValue(lp,section,strBindRetryDelay);
		else if(!iniSetInteger(lp,section,strBindRetryDelay,bbs->bind_retry_delay,&style))
			break;
	}
	/***********************************************************************/
	if(ftp!=NULL) {

		section = "FTP";

		if(!iniSetBool(lp,section,strAutoStart,run_ftp,&style))
			break;

		if(ftp->interface_addr==global->interface_addr)
			iniRemoveValue(lp,section,strInterface);
		else if(!iniSetIpAddress(lp,section,strInterface,ftp->interface_addr,&style))
			break;

		if(!iniSetShortInt(lp,section,strPort,ftp->port,&style))
			break;
		if(!iniSetShortInt(lp,section,strMaxClients,ftp->max_clients,&style))
			break;
		if(!iniSetShortInt(lp,section,strMaxInactivity,ftp->max_inactivity,&style))
			break;
		if(!iniSetShortInt(lp,section,"QwkTimeout",ftp->qwk_timeout,&style))
			break;

		/* Passive transfer settings */
		if(!iniSetIpAddress(lp,section,"PasvIpAddress",ftp->pasv_ip_addr,&style))
			break;
		if(!iniSetShortInt(lp,section,"PasvPortLow",ftp->pasv_port_low,&style))
			break;
		if(!iniSetShortInt(lp,section,"PasvPortHigh",ftp->pasv_port_high,&style))
			break;

		if(ftp->sem_chk_freq==global->sem_chk_freq)
			iniRemoveValue(lp,section,strSemFileCheckFrequency);
		else if(!iniSetShortInt(lp,section,strSemFileCheckFrequency,ftp->sem_chk_freq,&style))
			break;

		if(ftp->log_level==global->log_level)
			iniRemoveValue(lp,section,strLogLevel);
		else if(!iniSetLogLevel(lp,section,strLogLevel,ftp->log_level,&style))
			break;

		/* JavaScript Operating Parameters */
		if(!sbbs_set_js_settings(lp,section,&ftp->js,&global->js,&style))
			break;

		if(strcmp(ftp->host_name,global->host_name)==0
            || strcmp(bbs->host_name,cfg->sys_inetaddr)==0)
			iniRemoveKey(lp,section,strHostName);
		else if(!iniSetString(lp,section,strHostName,ftp->host_name,&style))
			break;

		if(stricmp(ftp->temp_dir,global->temp_dir)==0)
			iniRemoveKey(lp,section,strTempDirectory);
		else if(!iniSetString(lp,section,strTempDirectory,ftp->temp_dir,&style))
			break;

		if(!iniSetString(lp,section,"IndexFileName",ftp->index_file_name,&style))
			break;
		if(!iniSetString(lp,section,"HtmlIndexFile",ftp->html_index_file,&style))
			break;
		if(!iniSetString(lp,section,"HtmlIndexScript",ftp->html_index_script,&style))
			break;

		if(!iniSetString(lp,section,strAnswerSound,ftp->answer_sound,&style))
			break;
		if(!iniSetString(lp,section,strHangupSound,ftp->hangup_sound,&style))
			break;
		if(!iniSetString(lp,section,strHackAttemptSound,ftp->hack_sound,&style))
			break;
	
		if(!iniSetBitField(lp,section,strOptions,ftp_options,ftp->options,&style))
			break;

		if(ftp->bind_retry_count==global->bind_retry_count)
			iniRemoveValue(lp,section,strBindRetryCount);
		else if(!iniSetInteger(lp,section,strBindRetryCount,ftp->bind_retry_count,&style))
			break;
		if(ftp->bind_retry_delay==global->bind_retry_delay)
			iniRemoveValue(lp,section,strBindRetryDelay);
		else if(!iniSetInteger(lp,section,strBindRetryDelay,ftp->bind_retry_delay,&style))
			break;
	}

	/***********************************************************************/
	if(mail!=NULL) {

		section = "Mail";

		if(!iniSetBool(lp,section,strAutoStart,run_mail,&style))
			break;

		if(mail->interface_addr==global->interface_addr)
			iniRemoveValue(lp,section,strInterface);
		else if(!iniSetIpAddress(lp,section,strInterface,mail->interface_addr,&style))
			break;

		if(mail->sem_chk_freq==global->sem_chk_freq)
			iniRemoveValue(lp,section,strSemFileCheckFrequency);
		else if(!iniSetShortInt(lp,section,strSemFileCheckFrequency,mail->sem_chk_freq,&style))
			break;

		if(mail->log_level==global->log_level)
			iniRemoveValue(lp,section,strLogLevel);
		else if(!iniSetLogLevel(lp,section,strLogLevel,mail->log_level,&style))
			break;

		if(!iniSetShortInt(lp,section,"SMTPPort",mail->smtp_port,&style))
			break;
		if(!iniSetShortInt(lp,section,"POP3Port",mail->pop3_port,&style))
			break;
		if(!iniSetShortInt(lp,section,"RelayPort",mail->relay_port,&style))
			break;
		if(!iniSetShortInt(lp,section,strMaxClients,mail->max_clients,&style))
			break;
		if(!iniSetShortInt(lp,section,strMaxInactivity,mail->max_inactivity,&style))
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

		if(stricmp(mail->temp_dir,global->temp_dir)==0)
			iniRemoveKey(lp,section,strTempDirectory);
		else if(!iniSetString(lp,section,strTempDirectory,mail->temp_dir,&style))
			break;

		if(!iniSetString(lp,section,"RelayServer",mail->relay_server,&style))
			break;
		if(!iniSetString(lp,section,"RelayUsername",mail->relay_user,&style))
			break;
		if(!iniSetString(lp,section,"RelayPassword",mail->relay_pass,&style))
			break;

		if(!iniSetString(lp,section,"DNSServer",mail->dns_server,&style))
			break;

		if(!iniSetString(lp,section,"DefaultCharset",mail->default_charset,&style))
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

		/* JavaScript Operating Parameters */
		if(!sbbs_set_js_settings(lp,section,&mail->js,&global->js,&style))
			break;

		if(!iniSetBitField(lp,section,strOptions,mail_options,mail->options,&style))
			break;

		if(mail->bind_retry_count==global->bind_retry_count)
			iniRemoveValue(lp,section,strBindRetryCount);
		else if(!iniSetInteger(lp,section,strBindRetryCount,mail->bind_retry_count,&style))
			break;
		if(mail->bind_retry_delay==global->bind_retry_delay)
			iniRemoveValue(lp,section,strBindRetryDelay);
		else if(!iniSetInteger(lp,section,strBindRetryDelay,mail->bind_retry_delay,&style))
			break;
	}

	/***********************************************************************/
	if(services!=NULL) {

		section = "Services";

		if(!iniSetBool(lp,section,strAutoStart,run_services,&style))
			break;

		if(services->interface_addr==global->interface_addr)
			iniRemoveValue(lp,section,strInterface);
		else if(!iniSetIpAddress(lp,section,strInterface,services->interface_addr,&style))
			break;

		if(services->sem_chk_freq==global->sem_chk_freq)
			iniRemoveValue(lp,section,strSemFileCheckFrequency);
		else if(!iniSetShortInt(lp,section,strSemFileCheckFrequency,services->sem_chk_freq,&style))
			break;

		if(services->log_level==global->log_level)
			iniRemoveValue(lp,section,strLogLevel);
		else if(!iniSetLogLevel(lp,section,strLogLevel,services->log_level,&style))
			break;

		/* Configurable JavaScript default parameters */
		if(!sbbs_set_js_settings(lp,section,&services->js,&global->js,&style))
			break;

		if(strcmp(services->host_name,global->host_name)==0
            || strcmp(bbs->host_name,cfg->sys_inetaddr)==0)
			iniRemoveKey(lp,section,strHostName);
		else if(!iniSetString(lp,section,strHostName,services->host_name,&style))
			break;

		if(stricmp(services->temp_dir,global->temp_dir)==0)
			iniRemoveKey(lp,section,strTempDirectory);
		else if(!iniSetString(lp,section,strTempDirectory,services->temp_dir,&style))
			break;

		if(!iniSetString(lp,section,strAnswerSound,services->answer_sound,&style))
			break;
		if(!iniSetString(lp,section,strHangupSound,services->hangup_sound,&style))
			break;

		if(!iniSetBitField(lp,section,strOptions,service_options,services->options,&style))
			break;

		if(services->bind_retry_count==global->bind_retry_count)
			iniRemoveValue(lp,section,strBindRetryCount);
		else if(!iniSetInteger(lp,section,strBindRetryCount,services->bind_retry_count,&style))
			break;
		if(services->bind_retry_delay==global->bind_retry_delay)
			iniRemoveValue(lp,section,strBindRetryDelay);
		else if(!iniSetInteger(lp,section,strBindRetryDelay,services->bind_retry_delay,&style))
			break;
	}

	/***********************************************************************/
	if(web!=NULL) {

		section = "Web";

		if(!iniSetBool(lp,section,strAutoStart,run_web,&style))
			break;

		if(web->interface_addr==global->interface_addr)
			iniRemoveValue(lp,section,strInterface);
		else if(!iniSetIpAddress(lp,section,strInterface,web->interface_addr,&style))
			break;

		if(!iniSetShortInt(lp,section,strPort,web->port,&style))
			break;
		if(!iniSetShortInt(lp,section,strMaxClients,web->max_clients,&style))
			break;
		if(!iniSetShortInt(lp,section,strMaxInactivity,web->max_inactivity,&style))
			break;

		if(web->sem_chk_freq==global->sem_chk_freq)
			iniRemoveValue(lp,section,strSemFileCheckFrequency);
		else if(!iniSetShortInt(lp,section,strSemFileCheckFrequency,web->sem_chk_freq,&style))
			break;

		if(web->log_level==global->log_level)
			iniRemoveValue(lp,section,strLogLevel);
		else if(!iniSetLogLevel(lp,section,strLogLevel,web->log_level,&style))
			break;

		/* JavaScript Operating Parameters */
		if(!sbbs_set_js_settings(lp,section,&web->js,&global->js,&style))
			break;

		if(strcmp(web->host_name,global->host_name)==0
            || strcmp(bbs->host_name,cfg->sys_inetaddr)==0)
			iniRemoveKey(lp,section,strHostName);
		else if(!iniSetString(lp,section,strHostName,web->host_name,&style))
			break;

		if(stricmp(web->temp_dir,global->temp_dir)==0)
			iniRemoveKey(lp,section,strTempDirectory);
		else if(!iniSetString(lp,section,strTempDirectory,web->temp_dir,&style))
			break;

		if(!iniSetString(lp,section,"RootDirectory",web->root_dir,&style))
			break;
		if(!iniSetString(lp,section,"ErrorDirectory",web->error_dir,&style))
			break;
		if(!iniSetString(lp,section,"CGIDirectory",web->cgi_dir,&style))
			break;
		if(!iniSetString(lp,section,"Authentication",web->default_auth_list,&style))
			break;
		if(!iniSetString(lp,section,"HttpLogFile",web->logfile_base,&style))
			break;

		if(!iniSetString(lp,section,"DefaultCGIContent",web->default_cgi_content,&style))
			break;

		if(!iniSetStringList(lp,section,"IndexFileNames", "," ,web->index_file_name,&style))
			break;
		if(!iniSetStringList(lp,section,"CGIExtensions", "," ,web->cgi_ext,&style))
			break;

		if(!iniSetString(lp,section,"JavaScriptExtension",web->ssjs_ext,&style))
			break;

		if(!iniSetShortInt(lp,section,strMaxInactivity,web->max_inactivity,&style))
			break;
		if(!iniSetShortInt(lp,section,"MaxCgiInactivity",web->max_cgi_inactivity,&style))
			break;

		if(!iniSetString(lp,section,strAnswerSound,web->answer_sound,&style))
			break;
		if(!iniSetString(lp,section,strHangupSound,web->hangup_sound,&style))
			break;
		if(!iniSetString(lp,section,strHackAttemptSound,web->hack_sound,&style))
			break;

		if(!iniSetBitField(lp,section,strOptions,web_options,web->options,&style))
			break;

		if(web->bind_retry_count==global->bind_retry_count)
			iniRemoveValue(lp,section,strBindRetryCount);
		else if(!iniSetInteger(lp,section,strBindRetryCount,web->bind_retry_count,&style))
			break;
		if(web->bind_retry_delay==global->bind_retry_delay)
			iniRemoveValue(lp,section,strBindRetryDelay);
		else if(!iniSetInteger(lp,section,strBindRetryDelay,web->bind_retry_delay,&style))
			break;
		if(!iniSetShortInt(lp,section,"OutbufHighwaterMark",web->outbuf_highwater_mark,&style))
			break;
		if(!iniSetShortInt(lp,section,"OutbufDrainTimeout",web->outbuf_drain_timeout,&style))
			break;
	}

	/***********************************************************************/
	result=iniWriteFile(fp,list);

	} while(0);	/* finally */

	iniFreeStringList(list);

	return(result);
}
