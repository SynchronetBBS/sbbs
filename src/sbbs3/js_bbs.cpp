/* js_bbs.cpp */

/* Synchronet JavaScript "bbs" Object */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifdef JAVASCRIPT

/*****************************/
/* BBS Object Properites */
/*****************************/
enum {
	 BBS_PROP_SYS_STATUS
	,BBS_PROP_STARTUP_OPT
	,BBS_PROP_ANSWER_TIME
	,BBS_PROP_LOGON_TIME
	,BBS_PROP_NS_TIME
	,BBS_PROP_LAST_NS_TIME
	,BBS_PROP_ONLINE
	,BBS_PROP_TIMELEFT
	,BBS_PROP_EVENT_TIME
	,BBS_PROP_EVENT_CODE

	,BBS_PROP_NODE_NUM
	,BBS_PROP_NODE_MISC
	,BBS_PROP_NODE_ACTION
	,BBS_PROP_NODE_VAL_USER

	,BBS_PROP_LOGON_ULB
	,BBS_PROP_LOGON_DLB
	,BBS_PROP_LOGON_ULS
	,BBS_PROP_LOGON_DLS
	,BBS_PROP_LOGON_POSTS
	,BBS_PROP_LOGON_EMAILS
	,BBS_PROP_LOGON_FBACKS
	,BBS_PROP_POSTS_READ

	,BBS_PROP_MENU_DIR
	,BBS_PROP_MENU_FILE
	,BBS_PROP_MAIN_CMDS
	,BBS_PROP_FILE_CMDS

	,BBS_PROP_CURGRP
	,BBS_PROP_CURSUB
	,BBS_PROP_CURLIB
	,BBS_PROP_CURDIR

	,BBS_PROP_CONNECTION		/* READ ONLY */
	,BBS_PROP_RLOGIN_NAME
	,BBS_PROP_CLIENT_NAME

	,BBS_PROP_ALTUL

	,BBS_PROP_ERRORLEVEL		/* READ ONLY */

	/* READ ONLY */
	,BBS_PROP_SMB_GROUP		
	,BBS_PROP_SMB_GROUP_DESC
	,BBS_PROP_SMB_GROUP_NUM
	,BBS_PROP_SMB_SUB
	,BBS_PROP_SMB_SUB_DESC
	,BBS_PROP_SMB_SUB_CODE
	,BBS_PROP_SMB_SUB_NUM
	,BBS_PROP_SMB_ATTR
	,BBS_PROP_SMB_LAST_MSG
	,BBS_PROP_SMB_TOTAL_MSGS
	,BBS_PROP_SMB_MSGS
	,BBS_PROP_SMB_CURMSG

	/* READ ONLY */
	,BBS_PROP_MSG_TO
	,BBS_PROP_MSG_TO_EXT
	,BBS_PROP_MSG_TO_NET
	,BBS_PROP_MSG_TO_AGENT
	,BBS_PROP_MSG_FROM
	,BBS_PROP_MSG_FROM_EXT
	,BBS_PROP_MSG_FROM_NET
	,BBS_PROP_MSG_FROM_AGENT
	,BBS_PROP_MSG_REPLYTO
	,BBS_PROP_MSG_REPLYTO_EXT
	,BBS_PROP_MSG_REPLYTO_NET
	,BBS_PROP_MSG_REPLYTO_AGENT
	,BBS_PROP_MSG_SUBJECT
	,BBS_PROP_MSG_DATE
	,BBS_PROP_MSG_TIMEZONE
	,BBS_PROP_MSG_DATE_IMPORTED
	,BBS_PROP_MSG_ATTR
	,BBS_PROP_MSG_AUXATTR
	,BBS_PROP_MSG_NETATTR
	,BBS_PROP_MSG_OFFSET
	,BBS_PROP_MSG_NUMBER
	,BBS_PROP_MSG_EXPIRATION
	,BBS_PROP_MSG_FORWARDED
	,BBS_PROP_MSG_THREAD_BACK
	,BBS_PROP_MSG_THREAD_NEXT
	,BBS_PROP_MSG_THREAD_FIRST
	,BBS_PROP_MSG_ID
	,BBS_PROP_MSG_REPLY_ID
	,BBS_PROP_MSG_DELIVERY_ATTEMPTS

	/* READ ONLY */
	,BBS_PROP_BATCH_UPLOAD_TOTAL
	,BBS_PROP_BATCH_DNLOAD_TOTAL

};

#ifdef _DEBUG
	static char* bbs_prop_desc[] = {
	 "system status bitfield (see <tt>SS_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)"
	,"startup options bitfield (see <tt>BBS_OPT_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)"
	,"answer time, in time_t format"
	,"logon time, in time_t format"
	,"file newscan time, in time_t format"
	,"previous newscan time, in time_t format"
	,"online (see <tt>ON_*</tt> in <tt>sbbsdefs.js</tt> for valid values)"
	,"time left (in seconds)"
	,"time of next exclusive event (in time_t format), or 0 if none"
	,"internal code of next exclusive event"

	,"current node number"
	,"current node settings bitfield (see <tt>NM_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)"
	,"current node action (see <tt>nodedefs.js</tt> for valid values)"
	,"validation feedback user for this node (or 0 for no validation feedback required)"

	,"bytes uploaded during this session"
	,"bytes downloaded during this session"
	,"files uploaded during this session"
	,"files downloaded during this session"
	,"messages posted during this session"
	,"e-mails sent during this session"
	,"feedback messages sent during this session"
	,"messages read during this session"

	,"menu subdirectory (overrides default)"
	,"menu file (overrides default)"
	,"total main menu commands received from user during this session"
	,"total file menu commands received from user during this session"

	,"current message group"
	,"current message sub-board"
	,"current file library"
	,"current file directory"

	,"remote connection type"
	,"rlogin name"
	,"client name"

	,"current alternate upload path number"

	,"error level returned from last executed external program"

	/* READ ONLY */
	,"message group name of message being read"
	,"message group description of message being read"
	,"message group number of message being read"
	,"sub-board name of message being read"
	,"sub-board description of message being read"
	,"sub-board internal code of message being read"
	,"sub-board number of message being read"
	,"message base attributes"
	,"highest message number in message base"
	,"total number of messages in message base"
	,"number of messages loaded from message base"
	,"current message number in message base"

	/* READ ONLY */
	,"message recipient name"
	,"message recipient extension"
	,"message recipient network type"
	,"message recipient agent type"
	,"message sender name"
	,"message sender extension"
	,"message sender network type"
	,"message sender agent type"
	,"message reply-to name"
	,"message reply-to extension"
	,"message reply-to network type"
	,"message reply-to agent type"
	,"message subject"
	,"message date/time"
	,"message time zone"
	,"message date/time imported"
	,"message attributes"
	,"message auxillary attributes"
	,"message network attributes"
	,"message header offset"
	,"message number"
	,"message expiration"
	,"message forwarded"
	,"message thread, back message number (AKA msg_thread_orig)"
	,"message thread, next message number"
	,"message thread, first reply to this message"
	,"message identifier"
	,"message replied-to identifier"
	,"message delivery attempt counter"

	,"number of files in batch upload queue"
	,"number of files in batch download queue"

	,NULL
	};
#endif

