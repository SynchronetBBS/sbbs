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
	char	str[MAX_PATH+1];
	int 	i,file;
	FILE	*stream;

	if(cfg->prepped)
		return(FALSE);

	if(cfg->node_num<1 || cfg->node_num>MAX_NODES)
		return(FALSE);

	SAFECOPY(str,cfg->node_path[cfg->node_num-1]);
	prep_dir(cfg->ctrl_dir,str,sizeof(str));
	md(str);
	strcat(str,"node.cnf");
	backup(str, backup_level, TRUE);

	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
		|| (stream=fdopen(file,"wb"))==NULL) {
		return(FALSE); 
	}
	setvbuf(stream,NULL,_IOFBF,FNOPEN_BUF_SIZE);

	put_int(cfg->node_num,stream);
	put_str(cfg->node_name,stream);
	put_str(cfg->node_phone,stream);
	put_str(cfg->node_comspec,stream);				 /* Was node_logon */
	put_int(cfg->node_misc,stream);
	put_int(cfg->node_ivt,stream);
	put_int(cfg->node_swap,stream);
	put_str(cfg->node_swapdir,stream);
	put_int(cfg->node_valuser,stream);
	put_int(cfg->node_minbps,stream);
	put_str(cfg->node_arstr,stream);
	put_int(cfg->node_dollars_per_call,stream);
	put_str(cfg->node_editor,stream);
	put_str(cfg->node_viewer,stream);
	put_str(cfg->node_daily,stream);
	put_int(cfg->node_scrnlen,stream);
	put_int(cfg->node_scrnblank,stream);
	backslash(cfg->ctrl_dir);
	put_str(cfg->ctrl_dir,stream);
	backslash(cfg->text_dir);
	put_str(cfg->text_dir,stream);
	backslash(cfg->temp_dir);
	put_str(cfg->temp_dir,stream);
	for(i=0;i<10;i++)
		put_str(cfg->wfc_cmd[i],stream);
	for(i=0;i<12;i++)
		put_str(cfg->wfc_scmd[i],stream);
	put_str(cfg->mdm_hang,stream);
	put_int(cfg->node_sem_check,stream);
	put_int(cfg->node_stat_check,stream);
	put_str(cfg->scfg_cmd,stream);
	put_int(cfg->sec_warn,stream);
	put_int(cfg->sec_hangup,stream);
	put_int(cfg->node_erruser,stream);
	put_int(cfg->node_errlevel,stream);
	fclose(stream);

	return(TRUE);
}

