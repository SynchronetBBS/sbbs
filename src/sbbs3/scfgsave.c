/* scfgsave.c */

/* Synchronet configuration file save routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"

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
BOOL DLLCALL save_cfg(scfg_t* cfg, int backup_level)
{
	int i;

	if(cfg->prepped)
		return(FALSE);

	for(i=0;i<cfg->sys_nodes;i++) {
		if(cfg->node_path[i][0]==0) 
			sprintf(cfg->node_path[i],"../node%d",i+1);
		cfg->node_num=i+1;
		if(!write_node_cfg(cfg,backup_level))
			return(FALSE);
	}
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

	return(TRUE);
}

static BOOL fcopy(char* src, char* dest)
{
	int		ch;
	FILE*	in;
	FILE*	out;

	if((in=fopen(src,"rb"))==NULL)
		return(FALSE);
	if((out=fopen(dest,"wb"))==NULL) {
		fclose(in);
		return(FALSE);
	}

	while(!feof(in)) {
		ch=fgetc(in);
		if(ch==EOF)
			break;
		fputc(ch,out);
	}

	fclose(in);
	fclose(out);

	return(TRUE);
}

/****************************************************************************/
/****************************************************************************/
void DLLCALL backup(char *fname, int backup_level, BOOL ren)
{
	char oldname[MAX_PATH+1];
	char newname[MAX_PATH+1];
	int i;

	for(i=backup_level;i;i--) {
		sprintf(newname,"%s.%d",fname,i-1);
		if(i==backup_level)
			remove(newname);
		if(i==1) {
			if(ren == TRUE)
				rename(fname,newname);
			else
				fcopy(fname,newname);
			continue; 
		}
		sprintf(oldname,"%s.%d",fname,i-2);
		rename(oldname,newname); 
	}
}

/****************************************************************************/
/****************************************************************************/
BOOL DLLCALL write_node_cfg(scfg_t* cfg, int backup_level)
{
	char	str[128];
	int 	i,file;
	short	n;
	FILE	*stream;

	if(cfg->prepped)
		return(FALSE);

	if(cfg->node_num<1)
		return(FALSE);

	sprintf(str,cfg->node_path[cfg->node_num-1]);
	prep_dir(cfg->ctrl_dir,str,sizeof(str));
	md(str);
	strcat(str,"node.cnf");
	backup(str, backup_level, TRUE);

	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
		|| (stream=fdopen(file,"wb"))==NULL) {
		return(FALSE); 
	}
	setvbuf(stream,NULL,_IOFBF,2048);

	put_int(cfg->node_num,stream);
	put_str(cfg->node_name,stream);
	put_str(cfg->node_phone,stream);
	put_str(cfg->node_comspec,stream);				 /* Was node_logon */
	put_int(cfg->node_misc,stream);
	put_int(cfg->node_ivt,stream);
	put_int(cfg->node_swap,stream);
#if 0
	if(cfg->node_swapdir[0]) {
		backslash(cfg->node_swapdir);
		md(cfg->node_swapdir);               /* make sure it's a valid directory */
	}
#endif
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
	n=0;
	for(i=0;i<188;i++)					/* unused init to NULL */
		fwrite(&n,1,2,stream);
	n=(ushort)0xffff;					/* unused init to 0xff */
	for(i=0;i<256;i++)
		fwrite(&n,1,2,stream);
	put_int(cfg->com_port,stream);
	put_int(cfg->com_irq,stream);
	put_int(cfg->com_base,stream);
	put_int(cfg->com_rate,stream);
	put_int(cfg->mdm_misc,stream);
	put_str(cfg->mdm_init,stream);
	put_str(cfg->mdm_spec,stream);
	put_str(cfg->mdm_term,stream);
	put_str(cfg->mdm_dial,stream);
	put_str(cfg->mdm_offh,stream);
	put_str(cfg->mdm_answ,stream);
	put_int(cfg->mdm_reinit,stream);
	put_int(cfg->mdm_ansdelay,stream);
	put_int(cfg->mdm_rings,stream);
	put_int(cfg->mdm_results,stream);
	for(i=0;i<cfg->mdm_results;i++) {
		put_int(cfg->mdm_result[i].code,stream);
		put_int(cfg->mdm_result[i].rate,stream);
		put_int(cfg->mdm_result[i].cps,stream);
		put_str(cfg->mdm_result[i].str,stream); 
	}
	fclose(stream);

	return(TRUE);
}

