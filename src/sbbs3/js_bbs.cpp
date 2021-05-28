/* Synchronet JavaScript "bbs" Object */

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
#include "js_request.h"

#ifdef JAVASCRIPT

/*****************************/
/* BBS Object Properites */
/*****************************/
enum {
	 BBS_PROP_SYS_STATUS
	,BBS_PROP_STARTUP_OPT
	,BBS_PROP_ANSWER_TIME
	,BBS_PROP_LOGON_TIME
	,BBS_PROP_START_TIME
	,BBS_PROP_NS_TIME
	,BBS_PROP_LAST_NS_TIME
	,BBS_PROP_ONLINE
	,BBS_PROP_TIMELEFT
	,BBS_PROP_EVENT_TIME
	,BBS_PROP_EVENT_CODE

	,BBS_PROP_NODE_NUM
	,BBS_PROP_NODE_SETTINGS
	,BBS_PROP_NODE_STATUS
	,BBS_PROP_NODE_ERRORS
	,BBS_PROP_NODE_ACTION
	,BBS_PROP_NODE_USERON
	,BBS_PROP_NODE_CONNECTION
	,BBS_PROP_NODE_MISC
	,BBS_PROP_NODE_AUX
	,BBS_PROP_NODE_EXTAUX
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
	,BBS_PROP_CURSUB_CODE
	,BBS_PROP_CURLIB
	,BBS_PROP_CURDIR
	,BBS_PROP_CURDIR_CODE

	,BBS_PROP_CONNECTION		/* READ ONLY */
	,BBS_PROP_RLOGIN_NAME
	,BBS_PROP_RLOGIN_PASS
	,BBS_PROP_RLOGIN_TERM
	,BBS_PROP_CLIENT_NAME

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
	,BBS_PROP_MSG_FROM_BBSID
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
	,BBS_PROP_MSG_THREAD_ID
	,BBS_PROP_MSG_THREAD_BACK
	,BBS_PROP_MSG_THREAD_NEXT
	,BBS_PROP_MSG_THREAD_FIRST
	,BBS_PROP_MSG_ID
	,BBS_PROP_MSG_REPLY_ID
	,BBS_PROP_MSG_DELIVERY_ATTEMPTS

	,BBS_PROP_MSGHDR_TOS

	/* READ ONLY */
	,BBS_PROP_BATCH_UPLOAD_TOTAL
	,BBS_PROP_BATCH_DNLOAD_TOTAL

	/* READ ONLY */
	,BBS_PROP_FILE_NAME
	,BBS_PROP_FILE_DESC
	,BBS_PROP_FILE_DIR
	,BBS_PROP_FILE_ATTR
	,BBS_PROP_FILE_DATE
	,BBS_PROP_FILE_SIZE
	,BBS_PROP_FILE_CREDITS
	,BBS_PROP_FILE_ULER
	,BBS_PROP_FILE_DATE_ULED
	,BBS_PROP_FILE_DATE_DLED
	,BBS_PROP_FILE_TIMES_DLED

	,BBS_PROP_COMMAND_STR
};

#ifdef BUILD_JSDOCS
	static const char* bbs_prop_desc[] = {
	 "system status bitfield (see <tt>SS_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)"
	,"startup options bitfield (see <tt>BBS_OPT_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)"
	,"answer time, in <i>time_t</i> format"
	,"logon time, in <i>time_t</i> format"
	,"time from which user's time left is calculated, in <i>time_t</i> format"
	,"current file new-scan time, in <i>time_t</i> format"
	,"previous file new-scan time, in <i>time_t</i> format"
	,"online (see <tt>ON_*</tt> in <tt>sbbsdefs.js</tt> for valid values)"
	,"time left (in seconds)"
	,"time of next exclusive event (in <i>time_t</i> format), or 0 if none"
	,"internal code of next exclusive event"

	,"current node number"
	,"current node settings bitfield (see <tt>NM_*</tt> in <tt>sbbsdefs.js</tt> for bit definitions)"
	,"current node status value (see <tt>nodedefs.js</tt> for valid values)"
	,"current node error counter"
	,"current node action (see <tt>nodedefs.js</tt> for valid values)"
	,"current node user number (<i>useron</i> value)"
	,"current node connection type (see <tt>nodedefs.js</tt> for valid values)"
	,"current node misc value (see <tt>nodedefs.js</tt> for valid values)"
	,"current node aux value"
	,"current node extended aux (<i>extaux</i>) value"
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
	,"current message sub-board internal code"
	,"current file library"
	,"current file directory"
	,"current file directory internal code"

	,"remote connection type"
	,"login name given during RLogin negotiation"
	,"password specified during RLogin negotiation"
	,"terminal specified during RLogin negotiation"
	,"client name"

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
	,"message recipient network address"
	,"message recipient agent type"
	,"message sender name"
	,"message sender extension"
	,"message sender network address"
	,"message sender BBS ID"
	,"message sender agent type"
	,"message reply-to name"
	,"message reply-to extension"
	,"message reply-to network address"
	,"message reply-to agent type"
	,"message subject"
	,"message date/time"
	,"message time zone"
	,"message date/time imported"
	,"message attributes"
	,"message auxiliary attributes"
	,"message network attributes"
	,"message header offset"
	,"message number (unique, monotonically incrementing)"
	,"message expiration"
	,"message forwarded"
	,"message thread identifier (0 if unknown)"
	,"message thread, back message number"
	,"message thread, next message number"
	,"message thread, message number of first reply to this message"
	,"message identifier"
	,"message replied-to identifier"
	,"message delivery attempt counter"

	,"message header displayed at top-of-screen"

	,"file name"
	,"file description"
	,"file directory (number)"
	,"file attribute flags"
	,"file date"
	,"file size (in bytes)"
	,"file credit value"
	,"file uploader (user name)"
	,"file upload date"
	,"file last-download date"
	,"file download count"

	,"number of files in batch upload queue"
	,"number of files in batch download queue"

	,"current command shell/module <i>command string</i> value"
	,NULL
	};
#endif

extern JSClass js_bbs_class; // defined later
static sbbs_t *js_GetPrivate(JSContext *cx, JSObject *obj)
{
	return (sbbs_t *)js_GetClassPrivate(cx, obj, &js_bbs_class);
}

static JSBool js_bbs_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
	char		tmp[128];
	const char*	p=NULL;
	const char*	nulstr="";
	uint32		val=0;
    jsint       tiny;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, obj))==NULL)
		return(JS_FALSE);

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case BBS_PROP_SYS_STATUS:
			val=sbbs->sys_status;
			break;
		case BBS_PROP_STARTUP_OPT:
			val=sbbs->startup->options;
			break;
		case BBS_PROP_ANSWER_TIME:
			val=(uint32)sbbs->answertime;
			break;
		case BBS_PROP_LOGON_TIME:
			val=(uint32)sbbs->logontime;
			break;
		case BBS_PROP_START_TIME:
			val=(uint32)sbbs->starttime;
			break;
		case BBS_PROP_NS_TIME:
			val=(uint32)sbbs->ns_time;
			break;
		case BBS_PROP_LAST_NS_TIME:
			val=(uint32)sbbs->last_ns_time;
			break;
		case BBS_PROP_ONLINE:
			val=sbbs->online;
			break;
		case BBS_PROP_TIMELEFT:
			rc=JS_SUSPENDREQUEST(cx);
			val=sbbs->gettimeleft(false);
			JS_RESUMEREQUEST(cx, rc);
			break;
		case BBS_PROP_EVENT_TIME:
			val=(uint32)sbbs->event_time;
			break;
		case BBS_PROP_EVENT_CODE:
			p=sbbs->event_code;
			break;

		case BBS_PROP_NODE_NUM:
			val=sbbs->cfg.node_num;
			break;
		case BBS_PROP_NODE_SETTINGS:
			val=sbbs->cfg.node_misc;
			break;
		case BBS_PROP_NODE_STATUS:
			val=sbbs->thisnode.status;
			break;
		case BBS_PROP_NODE_ERRORS:
			val=sbbs->thisnode.errors;
			break;
		case BBS_PROP_NODE_ACTION:
			val=sbbs->action;
			break;
		case BBS_PROP_NODE_USERON:
			val=sbbs->thisnode.useron;
			break;
		case BBS_PROP_NODE_CONNECTION:
			val=sbbs->thisnode.connection;
			break;
		case BBS_PROP_NODE_MISC:
			val=sbbs->thisnode.misc;
			break;
		case BBS_PROP_NODE_AUX:
			val=sbbs->thisnode.aux;
			break;
		case BBS_PROP_NODE_EXTAUX:
			val=sbbs->thisnode.extaux;
			break;

		case BBS_PROP_NODE_VAL_USER:
			val=sbbs->cfg.node_valuser;
			break;

		case BBS_PROP_LOGON_ULB:
			val=(uint32_t)sbbs->logon_ulb;	// TODO: fix for > 4GB!
			break;
		case BBS_PROP_LOGON_DLB:
			val=(uint32_t)sbbs->logon_dlb;	// TODO: fix for > 4GB!
			break;
		case BBS_PROP_LOGON_ULS:
			val=(uint32_t)sbbs->logon_uls;	// TODO: fix for > 4GB!
			break;
		case BBS_PROP_LOGON_DLS:
			val=(uint32_t)sbbs->logon_dls;	// TODO: fix for > 4GB!
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
		case BBS_PROP_CURSUB_CODE:
			if(sbbs->cursubnum<sbbs->cfg.total_subs)
				p=sbbs->cfg.sub[sbbs->cursubnum]->code;
			else
				p=nulstr;
			break;

		case BBS_PROP_CURLIB:
			val=sbbs->curlib;
			break;
		case BBS_PROP_CURDIR:
			if(sbbs->curlib<sbbs->usrlibs)
				val=sbbs->curdir[sbbs->curlib];
			break;
		case BBS_PROP_CURDIR_CODE:
			if(sbbs->curdirnum<sbbs->cfg.total_dirs)
				p=sbbs->cfg.dir[sbbs->curdirnum]->code;
			else
				p=nulstr;
			break;

		case BBS_PROP_CONNECTION:
			p=sbbs->connection;
			break;
		case BBS_PROP_RLOGIN_NAME:
			p=sbbs->rlogin_name;
			break;
		case BBS_PROP_RLOGIN_PASS:
			p=sbbs->rlogin_pass;
			break;
		case BBS_PROP_RLOGIN_TERM:
			p=sbbs->rlogin_term;
			break;
		case BBS_PROP_CLIENT_NAME:
			p=sbbs->client_name;
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
			if(sbbs->usrsubs && sbbs->smb.subnum!=INVALID_SUB && sbbs->smb.subnum<sbbs->cfg.total_subs) {
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
			if(sbbs->current_msg_to==NULL)
				p=nulstr;
			else
				p=sbbs->current_msg_to;
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
				p=smb_netaddrstr(&sbbs->current_msg->to_net,tmp);
			break;
		case BBS_PROP_MSG_TO_AGENT:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->to_agent;
			break;
		case BBS_PROP_MSG_FROM:
			if(sbbs->current_msg_from==NULL)
				p=nulstr;
			else
				p=sbbs->current_msg_from;
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
				p=smb_netaddrstr(&sbbs->current_msg->from_net,tmp);
			break;
		case BBS_PROP_MSG_FROM_BBSID:
			if(sbbs->current_msg == NULL || sbbs->current_msg->ftn_bbsid == NULL)
				p = nulstr;
			else // Should we return only the last ID of the QWKnet route here?
				p = sbbs->current_msg->ftn_bbsid;
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
				p=smb_netaddrstr(&sbbs->current_msg->replyto_net,tmp);
			break;
		case BBS_PROP_MSG_REPLYTO_AGENT:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->replyto_agent;
			break;

		case BBS_PROP_MSG_SUBJECT:
			if(sbbs->current_msg_subj==NULL)
				p=nulstr;
			else
				p=sbbs->current_msg_subj;
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
				val=sbbs->current_msg->idx_offset;
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
		case BBS_PROP_MSG_THREAD_ID:
			if(sbbs->current_msg!=NULL)
				val=sbbs->current_msg->hdr.thread_id;
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
		case BBS_PROP_MSGHDR_TOS:
			val = sbbs->msghdr_tos;
			break;

		/* Currently Displayed File (sbbs.current_file) */
		case BBS_PROP_FILE_NAME:
			if(sbbs->current_file==NULL)
				p=nulstr;
			else
				p=sbbs->current_file->name;
			break;
		case BBS_PROP_FILE_DESC:
			if(sbbs->current_file==NULL)
				p=nulstr;
			else
				p=sbbs->current_file->desc;
			break;
		case BBS_PROP_FILE_ULER:
			if(sbbs->current_file==NULL)
				p=nulstr;
			else
				p=sbbs->current_file->from;
			break;
		case BBS_PROP_FILE_DATE:
			if(sbbs->current_file==NULL)
				p=nulstr;
			else
				val=(uint32)sbbs->current_file->time;
			break;
		case BBS_PROP_FILE_DATE_ULED:
			if(sbbs->current_file==NULL)
				p=nulstr;
			else
				val=sbbs->current_file->hdr.when_imported.time;
			break;
		case BBS_PROP_FILE_DATE_DLED:
			if(sbbs->current_file==NULL)
				p=nulstr;
			else
				val=sbbs->current_file->hdr.last_downloaded;
			break;
		case BBS_PROP_FILE_TIMES_DLED:
			if(sbbs->current_file==NULL)
				p=nulstr;
			else
				val=sbbs->current_file->hdr.times_downloaded;
			break;
		case BBS_PROP_FILE_SIZE:
			if(sbbs->current_file==NULL)
				p=nulstr;
			else // TODO: fix for 64-bit file sizes
				val=(uint32)sbbs->current_file->size;
			break;
		case BBS_PROP_FILE_CREDITS:
			if(sbbs->current_file==NULL)
				p=nulstr;
			else
				val=sbbs->current_file->cost;
			break;
		case BBS_PROP_FILE_DIR:
			if(sbbs->current_file==NULL)
				p=nulstr;
			else
				val=sbbs->current_file->dir;
			break;
		case BBS_PROP_FILE_ATTR:
			if(sbbs->current_file==NULL)
				p=nulstr;
			else
				val=sbbs->current_file->hdr.attr;
			break;

		case BBS_PROP_BATCH_UPLOAD_TOTAL:
			val = sbbs->batup_total();
			break;
		case BBS_PROP_BATCH_DNLOAD_TOTAL:
			val = sbbs->batdn_total();
			break;

		case BBS_PROP_COMMAND_STR:
			p=sbbs->main_csi.str;
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
		*vp = UINT_TO_JSVAL(val);

	return(JS_TRUE);
}

