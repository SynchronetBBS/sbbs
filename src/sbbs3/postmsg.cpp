/* Synchronet user create/post public message routine */

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

#include "sbbs.h"
#include "utf8.h"
#include "filedat.h"

int msgbase_open(scfg_t* cfg, smb_t* smb, unsigned int subnum, int* storage, long* dupechk_hashes, uint16_t* xlat)
{
	int i;

	*dupechk_hashes=SMB_HASH_SOURCE_DUPE;
	*xlat=XLAT_NONE;

	if((i=smb_open_sub(cfg, smb, subnum)) != SMB_SUCCESS)
		return i;

	if(smb->subnum==INVALID_SUB) {
		/* duplicate message-IDs must be allowed in mail database */
		*dupechk_hashes&=~(1<<SMB_HASH_SOURCE_MSG_ID);
	} else {
		if(cfg->sub[smb->subnum]->misc&SUB_LZH)
			*xlat=XLAT_LZH;
	}

	if(smb->status.max_crcs==0)	/* no CRC checking means no body text dupe checking */
		*dupechk_hashes&=~(1<<SMB_HASH_SOURCE_BODY);

	*storage=smb_storage_mode(cfg, smb);

	return i;
}

static uchar* findsig(char* msgbuf)
{
	char* tail = strstr(msgbuf, "\n-- \r\n");
	if(tail != NULL) {
		*tail = '\0';
		tail++;
		truncsp(msgbuf);
	}
	return (uchar*)tail;
}