/****************************************************************************/
/****************************************************************************/
BOOL write_main_cfg(scfg_t* cfg, int backup_level)
{
	char	str[MAX_PATH+1],c=0;
	int 	file;
	ushort	i,j;
	uint16_t n;
	FILE	*stream;

	if(cfg->prepped)
		return(FALSE);

	SAFEPRINTF(str,"%smain.cnf",cfg->ctrl_dir);
	backup(str, backup_level, TRUE);

	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
		|| (stream=fdopen(file,"wb"))==NULL) {
		return(FALSE); 
	}
	setvbuf(stream,NULL,_IOFBF,FNOPEN_BUF_SIZE);

	put_str(cfg->sys_name,stream);
	put_str(cfg->sys_id,stream);
	put_str(cfg->sys_location,stream);
	put_str(cfg->sys_phonefmt,stream);
	put_str(cfg->sys_op,stream);
	put_str(cfg->sys_guru,stream);
	put_str(cfg->sys_pass,stream);
	put_int(cfg->sys_nodes,stream);
	for(i=0;i<cfg->sys_nodes;i++) {
		if(cfg->node_path[i][0] == 0)
			SAFEPRINTF(cfg->node_path[i], "../node%u", i + 1);
		put_str(cfg->node_path[i],stream);
	}
	backslash(cfg->data_dir);
	put_str(cfg->data_dir,stream);

	backslash(cfg->exec_dir);
	put_str(cfg->exec_dir,stream);
	put_str(cfg->sys_logon,stream);
	put_str(cfg->sys_logout,stream);
	put_str(cfg->sys_daily,stream);
	put_int(cfg->sys_timezone,stream);
	put_int(cfg->sys_misc,stream);
	put_int(cfg->sys_lastnode,stream);
	put_int(cfg->sys_autonode,stream);
	put_int(cfg->uq,stream);
	put_int(cfg->sys_pwdays,stream);
	put_int(cfg->sys_deldays,stream);
	put_int(cfg->sys_exp_warn,stream);
	put_int(cfg->sys_autodel,stream);
	put_int(cfg->sys_def_stat,stream);
	put_str(cfg->sys_chat_arstr,stream);
	put_int(cfg->cdt_min_value,stream);
	put_int(cfg->max_minutes,stream);
	put_int(cfg->cdt_per_dollar,stream);
	put_str(cfg->new_pass,stream);
	put_str(cfg->new_magic,stream);
	put_str(cfg->new_sif,stream);
	put_str(cfg->new_sof,stream);

	put_int(cfg->new_level,stream);
	put_int(cfg->new_flags1,stream);
	put_int(cfg->new_flags2,stream);
	put_int(cfg->new_flags3,stream);
	put_int(cfg->new_flags4,stream);
	put_int(cfg->new_exempt,stream);
	put_int(cfg->new_rest,stream);
	put_int(cfg->new_cdt,stream);
	put_int(cfg->new_min,stream);
	put_str(cfg->new_xedit,stream);
	put_int(cfg->new_expire,stream);
	if(cfg->new_shell>=cfg->total_shells)
		cfg->new_shell=0;
	put_int(cfg->new_shell,stream);
	put_int(cfg->new_misc,stream);
	put_int(cfg->new_prot,stream);
	put_int(cfg->new_install,stream);
	put_int(cfg->new_msgscan_init, stream);
	put_int(cfg->guest_msgscan_init, stream);
	put_int(cfg->min_pwlen, stream);
	put_int(c, stream);
	n=0;
	for(i=0;i<4;i++)
		put_int(n,stream);

	put_int(cfg->expired_level,stream);
	put_int(cfg->expired_flags1,stream);
	put_int(cfg->expired_flags2,stream);
	put_int(cfg->expired_flags3,stream);
	put_int(cfg->expired_flags4,stream);
	put_int(cfg->expired_exempt,stream);
	put_int(cfg->expired_rest,stream);

	put_str(cfg->logon_mod,stream);
	put_str(cfg->logoff_mod,stream);
	put_str(cfg->newuser_mod,stream);
	put_str(cfg->login_mod,stream);
	put_str(cfg->logout_mod,stream);
	put_str(cfg->sync_mod,stream);
	put_str(cfg->expire_mod,stream);
	put_int(cfg->ctrlkey_passthru,stream);
	put_str(cfg->mods_dir,stream);
	put_str(cfg->logs_dir,stream);
	put_str(cfg->readmail_mod, stream);
	put_str(cfg->scanposts_mod, stream);
	put_str(cfg->scansubs_mod, stream);
	put_str(cfg->listmsgs_mod, stream);
	put_str(cfg->textsec_mod,stream);
	put_str(cfg->automsg_mod,stream);
	put_str(cfg->xtrnsec_mod,stream);

	n=0;
	for(i=0;i<17;i++)
		put_int(n,stream);
	put_str(cfg->nodelist_mod, stream);
	put_str(cfg->whosonline_mod, stream);
	put_str(cfg->privatemsg_mod, stream);
	put_str(cfg->logonlist_mod, stream);
	
    put_str(cfg->prextrn_mod,stream);
    put_str(cfg->postxtrn_mod,stream);
    
	n=0xffff;
	for(i=0;i<117;i++)
		put_int(n,stream);

	put_int(cfg->user_backup_level,stream);
	put_int(cfg->mail_backup_level,stream);

	n=0;
	for(i=0;i<10;i++) {
		put_int(cfg->val_level[i],stream);
		put_int(cfg->val_expire[i],stream);
		put_int(cfg->val_flags1[i],stream);
		put_int(cfg->val_flags2[i],stream);
		put_int(cfg->val_flags3[i],stream);
		put_int(cfg->val_flags4[i],stream);
		put_int(cfg->val_cdt[i],stream);
		put_int(cfg->val_exempt[i],stream);
		put_int(cfg->val_rest[i],stream);
		for(j=0;j<8;j++)
			put_int(n,stream); }

	c=0;
	for(i=0;i<100 && !feof(stream);i++) {
		put_int(cfg->level_timeperday[i],stream);
		put_int(cfg->level_timepercall[i],stream);
		put_int(cfg->level_callsperday[i],stream);
		put_int(cfg->level_freecdtperday[i],stream);
		put_int(cfg->level_linespermsg[i],stream);
		put_int(cfg->level_postsperday[i],stream);
		put_int(cfg->level_emailperday[i],stream);
		put_int(cfg->level_misc[i],stream);
		put_int(cfg->level_expireto[i],stream);
		put_int(c,stream);
		for(j=0;j<5;j++)
			put_int(n,stream); }

	/* Command Shells */

	put_int(cfg->total_shells,stream);
	for(i=0;i<cfg->total_shells;i++) {
		put_str(cfg->shell[i]->name,stream);
		put_str(cfg->shell[i]->code,stream);
		put_str(cfg->shell[i]->arstr,stream);
		put_int(cfg->shell[i]->misc,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream); 
	}

	fclose(stream);

	return(TRUE);
}

