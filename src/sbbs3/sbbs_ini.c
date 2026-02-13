/* Synchronet initialization (.ini) file routines */

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

#define STARTUP_INI_BITDESC_TABLES

#include "sockwrap.h"
#include <string.h> /* strchr, memset */

#include "sbbs_ini.h"
#include "dirwrap.h"    /* backslash */
#include "sbbsdefs.h"   /* JAVASCRIPT_* macros */
#include "nopen.h"      /* backup */

static const char* nulstr = "";
static const char* strAutoStart = "AutoStart";
static const char* strCtrlDirectory = "CtrlDirectory";
static const char* strTempDirectory = "TempDirectory";
static const char* strOptions = "Options";
static const char* strOutgoing4 = "OutboundInterface";
static const char* strOutgoing6 = "OutboundV6Interface";
static const char* strInterfaces = "Interface";
static const char* strPort = "Port";
static const char* strMaxClients = "MaxClients";
static const char* strMaxInactivity = "MaxInactivity";
static const char* strMaxDumbTermInactivity = "MaxDumbTermInactivity";
static const char* strMaxLoginInactivity = "MaxLoginInactivity";
static const char* strMaxNewUserInactivity = "MaxNewUserInactivity";
static const char* strMaxSessionInactivity = "MaxSessionInactivity";
static const char* strMaxSFTPInactivity = "MaxSFTPInactivity";
static const char* strMaxConConn = "MaxConcurrentConnections";
static const char* strMaxRequestPerPeriod = "MaxRequestsPerPeriod";
static const char* strRequestRateLimitPeriod = "RequestRateLimitPeriod";
static const char* strMaxConnectsPerPeriod = "MaxConnectsPerPeriod";
static const char* strConnectRateLimitPeriod = "ConnectRateLimitPeriod";
static const char* strHostName = "HostName";
static const char* strLogLevel = "LogLevel";
static const char* strEventLogLevel = "EventLogLevel";
static const char* strTLSErrorLevel = "TLSErrorLevel";
static const char* strBindRetryCount = "BindRetryCount";
static const char* strBindRetryDelay = "BindRetryDelay";
static const char* strAnswerSound = "AnswerSound";
static const char* strHangupSound = "HangupSound";
static const char* strLoginSound = "LoginSound";
static const char* strLogoutSound = "LogoutSound";
static const char* strHackAttemptSound = "HackAttemptSound";
static const char* strLoginAttemptDelay = "LoginAttemptDelay";
static const char* strLoginAttemptThrottle = "LoginAttemptThrottle";
static const char* strLoginAttemptHackThreshold = "LoginAttemptHackThreshold";
static const char* strLoginAttemptTempBanThreshold = "LoginAttemptTempBanThreshold";
static const char* strLoginAttemptTempBanDuration = "LoginAttemptTempBanDuration";
static const char* strLoginAttemptFilterThreshold = "LoginAttemptFilterThreshold";
static const char* strLoginAttemptFilterDuration = "LoginAttemptFilterDuration";
static const char* strLoginInfoSave = "LoginInfoSave";
static const char* strLoginRequirements = "LoginRequirements";
static const char* strArchiveRequirements = "ArchiveRequirements";
static const char* strJavaScriptMaxBytes       = "JavaScriptMaxBytes";
static const char* strJavaScriptTimeLimit      = "JavaScriptTimeLimit";
static const char* strJavaScriptGcInterval     = "JavaScriptGcInterval";
static const char* strJavaScriptYieldInterval  = "JavaScriptYieldInterval";
static const char* strJavaScriptLoadPath       = "JavaScriptLoadPath";
static const char* strJavaScriptOptions        = "JavaScriptOptions";
static const char* strSemFileCheckFrequency    = "SemFileCheckFrequency";
static const char* strIniFileName              = "iniFileName";
static const char* strFileVPathPrefix          = "FileVPathPrefix";
static const char* strFileVPathForVHosts       = "FileVPathForVHosts";
static const char* strFileIndexScript          = "FileIndexScript";

#define DEFAULT_LOG_LEVEL               LOG_DEBUG
#define DEFAULT_BIND_RETRY_COUNT        2
#define DEFAULT_BIND_RETRY_DELAY        15

void sbbs_get_ini_fname(char* ini_file, const char* ctrl_dir)
{
	/* pHostName is no longer used since iniFileName calls gethostname() itself */

#if defined(_WINSOCKAPI_)
	WSADATA WSAData;
	(void)WSAStartup(MAKEWORD(1, 1), &WSAData); /* req'd for gethostname */
#endif

#if defined(__unix__) && defined(PREFIX)
	sprintf(ini_file, PREFIX "/etc/sbbs.ini");
	if (fexistcase(ini_file))
		return;
#endif
	iniFileName(ini_file, MAX_PATH, ctrl_dir, "sbbs.ini");

#if defined(_WINSOCKAPI_)
	WSACleanup();
#endif
}

static bool iniSetStringWithGlobalDefault(str_list_t* lp, const char* section, const char* key
                                          , const char* value, const char* global_value, ini_style_t* style)
{
	if (value != global_value && strcmp(value, global_value) == 0) {
		return iniKeyExists(*lp, section, key) == false || iniRemoveValue(lp, section, key) == true;
	}
	return iniSetString(lp, section, key, value, style) != NULL;
}

static void sbbs_fix_js_settings(js_startup_t* js)
{
	/* Some sanity checking here */
	if (js->max_bytes == 0)
		js->max_bytes = JAVASCRIPT_MAX_BYTES;
}

void sbbs_get_js_settings(
	str_list_t list
	, const char* section
	, js_startup_t* js
	, js_startup_t* defaults)
{
	char  value[INI_MAX_VALUE_LEN];
	char* p;

	js->max_bytes       = (uint)iniGetBytes(list, section, strJavaScriptMaxBytes, /* unit: */ 1, defaults->max_bytes);
	js->time_limit      = iniGetInteger(list, section, strJavaScriptTimeLimit, defaults->time_limit);
	js->gc_interval     = iniGetInteger(list, section, strJavaScriptGcInterval, defaults->gc_interval);
	js->yield_interval  = iniGetInteger(list, section, strJavaScriptYieldInterval, defaults->yield_interval);
	js->options         = iniGetBitField(list, section, strJavaScriptOptions, js_options, defaults->options);

	/* Get JavaScriptLoadPath, use default if key is missing, use blank if key value is blank */
	if ((p = iniGetExistingString(list, section, strJavaScriptLoadPath, nulstr, value)) == NULL) {
		if (defaults != js)
			SAFECOPY(js->load_path, defaults->load_path);
	} else
		SAFECOPY(js->load_path, p);

	sbbs_fix_js_settings(js);
}

bool sbbs_set_js_settings(
	str_list_t* lp
	, const char* section
	, js_startup_t* js
	, js_startup_t* defaults
	, ini_style_t* style)
{
	bool         failure = false;
	js_startup_t global_defaults = {
		JAVASCRIPT_MAX_BYTES
		, JAVASCRIPT_TIME_LIMIT
		, JAVASCRIPT_GC_INTERVAL
		, JAVASCRIPT_YIELD_INTERVAL
		, JAVASCRIPT_OPTIONS
		, JAVASCRIPT_LOAD_PATH
	};
	SAFECOPY(global_defaults.load_path, JAVASCRIPT_LOAD_PATH);

	if (defaults == NULL)
		defaults = &global_defaults;

	sbbs_fix_js_settings(js);

	if (js->max_bytes == defaults->max_bytes)
		iniRemoveValue(lp, section, strJavaScriptMaxBytes);
	else
		failure |= iniSetBytes(lp, section, strJavaScriptMaxBytes, /*unit: */ 1, js->max_bytes, style) == NULL;

	if (js->time_limit == defaults->time_limit)
		iniRemoveValue(lp, section, strJavaScriptTimeLimit);
	else
		failure |= iniSetInteger(lp, section, strJavaScriptTimeLimit, js->time_limit, style) == NULL;

	if (js->gc_interval == defaults->gc_interval)
		iniRemoveValue(lp, section, strJavaScriptGcInterval);
	else
		failure |= iniSetInteger(lp, section, strJavaScriptGcInterval, js->gc_interval, style) == NULL;

	if (js->yield_interval == defaults->yield_interval)
		iniRemoveValue(lp, section, strJavaScriptYieldInterval);
	else
		failure |= iniSetInteger(lp, section, strJavaScriptYieldInterval, js->yield_interval, style) == NULL;

	if (strcmp(js->load_path, defaults->load_path) == 0)
		iniRemoveKey(lp, section, strJavaScriptLoadPath);
	else
		failure |= iniSetString(lp, section, strJavaScriptLoadPath, js->load_path, style) == NULL;

	return !failure;
}