/****************************************************************************/
/* Posts a message on sub-board number 'subnum'								*/
/* Returns true if posted, false if not.                                    */
/****************************************************************************/
bool sbbs_t::postmsg(uint subnum, long wm_mode, smb_t* resmb, smbmsg_t* remsg)
{
	char	str[256];
	char	title[LEN_TITLE+1] = "";
	char	top[256] = "";
	char	touser[64] = "";
	char	from[64];
	char	tags[64] = "";
	const char*	editor=NULL;
	const char*	charset=NULL;
	char*	msgbuf=NULL;
	uint16_t xlat;
	ushort	msgattr = 0;
	int 	i,storage;
	long	dupechk_hashes;
	long	length;
	FILE*	fp;
	smbmsg_t msg;
	uint	reason;
	str_list_t names = NULL;

	/* Security checks */
	if(!can_user_post(&cfg,subnum,&useron,&client,&reason)) {
		bputs(text[reason]);
		return false;
	}

	if(remsg) {
		SAFECOPY(title, msghdr_field(remsg, remsg->subj, NULL, term_supports(UTF8)));
		if(remsg->hdr.attr&MSG_ANONYMOUS)
			SAFECOPY(from,text[Anonymous]);
		else
			SAFECOPY(from, msghdr_field(remsg, remsg->from, NULL, term_supports(UTF8)));
		// If user posted this message, reply to the original recipient again
		if(remsg->to != NULL
			&& ((remsg->from_ext != NULL && atoi(remsg->from_ext)==useron.number)
				|| stricmp(useron.alias,remsg->from) == 0 || stricmp(useron.name,remsg->from) == 0))
			SAFECOPY(touser, msghdr_field(remsg, remsg->to, NULL, term_supports(UTF8)));
		else
			SAFECOPY(touser,from);
		if(remsg->to != NULL)
			strListPush(&names, remsg->to);
		msgattr=(ushort)(remsg->hdr.attr&MSG_PRIVATE);
		sprintf(top,text[RegardingByToOn]
			,title
			,from
			,msghdr_field(remsg, remsg->to, NULL, term_supports(UTF8))
			,timestr(remsg->hdr.when_written.time)
			,smb_zonestr(remsg->hdr.when_written.zone,NULL));
		if(remsg->tags != NULL)
			SAFECOPY(tags, remsg->tags);
	}

	bprintf(text[Posting],cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->lname);
	action=NODE_PMSG;
	nodesync();

	if(!(msgattr&MSG_PRIVATE) && (cfg.sub[subnum]->misc&SUB_PONLY
		|| (cfg.sub[subnum]->misc&SUB_PRIV && !noyes(text[PrivatePostQ]))))
		msgattr|=MSG_PRIVATE;

	if(sys_status&SS_ABORT) {
		strListFree(&names);
		return(false);
	}

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
		getstr(touser,i,K_LINE|K_EDIT|K_AUTODEL|K_TRIM,names);
		if(stricmp(touser,"ALL")
			&& !(cfg.sub[subnum]->misc&(SUB_PNET|SUB_FIDO|SUB_QNET|SUB_INET|SUB_ANON))) {
			if(cfg.sub[subnum]->misc&SUB_NAME) {
				if(!userdatdupe(useron.number,U_NAME,LEN_NAME,touser)) {
					bputs(text[UnknownUser]);
					strListFree(&names);
					return(false); 
				} 
			}
			else {
				if((i=finduser(touser))==0) {
					strListFree(&names);
					return(false);
				}
				username(&cfg,i,touser); 
			} 
		}
		if(sys_status&SS_ABORT) {
			strListFree(&names);
			return(false);
		}
	}
	strListFree(&names);

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

	if((i=smb_stack(&smb,SMB_STACK_PUSH))!=SMB_SUCCESS) {
		errormsg(WHERE,ERR_OPEN,cfg.sub[subnum]->code,i,smb.last_error);
		return(false); 
	}

	if((i=msgbase_open(&cfg,&smb,subnum,&storage,&dupechk_hashes,&xlat))!=SMB_SUCCESS) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		smb_stack(&smb,SMB_STACK_POP);
		return(false); 
	}

	if(remsg != NULL && resmb != NULL && !(wm_mode&WM_QUOTE)) {
		if(quotemsg(resmb, remsg))
			wm_mode |= WM_QUOTE;
	}

	if(!writemsg(str,top,title,wm_mode,subnum,touser
		,/* from: */cfg.sub[subnum]->misc&SUB_NAME ? useron.name : useron.alias
		,&editor, &charset)
		|| (length=(long)flength(str))<1) {	/* Bugfix Aug-20-2003: Reject negative length */
		bputs(text[Aborted]);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		return(false); 
	}
	if((cfg.sub[subnum]->misc&SUB_MSGTAGS)
		&& (tags[0] || text[TagMessageQ][0] == 0 || !noyes(text[TagMessageQ]))) {
		bputs(text[TagMessagePrompt]);
		getstr(tags, sizeof(tags)-1, K_EDIT|K_LINE|K_TRIM);
	}

	bputs(text[WritingIndx]);

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

	smb_hfield_str(&msg,RECIPIENT,touser);

	SAFECOPY(str,cfg.sub[subnum]->misc&SUB_NAME ? useron.name : useron.alias);
	smb_hfield_str(&msg,SENDER,str);

	sprintf(str,"%u",useron.number);
	smb_hfield_str(&msg,SENDEREXT,str);

	/* Security logging */
	msg_client_hfields(&msg,&client);
	smb_hfield_str(&msg,SENDERSERVER, server_host_name());

	smb_hfield_str(&msg,SUBJECT,title);

	add_msg_ids(&cfg, &smb, &msg, remsg);

	editor_info_to_msg(&msg, editor, charset);
	
	if(tags[0])
		smb_hfield_str(&msg, SMB_TAGS, tags);

	i=smb_addmsg(&smb,&msg,storage,dupechk_hashes,xlat,(uchar*)msgbuf, findsig(msgbuf));
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
	sprintf(str,"posted on %s %s"
		,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->lname);
	logline("P+",str);

	if(!(msgattr & MSG_ANONYMOUS)
		&& stricmp(touser, "All") != 0
		&& (remsg == NULL || remsg->from_net.type == NET_NONE)) {
		if(cfg.sub[subnum]->misc&SUB_NAME)
			i = userdatdupe(0, U_NAME, LEN_NAME, touser);
		else
			i = matchuser(&cfg, touser, TRUE /* sysop_alias */);
		if(i > 0 && i != useron.number) {
			SAFEPRINTF4(str, text[MsgPostedToYouVia]
				,cfg.sub[subnum]->misc&SUB_NAME ? useron.name : useron.alias
				,connection
				,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->lname);
			putsmsg(&cfg, i, str);
		}
	}

	signal_sub_sem(&cfg,subnum);

	user_event(EVENT_POST);

	return(true);
}

extern "C" void signal_sub_sem(scfg_t* cfg, uint subnum)
{
	char str[MAX_PATH+1];

	if(subnum==INVALID_SUB || subnum>=cfg->total_subs)	/* e-mail? */
		return;

	/* signal semaphore files */
	if(cfg->sub[subnum]->misc&SUB_FIDO && cfg->echomail_sem[0])		
		ftouch(cmdstr(cfg,NULL,cfg->echomail_sem,nulstr,nulstr,str,sizeof(str)));
	if(cfg->sub[subnum]->post_sem[0]) 
		ftouch(cmdstr(cfg,NULL,cfg->sub[subnum]->post_sem,nulstr,nulstr,str,sizeof(str)));
}

