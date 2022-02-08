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

BOOL allocerr(FILE* fp, char* error, size_t maxerrlen, long offset, const char *fname, size_t size)
{
	safe_snprintf(error, maxerrlen, "offset %ld in %s, allocating %u bytes of memory"
		,offset,fname, (uint)size);
	fclose(fp);
	return(FALSE);
}

/****************************************************************************/
/* Reads in NODE.CNF and initializes the associated variables				*/
/****************************************************************************/
BOOL read_node_cfg(scfg_t* cfg, char* error, size_t maxerrlen)
{
	char	c,str[MAX_PATH+1];
	int 	i;
	long	offset=0;
	FILE	*instream;

	const char* fname = "node.cnf";
	SAFEPRINTF2(str,"%s%s",cfg->node_dir,fname);
	if((instream=fnopen(NULL,str,O_RDONLY))==NULL) {
		safe_snprintf(error, maxerrlen, "%d (%s) opening %s",errno,STRERROR(errno),str);
		return(FALSE); 
	}

	get_int(cfg->node_num,instream);
	if(!cfg->node_num) {
		safe_snprintf(error, maxerrlen,"offset %ld in %s, Node number must be non-zero"
			,offset,fname);
		fclose(instream);
		return(FALSE); 
	}
	get_str(cfg->node_name,instream);
	get_str(cfg->node_phone,instream);
	get_str(cfg->node_comspec,instream);
	get_int(cfg->node_misc,instream);
	get_int(cfg->node_ivt,instream);
	get_int(cfg->node_swap,instream);
	get_str(cfg->node_swapdir,instream);
	get_int(cfg->node_valuser,instream);
	get_int(cfg->node_minbps,instream);
	get_str(cfg->node_arstr,instream);
	arstr(NULL,cfg->node_arstr, cfg, cfg->node_ar);

	get_int(cfg->node_dollars_per_call,instream);
	get_str(cfg->node_editor,instream);
	get_str(cfg->node_viewer,instream);
	get_str(cfg->node_daily,instream);
	get_int(c,instream);
	if(c) cfg->node_scrnlen=c;
	get_int(cfg->node_scrnblank,instream);
	get_str(cfg->text_dir,instream); 				/* ctrl directory */
	get_str(cfg->text_dir,instream); 				/* text directory */
	if(!cfg->text_dir[0])
		SAFECOPY(cfg->text_dir, "../text/");
	get_str(cfg->temp_dir,instream); 				/* temp directory */
	SAFECOPY(cfg->temp_dir,"temp");

	for(i=0;i<10;i++)  						/* WFC 0-9 DOS commands */
		get_str(cfg->wfc_cmd[i],instream); 
	for(i=0;i<12;i++)  						/* WFC F1-F12 shrinking DOS cmds */
		get_str(cfg->wfc_scmd[i],instream); 
	get_str(cfg->mdm_hang,instream);
	get_int(cfg->node_sem_check,instream);
	if(!cfg->node_sem_check) cfg->node_sem_check=60;
	get_int(cfg->node_stat_check,instream);
	if(!cfg->node_stat_check) cfg->node_stat_check=10;
	get_str(cfg->scfg_cmd,instream);	// unused
	get_int(cfg->sec_warn,instream);
	if(!cfg->sec_warn)
		cfg->sec_warn=180;
	get_int(cfg->sec_hangup,instream);
	if(!cfg->sec_hangup)
		cfg->sec_hangup=300;
	get_int(cfg->node_erruser, instream);
	get_int(cfg->node_errlevel, instream);

	fclose(instream);
	return(TRUE);
}

