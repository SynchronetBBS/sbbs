/* Synchronet DLL-exported mail-related routines */

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

#include "getmail.h"
#include "ini_file.h"

/****************************************************************************/
/* Returns the number of pieces of mail waiting for usernumber              */
/* If sent is non-zero, it returns the number of mail sent by usernumber    */
/* If usernumber is 0, it returns all mail on the system                    */
/****************************************************************************/
int getmail(scfg_t* cfg, int usernumber, bool sent, int attr)
{
	char     path[MAX_PATH + 1];
	int      i = 0;
	long     l;
	idxrec_t idx;
	smb_t    smb;

	ZERO_VAR(smb);
	SAFEPRINTF(smb.file, "%smail", cfg->data_dir);
	smb.retry_time = 1;   //cfg->smb_retry_time;
	SAFEPRINTF(path, "%s.sid", smb.file);
	l = (long)flength(path);
	if (l < (long)sizeof(idxrec_t))
		return 0;
	if (usernumber == 0 && attr == 0)
		return l / sizeof(idxrec_t);  /* Total system e-mail */
	smb.subnum = INVALID_SUB;

	if (smb_open_fp(&smb, &smb.sid_fp, SH_DENYNO) != 0)
		return 0;

	while (!smb_feof(smb.sid_fp)) {
		if (smb_fread(&smb, &idx, sizeof(idx), smb.sid_fp) != sizeof(idx))
			break;
		if (idx.number == 0)   /* invalid message number, ignore */
			continue;
		if (idx.attr & MSG_DELETE)
			continue;
		if (attr < 0 && (idx.attr & (~attr)) != 0)
			continue;
		if (attr > 0 && (idx.attr & attr) != attr)
			continue;
		if (usernumber == 0
		    || (!sent && idx.to == usernumber)
		    || (sent && idx.from == usernumber))
			i++;
	}
	smb_close(&smb);
	return i;
}

/****************************************************************************/
/* The index records of the undeleted mail waiting for usernumber (spam     */
/* included, as getmail() counts it): returns their number and, in *list,   */
/* a malloc'd array of them (NULL when there are none)                      */
/****************************************************************************/
static uint mail_waiting(scfg_t* cfg, uint usernumber, idxrec_t** list)
{
	char      path[MAX_PATH + 1];
	uint      count = 0;
	idxrec_t  idx;
	idxrec_t* np;
	smb_t     smb;

	*list = NULL;
	ZERO_VAR(smb);
	SAFEPRINTF(smb.file, "%smail", cfg->data_dir);
	smb.retry_time = 1;
	SAFEPRINTF(path, "%s.sid", smb.file);
	if (flength(path) < (off_t)sizeof(idxrec_t))
		return 0;
	smb.subnum = INVALID_SUB;
	if (smb_open_fp(&smb, &smb.sid_fp, SH_DENYNO) != 0)
		return 0;
	while (!smb_feof(smb.sid_fp)) {
		if (smb_fread(&smb, &idx, sizeof(idx), smb.sid_fp) != sizeof(idx))
			break;
		if (idx.number == 0 || (idx.attr & MSG_DELETE) || idx.to != usernumber)
			continue;
		if ((count % 100) == 0) {
			if ((np = realloc(*list, sizeof(idxrec_t) * (count + 100))) == NULL)
				break;
			*list = np;
		}
		(*list)[count++] = idx;
	}
	smb_close(&smb);
	return count;
}

