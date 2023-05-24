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

const char* strReadingIniFile = "Reading sbbs.ini ...";
const char* strDisabled = "<disabled>";

static const char* threshold(uint val)
{
	static char str[128];
	if(val == 0)
		return strDisabled;
	SAFEPRINTF(str, "%u", val);
	return str;
}

static const char* duration(uint val, bool verbose)
{
	static char str[128];
	if(val == 0)
		return strDisabled;
	return verbose ? duration_to_vstr(val, str, sizeof(str)) : duration_to_str(val, str, sizeof(str));;
}

static const char* vduration(uint val)
{
	return duration(val, true);
}

static const char* maximum(uint val)
{
	static char str[128];
	if(val == 0)
		return "Unlimited";
	SAFEPRINTF(str, "%u", val);
	return str;
}

static void login_attempt_cfg(struct login_attempt_settings* login_attempt)
{
	static int cur, bar;
	char str[256];
	BOOL changes = uifc.changes;

	while(1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%u ms", "Delay", login_attempt->delay);
		snprintf(opt[i++], MAX_OPLN, "%-30s%u ms", "Throttle", login_attempt->throttle);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Hack Log Threshold", threshold(login_attempt->hack_threshold));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Temporary Ban Threshold", threshold(login_attempt->tempban_threshold));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Temporary Ban Duration"
			,duration_to_vstr(login_attempt->tempban_duration, tmp, sizeof(tmp)));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Auto-filter Threshold", threshold(login_attempt->filter_threshold));
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`Failed Login Attempt Settings:`\n"
			"\n"
			"Settings that control the throttling, logging, and subsequent filtering\n"
			"of clients (based on IP address) that have failed login attempts.\n"
		;
		switch(uifc.list(WIN_ACT|WIN_BOT|WIN_SAV, 0, 0, 0, &cur, &bar
			,"Failed Login Attempts",opt)) {
			case 0:
				SAFEPRINTF(str, "%u", login_attempt->delay);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Millisecond Delay After Each Failed Login Attempt", str, 6, K_NUMBER|K_EDIT) > 0)
					login_attempt->delay = atoi(str);
				break;
			case 1:
				SAFEPRINTF(str, "%u", login_attempt->throttle);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Throttle multiplier (in milliseconds) for Failed Logins", str, 6, K_NUMBER|K_EDIT) > 0)
					login_attempt->throttle = atoi(str);
				break;
			case 2:
				SAFEPRINTF(str, "%u", login_attempt->hack_threshold);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Threshold for Logging Failed Logins to hack.log", str, 4, K_NUMBER|K_EDIT) > 0)
					login_attempt->hack_threshold = atoi(str);
				break;
			case 3:
				SAFEPRINTF(str, "%u", login_attempt->tempban_threshold);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Threshold for Temp-ban IPs of Failed Logins", str, 4, K_NUMBER|K_EDIT) > 0)
					login_attempt->tempban_threshold = atoi(str);
				break;
			case 4:
				SAFECOPY(str, duration(login_attempt->tempban_duration, false));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Duration of Temp-ban for Failed Logins", str, 6, K_EDIT) > 0)
					login_attempt->tempban_duration = (uint)parse_duration(str);
				break;
			case 5:
				SAFEPRINTF(str, "%u", login_attempt->filter_threshold);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Threshold for Filtering IPs of Failed Logins", str, 3, K_NUMBER|K_EDIT) > 0)
					login_attempt->filter_threshold = atoi(str);
				break;
			default:
				uifc.changes = changes;
				return;
		}
	}
}

static void js_startup_cfg(js_startup_t* js)
{
	static int cur, bar;
	char str[256];
	BOOL changes = uifc.changes;

	while(1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-20s%s bytes", "Heap Size", byte_count_to_str(js->max_bytes, str, sizeof(str)));
		snprintf(opt[i++], MAX_OPLN, "%-20s%u ticks", "Time Limit", js->time_limit);
		snprintf(opt[i++], MAX_OPLN, "%-20s%u ticks", "GC Interval ", js->gc_interval);
		snprintf(opt[i++], MAX_OPLN, "%-20s%u ticks", "Yield Interval", js->yield_interval);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Load Path", js->load_path);
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`JavaScript Server Settings:`\n"
			"\n"
			"Settings that control the server-side JavaScript execution environment.\n"
		;
		switch(uifc.list(WIN_ACT|WIN_BOT|WIN_SAV, 0, 0, 0, &cur, &bar
			,"JavaScript Settings",opt)) {
			case 0:
				byte_count_to_str(js->max_bytes, str, sizeof(str));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "JavaScript Heap Size (Maximum Allocated Bytes)", str, 6, K_UPPER|K_EDIT) > 0)
					js->max_bytes = (uint)parse_byte_count(str, 1);
				break;
			case 1:
				SAFEPRINTF(str, "%u", js->time_limit);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "JavaScript Execution Time Limit (in ticks)", str, 6, K_NUMBER|K_EDIT) > 0)
					js->time_limit = atoi(str);
				break;
			case 2:
				SAFEPRINTF(str, "%u", js->gc_interval);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "JavaScript Garbage Collection Interval (in ticks)", str, 6, K_NUMBER|K_EDIT) > 0)
					js->gc_interval = atoi(str);
				break;
			case 3:
				SAFEPRINTF(str, "%u", js->yield_interval);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "JavaScript Yield Interval (in ticks)", str, 6, K_NUMBER|K_EDIT) > 0)
					js->yield_interval = atoi(str);
				break;
			case 4:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "JavaScript Load Library Path", js->load_path, sizeof(js->load_path) - 1, K_EDIT);
				break;
			default:
				uifc.changes = changes;
				return;
		}
	}
}