static JSBool js_bbs_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
	char*		p=NULL;
	uint32		val=0;
    jsint       tiny;
	JSString*	js_str;
	sbbs_t*		sbbs;

	if((sbbs=js_GetPrivate(cx, obj))==NULL)
		return(JS_FALSE);

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	if(JSVAL_IS_NUMBER(*vp) || JSVAL_IS_BOOLEAN(*vp)) {
		if(!JS_ValueToECMAUint32(cx, *vp, &val))
			return(JS_FALSE);
	}
	else if(JSVAL_IS_STRING(*vp)) {
		if((js_str = JS_ValueToString(cx, *vp))==NULL)
			return(JS_FALSE);
		JSSTRING_TO_MSTRING(cx, js_str, p, NULL);
		HANDLE_PENDING(cx, p);
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
		case BBS_PROP_START_TIME:
			sbbs->starttime=val;
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
		case BBS_PROP_NODE_SETTINGS:
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
			if(p != NULL)
				SAFECOPY(sbbs->menu_dir,p);
			break;
		case BBS_PROP_MENU_FILE:
			if(p != NULL)
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
		case BBS_PROP_CURSUB_CODE:
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
		case BBS_PROP_CURDIR_CODE:
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
			if(p != NULL)
				SAFECOPY(sbbs->rlogin_name,p);
			break;
		case BBS_PROP_RLOGIN_PASS:
			if(p != NULL)
				SAFECOPY(sbbs->rlogin_pass,p);
			break;
		case BBS_PROP_RLOGIN_TERM:
			if(p != NULL)
				SAFECOPY(sbbs->rlogin_term,p);
			break;
		case BBS_PROP_CLIENT_NAME:
			if(p != NULL)
				SAFECOPY(sbbs->client_name,p);
			break;

		case BBS_PROP_COMMAND_STR:
			if(p != NULL)
				sprintf(sbbs->main_csi.str, "%.*s", 1024, p);
			break;

		default:
			if(p)
				free(p);
			return(JS_TRUE);
	}

	if(p)
		free(p);

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
	{	"start_time"		,BBS_PROP_START_TIME	,JSPROP_ENUMERATE	,314},
	{	"new_file_time"		,BBS_PROP_NS_TIME		,JSPROP_ENUMERATE	,310},
	{	"last_new_file_time",BBS_PROP_LAST_NS_TIME	,JSPROP_ENUMERATE	,310},
	{	"online"			,BBS_PROP_ONLINE		,JSPROP_ENUMERATE	,310},
	{	"timeleft"			,BBS_PROP_TIMELEFT		,JSPROP_READONLY	,310},	/* alias */
	{	"time_left"			,BBS_PROP_TIMELEFT		,PROP_READONLY		,311},
	{	"event_time"		,BBS_PROP_EVENT_TIME	,PROP_READONLY		,311},
	{	"event_code"		,BBS_PROP_EVENT_CODE	,PROP_READONLY		,311},
	{	"node_num"			,BBS_PROP_NODE_NUM		,PROP_READONLY		,310},
	{	"node_settings"		,BBS_PROP_NODE_SETTINGS	,JSPROP_ENUMERATE	,310},
	{	"node_status"		,BBS_PROP_NODE_STATUS	,PROP_READONLY		,31700},
	{	"node_errors"		,BBS_PROP_NODE_ERRORS	,PROP_READONLY		,31700},
	{	"node_action"		,BBS_PROP_NODE_ACTION	,JSPROP_ENUMERATE	,310},
	{	"node_useron"		,BBS_PROP_NODE_USERON	,PROP_READONLY		,31700},
	{	"node_connection"	,BBS_PROP_NODE_CONNECTION,PROP_READONLY		,31700},
	{	"node_misc"			,BBS_PROP_NODE_MISC		,PROP_READONLY		,31700},
	{	"node_aux"			,BBS_PROP_NODE_AUX		,PROP_READONLY		,31700},
	{	"node_extaux"		,BBS_PROP_NODE_EXTAUX	,PROP_READONLY		,31700},
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
	{	"cursub_code"		,BBS_PROP_CURSUB_CODE	,JSPROP_ENUMERATE	,314},
	{	"curlib"			,BBS_PROP_CURLIB		,JSPROP_ENUMERATE	,310},
	{	"curdir"			,BBS_PROP_CURDIR		,JSPROP_ENUMERATE	,310},
	{	"curdir_code"		,BBS_PROP_CURDIR_CODE	,JSPROP_ENUMERATE	,314},
	{	"connection"		,BBS_PROP_CONNECTION	,PROP_READONLY		,310},
	{	"rlogin_name"		,BBS_PROP_RLOGIN_NAME	,JSPROP_ENUMERATE	,310},
	{	"rlogin_password"	,BBS_PROP_RLOGIN_PASS	,JSPROP_ENUMERATE	,315},
	{	"rlogin_terminal"	,BBS_PROP_RLOGIN_TERM	,JSPROP_ENUMERATE	,316},
	{	"client_name"		,BBS_PROP_CLIENT_NAME	,JSPROP_ENUMERATE	,310},
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
	{	"msg_from_bbsid"	,BBS_PROP_MSG_FROM_BBSID	,PROP_READONLY	,31802},
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
	{	"msg_thread_id"		,BBS_PROP_MSG_THREAD_BACK	,PROP_READONLY	,316},
	{	"msg_thread_back"	,BBS_PROP_MSG_THREAD_BACK	,PROP_READONLY	,312},
	{	"msg_thread_orig"	,BBS_PROP_MSG_THREAD_BACK	,JSPROP_READONLY,310},	/* alias */
	{	"msg_thread_next"	,BBS_PROP_MSG_THREAD_NEXT	,PROP_READONLY	,310},
	{	"msg_thread_first"	,BBS_PROP_MSG_THREAD_FIRST	,PROP_READONLY	,310},
	{	"msg_id"			,BBS_PROP_MSG_ID			,PROP_READONLY	,310},
	{	"msg_reply_id"		,BBS_PROP_MSG_REPLY_ID		,PROP_READONLY	,310},
	{	"msg_delivery_attempts"	,BBS_PROP_MSG_DELIVERY_ATTEMPTS
														,PROP_READONLY	,310},

	{	"msghdr_top_of_screen"	,BBS_PROP_MSGHDR_TOS	,PROP_READONLY	,31702},

	{	"file_name"			,BBS_PROP_FILE_NAME			,PROP_READONLY	,317},
	{	"file_description"	,BBS_PROP_FILE_DESC			,PROP_READONLY	,317},
	{	"file_dir_number"	,BBS_PROP_FILE_DIR			,PROP_READONLY	,317},
	{	"file_attr"			,BBS_PROP_FILE_ATTR			,PROP_READONLY	,317},
	{	"file_date"			,BBS_PROP_FILE_DATE			,PROP_READONLY	,317},
	{	"file_size"			,BBS_PROP_FILE_SIZE			,PROP_READONLY	,317},
	{	"file_credits"		,BBS_PROP_FILE_CREDITS		,PROP_READONLY	,317},
	{	"file_uploader"		,BBS_PROP_FILE_ULER			,PROP_READONLY	,317},
	{	"file_upload_date"	,BBS_PROP_FILE_DATE_ULED	,PROP_READONLY	,317},
	{	"file_download_date",BBS_PROP_FILE_DATE_DLED	,PROP_READONLY	,317},
	{	"file_download_count",BBS_PROP_FILE_TIMES_DLED	,PROP_READONLY	,317},

	{	"batch_upload_total",BBS_PROP_BATCH_UPLOAD_TOTAL,PROP_READONLY	,310},
	{	"batch_dnload_total",BBS_PROP_BATCH_DNLOAD_TOTAL,PROP_READONLY	,310},

	{	"command_str"		,BBS_PROP_COMMAND_STR		,JSPROP_ENUMERATE, 314},
	{0}
};

/* Utility functions */
static uint get_subnum(JSContext* cx, sbbs_t* sbbs, jsval *argv, int argc, int pos)
{
	uint subnum=INVALID_SUB;

	if(argc>pos && JSVAL_IS_STRING(argv[pos])) {
		char * p;

		JSSTRING_TO_ASTRING(cx, JSVAL_TO_STRING(argv[pos]), p, LEN_EXTCODE+2, NULL);
		for(subnum=0;subnum<sbbs->cfg.total_subs;subnum++)
			if(!stricmp(sbbs->cfg.sub[subnum]->code,p))
				break;
	} else if(argc>pos && JSVAL_IS_NUMBER(argv[pos])) {
		uint32 i;
		if(!JS_ValueToECMAUint32(cx,argv[pos],&i))
			return JS_FALSE;
		subnum = i;
	}
	else if(sbbs->usrgrps>0)
		subnum=sbbs->usrsub[sbbs->curgrp][sbbs->cursub[sbbs->curgrp]];

	return(subnum);
}

static uint get_dirnum(JSContext* cx, sbbs_t* sbbs, jsval val, bool dflt)
{
	uint dirnum=INVALID_DIR;

	if(sbbs->usrlibs>0)
		dirnum=sbbs->usrdir[sbbs->curlib][sbbs->curdir[sbbs->curlib]];

	if(!dflt) {
		if(JSVAL_IS_STRING(val)) {
			char	*p;
			JSSTRING_TO_ASTRING(cx, JSVAL_TO_STRING(val), p, LEN_EXTCODE+2, NULL);
			for(dirnum=0;dirnum<sbbs->cfg.total_dirs;dirnum++)
				if(!stricmp(sbbs->cfg.dir[dirnum]->code,p))
					break;
		} else if(JSVAL_IS_NUMBER(val)) {
			uint32 i;
			if(!JS_ValueToECMAUint32(cx,val,&i))
				return JS_FALSE;
			dirnum = i;
		}
		else if(sbbs->usrlibs>0)
			dirnum=sbbs->usrdir[sbbs->curlib][sbbs->curdir[sbbs->curlib]];
	}

	return(dirnum);
}

/**************************/
/* bbs Object Methods */
/**************************/

static JSBool
js_menu(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
    JSString*	str;
 	sbbs_t*		sbbs;
	jsrefcount	rc;
	char		*menu;
	int32		mode = P_NONE;
	JSObject*	obj = JS_GetScopeChain(cx);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

 	str = JS_ValueToString(cx, argv[0]);
 	if (!str)
 		return(JS_FALSE);

	uintN argn = 1;
	if(argc > argn && JSVAL_IS_NUMBER(argv[argn])) {
		if(!JS_ValueToInt32(cx,argv[argn], &mode))
			return JS_FALSE;
		argn++;
	}
	if(argc > argn && JSVAL_IS_OBJECT(argv[argn])) {
		if((obj = JSVAL_TO_OBJECT(argv[argn])) == NULL)
			return JS_FALSE;
		argn++;
	}

	JSSTRING_TO_MSTRING(cx, str, menu, NULL);
	if(!menu)
		return JS_FALSE;
	rc=JS_SUSPENDREQUEST(cx);
	bool result = sbbs->menu(menu, mode, obj);
	free(menu);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, result ? JSVAL_TRUE : JSVAL_FALSE);

    return(JS_TRUE);
}