/****************************************************************************/
/****************************************************************************/
BOOL DLLCALL write_main_cfg(scfg_t* cfg, int backup_level)
{
	char	str[128],c=0;
	int 	file;
	short	i,j,n;
	FILE	*stream;

	if(cfg->prepped)
		return(FALSE);

	sprintf(str,"%smain.cnf",cfg->ctrl_dir);
	backup(str, backup_level, TRUE);

	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
		|| (stream=fdopen(file,"wb"))==NULL) {
		return(FALSE); 
	}
	setvbuf(stream,NULL,_IOFBF,2048);

	put_str(cfg->sys_name,stream);
	put_str(cfg->sys_id,stream);
	put_str(cfg->sys_location,stream);
	put_str(cfg->sys_phonefmt,stream);
	put_str(cfg->sys_op,stream);
	put_str(cfg->sys_guru,stream);
	put_str(cfg->sys_pass,stream);
	put_int(cfg->sys_nodes,stream);
	for(i=0;i<cfg->sys_nodes;i++) 
		put_str(cfg->node_path[i],stream);
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
	n=0;
	for(i=0;i<7;i++)
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

	put_int(c,stream);
	n=0;
	for(i=0;i<158;i++)
		put_int(n,stream);
	n=(ushort)0xffff;
	for(i=0;i<256;i++)
		put_int(n,stream);

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
			put_int(n,stream); }

	fclose(stream);

	return(TRUE);
}

/****************************************************************************/
/****************************************************************************/
BOOL DLLCALL write_msgs_cfg(scfg_t* cfg, int backup_level)
{
	char	str[128],c;
	char	dir[LEN_DIR+1]="";
	int 	i,j,k,file;
	short	n;
	long	l;
	FILE	*stream;
	smb_t	smb;

	if(cfg->prepped)
		return(FALSE);

	sprintf(str,"%smsgs.cnf",cfg->ctrl_dir);
	backup(str, backup_level, TRUE);

	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
		|| (stream=fdopen(file,"wb"))==NULL) {
		return(FALSE); 
	}
	setvbuf(stream,NULL,_IOFBF,2048);

	put_int(cfg->max_qwkmsgs,stream);
	put_int(cfg->mail_maxcrcs,stream);
	put_int(cfg->mail_maxage,stream);
	put_str(cfg->preqwk_arstr,stream);
	put_int(cfg->smb_retry_time,stream);
	n=0;
	for(i=0;i<235;i++)
		put_int(n,stream);
	n=(short)0xffff;
	for(i=0;i<256;i++)
		put_int(n,stream);

	/* Message Groups */

	put_int(cfg->total_grps,stream);
	for(i=0;i<cfg->total_grps;i++) {
		put_str(cfg->grp[i]->lname,stream);
		put_str(cfg->grp[i]->sname,stream);
		put_str(cfg->grp[i]->arstr,stream);
		put_str(cfg->grp[i]->code_prefix,stream);
		c=0;
		put_int(c,stream);
		n=0;
		for(j=0;j<27;j++)
			put_int(n,stream);
		n=(short)0xffff;
		for(j=0;j<16;j++)
			put_int(n,stream); }

	/* Message Sub-boards */

	backslash(cfg->echomail_dir);

	str[0]=0;
	for(i=n=0;i<cfg->total_subs;i++)
		if(cfg->sub[i]->grp<cfg->total_grps)		/* total VALID sub-boards */
			n++;
	put_int(n,stream);
	for(i=0;i<cfg->total_subs;i++) {
		if(cfg->sub[i]->grp>=cfg->total_grps) 	/* skip bogus group numbers */
			continue;
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
		put_str(cfg->sub[i]->echomail_sem,stream);
		put_str(cfg->sub[i]->newsgroup,stream);
		put_int(cfg->sub[i]->faddr,stream);
		put_int(cfg->sub[i]->maxmsgs,stream);
		put_int(cfg->sub[i]->maxcrcs,stream);
		put_int(cfg->sub[i]->maxage,stream);
		put_int(cfg->sub[i]->ptridx,stream);
		put_str(cfg->sub[i]->mod_arstr,stream);
		put_int(cfg->sub[i]->qwkconf,stream);
		c=0;
		put_int(c,stream);
		n=0;
		for(k=0;k<26;k++)
			put_int(n,stream);

		if(all_msghdr || (cfg->sub[i]->misc&SUB_HDRMOD && !no_msghdr)) {
			if(!cfg->sub[i]->data_dir[0])
				sprintf(smb.file,"%ssubs",cfg->data_dir);
			else
				sprintf(smb.file,"%s",cfg->sub[i]->data_dir);
			prep_dir(cfg->ctrl_dir,smb.file,sizeof(smb.file));
			strcat(smb.file,cfg->grp[cfg->sub[i]->grp]->code_prefix);
			strcat(smb.file,cfg->sub[i]->code_suffix);
			if(smb_open(&smb)!=0) {
				/* errormsg(WHERE,ERR_OPEN,smb.file,x); */
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
	put_str(cfg->echomail_dir,stream);
	backslash(cfg->fidofile_dir);
	put_str(cfg->fidofile_dir,stream);
	put_int(cfg->netmail_misc,stream);
	put_int(cfg->netmail_cost,stream);
	put_int(cfg->dflt_faddr,stream);
	n=0;
	for(i=0;i<28;i++)
		put_int(n,stream);

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
		put_int(cfg->qhub[i]->subs,stream);
		for(j=0;j<cfg->qhub[i]->subs;j++) {
			put_int(cfg->qhub[i]->conf[j],stream);
			put_int(cfg->qhub[i]->sub[j],stream);
			put_int(cfg->qhub[i]->mode[j],stream); }
		n=0;
		for(j=0;j<32;j++)
			put_int(n,stream); }
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
		sprintf(smb.file,"%smail",dir);
		if(smb_open(&smb)!=0) {
			return(FALSE); 
		}
		if(!filelength(fileno(smb.shd_fp))) {
			smb.status.max_msgs=MAX_SYSMAIL;
			smb.status.max_crcs=cfg->mail_maxcrcs;
			smb.status.max_age=cfg->mail_maxage;
			smb.status.attr=SMB_EMAIL;
			i=smb_create(&smb);
			smb_close(&smb);
			return(i==0);
		}
		if(smb_locksmbhdr(&smb)!=0) {
			smb_close(&smb);
			//errormsg(WHERE,ERR_LOCK,smb.file,x);
			return(FALSE); 
		}
		if(smb_getstatus(&smb)!=0) {
			smb_close(&smb);
			//errormsg(WHERE,ERR_READ,smb.file,x);
			return(FALSE); 
		}
		smb.status.max_msgs=MAX_SYSMAIL;
		smb.status.max_crcs=cfg->mail_maxcrcs;
		smb.status.max_age=cfg->mail_maxage;
		if(smb_putstatus(&smb)!=0) {
			smb_close(&smb);
			//errormsg(WHERE,ERR_WRITE,smb.file,x);
			return(FALSE); 
		}
		smb_close(&smb); 
	}

	return(TRUE);
}