/****************************************************************************/
/****************************************************************************/
BOOL write_msgs_cfg(scfg_t* cfg, int backup_level)
{
	char	str[MAX_PATH+1],c;
	char	dir[LEN_DIR+1]="";
	int 	i,j,k,file;
	uint16_t	n;
	int32_t	l;
	FILE	*stream;
	smb_t	smb;
	BOOL	result = TRUE;

	if(cfg->prepped)
		return(FALSE);

	ZERO_VAR(smb);

	SAFEPRINTF(str,"%smsgs.cnf",cfg->ctrl_dir);
	backup(str, backup_level, TRUE);

	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
		|| (stream=fdopen(file,"wb"))==NULL) {
		return(FALSE); 
	}
	setvbuf(stream,NULL,_IOFBF,FNOPEN_BUF_SIZE);

	put_int(cfg->max_qwkmsgs,stream);
	put_int(cfg->mail_maxcrcs,stream);
	put_int(cfg->mail_maxage,stream);
	put_str(cfg->preqwk_arstr,stream);
	put_int(cfg->smb_retry_time,stream);
	put_int(cfg->max_qwkmsgage,stream);
	put_int(cfg->max_spamage,stream);
	n=0;
	for(i=0;i<232;i++)
		put_int(n,stream);
	put_int(cfg->msg_misc,stream);
	n=0xffff;
	for(i=0;i<255;i++)
		put_int(n,stream);

	/* Message Groups */

	put_int(cfg->total_grps,stream);
	for(i=0;i<cfg->total_grps;i++) {
		put_str(cfg->grp[i]->lname,stream);
		put_str(cfg->grp[i]->sname,stream);
		put_str(cfg->grp[i]->arstr,stream);
		put_str(cfg->grp[i]->code_prefix,stream);
		c=cfg->grp[i]->sort;
		put_int(c,stream);
		n=0;
		for(j=0;j<27;j++)
			put_int(n,stream);
		n=0xffff;
		for(j=0;j<16;j++)
			put_int(n,stream); 
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
	put_int(n,stream);
	unsigned int subnum = 0;	/* New sub-board numbering (as saved) */
	for(unsigned grp = 0; grp < cfg->total_grps; grp++) {
		for(i=0;i<cfg->total_subs;i++) {
			if(cfg->sub[i]->lname[0] == 0
				|| cfg->sub[i]->sname[0] == 0
				|| cfg->sub[i]->code_suffix[0] == 0)
				continue;
			if(cfg->sub[i]->grp != grp)
				continue;
			cfg->sub[i]->subnum = subnum++;
			put_int(cfg->sub[i]->grp,stream);
			put_str(cfg->sub[i]->lname,stream);
			put_str(cfg->sub[i]->sname,stream);
			put_str(cfg->sub[i]->qwkname,stream);
			put_str(cfg->sub[i]->code_suffix,stream);
	#if 1
			if(cfg->sub[i]->data_dir[0]) {
				backslash(cfg->sub[i]->data_dir);
				md(cfg->sub[i]->data_dir);
			}
	#endif
			put_str(cfg->sub[i]->data_dir,stream);
			put_str(cfg->sub[i]->arstr,stream);
			put_str(cfg->sub[i]->read_arstr,stream);
			put_str(cfg->sub[i]->post_arstr,stream);
			put_str(cfg->sub[i]->op_arstr,stream);
			l=(cfg->sub[i]->misc&(~SUB_HDRMOD));    /* Don't write mod bit */
			put_int(l,stream);
			put_str(cfg->sub[i]->tagline,stream);
			put_str(cfg->sub[i]->origline,stream);
			put_str(cfg->sub[i]->post_sem,stream);
			put_str(cfg->sub[i]->newsgroup,stream);
			put_int(cfg->sub[i]->faddr,stream);
			put_int(cfg->sub[i]->maxmsgs,stream);
			put_int(cfg->sub[i]->maxcrcs,stream);
			put_int(cfg->sub[i]->maxage,stream);
			put_int(cfg->sub[i]->ptridx,stream);
			put_str(cfg->sub[i]->mod_arstr,stream);
			put_int(cfg->sub[i]->qwkconf,stream);
			c=0;
			put_int(c,stream); // unused
			put_int(cfg->sub[i]->pmode,stream);
			put_int(cfg->sub[i]->n_pmode,stream);
			put_str(cfg->sub[i]->area_tag, stream);
			n=0;
			for(k=0;k<4;k++)
				put_int(n,stream);

			if(all_msghdr || (cfg->sub[i]->misc&SUB_HDRMOD && !no_msghdr)) {
				if(!cfg->sub[i]->data_dir[0])
					SAFEPRINTF(smb.file,"%ssubs",cfg->data_dir);
				else
					SAFECOPY(smb.file,cfg->sub[i]->data_dir);
				prep_dir(cfg->ctrl_dir,smb.file,sizeof(smb.file));
				md(smb.file);
				SAFEPRINTF2(str,"%s%s"
					,cfg->grp[cfg->sub[i]->grp]->code_prefix
					,cfg->sub[i]->code_suffix);
				strlwr(str);
				strcat(smb.file,str);
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

	put_int(cfg->total_faddrs,stream);
	for(i=0;i<cfg->total_faddrs;i++) {
		put_int(cfg->faddr[i].zone,stream);
		put_int(cfg->faddr[i].net,stream);
		put_int(cfg->faddr[i].node,stream);
		put_int(cfg->faddr[i].point,stream); }

	put_str(cfg->origline,stream);
	put_str(cfg->netmail_sem,stream);
	put_str(cfg->echomail_sem,stream);
	backslash(cfg->netmail_dir);
	put_str(cfg->netmail_dir,stream);
	put_str(cfg->echomail_dir,stream);	/* not used */
	backslash(cfg->fidofile_dir);
	put_str(cfg->fidofile_dir,stream);
	put_int(cfg->netmail_misc,stream);
	put_int(cfg->netmail_cost,stream);
	put_int(cfg->dflt_faddr,stream);
	n=0;
	for(i=0;i<28;i++)
		put_int(n,stream);
	md(cfg->netmail_dir);

	/* QWKnet Config */

	put_str(cfg->qnet_tagline,stream);

	put_int(cfg->total_qhubs,stream);
	for(i=0;i<cfg->total_qhubs;i++) {
		put_str(cfg->qhub[i]->id,stream);
		put_int(cfg->qhub[i]->time,stream);
		put_int(cfg->qhub[i]->freq,stream);
		put_int(cfg->qhub[i]->days,stream);
		put_int(cfg->qhub[i]->node,stream);
		put_str(cfg->qhub[i]->call,stream);
		put_str(cfg->qhub[i]->pack,stream);
		put_str(cfg->qhub[i]->unpack,stream);
		n = 0;
		for(j=0;j<cfg->qhub[i]->subs;j++)
			if(cfg->qhub[i]->sub[j] != NULL) n++;
		put_int(n,stream);
		for(j=0;j<cfg->qhub[i]->subs;j++) {
			if(cfg->qhub[i]->sub[j] == NULL)
				continue;
			put_int(cfg->qhub[i]->conf[j],stream);
			n=(uint16_t)cfg->qhub[i]->sub[j]->subnum;
			put_int(n,stream);
			put_int(cfg->qhub[i]->mode[j],stream); 
		}
		put_int(cfg->qhub[i]->misc, stream);
		put_str(cfg->qhub[i]->fmt,stream);
		n=0;
		for(j=0;j<28;j++)
			put_int(n,stream); 
	}
	n=0;
	for(i=0;i<32;i++)
		put_int(n,stream);

	/* PostLink Config */

	memset(str,0,11);
	fwrite(str,11,1,stream);	/* unused, used to be site name */
	put_int(cfg->sys_psnum,stream);

	put_int(cfg->total_phubs,stream);
	for(i=0;i<cfg->total_phubs;i++) {
		put_str(cfg->phub[i]->name,stream);
		put_int(cfg->phub[i]->time,stream);
		put_int(cfg->phub[i]->freq,stream);
		put_int(cfg->phub[i]->days,stream);
		put_int(cfg->phub[i]->node,stream);
		put_str(cfg->phub[i]->call,stream);
		n=0;
		for(j=0;j<32;j++)
			put_int(n,stream); }

	put_str(cfg->sys_psname,stream);
	n=0;
	for(i=0;i<32;i++)
		put_int(n,stream);

	put_str(cfg->sys_inetaddr,stream); /* Internet address */
	put_str(cfg->inetmail_sem,stream);
	put_int(cfg->inetmail_misc,stream);
	put_int(cfg->inetmail_cost,stream);
	put_str(cfg->smtpmail_sem,stream);

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

	put_int(cfg->min_dspace,stream);
	put_int(cfg->max_batup,stream);
	put_int(cfg->max_batdn,stream);
	put_int(cfg->max_userxfer,stream);
	put_int(l,stream);					/* unused */
	put_int(cfg->cdt_up_pct,stream);
	put_int(cfg->cdt_dn_pct,stream);
	put_int(l,stream);					/* unused */
	memset(cmd, 0, sizeof(cmd));
	put_str(cmd,stream);
	put_int(cfg->leech_pct,stream);
	put_int(cfg->leech_sec,stream);
	put_int(cfg->file_misc,stream);
	n=0;
	for(i=0;i<30;i++)
		put_int(n,stream);

	/* Extractable File Types */

	put_int(cfg->total_fextrs,stream);
	for(i=0;i<cfg->total_fextrs;i++) {
		put_str(cfg->fextr[i]->ext,stream);
		put_str(cfg->fextr[i]->cmd,stream);
		put_str(cfg->fextr[i]->arstr,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream); }

	/* Compressable File Types */

	put_int(cfg->total_fcomps,stream);
	for(i=0;i<cfg->total_fcomps;i++) {
		put_str(cfg->fcomp[i]->ext,stream);
		put_str(cfg->fcomp[i]->cmd,stream);
		put_str(cfg->fcomp[i]->arstr,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream); }

	/* Viewable File Types */

	put_int(cfg->total_fviews,stream);
	for(i=0;i<cfg->total_fviews;i++) {
		put_str(cfg->fview[i]->ext,stream);
		put_str(cfg->fview[i]->cmd,stream);
		put_str(cfg->fview[i]->arstr,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream); }

	/* Testable File Types */

	put_int(cfg->total_ftests,stream);
	for(i=0;i<cfg->total_ftests;i++) {
		put_str(cfg->ftest[i]->ext,stream);
		put_str(cfg->ftest[i]->cmd,stream);
		put_str(cfg->ftest[i]->workstr,stream);
		put_str(cfg->ftest[i]->arstr,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream); }

	/* Download Events */

	put_int(cfg->total_dlevents,stream);
	for(i=0;i<cfg->total_dlevents;i++) {
		put_str(cfg->dlevent[i]->ext,stream);
		put_str(cfg->dlevent[i]->cmd,stream);
		put_str(cfg->dlevent[i]->workstr,stream);
		put_str(cfg->dlevent[i]->arstr,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream); }

	/* File Transfer Protocols */

	put_int(cfg->total_prots,stream);
	for(i=0;i<cfg->total_prots;i++) {
		put_int(cfg->prot[i]->mnemonic,stream);
		put_str(cfg->prot[i]->name,stream);
		put_str(cfg->prot[i]->ulcmd,stream);
		put_str(cfg->prot[i]->dlcmd,stream);
		put_str(cfg->prot[i]->batulcmd,stream);
		put_str(cfg->prot[i]->batdlcmd,stream);
		put_str(cfg->prot[i]->blindcmd,stream);
		put_str(cfg->prot[i]->bicmd,stream);
		put_int(cfg->prot[i]->misc,stream);
		put_str(cfg->prot[i]->arstr,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream); }

	/* Alternate File Paths */

	put_int(cfg->altpaths,stream);
	for(i=0;i<cfg->altpaths;i++) {
		backslash(cfg->altpath[i]);
		fwrite(cfg->altpath[i],LEN_DIR+1,1,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream); }

	/* File Libraries */

	put_int(cfg->total_libs,stream);
	for(i=0;i<cfg->total_libs;i++) {
		put_str(cfg->lib[i]->lname,stream);
		put_str(cfg->lib[i]->sname,stream);
		put_str(cfg->lib[i]->arstr,stream);
		put_str(cfg->lib[i]->parent_path,stream);
		put_str(cfg->lib[i]->code_prefix,stream);

		c = cfg->lib[i]->sort;
		put_int(c,stream);
		put_int(cfg->lib[i]->misc,stream);
		n=0x0000;
		for(j=0;j<1;j++)
			put_int(n,stream); 

		n=0xffff;
		for(j=0;j<16;j++)
			put_int(n,stream); 
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
	put_int(n,stream);
	unsigned int dirnum = 0;	/* New directory numbering (as saved) */
	for (j = 0; j < cfg->total_libs; j++) {
		for (i = 0; i < cfg->total_dirs; i++) {
			if (cfg->dir[i]->lname[0] == 0
				|| cfg->dir[i]->sname[0] == 0
				|| cfg->dir[i]->code_suffix[0] == 0)
				continue;
			if (cfg->dir[i]->lib == j) {
				cfg->dir[i]->dirnum = dirnum++;
				put_int(cfg->dir[i]->lib, stream);
				put_str(cfg->dir[i]->lname, stream);
				put_str(cfg->dir[i]->sname, stream);
				put_str(cfg->dir[i]->code_suffix, stream);

				if (cfg->dir[i]->data_dir[0]) {
					backslash(cfg->dir[i]->data_dir);
					md(cfg->dir[i]->data_dir);
				}

				put_str(cfg->dir[i]->data_dir, stream);
				put_str(cfg->dir[i]->arstr, stream);
				put_str(cfg->dir[i]->ul_arstr, stream);
				put_str(cfg->dir[i]->dl_arstr, stream);
				put_str(cfg->dir[i]->op_arstr, stream);
				backslash(cfg->dir[i]->path);
				put_str(cfg->dir[i]->path, stream);

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

				put_str(cfg->dir[i]->upload_sem, stream);
				put_int(cfg->dir[i]->maxfiles, stream);
				put_str(cfg->dir[i]->exts, stream);
				put_int(cfg->dir[i]->misc, stream);
				put_int(cfg->dir[i]->seqdev, stream);
				put_int(cfg->dir[i]->sort, stream);
				put_str(cfg->dir[i]->ex_arstr, stream);
				put_int(cfg->dir[i]->maxage, stream);
				put_int(cfg->dir[i]->up_pct, stream);
				put_int(cfg->dir[i]->dn_pct, stream);
				put_str(cfg->dir[i]->area_tag, stream);
				c = 0;
				put_int(c, stream);
				n = 0xffff;
				for (k = 0; k < 6; k++)
					put_int(n, stream);
			}
		}
	}

	/* Text File Sections */

	put_int(cfg->total_txtsecs,stream);
	for(i=0;i<cfg->total_txtsecs;i++) {
#if 1
		SAFECOPY(str,cfg->txtsec[i]->code);
		strlwr(str);
		safe_snprintf(path,sizeof(path),"%stext/%s",cfg->data_dir,str);
		md(path);
#endif
		put_str(cfg->txtsec[i]->name,stream);
		put_str(cfg->txtsec[i]->code,stream);
		put_str(cfg->txtsec[i]->arstr,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream); 
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

	put_int(cfg->total_gurus,stream);
	for(i=0;i<cfg->total_gurus;i++) {
		put_str(cfg->guru[i]->name,stream);
		put_str(cfg->guru[i]->code,stream);
		put_str(cfg->guru[i]->arstr,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream);
		}

	put_int(cfg->total_actsets,stream);
	for(i=0;i<cfg->total_actsets;i++)
		put_str(cfg->actset[i]->name,stream);

	put_int(cfg->total_chatacts,stream);
	for(i=0;i<cfg->total_chatacts;i++) {
		put_int(cfg->chatact[i]->actset,stream);
		put_str(cfg->chatact[i]->cmd,stream);
		put_str(cfg->chatact[i]->out,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream);
		}

	put_int(cfg->total_chans,stream);
	for(i=0;i<cfg->total_chans;i++) {
		put_int(cfg->chan[i]->actset,stream);
		put_str(cfg->chan[i]->name,stream);
		put_str(cfg->chan[i]->code,stream);
		put_str(cfg->chan[i]->arstr,stream);
		put_int(cfg->chan[i]->cost,stream);
		put_int(cfg->chan[i]->guru,stream);
		put_int(cfg->chan[i]->misc,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream);
		}

	put_int(cfg->total_pages,stream);
	for(i=0;i<cfg->total_pages;i++) {
		put_str(cfg->page[i]->cmd,stream);
		put_str(cfg->page[i]->arstr,stream);
		put_int(cfg->page[i]->misc,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream);
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

	put_int(cfg->total_swaps,stream);
	for(i=0;i<cfg->total_swaps;i++)
		put_str(cfg->swap[i]->cmd,stream);

	put_int(cfg->total_xedits,stream);
	for(i=0;i<cfg->total_xedits;i++) {
		put_str(cfg->xedit[i]->name,stream);
		put_str(cfg->xedit[i]->code,stream);
		put_str(cfg->xedit[i]->lcmd,stream);
		put_str(cfg->xedit[i]->rcmd,stream);
		put_int(cfg->xedit[i]->misc,stream);
		put_str(cfg->xedit[i]->arstr,stream);
		put_int(cfg->xedit[i]->type,stream);
		c = cfg->xedit[i]->soft_cr;
		put_int(c,stream);
		n=0;
		put_int(cfg->xedit[i]->quotewrap_cols, stream);
		for(j=0;j<6;j++)
			put_int(n,stream);
		}

	put_int(cfg->total_xtrnsecs,stream);
	for(i=0;i<cfg->total_xtrnsecs;i++) {
		put_str(cfg->xtrnsec[i]->name,stream);
		put_str(cfg->xtrnsec[i]->code,stream);
		put_str(cfg->xtrnsec[i]->arstr,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream);
		}

	/* Calculate and save the actual number (total) of xtrn programs that will be written */
	n = 0;
	for (i = 0; i < cfg->total_xtrns; i++)
		if (cfg->xtrn[i]->sec < cfg->total_xtrnsecs	/* Total VALID xtrn progs */
			&& cfg->xtrn[i]->name[0]
			&& cfg->xtrn[i]->code[0])
			n++;
	put_int(n,stream);
	for(sec=0;sec<cfg->total_xtrnsecs;sec++)
		for(i=0;i<cfg->total_xtrns;i++) {
			if(cfg->xtrn[i]->name[0] == 0
				|| cfg->xtrn[i]->code[0] == 0)
				continue;
			if(cfg->xtrn[i]->sec!=sec)
				continue;
			put_int(cfg->xtrn[i]->sec,stream);
			put_str(cfg->xtrn[i]->name,stream);
			put_str(cfg->xtrn[i]->code,stream);
			put_str(cfg->xtrn[i]->arstr,stream);
			put_str(cfg->xtrn[i]->run_arstr,stream);
			put_int(cfg->xtrn[i]->type,stream);
			put_int(cfg->xtrn[i]->misc,stream);
			put_int(cfg->xtrn[i]->event,stream);
			put_int(cfg->xtrn[i]->cost,stream);
			put_str(cfg->xtrn[i]->cmd,stream);
			put_str(cfg->xtrn[i]->clean,stream);
			put_str(cfg->xtrn[i]->path,stream);
			put_int(cfg->xtrn[i]->textra,stream);
			put_int(cfg->xtrn[i]->maxtime,stream);
			n=0;
			for(j=0;j<7;j++)
				put_int(n,stream);
			}

	put_int(cfg->total_events,stream);
	for(i=0;i<cfg->total_events;i++) {
		put_str(cfg->event[i]->code,stream);
		put_str(cfg->event[i]->cmd,stream);
		put_int(cfg->event[i]->days,stream);
		put_int(cfg->event[i]->time,stream);
		put_int(cfg->event[i]->node,stream);
		put_int(cfg->event[i]->misc,stream);
		put_str(cfg->event[i]->dir,stream);
		put_int(cfg->event[i]->freq,stream);
		put_int(cfg->event[i]->mdays,stream);
		put_int(cfg->event[i]->months,stream);
		put_int(cfg->event[i]->errlevel,stream);
		c=0;
		put_int(c,stream);
		n=0;
		for(j=0;j<3;j++)
			put_int(n,stream);
		}

	put_int(cfg->total_natvpgms,stream);
	for(i=0;i<cfg->total_natvpgms;i++)
		put_str(cfg->natvpgm[i]->name,stream);
	for(i=0;i<cfg->total_natvpgms;i++)
		put_int(cfg->natvpgm[i]->misc,stream);

	put_int(cfg->total_hotkeys,stream);
	for(i=0;i<cfg->total_hotkeys;i++) {
		put_int(cfg->hotkey[i]->key,stream);
		put_str(cfg->hotkey[i]->cmd,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream);
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