static JSBool js_bbs_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	char*		p=NULL;
	char*		nulstr="";
	ulong		val=0;
    jsint       tiny;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case BBS_PROP_SYS_STATUS:
			val=sbbs->sys_status;
			break;
		case BBS_PROP_STARTUP_OPT:
			val=sbbs->startup->options;
			break;
		case BBS_PROP_ANSWER_TIME:
			val=sbbs->answertime;
			break;
		case BBS_PROP_LOGON_TIME:
			val=sbbs->logontime;
			break;
		case BBS_PROP_NS_TIME:
			val=sbbs->ns_time;
			break;
		case BBS_PROP_LAST_NS_TIME:
			val=sbbs->last_ns_time;
			break;
		case BBS_PROP_ONLINE:
			val=sbbs->online;
			break;
		case BBS_PROP_TIMELEFT:
			val=sbbs->timeleft;
			break;
		case BBS_PROP_EVENT_TIME:
			val=sbbs->event_time;
			break;
		case BBS_PROP_EVENT_CODE:
			p=sbbs->event_code;
			break;

		case BBS_PROP_NODE_NUM:
			val=sbbs->cfg.node_num;
			break;
		case BBS_PROP_NODE_MISC:
			val=sbbs->cfg.node_misc;
			break;
		case BBS_PROP_NODE_ACTION:
			val=sbbs->action;
			break;
		case BBS_PROP_NODE_VAL_USER:
			val=sbbs->cfg.node_valuser;
			break;

		case BBS_PROP_LOGON_ULB:
			val=sbbs->logon_ulb;
			break;
		case BBS_PROP_LOGON_DLB:
			val=sbbs->logon_dlb;
			break;
		case BBS_PROP_LOGON_ULS:
			val=sbbs->logon_uls;
			break;
		case BBS_PROP_LOGON_DLS:
			val=sbbs->logon_dls;
			break;
		case BBS_PROP_LOGON_POSTS:
			val=sbbs->logon_posts;
			break;
		case BBS_PROP_LOGON_EMAILS:
			val=sbbs->logon_emails;
			break;
		case BBS_PROP_LOGON_FBACKS:
			val=sbbs->logon_fbacks;
			break;
		case BBS_PROP_POSTS_READ:
			val=sbbs->posts_read;
			break;

		case BBS_PROP_MENU_DIR:
			p=sbbs->menu_dir;
			break;
		case BBS_PROP_MENU_FILE:
			p=sbbs->menu_file;
			break;
		case BBS_PROP_MAIN_CMDS:
			val=sbbs->main_cmds;
			break;
		case BBS_PROP_FILE_CMDS:
			val=sbbs->xfer_cmds;
			break;

		case BBS_PROP_CURGRP:
			val=sbbs->curgrp;
			break;
		case BBS_PROP_CURSUB:
			if(sbbs->curgrp<sbbs->usrgrps)
				val=sbbs->cursub[sbbs->curgrp];
			break;
		case BBS_PROP_CURLIB:
			val=sbbs->curlib;
			break;
		case BBS_PROP_CURDIR:
			if(sbbs->curlib<sbbs->usrlibs)
				val=sbbs->curdir[sbbs->curlib];
			break;

		case BBS_PROP_CONNECTION:
			p=sbbs->connection;
			break;
		case BBS_PROP_RLOGIN_NAME:
			p=sbbs->rlogin_name;
			break;
		case BBS_PROP_CLIENT_NAME:
			p=sbbs->client_name;
			break;

		case BBS_PROP_ALTUL:
			val=sbbs->altul;
			break;

		case BBS_PROP_ERRORLEVEL:
			val=sbbs->errorlevel;
			break;

		/* Currently Open Message Base (sbbs.smb) */
		case BBS_PROP_SMB_GROUP:
			if(sbbs->smb.subnum==INVALID_SUB || sbbs->smb.subnum>=sbbs->cfg.total_subs)
				p=nulstr;
			else
				p=sbbs->cfg.grp[sbbs->cfg.sub[sbbs->smb.subnum]->grp]->sname;
			break;
		case BBS_PROP_SMB_GROUP_DESC:
			if(sbbs->smb.subnum==INVALID_SUB || sbbs->smb.subnum>=sbbs->cfg.total_subs)
				p=nulstr;
			else
				p=sbbs->cfg.grp[sbbs->cfg.sub[sbbs->smb.subnum]->grp]->lname;
			break;
		case BBS_PROP_SMB_GROUP_NUM:
			if(sbbs->smb.subnum!=INVALID_SUB && sbbs->smb.subnum<sbbs->cfg.total_subs) {
				uint ugrp;
				for(ugrp=0;ugrp<sbbs->usrgrps;ugrp++)
					if(sbbs->usrgrp[ugrp]==sbbs->cfg.sub[sbbs->smb.subnum]->grp)
						break;
				val=ugrp+1;
			}
			break;
		case BBS_PROP_SMB_SUB:
			if(sbbs->smb.subnum==INVALID_SUB || sbbs->smb.subnum>=sbbs->cfg.total_subs)
				p=nulstr;
			else
				p=sbbs->cfg.sub[sbbs->smb.subnum]->sname;
			break;
		case BBS_PROP_SMB_SUB_DESC:
			if(sbbs->smb.subnum==INVALID_SUB || sbbs->smb.subnum>=sbbs->cfg.total_subs)
				p=nulstr;
			else
				p=sbbs->cfg.sub[sbbs->smb.subnum]->lname;
			break;
		case BBS_PROP_SMB_SUB_CODE:
			if(sbbs->smb.subnum==INVALID_SUB || sbbs->smb.subnum>=sbbs->cfg.total_subs)
				p=nulstr;
			else
				p=sbbs->cfg.sub[sbbs->smb.subnum]->code;
			break;
		case BBS_PROP_SMB_SUB_NUM:
			if(sbbs->smb.subnum!=INVALID_SUB && sbbs->smb.subnum<sbbs->cfg.total_subs) {
				uint ugrp;
				for(ugrp=0;ugrp<sbbs->usrgrps;ugrp++)
					if(sbbs->usrgrp[ugrp]==sbbs->cfg.sub[sbbs->smb.subnum]->grp)
						break;
				uint usub;
				for(usub=0;usub<sbbs->usrsubs[ugrp];usub++)
					if(sbbs->usrsub[ugrp][usub]==sbbs->smb.subnum)
						break;
				val=usub+1;
			}
			break;
		case BBS_PROP_SMB_ATTR:
			val=sbbs->smb.status.attr;
			break;
		case BBS_PROP_SMB_LAST_MSG:
			val=sbbs->smb.status.last_msg;
			break;
		case BBS_PROP_SMB_TOTAL_MSGS:
			val=sbbs->smb.status.total_msgs;
			break;
		case BBS_PROP_SMB_MSGS:
			val=sbbs->smb.msgs;
			break;
		case BBS_PROP_SMB_CURMSG:
			val=sbbs->smb.curmsg;
			break;

		/* Currently Displayed Message Header (sbbs.current_msg) */
		case BBS_PROP_MSG_TO:
			if(sbbs->current_msg==NULL || sbbs->current_msg->to==NULL)
				p=nulstr;
			else
				p=sbbs->current_msg->to;
			break;
		case BBS_PROP_MSG_TO_EXT:
			if(sbbs->current_msg==NULL || sbbs->current_msg->to_ext==NULL)
				p=nulstr;
			else
				p=sbbs->current_msg->to_ext;
			break;
		case BBS_PROP_MSG_TO_NET:
			if(sbbs->current_msg==NULL || sbbs->current_msg->to_net.type==NET_NONE)
				p=nulstr;
			else
				p=smb_netaddr(&sbbs->current_msg->to_net);
			break;
		case BBS_PROP_MSG_TO_AGENT:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->to_agent;
			break;
		case BBS_PROP_MSG_FROM:
			if(sbbs->current_msg==NULL || sbbs->current_msg->from==NULL)
				p=nulstr;
			else
				p=sbbs->current_msg->from;
			break;
		case BBS_PROP_MSG_FROM_EXT:
			if(sbbs->current_msg==NULL || sbbs->current_msg->from_ext==NULL)
				p=nulstr;
			else
				p=sbbs->current_msg->from_ext;
			break;
		case BBS_PROP_MSG_FROM_NET:
			if(sbbs->current_msg==NULL || sbbs->current_msg->from_net.type==NET_NONE)
				p=nulstr;
			else
				p=smb_netaddr(&sbbs->current_msg->from_net);
			break;
		case BBS_PROP_MSG_FROM_AGENT:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->from_agent;
			break;
		case BBS_PROP_MSG_REPLYTO:
			if(sbbs->current_msg==NULL || sbbs->current_msg->replyto==NULL)
				p=nulstr;
			else
				p=sbbs->current_msg->replyto;
			break;
		case BBS_PROP_MSG_REPLYTO_EXT:
			if(sbbs->current_msg==NULL || sbbs->current_msg->replyto_ext==NULL)
				p=nulstr;
			else
				p=sbbs->current_msg->replyto_ext;
			break;
		case BBS_PROP_MSG_REPLYTO_NET:
			if(sbbs->current_msg==NULL || sbbs->current_msg->replyto_net.type==NET_NONE)
				p=nulstr;
			else
				p=smb_netaddr(&sbbs->current_msg->replyto_net);
			break;
		case BBS_PROP_MSG_REPLYTO_AGENT:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->replyto_agent;
			break;

		case BBS_PROP_MSG_SUBJECT:
			if(sbbs->current_msg==NULL || sbbs->current_msg->subj==NULL)
				p=nulstr;
			else
				p=sbbs->current_msg->subj;
			break;
		case BBS_PROP_MSG_DATE:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->hdr.when_written.time;
			break;
		case BBS_PROP_MSG_TIMEZONE:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->hdr.when_written.zone;
			break;
		case BBS_PROP_MSG_DATE_IMPORTED:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->hdr.when_imported.time;
			break;
		case BBS_PROP_MSG_ATTR:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->hdr.attr;
			break;
		case BBS_PROP_MSG_AUXATTR:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->hdr.auxattr;
			break;
		case BBS_PROP_MSG_NETATTR:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->hdr.netattr;
			break;
		case BBS_PROP_MSG_OFFSET:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->offset;
			break;
		case BBS_PROP_MSG_NUMBER:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->hdr.number;
			break;
		case BBS_PROP_MSG_EXPIRATION:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->expiration;
			break;
		case BBS_PROP_MSG_FORWARDED:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->forwarded;
			break;
		case BBS_PROP_MSG_THREAD_BACK:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->hdr.thread_back;
			break;
		case BBS_PROP_MSG_THREAD_NEXT:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->hdr.thread_next;
			break;
		case BBS_PROP_MSG_THREAD_FIRST:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->hdr.thread_first;
			break;
		case BBS_PROP_MSG_DELIVERY_ATTEMPTS:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->hdr.delivery_attempts;
			break;
		case BBS_PROP_MSG_ID:
			if(sbbs->current_msg==NULL || sbbs->current_msg->id==NULL)
				p=nulstr;
			else
				p=sbbs->current_msg->id;
			break;
		case BBS_PROP_MSG_REPLY_ID:
			if(sbbs->current_msg==NULL || sbbs->current_msg->reply_id==NULL)
				p=nulstr;
			else
				p=sbbs->current_msg->reply_id;
			break;

		case BBS_PROP_BATCH_UPLOAD_TOTAL:
			val=sbbs->batup_total;
			break;
		case BBS_PROP_BATCH_DNLOAD_TOTAL:
			val=sbbs->batdn_total;
			break;

		default:
			return(JS_TRUE);
	}
	if(p!=NULL) {
		JSString* js_str=JS_NewStringCopyZ(cx, p);
		if(js_str==NULL)
			return(JS_FALSE);
		*vp = STRING_TO_JSVAL(js_str);
	} else
		JS_NewNumberValue(cx,val,vp);

	return(JS_TRUE);
}

