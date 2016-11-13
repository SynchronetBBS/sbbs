/* postmsg.cpp */

/* Synchronet user create/post public message routine */

/* $Id$ */

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

/****************************************************************************/
/* FTN-compliant "Program Identifier"/PID									*/
/****************************************************************************/
extern "C" char* DLLCALL msg_program_id(char* pid)
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

int msgbase_open(scfg_t* cfg, smb_t* smb, int* storage, long* dupechk_hashes, uint16_t* xlat)
{
	int i;

	*storage=SMB_SELFPACK;
	*dupechk_hashes=SMB_HASH_SOURCE_DUPE;
	*xlat=XLAT_NONE;

	smb->retry_time=cfg->smb_retry_time;
	if(smb->subnum==INVALID_SUB) {
		safe_snprintf(smb->file,sizeof(smb->file),"%smail",cfg->data_dir);
		smb->status.max_crcs=cfg->mail_maxcrcs;
		smb->status.max_age=cfg->mail_maxage;
		smb->status.max_msgs=0;	/* unlimited */
		smb->status.attr=SMB_EMAIL;
		if(cfg->sys_misc&SM_FASTMAIL)
			*storage = SMB_FASTALLOC;
		/* duplicate message-IDs must be allowed in mail database */
		*dupechk_hashes&=~(1<<SMB_HASH_SOURCE_MSG_ID);
	} else {
		safe_snprintf(smb->file,sizeof(smb->file),"%s%s",cfg->sub[smb->subnum]->data_dir,cfg->sub[smb->subnum]->code);
		smb->status.max_crcs=cfg->sub[smb->subnum]->maxcrcs;
		smb->status.max_msgs=cfg->sub[smb->subnum]->maxmsgs;
		smb->status.max_age=cfg->sub[smb->subnum]->maxage;
		smb->status.attr=0;
		if(cfg->sub[smb->subnum]->misc&SUB_HYPER)
			*storage = smb->status.attr = SMB_HYPERALLOC;
		else if(cfg->sub[smb->subnum]->misc&SUB_FAST)
			*storage = SMB_FASTALLOC;

		if(cfg->sub[smb->subnum]->misc&SUB_LZH)
			*xlat=XLAT_LZH;
	}
	if(smb->status.max_crcs==0)	/* no CRC checking means no body text dupe checking */
		*dupechk_hashes&=~(1<<SMB_HASH_SOURCE_BODY);

	if((i=smb_open(smb)) != SMB_SUCCESS)
		return i;

	if(filelength(fileno(smb->shd_fp)) < 1) /* MsgBase doesn't exist yet, create it */
		i=smb_create(smb);

	return i;
}