/****************************************************************************/
/* The space the listed mail holds in the mail base: each message's header  */
/* and data, in whole blocks (data shared with other recipients' copies is  */
/* counted in full for each).  Only the start of each header is read.       */
/* Stops adding once the sum, begun at 'total', reaches a non-zero stop_at. */
/* A header that cannot be read, is corrupt, or is no longer that message,  */
/* counts 0.                                                                */
/****************************************************************************/
static uint64_t mail_stored_bytes(scfg_t* cfg, const idxrec_t* list, uint count, uint64_t total, uint64_t stop_at)
{
	char     path[MAX_PATH + 1];
	uchar    buf[SHD_BLOCK_LEN];
	int      file;
	int      rd;
	uint     i;
	uint     d;
	uint     fields;
	off_t    datlen;
	msghdr_t hdr;
	dfield_t dfield;

	SAFEPRINTF(path, "%smail.shd", cfg->data_dir);
	if ((file = sopen(path, O_RDONLY | O_BINARY, SH_DENYNO)) == -1)
		return total;
	for (i = 0; i < count && (stop_at == 0 || total < stop_at); i++) {
		if (lseek(file, list[i].offset, SEEK_SET) == -1)
			continue;
		if ((rd = read(file, buf, sizeof buf)) < (int)sizeof(hdr))
			continue;
		memcpy(&hdr, buf, sizeof hdr);
		if (memcmp(hdr.msghdr_id, SHD_HEADER_ID, LEN_HEADER_ID) != 0
		    || hdr.number != list[i].number
		    || (hdr.attr & MSG_DELETE))
			continue;
		/* The data fields lie within the header record: a count that does
		 * not fit its length is corrupt */
		if (hdr.length < sizeof(hdr)
		    || hdr.total_dfields > (hdr.length - sizeof(hdr)) / sizeof(dfield))
			continue;
		fields = (uint)((rd - sizeof(hdr)) / sizeof(dfield));
		datlen = 0;
		for (d = 0; d < hdr.total_dfields; d++) {
			if (d < fields)
				memcpy(&dfield, buf + sizeof(hdr) + (d * sizeof(dfield)), sizeof dfield);
			else {
				/* More data fields than the block read holds */
				if (d == fields
				    && lseek(file, (off_t)list[i].offset + (off_t)(sizeof(hdr) + (d * sizeof(dfield))), SEEK_SET) == -1)
					break;
				if (read(file, &dfield, sizeof dfield) != sizeof dfield)
					break;
			}
			if (dfield.length > 0 && (off_t)dfield.offset + dfield.length > datlen)
				datlen = (off_t)dfield.offset + dfield.length;
		}
		total += (uint64_t)smb_hdrblocks(hdr.length) * SHD_BLOCK_LEN
		         + (uint64_t)smb_datblocks(datlen) * SDT_BLOCK_LEN;
	}
	close(file);
	return total;
}

/****************************************************************************/
/* The space the user's private mail base (folders, Sent copies and Trash,  */
/* kept by exec/load/mailbox.js) holds: the 'bytes' value that library      */
/* writes to mailbox.usage.ini beside the base; without that value, the     */
/* sizes of the base's header and data files (never less than the space in  */
/* use); 0 when the user has no private base.                               */
/****************************************************************************/
static uint64_t private_mailbox_bytes(scfg_t* cfg, uint usernumber)
{
	char    path[MAX_PATH + 1];
	FILE*   fp;
	off_t   shd;
	off_t   sdt;
	int64_t bytes = -1;
	char    value[INI_MAX_VALUE_LEN];

	SAFEPRINTF2(path, "%suser/%04u/mailbox.shd", cfg->data_dir, usernumber);
	if ((shd = flength(path)) < 0)
		return 0;
	SAFEPRINTF2(path, "%suser/%04u/mailbox.usage.ini", cfg->data_dir, usernumber);
	if ((fp = iniOpenFile(path, /* for_modify: */ false)) != NULL) {
		/* Anything but a plain number (a damaged file) is not believed */
		if (iniReadString(fp, ROOT_SECTION, "bytes", "", value) != NULL
		    && value[0] != '\0' && strlen(value) <= 18 && strspn(value, "0123456789") == strlen(value))
			bytes = strtoll(value, NULL, 10);
		iniCloseFile(fp);
	}
	if (bytes >= 0)
		return bytes;
	SAFEPRINTF2(path, "%suser/%04u/mailbox.sdt", cfg->data_dir, usernumber);
	sdt = flength(path);
	return (uint64_t)shd + (sdt > 0 ? sdt : 0);
}

/****************************************************************************/
/* The bytes of mail counted against cfg->mailbox_quota for usernumber: the */
/* private mail base plus the mail waiting in the Inbox (spam included;     */
/* files attached to Inbox mail are not counted).                           */
/****************************************************************************/
uint64_t user_mailbox_bytes(scfg_t* cfg, uint usernumber)
{
	idxrec_t* list;
	uint      count;
	uint64_t  total;

	if (usernumber == 0)
		return 0;
	count = mail_waiting(cfg, usernumber, &list);
	total = mail_stored_bytes(cfg, list, count, private_mailbox_bytes(cfg, usernumber), /* stop_at: */ 0);
	free(list);
	return total;
}