static JSBool
js_menu_exists(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
    JSString*	str;
 	sbbs_t*		sbbs;
	jsrefcount	rc;
	char		*menu;

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

 	str = JS_ValueToString(cx, argv[0]);
 	if (!str)
 		return(JS_FALSE);

	JSSTRING_TO_MSTRING(cx, str, menu, NULL);
	if(!menu)
		return JS_FALSE;
	rc=JS_SUSPENDREQUEST(cx);
	bool result = sbbs->menu_exists(menu);
	free(menu);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(result));

    return(JS_TRUE);
}


static JSBool
js_hangup(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->hangup();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_nodesync(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval*		argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	jsrefcount	rc;
	JSBool		clearline = false;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc > 0 && JSVAL_IS_BOOLEAN(argv[0]))
		clearline = JSVAL_TO_BOOLEAN(argv[0]);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->getnodedat(sbbs->cfg.node_num,&sbbs->thisnode,0);
	sbbs->nodesync(clearline ? true : false);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_exec(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uintN		i;
	sbbs_t*		sbbs;
	uint32		mode=0;
    JSString*	cmd;
	JSString*	startup_dir=NULL;
	char*		p_startup_dir=NULL;
	jsrefcount	rc;
	char*		cstr;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	if((cmd=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	for(i=1;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i])) {
			if(!JS_ValueToECMAUint32(cx,argv[i],&mode))
				return JS_FALSE;
		}
		else if(JSVAL_IS_STRING(argv[i]))
			startup_dir=JS_ValueToString(cx,argv[i]);
	}

	if(startup_dir!=NULL) {
		JSSTRING_TO_MSTRING(cx, startup_dir, p_startup_dir, NULL);
		if(p_startup_dir==NULL)
			return JS_FALSE;
	}

	JSSTRING_TO_MSTRING(cx, cmd, cstr, NULL);
	if(cstr==NULL) {
		FREE_AND_NULL(p_startup_dir);
		return JS_FALSE;
	}
	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(sbbs->external(cstr,mode,p_startup_dir)));
	free(cstr);
	if(p_startup_dir)
		free(p_startup_dir);
	JS_RESUMEREQUEST(cx, rc);

    return(JS_TRUE);
}

static JSBool
js_exec_xtrn(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		i=0;
	char*		code;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(!js_argc(cx, argc, 1))
		return JS_FALSE;

	if(JSVAL_IS_STRING(argv[0])) {
		JSVALUE_TO_ASTRING(cx,argv[0],code, LEN_CODE+2, NULL);
		if(code==NULL)
			return(JS_FALSE);

		for(i=0;i<sbbs->cfg.total_xtrns;i++)
			if(!stricmp(sbbs->cfg.xtrn[i]->code,code))
				break;
	} else if(JSVAL_IS_NUMBER(argv[0])) {
		if(!JS_ValueToECMAUint32(cx,argv[0],&i))
			return JS_FALSE;
	}

	if(i>=sbbs->cfg.total_xtrns) {
		JS_ReportError(cx, "Invalid external program specified");
		return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->exec_xtrn(i)));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_user_event(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		i=0;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc && JSVAL_IS_NUMBER(argv[0])) {
		if(!JS_ValueToECMAUint32(cx,argv[0],&i))
			return JS_FALSE;
	}
	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->user_event((user_event_t)i)));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_chksyspass(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv = JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	char*		sys_pw = NULL;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if (argc) {
		JSString* str = JS_ValueToString(cx, argv[0]);
		JSSTRING_TO_ASTRING(cx, str, sys_pw, sizeof(sbbs->cfg.sys_pass)+2, NULL);
	}

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->chksyspass(sys_pw)));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_chkpass(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	char*		cstr;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	JSString* str=JS_ValueToString(cx,argv[0]);

	JSSTRING_TO_ASTRING(cx, str, cstr, LEN_PASS+2, NULL);
	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->chkpass(cstr,&sbbs->useron,true)));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}


static JSBool
js_text(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		i=0;
	sbbs_t*		sbbs;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if(argc && JSVAL_IS_NUMBER(argv[0])) {
		if(!JS_ValueToECMAUint32(cx,argv[0],&i))
			return JS_FALSE;
	}

	if(i > 0 && i <= TOTAL_TEXT) {
		JSString* js_str = JS_NewStringCopyZ(cx, sbbs->text[i - 1]);
		if(js_str==NULL)
			return(JS_FALSE);
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	}

	return(JS_TRUE);
}

static JSBool
js_replace_text(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		p;
	uint32		i=0;
	int			len;
	sbbs_t*		sbbs;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

 	if(!js_argc(cx, argc, 2))
		return(JS_FALSE);

	if(JSVAL_IS_NUMBER(argv[0])) {
		if(!JS_ValueToECMAUint32(cx,argv[0],&i))
			return JS_FALSE;
	}
	if(i < 1 || i > TOTAL_TEXT)
		return(JS_TRUE);
	i--;

	if(sbbs->text[i]!=sbbs->text_sav[i] && sbbs->text[i]!=nulstr)
		free(sbbs->text[i]);

	JSVALUE_TO_MSTRING(cx, argv[1], p, NULL);
	if(p==NULL)
		return(JS_FALSE);

	len=strlen(p);
	if(!len) {
		sbbs->text[i]=(char*)nulstr;
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
		free(p);
	} else {
		sbbs->text[i]=p;
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	}

	return(JS_TRUE);
}

static JSBool
js_revert_text(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		i=0;
	sbbs_t*		sbbs;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc && JSVAL_IS_NUMBER(argv[0])) {
		if(!JS_ValueToECMAUint32(cx,argv[0],&i))
			return JS_FALSE;
	}
	i--;

	if(i>=TOTAL_TEXT) {
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

	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);

	return(JS_TRUE);
}

static JSBool
js_load_text(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int			i;
	char		path[MAX_PATH+1];
	FILE*		stream;
	JSString*	js_str;
	sbbs_t*		sbbs;
	char*		cstr;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return(JS_TRUE);
	}
	JSSTRING_TO_MSTRING(cx, js_str, cstr, NULL);
	if(!cstr)
		return JS_FALSE;

	rc=JS_SUSPENDREQUEST(cx);
	for(i=0;i<TOTAL_TEXT;i++) {
		if(sbbs->text[i]!=sbbs->text_sav[i]) {
			if(sbbs->text[i]!=nulstr)
				free(sbbs->text[i]);
			sbbs->text[i]=sbbs->text_sav[i];
		}
	}
	sprintf(path,"%s%s.dat"
		,sbbs->cfg.ctrl_dir,cstr);
	free(cstr);

	if((stream=fnopen(NULL,path,O_RDONLY))==NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}
	for(i=0;i<TOTAL_TEXT && !feof(stream);i++) {
		if((sbbs->text[i]=readtext((long *)NULL,stream,i))==NULL) {
			i--;
			continue;
		}
		if(!strcmp(sbbs->text[i],sbbs->text_sav[i])) {	/* If identical */
			free(sbbs->text[i]);					/* Don't alloc */
			sbbs->text[i]=sbbs->text_sav[i];
		}
		else if(sbbs->text[i][0]==0) {
			free(sbbs->text[i]);
			sbbs->text[i]=(char*)nulstr;
		}
	}
	if(i<TOTAL_TEXT)
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	else
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);

	fclose(stream);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_atcode(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	char	str[128],str2[128],*p;
	char	*instr;
	size_t	disp_len;
	bool	padded_left=false;
	bool	padded_right=false;
	const char *cp;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	JSVALUE_TO_MSTRING(cx, argv[0], instr, NULL);
	if(instr==NULL)
		return(JS_FALSE);

	disp_len=strlen(instr);
	if((p=strstr(instr,"-L"))!=NULL)
		padded_left=true;
	else if((p=strstr(instr,"-R"))!=NULL)
		padded_right=true;
	if(p!=NULL) {
		if(*(p+2) && IS_DIGIT(*(p+2)))
			disp_len=atoi(p+2);
		*p=0;
	}

	if(disp_len >= sizeof(str))
		disp_len=sizeof(str)-1;

	rc=JS_SUSPENDREQUEST(cx);
	cp=sbbs->atcode(instr,str2,sizeof(str2));
	free(instr);
	JS_RESUMEREQUEST(cx, rc);
	if(cp==NULL)
		JS_SET_RVAL(cx, arglist, JSVAL_NULL);
	else {
		if(padded_left)
			sprintf(str,"%-*.*s",(int)disp_len,(int)disp_len,cp);
		else if(padded_right)
			sprintf(str,"%*.*s",(int)disp_len,(int)disp_len,cp);
		else
			SAFECOPY(str,cp);

		JSString* js_str = JS_NewStringCopyZ(cx, str);
		if(js_str==NULL)
			return(JS_FALSE);
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	}

	return(JS_TRUE);
}


static JSBool
js_logkey(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		p;
	JSBool		comma=false;
	JSString*	js_str;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return(JS_TRUE);
	}

	if(argc>1)
		JS_ValueToBoolean(cx,argv[1],&comma);

	JSSTRING_TO_MSTRING(cx, js_str, p, NULL);
	if(p==NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return(JS_TRUE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->logch(*p
		,comma ? true:false	// This is a dumb bool conversion to make BC++ happy
		);
	free(p);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	return(JS_TRUE);
}

static JSBool
js_logstr(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		p;
	JSString*	js_str;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return(JS_TRUE);
	}

	JSSTRING_TO_MSTRING(cx, js_str, p, NULL);
	if(p==NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return(JS_TRUE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->log(p);
	free(p);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	return(JS_TRUE);
}

static JSBool
js_finduser(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		p;
	JSString*	js_str;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));
		return(JS_TRUE);
	}

	JSSTRING_TO_MSTRING(cx, js_str, p, NULL);
	if(p==NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));
		return(JS_TRUE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(sbbs->finduser(p, /* silent_failure: */true)));
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_trashcan(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		str;
	char*		can;
	JSString*	js_str;
	JSString*	js_can;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 2))
		return(JS_FALSE);

	if((js_can=JS_ValueToString(cx, argv[0]))==NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return(JS_TRUE);
	}

	if((js_str=JS_ValueToString(cx, argv[1]))==NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return(JS_TRUE);
	}

	JSSTRING_TO_MSTRING(cx, js_can, can, NULL);
	if(can==NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return(JS_TRUE);
	}

	JSSTRING_TO_MSTRING(cx, js_str, str, NULL);
	if(str==NULL) {
		free(can);
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return(JS_TRUE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->trashcan(str,can)));
	free(can);
	free(str);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_newuser(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->newuser()));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_logon(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->logon()));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_login(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		name;
	char*		pw_prompt = NULL;
	char*		user_pw = NULL;
	char*		sys_pw = NULL;
	JSString*	js_name;
	JSString*	js_pw_prompt = NULL;
	JSString*	js_user_pw = NULL;
	JSString*	js_sys_pw = NULL;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 2))
		return(JS_FALSE);

	if((js_name=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	if(argc > 1)
		js_pw_prompt = JS_ValueToString(cx, argv[1]);
	if(argc > 2)
		js_user_pw = JS_ValueToString(cx, argv[2]);
	if(argc > 3)
		js_sys_pw = JS_ValueToString(cx, argv[3]);

	JSSTRING_TO_ASTRING(cx, js_name, name, (LEN_ALIAS > LEN_NAME) ? LEN_ALIAS+2 : LEN_NAME+2, NULL);
	if(name==NULL)
		return(JS_FALSE);

	JSSTRING_TO_MSTRING(cx, js_pw_prompt, pw_prompt, NULL);
	JSSTRING_TO_MSTRING(cx, js_user_pw, user_pw, NULL);
	JSSTRING_TO_MSTRING(cx, js_sys_pw, sys_pw, NULL);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->login(name,pw_prompt,user_pw,sys_pw)==LOGIC_TRUE ? JS_TRUE:JS_FALSE));
	JS_RESUMEREQUEST(cx, rc);
	FREE_AND_NULL(pw_prompt);
	FREE_AND_NULL(user_pw);
	FREE_AND_NULL(sys_pw);
	return(JS_TRUE);
}