extern "C" int msg_client_hfields(smbmsg_t* msg, client_t* client)
{
	int		i;
	char	port[16];
	char	date[64];

	if(client==NULL)
		return(-1);

	if(client->user!=NULL && client->usernum && (i=smb_hfield_str(msg,SENDERUSERID,client->user))!=SMB_SUCCESS)
		return(i);
	if(client->time
		&& (i=smb_hfield_str(msg,SENDERTIME,xpDateTime_to_isoDateTimeStr(gmtime_to_xpDateTime(client->time)
		,/* separators: */"","","", /* precision: */0
		,date,sizeof(date))))!=SMB_SUCCESS)
		return(i);
	if(*client->addr
		&& (i=smb_hfield_str(msg,SENDERIPADDR,client->addr))!=SMB_SUCCESS)
		return(i);
	if(*client->host
		&& (i=smb_hfield_str(msg,SENDERHOSTNAME,client->host))!=SMB_SUCCESS)
		return(i);
	if(client->protocol!=NULL && (i=smb_hfield_str(msg,SENDERPROTOCOL,client->protocol))!=SMB_SUCCESS)
		return(i);
	if(client->port) {
		SAFEPRINTF(port,"%u",client->port);
		return smb_hfield_str(msg,SENDERPORT,port);
	}
	return SMB_SUCCESS;
}

/* Note: finds signature delimiter automatically and (if applicable) separates msgbuf into body and tail */
/* Adds/generates Message-IDs when needed */
/* Auto-sets the UTF-8 indicators for UTF-8 encoded header fields and body text */
/* If you want to save a message body with CP437 chars that also happen to be valid UTF-8 sequences, you'll need to preset the ftn_charset header */
extern "C" int savemsg(scfg_t* cfg, smb_t* smb, smbmsg_t* msg, client_t* client, const char* server, char* msgbuf, smbmsg_t* remsg)
{
	ushort	xlat=XLAT_NONE;
	int 	i;
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

		smb->status.max_crcs=cfg->mail_maxcrcs;
		smb->status.max_age=cfg->mail_maxage;
		smb->status.max_msgs=0;	/* unlimited */
		smb->status.attr=SMB_EMAIL;

		/* duplicate message-IDs must be allowed in mail database */
		dupechk_hashes&=~(1<<SMB_HASH_SOURCE_MSG_ID);

	} else {	/* sub-board */

		smb->status.max_crcs=cfg->sub[smb->subnum]->maxcrcs;
		smb->status.max_msgs=cfg->sub[smb->subnum]->maxmsgs;
		smb->status.max_age=cfg->sub[smb->subnum]->maxage;
		smb->status.attr=0;

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
 
	add_msg_ids(cfg, smb, msg, remsg);

	if((msg->to != NULL && !str_is_ascii(msg->to) && utf8_str_is_valid(msg->to))
		|| (msg->from != NULL && !str_is_ascii(msg->from) && utf8_str_is_valid(msg->from))
		|| (msg->subj != NULL && !str_is_ascii(msg->subj) && utf8_str_is_valid(msg->subj)))
		msg->hdr.auxattr |= MSG_HFIELDS_UTF8;

	if(msg->ftn_charset == NULL && !str_is_ascii(msgbuf) && utf8_str_is_valid(msgbuf))
		smb_hfield_str(msg, FIDOCHARSET, FIDO_CHARSET_UTF8);

	msgbuf = strdup(msgbuf);
	if(msgbuf == NULL)
		return SMB_FAILURE;
	if((i=smb_addmsg(smb,msg,smb_storage_mode(cfg, smb),dupechk_hashes,xlat,(uchar*)msgbuf, findsig(msgbuf)))==SMB_SUCCESS
		&& msg->to!=NULL	/* no recipient means no header created at this stage */) {
		if(smb->subnum == INVALID_SUB) {
			if(msg->to_net.type == NET_FIDO && cfg->netmail_sem[0]) {
				char tmp[MAX_PATH + 1];
				ftouch(cmdstr(cfg,NULL,cfg->netmail_sem,nulstr,nulstr,tmp, sizeof(tmp)));
			}
		} else
			signal_sub_sem(cfg,smb->subnum);

		if(msg->to_net.type == NET_NONE && !(msg->hdr.attr & MSG_ANONYMOUS) && cfg->text != NULL) {
			int usernum = 0;
			if(msg->to_ext != NULL)
				usernum = atoi(msg->to_ext);
			else if(smb->subnum != INVALID_SUB && (cfg->sub[smb->subnum]->misc & SUB_NAME))
				usernum = userdatdupe(cfg, 0, U_NAME, LEN_NAME, msg->to, /* del: */FALSE, /* next: */FALSE, NULL, NULL);
			else
				usernum = matchuser(cfg, msg->to, TRUE /* sysop_alias */);
			if(usernum > 0 && (client == NULL || usernum != (int)client->usernum)) {
				char str[256];
				if(smb->subnum == INVALID_SUB) {
					safe_snprintf(str, sizeof(str), cfg->text[UserSentYouMail], msg->from);
					putsmsg(cfg, usernum, str);
				} else {
					char fido_buf[64];
					const char* via = smb_netaddrstr(&msg->from_net, fido_buf);
					if(via == NULL)
						via = (client == NULL) ? "" : client->protocol;
					safe_snprintf(str, sizeof(str), cfg->text[MsgPostedToYouVia]
						,msg->from
						,via
						,cfg->grp[cfg->sub[smb->subnum]->grp]->sname,cfg->sub[smb->subnum]->lname);
					putsmsg(cfg, usernum, str);
				}
			}
		}
	}
	free(msgbuf);
	return(i);
}