/****************************************************************************/
/* Reads in MAIN.CNF and initializes the associated variables				*/
/****************************************************************************/
BOOL read_main_cfg(scfg_t* cfg, char* error, size_t maxerrlen)
{
	char	str[MAX_PATH+1],c;
	short	i,j;
	int16_t	n;
	long	offset=0;
	FILE	*instream;

	const char* fname = "main.cnf";
	SAFEPRINTF2(str,"%s%s",cfg->ctrl_dir,fname);
	if((instream=fnopen(NULL,str,O_RDONLY))==NULL) {
		safe_snprintf(error, maxerrlen,"%d (%s) opening %s",errno,STRERROR(errno),str);
		return(FALSE); 
	}

	get_str(cfg->sys_name,instream);
	get_str(cfg->sys_id,instream);
	get_str(cfg->sys_location,instream);
	get_str(cfg->sys_phonefmt,instream);
	get_str(cfg->sys_op,instream);
	get_str(cfg->sys_guru,instream);
	get_str(cfg->sys_pass,instream);
	get_int(cfg->sys_nodes,instream);

	for(i=0;i<cfg->sys_nodes;i++) {
		get_str(cfg->node_path[i],instream);
#if defined(__unix__)
		strlwr(cfg->node_path[i]);
#endif
	}

	get_str(cfg->data_dir,instream); 			  /* data directory */
	get_str(cfg->exec_dir,instream); 			  /* exec directory */

	get_str(cfg->sys_logon,instream);
	get_str(cfg->sys_logout,instream);
	get_str(cfg->sys_daily,instream);
	get_int(cfg->sys_timezone,instream);
	get_int(cfg->sys_misc,instream);
	get_int(cfg->sys_lastnode,instream);
	get_int(cfg->sys_autonode,instream);
	get_int(cfg->uq,instream);
	get_int(cfg->sys_pwdays,instream);
	get_int(cfg->sys_deldays,instream);
	get_int(cfg->sys_exp_warn,instream); 	/* Days left till expiration warning */
	get_int(cfg->sys_autodel,instream);
	get_int(cfg->sys_def_stat,instream); 	/* default status line */

	get_str(cfg->sys_chat_arstr,instream);
	arstr(NULL,cfg->sys_chat_arstr,cfg,cfg->sys_chat_ar);

	get_int(cfg->cdt_min_value,instream);
	get_int(cfg->max_minutes,instream);
	get_int(cfg->cdt_per_dollar,instream);
	get_str(cfg->new_pass,instream);
	get_str(cfg->new_magic,instream);
	get_str(cfg->new_sif,instream);
	get_str(cfg->new_sof,instream);
	if(!cfg->new_sof[0])		/* if output not specified, use input file */
		SAFECOPY(cfg->new_sof,cfg->new_sif);

	/*********************/
	/* New User Settings */
	/*********************/

	get_int(cfg->new_level,instream);
	get_int(cfg->new_flags1,instream);
	get_int(cfg->new_flags2,instream);
	get_int(cfg->new_flags3,instream);
	get_int(cfg->new_flags4,instream);
	get_int(cfg->new_exempt,instream);
	get_int(cfg->new_rest,instream);
	get_int(cfg->new_cdt,instream);
	get_int(cfg->new_min,instream);
	get_str(cfg->new_xedit,instream);
	get_int(cfg->new_expire,instream);
	get_int(cfg->new_shell,instream);
	get_int(cfg->new_misc,instream);
	get_int(cfg->new_prot,instream);
	if(cfg->new_prot<' ')
		cfg->new_prot=' ';
	get_int(cfg->new_install,instream);
	get_int(cfg->new_msgscan_init,instream);
	get_int(cfg->guest_msgscan_init,instream);
	get_int(cfg->min_pwlen, instream);
	if(cfg->min_pwlen < MIN_PASS_LEN)
		cfg->min_pwlen = MIN_PASS_LEN;
	if(cfg->min_pwlen > LEN_PASS)
		cfg->min_pwlen = LEN_PASS;
	get_int(c, instream);
	for(i=0;i<4;i++)
		get_int(n,instream);

	/*************************/
	/* Expired User Settings */
	/*************************/

	get_int(cfg->expired_level,instream);
	get_int(cfg->expired_flags1,instream);
	get_int(cfg->expired_flags2,instream);
	get_int(cfg->expired_flags3,instream);
	get_int(cfg->expired_flags4,instream);
	get_int(cfg->expired_exempt,instream);
	get_int(cfg->expired_rest,instream);

	get_str(cfg->logon_mod,instream);
	get_str(cfg->logoff_mod,instream);
	get_str(cfg->newuser_mod,instream);
	get_str(cfg->login_mod,instream);
	if(!cfg->login_mod[0]) SAFECOPY(cfg->login_mod,"login");
	get_str(cfg->logout_mod,instream);
	get_str(cfg->sync_mod,instream);
	get_str(cfg->expire_mod,instream);
	get_int(cfg->ctrlkey_passthru,instream);
	get_str(cfg->mods_dir,instream);
	get_str(cfg->logs_dir,instream);
	if(!cfg->logs_dir[0]) SAFECOPY(cfg->logs_dir,cfg->data_dir);
	get_str(cfg->readmail_mod, instream);
	get_str(cfg->scanposts_mod, instream);
	get_str(cfg->scansubs_mod, instream);
	get_str(cfg->listmsgs_mod, instream);
	get_str(cfg->textsec_mod,instream);
	if(!cfg->textsec_mod[0]) SAFECOPY(cfg->textsec_mod,"text_sec");
	get_str(cfg->automsg_mod,instream);
	if(!cfg->automsg_mod[0]) SAFECOPY(cfg->automsg_mod,"automsg");
	get_str(cfg->xtrnsec_mod,instream);
	if(!cfg->xtrnsec_mod[0]) SAFECOPY(cfg->xtrnsec_mod,"xtrn_sec");

	for(i=0;i<17;i++)					/* unused - initialized to NULL */
		get_int(n,instream);
	get_str(cfg->nodelist_mod,instream);
	if(cfg->nodelist_mod[0] == '\xff')
		SAFECOPY(cfg->nodelist_mod, "nodelist");
	get_str(cfg->whosonline_mod,instream);
	if(cfg->whosonline_mod[0] == '\xff')
		SAFECOPY(cfg->whosonline_mod, "nodelist -active");
	get_str(cfg->privatemsg_mod,instream);
	if(cfg->privatemsg_mod[0] == '\xff')
		SAFECOPY(cfg->privatemsg_mod, "privatemsg");
	get_str(cfg->logonlist_mod,instream);
	if(cfg->logonlist_mod[0] == '\xff')
		SAFECOPY(cfg->logonlist_mod, "logonlist");

	get_str(cfg->prextrn_mod,instream);
	if(cfg->prextrn_mod[0] == '\xff')
	    SAFECOPY(cfg->prextrn_mod, "prextrn");
	get_str(cfg->postxtrn_mod,instream);
	if(cfg->postxtrn_mod[0] == '\xff')
	    SAFECOPY(cfg->postxtrn_mod, "postxtrn");

	get_str(cfg->tempxfer_mod, instream);
	if(cfg->tempxfer_mod[0] == '\xff') 
	    SAFECOPY(cfg->tempxfer_mod, "tempxfer");

	for(i=0;i<92;i++)					/* unused - initialized to 0xff */
		get_int(n,instream);

	get_str(cfg->new_genders, instream);
	if(cfg->new_genders[0] == '\xff')
		SAFECOPY(cfg->new_genders, "MFX");

	get_int(cfg->user_backup_level,instream);
	if(cfg->user_backup_level==0xffff)
		cfg->user_backup_level=5;
	get_int(cfg->mail_backup_level,instream);
	if(cfg->mail_backup_level==0xffff)
		cfg->mail_backup_level=5;

	/*******************/
	/* Validation Sets */
	/*******************/

	for(i=0;i<10 && !feof(instream);i++) {
		get_int(cfg->val_level[i],instream);
		get_int(cfg->val_expire[i],instream);
		get_int(cfg->val_flags1[i],instream);
		get_int(cfg->val_flags2[i],instream);
		get_int(cfg->val_flags3[i],instream);
		get_int(cfg->val_flags4[i],instream);
		get_int(cfg->val_cdt[i],instream);
		get_int(cfg->val_exempt[i],instream);
		get_int(cfg->val_rest[i],instream);
		for(j=0;j<8;j++)
			get_int(n,instream);
	}

	/***************************/
	/* Security Level Settings */
	/***************************/

	for(i=0;i<100 && !feof(instream);i++) {
		get_int(cfg->level_timeperday[i],instream);
		get_int(cfg->level_timepercall[i],instream);
		get_int(cfg->level_callsperday[i],instream);
		get_int(cfg->level_freecdtperday[i],instream);
		get_int(cfg->level_linespermsg[i],instream);
		get_int(cfg->level_postsperday[i],instream);
		get_int(cfg->level_emailperday[i],instream);
		get_int(cfg->level_misc[i],instream);
		get_int(cfg->level_expireto[i],instream);
		get_int(c,instream);
		for(j=0;j<5;j++)
			get_int(n,instream); 
	}
	if(i!=100) {
		safe_snprintf(error, maxerrlen,"Insufficient User Level Information: "
			"%d user levels read, 100 needed.",i);
		fclose(instream);
		return(FALSE); 
	}

	get_int(cfg->total_shells,instream);
	#ifdef SBBS
	if(!cfg->total_shells) {
		safe_snprintf(error, maxerrlen,"At least one command shell must be configured.");
		fclose(instream);
		return(FALSE); 
	}
	#endif

	if(cfg->total_shells) {
		if((cfg->shell=(shell_t **)malloc(sizeof(shell_t *)*cfg->total_shells))==NULL)
			return allocerr(instream, error, maxerrlen, offset,fname,sizeof(shell_t *)*cfg->total_shells);
	} else
		cfg->shell=NULL;

	for(i=0;i<cfg->total_shells;i++) {
		if(feof(instream)) break;
		if((cfg->shell[i]=(shell_t *)malloc(sizeof(shell_t)))==NULL)
			return allocerr(instream, error, maxerrlen, offset,fname,sizeof(shell_t));
		memset(cfg->shell[i],0,sizeof(shell_t));

		get_str(cfg->shell[i]->name,instream);
		get_str(cfg->shell[i]->code,instream);
		get_str(cfg->shell[i]->arstr,instream);
		arstr(NULL,cfg->shell[i]->arstr,cfg,cfg->shell[i]->ar);
		get_int(cfg->shell[i]->misc,instream);
		for(j=0;j<8;j++)
			get_int(n,instream);
	}
	cfg->total_shells=i;

	if(cfg->new_shell>=cfg->total_shells)
		cfg->new_shell=0;

	fclose(instream);
	return(TRUE);
}

