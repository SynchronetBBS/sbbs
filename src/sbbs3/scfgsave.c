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

#include "scfgsave.h"
#include "load_cfg.h"
#include "smblib.h"
#include "userdat.h"
#include "nopen.h"

BOOL no_msghdr=FALSE,all_msghdr=FALSE;

static char nulbuf[256]={0};
static int  pslen;
#define put_int(var,stream) fwrite(&var,1,sizeof(var),stream)
#define put_str(var,stream) { pslen=strlen(var); \
							fwrite(var,1,pslen > sizeof(var) \
								? sizeof(var) : pslen ,stream); \
							fwrite(nulbuf,1,pslen > sizeof(var) \
								? 0 : sizeof(var)-pslen,stream); }

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
	char	tmp[128];
	FILE*	fp;
	const char* section = ROOT_SECTION;
	smb_t	smb;

	if(cfg->prepped)
		return FALSE;

	ZERO_VAR(smb);

	SAFEPRINTF(path, "%smsgs.ini", cfg->ctrl_dir);
	backup(path, backup_level, TRUE);

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

	backslash(cfg->echomail_dir);

	str[0]=0;
	/* Calculate and save the actual number (total) of sub-boards that will be written */
	n = 0;
	for(i=0; i<cfg->total_subs; i++)
		if(cfg->sub[i]->grp < cfg->total_grps	/* total VALID sub-boards */
			&& cfg->sub[i]->lname[0]
			&& cfg->sub[i]->sname[0]
			&& cfg->sub[i]->code_suffix[0])
			n++;
	put_int(n, NULL);
	
	
	unsigned int subnum = 0;	/* New sub-board numbering (as saved) */
	for(unsigned grp = 0; grp < cfg->total_grps; grp++) {
		for(i=0;i<cfg->total_subs;i++) {
			if(cfg->sub[i]->lname[0] == 0
				|| cfg->sub[i]->sname[0] == 0
				|| cfg->sub[i]->code_suffix[0] == 0)
				continue;
			if(cfg->sub[i]->grp != grp)
				continue;
			SAFEPRINTF(tmp, "sub:%s:%s"
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
				strcat(smb.file,path);
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

	put_int(cfg->total_faddrs, NULL);
	for(i=0;i<cfg->total_faddrs;i++) {
		put_int(cfg->faddr[i].zone, NULL);
		put_int(cfg->faddr[i].net, NULL);
		put_int(cfg->faddr[i].node, NULL);
		put_int(cfg->faddr[i].point, NULL); }

	put_str(cfg->origline, NULL);
	put_str(cfg->netmail_sem, NULL);
	put_str(cfg->echomail_sem, NULL);
	backslash(cfg->netmail_dir);
	put_str(cfg->netmail_dir, NULL);
	put_str(cfg->echomail_dir, NULL);	/* not used */
	backslash(cfg->fidofile_dir);
	put_str(cfg->fidofile_dir, NULL);
	put_int(cfg->netmail_misc, NULL);
	put_int(cfg->netmail_cost, NULL);
	put_int(cfg->dflt_faddr, NULL);
	n=0;
	for(i=0;i<28;i++)
		put_int(n, NULL);
	md(cfg->netmail_dir);

	/* QWKnet Config */

	put_str(cfg->qnet_tagline, NULL);

	put_int(cfg->total_qhubs, NULL);
	for(i=0;i<cfg->total_qhubs;i++) {
		put_str(cfg->qhub[i]->id, NULL);
		put_int(cfg->qhub[i]->time, NULL);
		put_int(cfg->qhub[i]->freq, NULL);
		put_int(cfg->qhub[i]->days, NULL);
		put_int(cfg->qhub[i]->node, NULL);
		put_str(cfg->qhub[i]->call, NULL);
		put_str(cfg->qhub[i]->pack, NULL);
		put_str(cfg->qhub[i]->unpack, NULL);
		n = 0;
		for(j=0;j<cfg->qhub[i]->subs;j++)
			if(cfg->qhub[i]->sub[j] != NULL) n++;
		put_int(n, NULL);
		for(j=0;j<cfg->qhub[i]->subs;j++) {
			if(cfg->qhub[i]->sub[j] == NULL)
				continue;
			put_int(cfg->qhub[i]->conf[j], NULL);
			n=(uint16_t)cfg->qhub[i]->sub[j]->subnum;
			put_int(n, NULL);
			put_int(cfg->qhub[i]->mode[j], NULL); 
		}
		put_int(cfg->qhub[i]->misc, NULL);
		put_str(cfg->qhub[i]->fmt, NULL);
		n=0;
		for(j=0;j<28;j++)
			put_int(n, NULL); 
	}
	n=0;
	for(i=0;i<32;i++)
		put_int(n, NULL);

	/* PostLink Config */

	memset(str,0,11);
	fwrite(str,11,1, NULL);	/* unused, used to be site name */
	put_int(cfg->sys_psnum, NULL);

	put_int(cfg->total_phubs, NULL);
	for(i=0;i<cfg->total_phubs;i++) {
		put_str(cfg->phub[i]->name, NULL);
		put_int(cfg->phub[i]->time, NULL);
		put_int(cfg->phub[i]->freq, NULL);
		put_int(cfg->phub[i]->days, NULL);
		put_int(cfg->phub[i]->node, NULL);
		put_str(cfg->phub[i]->call, NULL);
		n=0;
		for(j=0;j<32;j++)
			put_int(n, NULL); }

	put_str(cfg->sys_psname, NULL);
	n=0;
	for(i=0;i<32;i++)
		put_int(n, NULL);

	put_str(cfg->sys_inetaddr, NULL); /* Internet address */
	put_str(cfg->inetmail_sem, NULL);
	put_int(cfg->inetmail_misc, NULL);
	put_int(cfg->inetmail_cost, NULL);
	put_str(cfg->smtpmail_sem, NULL);

	fclose(stream);

	/* MUST BE AT END */

	if(!no_msghdr) {
		strcpy(dir,cfg->data_dir);
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
			i=smb_create(&smb);
			smb_close(&smb);
			return(i==0);
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
	char	str[MAX_PATH+1],cmd[LEN_CMD+1],c;
	char	path[MAX_PATH+1];
	int 	i,j,k,file;
	uint16_t	n;
	int32_t	l=0;
	FILE	*stream;

	if(cfg->prepped)
		return(FALSE);

	SAFEPRINTF(str,"%sfile.cnf",cfg->ctrl_dir);
	backup(str, backup_level, TRUE);

	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
		|| (stream=fdopen(file,"wb"))==NULL) {
		return(FALSE); 
	}
	setvbuf(stream,NULL,_IOFBF,FNOPEN_BUF_SIZE);

	put_int(cfg->min_dspace, NULL);
	put_int(cfg->max_batup, NULL);
	put_int(cfg->max_batdn, NULL);
	put_int(cfg->max_userxfer, NULL);
	put_int(l, NULL);					/* unused */
	put_int(cfg->cdt_up_pct, NULL);
	put_int(cfg->cdt_dn_pct, NULL);
	put_int(l, NULL);					/* unused */
	memset(cmd, 0, sizeof(cmd));
	put_str(cmd, NULL);
	put_int(cfg->leech_pct, NULL);
	put_int(cfg->leech_sec, NULL);
	put_int(cfg->file_misc, NULL);
	put_int(cfg->filename_maxlen, NULL);
	n=0;
	for(i=0;i<29;i++)
		put_int(n, NULL);

	/* Extractable File Types */

	put_int(cfg->total_fextrs, NULL);
	for(i=0;i<cfg->total_fextrs;i++) {
		put_str(cfg->fextr[i]->ext, NULL);
		put_str(cfg->fextr[i]->cmd, NULL);
		put_str(cfg->fextr[i]->arstr, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL); }

	/* Compressable File Types */

	put_int(cfg->total_fcomps, NULL);
	for(i=0;i<cfg->total_fcomps;i++) {
		put_str(cfg->fcomp[i]->ext, NULL);
		put_str(cfg->fcomp[i]->cmd, NULL);
		put_str(cfg->fcomp[i]->arstr, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL); }

	/* Viewable File Types */

	put_int(cfg->total_fviews, NULL);
	for(i=0;i<cfg->total_fviews;i++) {
		put_str(cfg->fview[i]->ext, NULL);
		put_str(cfg->fview[i]->cmd, NULL);
		put_str(cfg->fview[i]->arstr, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL); }

	/* Testable File Types */

	put_int(cfg->total_ftests, NULL);
	for(i=0;i<cfg->total_ftests;i++) {
		put_str(cfg->ftest[i]->ext, NULL);
		put_str(cfg->ftest[i]->cmd, NULL);
		put_str(cfg->ftest[i]->workstr, NULL);
		put_str(cfg->ftest[i]->arstr, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL); }

	/* Download Events */

	put_int(cfg->total_dlevents, NULL);
	for(i=0;i<cfg->total_dlevents;i++) {
		put_str(cfg->dlevent[i]->ext, NULL);
		put_str(cfg->dlevent[i]->cmd, NULL);
		put_str(cfg->dlevent[i]->workstr, NULL);
		put_str(cfg->dlevent[i]->arstr, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL); }

	/* File Transfer Protocols */

	put_int(cfg->total_prots, NULL);
	for(i=0;i<cfg->total_prots;i++) {
		put_int(cfg->prot[i]->mnemonic, NULL);
		put_str(cfg->prot[i]->name, NULL);
		put_str(cfg->prot[i]->ulcmd, NULL);
		put_str(cfg->prot[i]->dlcmd, NULL);
		put_str(cfg->prot[i]->batulcmd, NULL);
		put_str(cfg->prot[i]->batdlcmd, NULL);
		put_str(cfg->prot[i]->blindcmd, NULL);
		put_str(cfg->prot[i]->bicmd, NULL);
		put_int(cfg->prot[i]->misc, NULL);
		put_str(cfg->prot[i]->arstr, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL); }

	/* Alternate File Paths */

	put_int(cfg->altpaths, NULL);
	for(i=0;i<cfg->altpaths;i++) {
		backslash(cfg->altpath[i]);
		fwrite(cfg->altpath[i],LEN_DIR+1,1, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL); }

	/* File Libraries */

	put_int(cfg->total_libs, NULL);
	for(i=0;i<cfg->total_libs;i++) {
		put_str(cfg->lib[i]->lname, NULL);
		put_str(cfg->lib[i]->sname, NULL);
		put_str(cfg->lib[i]->arstr, NULL);
		put_str(cfg->lib[i]->parent_path, NULL);
		put_str(cfg->lib[i]->code_prefix, NULL);

		c = cfg->lib[i]->sort;
		put_int(c, NULL);
		put_int(cfg->lib[i]->misc, NULL);
		c = cfg->lib[i]->vdir_name;
		put_int(c, NULL);
		c = 0x00;
		put_int(c, NULL);

		n=0xffff;
		for(j=0;j<16;j++)
			put_int(n, NULL); 
	}

	/* File Directories */

	/* Calculate and save the actual number (total) of dirs that will be written */
	n = 0;
	for (i = 0; i < cfg->total_dirs; i++)
		if (cfg->dir[i]->lib < cfg->total_libs	/* total VALID file dirs */
			&& cfg->dir[i]->lname[0]
			&& cfg->dir[i]->sname[0]
			&& cfg->dir[i]->code_suffix[0])
			n++;
	put_int(n, NULL);
	unsigned int dirnum = 0;	/* New directory numbering (as saved) */
	for (j = 0; j < cfg->total_libs; j++) {
		for (i = 0; i < cfg->total_dirs; i++) {
			if (cfg->dir[i]->lname[0] == 0
				|| cfg->dir[i]->sname[0] == 0
				|| cfg->dir[i]->code_suffix[0] == 0)
				continue;
			if (cfg->dir[i]->lib == j) {
				cfg->dir[i]->dirnum = dirnum++;
				put_int(cfg->dir[i]->lib, NULL);
				put_str(cfg->dir[i]->lname, NULL);
				put_str(cfg->dir[i]->sname, NULL);
				put_str(cfg->dir[i]->code_suffix, NULL);

				if (cfg->dir[i]->data_dir[0]) {
					backslash(cfg->dir[i]->data_dir);
					md(cfg->dir[i]->data_dir);
				}

				put_str(cfg->dir[i]->data_dir, NULL);
				put_str(cfg->dir[i]->arstr, NULL);
				put_str(cfg->dir[i]->ul_arstr, NULL);
				put_str(cfg->dir[i]->dl_arstr, NULL);
				put_str(cfg->dir[i]->op_arstr, NULL);
				backslash(cfg->dir[i]->path);
				put_str(cfg->dir[i]->path, NULL);

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
						if(cfg->dir[i]->data_dir[0])
							SAFECOPY(str, cfg->dir[i]->data_dir);
						else
							SAFEPRINTF(str, "%sdirs", cfg->data_dir);
						prep_dir(str, path, sizeof(path));
					}
					(void)mkpath(path);
				}

				put_str(cfg->dir[i]->upload_sem, NULL);
				put_int(cfg->dir[i]->maxfiles, NULL);
				put_str(cfg->dir[i]->exts, NULL);
				put_int(cfg->dir[i]->misc, NULL);
				put_int(cfg->dir[i]->seqdev, NULL);
				put_int(cfg->dir[i]->sort, NULL);
				put_str(cfg->dir[i]->ex_arstr, NULL);
				put_int(cfg->dir[i]->maxage, NULL);
				put_int(cfg->dir[i]->up_pct, NULL);
				put_int(cfg->dir[i]->dn_pct, NULL);
				put_str(cfg->dir[i]->area_tag, NULL);
				n = 0xffff;
				for (k = 0; k < 4; k++)
					put_int(n, NULL);
			}
		}
	}

	/* Text File Sections */

	put_int(cfg->total_txtsecs, NULL);
	for(i=0;i<cfg->total_txtsecs;i++) {
#if 1
		SAFECOPY(str,cfg->txtsec[i]->code);
		strlwr(str);
		safe_snprintf(path,sizeof(path),"%stext/%s",cfg->data_dir,str);
		md(path);
#endif
		put_str(cfg->txtsec[i]->name, NULL);
		put_str(cfg->txtsec[i]->code, NULL);
		put_str(cfg->txtsec[i]->arstr, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL); 
	}

	fclose(stream);

	return(TRUE);
}