static JSBool
js_logoff(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	jsrefcount	rc;
	JSBool		prompt=JS_TRUE;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc)
		JS_ValueToBoolean(cx,argv[0],&prompt);

	rc=JS_SUSPENDREQUEST(cx);
	if(!prompt || !sbbs->noyes(sbbs->text[LogOffQ])) {
		if(sbbs->cfg.logoff_mod[0])
			sbbs->exec_bin(sbbs->cfg.logoff_mod,&sbbs->main_csi);
		sbbs->user_event(EVENT_LOGOFF);
		sbbs->menu("logoff");
		sbbs->hangup();
	}
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_logout(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->logout();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_automsg(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->automsg();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_time_bank(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->time_bank();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_text_sec(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->text_sec();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_qwk_sec(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->qwk_sec();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_xtrn_sec(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		section = (char*)"";
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	if(argc > 0) {
		JSVALUE_TO_ASTRING(cx, argv[0], section, LEN_CODE + 1, NULL);
	}
	rc=JS_SUSPENDREQUEST(cx);
	sbbs->xtrn_sec(section);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_xfer_policy(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->xfer_policy();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_batchmenu(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->batchmenu();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_batchdownload(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->start_batch_download()));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_batchaddlist(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	jsrefcount	rc;
	char*		cstr;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	JSVALUE_TO_MSTRING(cx, argv[0], cstr, NULL);
	if(cstr==NULL)
		return JS_FALSE;
	rc=JS_SUSPENDREQUEST(cx);
	sbbs->batch_add_list(cstr);
	free(cstr);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_viewfile(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	jsrefcount	rc;
	char*		cstr;

	if((sbbs = js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist))) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], cstr, NULL);
	if(cstr == NULL)
		return JS_FALSE;

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->viewfile(cstr)));
	free(cstr);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_sendfile(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	char		prot=0;
	char*		desc=NULL;
	bool		autohang = true;
	char*		p;
	jsrefcount	rc;
	char*		cstr;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	if(argc>1) {
		JSVALUE_TO_ASTRING(cx, argv[1], p, 2, NULL);
		if(p!=NULL)
			prot=*p;
		uintN argn = 2;
		if(argc > argn && JSVAL_IS_STRING(argv[argn])) {
			JSVALUE_TO_MSTRING(cx, argv[argn], desc, NULL);
			argn++;
		}
		if(argc > argn && JSVAL_IS_BOOLEAN(argv[argn])) {
			autohang = JSVAL_TO_BOOLEAN(argv[argn]);
			argn++;
		}
	}

	JSVALUE_TO_MSTRING(cx, argv[0], cstr, NULL);
	if(cstr==NULL) {
		free(cstr);
		free(desc);
		return JS_FALSE;
	}
	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->sendfile(cstr, prot, desc, autohang)));
	free(cstr);
	free(desc);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_recvfile(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	char		prot=0;
	bool		autohang = true;
	char*		p;
	char*		cstr;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	if(argc>1) {
		JSVALUE_TO_ASTRING(cx, argv[1], p, 2, NULL);
 		if(p!=NULL)
			prot=*p;
		if(argc > 2)
			autohang = JSVAL_TO_BOOLEAN(argv[2]);
	}

	JSVALUE_TO_MSTRING(cx, argv[0], cstr, NULL);
	if(cstr==NULL)
		return JS_FALSE;
	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->recvfile(cstr, prot, autohang)));
	free(cstr);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_temp_xfer(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->temp_xfer();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_user_config(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->maindflts(&sbbs->useron);
	if(!(sbbs->useron.rest&FLAG('G')))    /* not guest */
		getuserdat(&sbbs->cfg,&sbbs->useron);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_user_sync(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	getuserdat(&sbbs->cfg,&sbbs->useron);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_sys_info(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->sys_info();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_sub_info(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	uint subnum=get_subnum(cx,sbbs,argv,argc,0);

	rc=JS_SUSPENDREQUEST(cx);
	if(subnum<sbbs->cfg.total_subs)
		sbbs->subinfo(subnum);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_dir_info(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	uint dirnum=get_dirnum(cx,sbbs,argv[0],argc == 0);
	rc=JS_SUSPENDREQUEST(cx);
	if(dirnum<sbbs->cfg.total_dirs)
		sbbs->dirinfo(dirnum);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_user_info(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->user_info();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_ver(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->ver();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_sys_stats(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->sys_stats();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_node_stats(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		node_num=0;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc>0 && JSVAL_IS_NUMBER(argv[0])) {
		if(!JS_ValueToECMAUint32(cx,argv[0],&node_num))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->node_stats(node_num);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_userlist(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		mode=UL_ALL;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc>0 && JSVAL_IS_NUMBER(argv[0])) {
		if(!JS_ValueToECMAUint32(cx,argv[0],&mode))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->userlist(mode);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_useredit(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32		usernumber=0;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc>0 && JSVAL_IS_NUMBER(argv[0])) {
		if(!JS_ValueToInt32(cx,argv[0],&usernumber))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->useredit(usernumber);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_change_user(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->change_user();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_logonlist(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		args=(char*)"";
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc > 0) {
		JSVALUE_TO_ASTRING(cx, argv[0], args, LEN_CMD, NULL);
	}
	rc=JS_SUSPENDREQUEST(cx);
	sbbs->logonlist(args);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_nodelist(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->nodelist();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_whos_online(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->whos_online(true);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_spy(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32		node_num=0;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	if(JSVAL_IS_NUMBER(argv[0])) {
		if(!JS_ValueToInt32(cx,argv[0],&node_num))
			return JS_FALSE;
	}
	rc=JS_SUSPENDREQUEST(cx);
	sbbs->spy(node_num);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_readmail(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		readwhich=MAIL_YOUR;
	uint32		usernumber;
	uint32		lm_mode = 0;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	usernumber=sbbs->useron.number;
	if(argc>0 && JSVAL_IS_NUMBER(argv[0])) {
		if(!JS_ValueToECMAUint32(cx,argv[0],&readwhich))
			return JS_FALSE;
	}
	if(argc>1 && JSVAL_IS_NUMBER(argv[1])) {
		if(!JS_ValueToECMAUint32(cx,argv[1],&usernumber))
			return JS_FALSE;
	}
	if(argc>2 && JSVAL_IS_NUMBER(argv[2])) {
		if(!JS_ValueToECMAUint32(cx, argv[2], &lm_mode))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->readmail(usernumber, readwhich, lm_mode);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_email(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		usernumber=1;
	uint32		mode=WM_EMAIL;
	const char	*def="";
	char*		top=(char *)def;
	char*		subj=(char *)def;
	JSString*	js_top=NULL;
	JSString*	js_subj=NULL;
	JSObject*	hdrobj;
	sbbs_t*		sbbs;
	smb_t*		resmb = NULL;
	smbmsg_t*	remsg = NULL;
	smbmsg_t	msg;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	ZERO_VAR(msg);
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	if(JSVAL_IS_NUMBER(argv[0])) {
		if(!JS_ValueToECMAUint32(cx,argv[0],&usernumber))
			return JS_FALSE;
	}
	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i])) {
			if(!JS_ValueToECMAUint32(cx,argv[i],&mode))
				return JS_FALSE;
		}
		else if(JSVAL_IS_STRING(argv[i]) && js_top==NULL)
			js_top=JS_ValueToString(cx,argv[i]);
		else if(JSVAL_IS_STRING(argv[i]))
			js_subj=JS_ValueToString(cx,argv[i]);
		else if(JSVAL_IS_OBJECT(argv[i]) && !JSVAL_IS_NULL(argv[i])) {
			if((hdrobj = JSVAL_TO_OBJECT(argv[i])) == NULL)
				return JS_FALSE;
			if(!js_GetMsgHeaderObjectPrivates(cx, hdrobj, &resmb, &remsg, /* post: */NULL)) {
				if(!js_ParseMsgHeaderObject(cx, hdrobj, &msg)) {
					JS_ReportError(cx, "msg hdr object cannot be parsed");
					return JS_FALSE;
				}
				remsg = &msg;
			}
		}
	}

	if(js_top!=NULL)
		JSSTRING_TO_MSTRING(cx, js_top, top, NULL);
	if(top==NULL)
		return JS_FALSE;
	if(js_subj!=NULL)
		JSSTRING_TO_MSTRING(cx, js_subj, subj, NULL);
	if(subj==NULL) {
		if(top != def)
			free(top);
		return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->email(usernumber, top, subj, mode, resmb, remsg)));
	smb_freemsgmem(&msg);
	if(top != def)
		free(top);
	if(subj != def)
		free(subj);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_netmail(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		mode=0;
	char*		to = NULL;
	char*		subj = NULL;
	JSString*	js_str;
	JSObject*	hdrobj;
	sbbs_t*		sbbs;
	smb_t*		resmb = NULL;
	smbmsg_t*	remsg = NULL;
	str_list_t	to_list = NULL;
	smbmsg_t	msg;
	jsrefcount	rc;
	bool		error = false;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	ZERO_VAR(msg);
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	for(uintN i=0; i<argc && !error; i++) {
		if(JSVAL_IS_NUMBER(argv[i])) {
			if(!JS_ValueToECMAUint32(cx,argv[i],&mode))
				error = true;
		}
		else if(JSVAL_IS_STRING(argv[i])) {
			if((js_str = JS_ValueToString(cx, argv[i])) == NULL) {
				error = true;
				break;
			}
			if(to == NULL && to_list == NULL) {
				JSSTRING_TO_MSTRING(cx, js_str, to, NULL);
			} else if(subj == NULL) {
				JSSTRING_TO_MSTRING(cx, js_str, subj, NULL);
			}
		}
		else if(JSVAL_IS_OBJECT(argv[i]) && !JSVAL_IS_NULL(argv[i])) {
			if((hdrobj = JSVAL_TO_OBJECT(argv[i])) == NULL) {
				error = true;
				break;
			}
			jsuint len=0;
			if(JS_GetArrayLength(cx, hdrobj, &len) && len > 0) { // to_list[]
				to_list = strListInit();
				for(jsuint j=0; j < len; j++) {
					jsval		val;
					if(!JS_GetElement(cx, hdrobj, j, &val)) {
						error = true;
						break;
					}
					if((js_str = JS_ValueToString(cx, val)) == NULL) {
						error = true;
						break;
					}
					char* cstr = NULL;
					JSSTRING_TO_ASTRING(cx, js_str, cstr, 64, NULL);
					if(cstr == NULL) {
						error = true;
						break;
					}
					strListPush(&to_list, cstr);
				}
				continue;
			}
			if(!js_GetMsgHeaderObjectPrivates(cx, hdrobj, &resmb, &remsg, /* post: */NULL)) {
				if(!js_ParseMsgHeaderObject(cx, hdrobj, &msg)) {
					JS_ReportError(cx, "msg hdr object cannot be parsed");
					error = true;
					break;
				}
				remsg = &msg;
			}
		}
	}

	rc=JS_SUSPENDREQUEST(cx);
	if(!error)
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->netmail(to, subj, mode, resmb, remsg, to_list)));
	smb_freemsgmem(&msg);
	strListFree(&to_list);
	FREE_AND_NULL(subj);
	FREE_AND_NULL(to);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_bulkmail(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uchar*		ar = NULL;
	sbbs_t*		sbbs;
	jsrefcount	rc;
	char		*p;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc) {
		JSVALUE_TO_MSTRING(cx, argv[0], p, NULL);
		if(p==NULL)
			return(JS_FALSE);
		ar=arstr(NULL, p, &sbbs->cfg, NULL);
		free(p);
	}
	rc=JS_SUSPENDREQUEST(cx);
	sbbs->bulkmail(ar);
	free(ar);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_upload_file(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint		dirnum=0;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	dirnum=get_dirnum(cx,sbbs,argv[0], argc == 0);

	if(dirnum>=sbbs->cfg.total_dirs) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return(JS_TRUE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->upload(dirnum)));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}


static JSBool
js_bulkupload(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint		dirnum=0;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	dirnum=get_dirnum(cx,sbbs,argv[0], argc == 0);

	if(dirnum>=sbbs->cfg.total_dirs) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return(JS_TRUE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->bulkupload(dirnum)==0));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_telnet_gate(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		addr;
	uint32		mode=0;
	JSString*	js_addr;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	if((js_addr=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	JSSTRING_TO_MSTRING(cx, js_addr, addr, NULL);
	if(addr==NULL)
		return(JS_FALSE);

	if(argc>1 && JSVAL_IS_NUMBER(argv[1])) {
		if(!JS_ValueToECMAUint32(cx,argv[1],&mode)) {
			free(addr);
			return JS_FALSE;
		}
	}

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->telnet_gate(addr,mode);
	free(addr);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_rlogin_gate(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval*		argv=JS_ARGV(cx, arglist);
	uintN		argn;
	char*		addr;
	char*		client_user_name=NULL;
	char*		server_user_name=NULL;
	char*		term_type=NULL;
	bool		fail = false;
	uint32		mode = 0;
	JSString*	js_str;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

	if((js_str=JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	JSSTRING_TO_MSTRING(cx, js_str, addr, NULL);
	if(addr==NULL)
		return(JS_FALSE);

	/* Parse optional arguments if provided */
	for(argn=1; argn<argc; argn++) {
		if(JSVAL_IS_STRING(argv[argn])) {
			if((js_str=JS_ValueToString(cx, argv[argn]))==NULL) {
				fail = true;
				break;
			}
			if(client_user_name==NULL) {
				JSSTRING_TO_MSTRING(cx, js_str, client_user_name, NULL);
			} else if(server_user_name==NULL) {
				JSSTRING_TO_MSTRING(cx, js_str, server_user_name, NULL);
			} else if(term_type==NULL) {
				JSSTRING_TO_MSTRING(cx, js_str, term_type, NULL);
			}
		} else if(JSVAL_IS_NUMBER(argv[argn])) {
			if(!JS_ValueToECMAUint32(cx,argv[argn],&mode)) {
				fail = true;
				break;
			}
		}
	}
	if(!fail) {
		rc=JS_SUSPENDREQUEST(cx);
		sbbs->telnet_gate(addr,mode|TG_RLOGIN,client_user_name,server_user_name,term_type);
		JS_RESUMEREQUEST(cx, rc);
	}
	FREE_AND_NULL(addr);
	FREE_AND_NULL(client_user_name);
	FREE_AND_NULL(server_user_name);
	FREE_AND_NULL(term_type);

	return(fail ? JS_FALSE : JS_TRUE);
}

static JSBool
js_pagesysop(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->sysop_page()));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_pageguru(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->guru_page()));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_multinode_chat(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	int32		channel=1;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc>1 && JSVAL_IS_NUMBER(argv[1])) {
		if(!JS_ValueToInt32(cx,argv[1],&channel))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->multinodechat(channel);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_private_message(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->nodemsg();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_private_chat(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	JSBool		local=false;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc)
		JS_ValueToBoolean(cx,argv[0],&local);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->privchat(local ? true:false);	// <- eliminates stupid msvc6 "performance warning"
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_get_node_message(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval*		argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	jsrefcount	rc;
	JSBool		clearline = false;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc > 0 && JSVAL_IS_BOOLEAN(argv[0]))
		clearline = JSVAL_TO_BOOLEAN(argv[0]);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->getnmsg(clearline ? true : false);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_put_node_message(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uintN		argn = 0;
	sbbs_t*		sbbs;
	int32		nodenum = 0;
	JSString*	js_msg;
	char*		msg = NULL;
	char 		str[256];
	char		tmp[512];
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	/* Get the destination node number */
	if(argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
		if(!JS_ValueToInt32(cx,argv[argn], &nodenum))
			return JS_FALSE;
		argn++;
	} else {
		rc=JS_SUSPENDREQUEST(cx);
		nodenum = sbbs->getnodetopage(/* all: */TRUE, /* telegram: */FALSE);
		JS_RESUMEREQUEST(cx, rc);
	}
	if(nodenum == 0)
		return JS_TRUE;

	int usernumber = 0;
	node_t node;
	if(nodenum >= 1) {	/* !all */
		sbbs->getnodedat(nodenum, &node, false);
		usernumber = node.useron;
		if((node.misc&NODE_POFF) && sbbs->useron.level < SYSOP_LEVEL) {
			sbbs->bprintf(sbbs->text[CantPageNode]
				, node.misc&NODE_ANON ? sbbs->text[UNKNOWN_USER] : username(&sbbs->cfg,node.useron,tmp));
			return JS_TRUE;
		}
	}

	/* Get the node message text */
	if(argn < argc) {
		if((js_msg = JS_ValueToString(cx, argv[argn])) == NULL)
			return JS_FALSE;
		argn++;
		JSSTRING_TO_MSTRING(cx, js_msg, msg, NULL);
	} else {
		if(nodenum >= 1)
			sbbs->bprintf(sbbs->text[SendingMessageToUser]
				,node.misc&NODE_ANON ? sbbs->text[UNKNOWN_USER]
				: username(&sbbs->cfg,node.useron, tmp)
				,node.misc&NODE_ANON ? 0 : node.useron);
		sbbs->bputs(sbbs->text[NodeMsgPrompt]);
		rc=JS_SUSPENDREQUEST(cx);
		char line[128];
		int result = sbbs->getstr(line,69,K_LINE);
		JS_RESUMEREQUEST(cx, rc);
		if(result < 1)
			return JS_TRUE;

		sprintf(str, sbbs->text[nodenum >= 1 ? NodeMsgFmt : AllNodeMsgFmt]
			,sbbs->cfg.node_num
			,sbbs->thisnode.misc&NODE_ANON
				? sbbs->text[UNKNOWN_USER] : sbbs->useron.alias, line);
		msg = strdup(str);
	}

	if(msg == NULL)
		return JS_FALSE;

	/* Send the message(s) */
	BOOL success = TRUE;
	rc=JS_SUSPENDREQUEST(cx);
	if(nodenum < 0 ) {	/* ALL */
		for(int i=1; i<=sbbs->cfg.sys_nodes && success; i++) {
			if(i==sbbs->cfg.node_num)
				continue;
			sbbs->getnodedat(i, &node, false);
			if((node.status==NODE_INUSE
				|| (sbbs->useron.level >= SYSOP_LEVEL && node.status==NODE_QUIET))
				&& (sbbs->useron.level >= SYSOP_LEVEL || !(node.misc&NODE_POFF)))
				if(putnmsg(&sbbs->cfg, i, msg) != 0)
					success = FALSE;
		}
		if(success) {
			sbbs->logline("C", "sent message to all nodes");
			sbbs->logline(nulstr, msg);
		}
	} else {
		success = putnmsg(&sbbs->cfg, nodenum, msg) == 0;
		if(success && !(node.misc&NODE_ANON))
			sbbs->bprintf(sbbs->text[MsgSentToUser],"Message"
				,username(&sbbs->cfg,usernumber,tmp), usernumber);
		SAFEPRINTF3(str, "%s message to %s on node %d:"
			,success ? "sent" : "FAILED to send", username(&sbbs->cfg, usernumber, tmp), nodenum);
		sbbs->logline("C",str);
		sbbs->logline(nulstr, msg);
	}
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(success));
	JS_RESUMEREQUEST(cx, rc);
	free(msg);

	return(JS_TRUE);
}

static JSBool
js_get_telegram(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	int32		usernumber;
	jsrefcount	rc;
	JSBool		clearline = false;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	usernumber=sbbs->useron.number;
	if(argc && JSVAL_IS_NUMBER(argv[0])) {
		if(!JS_ValueToInt32(cx,argv[0],&usernumber))
			return JS_FALSE;
	}
	if(argc > 1 && JSVAL_IS_BOOLEAN(argv[1]))
		clearline = JSVAL_TO_BOOLEAN(argv[1]);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->getsmsg(usernumber, clearline ? true : false);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_put_telegram(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uintN		argn = 0;
	sbbs_t*		sbbs;
	int32		usernumber = 0;
	JSString*	js_msg = NULL;
	char*		msg = NULL;
	char 		str[256];
	char		tmp[512];
	char		logbuf[512] = "";
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	/* Get the destination user number */
	if(argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
		if(!JS_ValueToInt32(cx,argv[argn], &usernumber))
			return JS_FALSE;
		argn++;
	} else {
		rc=JS_SUSPENDREQUEST(cx);
		usernumber = sbbs->getnodetopage(/* all: */FALSE, /* telegram: */TRUE);
		JS_RESUMEREQUEST(cx, rc);
	}

	/* Validate the destination user number */
	if(usernumber < 1)
		return JS_TRUE;

	if(usernumber == 1 && sbbs->useron.rest&FLAG('S')) { /* ! val fback */
		sbbs->bprintf(sbbs->text[R_Feedback], sbbs->cfg.sys_op);
		return JS_TRUE;
	}
	if(usernumber > 1 && sbbs->useron.rest&FLAG('E')) {
		sbbs->bputs(sbbs->text[R_Email]);
		return JS_TRUE;
	}

	/* Get the telegram message text */
	if(argn < argc) {
		if((js_msg = JS_ValueToString(cx, argv[argn])) == NULL)
			return JS_FALSE;
		argn++;
		JSSTRING_TO_MSTRING(cx, js_msg, msg, NULL);
	} else {
		char buf[512];

		rc=JS_SUSPENDREQUEST(cx);
		sbbs->bprintf(sbbs->text[SendingTelegramToUser]
			,username(&sbbs->cfg,usernumber,tmp),usernumber);
		SAFEPRINTF2(buf,sbbs->text[TelegramFmt]
			,sbbs->thisnode.misc&NODE_ANON ? sbbs->text[UNKNOWN_USER] : sbbs->useron.alias
			,sbbs->timestr(time(NULL)));
		int i=0;
		while(sbbs->online && i<5) {
			char line[256];
			sbbs->bputs("\1n: \1h");
			if(!sbbs->getstr(line, 70, i < 4 ? (K_WRAP|K_MSG) : (K_MSG)))
				break;
			SAFEPRINTF2(str,"%4s%s\r\n",nulstr,line);
			SAFECAT(buf, str);
			if(i && line[0])
				SAFECAT(logbuf, " ");
			SAFECAT(logbuf, line);
			i++;
		}
		JS_RESUMEREQUEST(cx, rc);
		if(!i)
			return JS_TRUE;
		if(sbbs->sys_status&SS_ABORT) {
			sbbs->bputs(crlf);
			return JS_TRUE;
		}
		msg = strdup(buf);
	}

	if(msg==NULL)
		return(JS_FALSE);

	rc=JS_SUSPENDREQUEST(cx);
	bool success = putsmsg(&sbbs->cfg,usernumber,msg)==0;
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(success));
	free(msg);

	SAFEPRINTF3(str,"%s telegram to %s #%u"
		,success ? "sent" : "FAILED to send", username(&sbbs->cfg,usernumber,tmp), usernumber);
	sbbs->logline("C",str);
	if(logbuf[0])
		sbbs->logline(nulstr,logbuf);
	if(success)
		sbbs->bprintf(sbbs->text[MsgSentToUser], "Telegram", username(&sbbs->cfg,usernumber,tmp), usernumber);

	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_cmdstr(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		p = NULL;
	const char	*def="";
	char*		fpath=(char *)def;
	char*		fspec=(char *)def;
	JSString*	js_str;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

 	if(!js_argc(cx, argc, 1))
		return(JS_FALSE);

 	js_str = JS_ValueToString(cx, argv[0]);
 	if (!js_str)
 		return(JS_FALSE);

	JSSTRING_TO_MSTRING(cx, js_str, p, NULL);
	if(p == NULL)
		return JS_FALSE;

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_STRING(argv[i])) {
			js_str = JS_ValueToString(cx, argv[i]);
			if(fpath==def) {
				JSSTRING_TO_MSTRING(cx, js_str, fpath, NULL);
				if(fpath==NULL) {
					if(fspec != def)
						free(fspec);
					free(p);
					return JS_FALSE;
				}
			}
			else if(fspec==def) {
				JSSTRING_TO_MSTRING(cx, js_str, fspec, NULL);
				if(fspec==NULL) {
					if(fpath != def)
						free(fpath);
					free(p);
					return JS_FALSE;
				}
			}
		}
	}

	rc=JS_SUSPENDREQUEST(cx);
	char* cmd = sbbs->cmdstr(p, fpath, fspec, NULL);
	free(p);
	if(fpath != def)
		free(fpath);
	if(fspec != def)
		free(fspec);
	JS_RESUMEREQUEST(cx, rc);

	if((js_str=JS_NewStringCopyZ(cx, cmd))==NULL)
		return(JS_FALSE);
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return(JS_TRUE);
}

static JSBool
js_getfilespec(JSContext *cx, uintN argc, jsval *arglist)
{
	char*		p;
	char		tmp[128];
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	p=sbbs->getfilespec(tmp);
	JS_RESUMEREQUEST(cx, rc);

	if(p==NULL)
		JS_SET_RVAL(cx, arglist,JSVAL_NULL);
	else {
		JSString* js_str = JS_NewStringCopyZ(cx, p);
		if(js_str==NULL)
			return(JS_FALSE);
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	}
	return(JS_TRUE);
}

static JSBool
js_export_filelist(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		mode=0;
	char*		fname=NULL;
    JSString*	js_str;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return JS_FALSE;

	uintN argn = 0;
	js_str = JS_ValueToString(cx, argv[argn]);
	JSSTRING_TO_MSTRING(cx, js_str, fname, NULL);
	HANDLE_PENDING(cx, fname);
	argn++;
	if(JSVAL_IS_NUMBER(argv[argn])) {
		if(!JS_ValueToECMAUint32(cx, argv[argn], &mode)) {
			free(fname);
			return JS_FALSE;
		}
		argn++;
	}
	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(sbbs->create_filelist(fname, mode)));
	free(fname);
	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}

static JSBool
js_listfiles(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		mode=0;
	const char	*def=ALLFILES;
	char*		afspec=NULL;
	char*		fspec=(char *)def;
	uint		dirnum;
    JSString*	js_str;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	dirnum=get_dirnum(cx,sbbs,argv[0], argc == 0);

	if(dirnum>=sbbs->cfg.total_dirs) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));
		return(JS_TRUE);
	}

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i])) {
			if(!JS_ValueToECMAUint32(cx,argv[i],&mode)) {
				if(fspec != def)
					FREE_AND_NULL(fspec);
				return JS_FALSE;
			}
		}
		else if(JSVAL_IS_STRING(argv[i])) {
			js_str = JS_ValueToString(cx, argv[i]);
			if(fspec != def)
				FREE_AND_NULL(fspec);
			JSSTRING_TO_MSTRING(cx, js_str, afspec, NULL);
			if(afspec==NULL)
				return JS_FALSE;
			fspec=afspec;
		}
	}

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(sbbs->listfiles(dirnum,fspec,0 /* tofile */,mode)));
	if(afspec)
		free(afspec);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}


static JSBool
js_listfileinfo(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		mode=FI_INFO;
	const char	*def=ALLFILES;
	char*		fspec=(char *)def;
	uint		dirnum;
    JSString*	js_str;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	dirnum=get_dirnum(cx,sbbs,argv[0], argc == 0);

	if(dirnum>=sbbs->cfg.total_dirs) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));
		return(JS_TRUE);
	}

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i])) {
			if(!JS_ValueToECMAUint32(cx,argv[i],&mode)) {
				if(fspec != def)
					free(fspec);
				return JS_FALSE;
			}
		}
		else if(JSVAL_IS_STRING(argv[i])) {
			js_str = JS_ValueToString(cx, argv[i]);
			if(fspec != def && fspec != NULL)
				free(fspec);
			JSSTRING_TO_MSTRING(cx, js_str, fspec, NULL);
			if(fspec==NULL)
				return JS_FALSE;
		}
	}

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(sbbs->listfileinfo(dirnum, fspec, mode)));
	if(fspec != def)
		free(fspec);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_post_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		mode=0;
	uint		subnum;
	uintN		n;
	JSObject*	hdrobj;
	sbbs_t*		sbbs;
	smb_t*		resmb = NULL;
	smbmsg_t*	remsg = NULL;
	smbmsg_t	msg;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	subnum=get_subnum(cx,sbbs,argv,argc,0);

	if(subnum>=sbbs->cfg.total_subs) 	// invalid sub-board
		return(JS_TRUE);

	ZERO_VAR(msg);

	for(n=1; n<argc; n++) {
		if(JSVAL_IS_NUMBER(argv[n])) {
			if(!JS_ValueToECMAUint32(cx,argv[n],&mode))
				return JS_FALSE;
		}
		else if(JSVAL_IS_OBJECT(argv[n]) && !JSVAL_IS_NULL(argv[n])) {
			if((hdrobj=JSVAL_TO_OBJECT(argv[n]))==NULL)
				return JS_FALSE;
			if(!js_GetMsgHeaderObjectPrivates(cx, hdrobj, &resmb, &remsg, /* post: */NULL)) {
				if(!js_ParseMsgHeaderObject(cx, hdrobj, &msg)) {
					JS_ReportError(cx, "msg hdr object cannot be parsed");
					return JS_FALSE;
				}
				remsg = &msg;
			}
		}
	}

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->postmsg(subnum, mode, resmb, remsg)));
	smb_freemsgmem(&msg);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_forward_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uintN		n;
	JSObject*	hdrobj;
	sbbs_t*		sbbs;
	smb_t*		smb = NULL;
	smbmsg_t*	msg = NULL;
	char*		to = NULL;
	char*		subject = NULL;
	char*		comment = NULL;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	for(n=0; n<argc; n++) {
		if(JSVAL_IS_OBJECT(argv[n]) && !JSVAL_IS_NULL(argv[n])) {
			if((hdrobj=JSVAL_TO_OBJECT(argv[n]))==NULL) {
				free(to);
				free(subject);
				free(comment);
				return JS_FALSE;
			}
			if(!js_GetMsgHeaderObjectPrivates(cx, hdrobj, &smb, &msg, /* post_t */NULL)) {
				JS_ReportError(cx, "msg hdr object lacks privates");
				free(to);
				free(subject);
				free(comment);
				return JS_FALSE;
			}
		} else if(JSVAL_IS_STRING(argv[n])) {
			JSString* str = JS_ValueToString(cx, argv[n]);
			if(to == NULL) {
				JSSTRING_TO_MSTRING(cx, str, to, NULL);
			} else if(subject == NULL) {
				JSSTRING_TO_MSTRING(cx, str, subject, NULL);
			} else if(comment == NULL) {
				JSSTRING_TO_MSTRING(cx, str, comment, NULL);
			}
		}
	}
	if(smb != NULL && msg != NULL && to != NULL) {
		rc=JS_SUSPENDREQUEST(cx);
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->forwardmsg(smb, msg, to, subject, comment)));
		JS_RESUMEREQUEST(cx, rc);
	}
	free(subject);
	free(comment);
	free(to);

	return JS_TRUE;
}