/****************************************************************************/
/* Reads in MSGS.CNF and initializes the associated variables				*/
/****************************************************************************/
BOOL read_msgs_cfg(scfg_t* cfg, char* error, size_t maxerrlen)
{
	char	str[MAX_PATH+1],c;
	short	i,j;
	int16_t	n,k;
	long	offset=0;
	FILE	*instream;

	const char* fname = "msgs.cnf";
	SAFEPRINTF2(str,"%s%s",cfg->ctrl_dir,fname);
	if((instream=fnopen(NULL,str,O_RDONLY))==NULL) {
		safe_snprintf(error, maxerrlen,"%d (%s) opening %s",errno,STRERROR(errno),str);
		return(FALSE);
	}

	/*************************/
	/* General Message Stuff */
	/*************************/

	get_int(cfg->max_qwkmsgs,instream);
	get_int(cfg->mail_maxcrcs,instream);
	get_int(cfg->mail_maxage,instream);
	get_str(cfg->preqwk_arstr,instream);
	arstr(NULL, cfg->preqwk_arstr, cfg, cfg->preqwk_ar);

	get_int(cfg->smb_retry_time,instream);	 /* odd byte */
	if(!cfg->smb_retry_time)
		cfg->smb_retry_time=30;
	get_int(cfg->max_qwkmsgage, instream);
	get_int(cfg->max_spamage, instream);
	for(i=0;i<232;i++)	/* NULL */
		get_int(n,instream);
	get_int(cfg->msg_misc,instream);
	for(i=0;i<255;i++)	/* 0xff */
		get_int(n,instream);

	/******************/
	/* Message Groups */
	/******************/

	get_int(cfg->total_grps,instream);

	if(cfg->total_grps) {
		if((cfg->grp=(grp_t **)malloc(sizeof(grp_t *)*cfg->total_grps))==NULL)
			return allocerr(instream, error, maxerrlen, offset,fname,sizeof(grp_t *)*cfg->total_grps);
	} else
		cfg->grp=NULL;

	for(i=0;i<cfg->total_grps;i++) {

		if(feof(instream)) break;
		if((cfg->grp[i]=(grp_t *)malloc(sizeof(grp_t)))==NULL)
			return allocerr(instream, error, maxerrlen, offset,fname,sizeof(grp_t));
		memset(cfg->grp[i],0,sizeof(grp_t));

		get_str(cfg->grp[i]->lname,instream);
		get_str(cfg->grp[i]->sname,instream);

		get_str(cfg->grp[i]->arstr,instream);
		arstr(NULL, cfg->grp[i]->arstr, cfg, cfg->grp[i]->ar);

		get_str(cfg->grp[i]->code_prefix,instream);

		get_int(c,instream);
		cfg->grp[i]->sort = c;
		for(j=0;j<43;j++)
			get_int(n,instream);
	}
	cfg->total_grps=i;

	/**********************/
	/* Message Sub-boards */
	/**********************/

	get_int(cfg->total_subs,instream);

	if(cfg->total_subs) {
		if((cfg->sub=(sub_t **)malloc(sizeof(sub_t *)*cfg->total_subs))==NULL)
			return allocerr(instream, error, maxerrlen, offset,fname,sizeof(sub_t *)*cfg->total_subs);
	} else
		cfg->sub=NULL;

	for(i=0;i<cfg->total_subs;i++) {
		if(feof(instream)) break;
		if((cfg->sub[i]=(sub_t *)malloc(sizeof(sub_t)))==NULL)
			return allocerr(instream, error, maxerrlen, offset,fname,sizeof(sub_t));
		memset(cfg->sub[i],0,sizeof(sub_t));

		cfg->sub[i]->subnum = i;

		get_int(cfg->sub[i]->grp,instream);
		get_str(cfg->sub[i]->lname,instream);
		get_str(cfg->sub[i]->sname,instream);
		get_str(cfg->sub[i]->qwkname,instream);
		get_str(cfg->sub[i]->code_suffix,instream);
		get_str(cfg->sub[i]->data_dir,instream);

#ifdef SBBS
		if(cfg->sub[i]->grp >= cfg->total_grps) {
			safe_snprintf(error, maxerrlen,"offset %ld in %s: invalid group number (%u) for sub-board: %s"
				,offset,fname
				,cfg->sub[i]->grp
				,cfg->sub[i]->code_suffix);
			fclose(instream);
			return(FALSE); 
		}
#endif

		get_str(cfg->sub[i]->arstr,instream);
		get_str(cfg->sub[i]->read_arstr,instream);
		get_str(cfg->sub[i]->post_arstr,instream);
		get_str(cfg->sub[i]->op_arstr,instream);

		arstr(NULL, cfg->sub[i]->arstr, cfg, cfg->sub[i]->ar);
		arstr(NULL, cfg->sub[i]->read_arstr, cfg, cfg->sub[i]->read_ar);
		arstr(NULL, cfg->sub[i]->post_arstr, cfg, cfg->sub[i]->post_ar);
		arstr(NULL, cfg->sub[i]->op_arstr, cfg, cfg->sub[i]->op_ar);

		get_int(cfg->sub[i]->misc,instream);
		if((cfg->sub[i]->misc&(SUB_FIDO|SUB_INET)) && !(cfg->sub[i]->misc&SUB_QNET))
			cfg->sub[i]->misc|=SUB_NOVOTING;

		get_str(cfg->sub[i]->tagline,instream);
		get_str(cfg->sub[i]->origline,instream);
		get_str(cfg->sub[i]->post_sem,instream);

		get_str(cfg->sub[i]->newsgroup,instream);

		get_int(cfg->sub[i]->faddr,instream);			/* FidoNet address */
		get_int(cfg->sub[i]->maxmsgs,instream);
		get_int(cfg->sub[i]->maxcrcs,instream);
		get_int(cfg->sub[i]->maxage,instream);
		get_int(cfg->sub[i]->ptridx,instream);
#ifdef SBBS
		for(j=0;j<i;j++)
			if(cfg->sub[i]->ptridx==cfg->sub[j]->ptridx) {
				safe_snprintf(error, maxerrlen,"offset %ld in %s: Duplicate pointer index for subs %s and %s"
					,offset,fname
					,cfg->sub[i]->code_suffix,cfg->sub[j]->code_suffix);
				fclose(instream);
				return(FALSE); 
			}
#endif

		get_str(cfg->sub[i]->mod_arstr,instream);
		arstr(NULL, cfg->sub[i]->mod_arstr, cfg,cfg->sub[i]->mod_ar);

		get_int(cfg->sub[i]->qwkconf,instream);
		get_int(c,instream); // unused
		get_int(cfg->sub[i]->pmode,instream);
		get_int(cfg->sub[i]->n_pmode,instream);
		get_str(cfg->sub[i]->area_tag, instream);
		get_int(c,instream);
		get_int(n,instream);
	}
	cfg->total_subs=i;

	/***********/
	/* FidoNet */
	/***********/

	get_int(cfg->total_faddrs,instream);

	if(cfg->total_faddrs) {
		if((cfg->faddr=(faddr_t *)malloc(sizeof(faddr_t)*cfg->total_faddrs))==NULL)
			return allocerr(instream, error, maxerrlen, offset,fname,sizeof(faddr_t)*cfg->total_faddrs);
	} else
		cfg->faddr=NULL;

	for(i=0;i<cfg->total_faddrs;i++)
		get_int(cfg->faddr[i],instream);

	// Sanity-check each sub's FidoNet-style address
	for(i = 0; i < cfg->total_subs; i++)
		cfg->sub[i]->faddr = *nearest_sysfaddr(cfg, &cfg->sub[i]->faddr);

	get_str(cfg->origline,instream);
	get_str(cfg->netmail_sem,instream);
	get_str(cfg->echomail_sem,instream);
	get_str(cfg->netmail_dir,instream);
	get_str(cfg->echomail_dir,instream);
	get_str(cfg->fidofile_dir,instream);
	get_int(cfg->netmail_misc,instream);
	get_int(cfg->netmail_cost,instream);
	get_int(cfg->dflt_faddr,instream);
	for(i=0;i<28;i++)
		get_int(n,instream);

	/**********/
	/* QWKnet */
	/**********/

	get_str(cfg->qnet_tagline,instream);

	get_int(cfg->total_qhubs,instream);

	if(cfg->total_qhubs) {
		if((cfg->qhub=(qhub_t **)malloc(sizeof(qhub_t *)*cfg->total_qhubs))==NULL)
			return allocerr(instream, error, maxerrlen, offset,fname,sizeof(qhub_t*)*cfg->total_qhubs);
	} else
		cfg->qhub=NULL;

	for(i=0;i<cfg->total_qhubs;i++) {
		if(feof(instream)) break;
		if((cfg->qhub[i]=(qhub_t *)malloc(sizeof(qhub_t)))==NULL)
			return allocerr(instream, error, maxerrlen, offset,fname,sizeof(qhub_t));
		memset(cfg->qhub[i],0,sizeof(qhub_t));

		get_str(cfg->qhub[i]->id,instream);
		get_int(cfg->qhub[i]->time,instream);
		get_int(cfg->qhub[i]->freq,instream);
		get_int(cfg->qhub[i]->days,instream);
		get_int(cfg->qhub[i]->node,instream);
		get_str(cfg->qhub[i]->call,instream);
		get_str(cfg->qhub[i]->pack,instream);
		get_str(cfg->qhub[i]->unpack,instream);
		get_int(k,instream);

		if(k) {
			if((cfg->qhub[i]->sub=(sub_t**)malloc(sizeof(sub_t*)*k))==NULL)
				return allocerr(instream, error, maxerrlen, offset,fname,sizeof(ulong)*k);
			if((cfg->qhub[i]->conf=(ushort *)malloc(sizeof(ushort)*k))==NULL)
				return allocerr(instream, error, maxerrlen, offset,fname,sizeof(ushort)*k);
			if((cfg->qhub[i]->mode=(char *)malloc(sizeof(char)*k))==NULL)
				return allocerr(instream, error, maxerrlen, offset,fname,sizeof(uchar)*k);
		}

		for(j=0;j<k;j++) {
			uint16_t	confnum;
			uint16_t	subnum;
			uint8_t		mode;
			if(feof(instream)) break;
			get_int(confnum,instream);
			get_int(subnum, instream);
			get_int(mode, instream);
			if(subnum < cfg->total_subs) {
				cfg->sub[subnum]->misc |= SUB_QNET;
				cfg->qhub[i]->sub[cfg->qhub[i]->subs]	= cfg->sub[subnum];
				cfg->qhub[i]->mode[cfg->qhub[i]->subs]	= mode;
				cfg->qhub[i]->conf[cfg->qhub[i]->subs]	= confnum;
				cfg->qhub[i]->subs++;
			}
		}
		get_int(cfg->qhub[i]->misc, instream);
		get_str(cfg->qhub[i]->fmt,instream);
		for(j=0;j<28;j++)
			get_int(n,instream);
	}

	cfg->total_qhubs=i;

	for(j=0;j<32;j++)
		get_int(n,instream);

	/************/
	/* PostLink */
	/************/

	fread(str,11,1,instream);		/* Unused - used to be Site Name */
	offset+=11;
	get_int(cfg->sys_psnum,instream);	/* Site Number */
	get_int(cfg->total_phubs,instream);

	if(cfg->total_phubs) {
		if((cfg->phub=(phub_t **)malloc(sizeof(phub_t *)*cfg->total_phubs))==NULL)
			return allocerr(instream, error, maxerrlen, offset,fname,sizeof(phub_t*)*cfg->total_phubs);
	} else
		cfg->phub=NULL;

	for(i=0;i<cfg->total_phubs;i++) {
		if(feof(instream)) break;
		if((cfg->phub[i]=(phub_t *)malloc(sizeof(phub_t)))==NULL)
			return allocerr(instream, error, maxerrlen, offset,fname,sizeof(phub_t));
		memset(cfg->phub[i],0,sizeof(phub_t));

		get_str(cfg->phub[i]->name,instream);
		get_int(cfg->phub[i]->time,instream);
		get_int(cfg->phub[i]->freq,instream);
		get_int(cfg->phub[i]->days,instream);
		get_int(cfg->phub[i]->node,instream);
		get_str(cfg->phub[i]->call,instream);
		for(j=0;j<32;j++)
			get_int(n,instream);
	}

	cfg->total_phubs=i;

	get_str(cfg->sys_psname,instream);	/* Site Name */

	for(j=0;j<32;j++)
		get_int(n,instream);

	/************/
	/* Internet */
	/************/

	get_str(cfg->sys_inetaddr,instream); /* Internet address */
	get_str(cfg->inetmail_sem,instream);
	get_int(cfg->inetmail_misc,instream);
	get_int(cfg->inetmail_cost,instream);
	get_str(cfg->smtpmail_sem,instream);

	fclose(instream);
	return(TRUE);
}