void sbbs_get_sound_settings(str_list_t list, const char* section, struct startup_sound_settings* sound
                             , struct startup_sound_settings* defaults)
{
	char  value[INI_MAX_VALUE_LEN];
	char* p;

	if ((p = iniGetString(list, section, strAnswerSound, defaults->answer, value)) != NULL && *p != '\0')
		SAFECOPY(sound->answer, value);

	if ((p = iniGetString(list, section, strLoginSound, defaults->login, value)) != NULL && *p != '\0')
		SAFECOPY(sound->login, value);

	if ((p = iniGetString(list, section, strLogoutSound, defaults->logout, value)) != NULL && *p != '\0')
		SAFECOPY(sound->logout, value);

	if ((p = iniGetString(list, section, strHangupSound, defaults->hangup, value)) != NULL && *p != '\0')
		SAFECOPY(sound->hangup, value);

	if ((p = iniGetString(list, section, strHackAttemptSound, defaults->hack, value)) != NULL && *p != '\0')
		SAFECOPY(sound->hack, value);
}

bool sbbs_set_sound_settings(
	str_list_t* lp
	, const char* section
	, struct startup_sound_settings* sound
	, struct startup_sound_settings* defaults
	, ini_style_t* style)
{
	if (!iniSetStringWithGlobalDefault(lp, section, strAnswerSound, sound->answer, defaults->answer, style))
		return false;
	if (!iniSetStringWithGlobalDefault(lp, section, strLoginSound, sound->login, defaults->login, style))
		return false;
	if (!iniSetStringWithGlobalDefault(lp, section, strLogoutSound, sound->logout, defaults->logout, style))
		return false;
	if (!iniSetStringWithGlobalDefault(lp, section, strHangupSound, sound->hangup, defaults->hangup, style))
		return false;
	if (!iniSetStringWithGlobalDefault(lp, section, strHackAttemptSound, sound->hack, defaults->hack, style))
		return false;
	return true;
}

static struct login_attempt_settings get_login_attempt_settings(str_list_t list, const char* section, global_startup_t* global)
{
	struct login_attempt_settings settings;

	settings.delay              = iniGetInteger(list, section, strLoginAttemptDelay, global == NULL ? 5000 : global->login_attempt.delay);
	settings.throttle           = iniGetInteger(list, section, strLoginAttemptThrottle, global == NULL ? 1000 : global->login_attempt.throttle);
	settings.hack_threshold     = iniGetInteger(list, section, strLoginAttemptHackThreshold, global == NULL ? 10 : global->login_attempt.hack_threshold);
	settings.tempban_threshold  = iniGetInteger(list, section, strLoginAttemptTempBanThreshold, global == NULL ? 20 : global->login_attempt.tempban_threshold);
	settings.tempban_duration   = (uint)iniGetDuration(list, section, strLoginAttemptTempBanDuration, global == NULL ? (10 * 60) : global->login_attempt.tempban_duration);
	settings.filter_threshold   = iniGetInteger(list, section, strLoginAttemptFilterThreshold, global == NULL ? 0 : global->login_attempt.filter_threshold);
	settings.filter_duration    = (uint)iniGetDuration(list, section, strLoginAttemptFilterDuration, global == NULL ? 0 : global->login_attempt.filter_duration);
	return settings;
}

static void set_login_attempt_settings(str_list_t* lp, const char* section, struct login_attempt_settings settings, ini_style_t style)
{
	iniSetInteger(lp, section, strLoginAttemptDelay, settings.delay, &style);
	iniSetInteger(lp, section, strLoginAttemptThrottle, settings.throttle, &style);
	iniSetInteger(lp, section, strLoginAttemptHackThreshold, settings.hack_threshold, &style);
	iniSetInteger(lp, section, strLoginAttemptTempBanThreshold, settings.tempban_threshold, &style);
	iniSetDuration(lp, section, strLoginAttemptTempBanDuration, settings.tempban_duration, &style);
	iniSetInteger(lp, section, strLoginAttemptFilterThreshold, settings.filter_threshold, &style);
	iniSetDuration(lp, section, strLoginAttemptFilterDuration, settings.filter_duration, &style);
}

static const struct in6_addr wildcard6;

static bool get_ini_globals(str_list_t list, global_startup_t* global)
{
	const char* section = "Global";
	char        value[INI_MAX_VALUE_LEN];
	char*       p;

	if (global->size != sizeof *global)
		return false;

	p = iniGetString(list, section, strCtrlDirectory, nulstr, value);
	if (*p) {
		SAFECOPY(global->ctrl_dir, value);
		backslash(global->ctrl_dir);
	}

	p = iniGetString(list, section, strTempDirectory, nulstr, value);
	if (*p) {
		SAFECOPY(global->temp_dir, value);
		backslash(global->temp_dir);
	}

	p = iniGetString(list, section, strHostName, nulstr, value);
	if (*p)
		SAFECOPY(global->host_name, value);

	SAFECOPY(global->login_ars, iniGetString(list, section, strLoginRequirements, nulstr, value));

	global->sem_chk_freq = (uint16_t)iniGetDuration(list, section, strSemFileCheckFrequency, DEFAULT_SEM_CHK_FREQ);
	global->interfaces = iniGetStringList(list, section, strInterfaces, ",", "0.0.0.0,::");
	global->outgoing4.s_addr = iniGetIpAddress(list, section, strOutgoing4, INADDR_ANY);
	global->outgoing6 = iniGetIp6Address(list, section, strOutgoing6, wildcard6);
	global->log_level = iniGetLogLevel(list, section, strLogLevel, DEFAULT_LOG_LEVEL);
	global->tls_error_level = iniGetLogLevel(list, section, strTLSErrorLevel, 0);
	global->bind_retry_count = iniGetInteger(list, section, strBindRetryCount, DEFAULT_BIND_RETRY_COUNT);
	global->bind_retry_delay = iniGetInteger(list, section, strBindRetryDelay, DEFAULT_BIND_RETRY_DELAY);
	global->login_attempt = get_login_attempt_settings(list, section, NULL);

	/* Setup default values here */
	global->js.max_bytes        = JAVASCRIPT_MAX_BYTES;
	global->js.time_limit       = JAVASCRIPT_TIME_LIMIT;
	global->js.gc_interval      = JAVASCRIPT_GC_INTERVAL;
	global->js.yield_interval   = JAVASCRIPT_YIELD_INTERVAL;
	global->js.options          = JAVASCRIPT_OPTIONS;
	SAFECOPY(global->js.load_path, JAVASCRIPT_LOAD_PATH);

	sbbs_get_js_settings(list, section, &global->js, &global->js);
	sbbs_get_sound_settings(list, section, &global->sound, &global->sound);

	return true;
}

void sbbs_free_ini(
	global_startup_t*      global
	, bbs_startup_t*         bbs
	, ftp_startup_t*         ftp
	, web_startup_t*         web
	, mail_startup_t*        mail
	, services_startup_t*    services
	)
{
	if (global != NULL) {
		strListFree(&global->interfaces);
	}
	if (bbs != NULL) {
		strListFree(&bbs->telnet_interfaces);
		strListFree(&bbs->rlogin_interfaces);
		strListFree(&bbs->ssh_interfaces);
	}
	if (web != NULL) {
		strListFree(&web->interfaces);
		strListFree(&web->tls_interfaces);
		strListFree(&web->index_file_name);
		strListFree(&web->cgi_ext);
	}
	if (ftp != NULL) {
		strListFree(&ftp->interfaces);
	}
	if (mail != NULL) {
		strListFree(&mail->interfaces);
		strListFree(&mail->pop3_interfaces);
	}
	if (services != NULL) {
		strListFree(&services->interfaces);
	}
}