static JSBool js_bbs_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	char*		p=NULL;
	int32		val=0;
    jsint       tiny;
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	if(JSVAL_IS_NUMBER(*vp) || JSVAL_IS_BOOLEAN(*vp))
		JS_ValueToInt32(cx, *vp, &val);
	else if(JSVAL_IS_STRING(*vp)) {
		if((js_str = JS_ValueToString(cx, *vp))==NULL)
			return(JS_FALSE);
		p=JS_GetStringBytes(js_str);
	}

	switch(tiny) {
		case BBS_PROP_SYS_STATUS:
			sbbs->sys_status=val;
			break;
		case BBS_PROP_STARTUP_OPT:
			sbbs->startup->options=val;
			break;
		case BBS_PROP_ANSWER_TIME:
			sbbs->answertime=val;
			break;
		case BBS_PROP_LOGON_TIME:
			sbbs->logontime=val;
			break;
		case BBS_PROP_NS_TIME:
			sbbs->ns_time=val;
			break;
		case BBS_PROP_LAST_NS_TIME:
			sbbs->last_ns_time=val;
			break;
		case BBS_PROP_ONLINE:
			sbbs->online=val;
			break;
		case BBS_PROP_NODE_MISC:
			sbbs->cfg.node_misc=val;
			break;
		case BBS_PROP_NODE_ACTION:
			sbbs->action=(uchar)val;
			break;
		case BBS_PROP_NODE_VAL_USER:
			sbbs->cfg.node_valuser=(ushort)val;
			break;
		case BBS_PROP_LOGON_ULB:
			sbbs->logon_ulb=val;
			break;
		case BBS_PROP_LOGON_DLB:
			sbbs->logon_dlb=val;
			break;
		case BBS_PROP_LOGON_ULS:
			sbbs->logon_uls=val;
			break;
		case BBS_PROP_LOGON_DLS:
			sbbs->logon_dls=val;
			break;
		case BBS_PROP_LOGON_POSTS:
			sbbs->logon_posts=val;
			break;
		case BBS_PROP_LOGON_EMAILS:
			sbbs->logon_emails=val;
			break;
		case BBS_PROP_LOGON_FBACKS:
			sbbs->logon_fbacks=val;
			break;
		case BBS_PROP_POSTS_READ:
			sbbs->posts_read=val;
			break;
		case BBS_PROP_MENU_DIR:
			SAFECOPY(sbbs->menu_dir,p);
			break;
		case BBS_PROP_MENU_FILE:
			SAFECOPY(sbbs->menu_file,p);
			break;
		case BBS_PROP_MAIN_CMDS:
			sbbs->main_cmds=val;
			break;
		case BBS_PROP_FILE_CMDS:
			sbbs->xfer_cmds=val;
			break;

		case BBS_PROP_CURGRP:
			if(p!=NULL) {	/* set by name */
				uint i;
				for(i=0;i<sbbs->usrgrps;i++)
					if(!stricmp(sbbs->cfg.grp[sbbs->usrgrp[i]]->sname,p))
						break;
				if(i<sbbs->usrgrps)
					sbbs->curgrp=i;
				break;
			}
			if((uint)val<sbbs->cfg.total_grps && (uint)val<sbbs->usrgrps)
				sbbs->curgrp=val;
			break;
		case BBS_PROP_CURSUB:
			if(p!=NULL) {	/* set by code */
				for(uint i=0;i<sbbs->usrgrps;i++)
					for(uint j=0;j<sbbs->usrsubs[i];j++)
						if(!stricmp(sbbs->cfg.sub[sbbs->usrsub[i][j]]->code,p)) {
							sbbs->curgrp=i;
							sbbs->cursub[i]=j;
							break; 
						}
				break;
			}
			if(sbbs->curgrp<sbbs->cfg.total_grps && (uint)val<sbbs->usrsubs[sbbs->curgrp])
				sbbs->cursub[sbbs->curgrp]=val;
			break;
		case BBS_PROP_CURLIB:
			if(p!=NULL) {	/* set by name */
				uint i;
				for(i=0;i<sbbs->usrlibs;i++)
					if(!stricmp(sbbs->cfg.lib[sbbs->usrlib[i]]->sname,p))
						break;
				if(i<sbbs->usrlibs)
					sbbs->curlib=i;
				break;
			}
			if((uint)val<sbbs->cfg.total_libs && (uint)val<sbbs->usrlibs)
				sbbs->curlib=val;
			break;
		case BBS_PROP_CURDIR:
			if(p!=NULL) {	/* set by code */
				for(uint i=0;i<sbbs->usrlibs;i++)
					for(uint j=0;j<sbbs->usrdirs[i];j++)
						if(!stricmp(sbbs->cfg.dir[sbbs->usrdir[i][j]]->code,p)) {
							sbbs->curlib=i;
							sbbs->curdir[i]=j;
							break; 
						}
				break;
			}
			if(sbbs->curlib<sbbs->cfg.total_libs && (uint)val<sbbs->usrdirs[sbbs->curlib])
				sbbs->curdir[sbbs->curlib]=val;
			break;

		case BBS_PROP_RLOGIN_NAME:
			SAFECOPY(sbbs->rlogin_name,p);
			break;
		case BBS_PROP_CLIENT_NAME:
			SAFECOPY(sbbs->client_name,p);
			break;

		case BBS_PROP_ALTUL:
			if(val<sbbs->cfg.altpaths)
				sbbs->altul=(ushort)val;
			break;
		default:
			return(JS_TRUE);
	}

	if(sbbs->usrgrps)
		sbbs->cursubnum=sbbs->usrsub[sbbs->curgrp][sbbs->cursub[sbbs->curgrp]];		/* Used for ARS */
	else
		sbbs->cursubnum=INVALID_SUB;
	if(sbbs->usrlibs) 
		sbbs->curdirnum=sbbs->usrdir[sbbs->curlib][sbbs->curdir[sbbs->curlib]];		/* Used for ARS */
	else 
		sbbs->curdirnum=INVALID_DIR;

	return(JS_TRUE);
}

#define PROP_READONLY JSPROP_ENUMERATE|JSPROP_READONLY