static void global_cfg(void)
{
	static int cur, bar;
	char str[256];
	char tmp[256];
	global_startup_t startup = {0};

	FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */false);
	if(fp == NULL) {
		uifc.msgf("Error opening %s", cfg.filename);
		return;
	}
	uifc.pop(strReadingIniFile);
	sbbs_read_ini(
		 fp
		,cfg.filename
		,&startup
		,NULL
		,NULL //&bbs_startup
		,NULL
		,NULL //&ftp_startup
		,NULL
		,NULL //&web_startup
		,NULL
		,NULL //&mail_startup
		,NULL
		,NULL //&services_startup
		);
	iniCloseFile(fp);
	uifc.pop(NULL);
	global_startup_t saved_startup = startup;

	while(1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-40s%s", "Log Level", iniLogLevelStringList()[startup.log_level]);
		snprintf(opt[i++], MAX_OPLN, "%-40s%s", "TLS Error Level", iniLogLevelStringList()[startup.tls_error_level]);
		snprintf(opt[i++], MAX_OPLN, "%-40s%s", "Network Interfaces (IPv4/6)", strListCombine(startup.interfaces, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-40s%s", "Outbound Interface (IPv4)", IPv4AddressToStr(startup.outgoing4.s_addr, tmp, sizeof(tmp)));
		snprintf(opt[i++], MAX_OPLN, "%-40s%s", "Bind Retry Count", threshold(startup.bind_retry_count));
		snprintf(opt[i++], MAX_OPLN, "%-40s%s", "Bind Retry Delay", vduration(startup.bind_retry_delay));
		snprintf(opt[i++], MAX_OPLN, "%-40s%s", "Semaphore File Check Interval", vduration(startup.sem_chk_freq));
		strcpy(opt[i++], "JavaScript Settings...");
		strcpy(opt[i++], "Failed Login Attempts...");
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`Global Server Settings:`\n"
			"\n"
			"Settings that are shared among multiple Synchronet servers.\n"
			"\n"
			"These settings can be over-ridden on a per-server basis by editing the\n"
			"corresponding keys in each `[server]` section of the `ctrl/sbbs.ini` file.\n"
		;
		switch(uifc.list(WIN_ACT|WIN_RHT|WIN_SAV|WIN_ESC, 0, 0, 0, &cur, &bar
			,"Global Server Settings",opt)) {
			case 0:
				uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, &startup.log_level, 0, "Log Level", iniLogLevelStringList());
				break;
			case 1:
				uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, &startup.tls_error_level, 0, "TLS Error Log Level", iniLogLevelStringList());
				break;
			case 2:
				strListCombine(startup.interfaces, str, sizeof(str), ", ");
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Inbound Network Interfaces (IPv4/6)", str, sizeof(str)-1, K_EDIT) >= 0) {
					strListFree(&startup.interfaces);
					strListSplitCopy(&startup.interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 3:
				IPv4AddressToStr(startup.outgoing4.s_addr, str, sizeof(str));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Outbound Network Interface (IPv4)", str, sizeof(str)-1, K_EDIT) > 0)
					startup.outgoing4.s_addr = parseIPv4Address(str);
				break;
			case 4:
				SAFECOPY(str, threshold(startup.bind_retry_count));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Port Bind Retry Count", str, 6, K_EDIT) > 0)
					startup.bind_retry_count = atoi(str);
				break;
			case 5:
				SAFECOPY(str, duration(startup.bind_retry_delay, false));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Port Bind Retry Delay", str, 6, K_EDIT) > 0)
					startup.bind_retry_delay = (uint)parse_duration(str);
				break;
			case 6:
				SAFECOPY(str, duration(startup.sem_chk_freq, false));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Semaphore File Check Interval", str, 6, K_EDIT) > 0)
					startup.sem_chk_freq = (uint16_t)parse_duration(str);
				break;
			case 7:
				js_startup_cfg(&startup.js);
				break;
			case 8:
				login_attempt_cfg(&startup.login_attempt);
				break;
			default:
				if(memcmp(&saved_startup, &startup, sizeof(startup)) != 0)
					uifc.changes = true;
				i = save_changes(WIN_MID);
				if(i < 0)
					continue;
				if(i == 0) {
					FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */true);
					if(fp == NULL)
						uifc.msgf("Error opening %s", cfg.filename);
					else {
						if(!sbbs_write_ini(
							 fp
							,&cfg
							,&startup
							,false
							,NULL
							,false
							,NULL
							,false
							,NULL
							,false
							,NULL
							,false
							,NULL
							))
							uifc.msgf("Error writing %s", cfg.filename);
						iniCloseFile(fp);
					}
				}
				sbbs_free_ini(&startup
					,NULL //&bbs_startup
					,NULL //&ftp_startup
					,NULL //&web_startup
					,NULL //&mail_startup
					,NULL //&services_startup
					);
				return;
		}
	}
}

