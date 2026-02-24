/* Synchronet configuration library routines */

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

#include "scfglib.h"
#include "load_cfg.h"
#include "nopen.h"
#include "ars_defs.h"
#include "findstr.h"
#include "ini_file.h"
#include "sockwrap.h"    // IPPORT_MQTT
#include "str_util.h"

const char* scfg_addr_list_separator = ",";

bool allocerr(char* error, size_t maxerrlen, const char* fname, const char *item, size_t size)
{
	snprintf(error, maxerrlen, "%s: allocating %u bytes of memory for %s"
	              , fname, (uint)size, item);
	return false;
}

/****************************************************************************/
/* Reads in node .ini and initializes the associated variables				*/
/****************************************************************************/
bool read_node_cfg(scfg_t* cfg, char* error, size_t maxerrlen)
{
	char        errstr[256];
	FILE*       fp;
	str_list_t  ini;
	char        value[INI_MAX_VALUE_LEN];
	bool        result = false;

	const char* fname = "node.ini";
	SAFEPRINTF2(cfg->filename, "%s%s", cfg->node_dir, fname);
	if ((fp = fnopen(NULL, cfg->filename, O_RDONLY)) == NULL) {
		snprintf(error, maxerrlen, "ERROR %d (%s) opening %s", errno, safe_strerror(errno, errstr, sizeof(errstr)), cfg->filename);
		return false;
	}
	ini = iniReadFile(fp);
	fclose(fp);

	if (ini == NULL) {
		snprintf(error, maxerrlen, "Read no contents from %s", cfg->filename);
		result = false;
	} else
		result = true;

	SAFECOPY(cfg->node_phone, iniGetString(ini, ROOT_SECTION, "phone", "", value));
	SAFECOPY(cfg->node_daily_cmd, iniGetString(ini, ROOT_SECTION, "daily", "", value));
	cfg->node_daily_misc = iniGetUInteger(ini, ROOT_SECTION, "daily_settings", 0);
	SAFECOPY(cfg->text_dir, iniGetString(ini, ROOT_SECTION, "text_dir", "../text/", value));
	SAFECOPY(cfg->temp_dir, iniGetString(ini, ROOT_SECTION, "temp_dir", "temp", value));
	SAFECOPY(cfg->node_arstr, iniGetString(ini, ROOT_SECTION, "ars", "", value));
	arstr(NULL, cfg->node_arstr, cfg, cfg->node_ar);

	cfg->node_misc = iniGetUInteger(ini, ROOT_SECTION, "settings", 0);

	iniFreeStringList(ini);

	return result;
}

/****************************************************************************/
/****************************************************************************/
static struct loadable_module read_loadable_mod(scfg_t* cfg, str_list_t ini, const char* name, const char* default_cmd)
{
	struct loadable_module mod = {};
	uint count = 0;
	char* cmd;
	char cmd_key[INI_MAX_VALUE_LEN];
	char ars_key[INI_MAX_VALUE_LEN];
	char value[INI_MAX_VALUE_LEN];

	while(1) {
		if (count < 1) {
			SAFECOPY(cmd_key, name);
			snprintf(ars_key, sizeof ars_key, "%s.ars", name);
		} else {
			snprintf(cmd_key, sizeof cmd_key, "%s.%u", name, count);
			snprintf(ars_key, sizeof ars_key, "%s.%u.ars", name, count);
			default_cmd = NULL;
		}
		if ((cmd = iniGetString(ini, NULL, cmd_key, default_cmd, value)) == NULL)
			break;
		strListPush(&mod.cmd, cmd);
		strListPush(&mod.ars, iniGetString(ini, NULL, ars_key, "", value));
		++count;
	}
	return mod;
}

/****************************************************************************/
/****************************************************************************/
static fevent_t read_fixed_event(scfg_t* cfg, str_list_t ini, const char* name)
{
	fevent_t event = {};
	uint count = 0;
	char* cmd;
	char section[INI_MAX_VALUE_LEN];
	char value[INI_MAX_VALUE_LEN];

	while(1) {
		if (count < 1)
			snprintf(section, sizeof section, "%s_event", name);
		else
			snprintf(section, sizeof section, "%s_event.%u", name, count);
		if ((cmd = iniGetString(ini, section, "cmd", NULL, value)) == NULL)
			break;
		strListPush(&event.cmd, cmd);
		if ((event.misc = realloc_or_free(event.misc, (count + 1) * sizeof (*event.misc))) == NULL) {
			break;
		}
		event.misc[count] = iniGetUInteger(ini, section, "settings", 0);
		++count;
	}
	return event;
}