static jsSyncPropertySpec js_bbs_properties[] = {
/*		 name				,tinyid					,flags				,ver	*/

	{	"sys_status"		,BBS_PROP_SYS_STATUS	,JSPROP_ENUMERATE	,310},
	{	"startup_options"	,BBS_PROP_STARTUP_OPT	,JSPROP_ENUMERATE	,310},
	{	"answer_time"		,BBS_PROP_ANSWER_TIME	,JSPROP_ENUMERATE	,310},
	{	"logon_time"		,BBS_PROP_LOGON_TIME	,JSPROP_ENUMERATE	,310},
	{	"new_file_time"		,BBS_PROP_NS_TIME		,JSPROP_ENUMERATE	,310},
	{	"last_new_file_time",BBS_PROP_LAST_NS_TIME	,JSPROP_ENUMERATE	,310},
	{	"online"			,BBS_PROP_ONLINE		,JSPROP_ENUMERATE	,310},
	{	"timeleft"			,BBS_PROP_TIMELEFT		,JSPROP_READONLY	,310},	/* alias */
	{	"time_left"			,BBS_PROP_TIMELEFT		,PROP_READONLY		,311},
	{	"event_time"		,BBS_PROP_EVENT_TIME	,PROP_READONLY		,311},
	{	"event_code"		,BBS_PROP_EVENT_CODE	,PROP_READONLY		,311},
	{	"node_num"			,BBS_PROP_NODE_NUM		,PROP_READONLY		,310},
	{	"node_settings"		,BBS_PROP_NODE_MISC		,JSPROP_ENUMERATE	,310},
	{	"node_action"		,BBS_PROP_NODE_ACTION	,JSPROP_ENUMERATE	,310},
	{	"node_val_user"		,BBS_PROP_NODE_VAL_USER	,JSPROP_ENUMERATE	,310},
	{	"logon_ulb"			,BBS_PROP_LOGON_ULB		,JSPROP_ENUMERATE	,310},
	{	"logon_dlb"			,BBS_PROP_LOGON_DLB		,JSPROP_ENUMERATE	,310},
	{	"logon_uls"			,BBS_PROP_LOGON_ULS		,JSPROP_ENUMERATE	,310},
	{	"logon_dls"			,BBS_PROP_LOGON_DLS		,JSPROP_ENUMERATE	,310},
	{	"logon_posts"		,BBS_PROP_LOGON_POSTS	,JSPROP_ENUMERATE	,310},
	{	"logon_emails"		,BBS_PROP_LOGON_EMAILS	,JSPROP_ENUMERATE	,310},
	{	"logon_fbacks"		,BBS_PROP_LOGON_FBACKS	,JSPROP_ENUMERATE	,310},
	{	"posts_read"		,BBS_PROP_POSTS_READ	,JSPROP_ENUMERATE	,310},
	{	"menu_dir"			,BBS_PROP_MENU_DIR		,JSPROP_ENUMERATE	,310},
	{	"menu_file"			,BBS_PROP_MENU_FILE		,JSPROP_ENUMERATE	,310},
	{	"main_cmds"			,BBS_PROP_MAIN_CMDS		,JSPROP_ENUMERATE	,310},
	{	"file_cmds"			,BBS_PROP_FILE_CMDS		,JSPROP_ENUMERATE	,310},
	{	"curgrp"			,BBS_PROP_CURGRP		,JSPROP_ENUMERATE	,310},
	{	"cursub"			,BBS_PROP_CURSUB		,JSPROP_ENUMERATE	,310},
	{	"curlib"			,BBS_PROP_CURLIB		,JSPROP_ENUMERATE	,310},
	{	"curdir"			,BBS_PROP_CURDIR		,JSPROP_ENUMERATE	,310},
	{	"connection"		,BBS_PROP_CONNECTION	,PROP_READONLY		,310},
	{	"rlogin_name"		,BBS_PROP_RLOGIN_NAME	,JSPROP_ENUMERATE	,310},
	{	"client_name"		,BBS_PROP_CLIENT_NAME	,JSPROP_ENUMERATE	,310},
	{	"alt_ul_dir"		,BBS_PROP_ALTUL			,JSPROP_ENUMERATE	,310},
	{	"errorlevel"		,BBS_PROP_ERRORLEVEL	,PROP_READONLY		,312},

	{	"smb_group"			,BBS_PROP_SMB_GROUP			,PROP_READONLY	,310},
	{	"smb_group_desc"	,BBS_PROP_SMB_GROUP_DESC	,PROP_READONLY	,310},
	{	"smb_group_number"	,BBS_PROP_SMB_GROUP_NUM		,PROP_READONLY	,310},
	{	"smb_sub"			,BBS_PROP_SMB_SUB			,PROP_READONLY	,310},
	{	"smb_sub_desc"		,BBS_PROP_SMB_SUB_DESC		,PROP_READONLY	,310},
	{	"smb_sub_code"		,BBS_PROP_SMB_SUB_CODE		,PROP_READONLY	,310},
	{	"smb_sub_number"	,BBS_PROP_SMB_SUB_NUM		,PROP_READONLY	,310},
	{	"smb_attr"			,BBS_PROP_SMB_ATTR			,PROP_READONLY	,310},
	{	"smb_last_msg"		,BBS_PROP_SMB_LAST_MSG		,PROP_READONLY	,310},
	{	"smb_total_msgs"	,BBS_PROP_SMB_TOTAL_MSGS	,PROP_READONLY	,310},
	{	"smb_msgs"			,BBS_PROP_SMB_MSGS			,PROP_READONLY	,310},
	{	"smb_curmsg"		,BBS_PROP_SMB_CURMSG		,PROP_READONLY	,310},
																		
	{	"msg_to"			,BBS_PROP_MSG_TO			,PROP_READONLY	,310},
	{	"msg_to_ext"		,BBS_PROP_MSG_TO_EXT		,PROP_READONLY	,310},
	{	"msg_to_net"		,BBS_PROP_MSG_TO_NET		,PROP_READONLY	,310},
	{	"msg_to_agent"		,BBS_PROP_MSG_TO_AGENT		,PROP_READONLY	,310},
	{	"msg_from"			,BBS_PROP_MSG_FROM			,PROP_READONLY	,310},
	{	"msg_from_ext"		,BBS_PROP_MSG_FROM_EXT		,PROP_READONLY	,310},
	{	"msg_from_net"		,BBS_PROP_MSG_FROM_NET		,PROP_READONLY	,310},
	{	"msg_from_agent"	,BBS_PROP_MSG_FROM_AGENT	,PROP_READONLY	,310},
	{	"msg_replyto"		,BBS_PROP_MSG_REPLYTO		,PROP_READONLY	,310},
	{	"msg_replyto_ext"	,BBS_PROP_MSG_REPLYTO_EXT	,PROP_READONLY	,310},
	{	"msg_replyto_net"	,BBS_PROP_MSG_REPLYTO_NET	,PROP_READONLY	,310},
	{	"msg_replyto_agent"	,BBS_PROP_MSG_REPLYTO_AGENT	,PROP_READONLY	,310},
	{	"msg_subject"		,BBS_PROP_MSG_SUBJECT		,PROP_READONLY	,310},
	{	"msg_date"			,BBS_PROP_MSG_DATE			,PROP_READONLY	,310},
	{	"msg_timezone"		,BBS_PROP_MSG_TIMEZONE		,PROP_READONLY	,310},
	{	"msg_date_imported"	,BBS_PROP_MSG_DATE_IMPORTED	,PROP_READONLY	,310},
	{	"msg_attr"			,BBS_PROP_MSG_ATTR			,PROP_READONLY	,310},
	{	"msg_auxattr"		,BBS_PROP_MSG_AUXATTR		,PROP_READONLY	,310},
	{	"msg_netattr"		,BBS_PROP_MSG_NETATTR		,PROP_READONLY	,310},
	{	"msg_offset"		,BBS_PROP_MSG_OFFSET		,PROP_READONLY	,310},
	{	"msg_number"		,BBS_PROP_MSG_NUMBER		,PROP_READONLY	,310},
	{	"msg_expiration"	,BBS_PROP_MSG_EXPIRATION	,PROP_READONLY	,310},
	{	"msg_forwarded"		,BBS_PROP_MSG_FORWARDED		,PROP_READONLY	,310},
	{	"msg_thread_back"	,BBS_PROP_MSG_THREAD_BACK	,PROP_READONLY	,312},
	{	"msg_thread_orig"	,BBS_PROP_MSG_THREAD_BACK	,JSPROP_READONLY,310},	/* alias */
	{	"msg_thread_next"	,BBS_PROP_MSG_THREAD_NEXT	,PROP_READONLY	,310},
	{	"msg_thread_first"	,BBS_PROP_MSG_THREAD_FIRST	,PROP_READONLY	,310},
	{	"msg_id"			,BBS_PROP_MSG_ID			,PROP_READONLY	,310},
	{	"msg_reply_id"		,BBS_PROP_MSG_REPLY_ID		,PROP_READONLY	,310},
	{	"msg_delivery_attempts"	,BBS_PROP_MSG_DELIVERY_ATTEMPTS
														,PROP_READONLY	,310},

	{	"batch_upload_total",BBS_PROP_BATCH_UPLOAD_TOTAL,PROP_READONLY	,310},
	{	"batch_dnload_total",BBS_PROP_BATCH_DNLOAD_TOTAL,PROP_READONLY	,310},
	{0}
};

/* Utility functions */
static uint get_subnum(JSContext* cx, sbbs_t* sbbs, jsval val)
{
	uint subnum=INVALID_SUB;

	if(JSVAL_IS_STRING(val)) {
		char* p=JS_GetStringBytes(JS_ValueToString(cx,val));
		for(subnum=0;subnum<sbbs->cfg.total_subs;subnum++)
			if(!stricmp(sbbs->cfg.sub[subnum]->code,p))
				break;
	} else if(JSVAL_IS_NUMBER(val))
		JS_ValueToInt32(cx,val,(int32*)&subnum);
	else if(sbbs->usrgrps>0)
		subnum=sbbs->usrsub[sbbs->curgrp][sbbs->cursub[sbbs->curgrp]];

	return(subnum);
}

static uint get_dirnum(JSContext* cx, sbbs_t* sbbs, jsval val)
{
	uint dirnum=INVALID_DIR;

	if(JSVAL_IS_STRING(val)) {
		char* p=JS_GetStringBytes(JS_ValueToString(cx,val));
		for(dirnum=0;dirnum<sbbs->cfg.total_dirs;dirnum++)
			if(!stricmp(sbbs->cfg.dir[dirnum]->code,p))
				break;
	} else if(JSVAL_IS_NUMBER(val))
		JS_ValueToInt32(cx,val,(int32*)&dirnum);
	else if(sbbs->usrlibs>0)
		dirnum=sbbs->usrdir[sbbs->curlib][sbbs->curdir[sbbs->curlib]];

	return(dirnum);
}

/**************************/
/* bbs Object Methods */
/**************************/

static JSBool
js_menu(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString*	str;
 	sbbs_t*		sbbs;
 
 	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
 		return(JS_FALSE);
 
 	str = JS_ValueToString(cx, argv[0]);
 	if (!str)
 		return(JS_FALSE);
 
	sbbs->menu(JS_GetStringBytes(str));

    return(JS_TRUE);
}
 
static JSBool
js_hangup(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->hangup();

	return(JS_TRUE);
}

static JSBool
js_nodesync(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->getnodedat(sbbs->cfg.node_num,&sbbs->thisnode,0);
	sbbs->nodesync();

	return(JS_TRUE);
}

static JSBool
js_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uintN		i;
	sbbs_t*		sbbs;
	long		mode=0;
    JSString*	cmd;
	JSString*	startup_dir=NULL;
	char*		p_startup_dir=NULL;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((cmd=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	for(i=1;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i]))
			JS_ValueToInt32(cx,argv[i],(int32*)&mode);
		else if(JSVAL_IS_STRING(argv[i]))
			startup_dir=JS_ValueToString(cx,argv[i]);
	}

	if(startup_dir!=NULL)
		p_startup_dir=JS_GetStringBytes(startup_dir);

	*rval = INT_TO_JSVAL(sbbs->external(JS_GetStringBytes(cmd),mode,p_startup_dir));

    return(JS_TRUE);
}

static JSBool
js_exec_xtrn(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		i=0;
	char*		code;
	sbbs_t*		sbbs;
    JSString*	str;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(JSVAL_IS_STRING(argv[0])) {
		if((str=JS_ValueToString(cx, argv[0]))==NULL)
			return(JS_FALSE);

		if((code=JS_GetStringBytes(str))==NULL)
			return(JS_FALSE);

		for(i=0;i<sbbs->cfg.total_xtrns;i++)
			if(!stricmp(sbbs->cfg.xtrn[i]->code,code))
				break;
	} else
		JS_ValueToInt32(cx,argv[0],&i);

	if(i>=sbbs->cfg.total_xtrns) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(sbbs->exec_xtrn(i));
	return(JS_TRUE);
}

static JSBool
js_user_event(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		i=0;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JS_ValueToInt32(cx,argv[0],&i);
	*rval = BOOLEAN_TO_JSVAL(sbbs->user_event((user_event_t)i));
	return(JS_TRUE);
}

static JSBool
js_chksyspass(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->chksyspass());
	return(JS_TRUE);
}

