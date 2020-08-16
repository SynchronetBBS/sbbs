/* Synchronet initialization (.ini) file routines */

/* $Id: sbbs_ini.c,v 1.170 2019/07/24 04:41:49 rswindell Exp $ */
// vi: tabstop=4

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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

#include "sockwrap.h"
#include <string.h>	/* strchr, memset */

#include "sbbs_ini.h"
#include "dirwrap.h"	/* backslash */
#include "sbbsdefs.h"	/* JAVASCRIPT_* macros */

static const char*	nulstr="";
static const char*  strAutoStart="AutoStart";
static const char*  strCtrlDirectory="CtrlDirectory";
static const char*  strTempDirectory="TempDirectory";
static const char*	strOptions="Options";
static const char*	strOutgoing4="OutboundInterface";
static const char*	strOutgoing6="OutboundV6Interface";
static const char*	strInterfaces="Interface";
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
static const char*	strLoginAttemptDelay="LoginAttemptDelay";
static const char*	strLoginAttemptThrottle="LoginAttemptThrottle";
static const char*	strLoginAttemptHackThreshold="LoginAttemptHackThreshold";
static const char*	strLoginAttemptTempBanThreshold="LoginAttemptTempBanThreshold";
static const char*	strLoginAttemptTempBanDuration="LoginAttemptTempBanDuration";
static const char*	strLoginAttemptFilterThreshold="LoginAttemptFilterThreshold";
static const char*	strJavaScriptMaxBytes		="JavaScriptMaxBytes";
static const char*	strJavaScriptContextStack	="JavaScriptContextStack";
static const char*	strJavaScriptTimeLimit		="JavaScriptTimeLimit";
static const char*	strJavaScriptGcInterval		="JavaScriptGcInterval";
static const char*	strJavaScriptYieldInterval	="JavaScriptYieldInterval";
static const char*	strJavaScriptLoadPath		="JavaScriptLoadPath";
static const char*	strSemFileCheckFrequency	="SemFileCheckFrequency";

#define DEFAULT_LOG_LEVEL				LOG_DEBUG
#define DEFAULT_BIND_RETRY_COUNT		2
#define DEFAULT_BIND_RETRY_DELAY		15

void sbbs_get_ini_fname(char* ini_file, char* ctrl_dir, char* pHostName)
{
	/* pHostName is no longer used since iniFileName calls gethostname() itself */

#if defined(_WINSOCKAPI_)	 
	WSADATA WSAData;	 
    WSAStartup(MAKEWORD(1,1), &WSAData); /* req'd for gethostname */	 
#endif	 

#if defined(__unix__) && defined(PREFIX)
	sprintf(ini_file,PREFIX"/etc/sbbs.ini");
	if(fexistcase(ini_file))
		return;
#endif
	iniFileName(ini_file,MAX_PATH,ctrl_dir,"sbbs.ini");

#if defined(_WINSOCKAPI_)	 
	WSACleanup();	 
#endif
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
	char	value[INI_MAX_VALUE_LEN];
    char*   p;

	js->max_bytes		= (ulong)iniGetBytes(list,section,strJavaScriptMaxBytes		,/* unit: */1,defaults->max_bytes);
	js->cx_stack		= (ulong)iniGetBytes(list,section,strJavaScriptContextStack	,/* unit: */1,defaults->cx_stack);
	js->time_limit		= iniGetInteger(list,section,strJavaScriptTimeLimit		,defaults->time_limit);
	js->gc_interval		= iniGetInteger(list,section,strJavaScriptGcInterval	,defaults->gc_interval);
	js->yield_interval	= iniGetInteger(list,section,strJavaScriptYieldInterval	,defaults->yield_interval);

	/* Get JavaScriptLoadPath, use default if key is missing, use blank if key value is blank */
    if((p=iniGetExistingString(list, section, strJavaScriptLoadPath, nulstr, value)) == NULL) {
		if(defaults!=js)
			SAFECOPY(js->load_path, defaults->load_path);
	} else
        SAFECOPY(js->load_path, p);

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
			,JAVASCRIPT_TIME_LIMIT
			,JAVASCRIPT_GC_INTERVAL
			,JAVASCRIPT_YIELD_INTERVAL
            ,JAVASCRIPT_LOAD_PATH
		};
	SAFECOPY(global_defaults.load_path, JAVASCRIPT_LOAD_PATH);

	if(defaults==NULL)
		defaults=&global_defaults;

	sbbs_fix_js_settings(js);

	if(js->max_bytes==defaults->max_bytes)
		iniRemoveValue(lp,section,strJavaScriptMaxBytes);
	else
		failure|=iniSetBytes(lp,section,strJavaScriptMaxBytes,/*unit: */1, js->max_bytes,style)==NULL;

	if(js->cx_stack==defaults->cx_stack)
		iniRemoveValue(lp,section,strJavaScriptContextStack);
	else 
		failure|=iniSetBytes(lp,section,strJavaScriptContextStack,/*unit: */1,js->cx_stack,style)==NULL;

	if(js->time_limit==defaults->time_limit)
		iniRemoveValue(lp,section,strJavaScriptTimeLimit);
	else
		failure|=iniSetInteger(lp,section,strJavaScriptTimeLimit,js->time_limit,style)==NULL;

	if(js->gc_interval==defaults->gc_interval)
		iniRemoveValue(lp,section,strJavaScriptGcInterval);
	else 
		failure|=iniSetInteger(lp,section,strJavaScriptGcInterval,js->gc_interval,style)==NULL;

	if(js->yield_interval==defaults->yield_interval)
		iniRemoveValue(lp,section,strJavaScriptYieldInterval);
	else 
		failure|=iniSetInteger(lp,section,strJavaScriptYieldInterval,js->yield_interval,style)==NULL;

	if(strcmp(js->load_path,defaults->load_path)==0)
		iniRemoveKey(lp,section,strJavaScriptLoadPath);
	else
		failure|=iniSetString(lp,section,strJavaScriptLoadPath,js->load_path,style)==NULL;

	return(!failure);
}

