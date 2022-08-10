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

BOOL no_msghdr=FALSE,all_msghdr=FALSE;

/****************************************************************************/
/****************************************************************************/
BOOL save_cfg(scfg_t* cfg, int backup_level)
{
	int i;

	if(cfg->prepped)
		return(FALSE);

	if(!write_main_cfg(cfg,backup_level))
		return(FALSE);
	if(!write_msgs_cfg(cfg,backup_level))
		return(FALSE);
	if(!write_file_cfg(cfg,backup_level))
		return(FALSE);
	if(!write_chat_cfg(cfg,backup_level))
		return(FALSE);
	if(!write_xtrn_cfg(cfg,backup_level))
		return(FALSE);

	for(i=0;i<cfg->sys_nodes;i++) {
		cfg->node_num=i+1;
		if(!write_node_cfg(cfg,backup_level))
			return(FALSE);
	}

	return(TRUE);
}

/****************************************************************************/
/****************************************************************************/
BOOL write_node_cfg(scfg_t* cfg, int backup_level)
{
	BOOL	result = FALSE;
	char	path[MAX_PATH+1];
	FILE*	fp;
	const char* section = ROOT_SECTION;

	if(cfg->prepped)
		return FALSE;

	if(cfg->node_num<1 || cfg->node_num>MAX_NODES)
		return FALSE;

	SAFECOPY(path, cfg->node_path[cfg->node_num-1]);
	prep_dir(cfg->ctrl_dir, path, sizeof(path));
	md(path);
	SAFECAT(path, "node.ini");
	backup(path, backup_level, TRUE);

	str_list_t ini = strListInit();
	iniSetString(&ini, section, "phone", cfg->node_phone, NULL);
	iniSetString(&ini, section, "daily", cfg->node_daily, NULL);
	iniSetString(&ini, section, "text_dir", cfg->text_dir, NULL);
	iniSetString(&ini, section, "temp_dir", cfg->temp_dir, NULL);
	iniSetString(&ini, section, "ars", cfg->node_arstr, NULL);

	iniSetLongInt(&ini, section, "settings", cfg->node_misc, NULL);
	iniSetShortInt(&ini, section, "valuser", cfg->node_valuser, NULL);
	iniSetShortInt(&ini, section, "sem_check", cfg->node_sem_check, NULL);
	iniSetShortInt(&ini, section, "stat_check", cfg->node_stat_check, NULL);
	iniSetShortInt(&ini, section, "sec_warn", cfg->sec_warn, NULL);
	iniSetShortInt(&ini, section, "sec_hangup", cfg->sec_hangup, NULL);
	iniSetShortInt(&ini, section, "erruser", cfg->node_erruser, NULL);
	iniSetShortInt(&ini, section, "errlevel", cfg->node_errlevel, NULL);

	if((fp = fopen(path, "w")) != NULL) {
		result = iniWriteFile(fp, ini);
		fclose(fp);
	}
	iniFreeStringList(ini);

	return result;
}