static JSBool
js_chkpass(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JSString* str=JS_ValueToString(cx,argv[0]);

	*rval = BOOLEAN_TO_JSVAL(sbbs->chkpass(JS_GetStringBytes(str),&sbbs->useron,true));
	return(JS_TRUE);
}


static JSBool
js_text(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		i=0;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JS_ValueToInt32(cx,argv[0],&i);
	i--;

	if(i<0 || i>=TOTAL_TEXT)
		*rval = JSVAL_NULL;
	else {
		JSString* js_str = JS_NewStringCopyZ(cx, sbbs->text[i]);
		if(js_str==NULL)
			return(JS_FALSE);
		*rval = STRING_TO_JSVAL(js_str);
	}

	return(JS_TRUE);
}

static JSBool
js_replace_text(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	int32		i=0;
	int			len;
	JSString*	js_str;
	sbbs_t*		sbbs;

	*rval = JSVAL_FALSE;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JS_ValueToInt32(cx,argv[0],&i);
	i--;

	if(i<0 || i>=TOTAL_TEXT)
		return(JS_TRUE);

	if(sbbs->text[i]!=sbbs->text_sav[i] && sbbs->text[i]!=nulstr)
		free(sbbs->text[i]);

	if((js_str=JS_ValueToString(cx, argv[1]))==NULL)
		return(JS_TRUE);

	if((p=JS_GetStringBytes(js_str))==NULL)
		return(JS_TRUE);

	len=strlen(p);
	if(!len) {
		sbbs->text[i]=nulstr;
		*rval = JSVAL_TRUE;
	} else {
		sbbs->text[i]=(char *)malloc(len+1);
		if(sbbs->text[i]==NULL) {
			sbbs->text[i]=sbbs->text_sav[i];
		} else {
			strcpy(sbbs->text[i],p);
			*rval = JSVAL_TRUE;
		}
	}

	return(JS_TRUE);
}

static JSBool
js_revert_text(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		i=0;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JS_ValueToInt32(cx,argv[0],&i);
	i--;

	if(i<0 || i>=TOTAL_TEXT) {
		for(i=0;i<TOTAL_TEXT;i++) {
			if(sbbs->text[i]!=sbbs->text_sav[i] && sbbs->text[i]!=nulstr)
				free(sbbs->text[i]);
			sbbs->text[i]=sbbs->text_sav[i]; 
		}
	} else {
		if(sbbs->text[i]!=sbbs->text_sav[i] && sbbs->text[i]!=nulstr)
			free(sbbs->text[i]);
		sbbs->text[i]=sbbs->text_sav[i];
	}

	*rval = JSVAL_TRUE;

	return(JS_TRUE);
}

static JSBool
js_load_text(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			i;
	char		path[MAX_PATH+1];
	FILE*		stream;
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	for(i=0;i<TOTAL_TEXT;i++) {
		if(sbbs->text[i]!=sbbs->text_sav[i]) {
			if(sbbs->text[i]!=nulstr)
				free(sbbs->text[i]);
			sbbs->text[i]=sbbs->text_sav[i]; 
		}
	}
	sprintf(path,"%s%s.dat"
		,sbbs->cfg.ctrl_dir,JS_GetStringBytes(js_str));

	if((stream=fnopen(NULL,path,O_RDONLY))==NULL) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}
	for(i=0;i<TOTAL_TEXT && !feof(stream);i++) {
		if((sbbs->text[i]=readtext((long *)NULL,stream))==NULL) {
			i--;
			continue; 
		}
		if(!strcmp(sbbs->text[i],sbbs->text_sav[i])) {	/* If identical */
			free(sbbs->text[i]);					/* Don't alloc */
			sbbs->text[i]=sbbs->text_sav[i]; 
		}
		else if(sbbs->text[i][0]==0) {
			free(sbbs->text[i]);
			sbbs->text[i]=nulstr; 
		} 
	}
	if(i<TOTAL_TEXT) 
		*rval = JSVAL_FALSE;
	else
		*rval = JSVAL_TRUE;

	fclose(stream);

	return(JS_TRUE);
}

static JSBool
js_atcode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		str[128];
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	char* p = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

	p=sbbs->atcode(p,str);

	if(p==NULL)
		*rval = JSVAL_NULL;
	else {
		JSString* js_str = JS_NewStringCopyZ(cx, p);
		if(js_str==NULL)
			return(JS_FALSE);
		*rval = STRING_TO_JSVAL(js_str);
	}

	return(JS_TRUE);
}


static JSBool
js_logkey(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSBool		comma=false;
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	if(argc>1)
		comma=JS_ValueToBoolean(cx,argv[1],&comma);

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	sbbs->logch(*p
		,comma ? true:false	// This is a dumb bool conversion to make BC++ happy
		);

	*rval = JSVAL_TRUE;
	return(JS_TRUE);
}

static JSBool
js_logstr(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	sbbs->log(p);

	*rval = JSVAL_TRUE;
	return(JS_TRUE);
}

static JSBool
js_finduser(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = INT_TO_JSVAL(0);
		return(JS_TRUE);
	}

	if((p=JS_GetStringBytes(js_str))==NULL) {
		*rval = INT_TO_JSVAL(0);
		return(JS_TRUE);
	}

	*rval = INT_TO_JSVAL(sbbs->finduser(p));
	return(JS_TRUE);
}

static JSBool
js_trashcan(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	char*		can;
	JSString*	js_str;
	JSString*	js_can;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_can=JS_ValueToString(cx, argv[0]))==NULL) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	if((js_str=JS_ValueToString(cx, argv[1]))==NULL) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	if((can=JS_GetStringBytes(js_can))==NULL) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	if((str=JS_GetStringBytes(js_str))==NULL) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(sbbs->trashcan(str,can));	// user args are reversed
	return(JS_TRUE);
}

static JSBool
js_newuser(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->newuser());
	return(JS_TRUE);
}

static JSBool
js_logon(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->logon());
	return(JS_TRUE);
}

static JSBool
js_login(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		name;
	char*		pw;
	JSString*	js_name;
	JSString*	js_pw;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_name=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((js_pw=JS_ValueToString(cx, argv[1]))==NULL) 
		return(JS_FALSE);

	if((name=JS_GetStringBytes(js_name))==NULL) 
		return(JS_FALSE);

	if((pw=JS_GetStringBytes(js_pw))==NULL) 
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->login(name,pw)==LOGIC_TRUE ? JS_TRUE:JS_FALSE);
	return(JS_TRUE);
}


static JSBool
js_logoff(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(!sbbs->noyes(sbbs->text[LogOffQ])) {
		if(sbbs->cfg.logoff_mod[0])
			sbbs->exec_bin(sbbs->cfg.logoff_mod,&sbbs->main_csi);
		sbbs->user_event(EVENT_LOGOFF);
		sbbs->menu("logoff");
		sbbs->riosync(0);
		sbbs->hangup(); 
	}

	return(JS_TRUE);
}

static JSBool
js_logout(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->logout();

	return(JS_TRUE);
}

static JSBool
js_automsg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->automsg();

	return(JS_TRUE);
}

static JSBool
js_time_bank(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->time_bank();

	return(JS_TRUE);
}

static JSBool
js_text_sec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->text_sec();

	return(JS_TRUE);
}

static JSBool
js_qwk_sec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->qwk_sec();

	return(JS_TRUE);
}

static JSBool
js_xtrn_sec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->xtrn_sec();

	return(JS_TRUE);
}

static JSBool
js_xfer_policy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->xfer_policy();

	return(JS_TRUE);
}

static JSBool
js_batchmenu(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->batchmenu();

	return(JS_TRUE);
}

static JSBool
js_batchdownload(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->start_batch_download());
	return(JS_TRUE);
}

static JSBool
js_batchaddlist(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->batch_add_list(JS_GetStringBytes(JS_ValueToString(cx, argv[0])));

	return(JS_TRUE);
}

static JSBool
js_temp_xfer(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->temp_xfer();

	return(JS_TRUE);
}

static JSBool
js_user_config(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->maindflts(&sbbs->useron);
	if(!(sbbs->useron.rest&FLAG('G')))    /* not guest */
		getuserdat(&sbbs->cfg,&sbbs->useron);

	return(JS_TRUE);
}

static JSBool
js_user_sync(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	getuserdat(&sbbs->cfg,&sbbs->useron);

	return(JS_TRUE);
}

static JSBool
js_sys_info(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->sys_info();

	return(JS_TRUE);
}

static JSBool
js_sub_info(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	uint subnum=get_subnum(cx,sbbs,argv[0]);

	if(subnum<sbbs->cfg.total_subs)
		sbbs->subinfo(subnum);

	return(JS_TRUE);
}

static JSBool
js_dir_info(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	uint dirnum=get_dirnum(cx,sbbs,argv[0]);
	if(dirnum<sbbs->cfg.total_dirs)
		sbbs->dirinfo(dirnum);

	return(JS_TRUE);
}

static JSBool
js_user_info(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->user_info();

	return(JS_TRUE);
}

static JSBool
js_ver(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->ver();

	return(JS_TRUE);
}

static JSBool
js_sys_stats(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->sys_stats();

	return(JS_TRUE);
}

static JSBool
js_node_stats(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		node_num=0;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc>0)
		JS_ValueToInt32(cx,argv[0],&node_num);

	sbbs->node_stats(node_num);

	return(JS_TRUE);
}

static JSBool
js_userlist(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		mode=UL_ALL;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc>0)
		JS_ValueToInt32(cx,argv[0],&mode);

	sbbs->userlist(mode);

	return(JS_TRUE);
}