static struct login_attempt_settings get_login_attempt_settings(str_list_t list, const char* section, global_startup_t* global)
{
	struct login_attempt_settings settings;

	settings.delay				=iniGetInteger(list,section,strLoginAttemptDelay			,global == NULL ? 5000 : global->login_attempt.delay);
	settings.throttle			=iniGetInteger(list,section,strLoginAttemptThrottle			,global == NULL ? 1000 : global->login_attempt.throttle);
	settings.hack_threshold		=iniGetInteger(list,section,strLoginAttemptHackThreshold	,global == NULL ? 10 : global->login_attempt.hack_threshold);
	settings.tempban_threshold	=iniGetInteger(list,section,strLoginAttemptTempBanThreshold	,global == NULL ? 20 : global->login_attempt.tempban_threshold);
	settings.tempban_duration	=(ulong)iniGetDuration(list,section,strLoginAttemptTempBanDuration	,global == NULL ? (10*60) : global->login_attempt.tempban_duration);
	settings.filter_threshold	=iniGetInteger(list,section,strLoginAttemptFilterThreshold	,global == NULL ? 0 : global->login_attempt.filter_threshold);
	return settings;
}

static void set_login_attempt_settings(str_list_t* lp, const char* section, struct login_attempt_settings settings, ini_style_t style)
{
	iniSetInteger(lp,section,strLoginAttemptDelay,settings.delay,&style);
	iniSetInteger(lp,section,strLoginAttemptThrottle,settings.throttle,&style);
	iniSetInteger(lp,section,strLoginAttemptHackThreshold,settings.hack_threshold,&style);
	iniSetInteger(lp,section,strLoginAttemptTempBanThreshold,settings.tempban_threshold,&style);
	iniSetDuration(lp,section,strLoginAttemptTempBanDuration,settings.tempban_duration,&style);
	iniSetInteger(lp,section,strLoginAttemptFilterThreshold,settings.filter_threshold,&style);
}