bool sbbs_read_ini(
	  FILE*                  fp
	, const char*            ini_fname
	, global_startup_t*      global
	, bool*                  run_bbs
	, bbs_startup_t*         bbs
	, bool*                  run_ftp
	, ftp_startup_t*         ftp
	, bool*                  run_web
	, web_startup_t*         web
	, bool*                  run_mail
	, mail_startup_t*        mail
	, bool*                  run_services
	, services_startup_t*    services
	)
{
	const char*      section;
	const char*      default_term_ansi;
#if defined(__linux__) || defined(__FreeBSD__)
	const char*      default_dosemu_path;
#if defined(__linux__)
	const char*      default_dosemuconf_path;
#endif
#endif
	char             value[INI_MAX_VALUE_LEN];
	str_list_t       list;
	global_startup_t global_buf;
	struct in6_addr  wildcard6 = {{{0}}};
	char *           global_interfaces;

	if (global == NULL) {
		memset(&global_buf, 0, sizeof(global_buf));
		global_buf.size = sizeof global_buf;
		global = &global_buf;
	}

	sbbs_free_ini(global
	              , bbs
	              , ftp
	              , web
	              , mail
	              , services
	              );

	list = iniReadFile(fp);

	if (!get_ini_globals(list, global)) {
		iniFreeStringList(list);
		return false;
	}

	if (global->ctrl_dir[0]) {
		if (bbs != NULL)
			SAFECOPY(bbs->ctrl_dir, global->ctrl_dir);
		if (ftp != NULL)
			SAFECOPY(ftp->ctrl_dir, global->ctrl_dir);
		if (web != NULL)
			SAFECOPY(web->ctrl_dir, global->ctrl_dir);
		if (mail != NULL)
			SAFECOPY(mail->ctrl_dir, global->ctrl_dir);
		if (services != NULL)
			SAFECOPY(services->ctrl_dir, global->ctrl_dir);
	}

	if (ini_fname != NULL && ini_fname[0]) {
		if (bbs != NULL)
			SAFECOPY(bbs->ini_fname, ini_fname);
		if (ftp != NULL)
			SAFECOPY(ftp->ini_fname, ini_fname);
		if (web != NULL)
			SAFECOPY(web->ini_fname, ini_fname);
		if (mail != NULL)
			SAFECOPY(mail->ini_fname, ini_fname);
		if (services != NULL)
			SAFECOPY(services->ini_fname, ini_fname);
	}

	global_interfaces = strListCombine(global->interfaces, NULL, 16384, ",");
	if (global == &global_buf)
		iniFreeStringList(global->interfaces);

	/***********************************************************************/
	section = "BBS";

	if (run_bbs != NULL)
		*run_bbs = iniGetBool(list, section, strAutoStart, true);

	if (bbs != NULL) {

		if (bbs->size != sizeof *bbs) {
			free(global_interfaces);
			iniFreeStringList(list);
			return false;
		}
		bbs->outgoing4.s_addr
		    = iniGetIpAddress(list, section, strOutgoing4, global->outgoing4.s_addr);
		bbs->outgoing6
		    = iniGetIp6Address(list, section, strOutgoing6, global->outgoing6);

		bbs->telnet_port
		    = iniGetUInt16(list, section, "TelnetPort", IPPORT_TELNET);
		bbs->telnet_interfaces
		    = iniGetStringList(list, section, "TelnetInterface", ",", global_interfaces);

		bbs->rlogin_port
		    = iniGetShortInt(list, section, "RLoginPort", 513);
		bbs->rlogin_interfaces
		    = iniGetStringList(list, section, "RLoginInterface", ",", global_interfaces);

		bbs->pet40_port
		    = iniGetShortInt(list, section, "Pet40Port", 64);
		bbs->pet80_port
		    = iniGetShortInt(list, section, "Pet80Port", 128);

		bbs->ssh_port
		    = iniGetShortInt(list, section, "SSHPort", 22);
		bbs->ssh_connect_timeout
		    = (uint16_t)iniGetDuration(list, section, "SSHConnectTimeout", 10);
		bbs->ssh_error_level
		    = iniGetLogLevel(list, section, "SSHErrorLevel", LOG_WARNING);
		bbs->ssh_interfaces
		    = iniGetStringList(list, section, "SSHInterface", ",", global_interfaces);

		bbs->first_node
		    = iniGetShortInt(list, section, "FirstNode", 1);
		bbs->last_node
		    = iniGetShortInt(list, section, "LastNode", 4);

		bbs->outbuf_highwater_mark
		    = iniGetShortInt(list, section, "OutbufHighwaterMark", 1024);
		bbs->outbuf_drain_timeout
		    = iniGetShortInt(list, section, "OutbufDrainTimeout", 10);

		bbs->sem_chk_freq
		    = (int)iniGetDuration(list, section, strSemFileCheckFrequency, global->sem_chk_freq);

		/* JavaScript operating parameters */
		sbbs_get_js_settings(list, section, &bbs->js, &global->js);

		SAFECOPY(bbs->host_name
		         , iniGetString(list, section, strHostName, global->host_name, value));

		SAFECOPY(bbs->temp_dir
		         , iniGetString(list, section, strTempDirectory, global->temp_dir, value));

		SAFECOPY(bbs->login_ars
		         , iniGetString(list, section, strLoginRequirements, global->login_ars, value));

		bbs->default_term_width = iniGetUInteger(list, section, "DefaultTermWidth", TERM_COLS_DEFAULT);
		bbs->default_term_height = iniGetUInteger(list, section, "DefaultTermHeight", TERM_ROWS_DEFAULT);

		/* Set default terminal type to "stock" termcap closest to "ansi-bbs" */
	#if defined(__FreeBSD__)
		default_term_ansi = "cons25";
	#else
		default_term_ansi = "pc3";
	#endif

		SAFECOPY(bbs->xtrn_term_ansi
		         , iniGetString(list, section, "ExternalTermANSI", default_term_ansi, value));
		SAFECOPY(bbs->xtrn_term_dumb
		         , iniGetString(list, section, "ExternalTermDumb", "dumb", value));

	#if defined(__linux__) || defined(__FreeBSD__)
	#if defined(__FreeBSD__)
		default_dosemu_path = "/usr/local/bin/doscmd";
	#else
		default_dosemu_path = "/usr/bin/dosemu.bin";
		default_dosemuconf_path = "";

		SAFECOPY(bbs->dosemuconf_path
		         , iniGetString(list, section, "DOSemuConfPath", default_dosemuconf_path, value));
	#endif
		bbs->usedosemu = iniGetBool(list, section, "UseDOSemu", true);
		SAFECOPY(bbs->dosemu_path
		         , iniGetString(list, section, "DOSemuPath", default_dosemu_path, value));
	#endif

		sbbs_get_sound_settings(list, section, &bbs->sound, &global->sound);

		bbs->log_level
		    = iniGetLogLevel(list, section, strLogLevel, global->log_level);
		bbs->event_log_level
		    = iniGetLogLevel(list, section, strEventLogLevel, bbs->log_level);
		bbs->options
		    = iniGetBitField(list, section, strOptions, bbs_options
		                     , BBS_OPT_XTRN_MINIMIZED);

		bbs->bind_retry_count = iniGetInteger(list, section, strBindRetryCount, global->bind_retry_count);
		bbs->bind_retry_delay = iniGetInteger(list, section, strBindRetryDelay, global->bind_retry_delay);

		bbs->login_attempt = get_login_attempt_settings(list, section, global);
		bbs->max_concurrent_connections = iniGetUInteger(list, section, strMaxConConn, 0);

		bbs->max_dumbterm_inactivity = (uint16_t)iniGetDuration(list, section, strMaxDumbTermInactivity, 60);
		bbs->max_login_inactivity = (uint16_t)iniGetDuration(list, section, strMaxLoginInactivity, 10 * 60);
		bbs->max_newuser_inactivity = (uint16_t)iniGetDuration(list, section, strMaxNewUserInactivity, 60 * 60);
		bbs->max_session_inactivity = (uint16_t)iniGetDuration(list, section, strMaxSessionInactivity, 10 * 60);
		bbs->max_sftp_inactivity = (uint16_t)iniGetDuration(list, section, strMaxSFTPInactivity, FTP_DEFAULT_MAX_INACTIVITY);

		SAFECOPY(bbs->web_file_vpath_prefix, iniGetString(list, "web", strFileVPathPrefix, nulstr, value));
	}

	/***********************************************************************/
	section = "FTP";

	if (run_ftp != NULL)
		*run_ftp = iniGetBool(list, section, strAutoStart, true);

	if (ftp != NULL) {

		if (ftp->size != sizeof *ftp) {
			free(global_interfaces);
			iniFreeStringList(list);
			return false;
		}
		ftp->outgoing4.s_addr
		    = iniGetIpAddress(list, section, strOutgoing4, global->outgoing4.s_addr);
		ftp->outgoing6
		    = iniGetIp6Address(list, section, strOutgoing6, global->outgoing6);
		ftp->port
		    = iniGetShortInt(list, section, strPort, IPPORT_FTP);
		ftp->interfaces
		    = iniGetStringList(list, section, strInterfaces, ",", global_interfaces);
		ftp->max_clients
		    = iniGetShortInt(list, section, strMaxClients, FTP_DEFAULT_MAX_CLIENTS);
		ftp->max_inactivity
		    = (uint16_t)iniGetDuration(list, section, strMaxInactivity, FTP_DEFAULT_MAX_INACTIVITY);    /* seconds */
		ftp->qwk_timeout
		    = (uint16_t)iniGetDuration(list, section, "QwkTimeout", FTP_DEFAULT_QWK_TIMEOUT);       /* seconds */
		ftp->sem_chk_freq
		    = (int)iniGetDuration(list, section, strSemFileCheckFrequency, global->sem_chk_freq);
		ftp->min_fsize
		    = iniGetBytes(list, section, "MinFileSize", 1, 0);
		ftp->max_fsize
		    = iniGetBytes(list, section, "MaxFileSize", 1, 0);

		/* Passive transfer settings (for stupid firewalls/NATs) */
		ftp->pasv_ip_addr.s_addr
		    = iniGetIpAddress(list, section, "PasvIpAddress", 0);
		ftp->pasv_ip6_addr
		    = iniGetIp6Address(list, section, "PasvIp6Address", wildcard6);
		ftp->pasv_port_low
		    = iniGetShortInt(list, section, "PasvPortLow", IPPORT_RESERVED);
		ftp->pasv_port_high
		    = iniGetShortInt(list, section, "PasvPortHigh", 0xffff);

		SAFECOPY(ftp->host_name
		         , iniGetString(list, section, strHostName, global->host_name, value));

		SAFECOPY(ftp->index_file_name
		         , iniGetString(list, section, "IndexFileName", "00index", value));

		sbbs_get_sound_settings(list, section, &ftp->sound, &global->sound);

		SAFECOPY(ftp->temp_dir
		         , iniGetString(list, section, strTempDirectory, global->temp_dir, value));

		SAFECOPY(ftp->login_ars
		         , iniGetString(list, section, strLoginRequirements, global->login_ars, value));
		SAFECOPY(ftp->login_info_save
				 , iniGetString(list, section, strLoginInfoSave, "", value));

		ftp->log_level
		    = iniGetLogLevel(list, section, strLogLevel, global->log_level);
		ftp->options
		    = iniGetBitField(list, section, strOptions, ftp_options
		                     , FTP_OPT_INDEX_FILE | FTP_OPT_ALLOW_QWK);

		ftp->bind_retry_count = iniGetInteger(list, section, strBindRetryCount, global->bind_retry_count);
		ftp->bind_retry_delay = iniGetInteger(list, section, strBindRetryDelay, global->bind_retry_delay);
		ftp->login_attempt = get_login_attempt_settings(list, section, global);
		ftp->max_concurrent_connections = iniGetUInteger(list, section, strMaxConConn, 0);
		ftp->max_requests_per_period = iniGetUInteger(list, section, strMaxRequestPerPeriod, 0);
		ftp->request_rate_limit_period = iniGetUInteger(list, section, strRequestRateLimitPeriod, 60 * 60);

	}

	/***********************************************************************/
	section = "Mail";

	if (run_mail != NULL)
		*run_mail = iniGetBool(list, section, strAutoStart, true);

	if (mail != NULL) {

		if (mail->size != sizeof *mail) {
			free(global_interfaces);
			iniFreeStringList(list);
		}
		mail->interfaces
		    = iniGetStringList(list, section, strInterfaces, ",", global_interfaces);
		mail->outgoing4.s_addr
		    = iniGetIpAddress(list, section, strOutgoing4, global->outgoing4.s_addr);
		mail->outgoing6
		    = iniGetIp6Address(list, section, strOutgoing6, global->outgoing6);
		mail->smtp_port
		    = iniGetShortInt(list, section, "SMTPPort", IPPORT_SMTP);
		mail->submission_port
		    = iniGetShortInt(list, section, "SubmissionPort", IPPORT_SUBMISSION);
		mail->submissions_port
		    = iniGetShortInt(list, section, "TLSSubmissionPort", IPPORT_SUBMISSIONS);
		mail->pop3_interfaces
		    = iniGetStringList(list, section, "POP3Interface", ",", global_interfaces);
		mail->pop3_port
		    = iniGetShortInt(list, section, "POP3Port", IPPORT_POP3);
		mail->pop3s_port
		    = iniGetShortInt(list, section, "TLSPOP3Port", IPPORT_POP3S);
		mail->relay_port
		    = iniGetShortInt(list, section, "RelayPort", IPPORT_SMTP);
		mail->max_clients
		    = iniGetShortInt(list, section, strMaxClients, MAIL_DEFAULT_MAX_CLIENTS);
		mail->max_inactivity
		    = (uint16_t)iniGetDuration(list, section, strMaxInactivity, MAIL_DEFAULT_MAX_INACTIVITY);       /* seconds */
		mail->max_delivery_attempts
		    = iniGetUInteger(list, section, "MaxDeliveryAttempts", MAIL_DEFAULT_MAX_DELIVERY_ATTEMPTS);
		mail->rescan_frequency
		    = (uint16_t)iniGetDuration(list, section, "RescanFrequency", MAIL_DEFAULT_RESCAN_FREQUENCY);    /* 60 minutes */
		mail->sem_chk_freq
		    = (int)iniGetDuration(list, section, strSemFileCheckFrequency, global->sem_chk_freq);
		mail->lines_per_yield
		    = iniGetUInteger(list, section, "LinesPerYield", MAIL_DEFAULT_LINES_PER_YIELD);
		mail->max_recipients
		    = iniGetUInteger(list, section, "MaxRecipients", MAIL_DEFAULT_MAX_RECIPIENTS);
		mail->max_msg_size
		    = (DWORD)iniGetBytes(list, section, "MaxMsgSize", /* units: */ 1, MAIL_DEFAULT_MAX_MSG_SIZE);
		mail->max_msgs_waiting
		    = iniGetInteger(list, section, "MaxMsgsWaiting", MAIL_DEFAULT_MAX_MSGS_WAITING);
		mail->connect_timeout
		    = (uint32_t)iniGetDuration(list, section, "ConnectTimeout", MAIL_DEFAULT_CONNECT_TIMEOUT);

		SAFECOPY(mail->host_name
		         , iniGetString(list, section, strHostName, global->host_name, value));

		SAFECOPY(mail->temp_dir
		         , iniGetString(list, section, strTempDirectory, global->temp_dir, value));

		SAFECOPY(mail->login_ars
		         , iniGetString(list, section, strLoginRequirements, global->login_ars, value));
		SAFECOPY(mail->archive_ars
		         , iniGetString(list, section, strArchiveRequirements, nulstr, value));

		SAFECOPY(mail->relay_server
		         , iniGetString(list, section, "RelayServer", nulstr, value));
		SAFECOPY(mail->relay_user
		         , iniGetString(list, section, "RelayUsername", nulstr, value));
		SAFECOPY(mail->relay_pass
		         , iniGetString(list, section, "RelayPassword", nulstr, value));

		SAFECOPY(mail->dns_server
		         , iniGetString(list, section, "DNSServer", nulstr, value));

		SAFECOPY(mail->default_user
		         , iniGetString(list, section, "DefaultUser", nulstr, value));
		SAFECOPY(mail->post_to
		         , iniGetString(list, section, "PostTo", nulstr, value));

		SAFECOPY(mail->dnsbl_hdr
		         , iniGetString(list, section, "DNSBlacklistHeader", "X-DNSBL", value));
		SAFECOPY(mail->dnsbl_tag
		         , iniGetString(list, section, "DNSBlacklistSubject", "SPAM", value));

		SAFECOPY(mail->pop3_sound
		         , iniGetString(list, section, "POP3Sound", nulstr, value));
		SAFECOPY(mail->inbound_sound
		         , iniGetString(list, section, "InboundSound", nulstr, value));
		SAFECOPY(mail->outbound_sound
		         , iniGetString(list, section, "OutboundSound", nulstr, value));
		sbbs_get_sound_settings(list, section, &mail->sound, &global->sound);

		/* JavaScript Operating Parameters */
		sbbs_get_js_settings(list, section, &mail->js, &global->js);

		mail->log_level
		    = iniGetLogLevel(list, section, strLogLevel, global->log_level);
		mail->tls_error_level
		    = iniGetLogLevel(list, section, strTLSErrorLevel, global->tls_error_level);
		mail->options
		    = iniGetBitField(list, section, strOptions, mail_options
		                     , MAIL_OPT_ALLOW_POP3);

		mail->bind_retry_count = iniGetInteger(list, section, strBindRetryCount, global->bind_retry_count);
		mail->bind_retry_delay = iniGetInteger(list, section, strBindRetryDelay, global->bind_retry_delay);
		mail->login_attempt = get_login_attempt_settings(list, section, global);
		mail->max_concurrent_connections = iniGetUInteger(list, section, strMaxConConn, 0);
		mail->max_requests_per_period = iniGetUInteger(list, section, strMaxRequestPerPeriod, 0);
		mail->request_rate_limit_period = iniGetUInteger(list, section, strRequestRateLimitPeriod, 60 * 60);
		mail->spam_block_duration = (uint)iniGetDuration(list, section, "SpamBlockDuration", 0);
		mail->notify_offline_users = iniGetBool(list, section, "NotifyOfflineUsers", false);
	}

	/***********************************************************************/
	section = "Services";

	if (run_services != NULL)
		*run_services = iniGetBool(list, section, strAutoStart, true);

	if (services != NULL) {

		if (services->size != sizeof *services) {
			free(global_interfaces);
			iniFreeStringList(list);
			return false;
		}
		services->interfaces
		    = iniGetStringList(list, section, strInterfaces, ",", global_interfaces);
		services->outgoing4.s_addr
		    = iniGetIpAddress(list, section, strOutgoing4, global->outgoing4.s_addr);
		services->outgoing6
		    = iniGetIp6Address(list, section, strOutgoing6, global->outgoing6);

		services->sem_chk_freq
		    = (int)iniGetDuration(list, section, strSemFileCheckFrequency, global->sem_chk_freq);

		/* JavaScript operating parameters */
		sbbs_get_js_settings(list, section, &services->js, &global->js);

		SAFECOPY(services->host_name
		         , iniGetString(list, section, strHostName, global->host_name, value));

		SAFECOPY(services->temp_dir
		         , iniGetString(list, section, strTempDirectory, global->temp_dir, value));

		SAFECOPY(services->login_ars
		         , iniGetString(list, section, strLoginRequirements, global->login_ars, value));
		SAFECOPY(services->login_info_save
		         , iniGetString(list, section, strLoginInfoSave, "", value));

		SAFECOPY(services->services_ini
		         , iniGetString(list, section, strIniFileName, "services.ini", value));

		sbbs_get_sound_settings(list, section, &services->sound, &global->sound);

		services->log_level
		    = iniGetLogLevel(list, section, strLogLevel, global->log_level);
		services->options
		    = iniGetBitField(list, section, strOptions, service_options
		                     , BBS_OPT_NO_HOST_LOOKUP);

		services->max_connects_per_period = iniGetUInteger(list, section, strMaxConnectsPerPeriod, 0);
		services->connect_rate_limit_period = iniGetUInteger(list, section, strConnectRateLimitPeriod, 60 * 60);

		services->bind_retry_count = iniGetInteger(list, section, strBindRetryCount, global->bind_retry_count);
		services->bind_retry_delay = iniGetInteger(list, section, strBindRetryDelay, global->bind_retry_delay);
		services->login_attempt = get_login_attempt_settings(list, section, global);
	}

	/***********************************************************************/
	section = "Web";

	if (run_web != NULL)
		*run_web = iniGetBool(list, section, strAutoStart, false);

	if (web != NULL) {

		if (web->size != sizeof *web) {
			free(global_interfaces);
			iniFreeStringList(list);
			return false;
		}
		web->interfaces
		    = iniGetStringList(list, section, strInterfaces, ",", global_interfaces);
		web->tls_interfaces
		    = iniGetStringList(list, section, "TLSInterface", ",", global_interfaces);
		web->port
		    = iniGetUInt16(list, section, strPort, IPPORT_HTTP);
		web->tls_port
		    = iniGetUInt16(list, section, "TLSPort", IPPORT_HTTPS);
		web->max_clients
		    = iniGetUInteger(list, section, strMaxClients, WEB_DEFAULT_MAX_CLIENTS);
		web->max_inactivity
		    = (uint16_t)iniGetDuration(list, section, strMaxInactivity, WEB_DEFAULT_MAX_INACTIVITY);        /* seconds */
		web->sem_chk_freq
		    = (int)iniGetDuration(list, section, strSemFileCheckFrequency, global->sem_chk_freq);

		/* JavaScript operating parameters */
		sbbs_get_js_settings(list, section, &web->js, &global->js);

		SAFECOPY(web->host_name
		         , iniGetString(list, section, strHostName, global->host_name, value));

		SAFECOPY(web->temp_dir
		         , iniGetString(list, section, strTempDirectory, global->temp_dir, value));

		SAFECOPY(web->login_ars
		         , iniGetString(list, section, strLoginRequirements, global->login_ars, value));
		SAFECOPY(web->login_info_save
		         , iniGetString(list, section, strLoginInfoSave, "", value));

		SAFECOPY(web->root_dir
		         , iniGetString(list, section, "RootDirectory", WEB_DEFAULT_ROOT_DIR, value));
		SAFECOPY(web->error_dir
		         , iniGetString(list, section, "ErrorDirectory", WEB_DEFAULT_ERROR_DIR, value));
		SAFECOPY(web->cgi_dir
		         , iniGetString(list, section, "CGIDirectory", WEB_DEFAULT_CGI_DIR, value));
		SAFECOPY(web->default_auth_list
		         , iniGetString(list, section, "Authentication", WEB_DEFAULT_AUTH_LIST, value));
		SAFECOPY(web->logfile_base
		         , iniGetString(list, section, "HttpLogFile", nulstr, value));
		SAFECOPY(web->file_index_script
		         , iniGetString(list, section, strFileIndexScript, nulstr, value));
		SAFECOPY(web->file_vpath_prefix
		         , iniGetString(list, section, strFileVPathPrefix, nulstr, value));
		web->file_vpath_for_vhosts = iniGetBool(list, section, strFileVPathForVHosts, false);

		SAFECOPY(web->default_cgi_content
		         , iniGetString(list, section, "DefaultCGIContent", WEB_DEFAULT_CGI_CONTENT, value));

		web->index_file_name
		    = iniGetStringList(list, section, "IndexFileNames", ",", "index.html,index.ssjs");
		web->cgi_ext
		    = iniGetStringList(list, section, "CGIExtensions", ",", ".cgi");
		SAFECOPY(web->ssjs_ext
		         , iniGetString(list, section, "JavaScriptExtension", ".ssjs", value));

		web->max_cgi_inactivity
		    = (uint16_t)iniGetDuration(list, section, "MaxCgiInactivity", WEB_DEFAULT_MAX_CGI_INACTIVITY);  /* seconds */

		sbbs_get_sound_settings(list, section, &web->sound, &global->sound);

		web->log_level
		    = iniGetLogLevel(list, section, strLogLevel, global->log_level);
		web->tls_error_level
		    = iniGetLogLevel(list, section, strTLSErrorLevel, global->tls_error_level);
		web->options
		    = iniGetBitField(list, section, strOptions, web_options
		                     , BBS_OPT_NO_HOST_LOOKUP | WEB_OPT_HTTP_LOGGING | WEB_OPT_NO_CGI);
		web->outbuf_drain_timeout
		    = iniGetUInteger(list, section, "OutbufDrainTimeout", 10);

		web->bind_retry_count = iniGetInteger(list, section, strBindRetryCount, global->bind_retry_count);
		web->bind_retry_delay = iniGetInteger(list, section, strBindRetryDelay, global->bind_retry_delay);
		web->login_attempt = get_login_attempt_settings(list, section, global);
		web->max_concurrent_connections = iniGetUInteger(list, section, strMaxConConn, WEB_DEFAULT_MAX_CON_CONN);
		web->max_requests_per_period = iniGetUInteger(list, section, strMaxRequestPerPeriod, 0);
		web->request_rate_limit_period = iniGetUInteger(list, section, strRequestRateLimitPeriod, 60 * 60);
		SAFECOPY(web->proxy_ip_header
		         , iniGetString(list, section, "RemoteIPHeader", nulstr, value));
		SAFECOPY(web->custom_log_fmt
		         , iniGetString(list, section, "CustomLogFormat", nulstr, value));
	}

	free(global_interfaces);
	iniFreeStringList(list);
	return true;
}

