/* sbbs_ini.c */

/* Synchronet console configuration (.ini) file routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include "dirwrap.h"	/* backslash */

static const char*	nulstr="";

static ini_bitdesc_t bbs_options[] = {

	{ BBS_OPT_KEEP_ALIVE			,"KEEP_ALIVE"			},
	{ BBS_OPT_XTRN_MINIMIZED		,"XTRN_MINIMIZED"		},
	{ BBS_OPT_AUTO_LOGON			,"AUTO_LOGON"			},
	{ BBS_OPT_DEBUG_TELNET			,"DEBUG_TELNET"			},
	{ BBS_OPT_SYSOP_AVAILABLE		,"SYSOP_AVAILABLE"		},
	{ BBS_OPT_ALLOW_RLOGIN			,"ALLOW_RLOGIN"			},
	{ BBS_OPT_USE_2ND_RLOGIN		,"USE_2ND_RLOGIN"		},
	{ BBS_OPT_NO_QWK_EVENTS			,"NO_QWK_EVENTS"		},
	{ BBS_OPT_NO_TELNET_GA			,"NO_TELNET_GA"			},
	{ BBS_OPT_NO_EVENTS				,"NO_EVENTS"			},
	{ BBS_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ BBS_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ BBS_OPT_GET_IDENT				,"GET_IDENT"			},
	{ BBS_OPT_NO_JAVASCRIPT			,"NO_JAVASCRIPT"		},
	{ BBS_OPT_LOCAL_TIMEZONE		,"LOCAL_TIMEZONE"		},
	{ BBS_OPT_MUTE					,"MUTE"					},
	/* terminator */										
	{ -1							,NULL					}
};

static ini_bitdesc_t ftp_options[] = {

	{ FTP_OPT_DEBUG_RX				,"DEBUG_RX"				},
	{ FTP_OPT_DEBUG_DATA			,"DEBUG_DATA"			},	
	{ FTP_OPT_INDEX_FILE			,"INDEX_FILE"			},
	{ FTP_OPT_DEBUG_TX				,"DEBUG_TX"				},
	{ FTP_OPT_ALLOW_QWK				,"ALLOW_QWK"			},
	{ FTP_OPT_NO_LOCAL_FSYS			,"NO_LOCAL_FSYS"		},
	{ FTP_OPT_DIR_FILES				,"DIR_FILES"			},
	{ FTP_OPT_KEEP_TEMP_FILES		,"KEEP_TEMP_FILES"		},
	{ FTP_OPT_HTML_INDEX_FILE		,"HTML_INDEX_FILE"		},
	{ FTP_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ FTP_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ FTP_OPT_NO_JAVASCRIPT			,"NO_JAVASCRIPT"		},
	{ FTP_OPT_LOCAL_TIMEZONE		,"LOCAL_TIMEZONE"		},
	{ FTP_OPT_MUTE					,"MUTE"					},
	/* terminator */										
	{ -1							,NULL					}
};

static ini_bitdesc_t web_options[] = {

	{ WEB_OPT_DEBUG_RX				,"DEBUG_RX"				},
	{ WEB_OPT_DEBUG_TX				,"DEBUG_TX"				},
	{ WEB_OPT_VIRTUAL_HOSTS			,"VIRTUAL_HOSTS"		},
	{ WEB_OPT_NO_CGI				,"NO_CGI"				},

	/* shared bits */
	{ BBS_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ BBS_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ BBS_OPT_GET_IDENT				,"GET_IDENT"			},
	{ BBS_OPT_NO_JAVASCRIPT			,"NO_JAVASCRIPT"		},
	{ BBS_OPT_LOCAL_TIMEZONE		,"LOCAL_TIMEZONE"		},
	{ BBS_OPT_MUTE					,"MUTE"					},

	/* terminator */										
	{ -1							,NULL					}
};