void free_node_cfg(scfg_t* cfg)
{
	if(cfg->mdm_result!=NULL) {
		FREE_AND_NULL(cfg->mdm_result);
	}
}

void free_main_cfg(scfg_t* cfg)
{
	int i;

	if(cfg->shell!=NULL) {
		for(i=0;i<cfg->total_shells;i++) {
			FREE_AND_NULL(cfg->shell[i]);
		}
		FREE_AND_NULL(cfg->shell);
	}
	cfg->total_shells=0;
}

void free_msgs_cfg(scfg_t* cfg)
{
	int i;

	if(cfg->grp!=NULL) {
		for(i=0;i<cfg->total_grps;i++) {
			FREE_AND_NULL(cfg->grp[i]);
		}
		FREE_AND_NULL(cfg->grp);
	}
	cfg->total_grps=0;

	if(cfg->sub!=NULL) {
		for(i=0;i<cfg->total_subs;i++) {
			FREE_AND_NULL(cfg->sub[i]);
		}
		FREE_AND_NULL(cfg->sub);
	}
	cfg->total_subs=0;

	FREE_AND_NULL(cfg->faddr);
	cfg->total_faddrs=0;

	if(cfg->qhub!=NULL) {
		for(i=0;i<cfg->total_qhubs;i++) {
			FREE_AND_NULL(cfg->qhub[i]->mode);
			FREE_AND_NULL(cfg->qhub[i]->conf);
			FREE_AND_NULL(cfg->qhub[i]->sub);
			FREE_AND_NULL(cfg->qhub[i]);
		}
		FREE_AND_NULL(cfg->qhub);
	}
	cfg->total_qhubs=0;

	if(cfg->phub!=NULL) {
		for(i=0;i<cfg->total_phubs;i++) {
			FREE_AND_NULL(cfg->phub[i]);
		}
		FREE_AND_NULL(cfg->phub);
	}
	cfg->total_phubs=0;
}

