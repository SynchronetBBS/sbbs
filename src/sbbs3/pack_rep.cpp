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
#include "post.h"
#include "qwk.h"

/****************************************************************************/
/* Creates an REP packet for upload to QWK hub 'hubnum'.                    */
/* Returns 1 if successful, 0 if not.										*/
/****************************************************************************/
bool sbbs_t::pack_rep(uint hubnum)
{
	char	str[256],tmp2[256];
	int 	file,mode;
	uint	i,j,k;
	long	l,msgcnt,submsgs,posts,packedmail,netfiles=0,deleted;
	ulong	mailmsgs;
	ulong	last,msgs;
	post_t	HUGE16 *post;
	mail_t	*mail;
    struct	_finddata_t ff;
	long	ff_handle;
	FILE	*rep;
	smbmsg_t msg;

	msgcnt=0L;
	delfiles(cfg.temp_dir,"*.*");
	sprintf(str,"%s%s.REP",cfg.data_dir,cfg.qhub[hubnum]->id);
	if(fexist(str)) {
		lprintf("Updating %s", str);
		external(cmdstr(cfg.qhub[hubnum]->unpack,str,"*.*",NULL),EX_OFFLINE);
	} else
		lprintf("Creating %s", str);
	/*************************************************/
	/* Create SYSID.MSG, write header and leave open */
	/*************************************************/
	sprintf(str,"%s%s.MSG",cfg.temp_dir,cfg.qhub[hubnum]->id);
	if((rep=fnopen(&file,str,O_CREAT|O_WRONLY))==NULL) {
		errormsg(WHERE,ERR_OPEN,str,O_CREAT|O_WRONLY);
		return(false); }
	if(filelength(file)<1) { 							/* New REP packet */
		sprintf(str,"%-128s",cfg.qhub[hubnum]->id);     /* So write header */
		fwrite(str,128,1,rep); }
	fseek(rep,0L,SEEK_END);
	/*********************/
	/* Pack new messages */
	/*********************/
	console|=CON_L_ECHO;

	sprintf(smb.file,"%sMAIL",cfg.data_dir);
	smb.retry_time=cfg.smb_retry_time;
	if((i=smb_open(&smb))!=0) {
		fclose(rep);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(false); }

	/***********************/
	/* Pack E-mail, if any */
	/***********************/
	qwkmail_time=time(NULL);
	mail=loadmail(&smb,&mailmsgs,0,MAIL_YOUR,0);
	packedmail=0;
	if(mailmsgs) {
		lprintf("Packing NetMail for %s", cfg.qhub[hubnum]->id);
		for(l=0;(ulong)l<mailmsgs;l++) {
	//		bprintf("\b\b\b\b\b%-5lu",l+1);

			msg.idx.offset=mail[l].offset;
			if(!loadmsg(&msg,mail[l].number))
				continue;

			sprintf(str,"%s/",cfg.qhub[hubnum]->id);
			if(msg.to_net.type!=NET_QWK
				|| (strcmp((char *)msg.to_net.addr,cfg.qhub[hubnum]->id)
				&& strncmp((char *)msg.to_net.addr,str,strlen(str)))) {
				smb_unlockmsghdr(&smb,&msg);
				smb_freemsgmem(&msg);
				continue; }

			msgtoqwk(&msg,rep,TO_QNET|REP|A_LEAVE,INVALID_SUB,0);
			packedmail++;
			smb_unlockmsghdr(&smb,&msg);
			smb_freemsgmem(&msg); }
		lprintf("Packed %d NetMail messages",packedmail); }
	smb_close(&smb);					/* Close the e-mail */
	if(mailmsgs)
		FREE(mail);

	#if 0
	useron.number=1;
	getuserdat(&useron);
	#endif
	for(i=0;i<cfg.qhub[hubnum]->subs;i++) {
		j=cfg.qhub[hubnum]->sub[i]; 			/* j now equals the real sub num */
		msgs=getlastmsg(j,&last,0);
		lncntr=0;						/* defeat pause */
		if(!msgs || last<=sub_ptr[j]) {
			if(sub_ptr[j]>last) {
				sub_ptr[j]=last;
				sub_last[j]=last; }
			lprintf(remove_ctrl_a(text[NScanStatusFmt],tmp)
				,cfg.grp[cfg.sub[j]->grp]->sname
				,cfg.sub[j]->lname,0L,msgs);
			continue; }

		sprintf(smb.file,"%s%s"
			,cfg.sub[j]->data_dir,cfg.sub[j]->code);
		smb.retry_time=cfg.smb_retry_time;
		if((k=smb_open(&smb))!=0) {
			errormsg(WHERE,ERR_OPEN,smb.file,k,smb.last_error);
			continue; }

		post=loadposts(&posts,j,sub_ptr[j],LP_BYSELF|LP_OTHERS|LP_PRIVATE|LP_REP);
		lprintf(remove_ctrl_a(text[NScanStatusFmt],tmp)
			,cfg.grp[cfg.sub[j]->grp]->sname
			,cfg.sub[j]->lname,posts,msgs);
		if(!posts)	{ /* no new messages */
			smb_close(&smb);
			continue; }

		sub_ptr[j]=last;                   /* set pointer */
		lputs(remove_ctrl_a(text[QWKPackingSubboard],tmp));	/* ptr to last msg	*/
		submsgs=0;
		for(l=0;l<posts;l++) {
	//		bprintf("\b\b\b\b\b%-5lu",l+1);

			msg.idx.offset=post[l].offset;
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

			mode=cfg.qhub[hubnum]->mode[i]|TO_QNET|REP;
			if(mode&A_LEAVE) mode|=(VIA|TZ);
			if(msg.from_net.type!=NET_QWK)
				mode|=TAGLINE;

			msgtoqwk(&msg,rep,mode,j,cfg.qhub[hubnum]->conf[i]);

			smb_freemsgmem(&msg);
			smb_unlockmsghdr(&smb,&msg);
			msgcnt++;
			submsgs++; }
		lprintf(remove_ctrl_a(text[QWKPackedSubboard],tmp),submsgs,msgcnt);
		LFREE(post);
		smb_close(&smb); }

	fclose(rep);			/* close MESSAGE.DAT */
	CRLF;
							/* Look for extra files to send out */
	sprintf(str,"%sQNET/%s.OUT/*.*",cfg.data_dir,cfg.qhub[hubnum]->id);
	ff_handle=_findfirst(str,&ff);
	while(ff_handle!=-1) {
		if(!(ff.attrib&_A_SUBDIR)) {
			sprintf(str,"%sQNET/%s.OUT/%s",cfg.data_dir,cfg.qhub[hubnum]->id,ff.name);
			sprintf(tmp2,"%s%s",cfg.temp_dir,ff.name);
			lprintf(remove_ctrl_a(text[RetrievingFile],tmp),str);
			if(!mv(str,tmp2,1))
				netfiles++;
		}
		if(_findnext(ff_handle,&ff)!=0) {
			_findclose(ff_handle);
			ff_handle=-1; } }
	if(netfiles)
		CRLF;

	if(!msgcnt && !netfiles && !packedmail) {
		lputs(remove_ctrl_a(text[QWKNoNewMessages],tmp));
		return(false); }

	/*******************/
	/* Compress Packet */
	/*******************/
	sprintf(str,"%s%s.REP",cfg.data_dir,cfg.qhub[hubnum]->id);
	sprintf(tmp2,"%s*.*",cfg.temp_dir);
	i=external(cmdstr(cfg.qhub[hubnum]->pack,str,tmp2,NULL),EX_OFFLINE);
	if(!fexist(str)) {
		lputs(remove_ctrl_a(text[QWKCompressionFailed],tmp));
		if(i)
			errormsg(WHERE,ERR_EXEC,cmdstr(cfg.qhub[hubnum]->pack,str,tmp2,NULL),i);
		else
			errorlog("Couldn't compress REP packet");
		return(false); }
	sprintf(str,"%sQNET/%s.OUT/",cfg.data_dir,cfg.qhub[hubnum]->id);
	delfiles(str,"*.*");

	if(packedmail) {						/* Delete NetMail */
		sprintf(smb.file,"%sMAIL",cfg.data_dir);
		smb.retry_time=cfg.smb_retry_time;
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
			if(mail[l].time>qwkmail_time)
				continue;
			msg.idx.offset=0;
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
			smb_freemsgmem(&msg); }

		if(deleted && cfg.sys_misc&SM_DELEMAIL)
			delmail(0,MAIL_YOUR);
		smb_close(&smb);
		if(mailmsgs)
			FREE(mail); }

	return(true);
}