static ini_bitdesc_t mail_options[] = {

	{ MAIL_OPT_DEBUG_RX_HEADER		,"DEBUG_RX_HEADER"		},
	{ MAIL_OPT_DEBUG_RX_BODY		,"DEBUG_RX_BODY"		},	
	{ MAIL_OPT_ALLOW_POP3			,"ALLOW_POP3"			},
	{ MAIL_OPT_DEBUG_TX				,"DEBUG_TX"				},
	{ MAIL_OPT_DEBUG_RX_RSP			,"DEBUG_RX_RSP"			},
	{ MAIL_OPT_RELAY_TX				,"RELAY_TX"				},
	{ MAIL_OPT_DEBUG_POP3			,"DEBUG_POP3"			},
	{ MAIL_OPT_ALLOW_RX_BY_NUMBER	,"ALLOW_RX_BY_NUMBER"	},
	{ MAIL_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ MAIL_OPT_USE_TCP_DNS			,"USE_TCP_DNS"			},
	{ MAIL_OPT_NO_SENDMAIL			,"NO_SENDMAIL"			},
	{ MAIL_OPT_ALLOW_RELAY			,"ALLOW_RELAY"			},
	{ MAIL_OPT_SMTP_AUTH_VIA_IP		,"SMTP_AUTH_VIA_IP"		},
	{ MAIL_OPT_DNSBL_REFUSE			,"DNSBL_REFUSE"			},
	{ MAIL_OPT_DNSBL_IGNORE			,"DNSBL_IGNORE"			},
	{ MAIL_OPT_DNSBL_BADUSER		,"DNSBL_BADUSER"		},
	{ MAIL_OPT_DNSBL_DEBUG			,"DNSBL_DEBUG"			},
	{ MAIL_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ MAIL_OPT_LOCAL_TIMEZONE		,"LOCAL_TIMEZONE"		},
	{ MAIL_OPT_MUTE					,"MUTE"					},
	/* terminator */
	{ -1							,NULL					}
};

static ini_bitdesc_t service_options[] = {

	{ BBS_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ BBS_OPT_GET_IDENT				,"GET_IDENT"			},
	{ BBS_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ BBS_OPT_MUTE					,"MUTE"					},
	/* terminator */				
	{ -1							,NULL					}
};