/****************************************************************************/
/****************************************************************************/
BOOL write_main_cfg(scfg_t* cfg, int backup_level)
{
	BOOL	result = FALSE;
	char	path[MAX_PATH+1];
	char	tmp[128];
	FILE*	fp;
	const char* section = ROOT_SECTION;

	if(cfg->prepped)
		return FALSE;

	SAFEPRINTF(path, "%smain.ini", cfg->ctrl_dir);
	backup(path, backup_level, TRUE);

	str_list_t ini = strListInit();
	iniSetString(&ini, section, "name", cfg->sys_name, NULL);
	iniSetString(&ini, section, "qwk_id", cfg->sys_id, NULL);
	iniSetString(&ini, section, "location", cfg->sys_location, NULL);
	iniSetString(&ini, section, "phonefmt", cfg->sys_phonefmt, NULL);
	iniSetString(&ini, section, "operator", cfg->sys_op, NULL);
	iniSetString(&ini, section, "guru", cfg->sys_guru, NULL);
	iniSetString(&ini, section, "password", cfg->sys_pass, NULL);
	for(uint i=0;i<cfg->sys_nodes;i++) {
		char key[128];
		SAFEPRINTF(key, "%u", i + 1);
		if(cfg->node_path[i][0] == 0)
			SAFEPRINTF(cfg->node_path[i], "../node%u", i + 1);
		iniSetString(&ini, "node_dir", key, cfg->node_path[i], NULL);
	}
	backslash(cfg->data_dir);
	iniSetString(&ini, "dir", "data", cfg->data_dir, NULL);
	backslash(cfg->exec_dir);
	iniSetString(&ini, "dir", "exec", cfg->exec_dir, NULL);

	iniSetString(&ini, section, "logon", cfg->sys_logon, NULL);
	iniSetString(&ini, section, "logout", cfg->sys_logout, NULL);
	iniSetString(&ini, section, "daily", cfg->sys_daily, NULL);
	iniSetShortInt(&ini, section, "timezone", cfg->sys_timezone, NULL);
	iniSetLongInt(&ini, section, "settings", cfg->sys_misc, NULL);
	iniSetShortInt(&ini, section, "lastnode", cfg->sys_lastnode, NULL);
	iniSetLongInt(&ini, "newuser", "questions", cfg->uq, NULL);
	iniSetShortInt(&ini, section, "pwdays", cfg->sys_pwdays, NULL);
	iniSetShortInt(&ini, section, "deldays", cfg->sys_deldays, NULL);
	iniSetShortInt(&ini, section, "exp_warn", cfg->sys_exp_warn, NULL);
	iniSetShortInt(&ini, section, "autodel", cfg->sys_autodel, NULL);
	iniSetString(&ini, section, "chat_ars", cfg->sys_chat_arstr, NULL);
	iniSetShortInt(&ini, section, "cdt_min_value", cfg->cdt_min_value, NULL);
	iniSetLongInt(&ini, section, "max_minutes", cfg->max_minutes, NULL);
	iniSetLongInt(&ini, section, "cdt_per_dollar", cfg->cdt_per_dollar, NULL);
	iniSetString(&ini, "newuser", "password", cfg->new_pass, NULL);
	iniSetString(&ini, "newuser", "magic_word", cfg->new_magic, NULL);
	iniSetString(&ini, "newuser", "sif", cfg->new_sif, NULL);
	iniSetString(&ini, "newuser", "sof", cfg->new_sof, NULL);

	iniSetShortInt(&ini, "newuser", "level", cfg->new_level, NULL);
	iniSetLongInt(&ini, "nweuser", "flags1", cfg->new_flags1, NULL);
	iniSetLongInt(&ini, "nweuser", "flags2", cfg->new_flags2, NULL);
	iniSetLongInt(&ini, "nweuser", "flags3", cfg->new_flags3, NULL);
	iniSetLongInt(&ini, "nweuser", "flags4", cfg->new_flags4, NULL);
	iniSetLongInt(&ini, "newuser", "exemptions", cfg->new_exempt, NULL);
	iniSetLongInt(&ini, "newuser", "restrictions", cfg->new_rest, NULL);
	iniSetLongInt(&ini, "newuser", "credits", cfg->new_cdt, NULL);
	iniSetLongInt(&ini, "newuser", "minutes", cfg->new_min, NULL);
	iniSetString(&ini, "newuser", "editor", cfg->new_xedit, NULL);
	iniSetShortInt(&ini, "newuser", "expiration_days", cfg->new_expire, NULL);
	if(cfg->new_shell>=cfg->total_shells)
		cfg->new_shell=0;
	if(cfg->total_shells > 0)
		iniSetString(&ini, "newuser", "command_shell", cfg->shell[cfg->new_shell]->code, NULL);
	iniSetLongInt(&ini, "newuser", "settings", cfg->new_misc, NULL);
	SAFEPRINTF(tmp, "%c", cfg->new_prot);
	iniSetString(&ini, "newuser", "download_protocol", tmp, NULL);
	iniSetBool(&ini, section, "new_install", cfg->new_install, NULL);
	iniSetShortInt(&ini, "newuser", "msgscan_init", cfg->new_msgscan_init, NULL);
	iniSetShortInt(&ini, section, "guest_msgscan_init", cfg->guest_msgscan_init, NULL);
	iniSetShortInt(&ini, section, "min_password_length", cfg->min_pwlen, NULL);
	iniSetLongInt(&ini, section, "max_log_size", cfg->max_log_size, NULL);
	iniSetShortInt(&ini, section, "max_logs_kept", cfg->max_logs_kept, NULL);

	section = "expired";
	iniSetShortInt(&ini, section, "level", cfg->expired_level, NULL);
	iniSetLongInt(&ini, section, "flags1", cfg->expired_flags1, NULL);
	iniSetLongInt(&ini, section, "flags2", cfg->expired_flags2, NULL);
	iniSetLongInt(&ini, section, "flags3", cfg->expired_flags3, NULL);
	iniSetLongInt(&ini, section, "flags4", cfg->expired_flags4, NULL);
	iniSetLongInt(&ini, section, "exemptions", cfg->expired_exempt, NULL);
	iniSetLongInt(&ini, section, "restrictions", cfg->expired_rest, NULL);

	section = "module";
	iniSetString(&ini, section, "logon", cfg->logon_mod, NULL);
	iniSetString(&ini, section, "logoff", cfg->logoff_mod, NULL);
	iniSetString(&ini, section, "newuser", cfg->newuser_mod, NULL);
	iniSetString(&ini, section, "login", cfg->login_mod, NULL);
	iniSetString(&ini, section, "logout", cfg->logout_mod, NULL);
	iniSetString(&ini, section, "sync", cfg->sync_mod, NULL);
	iniSetString(&ini, section, "expire", cfg->expire_mod, NULL);
	iniSetLongInt(&ini, ROOT_SECTION, "ctrlkey_passthru", cfg->ctrlkey_passthru, NULL);
	iniSetString(&ini, "dir", "mods", cfg->mods_dir, NULL);
	iniSetString(&ini, "dir", "logs", cfg->logs_dir, NULL);
	iniSetString(&ini, section, "readmail", cfg->readmail_mod, NULL);
	iniSetString(&ini, section, "scanposts", cfg->scanposts_mod, NULL);
	iniSetString(&ini, section, "scansubs", cfg->scansubs_mod, NULL);
	iniSetString(&ini, section, "listmsgs", cfg->listmsgs_mod, NULL);
	iniSetString(&ini, section, "textsec", cfg->textsec_mod, NULL);
	iniSetString(&ini, section, "automsg", cfg->automsg_mod, NULL);
	iniSetString(&ini, section, "xtrnsec", cfg->xtrnsec_mod, NULL);

	iniSetString(&ini, section, "nodelist", cfg->nodelist_mod, NULL);
	iniSetString(&ini, section, "whosonline", cfg->whosonline_mod, NULL);
	iniSetString(&ini, section, "privatemsg", cfg->privatemsg_mod, NULL);
	iniSetString(&ini, section, "logonlist", cfg->logonlist_mod, NULL);
	
    iniSetString(&ini, section, "prextrn", cfg->prextrn_mod, NULL);
    iniSetString(&ini, section, "postxtrn", cfg->postxtrn_mod, NULL);

	iniSetString(&ini, section, "tempxfer", cfg->tempxfer_mod, NULL);

	iniSetString(&ini, "newuser", "gender_options", cfg->new_genders, NULL);
	iniSetShortInt(&ini, ROOT_SECTION, "user_backup_level", cfg->user_backup_level, NULL);
	iniSetShortInt(&ini, ROOT_SECTION, "mail_backup_level", cfg->mail_backup_level, NULL);

	for(uint i=0; i<10; i++) {
		SAFEPRINTF(tmp, "valset:%u", i);
		section = tmp;
		iniSetShortInt(&ini, section, "level", cfg->val_level[i], NULL);
		iniSetShortInt(&ini, section, "expire", cfg->val_expire[i], NULL);
		iniSetLongInt(&ini, section, "flags1", cfg->val_flags1[i], NULL);
		iniSetLongInt(&ini, section, "flags2", cfg->val_flags2[i], NULL);
		iniSetLongInt(&ini, section, "flags3", cfg->val_flags3[i], NULL);
		iniSetLongInt(&ini, section, "flags4", cfg->val_flags4[i], NULL);
		iniSetLongInt(&ini, section, "credits", cfg->val_cdt[i], NULL);
		iniSetLongInt(&ini, section, "exempt", cfg->val_exempt[i], NULL);
		iniSetLongInt(&ini, section, "rest", cfg->val_rest[i], NULL);
	}

	for(uint i=0; i<100; i++) {
		SAFEPRINTF(tmp, "level:%u", i);
		section = tmp;
		iniSetShortInt(&ini, section, "timeperday", cfg->level_timeperday[i], NULL);
		iniSetShortInt(&ini, section, "timepercall", cfg->level_timepercall[i], NULL);
		iniSetShortInt(&ini, section, "callsperday", cfg->level_callsperday[i], NULL);
		iniSetShortInt(&ini, section, "linespermsg", cfg->level_linespermsg[i], NULL);
		iniSetShortInt(&ini, section, "postsperday", cfg->level_postsperday[i], NULL);
		iniSetShortInt(&ini, section, "emailperday", cfg->level_emailperday[i], NULL);
		iniSetLongInt(&ini, section, "settings", cfg->level_misc[i], NULL);
		iniSetShortInt(&ini, section, "expireto", cfg->level_expireto[i], NULL);
		iniSetLongInt(&ini, section, "freecdtperday", cfg->level_freecdtperday[i], NULL);
	}

	/* Command Shells */
	for(uint i=0; i<cfg->total_shells; i++) {
		SAFEPRINTF(tmp, "shell:%s", cfg->shell[i]->code);
		iniSetString(&ini, section, "name", cfg->shell[i]->name, NULL);
		iniSetString(&ini, section, "ars", cfg->shell[i]->arstr, NULL);
		iniSetLongInt(&ini, section, "settings", cfg->shell[i]->misc, NULL);
	}

	if((fp = fopen(path, "w")) != NULL) {
		result = iniWriteFile(fp, ini);
		fclose(fp);
	}
	iniFreeStringList(ini);

	return result;
}