extern "C" int votemsg(scfg_t* cfg, smb_t* smb, smbmsg_t* msg, const char* smsgfmt, const char* votefmt)
{
	int result;
	smbmsg_t remsg;

	ZERO_VAR(remsg);

	if(msg->hdr.when_imported.time == 0) {
		msg->hdr.when_imported.time = time32(NULL);
		msg->hdr.when_imported.zone = sys_timezone(cfg);
	}
	if(msg->hdr.when_written.time == 0)	/* Uninitialized */
		msg->hdr.when_written = msg->hdr.when_imported;

	add_msg_ids(cfg, smb, msg, /* remsg: */NULL);

	/* Look-up thread_back if RFC822 Reply-ID was specified */
	if(msg->hdr.thread_back == 0 && msg->reply_id != NULL) {
		if(smb_getmsgidx_by_msgid(smb, &remsg, msg->reply_id) == SMB_SUCCESS)
			msg->hdr.thread_back = remsg.idx.number;	/* poll or message being voted on */
	}
	if(msg->hdr.thread_back == 0) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s thread_back field is zero (reply_id=%s, ftn_reply=%s)"
			,__FUNCTION__, msg->reply_id, msg->ftn_reply);
		return SMB_ERR_HDR_FIELD;
	}
	if(smb_voted_already(smb, msg->hdr.thread_back, msg->from, (enum smb_net_type)msg->from_net.type, msg->from_net.addr))
		return SMB_DUPE_MSG;
	remsg.hdr.number = msg->hdr.thread_back;
	if((result = smb_getmsgidx(smb, &remsg)) != SMB_SUCCESS)
		return result;
	if((result = smb_getmsghdr(smb, &remsg)) != SMB_SUCCESS)
		return result;
	if(remsg.hdr.auxattr&POLL_CLOSED)
		result = SMB_CLOSED;
	else
		result = smb_addvote(smb, msg, smb_storage_mode(cfg, smb));
	if(result == SMB_SUCCESS && smsgfmt != NULL && remsg.from_ext != NULL) {
		user_t user;
		ZERO_VAR(user);
		user.number = atoi(remsg.from_ext);
		if(getuserdat(cfg, &user) == 0 && 
			(stricmp(remsg.from, user.alias) == 0 || stricmp(remsg.from, user.name) == 0)) {
			char from[256];
			char tstr[128];
			char smsg[4000];
			char votes[3000] = "";
			if(msg->from_net.type)
				safe_snprintf(from, sizeof(from), "%s (%s)", msg->from, smb_netaddr(&msg->from_net));
			else
				SAFECOPY(from, msg->from);
			if(remsg.hdr.type == SMB_MSG_TYPE_POLL && votefmt != NULL) {
				int answers = 0;
				for(int i=0; i<remsg.total_hfields; i++) {
					if(remsg.hfield[i].type == SMB_POLL_ANSWER) {
						if(msg->hdr.votes&(1<<answers)) {
							char vote[128];
							SAFEPRINTF(vote, votefmt, (char*)remsg.hfield_dat[i]);
							SAFECAT(votes, vote);
						}
						answers++;
					}
				}
			}
			safe_snprintf(smsg, sizeof(smsg), smsgfmt
				,timestr(cfg, msg->hdr.when_written.time, tstr)
				,cfg->grp[cfg->sub[smb->subnum]->grp]->sname
				,cfg->sub[smb->subnum]->sname
				,from
				,remsg.subj);
			SAFECAT(smsg, votes);
			putsmsg(cfg, user.number, smsg);
		}
	}
	smb_freemsgmem(&remsg);
	return result;
}

