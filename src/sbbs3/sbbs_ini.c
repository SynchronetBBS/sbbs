/* sbbs_ini.c */

#include "sbbs_ini.h"

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
	{ MAIL_OPT_DNSBL_REFUSE			,"DNSBL_REFUSE"			},
	{ MAIL_OPT_DNSBL_IGNORE			,"DNSBL_IGNORE"			},
	{ MAIL_OPT_DNSBL_BADUSER		,"DNSBL_BADUSER"		},
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
	const char* default_term;
	const char*	default_cgi_temp;
	char*		ctrl_dir;
	char		host_name[128];

	section = "Global";

	ctrl_dir=iniReadString(fp,section,"CtrlDirectory",nulstr);
	if(*ctrl_dir) {
		SAFECOPY(bbs->ctrl_dir,ctrl_dir);
		SAFECOPY(ftp->ctrl_dir,ctrl_dir);
		SAFECOPY(mail->ctrl_dir,ctrl_dir);
		SAFECOPY(services->ctrl_dir,ctrl_dir);
	}

	SAFECOPY(host_name,iniReadString(fp,section,"HostName",nulstr));
																		
	/***********************************************************************/
	section = "BBS";

	*run_bbs
		=iniReadBool(fp,section,"AutoStart",TRUE);

	bbs->telnet_interface
		=iniReadIpAddress(fp,section,"TelnetInterface",INADDR_ANY);
	bbs->telnet_port
		=iniReadShortInt(fp,section,"TelnetPort",IPPORT_TELNET);

	bbs->rlogin_interface
		=iniReadIpAddress(fp,section,"RLoginInterface",INADDR_ANY);
	bbs->rlogin_port
		=iniReadShortInt(fp,section,"RloginPort",513);

	bbs->first_node
		=iniReadShortInt(fp,section,"FirstNode",1);
	bbs->last_node
		=iniReadShortInt(fp,section,"LastNode",4);

	bbs->xtrn_polls_before_yield
		=iniReadInteger(fp,section,"ExternalYield",10);
	bbs->js_max_bytes
		=iniReadInteger(fp,section,"JavaScriptMaxBytes",0);

	/* Set default terminal type to "stock" termcap closest to "ansi-bbs" */
#if defined(__FreeBSD__)
	default_term="cons25";
#else
	default_term="pc3";
