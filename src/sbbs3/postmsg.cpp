/* postmsg.cpp */

/* Synchronet user create/post public message routine */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2007 Rob Swindell - http://www.synchro.net/copyright.html		*
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

static char* program_id(char* pid)
{
	char compiler[64];

	DESCRIBE_COMPILER(compiler);
	sprintf(pid,"%.10s %s%c-%s%s%s %s %s"
		,VERSION_NOTICE,VERSION,REVISION,PLATFORM_DESC
		,beta_version
#ifdef _DEBUG
		," Debug"
#else
		,""
#endif
		,__DATE__,compiler);
	return(pid);
}

/****************************************************************************/
/* Posts a message on subboard number sub, with 'top' as top of message.    */
/* Returns 1 if posted, 0 if not.                                           */
/****************************************************************************/
bool sbbs_t::postmsg(uint subnum, smbmsg_t *remsg, long wm_mode)
{
	char	str[256],title[LEN_TITLE+1],buf[SDT_BLOCK_LEN],top[256];
	char	msg_id[256];
	char	touser[64];
	char	from[64];
	char	pid[128];
	uint16_t xlat;
	ushort	msgattr;
	int 	i,j,x,file,storage;
	ulong	length,offset,crc=0xffffffff;
	FILE*	instream;
	smbmsg_t msg;
	uint	reason;

	if(remsg) {
		sprintf(title,"%.*s",LEN_TITLE,remsg->subj);
		if(remsg->hdr.attr&MSG_ANONYMOUS)
			SAFECOPY(from,text[Anonymous]);
		else
			SAFECOPY(from,remsg->from);
		// If user posted this message, reply to the original recipient again
		if((remsg->from_ext!=NULL && atoi(remsg->from_ext)==useron.number)
			|| stricmp(useron.alias,remsg->from)==0 || stricmp(useron.name,remsg->from)==0)
			SAFECOPY(touser,remsg->to);
		else
			SAFECOPY(touser,from);
		msgattr=(ushort)(remsg->hdr.attr&MSG_PRIVATE);
		sprintf(top,text[RegardingByToOn],title,from,remsg->to
			,timestr(remsg->hdr.when_written.time)
			,smb_zonestr(remsg->hdr.when_written.zone,NULL)); 
	} else {
		title[0]=0;
		touser[0]=0;
		top[0]=0;
		msgattr=0; 
	}

	/* Security checks */
	if(!can_user_post(&cfg,subnum,&useron,&reason)) {
		bputs(text[reason]);
		return false;
	}

	bprintf(text[Posting],cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->lname);
	action=NODE_PMSG;
	nodesync();

	if(!(msgattr&MSG_PRIVATE) && (cfg.sub[subnum]->misc&SUB_PONLY
		|| (cfg.sub[subnum]->misc&SUB_PRIV && !noyes(text[PrivatePostQ]))))
		msgattr|=MSG_PRIVATE;

	if(sys_status&SS_ABORT)
		return(false);

	if(
#if 0	/* we *do* support internet posts to specific people July-11-2002 */
		!(cfg.sub[subnum]->misc&SUB_INET) &&	// Prompt for TO: user
#endif
		(cfg.sub[subnum]->misc&SUB_TOUSER || msgattr&MSG_PRIVATE || touser[0])) {
		if(!touser[0] && !(msgattr&MSG_PRIVATE))
			SAFECOPY(touser,"All");
		bputs(text[PostTo]);
		i=LEN_ALIAS;
		if(cfg.sub[subnum]->misc&SUB_QNET)
			i=25;
		if(cfg.sub[subnum]->misc&SUB_FIDO)
			i=FIDO_NAME_LEN-1;
		if(cfg.sub[subnum]->misc&(SUB_PNET|SUB_INET))
			i=60;
		getstr(touser,i,K_UPRLWR|K_LINE|K_EDIT|K_AUTODEL);
		if(stricmp(touser,"ALL")
		&& !(cfg.sub[subnum]->misc&(SUB_PNET|SUB_FIDO|SUB_QNET|SUB_INET|SUB_ANON))) {
			if(cfg.sub[subnum]->misc&SUB_NAME) {
				if(!userdatdupe(useron.number,U_NAME,LEN_NAME,touser,0)) {
					bputs(text[UnknownUser]);
					return(false); 
				} 
			}
			else {
				if((i=finduser(touser))==0)
					return(false);
				username(&cfg,i,touser); 
			} 
		}
		if(sys_status&SS_ABORT)
			return(false); 
	}

	if(!touser[0])
		SAFECOPY(touser,"All");       // Default to ALL

	if(!stricmp(touser,"SYSOP") && !SYSOP)  // Change SYSOP to user #1
		username(&cfg,1,touser);

	if(msgattr&MSG_PRIVATE && !stricmp(touser,"ALL")) {
		bputs(text[NoToUser]);
		return(false); 
	}
	if(msgattr&MSG_PRIVATE)
		wm_mode|=WM_PRIVATE;

	if(cfg.sub[subnum]->misc&SUB_AONLY
		|| (cfg.sub[subnum]->misc&SUB_ANON && useron.exempt&FLAG('A')
			&& !noyes(text[AnonymousQ])))
		msgattr|=MSG_ANONYMOUS;

	if(cfg.sub[subnum]->mod_ar[0] && chk_ar(cfg.sub[subnum]->mod_ar,&useron))
		msgattr|=MSG_MODERATED;

	if(cfg.sub[subnum]->misc&SUB_SYSPERM && sub_op(subnum))
		msgattr|=MSG_PERMANENT;

	if(msgattr&MSG_PRIVATE)
		bputs(text[PostingPrivately]);

	if(msgattr&MSG_ANONYMOUS)
		bputs(text[PostingAnonymously]);

	if(cfg.sub[subnum]->misc&SUB_NAME)
		bputs(text[UsingRealName]);

	msg_tmp_fname(useron.xedit, str, sizeof(str));
	if(!writemsg(str,top,title,wm_mode,subnum,touser)
		|| (long)(length=flength(str))<1) {	/* Bugfix Aug-20-2003: Reject negative length */
		bputs(text[Aborted]);
		return(false); 
	}

	bputs(text[WritingIndx]);

	if((i=smb_stack(&smb,SMB_STACK_PUSH))!=SMB_SUCCESS) {
		errormsg(WHERE,ERR_OPEN,cfg.sub[subnum]->code,i,smb.last_error);
		return(false); 
	}

	sprintf(smb.file,"%s%s",cfg.sub[subnum]->data_dir,cfg.sub[subnum]->code);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=subnum;
	if((i=smb_open(&smb))!=SMB_SUCCESS) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		smb_stack(&smb,SMB_STACK_POP);
		return(false); 
	}

	if(filelength(fileno(smb.shd_fp))<1) {	 /* Create it if it doesn't exist */
		smb.status.max_crcs=cfg.sub[subnum]->maxcrcs;
		smb.status.max_msgs=cfg.sub[subnum]->maxmsgs;
		smb.status.max_age=cfg.sub[subnum]->maxage;
		smb.status.attr=cfg.sub[subnum]->misc&SUB_HYPER ? SMB_HYPERALLOC : 0;
		if((i=smb_create(&smb))!=SMB_SUCCESS) {
			smb_close(&smb);
			errormsg(WHERE,ERR_CREATE,smb.file,i,smb.last_error);
			smb_stack(&smb,SMB_STACK_POP);
			return(false); 
		} 
	}

	if((i=smb_locksmbhdr(&smb))!=SMB_SUCCESS) {
		smb_close(&smb);
		errormsg(WHERE,ERR_LOCK,smb.file,i,smb.last_error);
		smb_stack(&smb,SMB_STACK_POP);
		return(false); 
	}

	if((i=smb_getstatus(&smb))!=SMB_SUCCESS) {
		smb_close(&smb);
		errormsg(WHERE,ERR_READ,smb.file,i,smb.last_error);
		smb_stack(&smb,SMB_STACK_POP);
		return(false); 
	}

	length+=sizeof(xlat);	 /* +2 for translation string */

	if(length&0xfff00000UL) {
		smb_close(&smb);
		errormsg(WHERE,ERR_LEN,str,length,smb.last_error);
		smb_stack(&smb,SMB_STACK_POP);
		return(false); 
	}

	if(smb.status.attr&SMB_HYPERALLOC) {
		offset=smb_hallocdat(&smb);
		storage=SMB_HYPERALLOC; 
	}
	else {
		if((i=smb_open_da(&smb))!=SMB_SUCCESS) {
			smb_close(&smb);
			errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
			smb_stack(&smb,SMB_STACK_POP);
			return(false); 
		}
		if(cfg.sub[subnum]->misc&SUB_FAST) {
			offset=smb_fallocdat(&smb,length,1);
			storage=SMB_FASTALLOC; 
		}
		else {
			offset=smb_allocdat(&smb,length,1);
			storage=SMB_SELFPACK; 
		}
		smb_close_da(&smb); 
	}

	if((file=open(str,O_RDONLY|O_BINARY))==-1
		|| (instream=fdopen(file,"rb"))==NULL) {
		smb_freemsgdat(&smb,offset,length,1);
		smb_close(&smb);
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY|O_BINARY);
		smb_stack(&smb,SMB_STACK_POP);
		return(false); 
	}

	setvbuf(instream,NULL,_IOFBF,2*1024);
	fseek(smb.sdt_fp,offset,SEEK_SET);
	xlat=XLAT_NONE;
	fwrite(&xlat,2,1,smb.sdt_fp);
	x=SDT_BLOCK_LEN-2;				/* Don't read/write more than 255 */
	while(!feof(instream)) {
		memset(buf,0,x);
		j=fread(buf,1,x,instream);
		if(j<1)
			break;
		if(j>1 && (j!=x || feof(instream)) && buf[j-1]==LF && buf[j-2]==CR)
			buf[j-1]=buf[j-2]=0;	/* Convert to NULL */
		if(cfg.sub[subnum]->maxcrcs) {
			for(i=0;i<j;i++)
				crc=ucrc32(buf[i],crc); 
		}
		fwrite(buf,j,1,smb.sdt_fp);
		x=SDT_BLOCK_LEN; 
	}
	fflush(smb.sdt_fp);
	fclose(instream);
	crc=~crc;

	memset(&msg,0,sizeof(smbmsg_t));
	msg.hdr.version=smb_ver();
	msg.hdr.attr=msgattr;
	msg.hdr.when_written.time=msg.hdr.when_imported.time=time(NULL);
	msg.hdr.when_written.zone=msg.hdr.when_imported.zone=sys_timezone(&cfg);

	msg.hdr.number=smb.status.last_msg+1; /* this *should* be the new message number */

	smb_hfield_str(&msg,FIDOPID,program_id(pid));

	/* Generate default (RFC822) message-id (always) */
	SAFECOPY(msg_id,get_msgid(&cfg,subnum,&msg));
	smb_hfield_str(&msg,RFC822MSGID,msg_id);

	/* Generate FTN (FTS-9) MSGID */
	if(cfg.sub[subnum]->misc&SUB_FIDO) {
		SAFECOPY(msg_id,ftn_msgid(cfg.sub[subnum],&msg));
		smb_hfield_str(&msg,FIDOMSGID,msg_id);
	}
	if(remsg) {

		msg.hdr.thread_back=remsg->hdr.number;	/* needed for threading backward */

		/* Add RFC-822 Reply-ID (generate if necessary) */
		if(remsg->id!=NULL)
			smb_hfield_str(&msg,RFC822REPLYID,remsg->id);

		/* Add FidoNet Reply if original message has FidoNet MSGID */
		if(remsg->ftn_msgid!=NULL)
			smb_hfield_str(&msg,FIDOREPLYID,remsg->ftn_msgid);

		if((i=smb_updatethread(&smb, remsg, smb.status.last_msg+1))!=SMB_SUCCESS)
			errormsg(WHERE,"updating thread",smb.file,i,smb.last_error); 
	}


	if(cfg.sub[subnum]->maxcrcs) {
		i=smb_addcrc(&smb,crc);
		if(i) {
			smb_freemsgdat(&smb,offset,length,1);
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			attr(cfg.color[clr_err]);
			bputs("Duplicate message!\r\n");
			return(false); 
		} 
	}

	msg.hdr.offset=offset;

	smb_hfield_str(&msg,RECIPIENT,touser);
	strlwr(touser);

	SAFECOPY(str,cfg.sub[subnum]->misc&SUB_NAME ? useron.name : useron.alias);
	smb_hfield_str(&msg,SENDER,str);
	strlwr(str);

	sprintf(str,"%u",useron.number);
	smb_hfield_str(&msg,SENDEREXT,str);

	/* Security logging */
	msg_client_hfields(&msg,&client);

	smb_hfield_str(&msg,SUBJECT,title);

	smb_dfield(&msg,TEXT_BODY,length);

	i=smb_addmsghdr(&smb,&msg,storage);	// calls smb_unlocksmbhdr() 
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);

	smb_freemsgmem(&msg);
	if(i!=SMB_SUCCESS) {
		errormsg(WHERE,ERR_WRITE,smb.file,i,smb.last_error);
		smb_freemsgdat(&smb,offset,length,1);
		return(false); 
	}

	logon_posts++;
	user_posted_msg(&cfg, &useron, 1);
	bprintf(text[Posted],cfg.grp[cfg.sub[subnum]->grp]->sname
		,cfg.sub[subnum]->lname);
	sprintf(str,"%s posted on %s %s"
		,useron.alias,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->lname);
	logline("P+",str);

	signal_sub_sem(&cfg,subnum);

	user_event(EVENT_POST);

	return(true);
}

