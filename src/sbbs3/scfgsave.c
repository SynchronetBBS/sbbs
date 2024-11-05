/* Synchronet configuration file save routines */

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
#include "scfgsave.h"
#include "load_cfg.h"
#include "smblib.h"
#include "userdat.h"
#include "nopen.h"

bool no_msghdr=false,all_msghdr=false;

/****************************************************************************/
/****************************************************************************/
bool save_cfg(scfg_t* cfg)
{
	int i;

	if(cfg->prepped)
		return(false);

	if(!write_main_cfg(cfg))
		return(false);
	if(!write_msgs_cfg(cfg))
		return(false);
	if(!write_file_cfg(cfg))
		return(false);
	if(!write_chat_cfg(cfg))
		return(false);
	if(!write_xtrn_cfg(cfg))
		return(false);

	for(i=0;i<cfg->sys_nodes;i++) {
		cfg->node_num=i+1;
		if(!write_node_cfg(cfg))
			return(false);
	}

	return(true);
}

/****************************************************************************/
/****************************************************************************/
bool write_node_cfg(scfg_t* cfg)
{
	bool	result = false;
	char	inipath[MAX_PATH+1];
	FILE*	fp;

	if(cfg->prepped)
		return false;

	if(cfg->node_num<1 || cfg->node_num>MAX_NODES)
		return false;

	SAFECOPY(inipath, cfg->node_path[cfg->node_num-1]);
	prep_dir(cfg->ctrl_dir, inipath, sizeof(inipath));
	md(inipath);
	SAFECAT(inipath, "node.ini");
	backup(inipath, cfg->config_backup_level, true);

	str_list_t ini = strListInit();
	iniSetString(&ini, ROOT_SECTION, "phone", cfg->node_phone, NULL);
	iniSetString(&ini, ROOT_SECTION, "daily", cfg->node_daily, NULL);
	iniSetString(&ini, ROOT_SECTION, "text_dir", cfg->text_dir, NULL);
	iniSetString(&ini, ROOT_SECTION, "temp_dir", cfg->temp_dir, NULL);
	iniSetString(&ini, ROOT_SECTION, "ars", cfg->node_arstr, NULL);

	iniSetHexInt(&ini, ROOT_SECTION, "settings", cfg->node_misc, NULL);

	if((fp = fopen(inipath, "w")) != NULL) {
		result = iniWriteFile(fp, ini);
		fclose(fp);
	}
	iniFreeStringList(ini);

	return result;
}