static void termsrvr_cfg(void)
{
	static int cur, bar;
	char str[256];
	char tmp[256];
	BOOL enabled = FALSE;
	bbs_startup_t startup = {0};

	FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */false);
	if(fp == NULL) {
		uifc.msgf("Error opening %s", cfg.filename);
		return;
	}
	uifc.pop(strReadingIniFile);
	sbbs_read_ini(
		 fp
		,cfg.filename
		,NULL
		,&enabled
		,&startup
		,NULL
		,NULL //&ftp_startup
		,NULL
		,NULL //&web_startup
		,NULL
		,NULL //&mail_startup
		,NULL
		,NULL //&services_startup
		);
	iniCloseFile(fp);
	uifc.pop(NULL);
	bbs_startup_t saved_startup = startup;

	while(1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", enabled ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Log Level", iniLogLevelStringList()[startup.log_level]);
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "First Node", startup.first_node);
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "Last Node", startup.last_node);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "DOS Program Support", startup.options & BBS_OPT_NO_DOS ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "SSH Support", startup.options & BBS_OPT_ALLOW_SSH ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "SSH Interfaces"
			,startup.options & BBS_OPT_ALLOW_SSH ? strListCombine(startup.ssh_interfaces, tmp, sizeof(tmp), ", ") : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "SSH Port", startup.ssh_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "SSH Connect Timeout"
			,startup.options & BBS_OPT_ALLOW_SSH ? vduration(startup.ssh_connect_timeout) : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Telnet Support", startup.options & BBS_OPT_NO_TELNET ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Telnet Interfaces"
			,startup.options & BBS_OPT_NO_TELNET ? "N/A" : strListCombine(startup.telnet_interfaces, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "Telnet Port", startup.telnet_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Telnet Command Debug"
			,startup.options & BBS_OPT_NO_TELNET ? "N/A" : startup.options & BBS_OPT_DEBUG_TELNET ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Telnet Send Go-Aheads"
			,startup.options & BBS_OPT_NO_TELNET ? "N/A" : startup.options & BBS_OPT_NO_TELNET_GA ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "RLogin Support", startup.options & BBS_OPT_ALLOW_RLOGIN ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "RLogin Interfaces"
			,startup.options & BBS_OPT_ALLOW_RLOGIN ? strListCombine(startup.rlogin_interfaces, tmp, sizeof(tmp), ", ") : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "RLogin Port", startup.rlogin_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "40 Column PETSCII Port", startup.pet40_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "80 Column PETSCII Port", startup.pet80_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Concurrent Connections", maximum(startup.max_concurrent_connections));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Login Inactivity", vduration(startup.max_login_inactivity));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max New User Inactivity", vduration(startup.max_newuser_inactivity));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max User Inactivity", vduration(startup.max_session_inactivity));
		snprintf(opt[i++], MAX_OPLN, "%-30s%u ms", "Output Buffer Drain Timeout", startup.outbuf_drain_timeout);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Execute Timed Events", startup.options & BBS_OPT_NO_EVENTS ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Execute QWK-related Events"
			,startup.options & BBS_OPT_NO_EVENTS ? "N/A" : startup.options & BBS_OPT_NO_QWK_EVENTS ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Lookup Client Hostname", startup.options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		if(!enabled)
			i = 1;
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`Terminal Server Configuration:`\n"
			"\n"
			"The initialization settings of the Synchronet server that provides the\n"
			"traditional BBS experience over `Telnet`, `SSH`, `RLogin`, or `Raw TCP`\n"
			"protocols.\n"
			"\n"
			"For full documentation, see `http://wiki.synchro.net/server:terminal`\n"
		;
		switch(uifc.list(WIN_ACT|WIN_ESC|WIN_RHT|WIN_SAV, 0, 0, 0, &cur, &bar
			,"Terminal Server",opt)) {
			case 0:
				enabled = !enabled;
				uifc.changes = true;
				break;
			case 1:
				uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, &startup.log_level, 0, "Log Level", iniLogLevelStringList());
				break;
			case 2:
				SAFEPRINTF(str, "%u", startup.first_node);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "First Node Available For Terminal Logins", str, 3, K_NUMBER|K_EDIT) > 0)
					startup.first_node = atoi(str);
				break;
			case 3:
				SAFEPRINTF(str, "%u", startup.last_node);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Last Node Available For Terminal Logins", str, 3, K_NUMBER|K_EDIT) > 0)
					startup.last_node = atoi(str);
				break;
			case 4:
				startup.options ^= BBS_OPT_NO_DOS;
				break;
			case 5:
				startup.options ^= BBS_OPT_ALLOW_SSH;
				break;
			case 6:
				strListCombine(startup.ssh_interfaces, str, sizeof(str), ", ");
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "SSH Network Interfaces (IPv4/6)", str, sizeof(str)-1, K_EDIT) >= 0) {
					strListFree(&startup.ssh_interfaces);
					strListSplitCopy(&startup.ssh_interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 7:
				SAFEPRINTF(str, "%u", startup.ssh_port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "SSH TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.ssh_port = atoi(str);
				break;
			case 8:
				SAFECOPY(str, duration(startup.ssh_connect_timeout, false));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "SSH Connect Timeout", str, 6, K_EDIT) > 0)
					startup.ssh_connect_timeout = (uint16_t)parse_duration(str);
				break;
			case 9:
				startup.options ^= BBS_OPT_NO_TELNET;
				break;
			case 10:
				strListCombine(startup.telnet_interfaces, str, sizeof(str), ", ");
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Telnet Network Interfaces (IPv4/6)", str, sizeof(str)-1, K_EDIT) >= 0) {
					strListFree(&startup.telnet_interfaces);
					strListSplitCopy(&startup.telnet_interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 11:
				SAFEPRINTF(str, "%u", startup.telnet_port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Telnet TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.telnet_port = atoi(str);
				break;
			case 12:
				startup.options ^= BBS_OPT_DEBUG_TELNET;
				break;
			case 13:
				startup.options ^= BBS_OPT_NO_TELNET_GA;
				break;
			case 14:
				startup.options ^= BBS_OPT_ALLOW_RLOGIN;
				break;
			case 15:
				strListCombine(startup.rlogin_interfaces, str, sizeof(str), ", ");
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "RLogin Network Interfaces (IPv4/6)", str, sizeof(str)-1, K_EDIT) >= 0) {
					strListFree(&startup.rlogin_interfaces);
					strListSplitCopy(&startup.rlogin_interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 16:
				SAFEPRINTF(str, "%u", startup.rlogin_port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "RLogin TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.rlogin_port = atoi(str);
				break;
			case 17:
				SAFEPRINTF(str, "%u", startup.pet40_port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "40 Column CBM/PETSCII TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.pet40_port = atoi(str);
				break;
			case 18:
				SAFEPRINTF(str, "%u", startup.pet80_port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "80 Column CBM/PETSCII TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.pet80_port = atoi(str);
				break;
			case 19:
				SAFECOPY(str, maximum(startup.max_concurrent_connections));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Concurrent (Unauthenticated) Connections", str, 10, K_EDIT) > 0)
					startup.max_concurrent_connections = atoi(str);
				break;
			case 20:
				SAFECOPY(str, duration(startup.max_login_inactivity, false));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Socket Inactivity at Login", str, 10, K_EDIT) > 0)
					startup.max_login_inactivity = (uint16_t)parse_duration(str);
				break;
			case 21:
				SAFECOPY(str, duration(startup.max_newuser_inactivity, false));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Socket Inactivity at New User Registration", str, 10, K_EDIT) > 0)
					startup.max_newuser_inactivity = (uint16_t)parse_duration(str);
				break;
			case 22:
				SAFECOPY(str, duration(startup.max_session_inactivity, false));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Socket Inactivity during User Session", str, 10, K_EDIT) > 0)
					startup.max_session_inactivity = (uint16_t)parse_duration(str);
				break;
			case 23:
				SAFEPRINTF(str, "%u", startup.outbuf_drain_timeout);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Output Buffer Drain Timeout (milliseconds)", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.outbuf_drain_timeout = atoi(str);
				break;
			case 24:
				startup.options ^= BBS_OPT_NO_EVENTS;
				break;
			case 25:
				startup.options ^= BBS_OPT_NO_QWK_EVENTS;
				break;
			case 26:
				startup.options ^= BBS_OPT_NO_HOST_LOOKUP;
				break;
			default:
				if(memcmp(&saved_startup, &startup, sizeof(startup)) != 0)
					uifc.changes = true;
				i = save_changes(WIN_MID);
				if(i < 0)
					continue;
				if(i == 0) {
					FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */true);
					if(fp == NULL)
						uifc.msgf("Error opening %s", cfg.filename);
					else {
						if(!sbbs_write_ini(
							 fp
							,&cfg
							,NULL
							,enabled
							,&startup
							,false
							,NULL
							,false
							,NULL
							,false
							,NULL
							,false
							,NULL
							))
							uifc.msgf("Error writing %s", cfg.filename);
						iniCloseFile(fp);
					}
				}
				sbbs_free_ini(NULL
					,&startup
					,NULL //&ftp_startup
					,NULL //&web_startup
					,NULL //&mail_startup
					,NULL //&services_startup
					);
				return;
		}
	}
}

static void websrvr_cfg(void)
{
	static int cur, bar;
	char tmp[256];
	char str[256];
	BOOL enabled = FALSE;
	web_startup_t startup = {0};

	FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */false);
	if(fp == NULL) {
		uifc.msgf("Error opening %s", cfg.filename);
		return;
	}
	uifc.pop(strReadingIniFile);
	sbbs_read_ini(
		 fp
		,cfg.filename
		,NULL
		,NULL
		,NULL
		,NULL
		,NULL //&ftp_startup
		,&enabled
		,&startup
		,NULL
		,NULL //&mail_startup
		,NULL
		,NULL //&services_startup
		);
	iniCloseFile(fp);
	uifc.pop(NULL);
	web_startup_t saved_startup = startup;

	while(1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", enabled ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Log Level", iniLogLevelStringList()[startup.log_level]);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "HTTP Interfaces", strListCombine(startup.interfaces, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "HTTP Port", startup.port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "HTTPS Support", startup.options & WEB_OPT_ALLOW_TLS ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "HTTPS Interfaces"
			,startup.options & WEB_OPT_ALLOW_TLS ? strListCombine(startup.tls_interfaces, tmp, sizeof(tmp), ", ") : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "HTTPS Port", startup.tls_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "SSJS File Extension", startup.ssjs_ext);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Index Filenames", strListCombine(startup.index_file_name, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Content Root Directory", startup.root_dir);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Error Sub-directory", startup.error_dir);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Strict Transport Security", startup.options & WEB_OPT_HSTS_SAFE ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Virtual Host Support", startup.options & WEB_OPT_VIRTUAL_HOSTS ? "Yes" : "No");
		SAFECOPY(str, startup.logfile_base);
		if(*str == '\0')
			SAFEPRINTF(str, "[%slogs/http-*]", cfg.logs_dir);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Access Logging", startup.options & WEB_OPT_HTTP_LOGGING ? str : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Clients", maximum(startup.max_clients));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Inactivity", vduration(startup.max_inactivity));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Filebase Index Script", startup.file_index_script);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Filebase VPath Prefix", startup.file_vpath_prefix);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Filebase VPath for VHosts", startup.file_vpath_for_vhosts ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Authentication Methods", startup.default_auth_list);
		snprintf(opt[i++], MAX_OPLN, "%-30s%u ms", "Output Buffer Drain Timeout", startup.outbuf_drain_timeout);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Lookup Client Hostname", startup.options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		bool cgi_enabled = !(startup.options & WEB_OPT_NO_CGI);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "CGI Support",  cgi_enabled ? "Yes" : "No");
		if(cgi_enabled) {
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "CGI Directory", startup.cgi_dir);
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "CGI File Extensions", strListCombine(startup.cgi_ext, tmp, sizeof(tmp), ", "));
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "CGI Default Content-Type", startup.default_cgi_content);
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "CGI Max Inactivity", vduration(startup.max_cgi_inactivity));
		}
		if(!enabled)
			i = 1;
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`Web Server Configuration:`\n"
			"\n"
			"The initialization settings of the Synchronet Web Server that provides\n"
			"support for web browser access to the BBS over `HTTP` and `HTTPS` (TLS)\n"
			"protocols.\n"
			"\n"
			"Static content (e.g. HTML and graphics files) can be served up as well\n"
			"as dynamic content via Server-side JavaScript (SSJS) and external CGI\n"
			"processes (traditional and fast-CGI).\n"
			"\n"
			"Advanced features such as Virtual Hosts, Filebase access, and Strict\n"
			"Transport Security are also supported.\n"
			"\n"
			"For full documentation, see `http://wiki.synchro.net/server:web`\n"
		;
		switch(uifc.list(WIN_ACT|WIN_ESC|WIN_RHT|WIN_SAV, 0, 0, 0, &cur, &bar
			,"Web Server",opt)) {
			case 0:
				enabled = !enabled;
				uifc.changes = true;
				break;
			case 1:
				uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, &startup.log_level, 0, "Log Level", iniLogLevelStringList());
				break;
			case 2:
				strListCombine(startup.interfaces, str, sizeof(str), ", ");
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "HTTP Network Interfaces (IPv4/6)", str, sizeof(str)-1, K_EDIT) >= 0) {
					strListFree(&startup.interfaces);
					strListSplitCopy(&startup.interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 3:
				SAFEPRINTF(str, "%u", startup.port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "HTTP TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.port = atoi(str);
				break;
			case 4:
				startup.options ^= WEB_OPT_ALLOW_TLS;
				break;
			case 5:
				strListCombine(startup.tls_interfaces, str, sizeof(str), ", ");
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "HTTP/TLS (HTTPS) Network Interfaces (IPv4/6)", str, sizeof(str)-1, K_EDIT) >= 0) {
					strListFree(&startup.tls_interfaces);
					strListSplitCopy(&startup.tls_interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 6:
				SAFEPRINTF(str, "%u", startup.tls_port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "HTTP/TLS (HTTPS) TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.tls_port = atoi(str);
				break;
			case 7:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "Server-side JavaScript File Extension"
					, startup.ssjs_ext, sizeof(startup.ssjs_ext) -1, K_EDIT);
				break;
			case 8:
				strListCombine(startup.index_file_name, str, sizeof(str), ", ");
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Index Filenames", str, sizeof(str)-1, K_EDIT) >= 0) {
					strListFree(&startup.index_file_name);
					strListSplitCopy(&startup.index_file_name, str, ", ");
					uifc.changes = true;
				}
				break;
			case 9:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "Content Root Directory"
					,startup.root_dir, sizeof(startup.root_dir)-1, K_EDIT);
				break;
			case 10:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "Error Sub-directory"
					,startup.error_dir, sizeof(startup.error_dir)-1, K_EDIT);
				break;
			case 11:
				startup.options ^= WEB_OPT_HSTS_SAFE;
				break;
			case 12:
				startup.options ^= WEB_OPT_VIRTUAL_HOSTS;
				break;
			case 13:
				i = startup.options & WEB_OPT_HTTP_LOGGING ? 0 : 1;
				i = uifc.list(WIN_SAV|WIN_MID, 0, 0, 0, &i, 0, "Log Requests to Files in Combined Log Format", uifcYesNoOpts);
				if(i == 0) {
					startup.options |= WEB_OPT_HTTP_LOGGING;
					uifc.input(WIN_MID|WIN_SAV, 0, 0, "Base path/filename (blank = default)"
						,startup.logfile_base, sizeof(startup.logfile_base)-1, K_EDIT);
				} else if(i == 1)
					startup.options &= ~WEB_OPT_HTTP_LOGGING;
				break;
			case 14:
				SAFECOPY(str, maximum(startup.max_clients));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Client Count (0=Unlimited)", str, 10, K_EDIT) > 0)
					startup.max_clients = atoi(str);
				break;
			case 15:
				SAFECOPY(str, duration(startup.max_inactivity, false));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Client Inactivity", str, 10, K_EDIT) > 0)
					startup.max_inactivity = (uint16_t)parse_duration(str);
				break;
			case 16:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "Filebase Index Script"
					,startup.file_index_script, sizeof(startup.file_index_script)-1, K_EDIT);
				break;
			case 17:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "Filebase Virtual Path Prefix"
					,startup.file_vpath_prefix, sizeof(startup.file_vpath_prefix)-1, K_EDIT);
				break;
			case 18:
				startup.file_vpath_for_vhosts = !startup.file_vpath_for_vhosts;
				break;
			case 19:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "Authentication Methods"
					,startup.default_auth_list, sizeof(startup.default_auth_list)-1, K_EDIT);
				break;
			case 20:
				SAFEPRINTF(str, "%u", startup.outbuf_drain_timeout);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Output Buffer Drain Timeout (milliseconds)"
					,str, 5, K_NUMBER|K_EDIT) > 0)
					startup.outbuf_drain_timeout = atoi(str);
				break;
			case 21:
				startup.options ^= BBS_OPT_NO_HOST_LOOKUP;
				break;
			case 22:
				startup.options ^= WEB_OPT_NO_CGI;
				break;
			case 23:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "CGI Directory"
					,startup.cgi_dir, sizeof(startup.cgi_dir)-1, K_EDIT);
				break;
			case 24:
				strListCombine(startup.cgi_ext, str, sizeof(str), ", ");
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "CGI File Extensions", str, sizeof(str)-1, K_EDIT) >= 0) {
					strListFree(&startup.cgi_ext);
					strListSplitCopy(&startup.cgi_ext, str, ", ");
					uifc.changes = true;
				}
				break;
			case 25:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "Default CGI MIME Content-Type"
					,startup.default_cgi_content, sizeof(startup.default_cgi_content)-1, K_EDIT);
				break;
			case 26:
				duration_to_str(startup.max_cgi_inactivity, str, sizeof(str));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum CGI Inactivity", str, 10, K_EDIT) > 0)
					startup.max_cgi_inactivity = (uint16_t)parse_duration(str);
				break;
			default:
				if(memcmp(&saved_startup, &startup, sizeof(startup)) != 0)
					uifc.changes = true;
				i = save_changes(WIN_MID);
				if(i < 0)
					continue;
				if(i == 0) {
					FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */true);
					if(fp == NULL)
						uifc.msgf("Error opening %s", cfg.filename);
					else {
						if(!sbbs_write_ini(
							 fp
							,&cfg
							,NULL
							,FALSE
							,NULL
							,false
							,NULL
							,enabled
							,&startup
							,false
							,NULL
							,false
							,NULL
							))
							uifc.msgf("Error writing %s", cfg.filename);
						iniCloseFile(fp);
					}
				}
				sbbs_free_ini(NULL
					,NULL
					,NULL //&ftp_startup
					,&startup
					,NULL //&mail_startup
					,NULL //&services_startup
					);
				return;
		}
	}
}