/****************************************************************************/
/****************************************************************************/
BOOL write_chat_cfg(scfg_t* cfg, int backup_level)
{
	char	str[MAX_PATH+1];
	int 	i,j,file;
	uint16_t	n;
	FILE	*stream;

	if(cfg->prepped)
		return(FALSE);

	SAFEPRINTF(str,"%schat.cnf",cfg->ctrl_dir);
	backup(str, backup_level, TRUE);

	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
		|| (stream=fdopen(file,"wb"))==NULL) {
		return(FALSE); 
	}
	setvbuf(stream,NULL,_IOFBF,FNOPEN_BUF_SIZE);

	put_int(cfg->total_gurus, NULL);
	for(i=0;i<cfg->total_gurus;i++) {
		put_str(cfg->guru[i]->name, NULL);
		put_str(cfg->guru[i]->code, NULL);
		put_str(cfg->guru[i]->arstr, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL);
		}

	put_int(cfg->total_actsets, NULL);
	for(i=0;i<cfg->total_actsets;i++)
		put_str(cfg->actset[i]->name, NULL);

	put_int(cfg->total_chatacts, NULL);
	for(i=0;i<cfg->total_chatacts;i++) {
		put_int(cfg->chatact[i]->actset, NULL);
		put_str(cfg->chatact[i]->cmd, NULL);
		put_str(cfg->chatact[i]->out, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL);
		}

	put_int(cfg->total_chans, NULL);
	for(i=0;i<cfg->total_chans;i++) {
		put_int(cfg->chan[i]->actset, NULL);
		put_str(cfg->chan[i]->name, NULL);
		put_str(cfg->chan[i]->code, NULL);
		put_str(cfg->chan[i]->arstr, NULL);
		put_int(cfg->chan[i]->cost, NULL);
		put_int(cfg->chan[i]->guru, NULL);
		put_int(cfg->chan[i]->misc, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL);
		}

	put_int(cfg->total_pages, NULL);
	for(i=0;i<cfg->total_pages;i++) {
		put_str(cfg->page[i]->cmd, NULL);
		put_str(cfg->page[i]->arstr, NULL);
		put_int(cfg->page[i]->misc, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL);
		}

	fclose(stream);

	return(TRUE);
}


