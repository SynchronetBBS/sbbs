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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "scfg.h"
#include "sbbs_ini.h"
#include "netwrap.h"

const char* strDisabled = "<disabled>";

static const char* threshold(uint val)
{
	static char str[128];
	if(val == 0)
		return strDisabled;
	SAFEPRINTF(str, "%u", val);
	return str;
}

static const char* duration(uint val)
{
	static char str[128];
	if(val == 0)
		return strDisabled;
	return duration_to_vstr(val, str, sizeof(str));
}

static const char* maximum(uint val)
{
	static char str[128];
	if(val == 0)
		return "Unlimited";
	SAFEPRINTF(str, "%u", val);
	return str;
}

static void global_cfg(global_startup_t* startup)
{
	static int cur;
	char tmp[256];

	while(1) {
		int i = 0;
		sprintf(opt[i++], "%-40s%s", "Log Level", iniLogLevelStringList()[startup->log_level]);
		sprintf(opt[i++], "%-40s%s", "TLS Error Level", iniLogLevelStringList()[startup->tls_error_level]);
		sprintf(opt[i++], "%-40s%s", "Network Interfaces (IPv4/6)", strListCombine(startup->interfaces, tmp, sizeof(tmp), ", "));
		sprintf(opt[i++], "%-40s%s", "Outbound Interface (IPv4)", IPv4AddressToStr(startup->outgoing4.s_addr, tmp, sizeof(tmp)));
		sprintf(opt[i++], "%-40s%s", "Bind Retry Count", threshold(startup->bind_retry_count));
		sprintf(opt[i++], "%-40s%s", "Bind Retry Delay", duration(startup->bind_retry_delay));
		sprintf(opt[i++], "%-40s%u ms", "Failed Login Delay", startup->login_attempt.delay);
		sprintf(opt[i++], "%-40s%u ms", "Failed Login Throttle", startup->login_attempt.throttle);
		sprintf(opt[i++], "%-40s%s", "Failed Login Hack Log Threshold", threshold(startup->login_attempt.hack_threshold));
		sprintf(opt[i++], "%-40s%s", "Failed Login Temporary Ban Threshold", threshold(startup->login_attempt.tempban_threshold));
		sprintf(opt[i++], "%-40s%s", "Failed Login Temporary Ban Duration"
			,duration_to_vstr(startup->login_attempt.tempban_duration, tmp, sizeof(tmp)));
		sprintf(opt[i++], "%-40s%s", "Failed Login Auto-Filter Threshold", threshold(startup->login_attempt.filter_threshold));
		sprintf(opt[i++], "%-40s%s bytes", "JavaScript Heap Size", byte_count_to_str(startup->js.max_bytes, tmp, sizeof(tmp)));
		sprintf(opt[i++], "%-40s%u ticks", "JavaScript Time Limit", startup->js.time_limit);
		sprintf(opt[i++], "%-40s%u ticks", "JavaScript GC Interval ", startup->js.gc_interval);
		sprintf(opt[i++], "%-40s%u ticks", "JavaScript Yield Interval", startup->js.yield_interval);
		sprintf(opt[i++], "%-40s%s", "JavaScript Load Path", startup->js.load_path);
		sprintf(opt[i++], "%-40s%s", "Semaphore File Check Interval", duration(startup->sem_chk_freq));
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`Global Server Settings:`\n"
			"\n"
		;
		switch(uifc.list(WIN_ACT|WIN_CHE|WIN_RHT|WIN_SAV, 0, 0, 0, &cur, 0
			,"Global Server Setttings",opt)) {
			default:
				return;
		}
	}
}