static JSBool
js_useredit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		usernumber=0;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc>0)
		JS_ValueToInt32(cx,argv[0],&usernumber);

	sbbs->useredit(usernumber);

	return(JS_TRUE);
}

static JSBool
js_change_user(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->change_user();

	return(JS_TRUE);
}

static JSBool
js_logonlist(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->logonlist();

	return(JS_TRUE);
}

static JSBool
js_nodelist(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->nodelist();

	return(JS_TRUE);
}

static JSBool
js_whos_online(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->whos_online(true);

	return(JS_TRUE);
}

static JSBool
js_spy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		node_num=1;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JS_ValueToInt32(cx,argv[0],&node_num);
	sbbs->spy(node_num);

	return(JS_TRUE);
}

static JSBool
js_readmail(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		readwhich=MAIL_YOUR;
	int32		usernumber;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	usernumber=sbbs->useron.number;
	if(argc>0)
		JS_ValueToInt32(cx,argv[0],&readwhich);
	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&usernumber);

	sbbs->readmail(usernumber,readwhich);

	return(JS_TRUE);
}

static JSBool
js_email(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int32		usernumber=1;
	long		mode=WM_EMAIL;
	char*		top="";
	char*		subj="";
	JSString*	js_top=NULL;
	JSString*	js_subj=NULL;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JS_ValueToInt32(cx,argv[0],&usernumber);
	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i]))
			JS_ValueToInt32(cx,argv[i],(int32*)&mode);
		else if(JSVAL_IS_STRING(argv[i]) && js_top==NULL)
			js_top=JS_ValueToString(cx,argv[i]);
		else if(JSVAL_IS_STRING(argv[i]))
			js_subj=JS_ValueToString(cx,argv[i]);
	}

	if(js_top!=NULL)
		top=JS_GetStringBytes(js_top);
	if(js_subj!=NULL)
		subj=JS_GetStringBytes(js_subj);

	*rval = BOOLEAN_TO_JSVAL(sbbs->email(usernumber,top,subj,mode));
	return(JS_TRUE);
}
static JSBool
js_netmail(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=0;
	char*		subj="";
	JSString*	js_to;
	JSString*	js_subj=NULL;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_to=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i]))
			JS_ValueToInt32(cx,argv[i],(int32*)&mode);
		else if(JSVAL_IS_STRING(argv[i]))
			js_subj=JS_ValueToString(cx,argv[i]);
	}

	if(js_subj!=NULL)
		subj=JS_GetStringBytes(js_subj);

	*rval = BOOLEAN_TO_JSVAL(sbbs->netmail(JS_GetStringBytes(js_to),subj,mode));
	return(JS_TRUE);
}

static JSBool
js_bulkmail(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uchar*		ar=(uchar*)"";
	JSString*	js_ars=NULL;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc) {
		if((js_ars=JS_ValueToString(cx, argv[0]))==NULL)
			return(JS_FALSE);

		ar=arstr(NULL,JS_GetStringBytes(js_ars), &sbbs->cfg);
	}
	sbbs->bulkmail(ar);
	if(ar && ar[0])
		free(ar);

	return(JS_TRUE);
}

static JSBool
js_upload_file(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uint		dirnum=0;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	dirnum=get_dirnum(cx,sbbs,argv[0]);

	if(dirnum>=sbbs->cfg.total_dirs) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(sbbs->upload(dirnum));
	return(JS_TRUE);
}


static JSBool
js_bulkupload(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uint		dirnum=0;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	dirnum=get_dirnum(cx,sbbs,argv[0]);

	if(dirnum>=sbbs->cfg.total_dirs) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	*rval = BOOLEAN_TO_JSVAL(sbbs->bulkupload(dirnum)==0);
	return(JS_TRUE);
}

static JSBool
js_resort_dir(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uint		dirnum=0;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	dirnum=get_dirnum(cx,sbbs,argv[0]);

	if(dirnum>=sbbs->cfg.total_dirs) {
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	sbbs->resort(dirnum);

	*rval = JSVAL_TRUE;
	return(JS_TRUE);
}

static JSBool
js_telnet_gate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		addr;
	int32		mode=0;
	JSString*	js_addr;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((js_addr=JS_ValueToString(cx, argv[0]))==NULL) 
		return(JS_FALSE);

	if((addr=JS_GetStringBytes(js_addr))==NULL) 
		return(JS_FALSE);

	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&mode);

	sbbs->telnet_gate(addr,mode);
	
	return(JS_TRUE);
}

static JSBool
js_pagesysop(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->sysop_page());
	return(JS_TRUE);
}

static JSBool
js_pageguru(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->guru_page());
	return(JS_TRUE);
}

static JSBool
js_multinode_chat(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
	int32		channel=1;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc>1)
		JS_ValueToInt32(cx,argv[1],&channel);

	sbbs->multinodechat(channel);

	return(JS_TRUE);
}

static JSBool
js_private_message(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->nodemsg();

	return(JS_TRUE);
}

static JSBool
js_private_chat(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->privchat();

	return(JS_TRUE);
}

static JSBool
js_get_node_message(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->getnmsg();

	return(JS_TRUE);
}

static JSBool
js_put_node_message(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
	int32		node=0;
	JSString*	js_msg;
	char*		msg;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JS_ValueToInt32(cx,argv[0],&node);
	if(node<1)
		node=1;

	if((js_msg=JS_ValueToString(cx, argv[1]))==NULL) 
		return(JS_FALSE);

	if((msg=JS_GetStringBytes(js_msg))==NULL) 
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(putnmsg(&sbbs->cfg,node,msg)==0);

	return(JS_TRUE);
}

static JSBool
js_get_telegram(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
	int32		usernumber;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	usernumber=sbbs->useron.number;
	if(argc)
		JS_ValueToInt32(cx,argv[0],&usernumber);

	sbbs->getsmsg(usernumber);

	return(JS_TRUE);
}

static JSBool
js_put_telegram(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;
	int32		usernumber=0;
	JSString*	js_msg;
	char*		msg;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JS_ValueToInt32(cx,argv[0],&usernumber);
	if(usernumber<1)
		usernumber=1;

	if((js_msg=JS_ValueToString(cx, argv[1]))==NULL) 
		return(JS_FALSE);

	if((msg=JS_GetStringBytes(js_msg))==NULL) 
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(putsmsg(&sbbs->cfg,usernumber,msg)==0);

	return(JS_TRUE);
}

static JSBool
js_cmdstr(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char*		fpath="";
	char*		fspec="";
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

 	js_str = JS_ValueToString(cx, argv[0]);
 	if (!js_str)
 		return(JS_FALSE);

	p=JS_GetStringBytes(js_str);

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_STRING(argv[i])) {
			js_str = JS_ValueToString(cx, argv[i]);
			if(fpath==NULL)
				fpath=JS_GetStringBytes(js_str);
			else
				fspec=JS_GetStringBytes(js_str);
		}
	}

	p=sbbs->cmdstr(p,fpath,fspec,NULL);

	if((js_str=JS_NewStringCopyZ(cx, p))==NULL)
		return(JS_FALSE);
	*rval = STRING_TO_JSVAL(js_str);
	return(JS_TRUE);
}

static JSBool
js_getfilespec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	char		tmp[128];
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	p=sbbs->getfilespec(tmp);

	if(p==NULL)
		*rval=JSVAL_NULL;
	else {
		JSString* js_str = JS_NewStringCopyZ(cx, p);
		if(js_str==NULL)
			return(JS_FALSE);
		*rval = STRING_TO_JSVAL(js_str);
	}
	return(JS_TRUE);
}

static JSBool
js_listfiles(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=0;
	char*		fspec=ALLFILES;
	uint		dirnum;
    JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	dirnum=get_dirnum(cx,sbbs,argv[0]);

	if(dirnum>=sbbs->cfg.total_dirs) {
		*rval = INT_TO_JSVAL(0);
		return(JS_TRUE);
	}

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i]))
			JS_ValueToInt32(cx,argv[i],(int32*)&mode);
		else if(JSVAL_IS_STRING(argv[i])) {
			js_str = JS_ValueToString(cx, argv[i]);
			fspec=JS_GetStringBytes(js_str);
		}
	}

	*rval = INT_TO_JSVAL(sbbs->listfiles(dirnum,fspec,0 /* tofile */,mode));
	return(JS_TRUE);
}


static JSBool
js_listfileinfo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=FI_INFO;
	char*		fspec=ALLFILES;
	uint		dirnum;
    JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	dirnum=get_dirnum(cx,sbbs,argv[0]);

	if(dirnum>=sbbs->cfg.total_dirs) {
		*rval = INT_TO_JSVAL(0);
		return(JS_TRUE);
	}

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i]))
			JS_ValueToInt32(cx,argv[i],(int32*)&mode);
		else if(JSVAL_IS_STRING(argv[i])) {
			js_str = JS_ValueToString(cx, argv[i]);
			fspec=JS_GetStringBytes(js_str);
		}
	}

	*rval = INT_TO_JSVAL(sbbs->listfileinfo(dirnum,fspec,mode));
	return(JS_TRUE);
}

static JSBool
js_postmsg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=0;
	uint		subnum;
	uintN		n;
	JSObject*	hdrobj;
	sbbs_t*		sbbs;
	smbmsg_t*	remsg=NULL;
	smbmsg_t	msg;

	*rval = JSVAL_FALSE;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	subnum=get_subnum(cx,sbbs,argv[0]);

	if(subnum>=sbbs->cfg.total_subs) 	// invalid sub-board
		return(JS_TRUE);

	ZERO_VAR(msg);

	for(n=1; n<argc; n++) {
		if(JSVAL_IS_NUMBER(argv[n]))
			JS_ValueToInt32(cx,argv[n],(int32*)&mode);
		else if(JSVAL_IS_OBJECT(argv[n])) {
			if((hdrobj=JSVAL_TO_OBJECT(argv[n]))==NULL)
				return(JS_TRUE);
			remsg=&msg;
			if(!js_ParseMsgHeaderObject(cx,hdrobj,remsg))
				return(JS_TRUE);
		}
	}

	*rval = BOOLEAN_TO_JSVAL(sbbs->postmsg(subnum,remsg,mode));
	smb_freemsgmem(&msg);

	return(JS_TRUE);
}