static void get_ini_globals(str_list_t list, global_startup_t* global)
{
	const char* section = "Global";
	char		value[INI_MAX_VALUE_LEN];
	char*		p;
	struct in6_addr	wildcard6 = {{{0}}};

	p=iniGetString(list,section,strCtrlDirectory,nulstr,value);
	if(*p) {
	    SAFECOPY(global->ctrl_dir,value);
		backslash(global->ctrl_dir);
    }

	p=iniGetString(list,section,strTempDirectory,nulstr,value);
	if(*p) {
	    SAFECOPY(global->temp_dir,value);
		backslash(global->temp_dir);
    }

	p=iniGetString(list,section,strHostName,nulstr,value);
	if(*p)
        SAFECOPY(global->host_name,value);

	global->sem_chk_freq=iniGetShortInt(list,section,strSemFileCheckFrequency,DEFAULT_SEM_CHK_FREQ);
	iniFreeStringList(global->interfaces);
	global->interfaces=iniGetStringList(list,section,strInterfaces, ",", "0.0.0.0,::");
	global->outgoing4.s_addr=iniGetIpAddress(list,section,strOutgoing4,0);
	global->outgoing6=iniGetIp6Address(list,section,strOutgoing6,wildcard6);
	global->log_level=iniGetLogLevel(list,section,strLogLevel,DEFAULT_LOG_LEVEL);
	global->bind_retry_count=iniGetInteger(list,section,strBindRetryCount,DEFAULT_BIND_RETRY_COUNT);
	global->bind_retry_delay=iniGetInteger(list,section,strBindRetryDelay,DEFAULT_BIND_RETRY_DELAY);
	global->login_attempt = get_login_attempt_settings(list, section, NULL);

	/* Setup default values here */
	global->js.max_bytes		= JAVASCRIPT_MAX_BYTES;
	global->js.cx_stack			= JAVASCRIPT_CONTEXT_STACK;
	global->js.time_limit		= JAVASCRIPT_TIME_LIMIT;
	global->js.gc_interval		= JAVASCRIPT_GC_INTERVAL;
	global->js.yield_interval	= JAVASCRIPT_YIELD_INTERVAL;
    SAFECOPY(global->js.load_path, JAVASCRIPT_LOAD_PATH);

	/* Read .ini values here */
	sbbs_get_js_settings(list, section, &global->js, &global->js);
}