static void ftpsrvr_cfg(void)
{
	static int cur, bar;
	char tmp[256];
	char str[256];
	BOOL enabled = FALSE;
	ftp_startup_t startup = {0};

	FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */false);
	if(fp == NULL) {
		uifc.msgf("Error opening %s", cfg.filename);
		return;
	}
	uifc.pop(strReadingIniFile);
	sbbs_read_ini(
		 fp
		,cfg.filename
		,NULL
		,NULL
		,NULL
		,&enabled
		,&startup
		,NULL
		,NULL
		,NULL
		,NULL //&mail_startup
		,NULL
		,NULL //&services_startup
		);
	iniCloseFile(fp);
	uifc.pop(NULL);
	ftp_startup_t saved_startup = startup;

	while(1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", enabled ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Log Level", iniLogLevelStringList()[startup.log_level]);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Network Interfaces", strListCombine(startup.interfaces, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-30s%u, Data: %u", "Control Port", startup.port, startup.port - 1);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Passive Interface (IPv4)"
			,startup.options & FTP_OPT_LOOKUP_PASV_IP ? "<automatic>" : IPv4AddressToStr(startup.pasv_ip_addr.s_addr, tmp, sizeof(tmp)));
		snprintf(opt[i++], MAX_OPLN, "%-30s%u - %u", "Passive Port Range", startup.pasv_port_low, startup.pasv_port_high);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Auto-generate Index File", startup.options & FTP_OPT_INDEX_FILE ? startup.index_file_name : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "QWK Message Packet Transfers", startup.options & FTP_OPT_ALLOW_QWK ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "QWK Message Packet Timeout", startup.options & FTP_OPT_ALLOW_QWK ? vduration(startup.qwk_timeout) : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Clients", maximum(startup.max_clients));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Inactivity", vduration(startup.max_inactivity));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Concurrent Connections", maximum(startup.max_concurrent_connections));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Sysop File System Access", startup.options & FTP_OPT_NO_LOCAL_FSYS ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Allow Bounce Transfers", startup.options & FTP_OPT_ALLOW_BOUNCE ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Lookup Client Hostname", startup.options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		if(!enabled)
			i = 1;
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`FTP Server Configuration:`\n"
			"\n"
			"These settings define the initialization parameters of the Synchronet\n"
			"FTP Server.  FTP and FTPS (TLS) protocols are supported for downloads\n"
			"and uploads (including resume) to/from filebases and local filesystem.\n"
			"\n"
			"Active and passive FTP transfers are supported along with QWK message\n"
			"packet transfers, dynamically-generated file indexes, and other advanced\n"
			"features.\n"
			"\n"
			"For full documentation, see `http://wiki.synchro.net/server:ftp`\n"
		;
		switch(uifc.list(WIN_ACT|WIN_ESC|WIN_RHT|WIN_SAV, 0, 0, 0, &cur, &bar
			,"FTP Server",opt)) {
			case 0:
				enabled = !enabled;
				uifc.changes = true;
				break;
			case 1:
				uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, &startup.log_level, 0, "Log Level", iniLogLevelStringList());
				break;
			case 2:
				strListCombine(startup.interfaces, str, sizeof(str), ", ");
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Network Interfaces (IPv4/6)", str, sizeof(str)-1, K_EDIT) >= 0) {
					strListFree(&startup.interfaces);
					strListSplitCopy(&startup.interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 3:
				SAFEPRINTF(str, "%u", startup.port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Control TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.port = atoi(str);
				break;
			case 4:
				i = startup.options & FTP_OPT_LOOKUP_PASV_IP ? 0 : 1;
				i = uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, &i, 0, "Automatically Detect Public IP Address", uifcYesNoOpts);
				if(i == 0)
					startup.options |= FTP_OPT_LOOKUP_PASV_IP;
				else if(i == 1) {
					startup.options &= ~FTP_OPT_LOOKUP_PASV_IP;
					IPv4AddressToStr(startup.pasv_ip_addr.s_addr, str, sizeof(str));
					if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "IPv4 Address for Passive Connections", str, sizeof(str)-1, K_EDIT) > 0)
						startup.pasv_ip_addr.s_addr = parseIPv4Address(str);
				}
				break;
			case 5:
				SAFEPRINTF(str, "%u", startup.pasv_port_low);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Lowest Passive TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.pasv_port_low = atoi(str);
				else
					break;
				SAFEPRINTF(str, "%u", startup.pasv_port_high);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Highest Passive TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.pasv_port_high = atoi(str);
				break;
			case 6:
				i = startup.options & FTP_OPT_INDEX_FILE ? 0 : 1;
				i = uifc.list(WIN_SAV|WIN_MID, 0, 0, 0, &i, 0, "Automatically Generate Index Files", uifcYesNoOpts);
				if(i == 0) {
					startup.options |= FTP_OPT_INDEX_FILE;
					uifc.input(WIN_MID|WIN_SAV, 0, 0, "Index Filename"
						,startup.index_file_name, sizeof(startup.index_file_name)-1, K_EDIT);
				} else if(i == 1)
					startup.options &= ~FTP_OPT_INDEX_FILE;
				break;
			case 7:
				startup.options ^= FTP_OPT_ALLOW_QWK;
				break;
			case 8:
				SAFECOPY(str, duration(startup.qwk_timeout, false));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "QWK Message Packet Creation Timeout", str, 10, K_EDIT) > 0)
					startup.qwk_timeout = (uint16_t)parse_duration(str);
				break;
			case 9:
				SAFECOPY(str, maximum(startup.max_clients));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Client Count (0=Unlimited)", str, 10, K_EDIT) > 0)
					startup.max_clients = atoi(str);
				break;
			case 10:
				SAFECOPY(str, duration(startup.max_inactivity, false));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Client Inactivity", str, 10, K_EDIT) > 0)
					startup.max_inactivity = (uint16_t)parse_duration(str);
				break;
			case 11:
				SAFECOPY(str, maximum(startup.max_concurrent_connections));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Concurrent (Unauthenticated) Connections", str, 10, K_EDIT) > 0)
					startup.max_concurrent_connections = atoi(str);
				break;
			case 12:
				startup.options ^= FTP_OPT_NO_LOCAL_FSYS;
				break;
			case 13:
				startup.options ^= FTP_OPT_ALLOW_BOUNCE;
				break;
			case 14:
				startup.options ^= BBS_OPT_NO_HOST_LOOKUP;
				break;
			default:
				if(memcmp(&saved_startup, &startup, sizeof(startup)) != 0)
					uifc.changes = true;
				i = save_changes(WIN_MID);
				if(i < 0)
					continue;
				if(i == 0) {
					FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */true);
					if(fp == NULL)
						uifc.msgf("Error opening %s", cfg.filename);
					else {
						if(!sbbs_write_ini(
							 fp
							,&cfg
							,NULL
							,FALSE
							,NULL
							,enabled
							,&startup
							,false
							,NULL
							,false
							,NULL
							,false
							,NULL
							))
							uifc.msgf("Error writing %s", cfg.filename);
						iniCloseFile(fp);
					}
				}
				sbbs_free_ini(NULL
					,NULL
					,&startup
					,NULL
					,NULL //&mail_startup
					,NULL //&services_startup
					);
				return;
		}
	}
}

