/* Synchronet Message-ID generation routines */

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

#include "msg_id.h"
#include "smblib.h"
#include "scfglib.h"
#include "git_branch.h"
#include "git_hash.h"

static ulong msg_number(smbmsg_t* msg)
{
	if(msg->idx.number)
		return(msg->idx.number);
	return(msg->hdr.number);
}

uint32_t get_new_msg_number(smb_t* smb)
{
	BOOL locked = smb->locked;

	if(!locked && smb_locksmbhdr(smb) != SMB_SUCCESS)
		return 0;

	if(smb_getstatus(smb)!=SMB_SUCCESS)
		return 0;

	if(!locked) smb_unlocksmbhdr(smb);
	return smb->status.last_msg + 1;
}

static ulong msg_time(smbmsg_t* msg)
{
	if(msg->idx.time)
		return(msg->idx.time);
	return(msg->hdr.when_imported.time);
}

static ulong msgid_serialno(smbmsg_t* msg)
{
	return (msg_time(msg)-1000000000) + msg_number(msg);
}

/****************************************************************************/
/* Returns a FidoNet (FTS-9) message-ID										*/
/* Returns NULL if the message is from FidoNet and doesn't have a MSGID		*/
/* Pass NULL for msgid if (single-threaded) caller wishes to use static buf	*/
/****************************************************************************/
char* ftn_msgid(sub_t *sub, smbmsg_t* msg, char* msgid, size_t maxlen)
{
	static char msgidbuf[256];
	
	if(msgid == NULL) {
		msgid = msgidbuf;
		maxlen = sizeof(msgidbuf);
	}
	if(msg->ftn_msgid!=NULL && *msg->ftn_msgid!=0)
		return msg->ftn_msgid;

	if(msg->from_net.type == NET_FIDO)	// Don't generate a message-ID for imported FTN messages
		return NULL;

	safe_snprintf(msgid,maxlen
		,"%lu.%s@%s %08lx"
		,msg_number(msg)
		,sub->code
		,smb_faddrtoa(&sub->faddr,NULL)
		,msgid_serialno(msg)
		);

	return msgid;
}

/****************************************************************************/
/* Return a general purpose (RFC-822) message-ID							*/
/****************************************************************************/
char* get_msgid(scfg_t* cfg, uint subnum, smbmsg_t* msg, char* msgid, size_t maxlen)
{
	char*	host;

	if(msg->id!=NULL && *msg->id!=0) {
		strncpy(msgid,msg->id,maxlen);
		return(msg->id);
	}

	/* Try *really* hard to get a hostname from the configuration data */
	host=cfg->sys_inetaddr;
	if(!host[0]) {
		host=cfg->sys_id;
		if(!host[0]) {
			host=cfg->sys_name;
			if(!host[0]) {
				host=cfg->sys_op;
			}
		}
	}

	if(subnum>=cfg->total_subs)
		safe_snprintf(msgid,maxlen
			,"<%08lX.%lu@%s>"
			,msg_time(msg)
			,msg_number(msg)
			,host);
	else
		safe_snprintf(msgid,maxlen
			,"<%08lX.%lu.%s@%s>"
			,msg_time(msg)
			,msg_number(msg)
			,cfg->sub[subnum]->code
			,host);

	return(msgid);
}

/****************************************************************************/
/* Get (or generate) the original message-ID for a reply message			*/
/* Returns NULL if not a valid reply message								*/
/****************************************************************************/
char* get_replyid(scfg_t* cfg, smb_t* smb, smbmsg_t* msg, char* msgid, size_t maxlen)
{
	smbmsg_t remsg;

	if(msg->reply_id)
		return msg->reply_id;
	if(msg->hdr.thread_back == 0)
		return NULL;

	memset(&remsg, 0, sizeof(remsg));
	remsg.hdr.number=msg->hdr.thread_back;
	if(smb_getmsgidx(smb, &remsg) != SMB_SUCCESS)
		return NULL;
	if(smb_getmsghdr(smb, &remsg) != SMB_SUCCESS)
		return NULL;
	get_msgid(cfg, smb->subnum, &remsg, msgid, maxlen);
	smb_freemsgmem(&remsg);

	return msgid;
}

