/* Synchronet mail-related routines */

/* $Id: mail.cpp,v 1.33 2018/04/30 22:54:19 rswindell Exp $ */

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
/* Removes all mail messages for usernumber that have been marked 'deleted' */
/* smb_locksmbhdr() should be called prior to this function 				*/
/* Returns the number of messages removed									*/
/****************************************************************************/
int sbbs_t::delmail(uint usernumber, int which)
{
	ulong	i,l;
	time_t	now;
	idxrec_t *idxbuf;
	smbmsg_t msg;
	int		removed = 0;

	now=time(NULL);
	if((i=smb_getstatus(&smb))!=0) {
		errormsg(WHERE,ERR_READ,smb.file,i,smb.last_error);
		return 0;
	}
	if(!smb.status.total_msgs)
		return 0;
	if((idxbuf=(idxrec_t *)malloc(smb.status.total_msgs*sizeof(idxrec_t)))==NULL) {
		errormsg(WHERE,ERR_ALLOC,smb.file,smb.status.total_msgs*sizeof(idxrec_t));
		return 0;
	}
	if((i=smb_open_da(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		free(idxbuf);
		return 0;
	}
	if((i=smb_open_ha(&smb))!=0) {
		smb_close_da(&smb);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		free(idxbuf);
		return 0;
	}
	smb_rewind(smb.sid_fp);
	for(l=0;l<smb.status.total_msgs;) {
		memset(&msg, 0, sizeof(msg));
		if(smb_fread(&smb,&msg.idx,sizeof(idxrec_t),smb.sid_fp)!=sizeof(idxrec_t))
			break;
		if(!(msg.idx.attr&MSG_PERMANENT)
			&& ((which==MAIL_SENT && usernumber==msg.idx.from)
				|| (which==MAIL_YOUR && usernumber==msg.idx.to)
				|| (which==MAIL_ANY	&& (usernumber==msg.idx.to || usernumber==msg.idx.from))
				|| which==MAIL_ALL)) {
			if(smb.status.max_age && (now<0?0:(uintmax_t)now)>msg.idx.time
				&& (now-msg.idx.time)/(24L*60L*60L)>smb.status.max_age)
				msg.idx.attr|=MSG_DELETE;
			else if((msg.idx.attr&MSG_SPAM)
				&& cfg.max_spamage && (now<0?0:(uintmax_t)now) > msg.idx.time
				&& (now-msg.idx.time)/(24L*60L*60L) > cfg.max_spamage)
				msg.idx.attr|=MSG_DELETE;
			else if(msg.idx.attr&MSG_KILLREAD && msg.idx.attr&MSG_READ)
				msg.idx.attr|=MSG_DELETE;
			if(msg.idx.attr&MSG_DELETE) {
				/* Don't need to lock message because base is locked */
				if((i=smb_getmsghdr(&smb,&msg))!=0)
					errormsg(WHERE,ERR_READ,smb.file,i,smb.last_error);
				else {
					if(msg.hdr.attr!=msg.idx.attr) {
						msg.hdr.attr=msg.idx.attr;
						if((i=smb_putmsghdr(&smb,&msg))!=0)
							errormsg(WHERE,ERR_WRITE,smb.file,i,smb.last_error); 
					}
					if((i=smb_freemsg(&smb,&msg))!=0)
						errormsg(WHERE,ERR_REMOVE,smb.file,i,smb.last_error);
					if(msg.hdr.auxattr&MSG_FILEATTACH)
						delfattach(&cfg,&msg);
					smb_freemsgmem(&msg); 
				}
				removed++;
				continue; 
			}
		}
		idxbuf[l]=msg.idx;
		l++; 
	}
	smb_rewind(smb.sid_fp);
	for(i=0;i<l;i++) {
		if(smb_fwrite(&smb,&idxbuf[i],sizeof(idxrec_t),smb.sid_fp) != sizeof(idxrec_t)) {
			errormsg(WHERE, ERR_WRITE, smb.file, i);
			break;
		}
	}
	smb_fflush(smb.sid_fp);
	if(smb_fsetlength(smb.sid_fp, i * sizeof(idxrec_t)) != 0)
		errormsg(WHERE, "truncating", smb.file, i * sizeof(idxrec_t));
	smb.status.total_msgs=i;
	smb_putstatus(&smb);
	free(idxbuf);
	smb_close_ha(&smb);
	smb_close_da(&smb);
	return removed;
}


/***********************************************/
/* Tell the user that so-and-so read your mail */
/***********************************************/
void sbbs_t::telluser(smbmsg_t* msg)
{
	char str[256];
	uint usernumber,n;
	node_t node;

	if(msg->from_net.type)
		return;
	if(msg->from_ext)
		usernumber=atoi(msg->from_ext);
	else {
		usernumber=matchuser(&cfg,msg->from,TRUE /*sysop_alias*/);
		if(!usernumber)
			return; 
	}
	for(n=1;n<=cfg.sys_nodes;n++) { /* Tell user */
		getnodedat(n,&node,0);
		if(node.useron==usernumber
		&& (node.status==NODE_INUSE
		|| node.status==NODE_QUIET)) {
			sprintf(str
				,text[UserReadYourMailNodeMsg]
				,cfg.node_num,useron.alias);
			putnmsg(&cfg,n,str);
			break; 
		} 
	}
	if(n>cfg.sys_nodes) {
		now=time(NULL);
		sprintf(str,text[UserReadYourMail]
			,useron.alias,timestr(now));
		putsmsg(&cfg,usernumber,str); 
	}
}

/************************************************************************/
/* Deletes all mail waiting for user number 'usernumber'                */
/************************************************************************/
void sbbs_t::delallmail(uint usernumber, int which, bool permanent, long lm_mode)
{
	int 		i;
	long		deleted=0;
	uint32_t	msgs,u;
	mail_t		*mail;
	smbmsg_t 	msg;

	if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
		errormsg(WHERE,ERR_OPEN,"MAIL",i);
		return; 
	}
	sprintf(smb.file,"%smail",cfg.data_dir);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=INVALID_SUB;
	if((i=smb_open(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		smb_stack(&smb,SMB_STACK_POP);
		return; 
	}

	mail=loadmail(&smb,&msgs,usernumber,which,lm_mode);
	if(!msgs) {
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		return; 
	}
	if((i=smb_locksmbhdr(&smb))!=0) {			/* Lock the base, so nobody */
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		free(mail);
		errormsg(WHERE,ERR_LOCK,smb.file,i,smb.last_error);	/* messes with the index */
		return; 
	}
	for(u=0;u<msgs;u++) {
		msg.idx.offset=0;						/* search by number */
		if((mail[u].attr&MSG_PERMANENT) && !permanent)
			continue;
		if(loadmsg(&msg,mail[u].number) >= 0) {	   /* message still there */
			msg.hdr.attr|=MSG_DELETE;
			msg.hdr.attr&=~MSG_PERMANENT;
			msg.idx.attr=msg.hdr.attr;
			if((i=smb_putmsg(&smb,&msg))!=0)
				errormsg(WHERE,ERR_WRITE,smb.file,i,smb.last_error);
			else
				deleted++;
			smb_freemsgmem(&msg);
			smb_unlockmsghdr(&smb,&msg); 
		} 
	}

	if(msgs)
		free(mail);
	if(permanent && deleted && (cfg.sys_misc&SM_DELEMAIL))
		delmail(usernumber,MAIL_ANY);
	smb_unlocksmbhdr(&smb);
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
}