void sbbs_read_ini(
	 FILE*					fp
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
	const char* strInterface="Interface";
	const char* strJavaScriptMaxBytes="JavaScriptMaxBytes";
	const char* strSemFileCheckFrequency="SemFileCheckFrequency";
	char*		ctrl_dir;
	char*		temp_dir;
	char		host_name[128];
	char		value[MAX_VALUE_LEN];
	ulong		interface_addr;
	ulong		js_max_bytes;
	ushort		sem_chk_freq;

	section = "Global";

	ctrl_dir=iniGetString(fp,section,"CtrlDirectory",nulstr,value);
	if(*ctrl_dir) {
		backslash(ctrl_dir);
		if(bbs!=NULL)		SAFECOPY(bbs->ctrl_dir,ctrl_dir);
		if(ftp!=NULL)		SAFECOPY(ftp->ctrl_dir,ctrl_dir);
		if(mail!=NULL)		SAFECOPY(mail->ctrl_dir,ctrl_dir);
		if(services!=NULL)	SAFECOPY(services->ctrl_dir,ctrl_dir);
	}
	temp_dir=iniGetString(fp,section,"TempDirectory",nulstr,value);
	if(*temp_dir) {
		backslash(temp_dir);
		if(bbs!=NULL)		SAFECOPY(bbs->temp_dir,temp_dir);
		if(ftp!=NULL)		SAFECOPY(ftp->temp_dir,temp_dir);
	}

	SAFECOPY(host_name,iniGetString(fp,section,"HostName",nulstr,value));
	js_max_bytes=iniGetInteger(fp,section,strJavaScriptMaxBytes,0);
	sem_chk_freq=iniGetShortInt(fp,section,strSemFileCheckFrequency,0);
	interface_addr=iniGetIpAddress(fp,section,strInterface,INADDR_ANY);
																		
	/***********************************************************************/
	if(bbs!=NULL) {

		section = "BBS";

		if(run_bbs!=NULL)
			*run_bbs=iniGetBool(fp,section,"AutoStart",TRUE);

		bbs->telnet_interface
			=iniGetIpAddress(fp,section,"TelnetInterface",interface_addr);
		bbs->telnet_port
			=iniGetShortInt(fp,section,"TelnetPort",IPPORT_TELNET);

		bbs->rlogin_interface
			=iniGetIpAddress(fp,section,"RLoginInterface",interface_addr);
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
			=iniGetShortInt(fp,section,strSemFileCheckFrequency,sem_chk_freq);

		bbs->xtrn_polls_before_yield
			=iniGetInteger(fp,section,"ExternalYield",10);
		bbs->js_max_bytes
			=iniGetInteger(fp,section,strJavaScriptMaxBytes,js_max_bytes);

		SAFECOPY(bbs->host_name
			,iniGetString(fp,section,"HostName",host_name,value));

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

		bbs->options
			=iniGetBitField(fp,section,"Options",bbs_options
				,BBS_OPT_XTRN_MINIMIZED|BBS_OPT_SYSOP_AVAILABLE);
	}

	/***********************************************************************/
	if(ftp!=NULL) {

		section = "FTP";

		if(run_ftp!=NULL)
			*run_ftp=iniGetBool(fp,section,"AutoStart",TRUE);

		ftp->interface_addr
			=iniGetIpAddress(fp,section,strInterface,interface_addr);
		ftp->port
			=iniGetShortInt(fp,section,"Port",ftp->port);
		ftp->max_clients
			=iniGetShortInt(fp,section,"MaxClients",10);
		ftp->max_inactivity
			=iniGetShortInt(fp,section,"MaxInactivity",300);	/* seconds */
		ftp->qwk_timeout
			=iniGetShortInt(fp,section,"QwkTimeout",600);		/* seconds */
		ftp->sem_chk_freq
			=iniGetShortInt(fp,section,strSemFileCheckFrequency,sem_chk_freq);
		ftp->js_max_bytes
			=iniGetInteger(fp,section,strJavaScriptMaxBytes,js_max_bytes);

		SAFECOPY(ftp->host_name
			,iniGetString(fp,section,"HostName",host_name,value));

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

		ftp->options
			=iniGetBitField(fp,section,"Options",ftp_options
				,FTP_OPT_INDEX_FILE|FTP_OPT_HTML_INDEX_FILE|FTP_OPT_ALLOW_QWK);
	}

	/***********************************************************************/
	if(mail!=NULL) {

		section = "Mail";

		if(run_mail!=NULL)
			*run_mail=iniGetBool(fp,section,"AutoStart",TRUE);

		mail->interface_addr
			=iniGetIpAddress(fp,section,strInterface,interface_addr);
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
			=iniGetShortInt(fp,section,strSemFileCheckFrequency,sem_chk_freq);
		mail->lines_per_yield
			=iniGetShortInt(fp,section,"LinesPerYield",10);
		mail->max_recipients
			=iniGetShortInt(fp,section,"MaxRecipients",100);
		mail->max_msg_size
			=iniGetInteger(fp,section,"MaxMsgSize",10*1024*1024);	/* 10MB */

		SAFECOPY(mail->host_name
			,iniGetString(fp,section,"HostName",host_name,value));

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

		mail->options
			=iniGetBitField(fp,section,"Options",mail_options
				,MAIL_OPT_ALLOW_POP3);
	}

	/***********************************************************************/
	if(services!=NULL) {

		section = "Services";

		if(run_services!=NULL)
			*run_services=iniGetBool(fp,section,"AutoStart",TRUE);

		services->interface_addr
			=iniGetIpAddress(fp,section,strInterface,interface_addr);

		services->sem_chk_freq
			=iniGetShortInt(fp,section,strSemFileCheckFrequency,sem_chk_freq);

		services->js_max_bytes
			=iniGetInteger(fp,section,strJavaScriptMaxBytes,js_max_bytes);

		SAFECOPY(services->host_name
			,iniGetString(fp,section,"HostName",host_name,value));

		SAFECOPY(services->cfg_file
			,iniGetString(fp,section,"ConfigFile","services.cfg",value));

		SAFECOPY(services->answer_sound
			,iniGetString(fp,section,"AnswerSound",nulstr,value));
		SAFECOPY(services->hangup_sound
			,iniGetString(fp,section,"HangupSound",nulstr,value));

		services->options
			=iniGetBitField(fp,section,"Options",service_options
				,BBS_OPT_NO_HOST_LOOKUP);
	}

	/***********************************************************************/
	if(web!=NULL) {

		section = "Web";

		if(run_web!=NULL)
			*run_web=iniGetBool(fp,section,"AutoStart",FALSE);

		web->interface_addr
			=iniGetIpAddress(fp,section,strInterface,interface_addr);
		web->port
			=iniGetShortInt(fp,section,"Port",IPPORT_HTTP);
		web->sem_chk_freq
			=iniGetShortInt(fp,section,strSemFileCheckFrequency,sem_chk_freq);
		web->js_max_bytes
			=iniGetInteger(fp,section,strJavaScriptMaxBytes,js_max_bytes);

		SAFECOPY(web->host_name
			,iniGetString(fp,section,"HostName",host_name,value));

		SAFECOPY(web->root_dir
			,iniGetString(fp,section,"RootDirectory","../html",value));
		SAFECOPY(web->error_dir
			,iniGetString(fp,section,"ErrorDirectory","../html/error",value));

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

		web->options
			=iniGetBitField(fp,section,"Options",web_options
				,BBS_OPT_NO_HOST_LOOKUP);
	}
}