/****************************************************************************/
/* Add auto-generated message-IDs to a message, if doesn't already have		*/
/* The message base (smb) must be already opened							*/
/****************************************************************************/
BOOL add_msg_ids(scfg_t* cfg, smb_t* smb, smbmsg_t* msg, smbmsg_t* remsg)
{
	char msg_id[256];

	if(msg->hdr.number == 0)
		msg->hdr.number = get_new_msg_number(smb);

 	/* Generate FidoNet (FTS-9) MSGID (for messages posted to FTN sub-boards only) */
 	if(msg->ftn_msgid == NULL) {
		if(smb->subnum == INVALID_SUB && msg->to_net.type == NET_FIDO) {
			safe_snprintf(msg_id, sizeof(msg_id)
				,"%s %08lx"
				,smb_faddrtoa(nearest_sysfaddr(cfg, msg->to_net.addr), NULL)
				,msgid_serialno(msg)
				);
			if(smb_hfield_str(msg, FIDOMSGID, msg_id) != SMB_SUCCESS)
				return FALSE;
		}
		else if(smb->subnum != INVALID_SUB && cfg->sub[smb->subnum]->misc&SUB_FIDO) {
			if(ftn_msgid(cfg->sub[smb->subnum], msg, msg_id, sizeof(msg_id)) != NULL) {
				if(smb_hfield_str(msg, FIDOMSGID, msg_id) != SMB_SUCCESS)
					return FALSE;
			}
		}
 	}

	/* Generate Internet MSG-ID (for all messages) */
	if(msg->id == NULL) {
 		get_msgid(cfg, smb->subnum, msg, msg_id, sizeof(msg_id));
 		if(smb_hfield_str(msg, RFC822MSGID, msg_id) != SMB_SUCCESS)
			return FALSE;
	}

	/* Generate Reply-IDs (when appropriate) */
	if(remsg != NULL) {
		if(add_reply_ids(cfg, smb, msg, remsg) != TRUE)
			return FALSE;
	}

	/* Generate FidoNet Program Identifier */
 	if(msg->ftn_pid == NULL) {
		if(smb_hfield_str(msg, FIDOPID, msg_program_id(msg_id, sizeof(msg_id))) != TRUE)
			return FALSE;
	}

	return TRUE;	// Success
}

/****************************************************************************/
/* Adds reply-IDs and does some reply/thread-linkage to a new message		*/
/* Migrated from sbbs_t::postmsg()											*/
/* The message base (smb) must be already opened successfully				*/
/****************************************************************************/
BOOL add_reply_ids(scfg_t* cfg, smb_t* smb, smbmsg_t* msg, smbmsg_t* remsg)
{
	char* p;
	char replyid[256];

	msg->hdr.thread_back = remsg->hdr.number;	/* needed for threading backward */

	if((msg->hdr.thread_id = remsg->hdr.thread_id) == 0)
		msg->hdr.thread_id = remsg->hdr.number;

	/* Add RFC-822 Reply-ID (generate if necessary) */
	if((p = get_replyid(cfg, smb, msg, replyid, sizeof(replyid))) != NULL) {
		if(smb_hfield_str(msg, RFC822REPLYID, p) != SMB_SUCCESS)
			return FALSE;
	}

	/* Add FidoNet Reply if original message has FidoNet MSGID */
	if(remsg->ftn_msgid != NULL) {
		if(smb_hfield_str(msg, FIDOREPLYID, remsg->ftn_msgid) != SMB_SUCCESS)
			return FALSE;
	}

	return TRUE;	// Success
}

/****************************************************************************/
/* FTN-compliant "Program Identifier"/PID									*/
/****************************************************************************/
char* msg_program_id(char* pid, size_t maxlen)
{
	char compiler[64];

	DESCRIBE_COMPILER(compiler);
	snprintf(pid, maxlen, "%.10s %s%c-%s %s/%s %s %s"
		,VERSION_NOTICE,VERSION,REVISION,PLATFORM_DESC
		,GIT_BRANCH, GIT_HASH
		,__DATE__,compiler);
	return pid;
}