/****************************************************************************/
/* Reads in main.ini and initializes the associated variables				*/
/****************************************************************************/
bool read_main_cfg(scfg_t* cfg, char* error, size_t maxerrlen)
{
	bool        result = false;
	char        errstr[256];
	FILE*       fp;
	str_list_t  ini = NULL;
	char        value[INI_MAX_VALUE_LEN];
	str_list_t  section;

	const char* fname = "main.ini";
	SAFEPRINTF2(cfg->filename, "%s%s", cfg->ctrl_dir, fname);
	if ((fp = fnopen(NULL, cfg->filename, O_RDONLY)) == NULL) {
		snprintf(error, maxerrlen, "ERROR %d (%s) opening %s", errno, safe_strerror(errno, errstr, sizeof(errstr)), cfg->filename);
	} else {
		ini = iniReadFile(fp);
		fclose(fp);
		if (ini == NULL) {
			snprintf(error, maxerrlen, "No contents read from %s", cfg->filename);
			result = false;
		} else
			result = true;
	}

	SAFECOPY(cfg->sys_name, iniGetString(ini, ROOT_SECTION, "name", "", value));
	SAFECOPY(cfg->sys_op, iniGetString(ini, ROOT_SECTION, "operator", "", value));
	SAFECOPY(cfg->sys_pass, iniGetString(ini, ROOT_SECTION, "password", "", value));
	cfg->sys_pass_timeout = iniGetUInt32(ini, ROOT_SECTION, "password_timeout", 15 /* minutes */);
	SAFECOPY(cfg->sys_id, iniGetString(ini, ROOT_SECTION, "qwk_id", "", value));
	SAFECOPY(cfg->sys_guru, iniGetString(ini, ROOT_SECTION, "guru", "", value));
	SAFECOPY(cfg->sys_location, iniGetString(ini, ROOT_SECTION, "location", "", value));
	SAFECOPY(cfg->sys_phonefmt, iniGetString(ini, ROOT_SECTION, "phonefmt", "", value));
	SAFECOPY(cfg->sys_chat_arstr, iniGetString(ini, ROOT_SECTION, "chat_ars", "", value));
	arstr(NULL, cfg->sys_chat_arstr, cfg, cfg->sys_chat_ar);

	cfg->sys_timezone = iniGetInt16(ini, ROOT_SECTION, "timezone", 0);
	cfg->sys_misc = iniGetUInteger(ini, ROOT_SECTION, "settings", 0);
	cfg->sys_date_fmt = iniGetInteger(ini, ROOT_SECTION, "date_fmt", cfg->sys_misc & SM_EURODATE ? DDMMYY : MMDDYY);
	cfg->sys_date_sep = *iniGetString(ini, NULL, "date_sep", "/", value);
	cfg->sys_vdate_sep = *iniGetString(ini, NULL, "vdate_sep", "'", value);
	cfg->sys_date_verbal = iniGetBool(ini, NULL, "date_verbal", false);
	cfg->sys_login = iniGetUInteger(ini, ROOT_SECTION, "login", 0);
	cfg->sys_pwdays = iniGetInteger(ini, ROOT_SECTION, "pwdays", 0);
	cfg->sys_deldays = iniGetInteger(ini, ROOT_SECTION, "deldays", 0);
	cfg->sys_exp_warn = iniGetInteger(ini, ROOT_SECTION, "exp_warn", 0);
	cfg->sys_autodel = iniGetInteger(ini, ROOT_SECTION, "autodel", 0);
	cfg->cdt_min_value =  iniGetUInt16(ini, ROOT_SECTION, "cdt_min_value", 0);
	cfg->max_minutes = iniGetInteger(ini, ROOT_SECTION, "max_minutes", 0);
	cfg->cdt_per_dollar = (uint32_t)iniGetBytes(ini, ROOT_SECTION, "cdt_per_dollar", 1, 0);
	cfg->guest_msgscan_init = iniGetInteger(ini, ROOT_SECTION, "guest_msgscan_init", 0);
	cfg->hq_password = iniGetBool(ini, ROOT_SECTION, "hq_password", false);
	cfg->min_pwlen = iniGetInteger(ini, ROOT_SECTION, "min_password_length", 0);
	if (cfg->min_pwlen < MIN_PASS_LEN)
		cfg->min_pwlen = MIN_PASS_LEN;
	if (cfg->min_pwlen > LEN_PASS)
		cfg->min_pwlen = LEN_PASS;
	cfg->max_log_size = (uint32_t)iniGetBytes(ini, ROOT_SECTION, "max_log_size", 1, 0);
	cfg->max_logs_kept = iniGetUInt16(ini, ROOT_SECTION, "max_logs_kept", 0);
	cfg->ctrlkey_passthru = iniGetInteger(ini, ROOT_SECTION, "ctrlkey_passthru", 0);
	cfg->max_getkey_inactivity = (uint)iniGetDuration(ini, ROOT_SECTION, "max_getkey_inactivity", 300);
	cfg->inactivity_warn = (uchar)iniGetUInteger(ini, ROOT_SECTION, "inactivity_warn", 75);
	cfg->spinning_pause_prompt = iniGetBool(ini, ROOT_SECTION, "spinning_pause_prompt", true);
	cfg->create_self_signed_cert = iniGetBool(ini, ROOT_SECTION, "create_self_signed_cert", true);

	cfg->user_backup_level = iniGetUInteger(ini, ROOT_SECTION, "user_backup_level", 5);
	cfg->mail_backup_level = iniGetUInteger(ini, ROOT_SECTION, "mail_backup_level", 5);
	cfg->config_backup_level = iniGetUInteger(ini, ROOT_SECTION, "config_backup_level", 5);
	cfg->new_install = iniGetBool(ini, ROOT_SECTION, "new_install", true);
	cfg->valuser = iniGetUInteger(ini, ROOT_SECTION, "valuser", 0);
	cfg->erruser = iniGetUInteger(ini, ROOT_SECTION, "erruser", 0);
	cfg->errlevel = (uchar)iniGetUInteger(ini, ROOT_SECTION, "errlevel", LOG_CRIT);
	cfg->stats_interval = iniGetUInteger(ini, ROOT_SECTION, "stats_interval", 5);
	cfg->cache_filter_files = iniGetUInteger(ini, ROOT_SECTION, "cache_filter_files", 5);

	// fixed events
	cfg->sys_newuser = read_fixed_event(cfg, ini, "newuser");
	cfg->sys_logon = read_fixed_event(cfg, ini, "logon");
	cfg->sys_logout = read_fixed_event(cfg, ini, "logout");
	cfg->sys_daily = read_fixed_event(cfg, ini, "daily");
	cfg->sys_monthly = read_fixed_event(cfg, ini, "monthly");
	cfg->sys_weekly = read_fixed_event(cfg, ini, "weekly");

	named_str_list_t** sections = iniParseSections(ini);

	section = iniGetParsedSection(sections, "node_dir", /* cut: */ true);

	str_list_t         node_dirs = iniGetKeyList(section, NULL);
	cfg->sys_nodes = (uint16_t)strListCount(node_dirs);
	for (size_t i = 0; i < cfg->sys_nodes; i++) {
		SAFECOPY(cfg->node_path[i], iniGetString(section, NULL, node_dirs[i], "", value));
#if defined(__unix__)
		strlwr(cfg->node_path[i]);
#endif
	}
	iniFreeStringList(node_dirs);

	cfg->sys_lastnode = iniGetInteger(ini, ROOT_SECTION, "lastnode", cfg->sys_nodes);

	section = iniGetParsedSection(sections, "dir", /* cut: */ true);
	SAFECOPY(cfg->data_dir, iniGetString(section, NULL, "data", "../data/", value));
	SAFECOPY(cfg->exec_dir, iniGetString(section, NULL, "exec", "../exec/", value));
	SAFECOPY(cfg->mods_dir, iniGetString(section, NULL, "mods", "../mods/", value));
	SAFECOPY(cfg->logs_dir, iniGetString(section, NULL, "logs", cfg->data_dir, value));

	/*********************/
	/* New User Settings */
	/*********************/
	section = iniGetParsedSection(sections, "newuser", /* cut: */ true);
	cfg->uq = iniGetUInteger(section, NULL, "questions", DEFAULT_NEWUSER_QS);

	SAFECOPY(cfg->new_genders, iniGetString(section, NULL, "gender_options", "MFX", value));
	SAFECOPY(cfg->new_pass, iniGetString(section, NULL, "password", "", value));
	SAFECOPY(cfg->new_magic, iniGetString(section, NULL, "magic_word", "", value));
	SAFECOPY(cfg->new_sif, iniGetString(section, NULL, "sif", "", value));
	SAFECOPY(cfg->new_sof, iniGetString(section, NULL, "sof", cfg->new_sif, value));
	cfg->new_prot = *iniGetString(section, NULL, "download_protocol", " ", value);
	char new_shell[LEN_CODE + 1];
	SAFECOPY(new_shell, iniGetString(section, NULL, "command_shell", "default", value));
	SAFECOPY(cfg->new_xedit, iniGetString(section, NULL, "editor", "", value));

	cfg->new_level = iniGetInteger(section, NULL, "level", 50);
	cfg->new_flags1 = iniGetUInt32(section, NULL, "flags1", 0);
	cfg->new_flags2 = iniGetUInt32(section, NULL, "flags2", 0);
	cfg->new_flags3 = iniGetUInt32(section, NULL, "flags3", 0);
	cfg->new_flags4 = iniGetUInt32(section, NULL, "flags4", 0);
	cfg->new_exempt = iniGetUInt32(section, NULL, "exemptions", 0);
	cfg->new_rest = iniGetUInt32(section, NULL, "restrictions", 0);
	cfg->new_cdt = (uint32_t)iniGetBytes(section, NULL, "credits", 1, 0);
	cfg->new_min = iniGetUInteger(section, NULL, "minutes", 0);
	cfg->new_expire = iniGetInteger(section, NULL, "expiration_days", 0);
	cfg->new_misc = iniGetUInt32(section, NULL, "settings", 0);
	cfg->new_chat = iniGetUInt32(section, NULL, "chat_settings", CHAT_DEFAULT);
	cfg->new_qwk = iniGetUInt32(section, NULL, "qwk_settings", QWK_DEFAULT);
	cfg->new_msgscan_init = iniGetInteger(section, NULL, "msgscan_init", 0);

	/*************************/
	/* Expired User Settings */
	/*************************/
	section = iniGetParsedSection(sections, "expired", /* cut: */ true);
	cfg->expired_level = iniGetInteger(section, NULL, "level", 0);
	cfg->expired_flags1 = iniGetUInt32(section, NULL, "flags1", 0);
	cfg->expired_flags2 = iniGetUInt32(section, NULL, "flags2", 0);
	cfg->expired_flags3 = iniGetUInt32(section, NULL, "flags3", 0);
	cfg->expired_flags4 = iniGetUInt32(section, NULL, "flags4", 0);
	cfg->expired_exempt = iniGetUInt32(section, NULL, "exemptions", 0);
	cfg->expired_rest = iniGetUInt32(section, NULL, "restrictions", 0);

	/*****************/
	/* MQTT Settings */
	/*****************/
	section = iniGetParsedSection(sections, "mqtt", /* cut: */ true);
	cfg->mqtt.enabled = iniGetBool(section, NULL, "enabled", false);
	cfg->mqtt.verbose = iniGetBool(section, NULL, "verbose", true);
	SAFECOPY(cfg->mqtt.username, iniGetString(section, NULL, "username", "", value));
	SAFECOPY(cfg->mqtt.password, iniGetString(section, NULL, "password", "", value));
	SAFECOPY(cfg->mqtt.broker_addr, iniGetString(section, NULL, "broker_addr", "127.0.0.1", value));
	cfg->mqtt.broker_port = iniGetUInt16(section, NULL, "broker_port", IPPORT_MQTT);
	cfg->mqtt.keepalive = iniGetIntInRange(section, NULL, "keepalive", 5, 60, INT_MAX); // seconds
	cfg->mqtt.publish_qos = iniGetIntInRange(section, NULL, "publish_qos", 0, 0, 2);
	cfg->mqtt.subscribe_qos = iniGetIntInRange(section, NULL, "subscribe_qos", 0, 2, 2);
	cfg->mqtt.protocol_version = iniGetIntInRange(section, NULL, "protocol_version", 3, 4, 5);
	cfg->mqtt.log_level = iniGetLogLevel(section, NULL, "LogLevel", LOG_INFO);
	cfg->mqtt.tls.mode = iniGetIntInRange(section, NULL, "tls_mode", MQTT_TLS_DISABLED, MQTT_TLS_DISABLED, MQTT_TLS_SBBS);
	SAFECOPY(cfg->mqtt.tls.cafile, iniGetString(section, NULL, "tls_cafile", "", value));
	SAFECOPY(cfg->mqtt.tls.certfile, iniGetString(section, NULL, "tls_certfile", "", value));
	SAFECOPY(cfg->mqtt.tls.keyfile, iniGetString(section, NULL, "tls_keyfile", "", value));
	SAFECOPY(cfg->mqtt.tls.keypass, iniGetString(section, NULL, "tls_keypass", "", value));
	SAFECOPY(cfg->mqtt.tls.psk, iniGetString(section, NULL, "tls_psk", "", value));
	SAFECOPY(cfg->mqtt.tls.identity, iniGetString(section, NULL, "tls_identity", "", value));

	/***********/
	/* Modules */
	/***********/
	section = iniGetParsedSection(sections, "module", /* cut: */ true);
	cfg->logon_mod = read_loadable_mod(cfg, section, "logon", "logon");
	cfg->logoff_mod = read_loadable_mod(cfg, section, "logoff", "");
	cfg->newuser_prompts_mod = read_loadable_mod(cfg, section, "newuser_prompts", "newuser_prompts");
	cfg->newuser_info_mod = read_loadable_mod(cfg, section, "newuser_info", "newuser_info");
	cfg->newuser_mod = read_loadable_mod(cfg, section, "newuser", "newuser");
	cfg->usercfg_mod = read_loadable_mod(cfg, section, "usercfg", "user_settings");
	cfg->login_mod = read_loadable_mod(cfg, section, "login", "login");
	cfg->logout_mod = read_loadable_mod(cfg, section, "logout", "");
	cfg->sync_mod = read_loadable_mod(cfg, section, "sync", "");
	cfg->expire_mod = read_loadable_mod(cfg, section, "expire", "");
	cfg->emailsec_mod = read_loadable_mod(cfg, section, "emailsec", "email_sec");
	cfg->readmail_mod = read_loadable_mod(cfg, section, "readmail", "");
	cfg->scanposts_mod = read_loadable_mod(cfg, section, "scanposts", "");
	cfg->scansubs_mod = read_loadable_mod(cfg, section, "scansubs", "");
	cfg->listmsgs_mod = read_loadable_mod(cfg, section, "listmsgs", "");
	cfg->textsec_mod = read_loadable_mod(cfg, section, "textsec", "text_sec");
	cfg->chatsec_mod = read_loadable_mod(cfg, section, "chatsec", "chat_sec");
	cfg->automsg_mod = read_loadable_mod(cfg, section, "automsg", "automsg");
	cfg->feedback_mod = read_loadable_mod(cfg, section, "feedback", "");
	cfg->xtrnsec_mod = read_loadable_mod(cfg, section, "xtrnsec", "xtrn_sec");
	cfg->nodelist_mod = read_loadable_mod(cfg, section, "nodelist", "nodelist");
	cfg->userlist_mod = read_loadable_mod(cfg, section, "userlist", "");
	cfg->whosonline_mod = read_loadable_mod(cfg, section, "whosonline", "nodelist -active");
	cfg->privatemsg_mod = read_loadable_mod(cfg, section, "privatemsg", "privatemsg");
	cfg->logonlist_mod = read_loadable_mod(cfg, section, "logonlist", "logonlist");
	cfg->prextrn_mod = read_loadable_mod(cfg, section, "prextrn", "prextrn");
	cfg->postxtrn_mod = read_loadable_mod(cfg, section, "postxtrn", "postxtrn");
	cfg->scandirs_mod = read_loadable_mod(cfg, section, "scandirs", "");
	cfg->listfiles_mod = read_loadable_mod(cfg, section, "listfiles", "");
	cfg->fileinfo_mod = read_loadable_mod(cfg, section, "fileinfo", "");
	cfg->batxfer_mod = read_loadable_mod(cfg, section, "batxfer", "batchxfer");
	cfg->tempxfer_mod = read_loadable_mod(cfg, section, "tempxfer", "tempxfer");
	cfg->uselect_mod = read_loadable_mod(cfg, section, "uselect", "");

	/*******************/
	/* Validation Sets */
	/*******************/

	for (uint i = 0; i < 10; i++) {
		char name[128];
		SAFEPRINTF(name, "valset:%u", i);
		section = iniGetParsedSection(sections, name, /* cut: */ true);
		cfg->val_level[i] = iniGetInteger(section, NULL, "level", 0);
		cfg->val_expire[i] = iniGetInteger(section, NULL, "expire", 0);
		cfg->val_flags1[i] = iniGetUInt32(section, NULL, "flags1", 0);
		cfg->val_flags2[i] = iniGetUInt32(section, NULL, "flags2", 0);
		cfg->val_flags3[i] = iniGetUInt32(section, NULL, "flags3", 0);
		cfg->val_flags4[i] = iniGetUInt32(section, NULL, "flags4", 0);
		cfg->val_cdt[i] = (uint32_t)iniGetBytes(section, NULL, "credits", 1, 0);
		cfg->val_exempt[i] = iniGetUInt32(section, NULL, "exemptions", 0);
		cfg->val_rest[i] = iniGetUInt32(section, NULL, "restrictions", 0);
	}

	/***************************/
	/* Security Level Settings */
	/***************************/

	for (uint i = 0; i < 100; i++) {
		char name[128];
		SAFEPRINTF(name, "level:%u", i);
		section = iniGetParsedSection(sections, name, /* cut: */ true);
		cfg->level_timeperday[i] = iniGetInteger(section, NULL, "timeperday", i);
		cfg->level_timepercall[i] = iniGetInteger(section, NULL, "timepercall", i);
		cfg->level_callsperday[i] = iniGetInteger(section, NULL, "callsperday", i);
		cfg->level_linespermsg[i] = iniGetClampedInt(section, NULL, "linespermsg", 1, i, UINT16_MAX);
		cfg->level_postsperday[i] = iniGetInteger(section, NULL, "postsperday", i);
		cfg->level_emailperday[i] = iniGetInteger(section, NULL, "emailperday", i);
		cfg->level_misc[i] = iniGetUInteger(section, NULL, "settings", 0);
		cfg->level_expireto[i] = iniGetClampedInt(section, NULL, "expireto", 0, 0, 99);
		cfg->level_freecdtperday[i] = iniGetBytes(section, NULL, "freecdtperday", 1, 0);
		cfg->level_downloadsperday[i] = iniGetUInt32(section, NULL, "downloadsperday", 0);
	}

	str_list_t shell_list = iniGetParsedSectionList(sections, "shell:");
	cfg->total_shells = strListCount(shell_list);

	if ((cfg->shell = (shell_t **)malloc(sizeof(shell_t *) * cfg->total_shells)) == NULL)
		return allocerr(error, maxerrlen, fname, "shells", sizeof(shell_t *) * cfg->total_shells);

	cfg->new_shell = 0;
	for (int i = 0; i < cfg->total_shells; i++) {
		if ((cfg->shell[i] = (shell_t *)malloc(sizeof(shell_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "shell", sizeof(shell_t));
		memset(cfg->shell[i], 0, sizeof(shell_t));

		const char* name = shell_list[i];
		section = iniGetParsedSection(sections, name, /* cut: */ true);
		SAFECOPY(cfg->shell[i]->code, name + 6);
		SAFECOPY(cfg->shell[i]->name, iniGetString(section, NULL, "name", name + 6, value));
		SAFECOPY(cfg->shell[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->shell[i]->arstr, cfg, cfg->shell[i]->ar);
		cfg->shell[i]->misc = iniGetUInteger(section, NULL, "settings", 0);
		if (stricmp(cfg->shell[i]->code, new_shell) == 0)
			cfg->new_shell = i;
	}

	iniFreeParsedSections(sections);
	iniFreeStringList(shell_list);
	iniFreeStringList(ini);

	return result;
}

/****************************************************************************/
/* Reads in msgs.ini and initializes the associated variables				*/
/****************************************************************************/
bool read_msgs_cfg(scfg_t* cfg, char* error, size_t maxerrlen)
{
	char        errstr[256];
	FILE*       fp;
	str_list_t  ini;
	char        value[INI_MAX_VALUE_LEN];
	bool        result = false;

	const char* fname = "msgs.ini";
	SAFEPRINTF2(cfg->filename, "%s%s", cfg->ctrl_dir, fname);
	if ((fp = fnopen(NULL, cfg->filename, O_RDONLY)) == NULL) {
		snprintf(error, maxerrlen, "ERROR %d (%s) opening %s", errno, safe_strerror(errno, errstr, sizeof(errstr)), cfg->filename);
		return false;
	}
	ini = iniReadFile(fp);
	fclose(fp);

	if (ini == NULL) {
		snprintf(error, maxerrlen, "Read no contents from %s", cfg->filename);
		result = false;
	} else
		result = true;

	/*************************/
	/* General Message Stuff */
	/*************************/
	cfg->msg_misc = iniGetUInteger(ini, ROOT_SECTION, "settings", 0xffff0000);
	cfg->smb_retry_time = iniGetInteger(ini, ROOT_SECTION, "smb_retry_time", 30);

	named_str_list_t** sections = iniParseSections(ini);

	/* QWK stuff */
	str_list_t         section = iniGetParsedSection(sections, "QWK", /* cut: */ true);
	cfg->max_qwkmsgs = iniGetInteger(section, NULL, "max_msgs", 0);
	cfg->max_qwkmsgage = iniGetInteger(section, NULL, "max_age", 0);
	SAFECOPY(cfg->qnet_tagline, iniGetString(section, NULL, "default_tagline", "", value));

	/* E-Mail stuff */
	section = iniGetParsedSection(sections, "mail", /* cut: */ true);
	cfg->mail_maxcrcs = iniGetInteger(section, NULL, "max_crcs", 0);
	cfg->mail_maxage = iniGetInteger(section, NULL, "max_age", 0);
	cfg->max_spamage = iniGetInteger(section, NULL, "max_spam_age", 0);

	/******************/
	/* Message Groups */
	/******************/

	str_list_t grp_list = iniGetParsedSectionList(sections, "grp:");
	cfg->total_grps = strListCount(grp_list);

	if ((cfg->grp = (grp_t **)malloc(sizeof(grp_t *) * cfg->total_grps)) == NULL)
		return allocerr(error, maxerrlen, fname, "groups", sizeof(grp_t *) * cfg->total_grps);

	for (int i = 0; i < cfg->total_grps; i++) {

		const char* name = grp_list[i];
		if ((cfg->grp[i] = (grp_t *)malloc(sizeof(grp_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "group", sizeof(grp_t));
		section = iniGetParsedSection(sections, name, /* cut: */ true);
		memset(cfg->grp[i], 0, sizeof(grp_t));
		SAFECOPY(cfg->grp[i]->sname, name + 4);
		SAFECOPY(cfg->grp[i]->lname, iniGetString(section, NULL, "description", name + 4, value));
		SAFECOPY(cfg->grp[i]->code_prefix, iniGetString(section, NULL, "code_prefix", "", value));
		SAFECOPY(cfg->grp[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		arstr(NULL, cfg->grp[i]->arstr, cfg, cfg->grp[i]->ar);
		cfg->grp[i]->sort = iniGetInteger(section, NULL, "sort", 0);
	}
	iniFreeStringList(grp_list);

	/**********************/
	/* Message Sub-boards */
	/**********************/

	str_list_t sub_list = iniGetParsedSectionList(sections, "sub:");
	cfg->total_subs = strListCount(sub_list);

	if ((cfg->sub = (sub_t **)malloc(sizeof(sub_t *) * cfg->total_subs)) == NULL)
		return allocerr(error, maxerrlen, fname, "subs", sizeof(sub_t *) * cfg->total_subs);

	cfg->total_subs = 0;
	for (uint i = 0; sub_list[i] != NULL; i++) {

		char        group[INI_MAX_VALUE_LEN];
		const char* name = sub_list[i];
		SAFECOPY(group, name + 4);
		char*       p = strrchr(group, ':');
		if (p == NULL)
			continue;
		*p = '\0';
		char*       code = p + 1;
		int         grpnum = getgrpnum_from_name(cfg, group);
		if (!grpnum_is_valid(cfg, grpnum))
			continue;
		if ((cfg->sub[i] = (sub_t *)malloc(sizeof(sub_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "sub", sizeof(sub_t));
		section = iniGetParsedSection(sections, name, /* cut: */ true);
		memset(cfg->sub[i], 0, sizeof(sub_t));
		SAFECOPY(cfg->sub[i]->code_suffix, code);

		cfg->sub[i]->subnum = i;
		cfg->sub[i]->grp = grpnum;
		SAFECOPY(cfg->sub[i]->lname, iniGetString(section, NULL, "description", code, value));
		SAFECOPY(cfg->sub[i]->sname, iniGetString(section, NULL, "name", code, value));
		SAFECOPY(cfg->sub[i]->qwkname, iniGetString(section, NULL, "qwk_name", code, value));
		SAFECOPY(cfg->sub[i]->data_dir, iniGetString(section, NULL, "data_dir", "", value));

		SAFECOPY(cfg->sub[i]->arstr, iniGetString(section, NULL, "ars", "", value));
		SAFECOPY(cfg->sub[i]->read_arstr, iniGetString(section, NULL, "read_ars", "", value));
		SAFECOPY(cfg->sub[i]->post_arstr, iniGetString(section, NULL, "post_ars", "", value));
		SAFECOPY(cfg->sub[i]->op_arstr, iniGetString(section, NULL, "operator_ars", "", value));
		SAFECOPY(cfg->sub[i]->mod_arstr, iniGetString(section, NULL, "moderated_ars", "", value));

		arstr(NULL, cfg->sub[i]->arstr, cfg, cfg->sub[i]->ar);
		arstr(NULL, cfg->sub[i]->read_arstr, cfg, cfg->sub[i]->read_ar);
		arstr(NULL, cfg->sub[i]->post_arstr, cfg, cfg->sub[i]->post_ar);
		arstr(NULL, cfg->sub[i]->op_arstr, cfg, cfg->sub[i]->op_ar);
		arstr(NULL, cfg->sub[i]->mod_arstr, cfg, cfg->sub[i]->mod_ar);

		cfg->sub[i]->misc = iniGetUInteger(section, NULL, "settings", 0);
		if ((cfg->sub[i]->misc & (SUB_FIDO | SUB_INET)) && !(cfg->sub[i]->misc & SUB_QNET))
			cfg->sub[i]->misc |= SUB_NOVOTING;

		SAFECOPY(cfg->sub[i]->tagline, iniGetString(section, NULL, "qwknet_tagline", "", value));
		SAFECOPY(cfg->sub[i]->origline, iniGetString(section, NULL, "fidonet_origin", "", value));
		SAFECOPY(cfg->sub[i]->post_sem, iniGetString(section, NULL, "post_sem", "", value));
		SAFECOPY(cfg->sub[i]->newsgroup, iniGetString(section, NULL, "newsgroup", "", value));
		SAFECOPY(cfg->sub[i]->area_tag, iniGetString(section, NULL, "area_tag", "", value));

		cfg->sub[i]->faddr = smb_atofaddr(NULL, iniGetString(section, NULL, "fidonet_addr", "", value));
		cfg->sub[i]->maxmsgs = iniGetInteger(section, NULL, "max_msgs", 0);
		cfg->sub[i]->maxcrcs = iniGetInteger(section, NULL, "max_crcs", 0);
		cfg->sub[i]->maxage = iniGetInteger(section, NULL, "max_age", 0);
		cfg->sub[i]->ptridx = iniGetInteger(section, NULL, "ptridx", 0);
#ifdef SBBS
		for (uint j = 0; j < i; j++)
			if (cfg->sub[i]->ptridx == cfg->sub[j]->ptridx) {
				snprintf(error, maxerrlen, "%s: Duplicate pointer index for subs %s and %s"
				              , fname
				              , cfg->sub[i]->code_suffix, cfg->sub[j]->code_suffix);
				return false;
			}
#endif


		cfg->sub[i]->qwkconf = iniGetUInt16(section, NULL, "qwk_conf", 0);
		cfg->sub[i]->pmode = iniGetUInteger(section, NULL, "print_mode", 0);
		cfg->sub[i]->n_pmode = iniGetUInteger(section, NULL, "print_mode_neg", 0);
		if (!(cfg->sub[i]->pmode & P_NOXATTRS)) {
			cfg->sub[i]->pmode |= (cfg->sys_misc & SM_XATTR_SUPPORT) << P_XATTR_SHIFT;
			cfg->sub[i]->pmode |= P_NOXATTRS; // mark as "upgraded" to new scheme
		}
		++cfg->total_subs;
	}
	iniFreeStringList(sub_list);

	/***********/
	/* FidoNet */
	/***********/
	section = iniGetParsedSection(sections, "fidonet", /* cut: */ true);
	str_list_t faddr_list = iniGetStringList(section, NULL, "addr_list", scfg_addr_list_separator, "");
	cfg->total_faddrs = strListCount(faddr_list);

	if ((cfg->faddr = (faddr_t *)malloc(sizeof(faddr_t) * cfg->total_faddrs)) == NULL)
		return allocerr(error, maxerrlen, fname, "fido_addrs", sizeof(faddr_t) * cfg->total_faddrs);

	for (int i = 0; i < cfg->total_faddrs; i++)
		cfg->faddr[i] = smb_atofaddr(NULL, faddr_list[i]);
	iniFreeStringList(faddr_list);

	// Sanity-check each sub's FidoNet-style address
	for (int i = 0; i < cfg->total_subs; i++)
		cfg->sub[i]->faddr = *nearest_sysfaddr(cfg, &cfg->sub[i]->faddr);

	SAFECOPY(cfg->origline, iniGetString(section, NULL, "default_origin", "", value));
	SAFECOPY(cfg->netmail_sem, iniGetString(section, NULL, "netmail_sem", "", value));
	SAFECOPY(cfg->echomail_sem, iniGetString(section, NULL, "echomail_sem", "", value));
	SAFECOPY(cfg->netmail_dir, iniGetString(section, NULL, "netmail_dir", "", value));
	cfg->netmail_misc = iniGetUInt16(section, NULL, "netmail_settings", 0);
	cfg->netmail_cost = iniGetUInt32(section, NULL, "netmail_cost", 0);

	/**********/
	/* QWKnet */
	/**********/
	str_list_t qhub_list = iniGetParsedSectionList(sections, "qhub:");
	cfg->total_qhubs = strListCount(qhub_list);

	if ((cfg->qhub = (qhub_t **)malloc(sizeof(qhub_t *) * cfg->total_qhubs)) == NULL)
		return allocerr(error, maxerrlen, fname, "qhubs", sizeof(qhub_t*) * cfg->total_qhubs);

	cfg->total_qhubs = 0;
	for (uint i = 0; qhub_list[i] != NULL; i++) {
		const char* name = qhub_list[i];
		if ((cfg->qhub[i] = (qhub_t *)malloc(sizeof(qhub_t))) == NULL)
			return allocerr(error, maxerrlen, fname, "qhub", sizeof(qhub_t));
		section = iniGetParsedSection(sections, name, /* cut: */ true);
		memset(cfg->qhub[i], 0, sizeof(qhub_t));

		SAFECOPY(cfg->qhub[i]->id, name + 5);
		cfg->qhub[i]->enabled = iniGetBool(section, NULL, "enabled", true);
		cfg->qhub[i]->time = iniGetUInt16(section, NULL, "time", 0);
		cfg->qhub[i]->freq = iniGetUInt16(section, NULL, "freq", 0);
		cfg->qhub[i]->days = (uint8_t)iniGetUInteger(section, NULL, "days", 0);
		cfg->qhub[i]->node = iniGetUInteger(section, NULL, "node_num", 0);
		SAFECOPY(cfg->qhub[i]->call, iniGetString(section, NULL, "call", "", value));
		SAFECOPY(cfg->qhub[i]->pack, iniGetString(section, NULL, "pack", "", value));
		SAFECOPY(cfg->qhub[i]->unpack, iniGetString(section, NULL, "unpack", "", value));
		SAFECOPY(cfg->qhub[i]->fmt, iniGetString(section, NULL, "format", "zip", value));
		cfg->qhub[i]->misc = iniGetUInteger(section, NULL, "settings", 0);

		char       str[128];
		SAFEPRINTF(str, "qhubsub:%s:", cfg->qhub[i]->id);
		str_list_t qsub_list = iniGetParsedSectionList(sections, str);
		uint       k = strListCount(qsub_list);
		if (k) {
			if ((cfg->qhub[i]->sub = (sub_t**)malloc(sizeof(sub_t*) * k)) == NULL)
				return allocerr(error, maxerrlen, fname, "qhub sub", sizeof(sub_t) * k);
			if ((cfg->qhub[i]->conf = (uint16_t *)malloc(sizeof(uint16_t) * k)) == NULL)
				return allocerr(error, maxerrlen, fname, "qhub conf", sizeof(uint16_t) * k);
			if ((cfg->qhub[i]->mode = (uchar *)malloc(sizeof(uchar) * k)) == NULL)
				return allocerr(error, maxerrlen, fname, "qhub mode", sizeof(uchar) * k);
		}

		for (uint j = 0; j < k; j++) {
			uint16_t   confnum;
			int        subnum;
			char       subcode[LEN_EXTCODE + 1];
			uint       mode;
			confnum = atoi(qsub_list[j] + strlen(str));
			str_list_t subsection = iniGetParsedSection(sections, qsub_list[j], /* cut: */ true);
			SAFECOPY(subcode, iniGetString(subsection, NULL, "sub", "", value));
			subnum = getsubnum(cfg, subcode);
			mode = iniGetUInteger(subsection, NULL, "mode", 0);
			if (subnum_is_valid(cfg, subnum)) {
				cfg->sub[subnum]->misc |= SUB_QNET;
				cfg->qhub[i]->sub[cfg->qhub[i]->subs]   = cfg->sub[subnum];
				cfg->qhub[i]->mode[cfg->qhub[i]->subs]  = mode;
				cfg->qhub[i]->conf[cfg->qhub[i]->subs]  = confnum;
				cfg->qhub[i]->subs++;
			}
		}
		iniFreeStringList(qsub_list);
		++cfg->total_qhubs;
	}
	iniFreeStringList(qhub_list);

	/************/
	/* Internet */
	/************/
	section = iniGetParsedSection(sections, "Internet", /* cut: */ true);
	SAFECOPY(cfg->sys_inetaddr, iniGetString(section, NULL, "addr", "", value));
	SAFECOPY(cfg->inetmail_sem, iniGetString(section, NULL, "netmail_sem", "", value));
	SAFECOPY(cfg->smtpmail_sem, iniGetString(section, NULL, "smtp_sem", "", value));
	cfg->inetmail_misc = iniGetUInteger(section, NULL, "netmail_settings", 0);
	cfg->inetmail_cost = iniGetUInt32(section, NULL, "cost", 0);

	iniFreeStringList(ini);
	iniFreeParsedSections(sections);

	return result;
}

void free_node_cfg(scfg_t* cfg)
{
}

void free_main_cfg(scfg_t* cfg)
{
	int i;

	if (cfg->shell != NULL) {
		for (i = 0; i < cfg->total_shells; i++) {
			FREE_AND_NULL(cfg->shell[i]);
		}
		FREE_AND_NULL(cfg->shell);
	}
	cfg->total_shells = 0;
}

void free_msgs_cfg(scfg_t* cfg)
{
	int i;

	if (cfg->grp != NULL) {
		for (i = 0; i < cfg->total_grps; i++) {
			FREE_AND_NULL(cfg->grp[i]);
		}
		FREE_AND_NULL(cfg->grp);
	}
	cfg->total_grps = 0;

	if (cfg->sub != NULL) {
		for (i = 0; i < cfg->total_subs; i++) {
			FREE_AND_NULL(cfg->sub[i]);
		}
		FREE_AND_NULL(cfg->sub);
	}
	cfg->total_subs = 0;

	FREE_AND_NULL(cfg->faddr);
	cfg->total_faddrs = 0;

	if (cfg->qhub != NULL) {
		for (i = 0; i < cfg->total_qhubs; i++) {
			FREE_AND_NULL(cfg->qhub[i]->mode);
			FREE_AND_NULL(cfg->qhub[i]->conf);
			FREE_AND_NULL(cfg->qhub[i]->sub);
			FREE_AND_NULL(cfg->qhub[i]);
		}
		FREE_AND_NULL(cfg->qhub);
	}
	cfg->total_qhubs = 0;

	if (cfg->phub != NULL) {
		for (i = 0; i < cfg->total_phubs; i++) {
			FREE_AND_NULL(cfg->phub[i]);
		}
		FREE_AND_NULL(cfg->phub);
	}
	cfg->total_phubs = 0;
}

/************************************************************/
/* Create data and sub-dirs off data if not already created */
/************************************************************/
void make_data_dirs(scfg_t* cfg)
{
	char str[MAX_PATH + 1];

	md(cfg->data_dir);
	SAFEPRINTF(str, "%ssubs", cfg->data_dir);
	md(str);
	SAFEPRINTF(str, "%sdirs", cfg->data_dir);
	md(str);
	SAFEPRINTF(str, "%stext", cfg->data_dir);
	md(str);
	SAFEPRINTF(str, "%smsgs", cfg->data_dir);
	md(str);
	SAFEPRINTF(str, "%suser", cfg->data_dir);
	md(str);
	SAFEPRINTF(str, "%sqnet", cfg->data_dir);
	md(str);
	SAFEPRINTF(str, "%sfile", cfg->data_dir);
	md(str);

	md(cfg->logs_dir);
	SAFEPRINTF(str, "%slogs", cfg->logs_dir);
	md(str);

	if (cfg->mods_dir[0])
		md(cfg->mods_dir);

	for (int i = 0; i < cfg->total_dirs; i++) {
		md(cfg->dir[i]->data_dir);
		if (cfg->dir[i]->misc & DIR_FCHK)
			md(cfg->dir[i]->path);
	}
}

int getdirnum(scfg_t* cfg, const char* code)
{
	char fullcode[LEN_EXTCODE + 1];
	int  i;

	if (code == NULL || *code == '\0')
		return -1;

	for (i = 0; i < cfg->total_dirs; i++) {
		if (cfg->dir[i]->code[0] == '\0' && cfg->dir[i]->lib < cfg->total_libs) {
			SAFEPRINTF2(fullcode, "%s%s", cfg->lib[cfg->dir[i]->lib]->code_prefix, cfg->dir[i]->code_suffix);
			if (stricmp(fullcode, code) == 0)
				return i;
		} else {
			if (stricmp(cfg->dir[i]->code, code) == 0)
				return i;
		}
	}
	return -1;
}

int getlibnum(scfg_t* cfg, const char* code)
{
	int i = getdirnum(cfg, code);

	if (i >= 0)
		return cfg->dir[i]->lib;
	return i;
}

int getsubnum(scfg_t* cfg, const char* code)
{
	char fullcode[LEN_EXTCODE + 1];
	int  i;

	if (code == NULL || *code == '\0')
		return -1;

	for (i = 0; i < cfg->total_subs; i++) {
		if (cfg->sub[i]->code[0] == '\0' && cfg->sub[i]->grp < cfg->total_grps) {
			SAFEPRINTF2(fullcode, "%s%s", cfg->grp[cfg->sub[i]->grp]->code_prefix, cfg->sub[i]->code_suffix);
			if (stricmp(fullcode, code) == 0)
				return i;
		} else {
			if (stricmp(cfg->sub[i]->code, code) == 0)
				return i;
		}
	}
	return -1;
}

int getgrpnum(scfg_t* cfg, const char* code)
{
	int i = getdirnum(cfg, code);

	if (i >= 0)
		return cfg->sub[i]->grp;
	return i;
}

char* lib_name(scfg_t* cfg, int dirnum)
{
	if (!dirnum_is_valid(cfg, dirnum))
		return "<invalid dir>";
	int libnum = cfg->dir[dirnum]->lib;
	if (!libnum_is_valid(cfg, libnum))
		return "<invalid lib>";
	return cfg->lib[libnum]->sname;
}

char* dir_name(scfg_t* cfg, int dirnum)
{
	if (!dirnum_is_valid(cfg, dirnum))
		return "<invalid dir>";
	return cfg->dir[dirnum]->sname;
}

char* grp_name(scfg_t* cfg, int subnum)
{
	if (!subnum_is_valid(cfg, subnum))
		return "<invalid sub>";
	int grpnum = cfg->sub[subnum]->grp;
	if (!grpnum_is_valid(cfg, grpnum))
		return "<invalid grp>";
	return cfg->grp[grpnum]->sname;
}

char* sub_name(scfg_t* cfg, int subnum)
{
	if (!subnum_is_valid(cfg, subnum))
		return "<invalid sub>";
	return cfg->sub[subnum]->sname;
}

int getgrpnum_from_name(scfg_t* cfg, const char* name)
{
	int i;

	for (i = 0; i < cfg->total_grps; i++) {
		if (stricmp(cfg->grp[i]->sname, name) == 0)
			break;
	}
	return i;
}

int getlibnum_from_name(scfg_t* cfg, const char* name)
{
	int i;

	for (i = 0; i < cfg->total_libs; i++) {
		if (stricmp(cfg->lib[i]->sname, name) == 0)
			break;
	}
	return i;
}

int getxtrnnum(scfg_t* cfg, const char* code)
{
	int i;

	if (*code == '\0')
		return cfg->total_xtrns;
	for (i = 0; i < cfg->total_xtrns; i++) {
		if (stricmp(cfg->xtrn[i]->code, code) == 0)
			break;
	}
	return i;
}

int getxtrnsec(scfg_t* cfg, const char* code)
{
	int i;

	for (i = 0; i < cfg->total_xtrnsecs; i++) {
		if (stricmp(cfg->xtrnsec[i]->code, code) == 0)
			break;
	}
	return i;
}

int getgurunum(scfg_t* cfg, const char* code)
{
	int i;

	for (i = 0; i < cfg->total_gurus; i++) {
		if (stricmp(cfg->guru[i]->code, code) == 0)
			break;
	}
	return i;
}

int getchatactset(scfg_t* cfg, const char* name)
{
	int i;

	for (i = 0; i < cfg->total_actsets; i++) {
		if (stricmp(cfg->actset[i]->name, name) == 0)
			break;
	}
	return i;
}

// Returns 0 (internal editor) or 1-based external editor index
int getxeditnum(scfg_t* cfg, const char* code)
{
	int i;

	for (i = 0; i < cfg->total_xedits; ++i) {
		if (stricmp(cfg->xedit[i]->code, code) == 0)
			return i + 1;
	}
	return 0;
}

// Returns 0 (first shell) if shell code isn't valid
int getshellnum(scfg_t* cfg, const char* code, int dflt)
{
	int i;

	for (i = 0; i < cfg->total_shells; ++i) {
		if (stricmp(cfg->shell[i]->code, code) == 0)
			return i;
	}
	return dflt;
}

int gettextsec(scfg_t* cfg, const char* code)
{
	int i;

	for (i = 0; i < cfg->total_txtsecs; i++) {
		if (stricmp(cfg->txtsec[i]->code, code) == 0)
			break;
	}
	return i;
}

bool dirnum_is_valid(scfg_t* cfg, int dirnum)
{
	return (dirnum >= 0) && (cfg != NULL) && (dirnum < cfg->total_dirs);
}

bool libnum_is_valid(scfg_t* cfg, int libnum)
{
	return (libnum >= 0) && (cfg != NULL) && (libnum < cfg->total_libs);
}

bool subnum_is_valid(scfg_t* cfg, int subnum)
{
	return (subnum >= 0) && (cfg != NULL) && (subnum < cfg->total_subs);
}

bool grpnum_is_valid(scfg_t* cfg, int grpnum)
{
	return (grpnum >= 0) && (cfg != NULL) && (grpnum < cfg->total_grps);
}

bool xtrnnum_is_valid(scfg_t* cfg, int xtrnnum)
{
	return (xtrnnum >= 0) && (cfg != NULL) && (xtrnnum < cfg->total_xtrns);
}

bool xtrnsec_is_valid(scfg_t* cfg, int secnum)
{
	return (secnum >= 0) && (cfg != NULL) && (secnum < cfg->total_xtrnsecs);
}

bool shellnum_is_valid(scfg_t* cfg, int shellnum)
{
	return (shellnum >= 0) && (cfg != NULL) && (shellnum < cfg->total_shells);
}

bool textsec_is_valid(scfg_t* cfg, int secnum)
{
	return (secnum >= 0) && (cfg != NULL) && (secnum < cfg->total_txtsecs);
}

uint nearest_sysfaddr_index(scfg_t* cfg, faddr_t* addr)
{
	int  i;
	uint nearest = 0;
	int  min = INT_MAX;

	for (i = 0; i < cfg->total_faddrs; i++)
		if (memcmp(addr, &cfg->faddr[i], sizeof(*addr)) == 0)
			return i;
	for (i = 0; i < cfg->total_faddrs; i++)
		if (addr->zone == cfg->faddr[i].zone
		    && addr->net == cfg->faddr[i].net
		    && addr->node == cfg->faddr[i].node)
			return i;
	for (i = 0; i < cfg->total_faddrs; i++)
		if (addr->zone == cfg->faddr[i].zone
		    && addr->net == cfg->faddr[i].net)
			return i;
	for (i = 0; i < cfg->total_faddrs; i++) {
		int diff = abs((int)addr->zone - (int)cfg->faddr[i].zone);
		if (diff < min) {
			min = diff;
			nearest = i;
		}
	}
	return nearest;
}

faddr_t* nearest_sysfaddr(scfg_t* cfg, faddr_t* addr)
{
	int i = nearest_sysfaddr_index(cfg, addr);
	if (i < 0 || i >= cfg->total_faddrs)
		return addr;
	return &cfg->faddr[i];
}

char* sub_newsgroup_name(scfg_t* cfg, sub_t* sub, char* str, size_t size)
{
	if (sub->newsgroup[0])
		strlcpy(str, sub->newsgroup, size);
	else
		snprintf(str, size, "%s.%s", cfg->grp[sub->grp]->sname, sub->sname);
	return make_newsgroup_name(str);
}

char* sub_area_tag(scfg_t* cfg, sub_t* sub, char* str, size_t size)
{
	char* p;

	memset(str, 0, size);
	if (sub->area_tag[0])
		strncpy(str, sub->area_tag, size - 1);
	else if (sub->newsgroup[0])
		strncpy(str, sub->newsgroup, size - 1);
	else {
		strncpy(str, sub->sname, size - 1);
		REPLACE_CHARS(str, ' ', '_', p);
	}
	strupr(str);
	return str;
}

char* dir_area_tag(scfg_t* cfg, dir_t* dir, char* str, size_t size)
{
	char* p;

	memset(str, 0, size);
	if (dir->area_tag[0])
		strncpy(str, dir->area_tag, size - 1);
	else {
		strncpy(str, dir->sname, size - 1);
		REPLACE_CHARS(str, ' ', '_', p);
	}
	strupr(str);
	return str;
}

/****************************************************************************/
/* Returns virtual path for a file directory, without trailing slash		*/
/****************************************************************************/
char* dir_vpath(scfg_t* cfg, dir_t* dir, char* path, size_t size)
{
	if (dir->vshortcut[0] != '\0')
		return dir->vshortcut;
	else
		snprintf(path, size, "%s/%s"
		              , cfg->lib[dir->lib]->vdir, dir->vdir);
	return path;
}