/************************************************************/
/* Create data and sub-dirs off data if not already created */
/************************************************************/
void make_data_dirs(scfg_t* cfg)
{
	char	str[MAX_PATH+1];

	md(cfg->data_dir);
	SAFEPRINTF(str,"%ssubs",cfg->data_dir);
	md(str);
	SAFEPRINTF(str,"%sdirs",cfg->data_dir);
	md(str);
	SAFEPRINTF(str,"%stext",cfg->data_dir);
	md(str);
	SAFEPRINTF(str,"%smsgs",cfg->data_dir);
	md(str);
	SAFEPRINTF(str,"%suser",cfg->data_dir);
	md(str);
	SAFEPRINTF(str,"%sqnet",cfg->data_dir);
	md(str);
	SAFEPRINTF(str,"%sfile",cfg->data_dir);
	md(str);

	md(cfg->logs_dir);
	SAFEPRINTF(str,"%slogs",cfg->logs_dir);
	md(str);

	if(cfg->mods_dir[0])
		md(cfg->mods_dir);

	for(int i = 0; i < cfg->total_dirs; i++) {
		md(cfg->dir[i]->data_dir);
		if(cfg->dir[i]->misc & DIR_FCHK) 
			md(cfg->dir[i]->path);
	}
}

int getdirnum(scfg_t* cfg, const char* code)
{
	char fullcode[LEN_EXTCODE + 1];
	size_t i;

	if(code == NULL || *code == '\0')
		return -1;

	for(i=0;i<cfg->total_dirs;i++) {
		if(cfg->dir[i]->code[0] == '\0' && cfg->dir[i]->lib < cfg->total_libs ) {
			SAFEPRINTF2(fullcode, "%s%s", cfg->lib[cfg->dir[i]->lib]->code_prefix, cfg->dir[i]->code_suffix);
			if(stricmp(fullcode, code) == 0)
				return i;
		} else {
			if(stricmp(cfg->dir[i]->code, code)==0)
				return i;
		}
	}
	return -1;
}

