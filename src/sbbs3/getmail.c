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