static void sendmail_cfg(mail_startup_t* startup)
{
	const char* p;
	char str[256];
	static int cur, bar;

	while(1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", startup->options & MAIL_OPT_NO_SENDMAIL ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Rescan Interval", vduration(startup->rescan_frequency));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Connect Timeout", vduration(startup->connect_timeout));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Auto-exempt Recipients", startup->options & MAIL_OPT_NO_AUTO_EXEMPT ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "Max Delivery Attempts", startup->max_delivery_attempts);
		bool applicable = startup->options & MAIL_OPT_RELAY_TX;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Delivery Method", applicable ? "Relay" : "Direct");
		if(applicable) {
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Relay Server Address", startup->relay_server);
			snprintf(opt[i++], MAX_OPLN, "%-30s%u", "Relay Server TCP Port", startup->relay_port);
			if(startup->options & MAIL_OPT_RELAY_AUTH_PLAIN)
				p = "Plain";
			else if(startup->options & MAIL_OPT_RELAY_AUTH_LOGIN)
				p = "Login";
			else if(startup->options & MAIL_OPT_RELAY_AUTH_CRAM_MD5)
				p = "CRAM-MD5";
			else
				p = "None";
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Relay Server Authentication", p);
			if(startup->options & (MAIL_OPT_RELAY_AUTH_PLAIN | MAIL_OPT_RELAY_AUTH_LOGIN | MAIL_OPT_RELAY_AUTH_CRAM_MD5)) {
				snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Relay Server Username", startup->relay_user);
				snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Relay Server Password", startup->relay_pass);
			}
		}
		if(startup->options & MAIL_OPT_NO_SENDMAIL)
			i = 1;
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`SendMail Thread Configuration:`\n"
			"\n"
			"Set the operating parameters of the Synchronet Mail Server SendMail\n"
			"Thread.\n"
			"\n"
			"For full documentation, see `http://wiki.synchro.net/server:mail`\n"
		;
		switch(uifc.list(WIN_ACT|WIN_ESC|WIN_BOT|WIN_SAV, 0, 0, 0, &cur, &bar
			,"SendMail Thread",opt)) {
			case 0:
				startup->options ^= MAIL_OPT_NO_SENDMAIL;
				break;
			case 1:
				SAFECOPY(str, duration(startup->rescan_frequency, false));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "MailBase Rescan Interval", str, 5, K_EDIT) > 0)
					startup->rescan_frequency = (uint16_t)parse_duration(str);
				break;
			case 2:
				SAFECOPY(str, duration(startup->connect_timeout, false));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "SendMail Connect Timeout", str, 5, K_EDIT) > 0)
					startup->connect_timeout = (uint32_t)parse_duration(str);
				break;
			case 3:
				startup->options ^= MAIL_OPT_NO_AUTO_EXEMPT;
				break;
			case 4:
				SAFEPRINTF(str, "%u", startup->max_delivery_attempts);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Number of SendMail Delivery Attempts", str, 5, K_NUMBER|K_EDIT) > 0)
					startup->max_delivery_attempts = atoi(str);
				break;
			case 5:
				startup->options ^= MAIL_OPT_RELAY_TX;
				break;
			case 6:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "Relay Server Address", startup->relay_server, sizeof(startup->relay_server)-1, K_EDIT);
				break;
			case 7:
				SAFEPRINTF(str, "%u", startup->relay_port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Relay Server TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup->relay_port = atoi(str);
				break;
			case 8:
				i = 0;
				strcpy(opt[i++], "Plain");
				strcpy(opt[i++], "Login");
				strcpy(opt[i++], "CRAM-MD5");
				strcpy(opt[i++], "None");
				opt[i][0] = '\0';
				if(startup->options & MAIL_OPT_RELAY_AUTH_PLAIN)
					i = 0;
				else if(startup->options & MAIL_OPT_RELAY_AUTH_LOGIN)
					i = 1;
				else if(startup->options & MAIL_OPT_RELAY_AUTH_CRAM_MD5)
					i = 2;
				else
					i = 3;
				if(uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, &i, 0, "Relay Server Authentication Method", opt) < 0)
					break;
				startup->options &= ~(MAIL_OPT_RELAY_AUTH_PLAIN | MAIL_OPT_RELAY_AUTH_LOGIN | MAIL_OPT_RELAY_AUTH_CRAM_MD5);
				switch(i) {
					case 0:
						startup->options |= MAIL_OPT_RELAY_AUTH_PLAIN;
						break;
					case 1:
						startup->options |= MAIL_OPT_RELAY_AUTH_LOGIN;
						break;
					case 2:
						startup->options |= MAIL_OPT_RELAY_AUTH_CRAM_MD5;
						break;
				}
				break;
			case 9:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "Relay Server Username", startup->relay_user, sizeof(startup->relay_user)-1, K_EDIT);
				break;
			case 10:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "Relay Server Password", startup->relay_pass, sizeof(startup->relay_pass)-1, K_EDIT);
				break;
			default:
				return;
		}
	}
}