/****************************************************************************/
/* Tells whether new mail for 'user' is to be refused, and why:             */
/* MAILBOX_FULL_MSGS with more than cfg->max_msgs_waiting (0 = no limit)    */
/* messages in the Inbox (spam included, as counted by getmail()), else     */
/* MAILBOX_FULL_BYTES when the user's mail (see user_mailbox_bytes()) has   */
/* reached cfg->mailbox_quota (0 = no quota); the size of the new message   */
/* is not counted.  A user with the W exemption is subject to neither.      */
/* Mail from the system itself (system_sender) is never refused.            */
/* 'waiting' (may be NULL) gets the message count when it was taken, else 0 */
/****************************************************************************/
enum mailbox_full user_mailbox_full(scfg_t* cfg, const user_t* user, bool system_sender, uint* waiting)
{
	uint              count;
	idxrec_t*         list;
	uint64_t          bytes;
	enum mailbox_full result = MAILBOX_NOT_FULL;

	if (waiting != NULL)
		*waiting = 0;
	if (system_sender || user->number == 0 || (user->exempt & UEXEMPT_MAIL_WAITING)
	    || (cfg->max_msgs_waiting == 0 && cfg->mailbox_quota == 0))
		return MAILBOX_NOT_FULL;
	count = mail_waiting(cfg, user->number, &list);
	if (waiting != NULL)
		*waiting = count;
	if (cfg->max_msgs_waiting > 0 && count > cfg->max_msgs_waiting)
		result = MAILBOX_FULL_MSGS;
	else if (cfg->mailbox_quota > 0) {
		bytes = private_mailbox_bytes(cfg, user->number);
		if (bytes < cfg->mailbox_quota)
			bytes = mail_stored_bytes(cfg, list, count, bytes, /* stop_at: */ cfg->mailbox_quota);
		if (bytes >= cfg->mailbox_quota)
			result = MAILBOX_FULL_BYTES;
	}
	free(list);
	return result;
}