/****************************************************************************/
/****************************************************************************/
BOOL DLLCALL write_file_cfg(scfg_t* cfg, int backup_level)
{
	char	str[128],cmd[LEN_CMD+1],c;
	char	path[MAX_PATH+1];
	int 	i,j,k,file;
	short	n;
	long	l=0;
	FILE	*stream;

	if(cfg->prepped)
		return(FALSE);

	sprintf(str,"%sfile.cnf",cfg->ctrl_dir);
	backup(str, backup_level, TRUE);

	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
		|| (stream=fdopen(file,"wb"))==NULL) {
		return(FALSE); 
	}
	setvbuf(stream,NULL,_IOFBF,2048);

	put_int(cfg->min_dspace,stream);
	put_int(cfg->max_batup,stream);
	put_int(cfg->max_batdn,stream);
	put_int(cfg->max_userxfer,stream);
	put_int(l,stream);					/* unused */
	put_int(cfg->cdt_up_pct,stream);
	put_int(cfg->cdt_dn_pct,stream);
	put_int(l,stream);					/* unused */
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

		c=0;
		put_int(c,stream);
		n=(short)0x0000;
		for(j=0;j<3;j++)
			put_int(n,stream); 

		n=(short)0xffff;
		for(j=0;j<16;j++)
			put_int(n,stream); 
	}

	/* File Directories */

	put_int(cfg->total_dirs,stream);
	for(j=0;j<cfg->total_libs;j++)
		for(i=0;i<cfg->total_dirs;i++)
			if(cfg->dir[i]->lib==j) {
				put_int(cfg->dir[i]->lib,stream);
				put_str(cfg->dir[i]->lname,stream);
				put_str(cfg->dir[i]->sname,stream);
				put_str(cfg->dir[i]->code_suffix,stream);
#if 1
				if(cfg->dir[i]->data_dir[0]) {
					backslash(cfg->dir[i]->data_dir);
					md(cfg->dir[i]->data_dir);
				}
#endif
				put_str(cfg->dir[i]->data_dir,stream);
				put_str(cfg->dir[i]->arstr,stream);
				put_str(cfg->dir[i]->ul_arstr,stream);
				put_str(cfg->dir[i]->dl_arstr,stream);
				put_str(cfg->dir[i]->op_arstr,stream);
				backslash(cfg->dir[i]->path);
				put_str(cfg->dir[i]->path,stream);
#if 1
				if(cfg->dir[i]->misc&DIR_FCHK) {
					SAFECOPY(path,cfg->dir[i]->path);
					if(!path[0])		/* no file storage path specified */
						sprintf(path,"%sdirs/%s%s/"
							,cfg->data_dir
							,cfg->lib[cfg->dir[i]->lib]->code_prefix
							,cfg->dir[i]->code_suffix);
					else if(cfg->lib[cfg->dir[i]->lib]->parent_path[0]) {
						SAFECOPY(path,cfg->lib[cfg->dir[i]->lib]->parent_path);
						prep_dir(cfg->ctrl_dir,path,sizeof(path));
						md(path);
						backslash(path);
						strcat(path,cfg->dir[i]->path);
					}
					else
						prep_dir(cfg->ctrl_dir, path,sizeof(path));
					md(path); 
				}
#endif

				put_str(cfg->dir[i]->upload_sem,stream);
				put_int(cfg->dir[i]->maxfiles,stream);
				put_str(cfg->dir[i]->exts,stream);
				put_int(cfg->dir[i]->misc,stream);
				put_int(cfg->dir[i]->seqdev,stream);
				put_int(cfg->dir[i]->sort,stream);
				put_str(cfg->dir[i]->ex_arstr,stream);
				put_int(cfg->dir[i]->maxage,stream);
				put_int(cfg->dir[i]->up_pct,stream);
				put_int(cfg->dir[i]->dn_pct,stream);
				c=0;
				put_int(c,stream);
				n=0;
				for(k=0;k<8;k++)
					put_int(n,stream);
				n=(short)0xffff;
				for(k=0;k<16;k++)
					put_int(n,stream);
				}

	/* Text File Sections */

	put_int(cfg->total_txtsecs,stream);
	for(i=0;i<cfg->total_txtsecs;i++) {
#if 1
		sprintf(str,"%stext/%s",cfg->data_dir,cfg->txtsec[i]->code);
		md(str);
#endif
		put_str(cfg->txtsec[i]->name,stream);
		put_str(cfg->txtsec[i]->code,stream);
		put_str(cfg->txtsec[i]->arstr,stream);
		n=0;
		for(j=0;j<8;j++)
			put_int(n,stream); }

	fclose(stream);

	return(TRUE);
}