static void mailsrvr_cfg(void)
{
	static int cur, bar;
	char tmp[256];
	char str[256];
	const char* p;
	BOOL enabled = FALSE;
	mail_startup_t startup = {0};

	FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */false);
	if(fp == NULL) {
		uifc.msgf("Error opening %s", cfg.filename);
		return;
	}
	uifc.pop(strReadingIniFile);
	sbbs_read_ini(
		 fp
		,cfg.filename
		,NULL
		,NULL
		,NULL
		,NULL
		,NULL
		,NULL
		,NULL
		,&enabled
		,&startup
		,NULL
		,NULL //&services_startup
		);
	iniCloseFile(fp);
	uifc.pop(NULL);
	mail_startup_t saved_startup = startup;

	while(1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", enabled ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Log Level", iniLogLevelStringList()[startup.log_level]);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "SMTP Interfaces", strListCombine(startup.interfaces, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "SMTP Port", startup.smtp_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Submission Support", startup.options & MAIL_OPT_USE_SUBMISSION_PORT ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "Submission Port", startup.submission_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Submission/TLS Support", startup.options & MAIL_OPT_TLS_SUBMISSION ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "Submission/TLS Port", startup.submissions_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "POP3 Support", startup.options & MAIL_OPT_ALLOW_POP3 ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "POP3 Interfaces"
			,startup.options & (MAIL_OPT_ALLOW_POP3 | MAIL_OPT_TLS_POP3)
				? strListCombine(startup.pop3_interfaces, tmp, sizeof(tmp), ", ") : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "POP3 Port", startup.pop3_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "POP3/TLS Support", startup.options & MAIL_OPT_TLS_POP3 ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "POP3/TLS Port", startup.pop3s_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Clients", maximum(startup.max_clients));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Inactivity", vduration(startup.max_inactivity));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Concurrent Connections", maximum(startup.max_concurrent_connections));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Recipients Per Message", maximum(startup.max_recipients));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Messages Waiting", maximum(startup.max_msgs_waiting));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s bytes", "Max Receive Message Size", byte_count_to_str(startup.max_msg_size, tmp, sizeof(tmp)));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Default Recipient", startup.default_user);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Receive By User Number", startup.options & MAIL_OPT_ALLOW_RX_BY_NUMBER ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Receive By Sysop Aliases", startup.options & MAIL_OPT_ALLOW_SYSOP_ALIASES ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Notify Local Recipients", startup.options & MAIL_OPT_NO_NOTIFY ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Allow Users to Relay Mail", startup.options & MAIL_OPT_ALLOW_RELAY ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Lookup Client Hostname", startup.options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Check Headers against DNSBL", startup.options & MAIL_OPT_DNSBL_CHKRECVHDRS ? "Yes" : "No");
		if(startup.options & MAIL_OPT_DNSBL_REFUSE)
			p = "Refuse Session";
		else if(startup.options & MAIL_OPT_DNSBL_IGNORE)
			p = "Silently Ignore";
		else if(startup.options & MAIL_OPT_DNSBL_BADUSER)
			p = "Refuse Mail";
		else
			p = "Tag Mail";
		snprintf(opt[i++], MAX_OPLN, "%-30s%s%s", "DNS-Blacklisted Servers", startup.options & MAIL_OPT_DNSBL_THROTTLE ? "Throttle and " : "", p);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Hash DNS-Blacklisted Msgs", startup.options & MAIL_OPT_DNSBL_SPAMHASH ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Kill SPAM When Read", startup.options & MAIL_OPT_KILL_READ_SPAM ? "Yes": "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "SendMail Thread...", startup.options & MAIL_OPT_NO_SENDMAIL ? strDisabled : "");
		if(!enabled)
			i = 1;
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`Mail Server Configuration:`\n"
			"\n"
			"Set the initial parameters of the Synchronet Mail Server here.  This\n"
			"server supports receiving mail over [E]SMTP and [E]SMTPS protocols as\n"
			"well as sending mail to clients via the POP3 or POP3S protocols and\n"
			"sending mail to other servers (the SendMail thread) via [E]SMTP and\n"
			"[E]SMTPS protocols.\n"
			"\n"
			"For full documentation, see `http://wiki.synchro.net/server:mail`\n"
		;
		switch(uifc.list(WIN_ACT|WIN_ESC|WIN_RHT|WIN_SAV, 0, 0, 0, &cur, &bar
			,"Mail Server",opt)) {
			case 0:
				enabled = !enabled;
				uifc.changes = true;
				break;
			case 1:
				uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, &startup.log_level, 0, "Log Level", iniLogLevelStringList());
				break;
			case 2:
				strListCombine(startup.interfaces, str, sizeof(str), ", ");
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "SMTP Network Interfaces (IPv4/6)", str, sizeof(str)-1, K_EDIT) >= 0) {
					strListFree(&startup.interfaces);
					strListSplitCopy(&startup.interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 3:
				SAFEPRINTF(str, "%u", startup.smtp_port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "SMTP TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.smtp_port = atoi(str);
				break;
			case 4:
				startup.options ^= MAIL_OPT_USE_SUBMISSION_PORT;
				break;
			case 5:
				SAFEPRINTF(str, "%u", startup.submission_port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "SMTP-Submission TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.submission_port = atoi(str);
				break;
			case 6:
				startup.options ^= MAIL_OPT_TLS_SUBMISSION;
				break;
			case 7:
				SAFEPRINTF(str, "%u", startup.submissions_port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "SMTP-Submission/TLS TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.submissions_port = atoi(str);
				break;
			case 8:
				startup.options ^= MAIL_OPT_ALLOW_POP3;
				break;
			case 9:
				strListCombine(startup.pop3_interfaces, str, sizeof(str), ", ");
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "POP3 Network Interfaces (IPv4/6)", str, sizeof(str)-1, K_EDIT) >= 0) {
					strListFree(&startup.pop3_interfaces);
					strListSplitCopy(&startup.pop3_interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 10:
				SAFEPRINTF(str, "%u", startup.pop3_port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "POP3 TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.pop3_port = atoi(str);
				break;
			case 11:
				startup.options ^= MAIL_OPT_TLS_POP3;
				break;
			case 12:
				SAFEPRINTF(str, "%u", startup.pop3s_port);
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "POP3/TLS TCP Port", str, 5, K_NUMBER|K_EDIT) > 0)
					startup.pop3s_port = atoi(str);
				break;
			case 13:
				SAFECOPY(str, maximum(startup.max_clients));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Client Count (0=Unlimited)", str, 10, K_EDIT) > 0)
					startup.max_clients = atoi(str);
				break;
			case 14:
				SAFECOPY(str, duration(startup.max_inactivity, false));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Client Inactivity", str, 10, K_EDIT) > 0)
					startup.max_inactivity = (uint16_t)parse_duration(str);
				break;
			case 15:
				SAFECOPY(str, maximum(startup.max_concurrent_connections));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Concurrent (Unauthenticated) Connections", str, 10, K_EDIT) > 0)
					startup.max_concurrent_connections = atoi(str);
				break;
			case 16:
				SAFECOPY(str, maximum(startup.max_recipients));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Recipients per Message", str, 10, K_EDIT) > 0)
					startup.max_recipients = atoi(str);
				break;
			case 17:
				SAFECOPY(str, maximum(startup.max_msgs_waiting));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Messages Waiting per User", str, 10, K_EDIT) > 0)
					startup.max_msgs_waiting = atoi(str);
				break;
			case 18:
				byte_count_to_str(startup.max_msg_size, str, sizeof(str));
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Maximum Received Message Size (in bytes)", str, 10, K_EDIT) > 0)
					startup.max_msg_size = (uint32_t)parse_byte_count(str, 1);
				break;
			case 19:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "Default Recipient (user alias)"
					,startup.default_user, sizeof(startup.default_user)-1, K_EDIT);
				break;
			case 20:
				startup.options ^= MAIL_OPT_ALLOW_RX_BY_NUMBER;
				break;
			case 21:
				startup.options ^= MAIL_OPT_ALLOW_SYSOP_ALIASES;
				break;
			case 22:
				startup.options ^= MAIL_OPT_NO_NOTIFY;
				break;
			case 23:
				startup.options ^= MAIL_OPT_ALLOW_RELAY;
				break;
			case 24:
				startup.options ^= BBS_OPT_NO_HOST_LOOKUP;
				break;
			case 25:
				startup.options ^= MAIL_OPT_DNSBL_CHKRECVHDRS;
				break;
			case 26:
				i = 0;
				strcpy(opt[i++], "Refuse Session");
				strcpy(opt[i++], "Silently Ignore");
				strcpy(opt[i++], "Refuse Mail");
				strcpy(opt[i++], "Tag Mail");
				opt[i][0] = '\0';
				if(startup.options & MAIL_OPT_DNSBL_REFUSE)
					i = 0;
				else if(startup.options & MAIL_OPT_DNSBL_IGNORE)
					i = 1;
				else if(startup.options & MAIL_OPT_DNSBL_BADUSER)
					i = 2;
				else
					i = 3;
				if(uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, &i, 0, "DNS-Blacklisted Servers", opt) < 0)
					break;
				startup.options &= ~(MAIL_OPT_DNSBL_REFUSE | MAIL_OPT_DNSBL_IGNORE | MAIL_OPT_DNSBL_BADUSER);
				switch(i) {
					case 0:
						startup.options |= MAIL_OPT_DNSBL_REFUSE;
						break;
					case 1:
						startup.options |= MAIL_OPT_DNSBL_IGNORE;
						break;
					case 2:
						startup.options |= MAIL_OPT_DNSBL_BADUSER;
				}
				i = startup.options & MAIL_OPT_DNSBL_THROTTLE ? 0 : 1;
				if(uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, &i, 0, "Throttle DNS-Blacklisted Servers", uifcYesNoOpts) < 0)
					break;
				if(i == 0)
					startup.options |= MAIL_OPT_DNSBL_THROTTLE;
				else
					startup.options &= ~MAIL_OPT_DNSBL_THROTTLE;
				break;
			case 27:
				startup.options ^= MAIL_OPT_DNSBL_SPAMHASH;
				break;
			case 28:
				startup.options ^= MAIL_OPT_KILL_READ_SPAM;
				break;
			case 29:
				sendmail_cfg(&startup);
				break;
			default:
				if(memcmp(&saved_startup, &startup, sizeof(startup)) != 0)
					uifc.changes = true;
				i = save_changes(WIN_MID);
				if(i < 0)
					continue;
				if(i == 0) {
					FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */true);
					if(fp == NULL)
						uifc.msgf("Error opening %s", cfg.filename);
					else {
						if(!sbbs_write_ini(
							 fp
							,&cfg
							,NULL
							,FALSE
							,NULL
							,false
							,NULL
							,false
							,NULL
							,enabled
							,&startup
							,false
							,NULL
							))
							uifc.msgf("Error writing %s", cfg.filename);
						iniCloseFile(fp);
					}
				}
				sbbs_free_ini(NULL
					,NULL
					,NULL
					,NULL
					,&startup
					,NULL //&services_startup
					);
				return;
		}
	}
}