/****************************************************************************/
/****************************************************************************/
BOOL write_xtrn_cfg(scfg_t* cfg, int backup_level)
{
	char	str[MAX_PATH+1];
	uchar	c;
	int 	i,j,sec,file;
	uint16_t	n;
	FILE	*stream;

	if(cfg->prepped)
		return(FALSE);

	sprintf(str,"%sxtrn.cnf",cfg->ctrl_dir);
	backup(str, backup_level, TRUE);

	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
		|| (stream=fdopen(file,"wb"))==NULL) {
		return(FALSE); 
	}
	setvbuf(stream,NULL,_IOFBF,FNOPEN_BUF_SIZE);

	put_int(cfg->total_swaps, NULL);
	for(i=0;i<cfg->total_swaps;i++)
		put_str(cfg->swap[i]->cmd, NULL);

	put_int(cfg->total_xedits, NULL);
	for(i=0;i<cfg->total_xedits;i++) {
		put_str(cfg->xedit[i]->name, NULL);
		put_str(cfg->xedit[i]->code, NULL);
		put_str(cfg->xedit[i]->lcmd, NULL);
		put_str(cfg->xedit[i]->rcmd, NULL);
		put_int(cfg->xedit[i]->misc, NULL);
		put_str(cfg->xedit[i]->arstr, NULL);
		put_int(cfg->xedit[i]->type, NULL);
		c = cfg->xedit[i]->soft_cr;
		put_int(c, NULL);
		n=0;
		put_int(cfg->xedit[i]->quotewrap_cols, NULL);
		for(j=0;j<6;j++)
			put_int(n, NULL);
		}

	put_int(cfg->total_xtrnsecs, NULL);
	for(i=0;i<cfg->total_xtrnsecs;i++) {
		put_str(cfg->xtrnsec[i]->name, NULL);
		put_str(cfg->xtrnsec[i]->code, NULL);
		put_str(cfg->xtrnsec[i]->arstr, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL);
		}

	/* Calculate and save the actual number (total) of xtrn programs that will be written */
	n = 0;
	for (i = 0; i < cfg->total_xtrns; i++)
		if (cfg->xtrn[i]->sec < cfg->total_xtrnsecs	/* Total VALID xtrn progs */
			&& cfg->xtrn[i]->name[0]
			&& cfg->xtrn[i]->code[0])
			n++;
	put_int(n, NULL);
	for(sec=0;sec<cfg->total_xtrnsecs;sec++)
		for(i=0;i<cfg->total_xtrns;i++) {
			if(cfg->xtrn[i]->name[0] == 0
				|| cfg->xtrn[i]->code[0] == 0)
				continue;
			if(cfg->xtrn[i]->sec!=sec)
				continue;
			put_int(cfg->xtrn[i]->sec, NULL);
			put_str(cfg->xtrn[i]->name, NULL);
			put_str(cfg->xtrn[i]->code, NULL);
			put_str(cfg->xtrn[i]->arstr, NULL);
			put_str(cfg->xtrn[i]->run_arstr, NULL);
			put_int(cfg->xtrn[i]->type, NULL);
			put_int(cfg->xtrn[i]->misc, NULL);
			put_int(cfg->xtrn[i]->event, NULL);
			put_int(cfg->xtrn[i]->cost, NULL);
			put_str(cfg->xtrn[i]->cmd, NULL);
			put_str(cfg->xtrn[i]->clean, NULL);
			put_str(cfg->xtrn[i]->path, NULL);
			put_int(cfg->xtrn[i]->textra, NULL);
			put_int(cfg->xtrn[i]->maxtime, NULL);
			n=0;
			for(j=0;j<7;j++)
				put_int(n, NULL);
			}

	put_int(cfg->total_events, NULL);
	for(i=0;i<cfg->total_events;i++) {
		put_str(cfg->event[i]->code, NULL);
		put_str(cfg->event[i]->cmd, NULL);
		put_int(cfg->event[i]->days, NULL);
		put_int(cfg->event[i]->time, NULL);
		put_int(cfg->event[i]->node, NULL);
		put_int(cfg->event[i]->misc, NULL);
		put_str(cfg->event[i]->dir, NULL);
		put_int(cfg->event[i]->freq, NULL);
		put_int(cfg->event[i]->mdays, NULL);
		put_int(cfg->event[i]->months, NULL);
		put_int(cfg->event[i]->errlevel, NULL);
		c=0;
		put_int(c, NULL);
		n=0;
		for(j=0;j<3;j++)
			put_int(n, NULL);
		}

	put_int(cfg->total_natvpgms, NULL);
	for(i=0;i<cfg->total_natvpgms;i++)
		put_str(cfg->natvpgm[i]->name, NULL);
	for(i=0;i<cfg->total_natvpgms;i++)
		put_int(cfg->natvpgm[i]->misc, NULL);

	put_int(cfg->total_hotkeys, NULL);
	for(i=0;i<cfg->total_hotkeys;i++) {
		put_int(cfg->hotkey[i]->key, NULL);
		put_str(cfg->hotkey[i]->cmd, NULL);
		n=0;
		for(j=0;j<8;j++)
			put_int(n, NULL);
		}

	fclose(stream);

	return(TRUE);
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