extern "C" void DLLCALL signal_sub_sem(scfg_t* cfg, uint subnum)
{
	char str[MAX_PATH+1];

	if(subnum==INVALID_SUB || subnum>=cfg->total_subs)	/* e-mail? */
		return;

	/* signal semaphore files */
	if(cfg->sub[subnum]->misc&SUB_FIDO && cfg->echomail_sem[0])		
		ftouch(cmdstr(cfg,NULL,cfg->echomail_sem,nulstr,nulstr,str));
	if(cfg->sub[subnum]->post_sem[0]) 
		ftouch(cmdstr(cfg,NULL,cfg->sub[subnum]->post_sem,nulstr,nulstr,str));
}

extern "C" int DLLCALL msg_client_hfields(smbmsg_t* msg, client_t* client)
{
	int i;

	if(client==NULL)
		return(-1);

	if((i=smb_hfield_str(msg,SENDERIPADDR,client->addr))!=SMB_SUCCESS)
		return(i);
	if((i=smb_hfield_str(msg,SENDERHOSTNAME,client->host))!=SMB_SUCCESS)
		return(i);
	if((i=smb_hfield_str(msg,SENDERPROTOCOL,client->protocol))!=SMB_SUCCESS)
		return(i);
	return smb_hfield(msg,SENDERPORT,sizeof(client->port),&client->port);
}