void sbbs_read_ini(
	 FILE*					fp
	,const char*			ini_fname
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
	struct in6_addr	wildcard6 = {{{0}}};
	char		*global_interfaces;

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

	if(ini_fname!=NULL && ini_fname[0]) {
		if(bbs!=NULL)		SAFECOPY(bbs->ini_fname, ini_fname);
		if(ftp!=NULL)		SAFECOPY(ftp->ini_fname, ini_fname);
		if(web!=NULL)		SAFECOPY(web->ini_fname, ini_fname);
		if(mail!=NULL)		SAFECOPY(mail->ini_fname, ini_fname);
		if(services!=NULL)	SAFECOPY(services->ini_fname, ini_fname);
	}

	global_interfaces = strListCombine(global->interfaces, NULL, 16384, ",");

	/***********************************************************************/
	section = "BBS";

	if(run_bbs!=NULL)
		*run_bbs=iniGetBool(list,section,strAutoStart,TRUE);

	if(bbs!=NULL) {

		bbs->outgoing4.s_addr
			=iniGetIpAddress(list,section,strOutgoing4,global->outgoing4.s_addr);
		bbs->outgoing6
			=iniGetIp6Address(list,section,strOutgoing6,global->outgoing6);

		bbs->telnet_port
			=iniGetShortInt(list,section,"TelnetPort",IPPORT_TELNET);
		iniFreeStringList(bbs->telnet_interfaces);
		bbs->telnet_interfaces
			=iniGetStringList(list,section,"TelnetInterface",",",global_interfaces);

		bbs->rlogin_port
			=iniGetShortInt(list,section,"RLoginPort",513);
		iniFreeStringList(bbs->rlogin_interfaces);
		bbs->rlogin_interfaces
			=iniGetStringList(list,section,"RLoginInterface",",",global_interfaces);

		bbs->pet40_port
			=iniGetShortInt(list,section,"Pet40Port",64);
		bbs->pet80_port
			=iniGetShortInt(list,section,"Pet80Port",128);

		bbs->ssh_port
			=iniGetShortInt(list,section,"SSHPort",22);
		bbs->ssh_connect_timeout
			=iniGetShortInt(list,section,"SSHConnectTimeout",10);
		iniFreeStringList(bbs->ssh_interfaces);
		bbs->ssh_interfaces
			=iniGetStringList(list,section,"SSHInterface",",",global_interfaces);

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
				,BBS_OPT_XTRN_MINIMIZED);

		bbs->bind_retry_count=iniGetInteger(list,section,strBindRetryCount,global->bind_retry_count);
		bbs->bind_retry_delay=iniGetInteger(list,section,strBindRetryDelay,global->bind_retry_delay);

		bbs->login_attempt = get_login_attempt_settings(list, section, global);
		bbs->max_concurrent_connections = iniGetInteger(list, section, "MaxConcurrentConnections", 0);
	}

	/***********************************************************************/
	section = "FTP";

	if(run_ftp!=NULL)
		*run_ftp=iniGetBool(list,section,strAutoStart,TRUE);

	if(ftp!=NULL) {

		ftp->outgoing4.s_addr
			=iniGetIpAddress(list,section,strOutgoing4,global->outgoing4.s_addr);
		ftp->outgoing6
			=iniGetIp6Address(list,section,strOutgoing6,global->outgoing6);
		ftp->port
			=iniGetShortInt(list,section,strPort,IPPORT_FTP);
		iniFreeStringList(ftp->interfaces);
		ftp->interfaces
			=iniGetStringList(list,section,strInterfaces,",",global_interfaces);
		ftp->max_clients
			=iniGetShortInt(list,section,strMaxClients,FTP_DEFAULT_MAX_CLIENTS);
		ftp->max_inactivity
			=iniGetShortInt(list,section,strMaxInactivity,FTP_DEFAULT_MAX_INACTIVITY);	/* seconds */
		ftp->qwk_timeout
			=iniGetShortInt(list,section,"QwkTimeout",FTP_DEFAULT_QWK_TIMEOUT);		/* seconds */
		ftp->sem_chk_freq
			=iniGetShortInt(list,section,strSemFileCheckFrequency,global->sem_chk_freq);
		ftp->min_fsize
			=iniGetBytes(list,section,"MinFileSize",1,0);
		ftp->max_fsize
			=iniGetBytes(list,section,"MaxFileSize",1,0);

		/* Passive transfer settings (for stupid firewalls/NATs) */
		ftp->pasv_ip_addr.s_addr
			=iniGetIpAddress(list,section,"PasvIpAddress",0);
		ftp->pasv_ip6_addr
			=iniGetIp6Address(list,section,"PasvIp6Address",wildcard6);
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
		ftp->login_attempt = get_login_attempt_settings(list, section, global);
	}

	/***********************************************************************/
	section = "Mail";

	if(run_mail!=NULL)
		*run_mail=iniGetBool(list,section,strAutoStart,TRUE);

	if(mail!=NULL) {

		iniFreeStringList(mail->interfaces);
		mail->interfaces
			=iniGetStringList(list,section,strInterfaces,",",global_interfaces);
		mail->outgoing4.s_addr
			=iniGetIpAddress(list,section,strOutgoing4,global->outgoing4.s_addr);
		mail->outgoing6
			=iniGetIp6Address(list,section,strOutgoing6,global->outgoing6);
		mail->smtp_port
			=iniGetShortInt(list,section,"SMTPPort",IPPORT_SMTP);
		mail->submission_port
			=iniGetShortInt(list,section,"SubmissionPort",IPPORT_SUBMISSION);
		mail->submissions_port
			=iniGetShortInt(list,section,"TLSSubmissionPort",IPPORT_SUBMISSIONS);
		iniFreeStringList(mail->pop3_interfaces);
		mail->pop3_interfaces
			=iniGetStringList(list,section,"POP3Interface",",",global_interfaces);
		mail->pop3_port
			=iniGetShortInt(list,section,"POP3Port",IPPORT_POP3);
		mail->pop3s_port
			=iniGetShortInt(list,section,"TLSPOP3Port",IPPORT_POP3S);
		mail->relay_port
			=iniGetShortInt(list,section,"RelayPort",IPPORT_SMTP);
		mail->max_clients
			=iniGetShortInt(list,section,strMaxClients,MAIL_DEFAULT_MAX_CLIENTS);
		mail->max_inactivity
			=iniGetShortInt(list,section,strMaxInactivity,MAIL_DEFAULT_MAX_INACTIVITY);		/* seconds */
		mail->max_delivery_attempts
			=iniGetShortInt(list,section,"MaxDeliveryAttempts",MAIL_DEFAULT_MAX_DELIVERY_ATTEMPTS);
		mail->rescan_frequency
			=iniGetShortInt(list,section,"RescanFrequency",MAIL_DEFAULT_RESCAN_FREQUENCY);	/* 60 minutes */
		mail->sem_chk_freq
			=iniGetShortInt(list,section,strSemFileCheckFrequency,global->sem_chk_freq);
		mail->lines_per_yield
			=iniGetShortInt(list,section,"LinesPerYield",MAIL_DEFAULT_LINES_PER_YIELD);
		mail->max_recipients
			=iniGetShortInt(list,section,"MaxRecipients",MAIL_DEFAULT_MAX_RECIPIENTS);
		mail->max_msg_size
			=(DWORD)iniGetBytes(list,section,"MaxMsgSize",/* units: */1,MAIL_DEFAULT_MAX_MSG_SIZE);
		mail->max_msgs_waiting
			=iniGetInteger(list,section,"MaxMsgsWaiting",MAIL_DEFAULT_MAX_MSGS_WAITING);
		mail->connect_timeout
			=iniGetInteger(list,section,"ConnectTimeout",MAIL_DEFAULT_CONNECT_TIMEOUT);

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

		SAFECOPY(mail->newmail_notice
			,iniGetString(list,section,"NewMailNotice","%.0s\1n\1mNew e-mail from \1h%s \1n<\1h%s\1n>\r\n", value));
		SAFECOPY(mail->forward_notice
			,iniGetString(list,section,"ForwardNotice","\1n\1mand it was automatically forwarded to: \1h%s\1n\r\n", value));
	
		/* JavaScript Operating Parameters */
		sbbs_get_js_settings(list, section, &mail->js, &global->js);

		mail->log_level
			=iniGetLogLevel(list,section,strLogLevel,global->log_level);
		mail->options
			=iniGetBitField(list,section,strOptions,mail_options
				,MAIL_OPT_ALLOW_POP3);

		mail->bind_retry_count=iniGetInteger(list,section,strBindRetryCount,global->bind_retry_count);
		mail->bind_retry_delay=iniGetInteger(list,section,strBindRetryDelay,global->bind_retry_delay);
		mail->login_attempt = get_login_attempt_settings(list, section, global);
	}

	/***********************************************************************/
	section = "Services";

	if(run_services!=NULL)
		*run_services=iniGetBool(list,section,strAutoStart,TRUE);

	if(services!=NULL) {

		iniFreeStringList(services->interfaces);
		services->interfaces
			=iniGetStringList(list,section,strInterfaces,",",global_interfaces);
		services->outgoing4.s_addr
			=iniGetIpAddress(list,section,strOutgoing4,global->outgoing4.s_addr);
		services->outgoing6
			=iniGetIp6Address(list,section,strOutgoing6,global->outgoing6);

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
		services->login_attempt = get_login_attempt_settings(list, section, global);
	}

	/***********************************************************************/
	section = "Web";

	if(run_web!=NULL)
		*run_web=iniGetBool(list,section,strAutoStart,FALSE);

	if(web!=NULL) {

		iniFreeStringList(web->interfaces);
		web->interfaces
			=iniGetStringList(list,section,strInterfaces,",",global_interfaces);
		iniFreeStringList(web->tls_interfaces);
		web->tls_interfaces
			=iniGetStringList(list,section,"TLSInterface",",",global_interfaces);
		web->port
			=iniGetShortInt(list,section,strPort,IPPORT_HTTP);
		web->tls_port
			=iniGetShortInt(list,section,"TLSPort",IPPORT_HTTPS);
		web->max_clients
			=iniGetShortInt(list,section,strMaxClients,WEB_DEFAULT_MAX_CLIENTS);
		web->max_inactivity
			=iniGetShortInt(list,section,strMaxInactivity,WEB_DEFAULT_MAX_INACTIVITY);		/* seconds */
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

		web->max_cgi_inactivity
			=iniGetShortInt(list,section,"MaxCgiInactivity",WEB_DEFAULT_MAX_CGI_INACTIVITY);	/* seconds */

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
		web->outbuf_drain_timeout
			=iniGetShortInt(list,section,"OutbufDrainTimeout",10);

		web->bind_retry_count=iniGetInteger(list,section,strBindRetryCount,global->bind_retry_count);
		web->bind_retry_delay=iniGetInteger(list,section,strBindRetryDelay,global->bind_retry_delay);
		web->login_attempt = get_login_attempt_settings(list, section, global);
	}

	free(global_interfaces);
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
	style.section_separator = "";
	style.value_separator = " = ";
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

		iniSetString(lp,section,strCtrlDirectory,global->ctrl_dir,&style);
		iniSetString(lp,section,strTempDirectory,global->temp_dir,&style);
		iniSetString(lp,section,strHostName,global->host_name,&style);
		iniSetShortInt(lp,section,strSemFileCheckFrequency,global->sem_chk_freq,&style);
		iniSetIpAddress(lp,section,strOutgoing4,global->outgoing4.s_addr,&style);
		iniSetIp6Address(lp,section,strOutgoing6,global->outgoing6,&style);
		iniSetStringList(lp, section, strInterfaces, ",", global->interfaces, &style);
		iniSetLogLevel(lp,section,strLogLevel,global->log_level,&style);
		iniSetInteger(lp,section,strBindRetryCount,global->bind_retry_count,&style);
		iniSetInteger(lp,section,strBindRetryDelay,global->bind_retry_delay,&style);
		set_login_attempt_settings(lp, section, global->login_attempt, style);

		/* JavaScript operating parameters */
		if(!sbbs_set_js_settings(lp,section,&global->js,NULL,&style))
			break;
	}

	/***********************************************************************/
	if(bbs!=NULL) {

		section = "BBS";

		if(!iniSetBool(lp,section,strAutoStart,run_bbs,&style))
			break;

		if(strListCmp(bbs->telnet_interfaces, global->interfaces)==0)
			iniRemoveValue(lp,section,"TelnetInterface");
		else if(!iniSetStringList(lp,section,"TelnetInterface", ",", bbs->telnet_interfaces, &style))
			break;

		if(!iniSetShortInt(lp,section,"TelnetPort",bbs->telnet_port,&style))
			break;

		if(strListCmp(bbs->rlogin_interfaces, global->interfaces)==0)
			iniRemoveValue(lp,section,"RLoginInterface");
		else if(!iniSetStringList(lp,section,"RLoginInterface", ",", bbs->rlogin_interfaces,&style))
			break;
		if(!iniSetShortInt(lp,section,"RLoginPort",bbs->rlogin_port,&style))
			break;

		if(!iniSetShortInt(lp,section,"Pet40Port",bbs->pet40_port,&style))
			break;
		if(!iniSetShortInt(lp,section,"Pet80Port",bbs->pet80_port,&style))
			break;

		if(strListCmp(bbs->ssh_interfaces, global->interfaces)==0)
			iniRemoveValue(lp,section,"SSHInterface");
		else if(!iniSetStringList(lp,section,"SSHInterface", ",", bbs->ssh_interfaces,&style))
			break;
		if(!iniSetShortInt(lp,section,"SSHPort",bbs->ssh_port,&style))
			break;
		if(!iniSetShortInt(lp,section,"SSHConnectTimeout",bbs->ssh_connect_timeout,&style))
			break;

		if(!iniSetShortInt(lp,section,"FirstNode",bbs->first_node,&style))
			break;
		if(!iniSetShortInt(lp,section,"LastNode",bbs->last_node,&style))
			break;
		if(!iniSetShortInt(lp,section,"OutbufHighwaterMark",bbs->outbuf_highwater_mark,&style))
			break;
		if(!iniSetShortInt(lp,section,"OutbufDrainTimeout",bbs->outbuf_drain_timeout,&style))
			break;
		if(!iniSetInteger(lp,section,"MaxConcurrentConnections",bbs->max_concurrent_connections,&style))
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

		if(strListCmp(ftp->interfaces, global->interfaces)==0)
			iniRemoveValue(lp,section,strInterfaces);
		else if(!iniSetStringList(lp,section,strInterfaces,",",ftp->interfaces,&style))
			break;

		if(ftp->outgoing4.s_addr == global->outgoing4.s_addr)
			iniRemoveValue(lp,section,strOutgoing4);
		else if(!iniSetIpAddress(lp, section, strOutgoing4, ftp->outgoing4.s_addr, &style))
			break;

		if(memcmp(&ftp->outgoing6, &global->outgoing6, sizeof(ftp->outgoing6)))
			iniRemoveValue(lp,section,strOutgoing6);
		else if(!iniSetIp6Address(lp, section, strOutgoing6, ftp->outgoing6, &style))
			break;

		if(!iniSetShortInt(lp,section,strPort,ftp->port,&style))
			break;
		if(!iniSetShortInt(lp,section,strMaxClients,ftp->max_clients,&style))
			break;
		if(!iniSetShortInt(lp,section,strMaxInactivity,ftp->max_inactivity,&style))
			break;
		if(!iniSetShortInt(lp,section,"QwkTimeout",ftp->qwk_timeout,&style))
			break;
		if(!iniSetBytes(lp,section,"MinFileSize",1,ftp->min_fsize,&style))
			break;
		if(!iniSetBytes(lp,section,"MaxFileSize",1,ftp->max_fsize,&style))
			break;

		/* Passive transfer settings */
		if(!iniSetIpAddress(lp,section,"PasvIpAddress",ftp->pasv_ip_addr.s_addr,&style))
			break;
		if(!iniSetIp6Address(lp,section,"PasvIp6Address",ftp->pasv_ip6_addr,&style))
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
            || (bbs != NULL && strcmp(bbs->host_name,cfg->sys_inetaddr)==0))
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

		if(strListCmp(mail->interfaces, global->interfaces)==0)
			iniRemoveValue(lp,section,strInterfaces);
		else if(!iniSetStringList(lp,section,strInterfaces,",",mail->interfaces,&style))
			break;

		if(mail->outgoing4.s_addr == global->outgoing4.s_addr)
			iniRemoveValue(lp,section,strOutgoing4);
		else if(!iniSetIpAddress(lp, section, strOutgoing4, mail->outgoing4.s_addr, &style))
			break;

		if(memcmp(&mail->outgoing6, &global->outgoing6, sizeof(ftp->outgoing6)))
			iniRemoveValue(lp,section,strOutgoing6);
		else if(!iniSetIp6Address(lp, section, strOutgoing6, mail->outgoing6, &style))
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
		if(!iniSetShortInt(lp,section,"SubmissionPort",mail->submission_port,&style))
			break;
		if(!iniSetShortInt(lp,section,"TLSSubmissionPort",mail->submissions_port,&style))
			break;
		if(!iniSetShortInt(lp,section,"POP3Port",mail->pop3_port,&style))
			break;
		if(!iniSetShortInt(lp,section,"TLSPOP3Port",mail->pop3s_port,&style))
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
		if(!iniSetBytes(lp,section,"MaxMsgSize",/* unit: */1,mail->max_msg_size,&style))
			break;
		if(!iniSetInteger(lp,section,"MaxMsgsWaiting",mail->max_msgs_waiting,&style))
			break;
		if(!iniSetInteger(lp,section,"ConnectTimeout",mail->connect_timeout,&style))
			break;

		if(strcmp(mail->host_name,global->host_name)==0
            || (bbs != NULL && strcmp(bbs->host_name,cfg->sys_inetaddr)==0))
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
#if 0
		if(!iniSetStringLiteral(lp,section,"NewMailNotice",mail->newmail_notice,&style))
			break;
		if(!iniSetStringLiteral(lp,section,"ForwardNotice",mail->forward_notice,&style))
			break;