static void services_cfg(void)
{
	static int cur, bar;
	char tmp[256];
	char str[256];
	BOOL enabled = FALSE;
	services_startup_t startup = {0};

	FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */false);
	if(fp == NULL) {
		uifc.msgf("Error opening %s", cfg.filename);
		return;
	}
	uifc.pop(strReadingIniFile);
	sbbs_read_ini(
		 fp
		,cfg.filename
		,NULL
		,NULL
		,NULL
		,NULL
		,NULL
		,NULL
		,NULL
		,NULL
		,NULL
		,&enabled
		,&startup
		);
	iniCloseFile(fp);
	uifc.pop(NULL);
	services_startup_t saved_startup = startup;

	while(1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", enabled ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Log Level", iniLogLevelStringList()[startup.log_level]);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Network Interfaces", strListCombine(startup.interfaces, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Lookup Client Hostname", startup.options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Configuration File", startup.services_ini);
		if(!enabled)
			i = 1;
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`Services Server Configuration:`\n"
			"\n"
			"For full documentation, see `http://wiki.synchro.net/server:services`\n"

		;
		switch(uifc.list(WIN_ACT|WIN_ESC|WIN_RHT|WIN_SAV, 0, 0, 0, &cur, &bar
			,"Services Server",opt)) {
			case 0:
				enabled = !enabled;
				uifc.changes = true;
				break;
			case 1:
				uifc.list(WIN_MID|WIN_SAV, 0, 0, 0, &startup.log_level, 0, "Log Level", iniLogLevelStringList());
				break;
			case 2:
				strListCombine(startup.interfaces, str, sizeof(str), ", ");
				if(uifc.input(WIN_MID|WIN_SAV, 0, 0, "Network Interfaces (IPv4/6)", str, sizeof(str)-1, K_EDIT) >= 0) {
					strListFree(&startup.interfaces);
					strListSplitCopy(&startup.interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 3:
				startup.options ^= BBS_OPT_NO_HOST_LOOKUP;
				break;
			case 4:
				uifc.input(WIN_MID|WIN_SAV, 0, 0, "Services Configuration File", startup.services_ini, sizeof(startup.services_ini)-1, K_EDIT);
				break;
			default:
				if(memcmp(&saved_startup, &startup, sizeof(startup)) != 0)
					uifc.changes = true;
				i = save_changes(WIN_MID);
				if(i < 0)
					continue;
				if(i == 0) {
					FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */true);
					if(fp == NULL)
						uifc.msgf("Error opening %s", cfg.filename);
					else {
						if(!sbbs_write_ini(
							 fp
							,&cfg
							,NULL
							,FALSE
							,NULL
							,false
							,NULL
							,false
							,NULL
							,false
							,NULL
							,enabled
							,&startup
							))
							uifc.msgf("Error writing %s", cfg.filename);
						iniCloseFile(fp);
					}
				}
				sbbs_free_ini(NULL
					,NULL
					,NULL
					,NULL
					,NULL
					,&startup
					);
				return;
		}
	}
}