#endif

	SAFECOPY(bbs->host_name
		,iniReadString(fp,section,"HostName",host_name));

	SAFECOPY(bbs->xtrn_term
		,iniReadString(fp,section,"ExternalTerm",default_term));

	SAFECOPY(bbs->answer_sound
		,iniReadString(fp,section,"AnswerSound",nulstr));
	SAFECOPY(bbs->hangup_sound
		,iniReadString(fp,section,"HangupSound",nulstr));

	bbs->options
		=iniReadBitField(fp,section,"Options",bbs_options
			,BBS_OPT_XTRN_MINIMIZED|BBS_OPT_SYSOP_AVAILABLE);

	/***********************************************************************/
	section = "FTP";

	*run_ftp
		=iniReadBool(fp,section,"AutoStart",TRUE);

	ftp->interface_addr
		=iniReadIpAddress(fp,section,"Interface",INADDR_ANY);
	ftp->port
		=iniReadShortInt(fp,section,"Port",ftp->port);
	ftp->max_clients
		=iniReadShortInt(fp,section,"MaxClients",10);
	ftp->max_inactivity
		=iniReadShortInt(fp,section,"MaxInactivity",300);	/* seconds */
	ftp->qwk_timeout
		=iniReadShortInt(fp,section,"QwkTimeout",600);		/* seconds */
	ftp->js_max_bytes
		=iniReadInteger(fp,section,"JavaScriptMaxBytes",0);

	SAFECOPY(ftp->host_name
		,iniReadString(fp,section,"HostName",host_name));

	SAFECOPY(ftp->index_file_name
		,iniReadString(fp,section,"IndexFileName","00index"));
	SAFECOPY(ftp->html_index_file
		,iniReadString(fp,section,"HtmlIndexFile","00index.html"));
	SAFECOPY(ftp->html_index_script
		,iniReadString(fp,section,"HtmlIndexScript","ftp-html.js"));

	SAFECOPY(ftp->answer_sound
		,iniReadString(fp,section,"AnswerSound",nulstr));
	SAFECOPY(ftp->hangup_sound
		,iniReadString(fp,section,"HangupSound",nulstr));
	SAFECOPY(ftp->hack_sound
		,iniReadString(fp,section,"HackAttemptSound",nulstr));

	ftp->options
		=iniReadBitField(fp,section,"Options",ftp_options
			,FTP_OPT_INDEX_FILE|FTP_OPT_HTML_INDEX_FILE|FTP_OPT_ALLOW_QWK);

	/***********************************************************************/
	section = "Mail";

	*run_mail
		=iniReadBool(fp,section,"AutoStart",TRUE);

	mail->interface_addr
		=iniReadIpAddress(fp,section,"Interface",INADDR_ANY);
	mail->smtp_port
		=iniReadShortInt(fp,section,"SMTPPort",IPPORT_SMTP);
	mail->pop3_port
		=iniReadShortInt(fp,section,"POP3Port",IPPORT_POP3);
	mail->relay_port
		=iniReadShortInt(fp,section,"RelayPort",IPPORT_SMTP);
	mail->max_clients
		=iniReadShortInt(fp,section,"MaxClients",10);
	mail->max_inactivity
		=iniReadShortInt(fp,section,"MaxInactivity",120);		/* seconds */
	mail->max_delivery_attempts
		=iniReadShortInt(fp,section,"MaxDeliveryAttempts",50);
	mail->rescan_frequency
		=iniReadShortInt(fp,section,"RescanFrequency",3600);	/* 60 minutes */
	mail->lines_per_yield
		=iniReadShortInt(fp,section,"LinesPerYield",100);
	mail->max_recipients
		=iniReadShortInt(fp,section,"MaxRecipients",100);

	SAFECOPY(mail->host_name
		,iniReadString(fp,section,"HostName",host_name));

	SAFECOPY(mail->relay_server
		,iniReadString(fp,section,"RelayServer",mail->relay_server));
	SAFECOPY(mail->dns_server
		,iniReadString(fp,section,"DNSServer",mail->dns_server));

	SAFECOPY(mail->default_user
		,iniReadString(fp,section,"DefaultUser",nulstr));

	SAFECOPY(mail->dnsbl_hdr
		,iniReadString(fp,section,"DNSBlacklistHeader","X-DNSBL"));
	SAFECOPY(mail->dnsbl_tag
		,iniReadString(fp,section,"DNSBlacklistSubject","SPAM"));

	SAFECOPY(mail->pop3_sound
		,iniReadString(fp,section,"POP3Sound",nulstr));
	SAFECOPY(mail->inbound_sound
		,iniReadString(fp,section,"InboundSound",nulstr));
	SAFECOPY(mail->outbound_sound
		,iniReadString(fp,section,"OutboundSound",nulstr));

	mail->options
		=iniReadBitField(fp,section,"Options",mail_options
			,MAIL_OPT_ALLOW_POP3);

	/***********************************************************************/
	section = "Services";

	*run_services
		=iniReadBool(fp,section,"AutoStart",TRUE);

	services->interface_addr
		=iniReadIpAddress(fp,section,"Interface",INADDR_ANY);

	services->js_max_bytes
		=iniReadInteger(fp,section,"JavaScriptMaxBytes",0);

	SAFECOPY(services->host_name
		,iniReadString(fp,section,"HostName",host_name));

	SAFECOPY(services->cfg_file
		,iniReadString(fp,section,"ConfigFile",nulstr));

	SAFECOPY(services->answer_sound
		,iniReadString(fp,section,"AnswerSound",nulstr));
	SAFECOPY(services->hangup_sound
		,iniReadString(fp,section,"HangupSound",nulstr));

	services->options
		=iniReadBitField(fp,section,"Options",service_options
			,BBS_OPT_NO_HOST_LOOKUP);

	/***********************************************************************/
	section = "Web";

	*run_web
		=iniReadBool(fp,section,"AutoStart",FALSE);

	web->interface_addr
		=iniReadIpAddress(fp,section,"Interface",INADDR_ANY);
	web->port
		=iniReadShortInt(fp,section,"Port",IPPORT_HTTP);

	SAFECOPY(web->host_name
		,iniReadString(fp,section,"HostName",host_name));

	SAFECOPY(web->root_dir
		,iniReadString(fp,section,"RootDirectory","../html"));
	SAFECOPY(web->error_dir
		,iniReadString(fp,section,"ErrorDirectory","../html/error"));

	SAFECOPY(web->index_file_name
		,iniReadString(fp,section,"IndexFileName","index.html"));
	SAFECOPY(web->js_ext
		,iniReadString(fp,section,"JavaScriptExtension",".js"));

#ifdef __unix__
	default_cgi_temp = "/tmp";
#else
	if((default_cgi_temp = getenv("TEMP")) == NULL)
		default_cgi_temp = "";
#endif
	SAFECOPY(web->cgi_temp_dir
		,iniReadString(fp,section,"CGITempDirectory",default_cgi_temp));

	web->options
		=iniReadBitField(fp,section,"Options",web_options
			,BBS_OPT_NO_HOST_LOOKUP);
}