#endif
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

		if(strListCmp(services->interfaces, global->interfaces)==0)
			iniRemoveValue(lp,section,strInterfaces);
		else if(!iniSetStringList(lp,section,strInterfaces,",",services->interfaces,&style))
			break;

		if(services->outgoing4.s_addr == global->outgoing4.s_addr)
			iniRemoveValue(lp,section,strOutgoing4);
		else if(!iniSetIpAddress(lp, section, strOutgoing4, services->outgoing4.s_addr, &style))
			break;

		if(memcmp(&services->outgoing6, &global->outgoing6, sizeof(ftp->outgoing6)))
			iniRemoveValue(lp,section,strOutgoing6);
		else if(!iniSetIp6Address(lp, section, strOutgoing6, services->outgoing6, &style))
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
            || (bbs != NULL && strcmp(bbs->host_name,cfg->sys_inetaddr)==0))
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

		if(strListCmp(web->interfaces, global->interfaces)==0)
			iniRemoveValue(lp,section,strInterfaces);
		else if(!iniSetStringList(lp,section,strInterfaces,",",web->interfaces,&style))
			break;

		if(strListCmp(web->tls_interfaces, global->interfaces)==0)
			iniRemoveValue(lp,section,"TLSInterface");
		else if(!iniSetStringList(lp,section,"TLSInterface",",",web->tls_interfaces,&style))
			break;

		if(!iniSetShortInt(lp,section,strPort,web->port,&style))
			break;
		if(!iniSetShortInt(lp,section,"TLSPort",web->tls_port,&style))
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
            || (bbs != NULL && strcmp(bbs->host_name,cfg->sys_inetaddr)==0))
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
		if(!iniSetShortInt(lp,section,"OutbufDrainTimeout",web->outbuf_drain_timeout,&style))
			break;
	}

	/***********************************************************************/
	result=iniWriteFile(fp,list);

	} while(0);	/* finally */

	iniFreeStringList(list);

	return(result);
}