/****************************************************************************/
/****************************************************************************/
bool write_main_cfg(scfg_t* cfg)
{
	bool	result = false;
	char	inipath[MAX_PATH+1];
	char	name[INI_MAX_VALUE_LEN];
	char	tmp[128];
	FILE*	fp;

	if(cfg->prepped)
		return false;

	SAFEPRINTF(inipath, "%smain.ini", cfg->ctrl_dir);
	backup(inipath, cfg->config_backup_level, true);

	str_list_t ini = strListInit();
	iniSetString(&ini, ROOT_SECTION, "name", cfg->sys_name, NULL);
	iniSetString(&ini, ROOT_SECTION, "qwk_id", cfg->sys_id, NULL);
	iniSetString(&ini, ROOT_SECTION, "location", cfg->sys_location, NULL);
	iniSetString(&ini, ROOT_SECTION, "phonefmt", cfg->sys_phonefmt, NULL);
	iniSetString(&ini, ROOT_SECTION, "operator", cfg->sys_op, NULL);
	iniSetString(&ini, ROOT_SECTION, "guru", cfg->sys_guru, NULL);
	iniSetString(&ini, ROOT_SECTION, "password", cfg->sys_pass, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "password_timeout", cfg->sys_pass_timeout, NULL);
	iniSetInt16(&ini, ROOT_SECTION, "timezone", cfg->sys_timezone, NULL);
	iniSetHexInt(&ini, ROOT_SECTION, "settings", cfg->sys_misc, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "date_fmt", cfg->sys_date_fmt, NULL);
	SAFEPRINTF(tmp, "%c", cfg->sys_date_sep);
	iniSetString(&ini, ROOT_SECTION, "date_sep", tmp, NULL);
	iniSetBool(&ini, ROOT_SECTION, "date_verbal", cfg->sys_date_verbal, NULL);
	iniSetHexInt(&ini, ROOT_SECTION, "login", cfg->sys_login, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "lastnode", cfg->sys_lastnode, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "pwdays", cfg->sys_pwdays, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "deldays", cfg->sys_deldays, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "exp_warn", cfg->sys_exp_warn, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "autodel", cfg->sys_autodel, NULL);
	iniSetString(&ini, ROOT_SECTION, "chat_ars", cfg->sys_chat_arstr, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "cdt_min_value", cfg->cdt_min_value, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "max_minutes", cfg->max_minutes, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "cdt_per_dollar", cfg->cdt_per_dollar, NULL);
	iniSetBool(&ini, ROOT_SECTION, "new_install", cfg->new_install, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "guest_msgscan_init", cfg->guest_msgscan_init, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "min_password_length", cfg->min_pwlen, NULL);
	iniSetBytes(&ini, ROOT_SECTION, "max_log_size", 1, cfg->max_log_size, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "max_logs_kept", cfg->max_logs_kept, NULL);
	iniSetHexInt(&ini, ROOT_SECTION, "ctrlkey_passthru", cfg->ctrlkey_passthru, NULL);
	iniSetDuration(&ini, ROOT_SECTION, "max_getkey_inactivity", cfg->max_getkey_inactivity, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "inactivity_warn", cfg->inactivity_warn, NULL);
	iniSetBool(&ini, ROOT_SECTION, "spinning_pause_prompt", cfg->spinning_pause_prompt, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "user_backup_level", cfg->user_backup_level, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "mail_backup_level", cfg->mail_backup_level, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "config_backup_level", cfg->config_backup_level, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "valuser", cfg->valuser, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "erruser", cfg->erruser, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "errlevel", cfg->errlevel, NULL);

	for(uint i=0;i<cfg->sys_nodes;i++) {
		char key[128];
		SAFEPRINTF(key, "%u", i + 1);
		if(cfg->node_path[i][0] == 0)
			SAFEPRINTF(cfg->node_path[i], "../node%u", i + 1);
		iniSetString(&ini, "node_dir", key, cfg->node_path[i], NULL);
	}
	{
		const char* name = "dir";
		backslash(cfg->data_dir);
		backslash(cfg->exec_dir);
		iniSetString(&ini, name, "data", cfg->data_dir, NULL);
		iniSetString(&ini, name, "exec", cfg->exec_dir, NULL);
		iniSetString(&ini, name, "mods", cfg->mods_dir, NULL);
		iniSetString(&ini, name, "logs", cfg->logs_dir, NULL);
	}
	{
		const char* name = "newuser";
		iniSetHexInt(&ini, name, "questions", cfg->uq, NULL);
		iniSetString(&ini, name, "password", cfg->new_pass, NULL);
		iniSetString(&ini, name, "magic_word", cfg->new_magic, NULL);
		iniSetString(&ini, name, "sif", cfg->new_sif, NULL);
		iniSetString(&ini, name, "sof", cfg->new_sof, NULL);

		iniSetUInteger(&ini, name, "level", cfg->new_level, NULL);
		iniSetHexInt(&ini, name, "flags1", cfg->new_flags1, NULL);
		iniSetHexInt(&ini, name, "flags2", cfg->new_flags2, NULL);
		iniSetHexInt(&ini, name, "flags3", cfg->new_flags3, NULL);
		iniSetHexInt(&ini, name, "flags4", cfg->new_flags4, NULL);
		iniSetHexInt(&ini, name, "exemptions", cfg->new_exempt, NULL);
		iniSetHexInt(&ini, name, "restrictions", cfg->new_rest, NULL);
		iniSetUInteger(&ini, name, "credits", cfg->new_cdt, NULL);
		iniSetUInteger(&ini, name, "minutes", cfg->new_min, NULL);
		iniSetString(&ini, name, "editor", cfg->new_xedit, NULL);
		iniSetUInteger(&ini, name, "expiration_days", cfg->new_expire, NULL);
		if(cfg->new_shell>=cfg->total_shells)
			cfg->new_shell=0;
		if(cfg->total_shells > 0)
			iniSetString(&ini, name, "command_shell", cfg->shell[cfg->new_shell]->code, NULL);
		iniSetHexInt(&ini, name, "settings", cfg->new_misc, NULL);
		iniSetHexInt(&ini, name, "chat_settings", cfg->new_chat, NULL);
		iniSetHexInt(&ini, name, "qwk_settings", cfg->new_qwk, NULL);
		SAFEPRINTF(tmp, "%c", cfg->new_prot);
		iniSetString(&ini, name, "download_protocol", tmp, NULL);
		iniSetUInteger(&ini, name, "msgscan_init", cfg->new_msgscan_init, NULL);
		iniSetString(&ini, name, "gender_options", cfg->new_genders, NULL);
	}

	iniSetString(&ini, "logon_event", "cmd", cfg->sys_logon, NULL);
	iniSetString(&ini, "logout_event", "cmd", cfg->sys_logout, NULL);
	iniSetString(&ini, "daily_event", "cmd", cfg->sys_daily, NULL);
	iniSetString(&ini, "monthly_event", "cmd", cfg->sys_monthly, NULL);

	{
		const char* name = "expired";
		iniSetUInteger(&ini, name, "level", cfg->expired_level, NULL);
		iniSetHexInt(&ini, name, "flags1", cfg->expired_flags1, NULL);
		iniSetHexInt(&ini, name, "flags2", cfg->expired_flags2, NULL);
		iniSetHexInt(&ini, name, "flags3", cfg->expired_flags3, NULL);
		iniSetHexInt(&ini, name, "flags4", cfg->expired_flags4, NULL);
		iniSetHexInt(&ini, name, "exemptions", cfg->expired_exempt, NULL);
		iniSetHexInt(&ini, name, "restrictions", cfg->expired_rest, NULL);
	}

	{
		const char* name = "MQTT";
		iniSetBool(&ini, name, "Enabled", cfg->mqtt.enabled, NULL);
		iniSetBool(&ini, name, "Verbose", cfg->mqtt.verbose, NULL);
		iniSetString(&ini, name, "Broker_addr", cfg->mqtt.broker_addr, NULL);
		iniSetUInt16(&ini, name, "Broker_port", cfg->mqtt.broker_port, NULL);
		iniSetInteger(&ini, name, "Protocol_version", cfg->mqtt.protocol_version, NULL);
		iniSetInteger(&ini, name, "Keepalive", cfg->mqtt.keepalive, NULL);
		iniSetInteger(&ini, name, "Publish_QOS", cfg->mqtt.publish_qos, NULL);
		iniSetInteger(&ini, name, "Subscribe_QOS", cfg->mqtt.subscribe_qos, NULL);
		iniSetString(&ini, name, "Username", cfg->mqtt.username, NULL);
		iniSetString(&ini, name, "Password", cfg->mqtt.password, NULL);
		iniSetLogLevel(&ini, name , "LogLevel", cfg->mqtt.log_level, NULL);
		// TLS
		iniSetInteger(&ini, name, "TLS_mode", cfg->mqtt.tls.mode, NULL);
		iniSetString(&ini, name, "TLS_cafile", cfg->mqtt.tls.cafile, NULL);
		iniSetString(&ini, name, "TLS_certfile", cfg->mqtt.tls.certfile, NULL);
		iniSetString(&ini, name, "TLS_keyfile", cfg->mqtt.tls.keyfile, NULL);
		iniSetString(&ini, name, "TLS_keypass", cfg->mqtt.tls.keypass, NULL);
		iniSetString(&ini, name, "TLS_psk", cfg->mqtt.tls.psk, NULL);
		iniSetString(&ini, name, "TLS_identity", cfg->mqtt.tls.identity, NULL);
	}

	{
		const char* name = "module";
		iniSetString(&ini, name, "logon", cfg->logon_mod, NULL);
		iniSetString(&ini, name, "logoff", cfg->logoff_mod, NULL);
		iniSetString(&ini, name, "newuser", cfg->newuser_mod, NULL);
		iniSetString(&ini, name, "usercfg", cfg->usercfg_mod, NULL);
		iniSetString(&ini, name, "login", cfg->login_mod, NULL);
		iniSetString(&ini, name, "logout", cfg->logout_mod, NULL);
		iniSetString(&ini, name, "sync", cfg->sync_mod, NULL);
		iniSetString(&ini, name, "expire", cfg->expire_mod, NULL);
		iniSetString(&ini, name, "readmail", cfg->readmail_mod, NULL);
		iniSetString(&ini, name, "scanposts", cfg->scanposts_mod, NULL);
		iniSetString(&ini, name, "scansubs", cfg->scansubs_mod, NULL);
		iniSetString(&ini, name, "listmsgs", cfg->listmsgs_mod, NULL);
		iniSetString(&ini, name, "textsec", cfg->textsec_mod, NULL);
		iniSetString(&ini, name, "chatsec", cfg->chatsec_mod, NULL);
		iniSetString(&ini, name, "automsg", cfg->automsg_mod, NULL);
		iniSetString(&ini, name, "feedback", cfg->feedback_mod, NULL);
		iniSetString(&ini, name, "userlist", cfg->userlist_mod, NULL);

		iniSetString(&ini, name, "nodelist", cfg->nodelist_mod, NULL);
		iniSetString(&ini, name, "whosonline", cfg->whosonline_mod, NULL);
		iniSetString(&ini, name, "privatemsg", cfg->privatemsg_mod, NULL);
		iniSetString(&ini, name, "logonlist", cfg->logonlist_mod, NULL);

		iniSetString(&ini, name, "xtrnsec", cfg->xtrnsec_mod, NULL);
		iniSetString(&ini, name, "prextrn", cfg->prextrn_mod, NULL);
		iniSetString(&ini, name, "postxtrn", cfg->postxtrn_mod, NULL);

		iniSetString(&ini, name, "scandirs", cfg->scandirs_mod, NULL);
		iniSetString(&ini, name, "listfiles", cfg->listfiles_mod, NULL);
		iniSetString(&ini, name, "fileinfo", cfg->fileinfo_mod, NULL);
		iniSetString(&ini, name, "batxfer", cfg->batxfer_mod, NULL);
		iniSetString(&ini, name, "tempxfer", cfg->tempxfer_mod, NULL);
	}

	for(uint i=0; i<10; i++) {
		SAFEPRINTF(name, "valset:%u", i);
		str_list_t section = strListInit();
		iniSetUInteger(&section, name, "level", cfg->val_level[i], NULL);
		iniSetUInteger(&section, name, "expire", cfg->val_expire[i], NULL);
		iniSetHexInt(&section, name, "flags1", cfg->val_flags1[i], NULL);
		iniSetHexInt(&section, name, "flags2", cfg->val_flags2[i], NULL);
		iniSetHexInt(&section, name, "flags3", cfg->val_flags3[i], NULL);
		iniSetHexInt(&section, name, "flags4", cfg->val_flags4[i], NULL);
		iniSetUInteger(&section, name, "credits", cfg->val_cdt[i], NULL);
		iniSetHexInt(&section, name, "exemptions", cfg->val_exempt[i], NULL);
		iniSetHexInt(&section, name, "restrictions", cfg->val_rest[i], NULL);
		strListMerge(&ini, section);
		free(section);
	}

	for(uint i=0; i<100; i++) {
		SAFEPRINTF(name, "level:%u", i);
		str_list_t section = strListInit();
		iniSetUInteger(&section, name, "timeperday", cfg->level_timeperday[i], NULL);
		iniSetUInteger(&section, name, "timepercall", cfg->level_timepercall[i], NULL);
		iniSetUInteger(&section, name, "callsperday", cfg->level_callsperday[i], NULL);
		iniSetUInteger(&section, name, "linespermsg", cfg->level_linespermsg[i], NULL);
		iniSetUInteger(&section, name, "postsperday", cfg->level_postsperday[i], NULL);
		iniSetUInteger(&section, name, "emailperday", cfg->level_emailperday[i], NULL);
		iniSetHexInt(&section, name, "settings", cfg->level_misc[i], NULL);
		iniSetUInteger(&section, name, "expireto", cfg->level_expireto[i], NULL);
		iniSetBytes(&section, name, "freecdtperday", 1, cfg->level_freecdtperday[i], NULL);
		strListMerge(&ini, section);
		free(section);
	}

	/* Command Shells */
	for(int i=0; i<cfg->total_shells; i++) {
		SAFEPRINTF(name, "shell:%s", cfg->shell[i]->code);
		str_list_t section = strListInit();
		iniSetString(&section, name, "name", cfg->shell[i]->name, NULL);
		iniSetString(&section, name, "ars", cfg->shell[i]->arstr, NULL);
		iniSetHexInt(&section, name, "settings", cfg->shell[i]->misc, NULL);
		strListMerge(&ini, section);
		free(section);
	}

	if((fp = fopen(inipath, "w")) != NULL) {
		result = iniWriteFile(fp, ini);
		fclose(fp);
	}
	iniFreeStringList(ini);

	return result;
}