/****************************************************************************/
/* Permanently removes all mail sent to or from 'usernumber'                */
/* Intended for use when a user record is created over a deleted user's     */
/* record, so that the new user does not inherit the previous user's mail   */
/* (and mail sent by the previous user is not attributed to the new user).  */
/* Returns the number of messages removed                                   */
/****************************************************************************/
int delusermail(scfg_t* cfg, uint usernumber)
{
	int       removed = 0;
	uint32_t  i;
	uint32_t  kept = 0;
	idxrec_t *idxbuf;
	smb_t     smb;
	smbmsg_t  msg;

	if (cfg == NULL || usernumber < 1)
		return 0;

	ZERO_VAR(smb);
	SAFEPRINTF(smb.file, "%smail", cfg->data_dir);
	smb.retry_time = cfg->smb_retry_time;
	smb.subnum = INVALID_SUB;

	if (smb_open(&smb) != SMB_SUCCESS)
		return 0;
	if (smb_lock(&smb) != SMB_SUCCESS) {
		smb_close(&smb);
		return 0;
	}
	if (smb_locksmbhdr(&smb) != SMB_SUCCESS) {
		smb_unlock(&smb);
		smb_close(&smb);
		return 0;
	}
	if (smb_getstatus(&smb) != SMB_SUCCESS || smb.status.total_msgs < 1) {
		smb_unlocksmbhdr(&smb);
		smb_unlock(&smb);
		smb_close(&smb);
		return 0;
	}
	if ((idxbuf = (idxrec_t *)malloc(smb.status.total_msgs * sizeof(idxrec_t))) == NULL) {
		smb_unlocksmbhdr(&smb);
		smb_unlock(&smb);
		smb_close(&smb);
		return 0;
	}
	if (smb_open_da(&smb) != SMB_SUCCESS) {
		free(idxbuf);
		smb_unlocksmbhdr(&smb);
		smb_unlock(&smb);
		smb_close(&smb);
		return 0;
	}
	if (smb_open_ha(&smb) != SMB_SUCCESS) {
		smb_close_da(&smb);
		free(idxbuf);
		smb_unlocksmbhdr(&smb);
		smb_unlock(&smb);
		smb_close(&smb);
		return 0;
	}

	smb_rewind(smb.sid_fp);
	for (i = 0; i < smb.status.total_msgs; i++) {
		ZERO_VAR(msg);
		if (smb_fread(&smb, &msg.idx, sizeof(idxrec_t), smb.sid_fp) != sizeof(idxrec_t))
			break;
		/* Note: netmail (idx.to == 0) is never matched (usernumber is never 0 here) */
		if (msg.idx.to != usernumber && msg.idx.from != usernumber) {
			idxbuf[kept++] = msg.idx;
			continue;
		}
		/* Don't need to lock the message because the base is locked */
		if (smb_getmsghdr(&smb, &msg) != SMB_SUCCESS) {
			idxbuf[kept++] = msg.idx;
			continue;
		}
		msg.hdr.attr |= MSG_DELETE;
		msg.hdr.attr &= ~MSG_PERMANENT;
		msg.idx.attr = msg.hdr.attr;
		if (smb_putmsghdr(&smb, &msg) == SMB_SUCCESS
		    && smb_freemsg(&smb, &msg) == SMB_SUCCESS) {
			if (msg.hdr.auxattr & MSG_FILEATTACH)
				delfattach(cfg, &msg);
			removed++;
		} else
			idxbuf[kept++] = msg.idx;
		smb_freemsgmem(&msg);
	}

	smb_rewind(smb.sid_fp);
	for (i = 0; i < kept; i++) {
		if (smb_fwrite(&smb, &idxbuf[i], sizeof(idxrec_t), smb.sid_fp) != sizeof(idxrec_t))
			break;
	}
	smb_fflush(smb.sid_fp);
	smb_fsetlength(smb.sid_fp, i * sizeof(idxrec_t));
	smb.status.total_msgs = i;
	smb_putstatus(&smb);
	free(idxbuf);
	smb_close_ha(&smb);
	smb_close_da(&smb);
	smb_unlocksmbhdr(&smb);
	smb_unlock(&smb);
	smb_close(&smb);
	return removed;
}

/***************************/
/* Delete file attachments */
/***************************/
bool delfattach(scfg_t* cfg, smbmsg_t* msg)
{
	char  dir[MAX_PATH + 1];
	char  path[MAX_PATH + 1];
	char  files[128];
	char *tp, *sp, *p;

	if (msg->idx.to == 0)  /* netmail */
		SAFEPRINTF2(dir, "%sfile/%04u.out", cfg->data_dir, msg->idx.from);
	else
		SAFEPRINTF2(dir, "%sfile/%04u.in", cfg->data_dir, msg->idx.to);

	SAFECOPY(files, msg->subj);
	tp = files;
	while (1) {
		p = strchr(tp, ' ');
		if (p)
			*p = 0;
		sp = strrchr(tp, '/');              /* sp is slash pointer */
		if (!sp)
			sp = strrchr(tp, '\\');
		if (sp)
			tp = sp + 1;
		if (strcspn(tp, ILLEGAL_FILENAME_CHARS) == strlen(tp)) {
			SAFEPRINTF2(path, "%s/%s", dir, tp);
			if (fexist(path) && remove(path) != 0)
				return false;
		}
		if (!p)
			break;
		tp = p + 1;
	}
	rmdir(dir);                     /* remove the dir if it's empty */
	return true;
}