/****************************************************************************/
/* Posts a message on subboard number sub, with 'top' as top of message.    */
/* Returns 1 if posted, 0 if not.                                           */
/****************************************************************************/
bool sbbs_t::postmsg(uint subnum, smbmsg_t *remsg, long wm_mode)
{
	char	str[256],title[LEN_TITLE+1],top[256];
	char	msg_id[256];
	char	touser[64];
	char	from[64];
	char	pid[128];
	char*	editor=NULL;
	char*	msgbuf=NULL;
	uint16_t xlat;
	ushort	msgattr;
	int 	i,storage;
	long	dupechk_hashes;
	long	length;
	FILE*	fp;
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
	if(!can_user_post(&cfg,subnum,&useron,&client,&reason)) {
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
		getstr(touser,i,K_LINE|K_EDIT|K_AUTODEL);
		if(stricmp(touser,"ALL")
		&& !(cfg.sub[subnum]->misc&(SUB_PNET|SUB_FIDO|SUB_QNET|SUB_INET|SUB_ANON))) {
			if(cfg.sub[subnum]->misc&SUB_NAME) {
				if(!userdatdupe(useron.number,U_NAME,LEN_NAME,touser)) {
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
			&& !noyes(text[AnonymousQ]))) {
		msgattr|=MSG_ANONYMOUS;
		wm_mode|=WM_ANON;
	}

	if(cfg.sub[subnum]->mod_ar[0] && chk_ar(cfg.sub[subnum]->mod_ar,&useron,&client))
		msgattr|=MSG_MODERATED;

	if(cfg.sub[subnum]->misc&SUB_SYSPERM && sub_op(subnum))
		msgattr|=MSG_PERMANENT;

	if(msgattr&MSG_PRIVATE)
		bputs(text[PostingPrivately]);

	if(msgattr&MSG_ANONYMOUS)
		bputs(text[PostingAnonymously]);
	else if(cfg.sub[subnum]->misc&SUB_NAME)
		bputs(text[UsingRealName]);

	msg_tmp_fname(useron.xedit, str, sizeof(str));
	if(!writemsg(str,top,title,wm_mode,subnum,touser
		,/* from: */cfg.sub[subnum]->misc&SUB_NAME ? useron.name : useron.alias
		,&editor)
		|| (length=(long)flength(str))<1) {	/* Bugfix Aug-20-2003: Reject negative length */
		bputs(text[Aborted]);
		return(false); 
	}

	bputs(text[WritingIndx]);

	if((i=smb_stack(&smb,SMB_STACK_PUSH))!=SMB_SUCCESS) {
		errormsg(WHERE,ERR_OPEN,cfg.sub[subnum]->code,i,smb.last_error);
		return(false); 
	}

	smb.subnum=subnum;
	if((i=msgbase_open(&cfg,&smb,&storage,&dupechk_hashes,&xlat))!=SMB_SUCCESS) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		smb_stack(&smb,SMB_STACK_POP);
		return(false); 
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

	if((msgbuf=(char*)calloc(length+1,sizeof(char))) == NULL) {
		smb_close(&smb);
		errormsg(WHERE,ERR_ALLOC,"msgbuf",length+1);
		smb_stack(&smb,SMB_STACK_POP);
		return(false);
	}

	if((fp=fopen(str,"rb"))==NULL) {
		free(msgbuf);
		smb_close(&smb);
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY|O_BINARY);
		smb_stack(&smb,SMB_STACK_POP);
		return(false); 
	}

	i=fread(msgbuf,1,length,fp);
	fclose(fp);
	if(i != length) {
		free(msgbuf);
		smb_close(&smb);
		errormsg(WHERE,ERR_READ,str,length);
		smb_stack(&smb,SMB_STACK_POP);
		return(false);
	}
	truncsp(msgbuf);

	/* ToDo: split body/tail */

	memset(&msg,0,sizeof(msg));
	msg.hdr.attr=msgattr;
	msg.hdr.when_written.time=msg.hdr.when_imported.time=time32(NULL);
	msg.hdr.when_written.zone=msg.hdr.when_imported.zone=sys_timezone(&cfg);

	msg.hdr.number=smb.status.last_msg+1; /* this *should* be the new message number */

	if(remsg) {

		msg.hdr.thread_back=remsg->hdr.number;	/* needed for threading backward */

		if((msg.hdr.thread_id=remsg->hdr.thread_id) == 0)
			msg.hdr.thread_id=remsg->hdr.number;

		/* Add RFC-822 Reply-ID (generate if necessary) */
		if(remsg->id!=NULL)
			smb_hfield_str(&msg,RFC822REPLYID,remsg->id);

		/* Add FidoNet Reply if original message has FidoNet MSGID */
		if(remsg->ftn_msgid!=NULL)
			smb_hfield_str(&msg,FIDOREPLYID,remsg->ftn_msgid);

		if((i=smb_updatethread(&smb, remsg, smb.status.last_msg+1))!=SMB_SUCCESS)
			errormsg(WHERE,"updating thread",smb.file,i,smb.last_error); 
	}

	smb_hfield_str(&msg,RECIPIENT,touser);

	SAFECOPY(str,cfg.sub[subnum]->misc&SUB_NAME ? useron.name : useron.alias);
	smb_hfield_str(&msg,SENDER,str);

	sprintf(str,"%u",useron.number);
	smb_hfield_str(&msg,SENDEREXT,str);

	/* Security logging */
	msg_client_hfields(&msg,&client);
	smb_hfield_str(&msg,SENDERSERVER,startup->host_name);

	smb_hfield_str(&msg,SUBJECT,title);

	/* Generate default (RFC822) message-id (always) */
	get_msgid(&cfg,subnum,&msg,msg_id,sizeof(msg_id));
	smb_hfield_str(&msg,RFC822MSGID,msg_id);

	/* Generate FTN (FTS-9) MSGID */
	if(cfg.sub[subnum]->misc&SUB_FIDO) {
		ftn_msgid(cfg.sub[subnum],&msg,msg_id,sizeof(msg_id));
		smb_hfield_str(&msg,FIDOMSGID,msg_id);
	}

	/* Generate FidoNet Program Identifier */
	smb_hfield_str(&msg,FIDOPID,msg_program_id(pid));

	if(editor!=NULL)
		smb_hfield_str(&msg,SMB_EDITOR,editor);

	i=smb_addmsg(&smb,&msg,storage,dupechk_hashes,xlat,(uchar*)msgbuf,NULL);
	free(msgbuf);

	if(i==SMB_DUPE_MSG) {
		attr(cfg.color[clr_err]);
		bprintf(text[CantPostMsg], smb.last_error);
	} else if(i!=SMB_SUCCESS)
		errormsg(WHERE,ERR_WRITE,smb.file,i,smb.last_error);

	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	smb_freemsgmem(&msg);
	if(i!=SMB_SUCCESS)
		return(false); 

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
	int		i;
	char	port[16];
	char	date[64];

	if(client==NULL)
		return(-1);

	if(client->user!=NULL && (i=smb_hfield_str(msg,SENDERUSERID,client->user))!=SMB_SUCCESS)
		return(i);
	if((i=smb_hfield_str(msg,SENDERTIME,xpDateTime_to_isoDateTimeStr(gmtime_to_xpDateTime(client->time)
		,/* separators: */"","","", /* precision: */0
		,date,sizeof(date))))!=SMB_SUCCESS)
		return(i);
	if((i=smb_hfield_str(msg,SENDERIPADDR,client->addr))!=SMB_SUCCESS)
		return(i);
	if((i=smb_hfield_str(msg,SENDERHOSTNAME,client->host))!=SMB_SUCCESS)
		return(i);
	if(client->protocol!=NULL && (i=smb_hfield_str(msg,SENDERPROTOCOL,client->protocol))!=SMB_SUCCESS)
		return(i);
	SAFEPRINTF(port,"%u",client->port);
	return smb_hfield_str(msg,SENDERPORT,port);
}

extern "C" int DLLCALL savemsg(scfg_t* cfg, smb_t* smb, smbmsg_t* msg, client_t* client, const char* server, char* msgbuf)
{
	char	pid[128];
	char	msg_id[256];
	ushort	xlat=XLAT_NONE;
	int 	i;
	int		storage=SMB_SELFPACK;
	long	dupechk_hashes=SMB_HASH_SOURCE_DUPE;

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

		/* exception here during recycle:

	sbbs.dll!savemsg(scfg_t * cfg, smb_t * smb, smbmsg_t * msg, client_t * client, char * msgbuf)  Line 473 + 0xf bytes	C++
 	sbbs.dll!js_save_msg(JSContext * cx, JSObject * obj, unsigned int argc, long * argv, long * rval)  Line 1519 + 0x25 bytes	C
 	js32.dll!js_Invoke(JSContext * cx, unsigned int argc, unsigned int flags)  Line 1375 + 0x17 bytes	C
 	js32.dll!js_Interpret(JSContext * cx, unsigned char * pc, long * result)  Line 3944 + 0xf bytes	C
 	js32.dll!js_Execute(JSContext * cx, JSObject * chain, JSObject * script, JSStackFrame * down, unsigned int flags, long * result)  Line 1633 + 0x13 bytes	C
 	js32.dll!JS_ExecuteScript(JSContext * cx, JSObject * obj, JSObject * script, long * rval)  Line 4188 + 0x19 bytes	C
 	sbbs.dll!sbbs_t::js_execfile(const char * cmd, const char * startup_dir)  Line 686 + 0x27 bytes	C++
 	sbbs.dll!sbbs_t::external(const char * cmdline, long mode, const char * startup_dir)  Line 413 + 0x1e bytes	C++
 	sbbs.dll!event_thread(void * arg)  Line 2745 + 0x71 bytes	C++

	apparently the event_thread is sharing an scfg_t* with another thread! */


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
		msg->hdr.when_imported.time=time32(NULL);
		msg->hdr.when_imported.zone=sys_timezone(cfg);
	}
	if(msg->hdr.when_written.time==0)	/* Uninitialized */
		msg->hdr.when_written = msg->hdr.when_imported;

	msg->hdr.number=smb->status.last_msg+1;		/* needed for MSG-ID generation */

	if(smb->status.max_crcs==0)	/* no CRC checking means no body text dupe checking */
		dupechk_hashes&=~(1<<SMB_HASH_SOURCE_BODY);

	if(client!=NULL)
		msg_client_hfields(msg,client);
	if(server!=NULL)
		smb_hfield_str(msg,SENDERSERVER,server);
 
 	/* Generate RFC-822 Message-id  */
 	if(msg->id==NULL) {
 		get_msgid(cfg,smb->subnum,msg,msg_id,sizeof(msg_id));
 		smb_hfield_str(msg,RFC822MSGID,msg_id);
 	}
 
 	/* Generate FidoNet MSGID (for FidoNet sub-boards) */
 	if(smb->subnum!=INVALID_SUB && cfg->sub[smb->subnum]->misc&SUB_FIDO 
		&& msg->ftn_msgid==NULL) {
 		ftn_msgid(cfg->sub[smb->subnum],msg,msg_id,sizeof(msg_id));
 		smb_hfield_str(msg,FIDOMSGID,msg_id);
 	}

	/* Generate FidoNet Program Identifier */
 	if(msg->ftn_pid==NULL) 	
 		smb_hfield_str(msg,FIDOPID,msg_program_id(pid));

	if((i=smb_addmsg(smb,msg,storage,dupechk_hashes,xlat,(uchar*)msgbuf,NULL))==SMB_SUCCESS
		&& msg->to!=NULL	/* no recipient means no header created at this stage */) {
		if(smb->subnum == INVALID_SUB) {
			if(msg->to_net.type == NET_FIDO)
				ftouch(cmdstr(cfg,NULL,cfg->netmail_sem,nulstr,nulstr,NULL));
		} else
			signal_sub_sem(cfg,smb->subnum);
	}
	return(i);
}