/****************************************************************************/
/****************************************************************************/
bool write_msgs_cfg(scfg_t* cfg)
{
	bool	result = false;
	char	path[MAX_PATH+1];
	char	inipath[MAX_PATH+1];
	char	name[INI_MAX_VALUE_LEN];
	char	tmp[INI_MAX_VALUE_LEN];
	FILE*	fp;
	smb_t	smb;

	if(cfg->prepped)
		return false;

	ZERO_VAR(smb);

	SAFEPRINTF(inipath, "%smsgs.ini", cfg->ctrl_dir);
	backup(inipath, cfg->config_backup_level, true);

	str_list_t ini = strListInit();
	iniSetHexInt(&ini, ROOT_SECTION, "settings", cfg->msg_misc, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "smb_retry_time", cfg->smb_retry_time, NULL);

	{
		const char* name = "mail";
		iniSetUInteger(&ini, name, "max_crcs", cfg->mail_maxcrcs, NULL);
		iniSetUInteger(&ini, name, "max_age", cfg->mail_maxage, NULL);
		iniSetUInteger(&ini, name, "max_spam_age", cfg->max_spamage, NULL);
	}

	{
		const char* name = "qwk";
		iniSetUInteger(&ini, name, "max_msgs", cfg->max_qwkmsgs, NULL);
		iniSetString(&ini, name, "prepack_ars", cfg->preqwk_arstr, NULL);
		iniSetUInteger(&ini, name, "max_age", cfg->max_qwkmsgage, NULL);
		iniSetString(&ini, name, "default_tagline", cfg->qnet_tagline, NULL);
	}

	/* Message Groups */

	for(int i=0;i<cfg->total_grps;i++) {
		SAFEPRINTF(name, "grp:%s", cfg->grp[i]->sname);
		str_list_t section = strListInit();
		iniSetString(&section, name, "description", cfg->grp[i]->lname, NULL);
		iniSetString(&section, name, "ars", cfg->grp[i]->arstr, NULL);
		iniSetString(&section, name, "code_prefix", cfg->grp[i]->code_prefix, NULL);
		iniSetUInteger(&section, name, "sort", cfg->grp[i]->sort, NULL);
		strListMerge(&ini, section);
		free(section);
	}

	/* Message Sub-boards */

	for(int grp = 0; grp < cfg->total_grps; grp++) {
		for(int i=0; i<cfg->total_subs; i++) {
			if(cfg->sub[i]->lname[0] == 0
				|| cfg->sub[i]->sname[0] == 0
				|| cfg->sub[i]->code_suffix[0] == 0)
				continue;
			if(cfg->sub[i]->grp != grp)
				continue;
			SAFEPRINTF2(name, "sub:%s:%s"
				,cfg->grp[grp]->sname, cfg->sub[i]->code_suffix);
			str_list_t section = strListInit();
			iniSetString(&section, name, "description", cfg->sub[i]->lname, NULL);
			iniSetString(&section, name, "name", cfg->sub[i]->sname, NULL);
			iniSetString(&section, name, "qwk_name", cfg->sub[i]->qwkname, NULL);
	#if 1
			if(cfg->sub[i]->data_dir[0]) {
				backslash(cfg->sub[i]->data_dir);
				md(cfg->sub[i]->data_dir);
			}
	#endif
			iniSetString(&section, name, "data_dir", cfg->sub[i]->data_dir, NULL);
			iniSetString(&section, name, "ars", cfg->sub[i]->arstr, NULL);
			iniSetString(&section, name, "read_ars", cfg->sub[i]->read_arstr, NULL);
			iniSetString(&section, name, "post_ars", cfg->sub[i]->post_arstr, NULL);
			iniSetString(&section, name, "operator_ars", cfg->sub[i]->op_arstr, NULL);
			iniSetHexInt(&section, name, "settings", cfg->sub[i]->misc&(~SUB_HDRMOD), NULL);    /* Don't write mod bit */
			iniSetString(&section, name, "qwknet_tagline", cfg->sub[i]->tagline, NULL);
			iniSetString(&section, name, "fidonet_origin", cfg->sub[i]->origline, NULL);
			iniSetString(&section, name, "post_sem", cfg->sub[i]->post_sem, NULL);
			iniSetString(&section, name, "newsgroup", cfg->sub[i]->newsgroup, NULL);
			iniSetString(&section, name, "fidonet_addr", smb_faddrtoa(&cfg->sub[i]->faddr, tmp), NULL);
			iniSetUInteger(&section, name, "max_msgs", cfg->sub[i]->maxmsgs, NULL);
			iniSetUInteger(&section, name, "max_crcs", cfg->sub[i]->maxcrcs, NULL);
			iniSetUInteger(&section, name, "max_age", cfg->sub[i]->maxage, NULL);
			iniSetUInteger(&section, name, "ptridx", cfg->sub[i]->ptridx, NULL);
			iniSetString(&section, name, "moderated_ars", cfg->sub[i]->mod_arstr, NULL);
			iniSetUInteger(&section, name, "qwk_conf", cfg->sub[i]->qwkconf, NULL);
			iniSetHexInt(&section, name, "print_mode", cfg->sub[i]->pmode, NULL);
			iniSetHexInt(&section, name, "print_mode_neg", cfg->sub[i]->n_pmode, NULL);
			iniSetString(&section, name, "area_tag", cfg->sub[i]->area_tag, NULL);
			strListMerge(&ini, section);
			free(section);

			if(all_msghdr || (cfg->sub[i]->misc&SUB_HDRMOD && !no_msghdr)) {
				if(!cfg->sub[i]->data_dir[0])
					SAFEPRINTF(smb.file,"%ssubs",cfg->data_dir);
				else
					SAFECOPY(smb.file,cfg->sub[i]->data_dir);
				prep_dir(cfg->ctrl_dir,smb.file,sizeof(smb.file));
				md(smb.file);
				SAFEPRINTF2(path,"%s%s"
					,cfg->grp[cfg->sub[i]->grp]->code_prefix
					,cfg->sub[i]->code_suffix);
				strlwr(path);
				SAFECAT(smb.file,path);
				if(smb_open(&smb) != SMB_SUCCESS) {
					result = false;
					continue;
				}
				if(!filelength(fileno(smb.shd_fp))) {
					smb.status.max_crcs=cfg->sub[i]->maxcrcs;
					smb.status.max_msgs=cfg->sub[i]->maxmsgs;
					smb.status.max_age=cfg->sub[i]->maxage;
					smb.status.attr=cfg->sub[i]->misc&SUB_HYPER ? SMB_HYPERALLOC :0;
					if(smb_create(&smb)!=0)
						/* errormsg(WHERE,ERR_CREATE,smb.file,x) */;
					smb_close(&smb);
					continue; 
				}
				if(smb_locksmbhdr(&smb)!=0) {
					smb_close(&smb);
					/* errormsg(WHERE,ERR_LOCK,smb.file,x) */;
					continue; 
				}
				if(smb_getstatus(&smb)!=0) {
					smb_close(&smb);
					/* errormsg(WHERE,ERR_READ,smb.file,x) */;
					continue; 
				}
				if((!(cfg->sub[i]->misc&SUB_HYPER) || smb.status.attr&SMB_HYPERALLOC)
					&& smb.status.max_msgs==cfg->sub[i]->maxmsgs
					&& smb.status.max_crcs==cfg->sub[i]->maxcrcs
					&& smb.status.max_age==cfg->sub[i]->maxage) {	/* No change */
					smb_close(&smb);
					continue; 
				}
				smb.status.max_msgs=cfg->sub[i]->maxmsgs;
				smb.status.max_crcs=cfg->sub[i]->maxcrcs;
				smb.status.max_age=cfg->sub[i]->maxage;
				if(cfg->sub[i]->misc&SUB_HYPER)
					smb.status.attr|=SMB_HYPERALLOC;
				if(smb_putstatus(&smb)!=0) {
					smb_close(&smb);
					/* errormsg(WHERE,ERR_WRITE,smb.file,x); */
					continue; 
				}
				smb_close(&smb); 
			}
		}
	}

	/* FidoNet */
	{
		const char* name = "FidoNet";
		str_list_t section = strListInit();
		str_list_t addr_list = strListInit();
		for(int i=0; i<cfg->total_faddrs; i++)
			strListPush(&addr_list, smb_faddrtoa(&cfg->faddr[i], tmp));
		iniSetStringList(&section, name, "addr_list", ",", addr_list, NULL);
		strListFree(&addr_list);

		iniSetString(&section, name, "default_origin", cfg->origline, NULL);
		iniSetString(&section, name, "netmail_sem", cfg->netmail_sem, NULL);
		iniSetString(&section, name, "echomail_sem", cfg->echomail_sem, NULL);
		backslash(cfg->netmail_dir);
		iniSetString(&section, name, "netmail_dir", cfg->netmail_dir, NULL);
		iniSetHexInt(&section, name, "netmail_settings", cfg->netmail_misc, NULL);
		iniSetUInteger(&section, name, "netmail_cost", cfg->netmail_cost, NULL);
		md(cfg->netmail_dir);
		strListMerge(&ini, section);
		free(section);
	}

	/* QWKnet Config */
	for(int i=0; i<cfg->total_qhubs; i++) {
		SAFEPRINTF(name, "qhub:%s", cfg->qhub[i]->id);
		str_list_t section = strListInit();
		iniSetBool(&section, name, "enabled", cfg->qhub[i]->enabled, NULL);
		iniSetUInteger(&section, name, "time", cfg->qhub[i]->time, NULL);
		iniSetUInteger(&section, name, "freq", cfg->qhub[i]->freq, NULL);
		iniSetUInteger(&section, name, "days", cfg->qhub[i]->days, NULL);
		iniSetUInteger(&section, name, "node_num", cfg->qhub[i]->node, NULL);
		iniSetString(&section, name, "call", cfg->qhub[i]->call, NULL);
		iniSetString(&section, name, "pack", cfg->qhub[i]->pack, NULL);
		iniSetString(&section, name, "unpack", cfg->qhub[i]->unpack, NULL);
		iniSetHexInt(&section, name, "settings", cfg->qhub[i]->misc, NULL);
		iniSetString(&section, name, "format", cfg->qhub[i]->fmt, NULL);
		strListMerge(&ini, section);
		free(section);
		for(int j=0; j<cfg->qhub[i]->subs; j++) {
			sub_t* sub = cfg->qhub[i]->sub[j];
			if(sub == NULL)
				continue;
			SAFEPRINTF2(name, "qhubsub:%s:%u", cfg->qhub[i]->id, cfg->qhub[i]->conf[j]);
			str_list_t section = strListInit();
			char code[LEN_EXTCODE + 1];
			SAFEPRINTF2(code,"%s%s"
				,cfg->grp[sub->grp]->code_prefix
				,sub->code_suffix);
			iniSetString(&section, name, "sub", code, NULL);
			iniSetHexInt(&section, name, "settings", cfg->qhub[i]->mode[j], NULL);
			strListMerge(&ini, section);
			free(section);
		}
	}

	{
		const char* name = "Internet";
		str_list_t section = strListInit();
		iniSetString(&section, name, "addr", cfg->sys_inetaddr, NULL); /* Internet address */
		iniSetString(&section, name, "netmail_sem", cfg->inetmail_sem, NULL);
		iniSetHexInt(&section, name, "netmail_settings", cfg->inetmail_misc, NULL);
		iniSetUInteger(&section, name, "cost", cfg->inetmail_cost, NULL);
		iniSetString(&section, name, "smtp_sem", cfg->smtpmail_sem, NULL);
		strListMerge(&ini, section);
		free(section);
	}

	if((fp = fopen(inipath, "w")) != NULL) {
		result = iniWriteFile(fp, ini);
		fclose(fp);
	}
	iniFreeStringList(ini);


	/* MUST BE AT END */

	if(!no_msghdr) {
		char dir[MAX_PATH + 1];
		SAFECOPY(dir, cfg->data_dir);
		prep_dir(cfg->ctrl_dir,dir,sizeof(dir));
		md(dir);
		SAFEPRINTF(smb.file,"%smail",dir);
		if(smb_open(&smb)!=0) {
			return(false); 
		}
		if(!filelength(fileno(smb.shd_fp))) {
			smb.status.max_msgs=0;
			smb.status.max_crcs=cfg->mail_maxcrcs;
			smb.status.max_age=cfg->mail_maxage;
			smb.status.attr=SMB_EMAIL;
			int i=smb_create(&smb);
			smb_close(&smb);
			return(i == SMB_SUCCESS);
		}
		if(smb_locksmbhdr(&smb)!=0) {
			smb_close(&smb);
			return(false); 
		}
		if(smb_getstatus(&smb)!=0) {
			smb_close(&smb);
			return(false); 
		}
		smb.status.max_msgs=0;
		smb.status.max_crcs=cfg->mail_maxcrcs;
		smb.status.max_age=cfg->mail_maxage;
		if(smb_putstatus(&smb)!=0) {
			smb_close(&smb);
			return(false); 
		}
		smb_close(&smb); 
	}

	return result;
}