bool sbbs_write_ini(
	FILE*                  fp
	, scfg_t*                cfg
	, global_startup_t*      global
	, bool run_bbs
	, bbs_startup_t*         bbs
	, bool run_ftp
	, ftp_startup_t*         ftp
	, bool run_web
	, web_startup_t*         web
	, bool run_mail
	, mail_startup_t*        mail
	, bool run_services
	, services_startup_t*    services
	)
{
	const char*      section;
	bool             result = false;
	str_list_t       list;
	str_list_t*      lp;
	ini_style_t      style;
	global_startup_t global_buf;

	memset(&style, 0, sizeof(style));
	style.key_prefix = "\t";
	style.section_separator = "";
	style.value_separator = " = ";
	style.bit_separator = " | ";

	if ((list = iniReadFile(fp)) == NULL)
		return false;

	if (global == NULL) {
		memset(&global_buf, 0, sizeof(global_buf));
		global_buf.size = sizeof global_buf;
		if (!get_ini_globals(list, &global_buf))
			return false;
		global = &global_buf;
	}

	lp = &list;

	do { /* try */

		/***********************************************************************/
		if (global != &global_buf) {
			section = "Global";

			iniSetString(lp, section, strCtrlDirectory, global->ctrl_dir, &style);
			iniSetString(lp, section, strTempDirectory, global->temp_dir, &style);
			iniSetString(lp, section, strHostName, global->host_name, &style);
			iniSetString(lp, section, strLoginRequirements, global->login_ars, &style);
			iniSetDuration(lp, section, strSemFileCheckFrequency, global->sem_chk_freq, &style);
			if (global->outgoing4.s_addr != INADDR_ANY)
				iniSetIpAddress(lp, section, strOutgoing4, global->outgoing4.s_addr, &style);
			if (memcmp(&global->outgoing6, &wildcard6, sizeof(wildcard6)) != 0)
				iniSetIp6Address(lp, section, strOutgoing6, global->outgoing6, &style);
			iniSetStringList(lp, section, strInterfaces, ",", global->interfaces, &style);
			iniSetLogLevel(lp, section, strLogLevel, global->log_level, &style);
			iniSetLogLevel(lp, section, strTLSErrorLevel, global->tls_error_level, &style);
			iniSetInteger(lp, section, strBindRetryCount, global->bind_retry_count, &style);
			iniSetInteger(lp, section, strBindRetryDelay, global->bind_retry_delay, &style);
			set_login_attempt_settings(lp, section, global->login_attempt, style);

			/* JavaScript operating parameters */
			if (!sbbs_set_js_settings(lp, section, &global->js, NULL, &style))
				break;

			if (!sbbs_set_sound_settings(lp, section, &global->sound, &global->sound, &style))
				break;
		}

		/***********************************************************************/
		if (bbs != NULL) {

			section = "BBS";

			if (!iniSetBool(lp, section, strAutoStart, run_bbs, &style))
				break;

			if (strListCmp(bbs->telnet_interfaces, global->interfaces) == 0)
				iniRemoveValue(lp, section, "TelnetInterface");
			else if (!iniSetStringList(lp, section, "TelnetInterface", ",", bbs->telnet_interfaces, &style))
				break;

			if (!iniSetUInt16(lp, section, "TelnetPort", bbs->telnet_port, &style))
				break;

			if (strListCmp(bbs->rlogin_interfaces, global->interfaces) == 0)
				iniRemoveValue(lp, section, "RLoginInterface");
			else if (!iniSetStringList(lp, section, "RLoginInterface", ",", bbs->rlogin_interfaces, &style))
				break;
			if (!iniSetUInt16(lp, section, "RLoginPort", bbs->rlogin_port, &style))
				break;

			if (!iniSetUInt16(lp, section, "Pet40Port", bbs->pet40_port, &style))
				break;
			if (!iniSetUInt16(lp, section, "Pet80Port", bbs->pet80_port, &style))
				break;

			if (strListCmp(bbs->ssh_interfaces, global->interfaces) == 0)
				iniRemoveValue(lp, section, "SSHInterface");
			else if (!iniSetStringList(lp, section, "SSHInterface", ",", bbs->ssh_interfaces, &style))
				break;
			if (!iniSetUInt16(lp, section, "SSHPort", bbs->ssh_port, &style))
				break;
			if (!iniSetDuration(lp, section, "SSHConnectTimeout", bbs->ssh_connect_timeout, &style))
				break;
			if (!iniSetLogLevel(lp, section, "SSHErrorLevel", bbs->ssh_error_level, &style))
				break;

			if (!iniSetUInteger(lp, section, "FirstNode", bbs->first_node, &style))
				break;
			if (!iniSetUInteger(lp, section, "LastNode", bbs->last_node, &style))
				break;
			if (!iniSetUInteger(lp, section, "OutbufHighwaterMark", bbs->outbuf_highwater_mark, &style))
				break;
			if (!iniSetUInteger(lp, section, "OutbufDrainTimeout", bbs->outbuf_drain_timeout, &style))
				break;
			if (!iniSetUInteger(lp, section, strMaxConConn, bbs->max_concurrent_connections, &style))
				break;
			if (!iniSetDuration(lp, section, strMaxDumbTermInactivity, bbs->max_dumbterm_inactivity, &style))
				break;
			if (!iniSetDuration(lp, section, strMaxLoginInactivity, bbs->max_login_inactivity, &style))
				break;
			if (!iniSetDuration(lp, section, strMaxNewUserInactivity, bbs->max_newuser_inactivity, &style))
				break;
			if (!iniSetDuration(lp, section, strMaxSessionInactivity, bbs->max_session_inactivity, &style))
				break;
			if (!iniSetDuration(lp, section, strMaxSFTPInactivity, bbs->max_sftp_inactivity, &style))
				break;

			if (bbs->sem_chk_freq == global->sem_chk_freq)
				iniRemoveValue(lp, section, strSemFileCheckFrequency);
			else if (!iniSetDuration(lp, section, strSemFileCheckFrequency, bbs->sem_chk_freq, &style))
				break;

			if (bbs->log_level == global->log_level)
				iniRemoveValue(lp, section, strLogLevel);
			else if (!iniSetLogLevel(lp, section, strLogLevel, bbs->log_level, &style))
				break;
			if (bbs->event_log_level == bbs->log_level)
				iniRemoveValue(lp, section, strEventLogLevel);
			else if (!iniSetLogLevel(lp, section, strEventLogLevel, bbs->event_log_level, &style))
				break;

			/* JavaScript operating parameters */
			if (!sbbs_set_js_settings(lp, section, &bbs->js, &global->js, &style))
				break;

			if (strcmp(bbs->host_name, global->host_name) == 0
			    || strcmp(bbs->host_name, cfg->sys_inetaddr) == 0)
				iniRemoveKey(lp, section, strHostName);
			else if (!iniSetString(lp, section, strHostName, bbs->host_name, &style))
				break;

			if (stricmp(bbs->temp_dir, global->temp_dir) == 0)
				iniRemoveKey(lp, section, strTempDirectory);
			else if (!iniSetString(lp, section, strTempDirectory, bbs->temp_dir, &style))
				break;

			if (stricmp(bbs->login_ars, global->login_ars) == 0)
				iniRemoveKey(lp, section, strLoginRequirements);
			else if (!iniSetString(lp, section, strLoginRequirements, bbs->login_ars, &style))
				break;

			iniSetUInteger(lp, section, "DefaultTermWidth", bbs->default_term_width, &style);
			iniSetUInteger(lp, section, "DefaultTermHeight", bbs->default_term_height, &style);

			if (!iniSetString(lp, section, "ExternalTermANSI", bbs->xtrn_term_ansi, &style))
				break;
			if (!iniSetString(lp, section, "ExternalTermDumb", bbs->xtrn_term_dumb, &style))
				break;
#if defined(__linux__) || defined(__FreeBSD__)
			if (!iniSetString(lp, section, "DOSemuPath", bbs->dosemu_path, &style))
				break;
			if (!iniSetBool(lp, section, "UseDOSemu", bbs->usedosemu, &style))
				break;
#if defined(__linux__)
			if (!iniSetString(lp, section, "DOSemuConfPath", bbs->dosemuconf_path, &style))
				break;
#endif
#endif

			if (!sbbs_set_sound_settings(lp, section, &bbs->sound, &global->sound, &style))
				break;

			if (!iniSetBitField(lp, section, strOptions, bbs_options, bbs->options, &style))
				break;

			if (bbs->bind_retry_count == global->bind_retry_count)
				iniRemoveValue(lp, section, strBindRetryCount);
			else if (!iniSetInteger(lp, section, strBindRetryCount, bbs->bind_retry_count, &style))
				break;
			if (bbs->bind_retry_delay == global->bind_retry_delay)
				iniRemoveValue(lp, section, strBindRetryDelay);
			else if (!iniSetInteger(lp, section, strBindRetryDelay, bbs->bind_retry_delay, &style))
				break;
		}
		/***********************************************************************/
		if (ftp != NULL) {

			section = "FTP";

			if (!iniSetBool(lp, section, strAutoStart, run_ftp, &style))
				break;

			if (strListCmp(ftp->interfaces, global->interfaces) == 0)
				iniRemoveValue(lp, section, strInterfaces);
			else if (!iniSetStringList(lp, section, strInterfaces, ",", ftp->interfaces, &style))
				break;

			if (ftp->outgoing4.s_addr == global->outgoing4.s_addr)
				iniRemoveValue(lp, section, strOutgoing4);
			else if (!iniSetIpAddress(lp, section, strOutgoing4, ftp->outgoing4.s_addr, &style))
				break;

			if (memcmp(&ftp->outgoing6, &global->outgoing6, sizeof(ftp->outgoing6)) == 0)
				iniRemoveValue(lp, section, strOutgoing6);
			else if (!iniSetIp6Address(lp, section, strOutgoing6, ftp->outgoing6, &style))
				break;

			if (!iniSetUInt16(lp, section, strPort, ftp->port, &style))
				break;
			if (!iniSetUInteger(lp, section, strMaxClients, ftp->max_clients, &style))
				break;
			if (!iniSetDuration(lp, section, strMaxInactivity, ftp->max_inactivity, &style))
				break;
			if (!iniSetUInteger(lp, section, strMaxConConn, ftp->max_concurrent_connections, &style))
				break;
			if (!iniSetUInteger(lp, section, strMaxRequestPerPeriod, ftp->max_requests_per_period, &style))
				break;
			if (!iniSetUInteger(lp, section, strRequestRateLimitPeriod, ftp->request_rate_limit_period, &style))
				break;
			if (!iniSetDuration(lp, section, "QwkTimeout", ftp->qwk_timeout, &style))
				break;
			if (!iniSetBytes(lp, section, "MinFileSize", 1, ftp->min_fsize, &style))
				break;
			if (!iniSetBytes(lp, section, "MaxFileSize", 1, ftp->max_fsize, &style))
				break;

			/* Passive transfer settings */
			if (!iniSetIpAddress(lp, section, "PasvIpAddress", ftp->pasv_ip_addr.s_addr, &style))
				break;
			if (!iniSetIp6Address(lp, section, "PasvIp6Address", ftp->pasv_ip6_addr, &style))
				break;
			if (!iniSetUInt16(lp, section, "PasvPortLow", ftp->pasv_port_low, &style))
				break;
			if (!iniSetUInt16(lp, section, "PasvPortHigh", ftp->pasv_port_high, &style))
				break;

			if (ftp->sem_chk_freq == global->sem_chk_freq)
				iniRemoveValue(lp, section, strSemFileCheckFrequency);
			else if (!iniSetDuration(lp, section, strSemFileCheckFrequency, ftp->sem_chk_freq, &style))
				break;

			if (ftp->log_level == global->log_level)
				iniRemoveValue(lp, section, strLogLevel);
			else if (!iniSetLogLevel(lp, section, strLogLevel, ftp->log_level, &style))
				break;

			if (strcmp(ftp->host_name, global->host_name) == 0
			    || (bbs != NULL && strcmp(bbs->host_name, cfg->sys_inetaddr) == 0))
				iniRemoveKey(lp, section, strHostName);
			else if (!iniSetString(lp, section, strHostName, ftp->host_name, &style))
				break;

			if (stricmp(ftp->temp_dir, global->temp_dir) == 0)
				iniRemoveKey(lp, section, strTempDirectory);
			else if (!iniSetString(lp, section, strTempDirectory, ftp->temp_dir, &style))
				break;

			if (stricmp(ftp->login_ars, global->login_ars) == 0)
				iniRemoveKey(lp, section, strLoginRequirements);
			else if (!iniSetString(lp, section, strLoginRequirements, ftp->login_ars, &style))
				break;
			if (!iniSetString(lp, section, strLoginInfoSave, ftp->login_info_save, &style))
				break;

			if (!iniSetString(lp, section, "IndexFileName", ftp->index_file_name, &style))
				break;

			if (!sbbs_set_sound_settings(lp, section, &ftp->sound, &global->sound, &style))
				break;

			if (!iniSetBitField(lp, section, strOptions, ftp_options, ftp->options, &style))
				break;

			if (ftp->bind_retry_count == global->bind_retry_count)
				iniRemoveValue(lp, section, strBindRetryCount);
			else if (!iniSetInteger(lp, section, strBindRetryCount, ftp->bind_retry_count, &style))
				break;
			if (ftp->bind_retry_delay == global->bind_retry_delay)
				iniRemoveValue(lp, section, strBindRetryDelay);
			else if (!iniSetInteger(lp, section, strBindRetryDelay, ftp->bind_retry_delay, &style))
				break;
		}

		/***********************************************************************/
		if (mail != NULL) {

			section = "Mail";

			if (!iniSetBool(lp, section, strAutoStart, run_mail, &style))
				break;

			if (strListCmp(mail->interfaces, global->interfaces) == 0)
				iniRemoveValue(lp, section, strInterfaces);
			else if (!iniSetStringList(lp, section, strInterfaces, ",", mail->interfaces, &style))
				break;

			if (mail->outgoing4.s_addr == global->outgoing4.s_addr)
				iniRemoveValue(lp, section, strOutgoing4);
			else if (!iniSetIpAddress(lp, section, strOutgoing4, mail->outgoing4.s_addr, &style))
				break;

			if (memcmp(&mail->outgoing6, &global->outgoing6, sizeof(ftp->outgoing6)) == 0)
				iniRemoveValue(lp, section, strOutgoing6);
			else if (!iniSetIp6Address(lp, section, strOutgoing6, mail->outgoing6, &style))
				break;

			if (mail->sem_chk_freq == global->sem_chk_freq)
				iniRemoveValue(lp, section, strSemFileCheckFrequency);
			else if (!iniSetDuration(lp, section, strSemFileCheckFrequency, mail->sem_chk_freq, &style))
				break;

			if (mail->log_level == global->log_level)
				iniRemoveValue(lp, section, strLogLevel);
			else if (!iniSetLogLevel(lp, section, strLogLevel, mail->log_level, &style))
				break;
			if (mail->tls_error_level == global->tls_error_level)
				iniRemoveValue(lp, section, strTLSErrorLevel);
			else if (!iniSetLogLevel(lp, section, strTLSErrorLevel, mail->tls_error_level, &style))
				break;
			if (!iniSetUInt16(lp, section, "SMTPPort", mail->smtp_port, &style))
				break;
			if (!iniSetUInt16(lp, section, "SubmissionPort", mail->submission_port, &style))
				break;
			if (!iniSetUInt16(lp, section, "TLSSubmissionPort", mail->submissions_port, &style))
				break;
			if (!iniSetUInt16(lp, section, "POP3Port", mail->pop3_port, &style))
				break;
			if (!iniSetUInt16(lp, section, "TLSPOP3Port", mail->pop3s_port, &style))
				break;
			if (!iniSetUInt16(lp, section, "RelayPort", mail->relay_port, &style))
				break;
			if (!iniSetUInteger(lp, section, strMaxClients, mail->max_clients, &style))
				break;
			if (!iniSetDuration(lp, section, strMaxInactivity, mail->max_inactivity, &style))
				break;
			if (!iniSetUInteger(lp, section, "MaxDeliveryAttempts", mail->max_delivery_attempts, &style))
				break;
			if (!iniSetDuration(lp, section, "RescanFrequency", mail->rescan_frequency, &style))
				break;
			if (!iniSetUInteger(lp, section, "LinesPerYield", mail->lines_per_yield, &style))
				break;
			if (!iniSetUInteger(lp, section, "MaxRecipients", mail->max_recipients, &style))
				break;
			if (!iniSetBytes(lp, section, "MaxMsgSize", /* unit: */ 1, mail->max_msg_size, &style))
				break;
			if (!iniSetInteger(lp, section, "MaxMsgsWaiting", mail->max_msgs_waiting, &style))
				break;
			if (!iniSetDuration(lp, section, "ConnectTimeout", mail->connect_timeout, &style))
				break;
			if (!iniSetUInteger(lp, section, strMaxConConn, mail->max_concurrent_connections, &style))
				break;
			if (!iniSetUInteger(lp, section, strMaxRequestPerPeriod, mail->max_requests_per_period, &style))
				break;
			if (!iniSetUInteger(lp, section, strRequestRateLimitPeriod, mail->request_rate_limit_period, &style))
				break;

			if (strcmp(mail->host_name, global->host_name) == 0
			    || (bbs != NULL && strcmp(bbs->host_name, cfg->sys_inetaddr) == 0))
				iniRemoveKey(lp, section, strHostName);
			else if (!iniSetString(lp, section, strHostName, mail->host_name, &style))
				break;

			if (stricmp(mail->temp_dir, global->temp_dir) == 0)
				iniRemoveKey(lp, section, strTempDirectory);
			else if (!iniSetString(lp, section, strTempDirectory, mail->temp_dir, &style))
				break;

			if (stricmp(mail->login_ars, global->login_ars) == 0)
				iniRemoveKey(lp, section, strLoginRequirements);
			else if (!iniSetString(lp, section, strLoginRequirements, mail->login_ars, &style))
				break;
			if (!iniSetString(lp, section, strArchiveRequirements, mail->archive_ars, &style))
				break;

			if (!iniSetString(lp, section, "RelayServer", mail->relay_server, &style))
				break;
			if (!iniSetString(lp, section, "RelayUsername", mail->relay_user, &style))
				break;
			if (!iniSetString(lp, section, "RelayPassword", mail->relay_pass, &style))
				break;

			if (!iniSetString(lp, section, "DNSServer", mail->dns_server, &style))
				break;

			if (!iniSetString(lp, section, "DefaultUser", mail->default_user, &style))
				break;
			if (!iniSetString(lp, section, "PostTo", mail->post_to, &style))
				break;

			if (!iniSetString(lp, section, "DNSBlacklistHeader", mail->dnsbl_hdr, &style))
				break;
			if (!iniSetString(lp, section, "DNSBlacklistSubject", mail->dnsbl_tag, &style))
				break;

			if (!iniSetString(lp, section, "POP3Sound", mail->pop3_sound, &style))
				break;
			if (!iniSetString(lp, section, "InboundSound", mail->inbound_sound, &style))
				break;
			if (!iniSetString(lp, section, "OutboundSound", mail->outbound_sound, &style))
				break;

			if (!sbbs_set_sound_settings(lp, section, &mail->sound, &global->sound, &style))
				break;

			/* JavaScript Operating Parameters */
			if (!sbbs_set_js_settings(lp, section, &mail->js, &global->js, &style))
				break;

			if (!iniSetBitField(lp, section, strOptions, mail_options, mail->options, &style))
				break;

			if (mail->bind_retry_count == global->bind_retry_count)
				iniRemoveValue(lp, section, strBindRetryCount);
			else if (!iniSetInteger(lp, section, strBindRetryCount, mail->bind_retry_count, &style))
				break;
			if (mail->bind_retry_delay == global->bind_retry_delay)
				iniRemoveValue(lp, section, strBindRetryDelay);
			else if (!iniSetInteger(lp, section, strBindRetryDelay, mail->bind_retry_delay, &style))
				break;
			iniSetDuration(lp, section, "SpamBlockDuration", mail->spam_block_duration, &style);
			iniSetBool(lp, section, "NotifyOfflineUsers", mail->notify_offline_users, &style);
		}

		/***********************************************************************/
		if (services != NULL) {

			section = "Services";

			if (!iniSetBool(lp, section, strAutoStart, run_services, &style))
				break;

			if (strListCmp(services->interfaces, global->interfaces) == 0)
				iniRemoveValue(lp, section, strInterfaces);
			else if (!iniSetStringList(lp, section, strInterfaces, ",", services->interfaces, &style))
				break;

			if (services->outgoing4.s_addr == global->outgoing4.s_addr)
				iniRemoveValue(lp, section, strOutgoing4);
			else if (!iniSetIpAddress(lp, section, strOutgoing4, services->outgoing4.s_addr, &style))
				break;

			if (memcmp(&services->outgoing6, &global->outgoing6, sizeof(ftp->outgoing6)) == 0)
				iniRemoveValue(lp, section, strOutgoing6);
			else if (!iniSetIp6Address(lp, section, strOutgoing6, services->outgoing6, &style))
				break;

			if (services->sem_chk_freq == global->sem_chk_freq)
				iniRemoveValue(lp, section, strSemFileCheckFrequency);
			else if (!iniSetDuration(lp, section, strSemFileCheckFrequency, services->sem_chk_freq, &style))
				break;

			if (services->log_level == global->log_level)
				iniRemoveValue(lp, section, strLogLevel);
			else if (!iniSetLogLevel(lp, section, strLogLevel, services->log_level, &style))
				break;

			/* Configurable JavaScript default parameters */
			if (!sbbs_set_js_settings(lp, section, &services->js, &global->js, &style))
				break;

			if (strcmp(services->host_name, global->host_name) == 0
			    || (bbs != NULL && strcmp(bbs->host_name, cfg->sys_inetaddr) == 0))
				iniRemoveKey(lp, section, strHostName);
			else if (!iniSetString(lp, section, strHostName, services->host_name, &style))
				break;

			if (stricmp(services->temp_dir, global->temp_dir) == 0)
				iniRemoveKey(lp, section, strTempDirectory);
			else if (!iniSetString(lp, section, strTempDirectory, services->temp_dir, &style))
				break;

			if (stricmp(services->login_ars, global->login_ars) == 0)
				iniRemoveKey(lp, section, strLoginRequirements);
			else if (!iniSetString(lp, section, strLoginRequirements, services->login_ars, &style))
				break;
			if (!iniSetString(lp, section, strLoginInfoSave, services->login_info_save, &style))
				break;

			if (!sbbs_set_sound_settings(lp, section, &services->sound, &global->sound, &style))
				break;

			if (!iniSetString(lp, section, strIniFileName, services->services_ini, &style))
				break;

			if (!iniSetBitField(lp, section, strOptions, service_options, services->options, &style))
				break;

			if (services->bind_retry_count == global->bind_retry_count)
				iniRemoveValue(lp, section, strBindRetryCount);
			else if (!iniSetInteger(lp, section, strBindRetryCount, services->bind_retry_count, &style))
				break;
			if (services->bind_retry_delay == global->bind_retry_delay)
				iniRemoveValue(lp, section, strBindRetryDelay);
			else if (!iniSetInteger(lp, section, strBindRetryDelay, services->bind_retry_delay, &style))
				break;

			if (!iniSetUInteger(lp, section, strMaxConnectsPerPeriod, services->max_connects_per_period, &style))
				break;
			if (!iniSetUInteger(lp, section, strConnectRateLimitPeriod, services->connect_rate_limit_period, &style))
				break;
		}

		/***********************************************************************/
		if (web != NULL) {

			section = "Web";

			if (!iniSetBool(lp, section, strAutoStart, run_web, &style))
				break;

			if (strListCmp(web->interfaces, global->interfaces) == 0)
				iniRemoveValue(lp, section, strInterfaces);
			else if (!iniSetStringList(lp, section, strInterfaces, ",", web->interfaces, &style))
				break;

			if (strListCmp(web->tls_interfaces, global->interfaces) == 0)
				iniRemoveValue(lp, section, "TLSInterface");
			else if (!iniSetStringList(lp, section, "TLSInterface", ",", web->tls_interfaces, &style))
				break;
			if (!iniSetUInt16(lp, section, strPort, web->port, &style))
				break;
			if (!iniSetUInt16(lp, section, "TLSPort", web->tls_port, &style))
				break;
			if (!iniSetUInteger(lp, section, strMaxClients, web->max_clients, &style))
				break;
			if (!iniSetDuration(lp, section, strMaxInactivity, web->max_inactivity, &style))
				break;
			if (!iniSetDuration(lp, section, "MaxCgiInactivity", web->max_cgi_inactivity, &style))
				break;

			if (web->sem_chk_freq == global->sem_chk_freq)
				iniRemoveValue(lp, section, strSemFileCheckFrequency);
			else if (!iniSetDuration(lp, section, strSemFileCheckFrequency, web->sem_chk_freq, &style))
				break;

			if (web->log_level == global->log_level)
				iniRemoveValue(lp, section, strLogLevel);
			else if (!iniSetLogLevel(lp, section, strLogLevel, web->log_level, &style))
				break;
			if (web->tls_error_level == global->tls_error_level)
				iniRemoveValue(lp, section, strTLSErrorLevel);
			else if (!iniSetLogLevel(lp, section, strTLSErrorLevel, web->tls_error_level, &style))
				break;

			/* JavaScript Operating Parameters */
			if (!sbbs_set_js_settings(lp, section, &web->js, &global->js, &style))
				break;

			if (strcmp(web->host_name, global->host_name) == 0
			    || (bbs != NULL && strcmp(bbs->host_name, cfg->sys_inetaddr) == 0))
				iniRemoveKey(lp, section, strHostName);
			else if (!iniSetString(lp, section, strHostName, web->host_name, &style))
				break;

			if (stricmp(web->temp_dir, global->temp_dir) == 0)
				iniRemoveKey(lp, section, strTempDirectory);
			else if (!iniSetString(lp, section, strTempDirectory, web->temp_dir, &style))
				break;

			if (stricmp(web->login_ars, global->login_ars) == 0)
				iniRemoveKey(lp, section, strLoginRequirements);
			else if (!iniSetString(lp, section, strLoginRequirements, web->login_ars, &style))
				break;
			if (!iniSetString(lp, section, strLoginInfoSave, web->login_info_save, &style))
				break;

			if (!iniSetString(lp, section, "RootDirectory", web->root_dir, &style))
				break;
			if (!iniSetString(lp, section, "ErrorDirectory", web->error_dir, &style))
				break;
			if (!iniSetString(lp, section, "CGIDirectory", web->cgi_dir, &style))
				break;
			if (!iniSetString(lp, section, "Authentication", web->default_auth_list, &style))
				break;
			if (!iniSetString(lp, section, "HttpLogFile", web->logfile_base, &style))
				break;
			if (!iniSetString(lp, section, strFileIndexScript, web->file_index_script, &style))
				break;
			if (!iniSetString(lp, section, strFileVPathPrefix, web->file_vpath_prefix, &style))
				break;
			if (!iniSetBool(lp, section, strFileVPathForVHosts, web->file_vpath_for_vhosts, &style))
				break;

			if (!iniSetString(lp, section, "DefaultCGIContent", web->default_cgi_content, &style))
				break;

			if (!iniSetStringList(lp, section, "IndexFileNames", ",", web->index_file_name, &style))
				break;
			if (!iniSetStringList(lp, section, "CGIExtensions", ",", web->cgi_ext, &style))
				break;

			if (!iniSetString(lp, section, "JavaScriptExtension", web->ssjs_ext, &style))
				break;

			if (!sbbs_set_sound_settings(lp, section, &web->sound, &global->sound, &style))
				break;

			if (!iniSetBitField(lp, section, strOptions, web_options, web->options, &style))
				break;

			if (web->bind_retry_count == global->bind_retry_count)
				iniRemoveValue(lp, section, strBindRetryCount);
			else if (!iniSetInteger(lp, section, strBindRetryCount, web->bind_retry_count, &style))
				break;
			if (web->bind_retry_delay == global->bind_retry_delay)
				iniRemoveValue(lp, section, strBindRetryDelay);
			else if (!iniSetInteger(lp, section, strBindRetryDelay, web->bind_retry_delay, &style))
				break;
			if (!iniSetUInteger(lp, section, "OutbufDrainTimeout", web->outbuf_drain_timeout, &style))
				break;
			if (!iniSetUInteger(lp, section, strMaxConConn, web->max_concurrent_connections, &style))
				break;
			if (!iniSetUInteger(lp, section, strMaxRequestPerPeriod, web->max_requests_per_period, &style))
				break;
			if (!iniSetUInteger(lp, section, strRequestRateLimitPeriod, web->request_rate_limit_period, &style))
				break;
			if (!iniSetString(lp, section, "RemoteIPHeader", web->proxy_ip_header, &style))
				break;
			if (!iniSetString(lp, section, "CustomLogFormat", web->custom_log_fmt, &style))
				break;
		}

		/***********************************************************************/
		backup(cfg->filename, cfg->config_backup_level, /* rename: */ false);
		result = iniWriteFile(fp, list);

	} while (0); /* finally */

	iniFreeStringList(list);

	return result;
}