void server_cfg(void)
{
	static int srvr_dflt;
	BOOL run_bbs, run_ftp, run_web, run_mail, run_services;

	sbbs_get_ini_fname(cfg.filename, cfg.ctrl_dir);

	while(1) {
		FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */false);
		if(fp == NULL) {
			uifc.msgf("Error opening %s", cfg.filename);
			return;
		}
		uifc.pop(strReadingIniFile);
		sbbs_read_ini(
			 fp
			,cfg.filename
			,NULL //&global_startup
			,&run_bbs
			,NULL //&bbs_startup
			,&run_ftp
			,NULL //&ftp_startup
			,&run_web
			,NULL //&web_startup
			,&run_mail
			,NULL //&mail_startup
			,&run_services
			,NULL //&services_startup
			);
		iniCloseFile(fp);
		uifc.pop(NULL);

		int i = 0;
		strcpy(opt[i++], "Global Settings");
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Terminal Server", run_bbs ? "" : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Web Server", run_web ? "" : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "FTP Server", run_ftp ? "" : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Mail Server", run_mail ? "" : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Services Server", run_services ? "" : strDisabled);
		opt[i][0] = '\0';

		uifc.helpbuf=
			"`Server Configuration:`\n"
			"\n"
			"Here you can configure the initialization settings of the various\n"
			"Internet servers that are integrated into Synchronet BBS Software.\n"
			"\n"
			"`Global Settings`   Settings that are applied to multiple servers\n"
			"`Terminal Server`   Provides the traditional BBS user experience\n"
			"`Web Server`        Provides the modern HTTP/HTTPS browser experience\n"
			"`FTP Server`        Serves clients using ye olde File Transfer Protocol\n"
			"`Mail Server`       Supports SMTP and POP3 mail transfer protocols\n"
			"`Services Server`   Supports plug-in style servers: NNTP, IRC, IMAP, etc.\n"
			"\n"
			"For additional advanced Synchronet server initialization settings, see\n"
			"the `ctrl/sbbs.ini` file and `https://wiki.synchro.net/config:sbbs.ini`\n"
			"for reference.\n"
		;
		i = uifc.list(WIN_ORG|WIN_ACT,0,0,0,&srvr_dflt,0, "Server Configuration",opt);
		switch(i) {
			case 0:
				global_cfg();
				break;
			case 1:
				termsrvr_cfg();
				break;
			case 2:
				websrvr_cfg();
				break;
			case 3:
				ftpsrvr_cfg();
				break;
			case 4:
				mailsrvr_cfg();
				break;
			case 5:
				services_cfg();
				break;
			default:
				return;
		}
	}
}