extern "C" int DLLCALL votemsg(scfg_t* cfg, smb_t* smb, smbmsg_t* msg, const char* smsgfmt)
{
	int result;
	smbmsg_t remsg;

	ZERO_VAR(remsg);

	/* Look-up thread_back if RFC822 Reply-ID was specified */
	if(msg->hdr.thread_back == 0 && msg->reply_id != NULL) {
		if(smb_getmsgidx_by_msgid(smb, &remsg, msg->reply_id) == SMB_SUCCESS)
			msg->hdr.thread_back = remsg.idx.number;	/* needed for threading backward */
	}
	if(smb_voted_already(smb, msg->hdr.thread_back, msg->from, (enum smb_net_type)msg->from_net.type, msg->from_net.addr))
		return SMB_DUPE_MSG;
	result = smb_addvote(smb, msg, smb_storage_mode(cfg, smb));
	if(result == SMB_SUCCESS && smsgfmt != NULL) {
		remsg.hdr.number = msg->hdr.thread_back;
		if(smb_getmsgidx(smb, &remsg) == SMB_SUCCESS
			&& smb_getmsghdr(smb, &remsg) == SMB_SUCCESS) {
			if(remsg.from_ext != NULL) {
				user_t user;
				ZERO_VAR(user);
				user.number = atoi(remsg.from_ext);
				if(getuserdat(cfg, &user) == 0 && 
					(stricmp(remsg.from, user.alias) == 0 || stricmp(remsg.from, user.name) == 0)) {
					char from[256];
					char tstr[128];
					char smsg[256];
					if(msg->from_net.type)
						safe_snprintf(from, sizeof(from), "%s (%s)", msg->from, smb_netaddr(&msg->from_net));
					else
						SAFECOPY(from, msg->from);
					safe_snprintf(smsg, sizeof(smsg), smsgfmt
						,timestr(cfg, msg->hdr.when_written.time, tstr)
						,cfg->grp[cfg->sub[smb->subnum]->grp]->sname
						,cfg->sub[smb->subnum]->sname
						,from
						,(msg->hdr.attr&MSG_UPVOTE) ? "Up":"Down"
						,remsg.subj);
					putsmsg(cfg, user.number, smsg);
				}
			}
			smb_freemsgmem(&remsg);
		}
	}

	return result;
}