static JSBool
js_edit_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uintN		n;
	JSObject*	hdrobj;
	sbbs_t*		sbbs;
	smb_t*		smb = NULL;
	smbmsg_t*	msg = NULL;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	for(n=0; n<argc; n++) {
		if(JSVAL_IS_OBJECT(argv[n]) && !JSVAL_IS_NULL(argv[n])) {
			if((hdrobj=JSVAL_TO_OBJECT(argv[n]))==NULL)
				return JS_FALSE;
			if(!js_GetMsgHeaderObjectPrivates(cx, hdrobj, &smb, &msg, /* post_t */NULL)) {
				JS_ReportError(cx, "msg hdr object lacks privates");
				return JS_FALSE;
			}
		}
	}
	if(smb != NULL && msg != NULL) {
		rc=JS_SUSPENDREQUEST(cx);
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->editmsg(smb, msg)));
		JS_RESUMEREQUEST(cx, rc);
	}
	return JS_TRUE;
}

static JSBool
js_show_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		p_mode = 0;
	uintN		n;
	JSObject*	hdrobj;
	sbbs_t*		sbbs;
	smb_t*		smb = NULL;
	smbmsg_t*	msg = NULL;
	post_t*		post = NULL;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	for(n=0; n<argc; n++) {
		if(JSVAL_IS_NUMBER(argv[n])) {
			if(!JS_ValueToECMAUint32(cx, argv[n], &p_mode))
				return JS_FALSE;
		}
		else if(JSVAL_IS_OBJECT(argv[n]) && !JSVAL_IS_NULL(argv[n])) {
			if((hdrobj=JSVAL_TO_OBJECT(argv[n]))==NULL)
				return JS_FALSE;
			if(!js_GetMsgHeaderObjectPrivates(cx, hdrobj, &smb, &msg, &post)) {
				JS_ReportError(cx, "msg hdr object lacks privates");
				return JS_FALSE;
			}
		}
	}
	if(smb == NULL || msg == NULL)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->show_msg(smb, msg, p_mode, post)));
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_show_msg_header(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uintN		n;
	JSObject*	hdrobj;
	sbbs_t*		sbbs;
	smb_t*		smb = NULL;
	smbmsg_t*	msg = NULL;
	char*		subject = NULL;
	char*		from = NULL;
	char*		to = NULL;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	for(n=0; n<argc; n++) {
		if(JSVAL_IS_OBJECT(argv[n]) && !JSVAL_IS_NULL(argv[n])) {
			if((hdrobj=JSVAL_TO_OBJECT(argv[n]))==NULL) {
				JS_ReportError(cx, "invalid object argument");
				free(subject);
				free(from);
				free(to);
				return JS_FALSE;
			}
			if(!js_GetMsgHeaderObjectPrivates(cx, hdrobj, &smb, &msg, NULL)) {
				JS_ReportError(cx, "msg hdr object lacks privates");
				free(subject);
				free(from);
				free(to);
				return JS_FALSE;
			}
		} else if(JSVAL_IS_STRING(argv[n])) {
			JSString* str = JS_ValueToString(cx, argv[n]);
			if(subject == NULL) {
				JSSTRING_TO_MSTRING(cx, str, subject, NULL);
			} else if(from == NULL) {
				JSSTRING_TO_MSTRING(cx, str, from, NULL);
			} else if(to == NULL) {
				JSSTRING_TO_MSTRING(cx, str, to, NULL);
			}
		}
	}
	if(smb != NULL && msg != NULL) {
		rc=JS_SUSPENDREQUEST(cx);
		sbbs->show_msghdr(smb, msg, subject, from, to);
		JS_RESUMEREQUEST(cx, rc);
	}
	free(subject);
	free(from);
	free(to);

	return JS_TRUE;
}