/****************************************************************************/
/* Write the settings common between dirs and lib.dir_defaults				*/
/****************************************************************************/
static void write_dir_defaults_cfg(str_list_t* ini, const char* section, dir_t* dir)
{
	iniSetString(ini, section, "data_dir", dir->data_dir, NULL);
	iniSetString(ini, section, "upload_sem", dir->upload_sem, NULL);
	iniSetString(ini, section, "extensions", dir->exts, NULL);
	iniSetHexInt(ini, section, "settings", dir->misc, NULL);
	iniSetUInteger(ini, section, "seq_dev", dir->seqdev, NULL);
	iniSetUInteger(ini, section, "sort", dir->sort, NULL);
	iniSetUInteger(ini, section, "max_age", dir->maxage, NULL);
	iniSetUInteger(ini, section, "max_files", dir->maxfiles, NULL);
	iniSetUInteger(ini, section, "upload_credit_pct", dir->up_pct, NULL);
	iniSetUInteger(ini, section, "download_credit_pct", dir->dn_pct, NULL);
}

/****************************************************************************/
/****************************************************************************/
bool write_file_cfg(scfg_t* cfg)
{
	bool	result = false;
	char	path[MAX_PATH+1];
	char	inipath[MAX_PATH+1];
	char	name[INI_MAX_VALUE_LEN];

	if(cfg->prepped)
		return false;

	SAFEPRINTF(inipath, "%sfile.ini", cfg->ctrl_dir);
	backup(inipath, cfg->config_backup_level, true);

	str_list_t ini = strListInit();
	iniSetBytes(&ini, ROOT_SECTION, "min_dspace", 1, cfg->min_dspace, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "max_batup", cfg->max_batup, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "max_batdn", cfg->max_batdn, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "max_userxfer" ,cfg->max_userxfer, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "upload_credit_pct", cfg->cdt_up_pct, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "download_credit_pct", cfg->cdt_dn_pct, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "leech_pct", cfg->leech_pct, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "leech_sec", cfg->leech_sec, NULL);
	iniSetHexInt(&ini, ROOT_SECTION, "settings", cfg->file_misc, NULL);
	iniSetUInteger(&ini, ROOT_SECTION, "filename_maxlen", cfg->filename_maxlen, NULL);

	/* Extractable File Types */

	for(int i=0; i<cfg->total_fextrs; i++) {
		SAFEPRINTF(name, "extractor:%u", i);
		str_list_t section = strListInit();
		iniSetString(&section, name, "extension", cfg->fextr[i]->ext, NULL);
		iniSetString(&section, name, "cmd", cfg->fextr[i]->cmd, NULL);
		iniSetString(&section, name, "ars", cfg->fextr[i]->arstr, NULL);
		strListMerge(&ini, section);
		free(section);
	}

	/* Compressable File Types */

	for(int i=0; i<cfg->total_fcomps; i++) {
		SAFEPRINTF(name, "compressor:%u", i);
		str_list_t section = strListInit();
		iniSetString(&section, name, "extension", cfg->fcomp[i]->ext, NULL);
		iniSetString(&section, name, "cmd", cfg->fcomp[i]->cmd, NULL);
		iniSetString(&section, name, "ars", cfg->fcomp[i]->arstr, NULL);
		strListMerge(&ini, section);
		free(section);
	}

	/* Viewable File Types */

	for(int i=0; i<cfg->total_fviews; i++) {
		SAFEPRINTF(name, "viewer:%u", i);
		str_list_t section = strListInit();
		iniSetString(&section, name, "extension", cfg->fview[i]->ext, NULL);
		iniSetString(&section, name, "cmd", cfg->fview[i]->cmd, NULL);
		iniSetString(&section, name, "ars", cfg->fview[i]->arstr, NULL);
		strListMerge(&ini, section);
		free(section);
	}

	/* Testable File Types */

	for(int i=0; i<cfg->total_ftests; i++) {
		SAFEPRINTF(name, "tester:%u", i);
		str_list_t section = strListInit();
		iniSetString(&section, name, "extension", cfg->ftest[i]->ext, NULL);
		iniSetString(&section, name, "cmd", cfg->ftest[i]->cmd, NULL);
		iniSetString(&section, name, "working", cfg->ftest[i]->workstr, NULL);
		iniSetString(&section, name, "ars", cfg->ftest[i]->arstr, NULL);
		strListMerge(&ini, section);
		free(section);
	}

	/* Download Events */

	for(int i=0; i<cfg->total_dlevents; i++) {
		SAFEPRINTF(name, "dlevent:%u", i);
		str_list_t section = strListInit();
		iniSetString(&section, name, "extension", cfg->dlevent[i]->ext, NULL);
		iniSetString(&section, name, "cmd", cfg->dlevent[i]->cmd, NULL);
		iniSetString(&section, name, "working", cfg->dlevent[i]->workstr, NULL);
		iniSetString(&section, name, "ars", cfg->dlevent[i]->arstr, NULL);
		strListMerge(&ini, section);
		free(section);
	}

	/* File Transfer Protocols */

	for(int i=0; i<cfg->total_prots; i++) {
		char str[128];
		SAFEPRINTF(name, "protocol:%u", i);
		str_list_t section = strListInit();
		SAFEPRINTF(str, "%c", cfg->prot[i]->mnemonic);
		iniSetString(&section, name, "key", str, NULL);
		iniSetString(&section, name, "name", cfg->prot[i]->name, NULL);
		iniSetString(&section, name, "ulcmd", cfg->prot[i]->ulcmd, NULL);
		iniSetString(&section, name, "dlcmd", cfg->prot[i]->dlcmd, NULL);
		iniSetString(&section, name, "batulcmd", cfg->prot[i]->batulcmd, NULL);
		iniSetString(&section, name, "batdlcmd", cfg->prot[i]->batdlcmd, NULL);
		iniSetHexInt(&section, name, "settings", cfg->prot[i]->misc, NULL);
		iniSetString(&section, name, "ars", cfg->prot[i]->arstr, NULL);
		strListMerge(&ini, section);
		free(section);
	}

	/* File Libraries */

	for(int i=0; i<cfg->total_libs; i++) {
		SAFEPRINTF(name, "lib:%s", cfg->lib[i]->sname);
		str_list_t section = strListInit();
		iniSetString(&section, name, "description", cfg->lib[i]->lname, NULL);
		iniSetString(&section, name, "ars", cfg->lib[i]->arstr, NULL);
		iniSetString(&section, name, "upload_ars", cfg->lib[i]->ul_arstr, NULL);
		iniSetString(&section, name, "download_ars", cfg->lib[i]->dl_arstr, NULL);
		iniSetString(&section, name, "operator_ars", cfg->lib[i]->op_arstr, NULL);
		iniSetString(&section, name, "exempt_ars", cfg->lib[i]->ex_arstr, NULL);
		iniSetString(&section, name, "parent_path", cfg->lib[i]->parent_path, NULL);
		iniSetString(&section, name, "code_prefix", cfg->lib[i]->code_prefix, NULL);

		iniSetUInteger(&section, name, "sort", cfg->lib[i]->sort, NULL);
		iniSetHexInt(&section, name, "settings", cfg->lib[i]->misc, NULL);
		iniSetUInteger(&section, name, "vdir_name", cfg->lib[i]->vdir_name, NULL);
		strListMerge(&ini, section);
		free(section);

		SAFEPRINTF(name, "dir_defaults:%s", cfg->lib[i]->sname);
		section = strListInit();
		write_dir_defaults_cfg(&section, name, &cfg->lib[i]->dir_defaults);
		strListMerge(&ini, section);
		free(section);
	}

	/* File Directories */

	unsigned int dirnum = 0;	/* New directory numbering (as saved) */
	for (int j = 0; j < cfg->total_libs; j++) {
		for (int i = 0; i < cfg->total_dirs; i++) {
			if (cfg->dir[i]->lname[0] == 0
				|| cfg->dir[i]->sname[0] == 0
				|| cfg->dir[i]->code_suffix[0] == 0)
				continue;
			if (cfg->dir[i]->lib == j) {
				cfg->dir[i]->dirnum = dirnum++;
				SAFEPRINTF2(name, "dir:%s:%s"
					,cfg->lib[j]->sname, cfg->dir[i]->code_suffix);
				str_list_t section = strListInit();
				iniSetString(&section, name, "description", cfg->dir[i]->lname, NULL);
				iniSetString(&section, name, "name", cfg->dir[i]->sname, NULL);

				if (cfg->dir[i]->data_dir[0]) {
					backslash(cfg->dir[i]->data_dir);
					md(cfg->dir[i]->data_dir);
				}

				iniSetString(&section, name, "ars", cfg->dir[i]->arstr, NULL);
				iniSetString(&section, name, "upload_ars", cfg->dir[i]->ul_arstr, NULL);
				iniSetString(&section, name, "download_ars", cfg->dir[i]->dl_arstr, NULL);
				iniSetString(&section, name, "operator_ars", cfg->dir[i]->op_arstr, NULL);
				iniSetString(&section, name, "exempt_ars", cfg->dir[i]->ex_arstr, NULL);
				iniSetString(&section, name, "area_tag", cfg->dir[i]->area_tag, NULL);
				backslash(cfg->dir[i]->path);
				iniSetString(&section, name, "path", cfg->dir[i]->path, NULL);

				if (cfg->dir[i]->misc&DIR_FCHK) {
					SAFECOPY(path, cfg->dir[i]->path);
					if (!path[0]) {		/* no file storage path specified */
						SAFEPRINTF2(path, "%s%s"
							, cfg->lib[cfg->dir[i]->lib]->code_prefix
							, cfg->dir[i]->code_suffix);
						strlwr(path);
					}
					if(cfg->lib[cfg->dir[i]->lib]->parent_path[0])
						prep_dir(cfg->lib[cfg->dir[i]->lib]->parent_path, path, sizeof(path));
					else {
						char str[MAX_PATH + 1];
						if(cfg->dir[i]->data_dir[0])
							SAFECOPY(str, cfg->dir[i]->data_dir);
						else
							SAFEPRINTF(str, "%sdirs", cfg->data_dir);
						prep_dir(str, path, sizeof(path));
					}
					(void)mkpath(path);
				}

				write_dir_defaults_cfg(&section, name, cfg->dir[i]);
				strListMerge(&ini, section);
				free(section);
			}
		}
	}

	/* Text File Sections */

	for(int i=0; i<cfg->total_txtsecs; i++) {
		char str[LEN_CODE + 1];
#if 1
		SAFECOPY(str,cfg->txtsec[i]->code);
		strlwr(str);
		safe_snprintf(path,sizeof(path),"%stext/%s",cfg->data_dir,str);
		md(path);
#endif
		SAFEPRINTF(name, "text:%s", cfg->txtsec[i]->code);
		str_list_t section = strListInit();
		iniSetString(&section, name, "name", cfg->txtsec[i]->name, NULL);
		iniSetString(&section, name, "ars", cfg->txtsec[i]->arstr, NULL);
		strListMerge(&ini, section);
		free(section);
	}

	FILE* fp = fopen(inipath, "w");
	if(fp != NULL) {
		result = iniWriteFile(fp, ini);
		fclose(fp);
	}
	iniFreeStringList(ini);

	return result;
}