static void termsrvr_cfg(BOOL* enabled, bbs_startup_t* startup)
{
	static int cur, bar;
	char tmp[256];

	while(1) {
		int i = 0;
		sprintf(opt[i++], "%-30s%s", "Enabled", *enabled ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Log Level", iniLogLevelStringList()[startup->log_level]);
		sprintf(opt[i++], "%-30s%u", "First Node", startup->first_node);
		sprintf(opt[i++], "%-30s%u", "Last Node", startup->last_node);
		sprintf(opt[i++], "%-30s%s", "DOS Program Support", startup->options & BBS_OPT_NO_DOS ? "No" : "Yes");
		sprintf(opt[i++], "%-30s%s", "SSH Support", startup->options & BBS_OPT_ALLOW_SSH ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "SSH Interfaces", startup->options & BBS_OPT_ALLOW_SSH ? strListCombine(startup->ssh_interfaces, tmp, sizeof(tmp), ", ") : "N/A");
		sprintf(opt[i++], "%-30s%u", "SSH Port", startup->ssh_port);
		sprintf(opt[i++], "%-30s%s", "SSH Connect Timeout", startup->options & BBS_OPT_ALLOW_SSH ? duration(startup->ssh_connect_timeout) : "N/A");
		sprintf(opt[i++], "%-30s%s", "Telnet Support", startup->options & BBS_OPT_NO_TELNET ? "No" : "Yes");
		sprintf(opt[i++], "%-30s%s", "Telnet Interfaces", startup->options & BBS_OPT_NO_TELNET ? "N/A" : strListCombine(startup->telnet_interfaces, tmp, sizeof(tmp), ", "));
		sprintf(opt[i++], "%-30s%u", "Telnet Port", startup->telnet_port);
		sprintf(opt[i++], "%-30s%s", "Telnet Command Debug", startup->options & BBS_OPT_NO_TELNET ? "N/A" : startup->options & BBS_OPT_DEBUG_TELNET ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Telnet Send Go-Aheads", startup->options & BBS_OPT_NO_TELNET ? "N/A" : startup->options & BBS_OPT_NO_TELNET_GA ? "No" : "Yes");
		sprintf(opt[i++], "%-30s%s", "RLogin Support", startup->options & BBS_OPT_ALLOW_RLOGIN ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "RLogin Interfaces", startup->options & BBS_OPT_ALLOW_RLOGIN ? strListCombine(startup->rlogin_interfaces, tmp, sizeof(tmp), ", ") : "N/A");
		sprintf(opt[i++], "%-30s%u", "RLogin Port", startup->rlogin_port);
		sprintf(opt[i++], "%-30s%u", "40 Column PETSCII Port", startup->pet40_port);
		sprintf(opt[i++], "%-30s%u", "80 Column PETSCII Port", startup->pet80_port);
		sprintf(opt[i++], "%-30s%s", "Max Concurrent Connections", maximum(startup->max_concurrent_connections));
		sprintf(opt[i++], "%-30s%s", "Max Login Inactivity", duration(startup->max_login_inactivity));
		sprintf(opt[i++], "%-30s%s", "Max New User Inactivity", duration(startup->max_newuser_inactivity));
		sprintf(opt[i++], "%-30s%s", "Max User Inactivity", duration(startup->max_session_inactivity));
		sprintf(opt[i++], "%-30s%u ms", "Output Buffer Drain Timeout", startup->outbuf_drain_timeout);
		sprintf(opt[i++], "%-30s%s", "Execute Timed Events", startup->options & BBS_OPT_NO_EVENTS ? "No" : "Yes");
		sprintf(opt[i++], "%-30s%s", "Execute QWK-relatd Events", startup->options & BBS_OPT_NO_EVENTS ? "N/A" : startup->options & BBS_OPT_NO_QWK_EVENTS ? "No" : "Yes");
		sprintf(opt[i++], "%-30s%s", "Lookup Client Hostname", startup->options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		if(!*enabled)
			i = 1;
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`Terminal Server Configuration:`\n"
			"\n"
		;
		switch(uifc.list(WIN_ACT|WIN_CHE|WIN_RHT|WIN_SAV, 0, 0, 0, &cur, &bar
			,"Terminal Server",opt)) {
			case 0:
				*enabled = !*enabled;
				break;
			case 1:
				startup->options ^= BBS_OPT_NO_TELNET;
				break;
			case 2:
				startup->options ^= BBS_OPT_ALLOW_SSH;
				break;
			case 3:
				startup->options ^= BBS_OPT_ALLOW_RLOGIN;
				break;
			default:
				return;
		}
	}
}

static void websrvr_cfg(BOOL* enabled, web_startup_t* startup)
{
	static int cur, bar;
	char tmp[256];

	while(1) {
		int i = 0;
		sprintf(opt[i++], "%-30s%s", "Enabled", *enabled ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Log Level", iniLogLevelStringList()[startup->log_level]);
		sprintf(opt[i++], "%-30s%s", "HTTP Interfaces", strListCombine(startup->interfaces, tmp, sizeof(tmp), ", "));
		sprintf(opt[i++], "%-30s%u", "HTTP Port", startup->port);
		sprintf(opt[i++], "%-30s%s", "HTTPS Support", startup->options & WEB_OPT_ALLOW_TLS ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "HTTPS Interfaces", startup->options & WEB_OPT_ALLOW_TLS ? strListCombine(startup->tls_interfaces, tmp, sizeof(tmp), ", ") : "N/A");
		sprintf(opt[i++], "%-30s%u", "HTTPS Port", startup->tls_port);
		sprintf(opt[i++], "%-30s%s", "SSJS File Extension", startup->ssjs_ext);
		sprintf(opt[i++], "%-30s%s", "Index Filenames", strListCombine(startup->index_file_name, tmp, sizeof(tmp), ", "));
		sprintf(opt[i++], "%-30s%s", "Content Root Directory", startup->root_dir);
		sprintf(opt[i++], "%-30s%s", "Error Sub-directory", startup->error_dir);
		sprintf(opt[i++], "%-30s%s", "Strict Transport Security", startup->options & WEB_OPT_HSTS_SAFE ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Virtual Host Support", startup->options & WEB_OPT_VIRTUAL_HOSTS ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Access Logging", startup->options & WEB_OPT_HTTP_LOGGING ? startup->logfile_base : strDisabled);
		sprintf(opt[i++], "%-30s%s", "Max Clients", maximum(startup->max_clients));
		sprintf(opt[i++], "%-30s%s", "Max Inactivity", duration(startup->max_inactivity));
		sprintf(opt[i++], "%-30s%s", "Filebase Index Script", startup->file_index_script);
		sprintf(opt[i++], "%-30s%s", "Filebase VPath Prefix", startup->file_vpath_prefix);
		sprintf(opt[i++], "%-30s%s", "Filebase VPath for VHosts", startup->file_vpath_for_vhosts ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Authentication Methods", startup->default_auth_list);
		sprintf(opt[i++], "%-30s%u ms", "Output Buffer Drain Timeout", startup->outbuf_drain_timeout);
		sprintf(opt[i++], "%-30s%s", "Lookup Client Hostname", startup->options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		bool cgi_enabled = !(startup->options & WEB_OPT_NO_CGI);
		sprintf(opt[i++], "%-30s%s", "CGI Support",  cgi_enabled ? "Yes" : "No");
		if(cgi_enabled) {
			sprintf(opt[i++], "%-30s%s", "CGI Directory", startup->cgi_dir);
			sprintf(opt[i++], "%-30s%s", "CGI File Extensions", strListCombine(startup->cgi_ext, tmp, sizeof(tmp), ", "));
			sprintf(opt[i++], "%-30s%s", "CGI Default Content-Type", startup->default_cgi_content);
			sprintf(opt[i++], "%-30s%s", "CGI Max Inactivity", duration(startup->max_cgi_inactivity));
		}
		if(!*enabled)
			i = 1;
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`Web Server Configuration:`\n"
			"\n"
		;
		switch(uifc.list(WIN_ACT|WIN_CHE|WIN_RHT|WIN_SAV, 0, 0, 0, &cur, &bar
			,"Web Server",opt)) {
			case 0:
				*enabled = !*enabled;
				break;
			case 2:
				startup->options ^= WEB_OPT_ALLOW_TLS;
				break;
			default:
				return;
		}
	}
}

static void ftpsrvr_cfg(BOOL* enabled, ftp_startup_t* startup)
{
	static int cur, bar;
	char tmp[256];

	while(1) {
		int i = 0;
		sprintf(opt[i++], "%-30s%s", "Enabled", *enabled ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Log Level", iniLogLevelStringList()[startup->log_level]);
		sprintf(opt[i++], "%-30s%s", "Network Interfaces", strListCombine(startup->interfaces, tmp, sizeof(tmp), ", "));
		sprintf(opt[i++], "%-30s%u, Data: %u", "Control Port", startup->port, startup->port - 1);
		sprintf(opt[i++], "%-30s%s", "Passive Interface (IPv4)", startup->options & FTP_OPT_LOOKUP_PASV_IP ? "<automatic>" : IPv4AddressToStr(startup->pasv_ip_addr.s_addr, tmp, sizeof(tmp)));
		sprintf(opt[i++], "%-30s%u - %u", "Passive Port Range", startup->pasv_port_low, startup->pasv_port_high);
		sprintf(opt[i++], "%-30s%s", "Auto-generate Index File", startup->options & FTP_OPT_INDEX_FILE ? startup->index_file_name : strDisabled);
		sprintf(opt[i++], "%-30s%s", "QWK Message Packet Transfers", startup->options & FTP_OPT_ALLOW_QWK ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "QWK Message Packet Timeout", startup->options & FTP_OPT_ALLOW_QWK ? duration(startup->qwk_timeout) : "N/A");
		sprintf(opt[i++], "%-30s%s", "Max Clients", maximum(startup->max_clients));
		sprintf(opt[i++], "%-30s%s", "Max Inactivity", duration(startup->max_inactivity));
		sprintf(opt[i++], "%-30s%s", "Max Concurrent Connections", maximum(startup->max_concurrent_connections));
		sprintf(opt[i++], "%-30s%s", "Sysop Filesystem Access", startup->options & FTP_OPT_NO_LOCAL_FSYS ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Allow Bounce Transfers", startup->options & FTP_OPT_ALLOW_BOUNCE ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Lookup Client Hostname", startup->options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		if(!*enabled)
			i = 1;
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`FTP Server Configuration:`\n"
			"\n"
		;
		switch(uifc.list(WIN_ACT|WIN_CHE|WIN_RHT|WIN_SAV, 0, 0, 0, &cur, &bar
			,"FTP Server",opt)) {
			case 0:
				*enabled = !*enabled;
				break;
			default:
				return;
		}
	}
}

static void mailsrvr_cfg(BOOL* enabled, mail_startup_t* startup)
{
	static int cur, bar;
	char tmp[256];
	const char* p;

	while(1) {
		int i = 0;
		sprintf(opt[i++], "%-30s%s", "Enabled", *enabled ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Log Level", iniLogLevelStringList()[startup->log_level]);
		sprintf(opt[i++], "%-30s%s", "SMTP Interfaces", strListCombine(startup->interfaces, tmp, sizeof(tmp), ", "));
		sprintf(opt[i++], "%-30s%u", "SMTP Port", startup->smtp_port);
		sprintf(opt[i++], "%-30s%s", "Submission Support", startup->options & MAIL_OPT_USE_SUBMISSION_PORT ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%u", "Submission Port", startup->submission_port);
		sprintf(opt[i++], "%-30s%s", "Submission/TLS Support", startup->options & MAIL_OPT_TLS_SUBMISSION ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%u", "Submission/TLS Port", startup->submissions_port);
		sprintf(opt[i++], "%-30s%s", "POP3 Support", startup->options & MAIL_OPT_ALLOW_POP3 ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "POP3 Interfaces"
			,startup->options & (MAIL_OPT_ALLOW_POP3 | MAIL_OPT_TLS_POP3)
				? strListCombine(startup->pop3_interfaces, tmp, sizeof(tmp), ", ") : "N/A");
		sprintf(opt[i++], "%-30s%u", "POP3 Port", startup->pop3_port);
		sprintf(opt[i++], "%-30s%s", "POP3/TLS Support", startup->options & MAIL_OPT_TLS_POP3 ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%u", "POP3/TLS Port", startup->pop3s_port);
		sprintf(opt[i++], "%-30s%s", "Max Clients", maximum(startup->max_clients));
		sprintf(opt[i++], "%-30s%s", "Max Inactivity", duration(startup->max_inactivity));
		sprintf(opt[i++], "%-30s%s", "Max Concurrent Connections", maximum(startup->max_concurrent_connections));
		sprintf(opt[i++], "%-30s%s", "Max Recipients Per Message", maximum(startup->max_recipients));
		sprintf(opt[i++], "%-30s%s", "Max Messages Waiting", maximum(startup->max_msgs_waiting));
		sprintf(opt[i++], "%-30s%s bytes", "Max Receive Message Size", byte_count_to_str(startup->max_msg_size, tmp, sizeof(tmp)));
		sprintf(opt[i++], "%-30s%s", "Allow Users to Relay Mail", startup->options & MAIL_OPT_ALLOW_RELAY ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Receive By Sysop Aliases", startup->options & MAIL_OPT_ALLOW_SYSOP_ALIASES ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Notify Local Recipients", startup->options & MAIL_OPT_NO_NOTIFY ? "No" : "Yes");
		sprintf(opt[i++], "%-30s%s", "Lookup Client Hostname", startup->options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		sprintf(opt[i++], "%-30s%s", "Check Headers against DNSBL", startup->options & MAIL_OPT_DNSBL_CHKRECVHDRS ? "Yes" : "No");
		if(startup->options & MAIL_OPT_DNSBL_REFUSE)
			p = "Refuse Session";
		else if(startup->options & MAIL_OPT_DNSBL_IGNORE)
			p = "Silently Ignore";
		else if(startup->options & MAIL_OPT_DNSBL_BADUSER)
			p = "Refuse Mail";
		else if(startup->options)
			p = "Tag Mail";
		sprintf(opt[i++], "%-30s%s%s", "Blacklisted Servers", startup->options & MAIL_OPT_DNSBL_THROTTLE ? "Throttle and " : "", p);
		sprintf(opt[i++], "%-30s%s", "Hash Blacklisted Messages", startup->options & MAIL_OPT_DNSBL_SPAMHASH ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Auto-exempt Recipients", startup->options & MAIL_OPT_NO_AUTO_EXEMPT ? "No" : "Yes");
		sprintf(opt[i++], "%-30s%s", "Kill SPAM When Read", startup->options & MAIL_OPT_KILL_READ_SPAM ? "Yes": "No");
		sprintf(opt[i++], "%-30s%s", "SendMail Support", startup->options & MAIL_OPT_NO_SENDMAIL ? "No" : "Yes");
		if(!(startup->options & MAIL_OPT_NO_SENDMAIL)) {
			bool applicable = startup->options & MAIL_OPT_RELAY_TX;
			sprintf(opt[i++], "%-30s%s", "SendMail Delivery", applicable ? "Relay" : "Direct");
			sprintf(opt[i++], "%-30s%s", "SendMail Relay Server", applicable ? startup->relay_server : "N/A");
			sprintf(opt[i++], "%-30s%u", "SendMail Relay Port", startup->relay_port);
			sprintf(opt[i++], "%-30s%s", "SendMail Relay User", applicable ? startup->relay_user : "N/A");
			sprintf(opt[i++], "%-30s%s", "SendMail Relay Password", applicable ? startup->relay_pass : "N/A");
			if(startup->options & MAIL_OPT_RELAY_AUTH_PLAIN)
				p = "Plain";
			else if(startup->options & MAIL_OPT_RELAY_AUTH_LOGIN)
				p = "Login";
			else if(startup->options & MAIL_OPT_RELAY_AUTH_CRAM_MD5)
				p = "CRAM-MD5";
			else
				p = "None";
			sprintf(opt[i++], "%-30s%s", "SendMail Relay Auth", applicable ? p : "N/A");
			sprintf(opt[i++], "%-30s%u", "SendMail Max Attempts", startup->max_delivery_attempts);
			sprintf(opt[i++], "%-30s%s", "SendMail Rescan Interval", duration(startup->rescan_frequency));
			sprintf(opt[i++], "%-30s%s", "SendMail Connect Timeout", duration(startup->connect_timeout));
		}
		if(!*enabled)
			i = 1;
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`Mail Server Configuration:`\n"
			"\n"
		;
		switch(uifc.list(WIN_ACT|WIN_CHE|WIN_RHT|WIN_SAV, 0, 0, 0, &cur, &bar
			,"Mail Server",opt)) {
			case 0:
				*enabled = !*enabled;
				break;
			default:
				return;
		}
	}
}

static void services_cfg(BOOL* enabled, services_startup_t* startup)
{
	static int cur, bar;
	char tmp[256];

	while(1) {
		int i = 0;
		sprintf(opt[i++], "%-30s%s", "Enabled", *enabled ? "Yes" : "No");
		sprintf(opt[i++], "%-30s%s", "Log Level", iniLogLevelStringList()[startup->log_level]);
		sprintf(opt[i++], "%-30s%s", "Network Interfaces", strListCombine(startup->interfaces, tmp, sizeof(tmp), ", "));
		sprintf(opt[i++], "%-30s%s", "Lookup Client Hostname", startup->options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		sprintf(opt[i++], "%-30s%s", "Configuration File", startup->services_ini);
		if(!*enabled)
			i = 1;
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`Services Server Configuration:`\n"
			"\n"
		;
		switch(uifc.list(WIN_ACT|WIN_CHE|WIN_RHT|WIN_SAV, 0, 0, 0, &cur, &bar
			,"Services Server",opt)) {
			case 0:
				*enabled = !*enabled;
				break;
			default:
				return;
		}
	}
}

void server_cfg(void)
{
	static int srvr_dflt;

	BOOL run_bbs, run_ftp, run_web, run_mail, run_services;
	global_startup_t global_startup = {0};
	bbs_startup_t bbs_startup = {0};
	ftp_startup_t ftp_startup = {0};
	web_startup_t web_startup = {0};
	mail_startup_t mail_startup = {0};
	services_startup_t services_startup = {0};

	char ini_fname[MAX_PATH + 1];
	sbbs_get_ini_fname(ini_fname, cfg.ctrl_dir);
	FILE* fp = fopen(ini_fname, "r");
	if(fp == NULL) {
		uifc.msgf("Error opening %s", ini_fname);
		return;
	}
	display_filename(ini_fname);
	sbbs_read_ini(
		 fp
		,ini_fname
		,&global_startup
		,&run_bbs
		,&bbs_startup
		,&run_ftp
		,&ftp_startup
		,&run_web
		,&web_startup
		,&run_mail
		,&mail_startup
		,&run_services
		,&services_startup
		);
	fclose(fp);

	while(1) {
		int i = 0;
		strcpy(opt[i++], "Global Settings");
		sprintf(opt[i++], "%-27s%s", "Terminal Server", run_bbs ? "Enabled" : strDisabled);
		sprintf(opt[i++], "%-27s%s", "Web Server", run_web ? "Enabled" : strDisabled);
		sprintf(opt[i++], "%-27s%s", "FTP Server", run_ftp ? "Enabled" : strDisabled);
		sprintf(opt[i++], "%-27s%s", "Mail Server", run_mail ? "Enabled" : strDisabled);
		sprintf(opt[i++], "%-27s%s", "Services Server", run_services ? "Enabled" : strDisabled);
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`Server Configuration:`\n"
			"\n"
			"Here you can configure initialization settings of the various TCP/IP\n"
			"servers that are integrated into Synchronet BBS Software.\n"
			"\n"
			"For additinal advanced Synchronet server initialization settings, see\n"
			"the `ctrl/sbbs.ini` file and `https://wiki.synchro.net/config:sbbs.ini`\n"
			"for reference.\n"
		;
		switch(uifc.list(WIN_ORG|WIN_ACT|WIN_CHE,0,0,0,&srvr_dflt,0
			,"Server Configuration",opt)) {
			case 0:
				global_cfg(&global_startup);
				break;
			case 1:
				termsrvr_cfg(&run_bbs, &bbs_startup);
				break;
			case 2:
				websrvr_cfg(&run_web, &web_startup);
				break;
			case 3:
				ftpsrvr_cfg(&run_ftp, &ftp_startup);
				break;
			case 4:
				mailsrvr_cfg(&run_mail, &mail_startup);
				break;
			case 5:
				services_cfg(&run_services, &services_startup);
				break;
			default:
				return;
		}
	}
}