extern "C" int closepoll(scfg_t* cfg, smb_t* smb, uint32_t msgnum, const char* username)
{
	int result;
	smbmsg_t msg;

	ZERO_VAR(msg);

	msg.hdr.when_imported.time = time32(NULL);
	msg.hdr.when_imported.zone = sys_timezone(cfg);
	msg.hdr.when_written = msg.hdr.when_imported;
	msg.hdr.thread_back = msgnum;
	smb_hfield_str(&msg, SENDER, username);

	add_msg_ids(cfg, smb, &msg, /* remsg: */NULL);

	result = smb_addpollclosure(smb, &msg, smb_storage_mode(cfg, smb));

	smb_freemsgmem(&msg);
	return result;
}

extern "C" int postpoll(scfg_t* cfg, smb_t* smb, smbmsg_t* msg)
{
	if(msg->hdr.when_imported.time == 0) {
		msg->hdr.when_imported.time = time32(NULL);
		msg->hdr.when_imported.zone = sys_timezone(cfg);
	}
	if(msg->hdr.when_written.time == 0)
		msg->hdr.when_written = msg->hdr.when_imported;

	add_msg_ids(cfg, smb, msg, /* remsg: */NULL);

	return smb_addpoll(smb, msg, smb_storage_mode(cfg, smb));
}

// Send an email and a short-message to a local user about something important (e.g. a system error)
extern "C" int notify(scfg_t* cfg, uint usernumber, const char* subject, const char* text)
{
	int			i;
	smb_t		smb = {0};
	uint16_t	xlat;
	int			storage;
	long		dupechk_hashes;
	uint16_t	agent = AGENT_PROCESS;
	uint16_t	nettype = NET_UNKNOWN;
	smbmsg_t	msg = {0};
	user_t		user = {0};
	char		str[128];

	user.number = usernumber;
	if((i = getuserdat(cfg, &user)) != 0)
		return i;

	msg.hdr.when_imported.time = time32(NULL);
	msg.hdr.when_imported.zone = sys_timezone(cfg);
	msg.hdr.when_written = msg.hdr.when_imported;
	smb_hfield(&msg, SENDERAGENT, sizeof(agent), &agent);
	smb_hfield_str(&msg, SENDER, cfg->sys_name);
	smb_hfield_str(&msg, RECIPIENT, user.alias);
	if(cfg->sys_misc&SM_FWDTONET && user.misc&NETMAIL && user.netmail[0]) {
		smb_hfield_netaddr(&msg, RECIPIENTNETADDR, user.netmail, &nettype);
		smb_hfield_bin(&msg, RECIPIENTNETTYPE, nettype);
	} else {
		SAFEPRINTF(str, "%u", usernumber);
		smb_hfield_str(&msg, RECIPIENTEXT, str);
	}
	smb_hfield_str(&msg, SUBJECT, subject);
	add_msg_ids(cfg, &smb, &msg, /* remsg: */NULL);
	if(msgbase_open(cfg, &smb, INVALID_SUB, &storage, &dupechk_hashes, &xlat) == SMB_SUCCESS) {
		smb_addmsg(&smb, &msg, storage, dupechk_hashes, xlat, (uchar*)text, /* tail: */NULL);
		smb_close(&smb);
	}
	smb_freemsgmem(&msg);

	char smsg[1024];
	if(text != NULL)
		safe_snprintf(smsg, sizeof(smsg),"\1n\1h%s \1r%s:\r\n%s\1n\r\n"
			,timestr(cfg, msg.hdr.when_imported.time, str)
			,subject
			,text);
	else
		safe_snprintf(smsg, sizeof(smsg),"\1n\1h%s \1r%s\1n\r\n"
			,timestr(cfg, msg.hdr.when_imported.time, str)
			,subject);
	return putsmsg(cfg, usernumber, smsg);
}