/****************************************************************************/
/****************************************************************************/
bool write_chat_cfg(scfg_t* cfg)
{
	bool	result = false;
	char	inipath[MAX_PATH+1];
	char	section[INI_MAX_VALUE_LEN];

	if(cfg->prepped)
		return false;

	SAFEPRINTF(inipath, "%schat.ini", cfg->ctrl_dir);
	backup(inipath, cfg->config_backup_level, true);

	str_list_t ini = strListInit();
	for(int i=0; i<cfg->total_gurus; i++) {
		SAFEPRINTF(section, "guru:%s", cfg->guru[i]->code);
		iniSetString(&ini, section, "name", cfg->guru[i]->name, NULL);
		iniSetString(&ini, section, "ars", cfg->guru[i]->arstr, NULL);
	}

	for(int i=0; i<cfg->total_actsets; i++) {
		SAFEPRINTF(section, "actions:%s", cfg->actset[i]->name);
		for(int j=0; j<cfg->total_chatacts; j++) {
			if(cfg->chatact[j]->actset == i)
				iniSetString(&ini, section, cfg->chatact[j]->cmd, cfg->chatact[j]->out, NULL);
		}
	}

	for(int i=0; i<cfg->total_chans; i++) {
		SAFEPRINTF(section, "chan:%s", cfg->chan[i]->code);
		iniSetString(&ini, section, "actions", cfg->actset[cfg->chan[i]->actset]->name, NULL);
		iniSetString(&ini, section, "name", cfg->chan[i]->name, NULL);
		iniSetString(&ini, section, "ars", cfg->chan[i]->arstr, NULL);
		iniSetUInteger(&ini, section, "cost", cfg->chan[i]->cost, NULL);
		iniSetString(&ini, section, "guru", cfg->guru[cfg->chan[i]->guru]->code, NULL);
		iniSetHexInt(&ini, section, "settings", cfg->chan[i]->misc, NULL);
	}

	for(int i=0; i<cfg->total_pages; i++) {
		SAFEPRINTF(section, "pager:%u", i);
		iniSetString(&ini, section, "cmd", cfg->page[i]->cmd, NULL);
		iniSetString(&ini, section, "ars", cfg->page[i]->arstr, NULL);
		iniSetHexInt(&ini, section, "settings", cfg->page[i]->misc, NULL);
	}

	FILE* fp = fopen(inipath, "w");
	if(fp != NULL) {
		result = iniWriteFile(fp, ini);
		fclose(fp);
	}
	iniFreeStringList(ini);

	return result;
}