static JSBool
js_msgscan_cfg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=SUB_CFG_NSCAN;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc && JSVAL_IS_NUMBER(argv[0]))
		JS_ValueToInt32(cx,argv[0],(int32*)&mode);

	sbbs->new_scan_cfg(mode);

	return(JS_TRUE);
}


static JSBool
js_msgscan_ptrs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sbbs->new_scan_ptr_cfg();

	return(JS_TRUE);
}

static JSBool
js_msgscan_reinit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	for(uint i=0;i<sbbs->cfg.total_subs;i++) {
		sbbs->subscan[i].ptr=sbbs->subscan[i].sav_ptr;
		sbbs->subscan[i].last=sbbs->subscan[i].sav_last; 
	}
	sbbs->bputs(sbbs->text[MsgPtrsInitialized]);

	return(JS_TRUE);
}

static JSBool
js_scansubs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=SCAN_NEW;
	BOOL		all=FALSE;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	for(uintN i=0;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i]))
			JS_ValueToInt32(cx,argv[i],(int32*)&mode);
		else if(JSVAL_IS_BOOLEAN(argv[i]))
			all=JSVAL_TO_BOOLEAN(argv[i]);
	}

	if(all)
		sbbs->scanallsubs(mode);
	else
		sbbs->scansubs(mode);

	return(JS_TRUE);
}

static JSBool
js_scandirs(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		mode=0;
	BOOL		all=FALSE;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	for(uintN i=0;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i]))
			JS_ValueToInt32(cx,argv[i],(int32*)&mode);
		else if(JSVAL_IS_BOOLEAN(argv[i]))
			all=JSVAL_TO_BOOLEAN(argv[i]);
	}

	if(all)
		sbbs->scanalldirs(mode);
	else
		sbbs->scandirs(mode);

	return(JS_TRUE);
}

static JSBool
js_scanposts(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		find="";
	long		mode=0;
	uint		subnum;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	subnum=get_subnum(cx,sbbs,argv[0]);

	if(subnum>=sbbs->cfg.total_subs) {	// invalid sub-board
		*rval = JSVAL_FALSE;
		return(JS_TRUE);
	}

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i]))
			JS_ValueToInt32(cx,argv[i],(int32*)&mode);
		else if(JSVAL_IS_STRING(argv[i]))
			find=JS_GetStringBytes(JS_ValueToString(cx,argv[i]));
	}

	*rval = BOOLEAN_TO_JSVAL(sbbs->scanposts(subnum,mode,find)==0);
	return(JS_TRUE);
}

static JSBool
js_getnstime(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	time_t		t=time(NULL);
	sbbs_t*		sbbs;

	*rval = JSVAL_NULL;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToInt32(cx,argv[0],(int32*)&t);

	if(sbbs->inputnstime(&t)==true)
		JS_NewNumberValue(cx,t,rval);

	return(JS_TRUE);
}

static JSBool
js_select_shell(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->select_shell());
	return(JS_TRUE);
}

static JSBool
js_select_editor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->select_editor());
	return(JS_TRUE);
}