static JSBool
js_download_msg_attachments(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uintN		n;
	JSObject*	hdrobj;
	sbbs_t*		sbbs;
	smb_t*		smb = NULL;
	smbmsg_t*	msg = NULL;
	bool		del = true;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	for(n=0; n<argc; n++) {
		if(JSVAL_IS_OBJECT(argv[n]) && !JSVAL_IS_NULL(argv[n])) {
			if((hdrobj=JSVAL_TO_OBJECT(argv[n]))==NULL)
				return JS_FALSE;
			if(!js_GetMsgHeaderObjectPrivates(cx, hdrobj, &smb, &msg, NULL)) {
				JS_ReportError(cx, "msg hdr object lacks privates");
				return JS_FALSE;
			}
		} else if(JSVAL_IS_BOOLEAN(argv[n])) {
			del = JSVAL_TO_BOOLEAN(argv[n]) ? true : false;
		}
	}
	if(smb == NULL || msg == NULL)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->download_msg_attachments(smb, msg, del);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_change_msg_attr(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	JSObject*	hdrobj;
	sbbs_t*		sbbs;
	smbmsg_t*	msg = NULL;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc < 1 || !JSVAL_IS_OBJECT(argv[0]) || JSVAL_IS_NULL(argv[0]))
		return JS_TRUE;

	if((hdrobj=JSVAL_TO_OBJECT(argv[0]))==NULL)
		return JS_FALSE;
	if(!js_GetMsgHeaderObjectPrivates(cx, hdrobj, NULL, &msg, NULL)) {
		JS_ReportError(cx, "msg hdr object lacks privates");
		return JS_FALSE;
	}
	if(msg == NULL)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	int32 attr = sbbs->chmsgattr(*msg);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(attr));
	return JS_TRUE;
}

static JSBool
js_msgscan_cfg(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		mode=SUB_CFG_NSCAN;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc && JSVAL_IS_NUMBER(argv[0])) {
		if(!JS_ValueToECMAUint32(cx,argv[0],&mode))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->new_scan_cfg(mode);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}