/****************************************************************************/
/****************************************************************************/
BOOL write_msgs_cfg(scfg_t* cfg, int backup_level)
{
	BOOL	result = FALSE;
	char	path[MAX_PATH+1];
	char	tmp[INI_MAX_VALUE_LEN];
	FILE*	fp;
	const char* section = ROOT_SECTION;
	smb_t	smb;

	if(cfg->prepped)
		return FALSE;

	ZERO_VAR(smb);

	SAFEPRINTF(path, "%smsgs.ini", cfg->ctrl_dir);
	backup(path, backup_level, TRUE);

	str_list_t ini = strListInit();
	iniSetLongInt(&ini, "qwk", "max_msgs", cfg->max_qwkmsgs, NULL);
	iniSetLongInt(&ini, "mail", "max_crcs", cfg->mail_maxcrcs, NULL);
	iniSetLongInt(&ini, "mail", "max_age", cfg->mail_maxage, NULL);
	iniSetString(&ini, "qwk", "prepack_ars", cfg->preqwk_arstr, NULL);
	iniSetShortInt(&ini, ROOT_SECTION, "smb_retry_time", cfg->smb_retry_time, NULL);
	iniSetShortInt(&ini, "qwk", "max_age", cfg->max_qwkmsgage, NULL);
	iniSetShortInt(&ini, "mail", "max_spam_age", cfg->max_spamage, NULL);
	iniSetLongInt(&ini, ROOT_SECTION, "settings", cfg->msg_misc, NULL);

	/* Message Groups */

	for(uint i=0;i<cfg->total_grps;i++) {
		SAFEPRINTF(tmp, "grp:%s", cfg->grp[i]->sname);
		section = tmp;
		iniSetString(&ini, section, "description", cfg->grp[i]->lname, NULL);
		iniSetString(&ini, section, "ars", cfg->grp[i]->arstr, NULL);
		iniSetString(&ini, section, "code_prefix", cfg->grp[i]->code_prefix, NULL);
		iniSetShortInt(&ini, section, "sort", cfg->grp[i]->sort, NULL);
	}

	/* Message Sub-boards */

	unsigned int subnum = 0;	/* New sub-board numbering (as saved) */
	for(unsigned grp = 0; grp < cfg->total_grps; grp++) {
		for(uint i=0; i<cfg->total_subs; i++) {
			if(cfg->sub[i]->lname[0] == 0
				|| cfg->sub[i]->sname[0] == 0
				|| cfg->sub[i]->code_suffix[0] == 0)
				continue;
			if(cfg->sub[i]->grp != grp)
				continue;
			SAFEPRINTF2(tmp, "sub:%s:%s"
				,cfg->grp[grp]->sname, cfg->sub[i]->code_suffix);
			section = tmp;
			cfg->sub[i]->subnum = subnum++;
			iniSetString(&ini, section, "description", cfg->sub[i]->lname, NULL);
			iniSetString(&ini, section, "name", cfg->sub[i]->sname, NULL);
			iniSetString(&ini, section, "qwk_name", cfg->sub[i]->qwkname, NULL);
	#if 1
			if(cfg->sub[i]->data_dir[0]) {
				backslash(cfg->sub[i]->data_dir);
				md(cfg->sub[i]->data_dir);
			}
	#endif
			iniSetString(&ini, section, "data_dir", cfg->sub[i]->data_dir, NULL);
			iniSetString(&ini, section, "ars", cfg->sub[i]->arstr, NULL);
			iniSetString(&ini, section, "read_ars", cfg->sub[i]->read_arstr, NULL);
			iniSetString(&ini, section, "post_ars", cfg->sub[i]->post_arstr, NULL);
			iniSetString(&ini, section, "operator_ars", cfg->sub[i]->op_arstr, NULL);
			iniSetLongInt(&ini, section, "esttings", cfg->sub[i]->misc&(~SUB_HDRMOD), NULL);    /* Don't write mod bit */
			iniSetString(&ini, section, "qwknet_tagline", cfg->sub[i]->tagline, NULL);
			iniSetString(&ini, section, "fidonet_origin", cfg->sub[i]->origline, NULL);
			iniSetString(&ini, section, "post_sem", cfg->sub[i]->post_sem, NULL);
			iniSetString(&ini, section, "newsgroup", cfg->sub[i]->newsgroup, NULL);
			iniSetString(&ini, section, "fidonet_addr", smb_faddrtoa(&cfg->sub[i]->faddr, tmp), NULL);
			iniSetLongInt(&ini, section, "max_msgs", cfg->sub[i]->maxmsgs, NULL);
			iniSetLongInt(&ini, section, "max_crcs", cfg->sub[i]->maxcrcs, NULL);
			iniSetShortInt(&ini, section, "max_age", cfg->sub[i]->maxage, NULL);
			iniSetShortInt(&ini, section, "ptridx", cfg->sub[i]->ptridx, NULL);
			iniSetString(&ini, section, "moderated_ars", cfg->sub[i]->mod_arstr, NULL);
			iniSetShortInt(&ini, section, "qwk_conf", cfg->sub[i]->qwkconf, NULL);
			iniSetLongInt(&ini, section, "print_mode", cfg->sub[i]->pmode, NULL);
			iniSetLongInt(&ini, section, "print_mode_neg", cfg->sub[i]->n_pmode, NULL);
			iniSetString(&ini, section, "area_tag", cfg->sub[i]->area_tag, NULL);

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
					result = FALSE;
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

	section = "FidoNet";
	str_list_t addr_list = strListInit();
	for(uint i=0; i<cfg->total_faddrs; i++)
		strListPush(&addr_list, smb_faddrtoa(&cfg->faddr[i], tmp));
	iniSetStringList(&ini, section, "addr_list", ",", addr_list, NULL);
	strListFree(&addr_list);

	iniSetString(&ini, section, "default_origin", cfg->origline, NULL);
	iniSetString(&ini, section, "netmail_sem", cfg->netmail_sem, NULL);
	iniSetString(&ini, section, "echomail_sem", cfg->echomail_sem, NULL);
	backslash(cfg->netmail_dir);
	iniSetString(&ini, section, "netmail_dir", cfg->netmail_dir, NULL);
	iniSetLongInt(&ini, section, "netmail_settings", cfg->netmail_misc, NULL);
	iniSetLongInt(&ini, section, "netmail_cost", cfg->netmail_cost, NULL);
	md(cfg->netmail_dir);

	/* QWKnet Config */

	iniSetString(&ini, "QWK", "default_tagline", cfg->qnet_tagline, NULL);

	for(uint i=0; i<cfg->total_qhubs; i++) {
		SAFEPRINTF(tmp, "qhub:%s", cfg->qhub[i]->id);
		section = tmp;
		iniSetShortInt(&ini, section, "time", cfg->qhub[i]->time, NULL);
		iniSetShortInt(&ini, section, "freq", cfg->qhub[i]->freq, NULL);
		iniSetShortInt(&ini, section, "days", cfg->qhub[i]->days, NULL);
		iniSetShortInt(&ini, section, "node", cfg->qhub[i]->node, NULL);
		iniSetString(&ini, section, "call", cfg->qhub[i]->call, NULL);
		iniSetString(&ini, section, "pack", cfg->qhub[i]->pack, NULL);
		iniSetString(&ini, section, "unpack", cfg->qhub[i]->unpack, NULL);
		iniSetLongInt(&ini, section, "settings", cfg->qhub[i]->misc, NULL);
		iniSetString(&ini, section, "format", cfg->qhub[i]->fmt, NULL);
		for(uint j=0; j<cfg->qhub[i]->subs; j++) {
			if(cfg->qhub[i]->sub[j] == NULL)
				continue;
			int subnum = cfg->qhub[i]->sub[j]->subnum;
			if(!is_valid_subnum(cfg, subnum))
				continue;
			SAFEPRINTF2(tmp, "qhubsub:%s:%u", cfg->qhub[i]->id, cfg->qhub[i]->conf[j]);
			iniSetString(&ini, section, "sub", cfg->sub[subnum]->code, NULL);
			iniSetLongInt(&ini, section, "settings", cfg->qhub[i]->mode[j], NULL);
		}
	}

	section = "Internet";
	iniSetString(&ini, section, "addr", cfg->sys_inetaddr, NULL); /* Internet address */
	iniSetString(&ini, section, "netmail_sem", cfg->inetmail_sem, NULL);
	iniSetLongInt(&ini, section, "settings", cfg->inetmail_misc, NULL);
	iniSetLongInt(&ini, section, "cost", cfg->inetmail_cost, NULL);
	iniSetString(&ini, section, "smtp_sem", cfg->smtpmail_sem, NULL);

	if((fp = fopen(path, "w")) != NULL) {
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
			return(FALSE); 
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
			return(FALSE); 
		}
		if(smb_getstatus(&smb)!=0) {
			smb_close(&smb);
			return(FALSE); 
		}
		smb.status.max_msgs=0;
		smb.status.max_crcs=cfg->mail_maxcrcs;
		smb.status.max_age=cfg->mail_maxage;
		if(smb_putstatus(&smb)!=0) {
			smb_close(&smb);
			return(FALSE); 
		}
		smb_close(&smb); 
	}

	return result;
}


/****************************************************************************/
/****************************************************************************/
BOOL write_file_cfg(scfg_t* cfg, int backup_level)
{
	BOOL	result = FALSE;
	char	path[MAX_PATH+1];
	char	tmp[INI_MAX_VALUE_LEN];
	const char* section = ROOT_SECTION;

	if(cfg->prepped)
		return FALSE;

	SAFEPRINTF(path, "%sfile.ini", cfg->ctrl_dir);
	backup(path, backup_level, TRUE);

	str_list_t ini = strListInit();
	iniSetShortInt(&ini, section, "min_dspace", cfg->min_dspace, NULL);
	iniSetShortInt(&ini, section, "max_batup", cfg->max_batup, NULL);
	iniSetShortInt(&ini, section, "max_batdn", cfg->max_batdn, NULL);
	iniSetShortInt(&ini, section, "max_userxfer" ,cfg->max_userxfer, NULL);
	iniSetShortInt(&ini, section, "cdt_up_pct", cfg->cdt_up_pct, NULL);
	iniSetShortInt(&ini, section, "cdt_dn_pct", cfg->cdt_dn_pct, NULL);
	iniSetShortInt(&ini, section, "leech_cpt", cfg->leech_pct, NULL);
	iniSetShortInt(&ini, section, "leech_sec", cfg->leech_sec, NULL);
	iniSetLongInt(&ini, section, "settings", cfg->file_misc, NULL);
	iniSetShortInt(&ini, section, "filename_maxlen", cfg->filename_maxlen, NULL);

	/* Extractable File Types */

	for(uint i=0; i<cfg->total_fextrs; i++) {
		SAFEPRINTF(tmp, "extractor:%u", i);
		section = tmp;
		iniSetString(&ini, section, "extension", cfg->fextr[i]->ext, NULL);
		iniSetString(&ini, section, "cmd", cfg->fextr[i]->cmd, NULL);
		iniSetString(&ini, section, "ars", cfg->fextr[i]->arstr, NULL);
	}

	/* Compressable File Types */

	for(uint i=0; i<cfg->total_fcomps; i++) {
		SAFEPRINTF(tmp, "compressor:%u", i);
		section = tmp;
		iniSetString(&ini, section, "extension", cfg->fcomp[i]->ext, NULL);
		iniSetString(&ini, section, "cmd", cfg->fcomp[i]->cmd, NULL);
		iniSetString(&ini, section, "ars", cfg->fcomp[i]->arstr, NULL);
	}

	/* Viewable File Types */

	for(uint i=0; i<cfg->total_fviews; i++) {
		SAFEPRINTF(tmp, "viewer:%u", i);
		section = tmp;
		iniSetString(&ini, section, "extension", cfg->fview[i]->ext, NULL);
		iniSetString(&ini, section, "cmd", cfg->fview[i]->cmd, NULL);
		iniSetString(&ini, section, "ars", cfg->fview[i]->arstr, NULL);
	}

	/* Testable File Types */

	for(uint i=0; i<cfg->total_ftests; i++) {
		SAFEPRINTF(tmp, "tester:%u", i);
		section = tmp;
		iniSetString(&ini, section, "extension", cfg->ftest[i]->ext, NULL);
		iniSetString(&ini, section, "cmd", cfg->ftest[i]->cmd, NULL);
		iniSetString(&ini, section, "working", cfg->ftest[i]->workstr, NULL);
		iniSetString(&ini, section, "ars", cfg->ftest[i]->arstr, NULL);
	}

	/* Download Events */

	for(uint i=0; i<cfg->total_dlevents; i++) {
		SAFEPRINTF(tmp, "dlevent:%u", i);
		section = tmp;
		iniSetString(&ini, section, "extension", cfg->dlevent[i]->ext, NULL);
		iniSetString(&ini, section, "cmd", cfg->dlevent[i]->cmd, NULL);
		iniSetString(&ini, section, "working", cfg->dlevent[i]->workstr, NULL);
		iniSetString(&ini, section, "ars", cfg->dlevent[i]->arstr, NULL);
	}

	/* File Transfer Protocols */

	for(uint i=0; i<cfg->total_prots; i++) {
		char str[128];
		SAFEPRINTF(tmp, "protocol:%u", i);
		section = tmp;
		SAFEPRINTF(str, "%c", cfg->prot[i]->mnemonic);
		iniSetString(&ini, section, "key", str, NULL);
		iniSetString(&ini, section, "name", cfg->prot[i]->name, NULL);
		iniSetString(&ini, section, "ulcmd", cfg->prot[i]->ulcmd, NULL);
		iniSetString(&ini, section, "dlcmd", cfg->prot[i]->dlcmd, NULL);
		iniSetString(&ini, section, "batulcmd", cfg->prot[i]->batulcmd, NULL);
		iniSetString(&ini, section, "batdlcmd", cfg->prot[i]->batdlcmd, NULL);
		iniSetLongInt(&ini, section, "settings", cfg->prot[i]->misc, NULL);
		iniSetString(&ini, section, "ars", cfg->prot[i]->arstr, NULL);
	}

	/* File Libraries */

	for(uint i=0; i<cfg->total_libs; i++) {
		SAFEPRINTF(tmp, "lib:%s", cfg->lib[i]->sname);
		section = tmp;
		iniSetString(&ini, section, "description", cfg->lib[i]->lname, NULL);
		iniSetString(&ini, section, "ars", cfg->lib[i]->arstr, NULL);
		iniSetString(&ini, section, "parent_path", cfg->lib[i]->parent_path, NULL);
		iniSetString(&ini, section, "code_prefix", cfg->lib[i]->code_prefix, NULL);

		iniSetShortInt(&ini, section, "sort", cfg->lib[i]->sort, NULL);
		iniSetLongInt(&ini, section, "settings", cfg->lib[i]->misc, NULL);
		iniSetShortInt(&ini, section, "vdir_name", cfg->lib[i]->vdir_name, NULL);
	}

	/* File Directories */

	unsigned int dirnum = 0;	/* New directory numbering (as saved) */
	for (uint j = 0; j < cfg->total_libs; j++) {
		for (uint i = 0; i < cfg->total_dirs; i++) {
			if (cfg->dir[i]->lname[0] == 0
				|| cfg->dir[i]->sname[0] == 0
				|| cfg->dir[i]->code_suffix[0] == 0)
				continue;
			if (cfg->dir[i]->lib == j) {
				cfg->dir[i]->dirnum = dirnum++;
				SAFEPRINTF2(tmp, "dir:%s:%s"
					,cfg->lib[j]->sname, cfg->dir[i]->code_suffix);
				section = tmp;
				iniSetString(&ini, section, "description", cfg->dir[i]->lname, NULL);
				iniSetString(&ini, section, "name", cfg->dir[i]->sname, NULL);

				if (cfg->dir[i]->data_dir[0]) {
					backslash(cfg->dir[i]->data_dir);
					md(cfg->dir[i]->data_dir);
				}

				iniSetString(&ini, section, "data_dir", cfg->dir[i]->data_dir, NULL);
				iniSetString(&ini, section, "ars", cfg->dir[i]->arstr, NULL);
				iniSetString(&ini, section, "upload_ars", cfg->dir[i]->ul_arstr, NULL);
				iniSetString(&ini, section, "download_ars", cfg->dir[i]->dl_arstr, NULL);
				iniSetString(&ini, section, "operator_ars", cfg->dir[i]->op_arstr, NULL);
				backslash(cfg->dir[i]->path);
				iniSetString(&ini, section, "path", cfg->dir[i]->path, NULL);

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

				iniSetString(&ini, section, "upload_sem", cfg->dir[i]->upload_sem, NULL);
				iniSetShortInt(&ini, section, "maxfiles", cfg->dir[i]->maxfiles, NULL);
				iniSetString(&ini, section, "extensions", cfg->dir[i]->exts, NULL);
				iniSetLongInt(&ini, section, "settings", cfg->dir[i]->misc, NULL);
				iniSetShortInt(&ini, section, "seqdev", cfg->dir[i]->seqdev, NULL);
				iniSetShortInt(&ini, section, "sort", cfg->dir[i]->sort, NULL);
				iniSetString(&ini, section, "exempt_ars", cfg->dir[i]->ex_arstr, NULL);
				iniSetShortInt(&ini, section, "maxage", cfg->dir[i]->maxage, NULL);
				iniSetShortInt(&ini, section, "up_pct", cfg->dir[i]->up_pct, NULL);
				iniSetShortInt(&ini, section, "dn_pct", cfg->dir[i]->dn_pct, NULL);
				iniSetString(&ini, section, "area_tag", cfg->dir[i]->area_tag, NULL);
			}
		}
	}

	/* Text File Sections */

	for(uint i=0; i<cfg->total_txtsecs; i++) {
		char str[LEN_CODE + 1];
#if 1
		SAFECOPY(str,cfg->txtsec[i]->code);
		strlwr(str);
		safe_snprintf(path,sizeof(path),"%stext/%s",cfg->data_dir,str);
		md(path);
#endif
		SAFEPRINTF(tmp, "text:%s", cfg->txtsec[i]->code);
		iniSetString(&ini, section, "name", cfg->txtsec[i]->name, NULL);
		iniSetString(&ini, section, "ars", cfg->txtsec[i]->arstr, NULL);
	}

	SAFEPRINTF(path, "%sfile.ini", cfg->ctrl_dir);
	FILE* fp = fopen(path, "w");
	if(fp != NULL) {
		result = iniWriteFile(fp, ini);
		fclose(fp);
	}
	iniFreeStringList(ini);

	return result;
}


/****************************************************************************/
/****************************************************************************/
BOOL write_chat_cfg(scfg_t* cfg, int backup_level)
{
	BOOL	result = FALSE;
	char	path[MAX_PATH+1];
	char	section[INI_MAX_VALUE_LEN];

	if(cfg->prepped)
		return FALSE;

	SAFEPRINTF(path, "%schat.ini", cfg->ctrl_dir);
	backup(path, backup_level, TRUE);

	str_list_t ini = strListInit();
	for(uint i=0; i<cfg->total_gurus; i++) {
		SAFEPRINTF(section, "guru:%s", cfg->guru[i]->code);
		iniSetString(&ini, section, "name", cfg->guru[i]->name, NULL);
		iniSetString(&ini, section, "ars", cfg->guru[i]->arstr, NULL);
	}

	for(uint i=0; i<cfg->total_actsets; i++) {
		SAFEPRINTF(section, "actions:%s", cfg->actset[i]->name);
		for(uint j=0; j<cfg->total_chatacts; j++) {
			if(cfg->chatact[j]->actset == i)
				iniSetString(&ini, section, cfg->chatact[j]->cmd, cfg->chatact[j]->out, NULL);
		}
	}

	for(uint i=0; i<cfg->total_chans; i++) {
		SAFEPRINTF(section, "chan:%s", cfg->chan[i]->code);
		iniSetString(&ini, section, "actions", cfg->actset[cfg->chan[i]->actset]->name, NULL);
		iniSetString(&ini, section, "name", cfg->chan[i]->name, NULL);
		iniSetString(&ini, section, "ars", cfg->chan[i]->arstr, NULL);
		iniSetLongInt(&ini, section, "cost", cfg->chan[i]->cost, NULL);
		iniSetString(&ini, section, "guru", cfg->guru[cfg->chan[i]->guru]->code, NULL);
		iniSetLongInt(&ini, section, "settings", cfg->chan[i]->misc, NULL);
	}

	for(uint i=0; i<cfg->total_pages; i++) {
		SAFEPRINTF(section, "pager:%u", i);
		iniSetString(&ini, section, "cmd", cfg->page[i]->cmd, NULL);
		iniSetString(&ini, section, "ars", cfg->page[i]->arstr, NULL);
		iniSetLongInt(&ini, section, "settings", cfg->page[i]->misc, NULL);
	}

	FILE* fp = fopen(path, "w");
	if(fp != NULL) {
		result = iniWriteFile(fp, ini);
		fclose(fp);
	}
	iniFreeStringList(ini);

	return result;
}


/****************************************************************************/
/****************************************************************************/
BOOL write_xtrn_cfg(scfg_t* cfg, int backup_level)
{
	BOOL	result = FALSE;
	char	path[MAX_PATH+1];
	char	section[INI_MAX_VALUE_LEN];

	if(cfg->prepped)
		return FALSE;

	SAFEPRINTF(path, "%sxtrn.ini", cfg->ctrl_dir);
	backup(path, backup_level, TRUE);

	str_list_t ini = strListInit();
	for(uint i=0; i<cfg->total_xedits; i++) {
		SAFEPRINTF(section, "editor:%s", cfg->xedit[i]->code);
		iniSetString(&ini, section, "name", cfg->xedit[i]->name, NULL);
		iniSetString(&ini, section, "cmd", cfg->xedit[i]->rcmd, NULL);
		iniSetLongInt(&ini, section, "settings", cfg->xedit[i]->misc, NULL);
		iniSetString(&ini, section, "ars", cfg->xedit[i]->arstr, NULL);
		iniSetShortInt(&ini, section, "type", cfg->xedit[i]->type, NULL);
		iniSetShortInt(&ini, section, "soft_cr", cfg->xedit[i]->soft_cr, NULL);
		iniSetShortInt(&ini, section, "quotewrap_cols", cfg->xedit[i]->quotewrap_cols, NULL);
	}

	for(uint i=0; i<cfg->total_xtrnsecs; i++) {
		SAFEPRINTF(section, "sec:%s", cfg->xtrnsec[i]->code);
		iniSetString(&ini, section, "name", cfg->xtrnsec[i]->name, NULL);
		iniSetString(&ini, section, "ars", cfg->xtrnsec[i]->arstr, NULL);
	}

	for(uint sec=0; sec<cfg->total_xtrnsecs; sec++) {
		for(uint i=0; i<cfg->total_xtrns; i++) {
			if(cfg->xtrn[i]->name[0] == 0
				|| cfg->xtrn[i]->code[0] == 0)
				continue;
			if(cfg->xtrn[i]->sec!=sec)
				continue;
			SAFEPRINTF2(section, "prog:%s:%s", cfg->xtrnsec[sec]->code, cfg->xtrn[i]->code);
			iniSetString(&ini, section, "name", cfg->xtrn[i]->name, NULL);
			iniSetString(&ini, section, "ars", cfg->xtrn[i]->arstr, NULL);
			iniSetString(&ini, section, "run_ars", cfg->xtrn[i]->run_arstr, NULL);
			iniSetShortInt(&ini, section, "type", cfg->xtrn[i]->type, NULL);
			iniSetLongInt(&ini, section, "settings", cfg->xtrn[i]->misc, NULL);
			iniSetShortInt(&ini, section, "event", cfg->xtrn[i]->event, NULL);
			iniSetLongInt(&ini, section, "cost", cfg->xtrn[i]->cost, NULL);
			iniSetString(&ini, section, "cmd", cfg->xtrn[i]->cmd, NULL);
			iniSetString(&ini, section, "clean_cmd", cfg->xtrn[i]->clean, NULL);
			iniSetString(&ini, section, "startup_dir", cfg->xtrn[i]->path, NULL);
			iniSetShortInt(&ini, section, "textra", cfg->xtrn[i]->textra, NULL);
			iniSetShortInt(&ini, section, "max_time", cfg->xtrn[i]->maxtime, NULL);
		}
	}

	for(uint i=0; i<cfg->total_events; i++) {
		SAFEPRINTF(section, "event:%s", cfg->event[i]->code);
		iniSetString(&ini, section, "cmd", cfg->event[i]->cmd, NULL);
		iniSetShortInt(&ini, section, "days", cfg->event[i]->days, NULL);
		iniSetShortInt(&ini, section, "time", cfg->event[i]->time, NULL);
		iniSetShortInt(&ini, section, "node", cfg->event[i]->node, NULL);
		iniSetLongInt(&ini, section, "settings", cfg->event[i]->misc, NULL);
		iniSetString(&ini, section, "startup_dir", cfg->event[i]->dir, NULL);
		iniSetShortInt(&ini, section, "freq", cfg->event[i]->freq, NULL);
		iniSetShortInt(&ini, section, "mdays", cfg->event[i]->mdays, NULL);
		iniSetShortInt(&ini, section, "months", cfg->event[i]->months, NULL);
		iniSetShortInt(&ini, section, "errlevel", cfg->event[i]->errlevel, NULL);
	}

	for(uint i=0; i<cfg->total_natvpgms; i++) {
		SAFEPRINTF(section, "native:%s", cfg->natvpgm[i]->name);
		iniAddSection(&ini, section, NULL);
	}

	for(uint i=0; i<cfg->total_hotkeys; i++) {
		SAFEPRINTF(section, "hotkey:%c", cfg->hotkey[i]->key);
		iniSetString(&ini, section, "cmd", cfg->hotkey[i]->cmd, NULL);
	}

	FILE* fp = fopen(path, "w");
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
       	if(getnodedat(cfg,i+1,&node, /* lockit: */TRUE, &file)!=0)
			continue;
        node.misc|=NODE_RRUN;
        if(putnodedat(cfg,i+1,&node, /* closeit: */FALSE, file))
            break;
    }
	CLOSE_OPEN_FILE(file);

	SAFEPRINTF(str,"%srecycle",cfg->ctrl_dir);		ftouch(str);
}