extern "C" int DLLCALL savemsg(scfg_t* cfg, smb_t* smb, smbmsg_t* msg, client_t* client, char* msgbuf)
{
	char	pid[128];
	char	msg_id[256];
	ushort	xlat=XLAT_NONE;
	int 	i;
	int		storage=SMB_SELFPACK;
	long	dupechk_hashes=SMB_HASH_SOURCE_ALL;

	if(msg==NULL)
		return(SMB_FAILURE);

	if(!SMB_IS_OPEN(smb)) {
		if(smb->subnum==INVALID_SUB)
			sprintf(smb->file,"%smail",cfg->data_dir);
		else
			sprintf(smb->file,"%s%s",cfg->sub[smb->subnum]->data_dir,cfg->sub[smb->subnum]->code);
		smb->retry_time=cfg->smb_retry_time;
		if((i=smb_open(smb))!=SMB_SUCCESS)
			return(i);
	}

	/* Lock the msgbase early to preserve our message number (used in MSG-IDs) */
	if(!smb->locked && smb_locksmbhdr(smb)!=SMB_SUCCESS)
		return(SMB_ERR_LOCK);

	if(filelength(fileno(smb->shd_fp))>0 && (i=smb_getstatus(smb))!=SMB_SUCCESS) {
		if(smb->locked)
			smb_unlocksmbhdr(smb);
		return(i);
	}

	if(smb->subnum==INVALID_SUB) {	/* e-mail */

		smb->status.max_crcs=cfg->mail_maxcrcs;
		smb->status.max_age=cfg->mail_maxage;
		smb->status.max_msgs=0;	/* unlimited */
		smb->status.attr=SMB_EMAIL;

		if(cfg->sys_misc&SM_FASTMAIL)
			storage=SMB_FASTALLOC;

		/* duplicate message-IDs must be allowed in mail database */
		dupechk_hashes&=~(1<<SMB_HASH_SOURCE_MSG_ID);

	} else {	/* sub-board */

		smb->status.max_crcs=cfg->sub[smb->subnum]->maxcrcs;
		smb->status.max_msgs=cfg->sub[smb->subnum]->maxmsgs;
		smb->status.max_age=cfg->sub[smb->subnum]->maxage;
		smb->status.attr=0;

		if(cfg->sub[smb->subnum]->misc&SUB_HYPER)
			storage = smb->status.attr = SMB_HYPERALLOC;
		else if(cfg->sub[smb->subnum]->misc&SUB_FAST)
			storage = SMB_FASTALLOC;

		if(cfg->sub[smb->subnum]->misc&SUB_LZH)
			xlat=XLAT_LZH;

		/* enforce anonymous/private posts only */
 		if(cfg->sub[smb->subnum]->misc&SUB_PONLY)
 			msg->hdr.attr|=MSG_PRIVATE;
 		if(cfg->sub[smb->subnum]->misc&SUB_AONLY)
 			msg->hdr.attr|=MSG_ANONYMOUS;
	}

	if(msg->hdr.when_imported.time==0) {
		msg->hdr.when_imported.time=time(NULL);
		msg->hdr.when_imported.zone=sys_timezone(cfg);
	}
	if(msg->hdr.when_written.time==0)	/* Uninitialized */
		msg->hdr.when_written = msg->hdr.when_imported;

	msg->hdr.number=smb->status.last_msg+1;		/* needed for MSG-ID generation */

	if(smb->status.max_crcs==0)	/* no CRC checking means no body text dupe checking */
		dupechk_hashes&=~(1<<SMB_HASH_SOURCE_BODY);

	if(client!=NULL)
		msg_client_hfields(msg,client);
 
	/* Generate FidoNet Program Identifier */
 	if(msg->ftn_pid==NULL) 	
 		smb_hfield_str(msg,FIDOPID,program_id(pid));
 
 	/* Generate RFC-822 Message-id  */
 	if(msg->id==NULL) {
 		SAFECOPY(msg_id,get_msgid(cfg,smb->subnum,msg));
 		smb_hfield_str(msg,RFC822MSGID,msg_id);
 	}
 
 	/* Generate FidoNet MSGID (for FidoNet sub-boards) */
 	if(smb->subnum!=INVALID_SUB && cfg->sub[smb->subnum]->misc&SUB_FIDO 
		&& msg->ftn_msgid==NULL) {
 		SAFECOPY(msg_id,ftn_msgid(cfg->sub[smb->subnum],msg));
 		smb_hfield_str(msg,FIDOMSGID,msg_id);
 	}

	if((i=smb_addmsg(smb,msg,storage,dupechk_hashes,xlat,(uchar*)msgbuf,NULL))==SMB_SUCCESS
		&& msg->to!=NULL	/* no recipient means no header created at this stage */)
		signal_sub_sem(cfg,smb->subnum);

	return(i);
}