static JSBool
js_msgscan_ptrs(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->new_scan_ptr_cfg();
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_msgscan_reinit(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	for(uint i=0;i<sbbs->cfg.total_subs;i++) {
		sbbs->subscan[i].ptr=sbbs->subscan[i].sav_ptr;
		sbbs->subscan[i].last=sbbs->subscan[i].sav_last;
	}
	sbbs->bputs(sbbs->text[MsgPtrsInitialized]);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_scansubs(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		mode=SCAN_NEW;
	BOOL		all=FALSE;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	for(uintN i=0;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i])) {
			if(!JS_ValueToECMAUint32(cx,argv[i],&mode))
				return JS_FALSE;
		}
		else if(JSVAL_IS_BOOLEAN(argv[i]))
			all=JSVAL_TO_BOOLEAN(argv[i]);
	}

	rc=JS_SUSPENDREQUEST(cx);
	if(all)
		sbbs->scanallsubs(mode);
	else
		sbbs->scansubs(mode);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_scandirs(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uint32		mode=0;
	BOOL		all=FALSE;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	for(uintN i=0;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i])) {
			if(!JS_ValueToECMAUint32(cx,argv[i],&mode))
				return JS_FALSE;
		}
		else if(JSVAL_IS_BOOLEAN(argv[i]))
			all=JSVAL_TO_BOOLEAN(argv[i]);
	}

	rc=JS_SUSPENDREQUEST(cx);
	if(all)
		sbbs->scanalldirs(mode);
	else
		sbbs->scandirs(mode);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_scanposts(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	const char	*def="";
	char*		find=(char *)def;
	uint32		mode=0;
	uint		subnum;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	subnum=get_subnum(cx,sbbs,argv,argc,0);

	if(subnum>=sbbs->cfg.total_subs) {	// invalid sub-board
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		return(JS_TRUE);
	}

	for(uintN i=1;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i])) {
			if(!JS_ValueToECMAUint32(cx,argv[i],&mode)) {
				if(find != def)
					free(find);
				return JS_FALSE;
			}
		}
		else if(JSVAL_IS_STRING(argv[i]) && find==def) {
			JSVALUE_TO_MSTRING(cx, argv[i], find, NULL);
			if(find==NULL)
				return JS_FALSE;
		}
	}

	if(*find)
		mode|=SCAN_FIND;

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->scanposts(subnum,mode,find)==0));
	if(find != def)
		free(find);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_listmsgs(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	const char	*def="";
	char*		find=(char *)def;
	uint32		mode=SCAN_INDEX;
	uint32		start=0;
	uint		subnum;
	sbbs_t*		sbbs;
	uintN		argn=0;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));

	subnum=get_subnum(cx,sbbs,argv,argc,argn++);

	if(subnum>=sbbs->cfg.total_subs) 	// invalid sub-board
		return(JS_TRUE);

	if(argc > argn && JSVAL_IS_NUMBER(argv[argn])) {
		if(!JS_ValueToECMAUint32(cx,argv[argn++],&mode))
			return JS_FALSE;
	}
	if(argc > argn && JSVAL_IS_NUMBER(argv[argn])) {
		if(!JS_ValueToECMAUint32(cx,argv[argn++],&start))
			return JS_FALSE;
	}
	if(argc > argn && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], find, NULL);
		if(find==NULL)
			return JS_FALSE;
		argn++;
	}

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(sbbs->listsub(subnum,mode,start,find)));
	if(find != def)
		free(find);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_getnstime(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	time_t		t=time(NULL);
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if(argc && JSVAL_IS_NUMBER(argv[0])) {
		uint32 i;
		if(!JS_ValueToECMAUint32(cx,argv[0],&i))
			return JS_FALSE;
		t = i;
	}

	rc=JS_SUSPENDREQUEST(cx);
	if(sbbs->inputnstime(&t)==true) {
		JS_RESUMEREQUEST(cx, rc);
		JS_SET_RVAL(cx, arglist,DOUBLE_TO_JSVAL((double)t));
	}
	else
		JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_select_shell(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->select_shell()));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_select_editor(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->select_editor()));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_get_time_left(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(sbbs->gettimeleft()));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_chk_ar(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	uchar*		ar;
	sbbs_t*		sbbs;
	jsrefcount	rc;
	char		*p;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL);
	if(p==NULL)
		return JS_FALSE;

	rc=JS_SUSPENDREQUEST(cx);
	ar = arstr(NULL,p,&sbbs->cfg,NULL);
	free(p);

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->chk_ar(ar,&sbbs->useron,&sbbs->client)));

	if(ar!=NULL)
		free(ar);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_select_node(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	jsrefcount	rc;
	BOOL		all = FALSE;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	if(argc > 0 && JSVAL_IS_BOOLEAN(argv[0]))
		all = JSVAL_TO_BOOLEAN(argv[0]);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(sbbs->getnodetopage(all, /* telegram: */FALSE)));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_select_user(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	if((sbbs=js_GetPrivate(cx, JS_THIS_OBJECT(cx, arglist)))==NULL)
		return(JS_FALSE);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(sbbs->getnodetopage(/* all: */FALSE, /* telegram: */TRUE)));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static jsSyncMethodSpec js_bbs_functions[] = {
	{"atcode",			js_atcode,			1,	JSTYPE_STRING,	JSDOCSTR("code_string")
	,JSDOCSTR("returns @-code value, specified <i>code</i> string does not include @ character delimiters")
	,310
	},
	/* text.dat */
	{"text",			js_text,			1,	JSTYPE_STRING,	JSDOCSTR("line_number")
	,JSDOCSTR("returns specified text string from text.dat")
	,310
	},
	{"replace_text",	js_replace_text,	2,	JSTYPE_BOOLEAN,	JSDOCSTR("line_number, text")
	,JSDOCSTR("replaces specified text string in memory")
	,310
	},
	{"revert_text",		js_revert_text,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("[line_number=<i>all</i>]")
	,JSDOCSTR("reverts specified text string to original text string; "
		"if <i>line_number</i> unspecified, reverts all text lines")
	,310
	},
	{"load_text",		js_load_text,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("base_filename")
	,JSDOCSTR("load an alternate text.dat from ctrl directory, automatically appends '.dat' to basefilename")
	,310
	},
	/* procedures */
	{"newuser",			js_newuser,			0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("interactive new user procedure")
	,310
	},
	{"login",			js_login,			4,	JSTYPE_BOOLEAN,	JSDOCSTR("user_name [,password_prompt] [,user_password] [,system_password]")
	,JSDOCSTR("login with <i>user_name</i>, displaying <i>password_prompt</i> for user's password (if required), "
	"optionally supplying the user's password and the system password as arguments so as to not be prompted")
	,310
	},
	{"logon",			js_logon,			0,	JSTYPE_BOOLEAN,	JSDOCSTR("")
	,JSDOCSTR("interactive logon procedure")
	,310
	},
	{"logoff",			js_logoff,			1,	JSTYPE_VOID,	JSDOCSTR("[prompt=<i>true</i>]")
	,JSDOCSTR("interactive logoff procedure, pass <i>false</i> for <i>prompt</i> argument to avoid yes/no prompt")
	,315
	},
	{"logout",			js_logout,			0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("non-interactive logout procedure")
	,310
	},
	{"hangup",			js_hangup,			0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("hangup (disconnect) immediately")
	,310
	},
	{"node_sync",		js_nodesync,		1,	JSTYPE_ALIAS },
	{"nodesync",		js_nodesync,		1,	JSTYPE_VOID,	JSDOCSTR("[clearline=<i>false</i>]")
	,JSDOCSTR("synchronize with node database, checks for messages, interruption, etc. (AKA node_sync), "
	"clears the current console line if there's a message to print when <i>clearline</i> is <i>true</i>.")
	,310
	},
	{"auto_msg",		js_automsg,			0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("read/create system's auto-message")
	,310
	},
	{"time_bank",		js_time_bank,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("enter the time banking system")
	,310
	},
	{"qwk_sec",			js_qwk_sec,			0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("enter the QWK message packet upload/download/config section")
	,310
	},
	{"text_sec",		js_text_sec,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("enter the text files section")
	,310
	},
	{"xtrn_sec",		js_xtrn_sec,		0,	JSTYPE_VOID,	JSDOCSTR("[section]")
	,JSDOCSTR("enter the external programs section (or go directly to the specified <i>section</i>)")
	,310
	},
	{"xfer_policy",		js_xfer_policy,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("display the file transfer policy")
	,310
	},
	{"batch_menu",		js_batchmenu,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("enter the batch file transfer menu")
	,310
	},
	{"batch_download",	js_batchdownload,	0,	JSTYPE_BOOLEAN,	JSDOCSTR("")
	,JSDOCSTR("start a batch download")
	,310
	},
	{"batch_add_list",	js_batchaddlist,	1,	JSTYPE_VOID,	JSDOCSTR("list_filename")
	,JSDOCSTR("add file list to batch download queue")
	,310
	},
	{"view_file",		js_viewfile,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("filename")
	,JSDOCSTR("list contents of specified filename (complete path)")
	,319
	},
	{"send_file",		js_sendfile,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("filename [,protocol] [,description] [,autohang=true]")
	,JSDOCSTR("send specified filename (complete path) to user via user-prompted "
		"(or optionally specified) protocol.<br>"
		"The optional <i>description</i> string is used for logging purposes.<br>"
		"When <i>autohang=true</i>, disconnect after transfer based on user's default setting."
	)
	,314
	},
	{"receive_file",	js_recvfile,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("filename [,protocol] [,autohang=true]")
	,JSDOCSTR("received specified filename (complete path) from user via user-prompted "
		"(or optionally specified) protocol.<br>"
		"When <i>autohang=true</i>, disconnect after transfer based on user's default setting."
	)
	,314
	},
	{"temp_xfer",		js_temp_xfer,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("enter the temporary file tranfer menu")
	,310
	},
	{"user_sync",		js_user_sync,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("read the current user data from the database")
	,310
	},
	{"user_config",		js_user_config,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("enter the user settings configuration menu")
	,310
	},
	{"sys_info",		js_sys_info,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("display system information")
	,310
	},
	{"sub_info",		js_sub_info,		1,	JSTYPE_VOID,	JSDOCSTR("[sub-board=<i>current</i>]")
	,JSDOCSTR("display message sub-board information (current <i>sub-board</i>, if unspecified)")
	,310
	},
	{"dir_info",		js_dir_info,		0,	JSTYPE_VOID,	JSDOCSTR("[directory=<i>current</i>]")
	,JSDOCSTR("display file directory information (current <i>directory</i>, if unspecified)")
	,310
	},
	{"user_info",		js_user_info,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("display current user information")
	,310
	},
	{"ver",				js_ver,				0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("display software version information")
	,310
	},
	{"sys_stats",		js_sys_stats,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("display system statistics")
	,310
	},
	{"node_stats",		js_node_stats,		0,	JSTYPE_VOID,	JSDOCSTR("[node_number=<i>current</i>]")
	,JSDOCSTR("display current (or specified) node statistics")
	,310
	},
	{"list_users",		js_userlist,		0,	JSTYPE_VOID,	JSDOCSTR("[mode=<tt>UL_ALL</tt>]")
	,JSDOCSTR("display user list"
	"(see <tt>UL_*</tt> in <tt>sbbsdefs.js</tt> for valid <i>mode</i> values)")
	,310
	},
	{"edit_user",		js_useredit,		0,	JSTYPE_VOID,	JSDOCSTR("[user_number=<i>current</i>]")
	,JSDOCSTR("enter the user editor")
	,310
	},
	{"change_user",		js_change_user,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("change to a different user")
	,310
	},
	{"list_logons",		js_logonlist,		0,	JSTYPE_VOID,	JSDOCSTR("[arguments]")
	,JSDOCSTR("display the logon list (optionally passing arguments to the logon list module)")
	,310
	},
	{"read_mail",		js_readmail,		0,	JSTYPE_VOID,	JSDOCSTR("[which=<tt>MAIL_YOUR</tt>] [,user_number=<i>current</i>] [,loadmail_mode=<tt>0</tt>]")
	,JSDOCSTR("read private e-mail"
	"(see <tt>MAIL_*</tt> in <tt>sbbsdefs.js</tt> for valid <i>which</i> values)")
	,310
	},
	{"email",			js_email,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("to_user_number [,mode=<tt>WM_EMAIL</tt>] [,top=<i>none</i>] [,subject=<i>none</i>] [,object reply_header]")
	,JSDOCSTR("send private e-mail to a local user (<i>reply_header</i> added in v3.17c)")
	,310
	},
	{"netmail",			js_netmail,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("[string address or array of addresses] [,mode=<tt>WM_NONE</tt>] [,subject=<i>none</i>] [,object reply_header]")
	,JSDOCSTR("send private netmail (<i>reply_header</i> added in v3.17c, <i>array of addresses</i> added in v3.18a)")
	,310
	},
	{"bulk_mail",		js_bulkmail,		0,	JSTYPE_VOID,	JSDOCSTR("[ars]")
	,JSDOCSTR("send bulk private e-mail, if <i>ars</i> not specified, prompt for destination users")
	,310
	},
	{"upload_file",		js_upload_file,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("[directory=<i>current</i>]")
	,JSDOCSTR("upload file to file directory specified by number or internal code")
	,310
	},
	{"bulk_upload",		js_bulkupload,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("[directory=<i>current</i>]")
	,JSDOCSTR("add files (already in local storage path) to file directory "
		"specified by number or internal code")
	,310
	},
	{"export_filelist",	js_export_filelist,	2,	JSTYPE_NUMBER,	JSDOCSTR("filename [,mode=<tt>FL_NONE</tt>]")
	,JSDOCSTR("export list of files to a text file, optionally specifying a file list mode (e.g. <tt>FL_ULTIME</tt>), returning the number of files listed")
	,319
	},
	{"list_files",		js_listfiles,		1,	JSTYPE_NUMBER,	JSDOCSTR("[directory=<i>current</i>] [,filespec=<tt>\"*.*\"</tt> or search_string] [,mode=<tt>FL_NONE</tt>]")
	,JSDOCSTR("list files in the specified file directory, "
		"optionally specifying a file specification (wildcards) or a description search string, "
		"and <i>mode</i> (bitfield)")
	,310
	},
	{"list_file_info",	js_listfileinfo,	1,	JSTYPE_NUMBER,	JSDOCSTR("[directory=<i>current</i>] [,filespec=<tt>\"*.*\"</tt>] [,mode=<tt>FI_INFO</tt>]")
	,JSDOCSTR("list extended file information for files in the specified file directory")
	,310
	},
	{"post_msg",		js_post_msg,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("[sub-board=<i>current</i>] [,mode=<tt>WM_NONE</tt>] [,object reply_header]")
	,JSDOCSTR("post a message in the specified message sub-board (number or internal code) "
		"with optional <i>mode</i> (bitfield)<br>"
		"If <i>reply_header</i> is specified (a header object returned from <i>MsgBase.get_msg_header()</i>), that header "
		"will be used for the in-reply-to header fields.")
	,313
	},
	{"forward_msg",		js_forward_msg,		2,	JSTYPE_BOOLEAN,	JSDOCSTR("object header, string to [,string subject] [,string comment]")
	,JSDOCSTR("Forward a message")
	,31802
	},
	{"edit_msg",		js_edit_msg,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("object header")
	,JSDOCSTR("Edit a message")
	,31802
	},
	{"show_msg",		js_show_msg,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("object header [,mode=<tt>P_NONE</tt>] ")
	,JSDOCSTR("show a message's header and body (text) with optional print <i>mode</i> (bitfield)<br>"
		"<i>header</i> must be a header object returned from <i>MsgBase.get_msg_header()</i>)")
	,31702
	},
	{"show_msg_header",	js_show_msg_header,	1,	JSTYPE_VOID,	JSDOCSTR("object header [,subject] [,from] [,to]")
	,JSDOCSTR("show a message's header (only)<br>"
		"<i>header</i> must be a header object returned from <i>MsgBase.get_msg_header()</i>)")
	,31702
	},
	{"download_msg_attachments", js_download_msg_attachments, 1, JSTYPE_VOID, JSDOCSTR("object header")
	,JSDOCSTR("prompt the user to download each of the message's file attachments (if there are any)<br>"
		"<i>header</i> must be a header object returned from <i>MsgBase.get_msg_header()</i>)")
	,31702
	},
	{"change_msg_attr", js_change_msg_attr, 1, JSTYPE_NUMBER, JSDOCSTR("object header")
	,JSDOCSTR("prompt the user to modify the specified message header attributes")
	,31702
	},
	{"cfg_msg_scan",	js_msgscan_cfg,		0,	JSTYPE_VOID,	JSDOCSTR("[type=<tt>SCAN_CFG_NEW</tt>]")
	,JSDOCSTR("configure message scan "
		"(<i>type</i> is either <tt>SCAN_CFG_NEW</tt> or <tt>SCAN_CFG_TOYOU</tt>)")
	,310
	},
	{"cfg_msg_ptrs",	js_msgscan_ptrs,	0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("change message scan pointer values")
	,310
	},
	{"reinit_msg_ptrs",	js_msgscan_reinit,	0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("re-initialize new message scan pointers")
	,310
	},
	{"scan_subs",		js_scansubs,		0,	JSTYPE_VOID,	JSDOCSTR("[mode=<tt>SCAN_NEW</tt>] [,all=<tt>false</tt>]")
	,JSDOCSTR("scan sub-boards for messages")
	,310
	},
	{"scan_dirs",		js_scandirs,		0,	JSTYPE_VOID,	JSDOCSTR("[mode=<tt>FL_NONE</tt>] [,all=<tt>false</tt>]")
	,JSDOCSTR("scan directories for files")
	,310
	},
	{"scan_posts",		js_scanposts,		1,	JSTYPE_ALIAS },
	{"scan_msgs",		js_scanposts,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("[sub-board=<i>current</i>] [,mode=<tt>SCAN_READ</tt>] [,find]")
	,JSDOCSTR("scan messages in the specified message sub-board (number or internal code), "
		"optionally search for 'find' string (AKA scan_posts)")
	,310
	},
	{"list_msgs",		js_listmsgs,		1,	JSTYPE_NUMBER,	JSDOCSTR("[sub-board=<i>current</i>] [,mode=<tt>SCAN_INDEX</tt>] [,message_number=<tt>0</tt>] [,find]")
	,JSDOCSTR("list messages in the specified message sub-board (number or internal code), "
		"optionally search for 'find' string, returns number of messages listed")
	,314
	},
	/* menuing */
	{"menu",			js_menu,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("base_filename [,mode=<tt>P_NONE</tt>] [,object scope]")
	,JSDOCSTR("display a menu file from the text/menu directory.<br>"
	"See <tt>P_*</tt> in <tt>sbbsdefs.js</tt> for <i>mode</i> flags.<br>"
	"When <i>scope</i> is specified, <tt>@JS:property@</tt> codes will expand the referenced property names.<br>"
	"To display a randomly-chosen menu file, including wild-card (* or ?) characters in the <tt>base_filename</tt>.")
	,310
	},
	{"menu_exists",		js_menu_exists,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("base_filename")
	,JSDOCSTR("returns true if the referenced menu file exists (i.e. in the text/menu directory)")
	,31700
	},
	{"log_key",			js_logkey,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("key [,comma=<tt>false</tt>]")
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
	,JSDOCSTR("search file for pseudo-regexp (search string) in trashcan file (text/base_filename.can)")
	,310
	},
	/* xtrn programs/modules */
	{"exec",			js_exec,			2,	JSTYPE_NUMBER,	JSDOCSTR("cmdline [,mode=<tt>EX_NONE</tt>] [,startup_dir]")
	,JSDOCSTR("execute a program, optionally changing current directory to <i>startup_dir</i> "
	"(see <tt>EX_*</tt> in <tt>sbbsdefs.js</tt> for valid <i>mode</i> flags.)")
	,310
	},
	{"exec_xtrn",		js_exec_xtrn,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("xtrn_number_or_code")
	,JSDOCSTR("execute external program by number or internal code")
	,310
	},
	{"user_event",		js_user_event,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("event_type")
	,JSDOCSTR("execute user event by event type "
	"(see <tt>EVENT_*</tt> in <tt>sbbsdefs.js</tt> for valid values)")
	,310
	},
	{"telnet_gate",		js_telnet_gate,		1,	JSTYPE_VOID,	JSDOCSTR("address [,mode=<tt>TG_NONE</tt>]")
	,JSDOCSTR("external Telnet gateway (see <tt>TG_*</tt> in <tt>sbbsdefs.js</tt> for valid <i>mode</i> flags.)")
	,310
	},
	{"rlogin_gate",		js_rlogin_gate,		1,	JSTYPE_VOID,	JSDOCSTR("address [,client-user-name=<tt>user.alias</tt>, server-user-name=<tt>user.name</tt>, terminal=<tt>console.terminal</tt>] [,mode=<tt>TG_NONE</tt>]")
	,JSDOCSTR("external RLogin gateway (see <tt>TG_*</tt> in <tt>sbbsdefs.js</tt> for valid <i>mode</i> flags.)")
	,316
	},
	/* security */
	{"check_syspass",	js_chksyspass,		0,	JSTYPE_BOOLEAN,	JSDOCSTR("[sys_pw]")
	,JSDOCSTR("verify system password, prompting for the password if not passed as an argument")
	,310
	},
	{"good_password",	js_chkpass,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("password")
	,JSDOCSTR("check if requested user password meets minimum password requirements "
		"(length, uniqueness, etc.)")
	,310
	},
	/* chat/node stuff */
	{"page_sysop",		js_pagesysop,		0,	JSTYPE_BOOLEAN,	JSDOCSTR("")
	,JSDOCSTR("page the sysop for chat, returns <i>false</i> if the sysop could not be paged")
	,310
	},
	{"page_guru",		js_pageguru,		0,	JSTYPE_BOOLEAN,	JSDOCSTR("")
	,JSDOCSTR("page the guru for chat")
	,310
	},
	{"multinode_chat",	js_multinode_chat,	0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("enter multi-node chat")
	,310
	},
	{"private_message",	js_private_message,	0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("use the private inter-node message prompt")
	,310
	},
	{"private_chat",	js_private_chat,	0,	JSTYPE_VOID,	JSDOCSTR("[local=<i>false</i>]")
	,JSDOCSTR("enter private inter-node chat, or local sysop chat (if <i>local</i>=<i>true</i>)")
	,310
	},
	{"get_node_message",js_get_node_message,1,	JSTYPE_VOID,	JSDOCSTR("[clearline=<i>false</i>]")
	,JSDOCSTR("receive and display an inter-node message")
	,310
	},
	{"put_node_message",js_put_node_message,2,	JSTYPE_BOOLEAN,	JSDOCSTR("[node_number] [,text]")
	,JSDOCSTR("send an inter-node message (specify a <i>node_number</i> value of <tt>-1</tt> for 'all active nodes')")
	,31700
	},
	{"get_telegram",	js_get_telegram,	2,	JSTYPE_VOID,	JSDOCSTR("[user_number=<i>current</i>], [clearline=<i>false</i>]")
	,JSDOCSTR("receive and display waiting telegrams for specified (or current) user")
	,310
	},
	{"put_telegram",	js_put_telegram,	2,	JSTYPE_BOOLEAN,	JSDOCSTR("[user_number] [,text]")
	,JSDOCSTR("send a telegram (short multi-line stored message) to a user")
	,31700
	},
	{"list_nodes",		js_nodelist,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("list all nodes")
	,310
	},
	{"whos_online",		js_whos_online,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("list active nodes only (who's online)")
	,310
	},
	{"spy",				js_spy,				1,	JSTYPE_VOID,	JSDOCSTR("node_number")
	,JSDOCSTR("spy on a node")
	,310
	},
	/* misc */
	{"cmdstr",			js_cmdstr,			1,	JSTYPE_STRING,	JSDOCSTR("command_string [,fpath=<tt>\"\"</tt>] [,fspec=<tt>\"\"</tt>]")
	,JSDOCSTR("return expanded command string using Synchronet command-line specifiers")
	,310
	},
	/* input */
	{"get_filespec",	js_getfilespec,		0,	JSTYPE_STRING,	JSDOCSTR("")
	,JSDOCSTR("returns a file specification input by the user (optionally with wildcards)")
	,310
	},
	{"get_newscantime",	js_getnstime,		1,	JSTYPE_NUMBER,	JSDOCSTR("time=<i>current</i>")
	,JSDOCSTR("confirm or change a new-scan time, returns the new new-scan time value (<i>time_t</i> format)")
	,310
	},
	{"select_shell",	js_select_shell,	0,	JSTYPE_BOOLEAN,	JSDOCSTR("")
	,JSDOCSTR("prompt user to select a new command shell")
	,310
	},
	{"select_editor",	js_select_editor,	0,	JSTYPE_BOOLEAN,	JSDOCSTR("")
	,JSDOCSTR("prompt user to select a new external message editor")
	,310
	},
	{"get_time_left",	js_get_time_left,	0,	JSTYPE_NUMBER,	JSDOCSTR("")
	,JSDOCSTR("check the user's available remaining time online and return the value, in seconds<br>"
	"This method will inform (and disconnect) the user when they are out of time")
	,31401
	},
	{"compare_ars",		js_chk_ar,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("ars")
	,JSDOCSTR("verify the current user online meets the specified Access Requirements String")
	,315
	},
	{"select_node",		js_select_node,		1,	JSTYPE_NUMBER,	JSDOCSTR("all_is_an_option=<i>false</i>")
	,JSDOCSTR("Choose an active node to interact with.<br>Returns the selected node number, 0 (for none) or -1 for 'All'.")
	,31700
	},
	{"select_user",		js_select_user,		1,	JSTYPE_NUMBER,	JSDOCSTR("")
	,JSDOCSTR("Choose a user to interact with.")
	,31700
	},
	{0}
};

static JSBool js_bbs_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSBool			ret;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;

		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	ret=js_SyncResolve(cx, obj, name, js_bbs_properties, js_bbs_functions, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_bbs_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_bbs_resolve(cx, obj, JSID_VOID));
}

JSClass js_bbs_class = {
     "BBS"					/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_bbs_get				/* getProperty	*/
	,js_bbs_set				/* setProperty	*/
	,js_bbs_enumerate		/* enumerate	*/
	,js_bbs_resolve			/* resolve		*/
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

	JS_SetPrivate(cx, obj, JS_GetContextPrivate(cx));

	if((mods=JS_DefineObject(cx, obj, "mods", NULL, NULL ,JSPROP_ENUMERATE))==NULL)
		return(NULL);

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,mods,"Global repository for 3rd party modifications",312);
	js_DescribeSyncObject(cx,obj,"Controls the Telnet/SSH/RLogin BBS experience",310);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", bbs_prop_desc, JSPROP_READONLY);
#endif

	return(obj);
}

#endif	/* JAVSCRIPT */