/****************************************************************************/
/****************************************************************************/
BOOL DLLCALL write_chat_cfg(scfg_t* cfg, int backup_level)
{
	char	str[128];
	int 	i,j,file;
	short	n;
	FILE	*stream;

	if(cfg->prepped)
		return(FALSE);

	sprintf(str,"%schat.cnf",cfg->ctrl_dir);
	backup(str, backup_level, TRUE);

	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
		|| (stream=fdopen(file,"wb"))==NULL) {
		return(FALSE); 
	}
	setvbuf(stream,NULL,_IOFBF,2048);

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
BOOL DLLCALL write_xtrn_cfg(scfg_t* cfg, int backup_level)
{
	uchar	str[128],c;
	int 	i,j,sec,file;
	short	n;
	FILE	*stream;

	if(cfg->prepped)
		return(FALSE);

	sprintf(str,"%sxtrn.cnf",cfg->ctrl_dir);
	backup(str, backup_level, TRUE);

	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
		|| (stream=fdopen(file,"wb"))==NULL) {
		return(FALSE); 
	}
	setvbuf(stream,NULL,_IOFBF,2048);

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
		c=0;
		put_int(c,stream);
		n=0;
		for(j=0;j<7;j++)
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

	put_int(cfg->total_xtrns,stream);
	for(sec=0;sec<cfg->total_xtrnsecs;sec++)
		for(i=0;i<cfg->total_xtrns;i++) {
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
		n=0;
		for(j=0;j<5;j++)
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

void DLLCALL refresh_cfg(scfg_t* cfg)
{
	char	str[MAX_PATH+1];
    int		i;
	int		file;
    node_t	node;
    
    for(i=0;i<cfg->sys_nodes;i++) {
		file=-1;
		memset(&node,0,sizeof(node));
       	getnodedat(cfg,i+1,&node,&file);
		if(file==-1)
			continue;
        node.misc|=NODE_RRUN;
        if(putnodedat(cfg,i+1,&node,file))
            break;
    }

	sprintf(str,"%sftpsrvr.rec",cfg->ctrl_dir);
	if((file=open(str,O_WRONLY|O_CREAT|O_TRUNC,S_IWRITE|S_IREAD))!=-1)
		close(file);
	sprintf(str,"%smailsrvr.rec",cfg->ctrl_dir);
	if((file=open(str,O_WRONLY|O_CREAT|O_TRUNC,S_IWRITE|S_IREAD))!=-1)
		close(file);
	sprintf(str,"%sservices.rec",cfg->ctrl_dir);
	if((file=open(str,O_WRONLY|O_CREAT|O_TRUNC,S_IWRITE|S_IREAD))!=-1)
		close(file);
}
