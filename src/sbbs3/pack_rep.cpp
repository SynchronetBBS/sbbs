/* pack_rep.cpp */

/* Synchronet QWK reply (REP) packet creation routine */

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
#include "qwk.h"

/****************************************************************************/
/* Creates an REP packet for upload to QWK hub 'hubnum'.                    */
/* Returns 1 if successful, 0 if not.										*/
/****************************************************************************/
bool sbbs_t::pack_rep(uint hubnum)
{
	char	str[MAX_PATH+1];
	char 	tmp[MAX_PATH+1],tmp2[MAX_PATH+1];
	int 	file,mode;
	uint	i,j,k;
	long	l,msgcnt,submsgs,posts,packedmail,netfiles=0,deleted;
	ulong	mailmsgs;
	ulong	last,msgs;
	post_t	HUGE16 *post;
	mail_t	*mail;
	FILE*	rep;
	DIR*	dir;
	DIRENT*	dirent;
	smbmsg_t msg;

	msgcnt=0L;
	delfiles(cfg.temp_dir,ALLFILES);
	sprintf(str,"%s%s.rep",cfg.data_dir,cfg.qhub[hubnum]->id);
	if(fexist(str)) {
		eprintf("Updating %s", str);
		external(cmdstr(cfg.qhub[hubnum]->unpack,str,ALLFILES,NULL),EX_OFFLINE);
	} else
		eprintf("Creating %s", str);
	/*************************************************/
	/* Create SYSID.MSG, write header and leave open */
	/*************************************************/
	sprintf(str,"%s%s.msg",cfg.temp_dir,cfg.qhub[hubnum]->id);
	if((rep=fnopen(&file,str,O_CREAT|O_WRONLY|O_TRUNC))==NULL) {
		errormsg(WHERE,ERR_CREATE,str,O_CREAT|O_WRONLY|O_TRUNC);
		return(false); }
	if(filelength(file)<1) { 							/* New REP packet */
		sprintf(str,"%-*s"
			,QWK_BLOCK_LEN,cfg.qhub[hubnum]->id);		/* So write header */
		fwrite(str,QWK_BLOCK_LEN,1,rep); 
	}
	fseek(rep,0L,SEEK_END);
	/*********************/
	/* Pack new messages */
	/*********************/
	console|=CON_L_ECHO;

	sprintf(smb.file,"%smail",cfg.data_dir);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=INVALID_SUB;
	if((i=smb_open(&smb))!=0) {
		fclose(rep);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(false); }

	/***********************/
	/* Pack E-mail, if any */
	/***********************/
	qwkmail_last=0;
	mail=loadmail(&smb,&mailmsgs,0,MAIL_YOUR,0);
	packedmail=0;
	if(mailmsgs) {
		eprintf("Packing NetMail for %s", cfg.qhub[hubnum]->id);
		for(l=0;(ulong)l<mailmsgs;l++) {
	//		bprintf("\b\b\b\b\b%-5lu",l+1);

			memset(&msg,0,sizeof(msg));
			msg.idx=mail[l];
			if(msg.idx.number>qwkmail_last)
				qwkmail_last=msg.idx.number;
			if(!loadmsg(&msg,mail[l].number))
				continue;

			sprintf(str,"%s/",cfg.qhub[hubnum]->id);
			if(msg.to_net.type!=NET_QWK
				|| (strcmp((char *)msg.to_net.addr,cfg.qhub[hubnum]->id)
				&& strncmp((char *)msg.to_net.addr,str,strlen(str)))) {
				smb_unlockmsghdr(&smb,&msg);
				smb_freemsgmem(&msg);
				continue; }

			msgtoqwk(&msg,rep,QM_TO_QNET|QM_REP|A_LEAVE,INVALID_SUB,0);
			packedmail++;
			smb_unlockmsghdr(&smb,&msg);
			smb_freemsgmem(&msg); 
			YIELD();	/* yield */
		}
		eprintf("Packed %d NetMail messages",packedmail); 
	}
	smb_close(&smb);					/* Close the e-mail */
	if(mailmsgs)
		FREE(mail);

	for(i=0;i<cfg.qhub[hubnum]->subs;i++) {
		j=cfg.qhub[hubnum]->sub[i]; 			/* j now equals the real sub num */
		msgs=getlastmsg(j,&last,0);
		lncntr=0;						/* defeat pause */
		if(!msgs || last<=subscan[j].ptr) {
			if(subscan[j].ptr>last) {
				subscan[j].ptr=last;
				subscan[j].ptr=last; }
			eprintf(remove_ctrl_a(text[NScanStatusFmt],tmp)
				,cfg.grp[cfg.sub[j]->grp]->sname
				,cfg.sub[j]->lname,0L,msgs);
			continue; }

		sprintf(smb.file,"%s%s"
			,cfg.sub[j]->data_dir,cfg.sub[j]->code);
		smb.retry_time=cfg.smb_retry_time;
		smb.subnum=j;
		if((k=smb_open(&smb))!=0) {
			errormsg(WHERE,ERR_OPEN,smb.file,k,smb.last_error);
			continue; }

		post=loadposts(&posts,j,subscan[j].ptr,LP_BYSELF|LP_OTHERS|LP_PRIVATE|LP_REP);
		eprintf(remove_ctrl_a(text[NScanStatusFmt],tmp)
			,cfg.grp[cfg.sub[j]->grp]->sname
			,cfg.sub[j]->lname,posts,msgs);
		if(!posts)	{ /* no new messages */
			smb_close(&smb);
			continue; }

		subscan[j].ptr=last;                   /* set pointer */
		eprintf("%s",remove_ctrl_a(text[QWKPackingSubboard],tmp));	/* ptr to last msg	*/
		submsgs=0;
		for(l=0;l<posts;l++) {
	//		bprintf("\b\b\b\b\b%-5lu",l+1);

			memset(&msg,0,sizeof(msg));
			msg.idx=post[l];
			if(!loadmsg(&msg,post[l].number))
				continue;

			if(msg.from_net.type && msg.from_net.type!=NET_QWK &&
				!(cfg.sub[j]->misc&SUB_GATE)) {
				smb_freemsgmem(&msg);
				smb_unlockmsghdr(&smb,&msg);
				continue; }

			if(!strnicmp(msg.subj,"NE:",3) || (msg.from_net.type==NET_QWK &&
				route_circ((char *)msg.from_net.addr,cfg.qhub[hubnum]->id))) {
				smb_freemsgmem(&msg);
				smb_unlockmsghdr(&smb,&msg);
				continue; }

			mode=cfg.qhub[hubnum]->mode[i]|QM_TO_QNET|QM_REP;
			if(mode&A_LEAVE) mode|=(QM_VIA|QM_TZ|QM_MSGID);
			if(msg.from_net.type!=NET_QWK)
				mode|=QM_TAGLINE;

			msgtoqwk(&msg,rep,mode,j,cfg.qhub[hubnum]->conf[i]);

			smb_freemsgmem(&msg);
			smb_unlockmsghdr(&smb,&msg);
			msgcnt++;
			submsgs++; 
			if(!(l%50))
				YIELD(); /* yield */
		}
		eprintf(remove_ctrl_a(text[QWKPackedSubboard],tmp),submsgs,msgcnt);
		LFREE(post);
		smb_close(&smb); 
		YIELD();	/* yield */
	}

	fclose(rep);			/* close HUB_ID.MSG */
	CRLF;
							/* Look for extra files to send out */
	sprintf(str,"%sqnet/%s.out",cfg.data_dir,cfg.qhub[hubnum]->id);
	strlwr(str);
	dir=opendir(str);
	while(dir!=NULL && (dirent=readdir(dir))!=NULL) {
		sprintf(str,"%sqnet/%s.out/%s",cfg.data_dir,cfg.qhub[hubnum]->id,dirent->d_name);
		strlwr(str);
		if(isdir(str))
			continue;
		sprintf(tmp2,"%s%s",cfg.temp_dir,dirent->d_name);
		eprintf(remove_ctrl_a(text[RetrievingFile],tmp),str);
		if(!mv(str,tmp2,1))
			netfiles++;
	}
	if(dir!=NULL)
		closedir(dir);
	if(netfiles)
		CRLF;

	if(!msgcnt && !netfiles && !packedmail) {
		eprintf("%s",remove_ctrl_a(text[QWKNoNewMessages],tmp));
		return(false); 
	}

	/*******************/
	/* Compress Packet */
	/*******************/
	sprintf(str,"%s%s.rep",cfg.data_dir,cfg.qhub[hubnum]->id);
	sprintf(tmp2,"%s%s",cfg.temp_dir,ALLFILES);
	i=external(cmdstr(cfg.qhub[hubnum]->pack,str,tmp2,NULL)
		,EX_OFFLINE|EX_WILDCARD);
	if(!fexist(str)) {
		eprintf("%s",remove_ctrl_a(text[QWKCompressionFailed],tmp));
		if(i)
			errormsg(WHERE,ERR_EXEC,cmdstr(cfg.qhub[hubnum]->pack,str,tmp2,NULL),i);
		else
			errorlog("Couldn't compress REP packet");
		return(false); }
	sprintf(str,"%sqnet/%s.out/",cfg.data_dir,cfg.qhub[hubnum]->id);
	strlwr(str);
	delfiles(str,ALLFILES);

	if(packedmail) {						/* Delete NetMail */
		sprintf(smb.file,"%smail",cfg.data_dir);
		smb.retry_time=cfg.smb_retry_time;
		smb.subnum=INVALID_SUB;
		if((i=smb_open(&smb))!=0) {
			errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
			return(true); }

		mail=loadmail(&smb,&mailmsgs,0,MAIL_YOUR,0);

		if((i=smb_locksmbhdr(&smb))!=0) {			  /* Lock the base, so nobody */
			if(mailmsgs)
				FREE(mail);
			smb_close(&smb);
			errormsg(WHERE,ERR_LOCK,smb.file,i,smb.last_error);	/* messes with the index */
			return(true); }

		if((i=smb_getstatus(&smb))!=0) {
			if(mailmsgs)
				FREE(mail);
			smb_close(&smb);
			errormsg(WHERE,ERR_READ,smb.file,i,smb.last_error);
			return(true); }

		deleted=0;
		/* Mark as READ and DELETE */
		for(l=0;(ulong)l<mailmsgs;l++) {
			if(mail[l].number>qwkmail_last)
				continue;
			memset(&msg,0,sizeof(msg));
			msg.idx=mail[l];
			if(!loadmsg(&msg,mail[l].number))
				continue;

			sprintf(str,"%s/",cfg.qhub[hubnum]->id);
			if(msg.to_net.type!=NET_QWK
				|| (strcmp((char *)msg.to_net.addr,cfg.qhub[hubnum]->id)
				&& strncmp((char *)msg.to_net.addr,str,strlen(str)))) {
				smb_unlockmsghdr(&smb,&msg);
				smb_freemsgmem(&msg);
				continue; }

			msg.hdr.attr|=MSG_DELETE;
			msg.idx.attr=msg.hdr.attr;
			if((i=smb_putmsg(&smb,&msg))!=0)
				errormsg(WHERE,ERR_WRITE,smb.file,i,smb.last_error);
			else
				deleted++;
			smb_unlockmsghdr(&smb,&msg);
			smb_freemsgmem(&msg); 
		}

		if(deleted && cfg.sys_misc&SM_DELEMAIL)
			delmail(0,MAIL_YOUR);
		smb_close(&smb);
		if(mailmsgs)
			FREE(mail); 
		eprintf("Deleted %d sent NetMail messages",deleted); 
	}

	return(true);
}