/****************************************************************************/
/****************************************************************************/
bool write_xtrn_cfg(scfg_t* cfg)
{
	bool	result = false;
	char	inipath[MAX_PATH+1];
	char	name[INI_MAX_VALUE_LEN];

	if(cfg->prepped)
		return false;

	SAFEPRINTF(inipath, "%sxtrn.ini", cfg->ctrl_dir);
	backup(inipath, cfg->config_backup_level, true);

	str_list_t ini = strListInit();
	for(int i=0; i<cfg->total_xedits; i++) {
		SAFEPRINTF(name, "editor:%s", cfg->xedit[i]->code);
		str_list_t section = strListInit();
		iniSetString(&section, name, "name", cfg->xedit[i]->name, NULL);
		iniSetString(&section, name, "cmd", cfg->xedit[i]->rcmd, NULL);
		iniSetHexInt(&section, name, "settings", cfg->xedit[i]->misc, NULL);
		iniSetString(&section, name, "ars", cfg->xedit[i]->arstr, NULL);
		iniSetUInteger(&section, name, "type", cfg->xedit[i]->type, NULL);
		iniSetUInteger(&section, name, "soft_cr", cfg->xedit[i]->soft_cr, NULL);
		iniSetUInteger(&section, name, "quotewrap_cols", cfg->xedit[i]->quotewrap_cols, NULL);
		strListMerge(&ini, section);
		free(section);
	}

	for(int i=0; i<cfg->total_xtrnsecs; i++) {
		SAFEPRINTF(name, "sec:%s", cfg->xtrnsec[i]->code);
		str_list_t section = strListInit();
		iniSetString(&section, name, "name", cfg->xtrnsec[i]->name, NULL);
		iniSetString(&section, name, "ars", cfg->xtrnsec[i]->arstr, NULL);
		strListMerge(&ini, section);
		free(section);
	}

	for(int sec=0; sec<cfg->total_xtrnsecs; sec++) {
		for(int i=0; i<cfg->total_xtrns; i++) {
			if(cfg->xtrn[i]->name[0] == 0
				|| cfg->xtrn[i]->code[0] == 0)
				continue;
			if(cfg->xtrn[i]->sec!=sec)
				continue;
			SAFEPRINTF2(name, "prog:%s:%s", cfg->xtrnsec[sec]->code, cfg->xtrn[i]->code);
			str_list_t section = strListInit();
			iniSetString(&section, name, "name", cfg->xtrn[i]->name, NULL);
			iniSetString(&section, name, "ars", cfg->xtrn[i]->arstr, NULL);
			iniSetString(&section, name, "execution_ars", cfg->xtrn[i]->run_arstr, NULL);
			iniSetUInteger(&section, name, "type", cfg->xtrn[i]->type, NULL);
			iniSetHexInt(&section, name, "settings", cfg->xtrn[i]->misc, NULL);
			iniSetUInteger(&section, name, "event", cfg->xtrn[i]->event, NULL);
			iniSetUInteger(&section, name, "cost", cfg->xtrn[i]->cost, NULL);
			iniSetString(&section, name, "cmd", cfg->xtrn[i]->cmd, NULL);
			iniSetString(&section, name, "clean_cmd", cfg->xtrn[i]->clean, NULL);
			iniSetString(&section, name, "startup_dir", cfg->xtrn[i]->path, NULL);
			iniSetUInteger(&section, name, "textra", cfg->xtrn[i]->textra, NULL);
			iniSetUInteger(&section, name, "max_time", cfg->xtrn[i]->maxtime, NULL);
			iniSetDuration(&section, name, "max_inactivity", cfg->xtrn[i]->max_inactivity, NULL);
			strListMerge(&ini, section);
			free(section);
		}
	}

	for(int i=0; i<cfg->total_events; i++) {
		SAFEPRINTF(name, "event:%s", cfg->event[i]->code);
		str_list_t section = strListInit();
		iniSetString(&section, name, "cmd", cfg->event[i]->cmd, NULL);
		iniSetUInteger(&section, name, "days", cfg->event[i]->days, NULL);
		iniSetUInteger(&section, name, "time", cfg->event[i]->time, NULL);
		iniSetUInteger(&section, name, "node_num", cfg->event[i]->node, NULL);
		iniSetHexInt(&section, name, "settings", cfg->event[i]->misc, NULL);
		iniSetString(&section, name, "startup_dir", cfg->event[i]->dir, NULL);
		iniSetUInteger(&section, name, "freq", cfg->event[i]->freq, NULL);
		iniSetHexInt(&section, name, "mdays", cfg->event[i]->mdays, NULL);
		iniSetUInteger(&section, name, "months", cfg->event[i]->months, NULL);
		iniSetUInteger(&section, name, "errlevel", cfg->event[i]->errlevel, NULL);
		strListMerge(&ini, section);
		free(section);
	}

	for(int i=0; i<cfg->total_natvpgms; i++) {
		SAFEPRINTF(name, "native:%s", cfg->natvpgm[i]->name);
		iniAddSection(&ini, name, NULL);
	}

	for(int i=0; i<cfg->total_hotkeys; i++) {
		SAFEPRINTF(name, "hotkey:%u", cfg->hotkey[i]->key);
		iniSetString(&ini, name, "cmd", cfg->hotkey[i]->cmd, NULL);
	}

	FILE* fp = fopen(inipath, "w");
	if(fp != NULL) {
		result = iniWriteFile(fp, ini);
		fclose(fp);
	}
	iniFreeStringList(ini);

	return result;
}

void refresh_cfg(scfg_t* cfg)
{
	char	str[MAX_PATH+1];
    int		i;
	int		file = -1;
    node_t	node;
    
    for(i=0;i<cfg->sys_nodes;i++) {
       	if(getnodedat(cfg,i+1,&node, /* lockit: */true, &file)!=0)
			continue;
        node.misc|=NODE_RRUN;
        if(putnodedat(cfg,i+1,&node, /* closeit: */false, file))
            break;
    }
	CLOSE_OPEN_FILE(file);

	SAFEPRINTF(str,"%srecycle",cfg->ctrl_dir);		ftouch(str);
}