int getlibnum(scfg_t* cfg, const char* code)
{
	int i = getdirnum(cfg, code);

	if(i >= 0)
		return cfg->dir[i]->lib;
	return i;
}

int getsubnum(scfg_t* cfg, const char* code)
{
	char fullcode[LEN_EXTCODE + 1];
	size_t i;

	if(code == NULL || *code == '\0')
		return -1;

	for(i=0;i<cfg->total_subs;i++) {
		if(cfg->sub[i]->code[0] == '\0' && cfg->sub[i]->grp < cfg->total_grps ) {
			SAFEPRINTF2(fullcode, "%s%s", cfg->grp[cfg->sub[i]->grp]->code_prefix, cfg->sub[i]->code_suffix);
			if(stricmp(fullcode, code) == 0)
				return i;
		} else {
			if(stricmp(cfg->sub[i]->code,code) == 0)
				return i;
		}
	}
	return -1;
}

int getgrpnum(scfg_t* cfg, const char* code)
{
	int i = getdirnum(cfg, code);

	if(i >= 0)
			return cfg->sub[i]->grp;
	return i;
}

faddr_t* nearest_sysfaddr(scfg_t* cfg, faddr_t* addr)
{
	uint i;

	if(cfg->total_faddrs <= 0)
		return addr;

	for(i=0; i < cfg->total_faddrs; i++)
		if(memcmp(addr, &cfg->faddr[i], sizeof(*addr)) == 0)
			return &cfg->faddr[i];
	for(i=0; i < cfg->total_faddrs; i++)
		if(addr->zone == cfg->faddr[i].zone
			&& addr->net == cfg->faddr[i].net
			&& addr->node == cfg->faddr[i].node)
			return &cfg->faddr[i];
	for(i=0; i < cfg->total_faddrs; i++)
		if(addr->zone == cfg->faddr[i].zone
			&& addr->net == cfg->faddr[i].net)
			return &cfg->faddr[i];
	for(i=0; i < cfg->total_faddrs; i++)
		if(addr->zone == cfg->faddr[i].zone)
			return &cfg->faddr[i];
	return &cfg->faddr[0];
}