/****************************************************************************/
/* Adds a local copy of an e-mail message just saved for forwarding to a    */
/* user's netmail address (when MM_FWDKEEP is set): a second header,        */
/* addressed to local user 'usernumber', that shares the forwarded          */
/* message's data (the data's reference count is raised by one).  The copy  */
/* has all of the forwarded header's fields except the recipient's net type */
/* and address.  Returns an SMB_* result code.                              */
/****************************************************************************/
int add_fwd_mail_copy(smb_t* smb, smbmsg_t* fwdmsg, uint usernumber, int storage)
{
	char     str[16];
	int      i;
	smbmsg_t msg;

	memset(&msg, 0, sizeof msg);
	msg.hdr = fwdmsg->hdr;
	msg.flags = fwdmsg->flags;
	if (msg.hdr.thread_id == fwdmsg->hdr.number)
		msg.hdr.thread_id = 0;   /* the copy starts its own thread */
	msg.hdr.thread_next = 0;
	msg.hdr.thread_first = 0;
	if (msg.hdr.total_dfields > 0) {
		if ((msg.dfield = malloc(msg.hdr.total_dfields * sizeof(*msg.dfield))) == NULL)
			return SMB_ERR_MEM;
		memcpy(msg.dfield, fwdmsg->dfield, msg.hdr.total_dfields * sizeof(*msg.dfield));
	}
	for (i = 0; i < fwdmsg->total_hfields; i++) {
		switch (fwdmsg->hfield[i].type) {
			case RECIPIENTNETTYPE:
			case RECIPIENTNETADDR:
			case RECIPIENTEXT:
				continue;
		}
		if (smb_hfield_add(&msg, fwdmsg->hfield[i].type, fwdmsg->hfield[i].length
		                   , fwdmsg->hfield_dat[i], /* insert: */ false) != SMB_SUCCESS) {
			smb_freemsgmem(&msg);
			return SMB_ERR_MEM;
		}
	}
	SAFEPRINTF(str, "%u", usernumber);
	if ((i = smb_hfield_str(&msg, RECIPIENTEXT, str)) == SMB_SUCCESS
	    && (i = smb_incmsg_dfields(smb, &msg, 1)) == SMB_SUCCESS) {
		/* The reference is added first: a header must never outlive its data */
		if ((i = smb_addmsghdr(smb, &msg, storage)) != SMB_SUCCESS)
			smb_freemsg_dfields(smb, &msg, 1);
	}
	smb_freemsgmem(&msg);
	return i;
}

/****************************************************************************/
/* Loads mail waiting for user number 'usernumber' into the mail array of   */
/* of pointers to mail_t (message numbers and attributes)                   */
/* smb_open(&smb) must be called prior										*/
/****************************************************************************/
mail_t* loadmail(smb_t* smb, uint32_t* msgs, uint usernumber
                 , int which, long mode)
{
	ulong    l = 0;
	idxrec_t idx;
	mail_t*  mail = NULL;

	if (msgs == NULL)
		return NULL;

	*msgs = 0;

	if (smb == NULL)
		return NULL;

	if (smb_locksmbhdr(smb) != 0)                  /* Be sure noone deletes or */
		return NULL;                          /* adds while we're reading */

	smb_rewind(smb->sid_fp);
	while (!smb_feof(smb->sid_fp)) {
		if (smb_fread(smb, &idx, sizeof(idx), smb->sid_fp) != sizeof(idx))
			break;
		if (idx.number == 0)   /* invalid message number, ignore */
			continue;
		if ((which == MAIL_SENT && idx.from != usernumber)
		    || (which == MAIL_YOUR && idx.to != usernumber)
		    || (which == MAIL_ANY && idx.from != usernumber && idx.to != usernumber))
			continue;
		if (idx.attr & MSG_DELETE && !(mode & LM_INCDEL))    /* Don't included deleted msgs */
			continue;
		if (mode & LM_UNREAD && idx.attr & MSG_READ)
			continue;
		if (mode & LM_NOSPAM && idx.attr & MSG_SPAM)
			continue;
		if (mode & LM_SPAMONLY && !(idx.attr & MSG_SPAM))
			continue;
		mail_t* np;
		if ((np = realloc(mail, sizeof(mail_t) * (l + 1))) == NULL) {
			free(mail);
			smb_unlocksmbhdr(smb);
			return NULL;
		}
		mail = np;
		mail[l] = idx;
		l++;
	}
	smb_unlocksmbhdr(smb);
	*msgs = l;
	if (l && (mode & LM_REVERSE)) {
		mail_t* reversed = malloc(sizeof(mail_t) * l);
		if (reversed == NULL) {
			free(mail);
			return NULL;
		}
		for (ulong n = 0; n < l; n++)
			reversed[n] = mail[l - (n + 1)];
		free(mail);
		mail = reversed;
	}
	return mail;
}

void freemail(mail_t* mail)
{
	if (mail != NULL)
		free(mail);
}