static jsSyncMethodSpec js_bbs_functions[] = {
	{"atcode",			js_atcode,			1,	JSTYPE_STRING,	JSDOCSTR("string code")
	,JSDOCSTR("returns @-code value, specified <i>code</i> string does not include @ character delimiters")
	,310
	},
	/* text.dat */
	{"text",			js_text,			1,	JSTYPE_STRING,	JSDOCSTR("number line")
	,JSDOCSTR("returns specified text string from text.dat")
	,310
	},
	{"replace_text",	js_replace_text,	2,	JSTYPE_BOOLEAN,	JSDOCSTR("number line, string text")
	,JSDOCSTR("replaces specified text string in memory")
	,310
	},
	{"revert_text",		js_revert_text,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("[number line]")
	,JSDOCSTR("reverts specified text string to original text string; "
		"if <i>line</i> unspecified, reverts all text lines")
	,310
	},
	{"load_text",		js_load_text,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("string basefilename")
	,JSDOCSTR("load an alternate text.dat from ctrl directory, automatically appends '.dat' to basefilename")
	,310
	},
	/* procedures */
	{"newuser",			js_newuser,			0,	JSTYPE_VOID,	""
	,JSDOCSTR("interactive new user procedure")
	,310
	},
	{"login",			js_login,			2,	JSTYPE_BOOLEAN,	JSDOCSTR("string username, password_prompt")
	,JSDOCSTR("login with <i>username</i>, displaying <i>password_prompt</i> for password (if required)")
	,310
	},
	{"logon",			js_logon,			0,	JSTYPE_BOOLEAN,	""
	,JSDOCSTR("interactive logon procedure")
	,310
	},
	{"logoff",			js_logoff,			0,	JSTYPE_VOID,	""
	,JSDOCSTR("interactive logoff procedure")
	,310
	},
	{"logout",			js_logout,			0,	JSTYPE_VOID,	""
	,JSDOCSTR("non-interactive logout procedure")
	,310
	},
	{"hangup",			js_hangup,			0,	JSTYPE_VOID,	""
	,JSDOCSTR("hangup (disconnect) immediately")
	,310
	},
	{"node_sync",		js_nodesync,		0,	JSTYPE_ALIAS },
	{"nodesync",		js_nodesync,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("synchronize with node database, checks for messages, interruption, etc. (AKA node_sync)")
	,310
	},
	{"auto_msg",		js_automsg,			0,	JSTYPE_VOID,	""
	,JSDOCSTR("read/create system's auto-message")
	,310
	},		
	{"time_bank",		js_time_bank,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("enter the time banking system")
	,310
	},		
	{"qwk_sec",			js_qwk_sec,			0,	JSTYPE_VOID,	""
	,JSDOCSTR("enter the QWK message packet upload/download/config section")
	,310
	},		
	{"text_sec",		js_text_sec,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("enter the text files section")
	,310
	},		
	{"xtrn_sec",		js_xtrn_sec,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("enter the external programs section")
	,310
	},		
	{"xfer_policy",		js_xfer_policy,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("display the file transfer policy")
	,310
	},		
	{"batch_menu",		js_batchmenu,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("enter the batch file transfer menu")
	,310
	},		
	{"batch_download",	js_batchdownload,	0,	JSTYPE_BOOLEAN,	""
	,JSDOCSTR("start a batch download")
	,310
	},		
	{"batch_add_list",	js_batchaddlist,	1,	JSTYPE_VOID,	JSDOCSTR("filename")
	,JSDOCSTR("add file list to batch download queue")
	,310
	},		
	{"temp_xfer",		js_temp_xfer,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("enter the temporary file tranfer menu")
	,310
	},		
	{"user_sync",		js_user_sync,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("read the current user data from the database")
	,310
	},		
	{"user_config",		js_user_config,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("enter the user settings configuration menu")
	,310
	},		
	{"sys_info",		js_sys_info,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("display system information")
	,310
	},		
	{"sub_info",		js_sub_info,		1,	JSTYPE_VOID,	JSDOCSTR("[subboard]")
	,JSDOCSTR("display message sub-board information (current <i>subboard</i>, if unspecified)")
	,310
	},		
	{"dir_info",		js_dir_info,		0,	JSTYPE_VOID,	JSDOCSTR("[directory]")
	,JSDOCSTR("display file directory information (current <i>directory</i>, if unspecified)")
	,310
	},		
	{"user_info",		js_user_info,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("display current user information")
	,310
	},		
	{"ver",				js_ver,				0,	JSTYPE_VOID,	""
	,JSDOCSTR("display software version information")
	,310
	},		
	{"sys_stats",		js_sys_stats,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("display system statistics")
	,310
	},		
	{"node_stats",		js_node_stats,		0,	JSTYPE_VOID,	JSDOCSTR("[node_number]")
	,JSDOCSTR("display current (or specified) node statistics")
	,310
	},		
	{"list_users",		js_userlist,		0,	JSTYPE_VOID,	JSDOCSTR("[mode]")
	,JSDOCSTR("display user list"
	"(see <tt>UL_*</tt> in <tt>sbbsdefs.js</tt> for valid <i>mode</i> values)")
	,310
	},		
	{"edit_user",		js_useredit,		0,	JSTYPE_VOID,	JSDOCSTR("[user_number]")
	,JSDOCSTR("enter the user editor")
	,310
	},		
	{"change_user",		js_change_user,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("change to a different user")
	,310
	},		
	{"list_logons",		js_logonlist,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("display the logon list")
	,310
	},		
	{"read_mail",		js_readmail,		0,	JSTYPE_VOID,	JSDOCSTR("[which [,user_number]]")
	,JSDOCSTR("read private e-mail"
	"(see <tt>MAIL_*</tt> in <tt>sbbsdefs.js</tt> for valid <i>which</i> values)")
	,310
	},		
	{"email",			js_email,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("number user [,number mode] [,string top] [,string subject]")
	,JSDOCSTR("send private e-mail or netmail")
	,310
	},		
	{"netmail",			js_netmail,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string address [,number mode] [,string subject]")
	,JSDOCSTR("send private netmail")
	,310
	},		
	{"bulk_mail",		js_bulkmail,		0,	JSTYPE_VOID,	JSDOCSTR("[ars]")
	,JSDOCSTR("send bulk private e-mail")
	,310
	},		
	{"upload_file",		js_upload_file,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("directory")
	,JSDOCSTR("upload file to file directory specified by number or internal code")
	,310
	},		
	{"bulk_upload",		js_bulkupload,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("directory")
	,JSDOCSTR("add files (already in local storage path) to file directory "
		"specified by number or internal code")
	,310
	},		
	{"resort_dir",		js_resort_dir,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("directory")
	,JSDOCSTR("re-sort the file directory specified by number or internal code)")
	,310
	},		
	{"list_files",		js_listfiles,		1,	JSTYPE_NUMBER,	JSDOCSTR("directory [,string filespec] [,number mode]")
	,JSDOCSTR("list files in the specified file directory, "
		"optionally specifying a file specification (wildcards) and <i>mode</i> (bitfield)")
	,310
	},		
	{"list_file_info",	js_listfileinfo,	1,	JSTYPE_NUMBER,	JSDOCSTR("directory [,string filespec] [,number mode]")
	,JSDOCSTR("list extended file information for files in the specified file directory")
	,310
	},		
	{"post_msg",		js_postmsg,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("sub-board [,number mode] [,object reply_header]")
	,JSDOCSTR("post a message in the specified message sub-board (number or internal code) "
		"with optinal <i>mode</i> (bitfield)<br>"
		"If <i>reply_header</i> is specified (a header object returned from <i>MsgBase.get_msg_header()</i>), that header "
		"will be used for the in-reply-to header fields (this argument added in v3.13)")
	,313
	},		
	{"cfg_msg_scan",	js_msgscan_cfg,		0,	JSTYPE_VOID,	JSDOCSTR("[number type]")
	,JSDOCSTR("configure message scan "
		"(<i>type</i> is either <tt>SCAN_CFG_NEW</tt> or <tt>SCAN_CFG_TOYOU</tt>)")
	,310
	},		
	{"cfg_msg_ptrs",	js_msgscan_ptrs,	0,	JSTYPE_VOID,	""
	,JSDOCSTR("change message scan pointer values")
	,310
	},		
	{"reinit_msg_ptrs",	js_msgscan_reinit,	0,	JSTYPE_VOID,	""
	,JSDOCSTR("re-initialize new message scan pointers")
	,310
	},		
	{"scan_subs",		js_scansubs,		0,	JSTYPE_VOID,	JSDOCSTR("[number mode, boolean all]")
	,JSDOCSTR("scan sub-boards for messages")
	,310
	},		
	{"scan_dirs",		js_scandirs,		0,	JSTYPE_VOID,	JSDOCSTR("[number mode, boolean all]")
	,JSDOCSTR("scan directories for files")
	,310
	},		
	{"scan_posts",		js_scanposts,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("[sub-board, number mode, string find]")
	,JSDOCSTR("scan posts in the specified message sub-board (number or internal code), "
		"optionally search for 'find' string")
	,310
	},		
	/* menuing */
	{"menu",			js_menu,			1,	JSTYPE_VOID,	JSDOCSTR("base_filename")
	,JSDOCSTR("display a menu file from the text/menu directory")
	,310
	},		
	{"log_key",			js_logkey,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("key [,boolean comma]")
	,JSDOCSTR("log key to node.log (comma optional)")
	,310
	},		
	{"log_str",			js_logstr,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("text")
	,JSDOCSTR("log string to node.log")
	,310
	},		
	/* users */
	{"finduser",		js_finduser,		1,	JSTYPE_NUMBER,	JSDOCSTR("username_or_number")
	,JSDOCSTR("find user name (partial name support), interactive")
	,310
	},		
	{"trashcan",		js_trashcan,		2,	JSTYPE_BOOLEAN,	JSDOCSTR("base_filename, search_string")
	,JSDOCSTR("search file for psuedo-regexp (search string) in trashcan file (text/base_filename.can)")
	,310
	},		
	/* xtrn programs/modules */
	{"exec",			js_exec,			2,	JSTYPE_NUMBER,	JSDOCSTR("cmdline [,number mode] [,string startup_dir]")
	,JSDOCSTR("execute a program, optionally changing current directory to <i>startup_dir</i> "
	"(see <tt>EX_*</tt> in <tt>sbbsdefs.js</tt> for valid <i>mode</i> bits)")
	,310
	},		
	{"exec_xtrn",		js_exec_xtrn,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("xtrn_number_or_code")
	,JSDOCSTR("execute external program by internal code")
	,310
	},		
	{"user_event",		js_user_event,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("number event_type")
	,JSDOCSTR("execute user event by event type "
	"(see <tt>EVENT_*</tt> in <tt>sbbsdefs.js</tt> for valid values)")
	,310
	},		
	{"telnet_gate",		js_telnet_gate,		1,	JSTYPE_VOID,	JSDOCSTR("string address [,number mode]")
	,JSDOCSTR("external telnet gateway (see <tt>TG_*</tt> in <tt>sbbsdefs.js</tt> for valid <i>mode</i> bits)")
	,310
	},		
	/* security */
	{"check_syspass",	js_chksyspass,		0,	JSTYPE_BOOLEAN,	""
	,JSDOCSTR("prompt for and verify system password")
	,310
	},
	{"good_password",	js_chkpass,			1,	JSTYPE_STRING,	JSDOCSTR("string password")
	,JSDOCSTR("check if requested user password meets minimum password requirements "
		"(length, uniqueness, etc.)")
	,310
	},
	/* chat/node stuff */
	{"page_sysop",		js_pagesysop,		0,	JSTYPE_BOOLEAN,	""
	,JSDOCSTR("page the sysop for chat")
	,310
	},		
	{"page_guru",		js_pageguru,		0,	JSTYPE_BOOLEAN,	""
	,JSDOCSTR("page the guru for chat")
	,310
	},		
	{"multinode_chat",	js_multinode_chat,	0,	JSTYPE_VOID,	""
	,JSDOCSTR("enter multi-node chat")
	,310
	},		
	{"private_message",	js_private_message,	0,	JSTYPE_VOID,	""
	,JSDOCSTR("use the private inter-node message prompt")
	,310
	},		
	{"private_chat",	js_private_chat,	0,	JSTYPE_VOID,	""
	,JSDOCSTR("enter private inter-node chat")
	,310
	},		
	{"get_node_message",js_get_node_message,0,	JSTYPE_VOID,	""
	,JSDOCSTR("receive and display an inter-node message")
	,310
	},		
	{"put_node_message",js_put_node_message,2,	JSTYPE_BOOLEAN,	JSDOCSTR("number node, string text")
	,JSDOCSTR("send an inter-node message")
	,310
	},		
	{"get_telegram",	js_get_telegram,	1,	JSTYPE_VOID,	JSDOCSTR("[number usernum]")
	,JSDOCSTR("receive and display a telegram")
	,310
	},		
	{"put_telegram",	js_put_telegram,	2,	JSTYPE_BOOLEAN,	JSDOCSTR("number user, string text")
	,JSDOCSTR("send a telegram to a user")
	,310
	},		
	{"list_nodes",		js_nodelist,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("list all nodes")
	,310
	},		
	{"whos_online",		js_whos_online,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("list active nodes only (who's online)")
	,310
	},		
	{"spy",				js_spy,				1,	JSTYPE_VOID,	JSDOCSTR("node_number")
	,JSDOCSTR("spy on a node")
	,310
	},		
	/* misc */
	{"cmdstr",			js_cmdstr,			1,	JSTYPE_STRING,	JSDOCSTR("string str [,string fpath] [,string fspec]")
	,JSDOCSTR("return expanded command string using Synchronet command-line specifiers")
	,310
	},		
	/* input */
	{"get_filespec",	js_getfilespec,		0,	JSTYPE_STRING,	""	
	,JSDOCSTR("returns a file specification input by the user (optionally with wildcards)")
	,310
	},		
	{"get_newscantime",	js_getnstime,		1,	JSTYPE_NUMBER,	JSDOCSTR("number time")
	,JSDOCSTR("confirm or change newscan time, returns new newscan time value (time_t format)")
	,310
	},		
	{"select_shell",	js_select_shell,	0,	JSTYPE_BOOLEAN,	""
	,JSDOCSTR("prompt user to select a new command shell")
	,310
	},
	{"select_editor",	js_select_editor,	0,	JSTYPE_BOOLEAN,	""
	,JSDOCSTR("prompt user to select a new external message editor")
	,310
	},
	{0}
};


static JSClass js_bbs_class = {
     "BBS"					/* name			*/
    ,0						/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_bbs_get				/* getProperty	*/
	,js_bbs_set				/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

JSObject* js_CreateBbsObject(JSContext* cx, JSObject* parent)
{
	JSObject* obj;
	JSObject* mods;

	obj = JS_DefineObject(cx, parent, "bbs", &js_bbs_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY);

	if(obj==NULL)
		return(NULL);

	if(!js_DefineSyncProperties(cx, obj, js_bbs_properties))
		return(NULL);

	if (!js_DefineSyncMethods(cx, obj, js_bbs_functions, FALSE)) 
		return(NULL);

	if((mods=JS_DefineObject(cx, obj, "mods", NULL, NULL ,JSPROP_ENUMERATE))==NULL)
		return(NULL);

#ifdef _DEBUG
	js_DescribeSyncObject(cx,mods,"Global repository for 3rd party modifications",312);
	js_DescribeSyncObject(cx,obj,"Controls the Telnet/RLogin BBS experience",310);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", bbs_prop_desc, JSPROP_READONLY);
#endif

	return(obj);
}

#endif	/* JAVSCRIPT */
