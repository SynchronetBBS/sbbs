/* readmsgs.cpp */

/* Synchronet public message reading function */

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

int sbbs_t::sub_op(uint subnum)
{
	return(SYSOP || (cfg.sub[subnum]->op_ar[0] && chk_ar(cfg.sub[subnum]->op_ar,&useron)));
}


void sbbs_t::listmsgs(int subnum, post_t HUGE16 *post, long i, long posts)
{
	char ch;
	smbmsg_t msg;


	bputs(text[MailOnSystemLstHdr]);
	msg.total_hfields=0;
	while(i<posts && !msgabort()) {
		if(msg.total_hfields)
			smb_freemsgmem(&msg);
		msg.total_hfields=0;
		msg.idx.offset=post[i].offset;
		if(!loadmsg(&msg,post[i].number))
			break;
		smb_unlockmsghdr(&smb,&msg);
		if(msg.hdr.attr&MSG_DELETE)
			ch='-';
		else if((!stricmp(msg.to,useron.alias) || !stricmp(msg.to,useron.name))
			&& !(msg.hdr.attr&MSG_READ))
			ch='!';
		else if(msg.hdr.number>subscan[subnum].ptr)
			ch='*';
		else
			ch=' ';
		bprintf(text[SubMsgLstFmt],(long)i+1
			,msg.hdr.attr&MSG_ANONYMOUS && !sub_op(subnum)
			? text[Anonymous] : msg.from
			,msg.to
			,ch
			,msg.subj);
		smb_freemsgmem(&msg);
		msg.total_hfields=0;
		i++; }
}

char *binstr(uchar *buf, ushort length)
{
	static char str[512];
	char tmp[128];
	int i;

	str[0]=0;
	for(i=0;i<length;i++)
		if(buf[i] && (buf[i]<SP || buf[i]>=0x7f)
			&& buf[i]!='\r' && buf[i]!='\n' && buf[i]!='\t')
			break;
	if(i==length)		/* not binary */
		return((char*)buf);
	for(i=0;i<length;i++) {
		sprintf(tmp,"%02X ",buf[i]);
		strcat(str,tmp); 
	}
	return(str);
}


void sbbs_t::msghdr(smbmsg_t* msg)
{
	int i;

	CRLF;

	/* variable fields */
	for(i=0;i<msg->total_hfields;i++)
		bprintf("%-16.16s %s\r\n"
			,smb_hfieldtype(msg->hfield[i].type)
			,binstr((uchar *)msg->hfield_dat[i],msg->hfield[i].length));

	/* fixed fields */
	bprintf("%-16.16s %s %s\r\n","when_written"	
		,timestr((time_t *)&msg->hdr.when_written.time), zonestr(msg->hdr.when_written.zone));
	bprintf("%-16.16s %s %s\r\n","when_imported"	
		,timestr((time_t *)&msg->hdr.when_imported.time), zonestr(msg->hdr.when_imported.zone));
	bprintf("%-16.16s %04Xh\r\n","type"				,msg->hdr.type);
	bprintf("%-16.16s %04Xh\r\n","version"			,msg->hdr.version);
	bprintf("%-16.16s %04Xh\r\n","attr"				,msg->hdr.attr);
	bprintf("%-16.16s %08lXh\r\n","auxattr"			,msg->hdr.auxattr);
	bprintf("%-16.16s %08lXh\r\n","netattr"			,msg->hdr.netattr);
	bprintf("%-16.16s %ld\r\n"	 ,"number"			,msg->hdr.number);
	bprintf("%-16.16s %06lXh\r\n","header offset"	,msg->idx.offset);
	bprintf("%-16.16s %u\r\n"	 ,"header length"	,msg->hdr.length);

	/* optional fixed fields */
	if(msg->hdr.thread_orig)
		bprintf("%-16.16s %ld\r\n"	,"thread_orig"		,msg->hdr.thread_orig);
	if(msg->hdr.thread_next)
		bprintf("%-16.16s %ld\r\n"	,"thread_next"		,msg->hdr.thread_next);
	if(msg->hdr.thread_first)
		bprintf("%-16.16s %ld\r\n"	,"thread_first"		,msg->hdr.thread_first);
	if(msg->hdr.delivery_attempts)
		bprintf("%-16.16s %hu\r\n"	,"delivery_attempts",msg->hdr.delivery_attempts);
	if(msg->hdr.times_downloaded)
		bprintf("%-16.16s %lu\r\n"	,"times_downloaded"	,msg->hdr.times_downloaded);
	if(msg->hdr.last_downloaded)
		bprintf("%-16.16s %s\r\n"	,"last_downloaded"	,timestr((time_t*)&msg->hdr.last_downloaded));

	/* convenience integers */
	if(msg->expiration)
		bprintf("%-16.16s %s\r\n"	,"expiration"	
			,timestr((time_t *)&msg->expiration));
	if(msg->priority)
		bprintf("%-16.16s %lu\r\n"	,"priority"			,msg->priority);
	if(msg->cost)
		bprintf("%-16.16s %lu\r\n"	,"cost"				,msg->cost);

	bprintf("%-16.16s %06lXh\r\n"	,"data offset"		,msg->hdr.offset);
	for(i=0;i<msg->hdr.total_dfields;i++)
		bprintf("data field[%u]    %s, offset %lu, length %lu\r\n"
				,i
				,smb_dfieldtype(msg->dfield[i].type)
				,msg->dfield[i].offset
				,msg->dfield[i].length);
}

/****************************************************************************/
/****************************************************************************/
post_t HUGE16 * sbbs_t::loadposts(long *posts, uint subnum, ulong ptr, long mode)
{
	char name[128];
	ushort aliascrc,namecrc,sysop;
	int i,skip;
	ulong l=0,total,alloc_len;
	smbmsg_t msg;
	idxrec_t idx;
	post_t HUGE16 *post;

	if(posts==NULL)
		return(NULL);

	(*posts)=0;

	if((i=smb_locksmbhdr(&smb))!=0) {				/* Be sure noone deletes or */
		errormsg(WHERE,ERR_LOCK,smb.file,i);		/* adds while we're reading */
		return(NULL); }

	total=filelength(fileno(smb.sid_fp))/sizeof(idxrec_t); /* total msgs in sub */

	if(!total) {			/* empty */
		smb_unlocksmbhdr(&smb);
		return(NULL); }

	strcpy(name,useron.name);
	strlwr(name);
	namecrc=crc16(name);
	strcpy(name,useron.alias);
	strlwr(name);
	aliascrc=crc16(name);
	sysop=crc16("sysop");

	rewind(smb.sid_fp);

	alloc_len=sizeof(post_t)*total;
	#ifdef __OS2__
		while(alloc_len%4096)
			alloc_len++;
	#endif
	if((post=(post_t HUGE16 *)LMALLOC(alloc_len))==NULL) {	/* alloc for max */
		smb_unlocksmbhdr(&smb);
		errormsg(WHERE,ERR_ALLOC,smb.file,sizeof(post_t *)*cfg.sub[subnum]->maxmsgs);
		return(NULL); }
	while(!feof(smb.sid_fp)) {
		skip=0;
		if(smb_fread(&idx,sizeof(idx),smb.sid_fp) != sizeof(idx))
			break;

		if(idx.number==0)	/* invalid message number, ignore */
			continue;

		if(idx.number<=ptr)
			continue;

		if(idx.attr&MSG_READ && mode&LP_UNREAD) /* Skip read messages */
			continue;

		if(idx.attr&MSG_DELETE) {		/* Pre-flagged */
			if(mode&LP_REP) 			/* Don't include deleted msgs in REP pkt */
				continue;
			if(!(cfg.sys_misc&SM_SYSVDELM)) /* Noone can view deleted msgs */
				continue;
			if(!(cfg.sys_misc&SM_USRVDELM)	/* Users can't view deleted msgs */
				&& !sub_op(subnum)) 	/* not sub-op */
				continue;
			if(!sub_op(subnum)			/* not sub-op */
				&& idx.from!=namecrc && idx.from!=aliascrc) /* not for you */
				continue; }

		if(idx.attr&MSG_MODERATED && !(idx.attr&MSG_VALIDATED)
			&& (mode&LP_REP || !sub_op(subnum)))
			break;

		if(idx.attr&MSG_PRIVATE && !(mode&LP_PRIVATE)
			&& !sub_op(subnum) && !(useron.rest&FLAG('Q'))) {
			if(idx.to!=namecrc && idx.from!=namecrc
				&& idx.to!=aliascrc && idx.from!=aliascrc
				&& (useron.number!=1 || idx.to!=sysop))
				continue;
			if(!smb_lockmsghdr(&smb,&msg)) {
				if(!smb_getmsghdr(&smb,&msg)) {
					if(stricmp(msg.to,useron.alias)
						&& stricmp(msg.from,useron.alias)
						&& stricmp(msg.to,useron.name)
						&& stricmp(msg.from,useron.name)
						&& (useron.number!=1 || stricmp(msg.to,"sysop")
						|| msg.from_net.type))
						skip=1;
					smb_freemsgmem(&msg); }
				smb_unlockmsghdr(&smb,&msg); }
			if(skip)
				continue; }


		if(!(mode&LP_BYSELF) && (idx.from==namecrc || idx.from==aliascrc)) {
			msg.idx=idx;
			if(!smb_lockmsghdr(&smb,&msg)) {
				if(!smb_getmsghdr(&smb,&msg)) {
					if(!stricmp(msg.from,useron.alias)
						|| !stricmp(msg.from,useron.name))
						skip=1;
					smb_freemsgmem(&msg); }
				smb_unlockmsghdr(&smb,&msg); }
			if(skip)
				continue; }

		if(!(mode&LP_OTHERS)) {
			if(idx.to!=namecrc && idx.to!=aliascrc
				&& (useron.number!=1 || idx.to!=sysop))
				continue;
			msg.idx=idx;
			if(!smb_lockmsghdr(&smb,&msg)) {
				if(!smb_getmsghdr(&smb,&msg)) {
					if(stricmp(msg.to,useron.alias) && stricmp(msg.to,useron.name)
						&& (useron.number!=1 || stricmp(msg.to,"sysop")
						|| msg.from_net.type))
						skip=1;
					smb_freemsgmem(&msg); }
				smb_unlockmsghdr(&smb,&msg); }
			if(skip)
				continue; }

		memcpy(&post[l],&idx,sizeof(idx));
		l++; 
	}
	smb_unlocksmbhdr(&smb);
	if(!l)
		FREE_AND_NULL(post);

	(*posts)=l;
	return(post);
}

static int get_start_msg(sbbs_t* sbbs, smb_t* smb)
{
	int i,j=smb->curmsg+1;

	if(j<smb->msgs)
		j++;
	else
		j=1;
	sbbs->bprintf(sbbs->text[StartWithN],j);
	if((i=sbbs->getnum(smb->msgs))<0)
		return(i);
	if(i==0)
		return(j-1);
	return(i-1);
}

/****************************************************************************/
/* Reads posts on subboard sub. 'mode' determines new-posts only, browse,   */
/* or continuous read.                                                      */
/* Returns 0 if normal completion, 1 if aborted.                            */
/* Called from function main_sec                                            */
/****************************************************************************/
int sbbs_t::scanposts(uint subnum, long mode, char *find)
{
	char	str[256],str2[256],reread=0,mismatches=0
			,done=0,domsg=1,HUGE16 *buf,*p;
	char	find_buf[128];
	char	tmp[128];
	int		i;
	uint 	usub,ugrp,reads=0;
	uint	lp=0;
	long	org_mode=mode;
	ulong	msgs,last,l;
	post_t	HUGE16 *post;
	smbmsg_t	msg;

	cursubnum=subnum;	/* for ARS */
	if(!chk_ar(cfg.sub[subnum]->read_ar,&useron)) {
		bprintf("\1n\r\nYou can't read messages on %s %s\r\n"
				,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->sname);
		return(0); }
	msg.total_hfields=0;				/* init to NULL, specify not-allocated */
	if(!(mode&SCAN_CONST))
		lncntr=0;
	if((msgs=getlastmsg(subnum,&last,0))==0) {
		if(mode&(SCAN_NEW|SCAN_TOYOU))
			bprintf(text[NScanStatusFmt]
				,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->lname,0L,0L);
		else
			bprintf(text[NoMsgsOnSub]
				,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->sname);
		return(0); }
	if(mode&SCAN_NEW && subscan[subnum].ptr>=last && !(mode&SCAN_BACK)) {
		if(subscan[subnum].ptr>last)
			subscan[subnum].ptr=subscan[subnum].last=last;
		bprintf(text[NScanStatusFmt]
			,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->lname,0L,msgs);
		return(0); }

	if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
		errormsg(WHERE,ERR_OPEN,cfg.sub[subnum]->code,i);
		return(0); }
	sprintf(smb.file,"%s%s",cfg.sub[subnum]->data_dir,cfg.sub[subnum]->code);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=subnum;
	if((i=smb_open(&smb))!=0) {
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(0); }

	if(!(mode&SCAN_TOYOU)
		&& (!mode || mode&SCAN_FIND || !(subscan[subnum].cfg&SUB_CFG_YSCAN)))
		lp=LP_BYSELF|LP_OTHERS;
	if(mode&SCAN_TOYOU)
		lp|=LP_UNREAD;
	post=loadposts(&smb.msgs,subnum,0,lp);
	if(mode&SCAN_NEW) { 		  /* Scanning for new messages */
		for(smb.curmsg=0;smb.curmsg<smb.msgs;smb.curmsg++)
			if(subscan[subnum].ptr<post[smb.curmsg].number)
				break;
		bprintf(text[NScanStatusFmt]
			,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->lname,smb.msgs-smb.curmsg,msgs);
		if(!smb.msgs) {		  /* no messages at all */
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			return(0); }
		if(smb.curmsg==smb.msgs) {  /* no new messages */
			if(!(mode&SCAN_BACK)) {
				if(post)
					LFREE(post);
				smb_close(&smb);
				smb_stack(&smb,SMB_STACK_POP);
				return(0); }
			smb.curmsg=smb.msgs-1; } }
	else {
		if(mode&SCAN_TOYOU)
			bprintf(text[NScanStatusFmt]
			   ,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->lname,smb.msgs,msgs);
		if(!smb.msgs) {
			if(!(mode&SCAN_TOYOU))
				bprintf(text[NoMsgsOnSub]
					,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->sname);
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			return(0); }
		if(mode&SCAN_FIND) {
			bprintf(text[SearchSubFmt]
				,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->lname,smb.msgs);
			domsg=1;
			smb.curmsg=0; }
		else if(mode&SCAN_TOYOU)
			smb.curmsg=0;
		else {
			for(smb.curmsg=0;smb.curmsg<smb.msgs;smb.curmsg++)
				if(post[smb.curmsg].number>=subscan[subnum].last)
					break;
			if(smb.curmsg==smb.msgs)
				smb.curmsg=smb.msgs-1;

			domsg=1; } }

	if(useron.misc&RIP)
		menu("msgscan");

	if((i=smb_locksmbhdr(&smb))!=0) {
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_LOCK,smb.file,i);
		return(0); }
	if((i=smb_getstatus(&smb))!=0) {
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_READ,smb.file,i);
		return(0); }
	smb_unlocksmbhdr(&smb);
	last=smb.status.last_msg;

	action=NODE_RMSG;
	if(mode&SCAN_CONST) {   /* update action */
		getnodedat(cfg.node_num,&thisnode,1);
		thisnode.action=NODE_RMSG;
		putnodedat(cfg.node_num,&thisnode); }
	while(online && !done) {

		action=NODE_RMSG;

		if(mode&(SCAN_CONST|SCAN_FIND) && sys_status&SS_ABORT)
			break;

		if(post==NULL)	/* Been unloaded */
			post=loadposts(&smb.msgs,subnum,0,lp);   /* So re-load */

		if(!smb.msgs) {
			done=1;
			continue; }

		while(smb.curmsg>=smb.msgs) smb.curmsg--;

		for(ugrp=0;ugrp<usrgrps;ugrp++)
			if(usrgrp[ugrp]==cfg.sub[subnum]->grp)
				break;
		for(usub=0;usub<usrsubs[ugrp];usub++)
			if(usrsub[ugrp][usub]==subnum)
				break;
		usub++;
		ugrp++;

		msg.idx.offset=post[smb.curmsg].offset;
		msg.idx.number=post[smb.curmsg].number;
		msg.idx.to=post[smb.curmsg].to;
		msg.idx.from=post[smb.curmsg].from;
		msg.idx.subj=post[smb.curmsg].subj;

		if((i=smb_locksmbhdr(&smb))!=0) {
			errormsg(WHERE,ERR_LOCK,smb.file,i);
			break; }
		if((i=smb_getstatus(&smb))!=0) {
			smb_unlocksmbhdr(&smb);
			errormsg(WHERE,ERR_READ,smb.file,i);
			break; }
		smb_unlocksmbhdr(&smb);

		if(smb.status.last_msg!=last) { 	/* New messages */
			last=smb.status.last_msg;
			if(post) {
				LFREE((void *)post); }
			post=loadposts(&smb.msgs,subnum,0,lp);   /* So re-load */
			if(!smb.msgs)
				break;
			for(smb.curmsg=0;smb.curmsg<smb.msgs;smb.curmsg++)
				if(post[smb.curmsg].number==msg.idx.number)
					break;
			if(smb.curmsg>(smb.msgs-1))
				smb.curmsg=(smb.msgs-1);
			continue; }

		if(msg.total_hfields)
			smb_freemsgmem(&msg);
		msg.total_hfields=0;

		if(!loadmsg(&msg,post[smb.curmsg].number)) {
			if(mismatches>5) {	/* We can't do this too many times in a row */
				errormsg(WHERE,ERR_CHK,smb.file,post[smb.curmsg].number);
				break; }
			if(post)
				LFREE(post);
			post=loadposts(&smb.msgs,subnum,0,lp);
			if(!smb.msgs)
				break;
			if(smb.curmsg>(smb.msgs-1))
				smb.curmsg=(smb.msgs-1);
			mismatches++;
			continue; }
		smb_unlockmsghdr(&smb,&msg);

		mismatches=0;

		if(domsg) {

			if(!reread && mode&SCAN_FIND) { 			/* Find text in messages */
				buf=smb_getmsgtxt(&smb,&msg,GETMSGTXT_TAILS);
				if(!buf) {
					if(smb.curmsg<smb.msgs-1) 
						smb.curmsg++;
					else if(org_mode&SCAN_FIND)  
						done=1;
					else if(smb.curmsg>=smb.msgs-1)
							domsg=0;
					continue; 
				}
				strupr((char *)buf);
				if(!strstr((char *)buf,find) && !strstr(msg.subj,find)) {
					FREE(buf);
					if(smb.curmsg<smb.msgs-1) 
						smb.curmsg++;
					else if(org_mode&SCAN_FIND) 
							done=1;
					else if(smb.curmsg>=smb.msgs-1)
							domsg=0;
					continue; 
				}
				FREE(buf); 
			}

			if(mode&SCAN_CONST)
				bprintf(text[ZScanPostHdr],ugrp,usub,smb.curmsg+1,smb.msgs);

			if(!reads && mode)
				CRLF;

			show_msg(&msg
				,msg.from_ext && !strcmp(msg.from_ext,"1") && !msg.from_net.type
					? 0:P_NOATCODES);

			reads++;	/* number of messages actually read during this sub-scan */

			/* Message is to this user and hasn't been read, so flag as read */
			if((!stricmp(msg.to,useron.name) || !stricmp(msg.to,useron.alias)
				|| (useron.number==1 && !stricmp(msg.to,"sysop")
				&& !msg.from_net.type))
				&& !(msg.hdr.attr&MSG_READ)) {
				if(msg.total_hfields)
					smb_freemsgmem(&msg);
				msg.total_hfields=0;
				msg.idx.offset=0;
				if(!smb_locksmbhdr(&smb)) { 			  /* Lock the entire base */
					if(loadmsg(&msg,msg.idx.number)) {
						msg.hdr.attr|=MSG_READ;
						msg.idx.attr=msg.hdr.attr;
						if((i=smb_putmsg(&smb,&msg))!=0)
							errormsg(WHERE,ERR_WRITE,smb.file,i);
						smb_unlockmsghdr(&smb,&msg); }
					smb_unlocksmbhdr(&smb); }
				if(!msg.total_hfields) {				/* unsuccessful reload */
					domsg=0;
					continue; } }

			subscan[subnum].last=post[smb.curmsg].number;

			if(subscan[subnum].ptr<post[smb.curmsg].number && !(mode&SCAN_TOYOU)) {
				posts_read++;
				subscan[subnum].ptr=post[smb.curmsg].number; } }
		else domsg=1;
		if(mode&SCAN_CONST) {
			if(smb.curmsg<smb.msgs-1) smb.curmsg++;
				else done=1;
			continue; }
		if(useron.misc&WIP)
			menu("msgscan");
		ASYNC;
		bprintf(text[ReadingSub],ugrp,cfg.grp[cfg.sub[subnum]->grp]->sname
			,usub,cfg.sub[subnum]->sname,smb.curmsg+1,smb.msgs);
		sprintf(str,"ABCDEFILMPQRTY?<>[]{}-+.,");
		if(sub_op(subnum))
			strcat(str,"O");
		reread=0;
		l=getkeys(str,smb.msgs);
		if(l&0x80000000L) {
			if((long)l==-1) { /* ctrl-c */
				if(msg.total_hfields)
					smb_freemsgmem(&msg);
				if(post)
					LFREE(post);
				smb_close(&smb);
				smb_stack(&smb,SMB_STACK_POP);
				return(1); }
			smb.curmsg=(l&~0x80000000L)-1;
			reread=1;
			continue; }
		switch(l) {
			case 'A':   
			case 'R':   
				if((char)l==(cfg.sys_misc&SM_RA_EMU ? 'A' : 'R')) { 
					reread=1;	/* re-read last message */
					break;
				}
				/* Reply to last message */
				domsg=0;
				if(!chk_ar(cfg.sub[subnum]->post_ar,&useron)) {
					bputs(text[CantPostOnSub]);
					break; }
				quotemsg(&msg,0);
				FREE_AND_NULL(post);
				postmsg(subnum,&msg,WM_QUOTE);
				if(mode&SCAN_TOYOU)
					domsg=1;
				break;
			case 'B':   /* Skip sub-board */
				if(mode&SCAN_NEW && !noyes(text[RemoveFromNewScanQ]))
					subscan[subnum].cfg&=~SUB_CFG_NSCAN;
				if(msg.total_hfields)
					smb_freemsgmem(&msg);
				if(post)
					LFREE(post);
				smb_close(&smb);
				smb_stack(&smb,SMB_STACK_POP);
				return(0);
			case 'C':   /* Continuous */
				mode|=SCAN_CONST;
				if(smb.curmsg<smb.msgs-1) smb.curmsg++;
				else done=1;
				break;
			case 'D':       /* Delete message on sub-board */
				domsg=0;
				if(!sub_op(subnum)) {
					if(!(cfg.sub[subnum]->misc&SUB_DEL)) {
						bputs(text[CantDeletePosts]);
						domsg=0;
						break; 
					}
					if(cfg.sub[subnum]->misc&SUB_DELLAST && smb.curmsg!=(smb.msgs-1)) {
						bputs("\1n\r\nCan only delete last message.\r\n");
						domsg=0;
						break;
					}
					if(stricmp(cfg.sub[subnum]->misc&SUB_NAME
						? useron.name : useron.alias, msg.from)
					&& stricmp(cfg.sub[subnum]->misc&SUB_NAME
						? useron.name : useron.alias, msg.to)) {
						bprintf(text[YouDidntPostMsgN],smb.curmsg+1);
						break; 
					} 
				}
				if(msg.hdr.attr&MSG_PERMANENT) {
					bputs("\1n\r\nMessage is marked permanent.\r\n");
					domsg=0;
					break; 
				}

				FREE_AND_NULL(post);

				if(msg.total_hfields)
					smb_freemsgmem(&msg);
				msg.total_hfields=0;
				msg.idx.offset=0;
				if(loadmsg(&msg,msg.idx.number)) {
					msg.idx.attr^=MSG_DELETE;
					msg.hdr.attr=msg.idx.attr;
					if((i=smb_putmsg(&smb,&msg))!=0)
						errormsg(WHERE,ERR_WRITE,smb.file,i);
					smb_unlockmsghdr(&smb,&msg);
					if(i==0 && msg.idx.attr&MSG_DELETE) {
						sprintf(str,"%s removed post from %s %s"
							,useron.alias
							,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->lname);
						logline("P-",str);
						if(!stricmp(cfg.sub[subnum]->misc&SUB_NAME
							? useron.name : useron.alias, msg.from))
							useron.posts=(ushort)adjustuserrec(&cfg,useron.number
								,U_POSTS,5,-1); } }
				domsg=1;
				if((cfg.sys_misc&SM_SYSVDELM		// anyone can view delete msgs
					|| (cfg.sys_misc&SM_USRVDELM	// sys/subops can view deleted msgs
					&& sub_op(subnum)))
					&& smb.curmsg<smb.msgs-1)
					smb.curmsg++;
				if(smb.curmsg>=smb.msgs-1)
					done=1;
				break;
			case 'E':   /* edit last post */
				if(!sub_op(subnum)) {
					if(!(cfg.sub[subnum]->misc&SUB_EDIT)) {
						bputs("\1n\r\nCan't edit messages on this message base.\r\n");
						// bputs(text[CantDeletePosts]);
						domsg=0;
						break; 
					}
					if(cfg.sub[subnum]->misc&SUB_EDITLAST && smb.curmsg!=(smb.msgs-1)) {
						bputs("\1n\r\nCan only edit last message.\r\n");
						domsg=0;
						break;
					}
					if(stricmp(cfg.sub[subnum]->misc&SUB_NAME
						? useron.name : useron.alias, msg.from)) {
						bprintf(text[YouDidntPostMsgN],smb.curmsg+1);
						domsg=0;
						break; 
					} 
				}
				FREE_AND_NULL(post);
				editmsg(&msg,subnum);
				break;
			case 'F':   /* find text in messages */
				domsg=0;
				mode&=~SCAN_FIND;	/* turn off find mode */
				if((i=get_start_msg(this,&smb))<0)
					break;
				bputs(text[SearchStringPrompt]);
				if(!getstr(find_buf,40,K_LINE|K_UPPER))
					break;
				if(yesno(text[DisplaySubjectsOnlyQ]))
					searchposts(subnum,post,(long)i,smb.msgs,find_buf);
				else {
					smb.curmsg=i;
					find=find_buf;
					mode|=SCAN_FIND;
					domsg=1;
				}
				break;
			case 'I':   /* Sub-board information */
				domsg=0;
				subinfo(subnum);
				break;
			case 'L':   /* List messages */
				domsg=0;
				if((i=get_start_msg(this,&smb))<0)
					break;
				listmsgs(subnum,post,i,smb.msgs);
				sys_status&=~SS_ABORT;
				break;
			case 'M':   /* Reply to last post in mail */
				domsg=0;
				if(msg.hdr.attr&MSG_ANONYMOUS && !sub_op(subnum)) {
					bputs(text[CantReplyToAnonMsg]);
					break; }
				if(!sub_op(subnum) && msg.hdr.attr&MSG_PRIVATE
					&& stricmp(msg.to,useron.name)
					&& stricmp(msg.to,useron.alias))
					break;
				sprintf(str2,text[Regarding]
					,msg.subj
					,timestr((time_t *)&msg.hdr.when_written.time));
				if(msg.from_net.addr==NULL)
					strcpy(str,msg.from);
				else if(msg.from_net.type==NET_FIDO)
					sprintf(str,"%s @%s",msg.from
						,faddrtoa((faddr_t *)msg.from_net.addr,tmp));
				else if(msg.from_net.type==NET_INTERNET)
					strcpy(str,(char *)msg.from_net.addr);
				else
					sprintf(str,"%s@%s",msg.from,(char *)msg.from_net.addr);
				bputs(text[Email]);
				if(!getstr(str,60,K_EDIT|K_AUTODEL))
					break;

				FREE_AND_NULL(post);
				quotemsg(&msg,1);
				if(msg.from_net.type==NET_INTERNET && strchr(str,'@'))
					inetmail(str,msg.subj,WM_QUOTE|WM_NETMAIL);
				else {
					p=strrchr(str,'@');
					if(p)								/* FidoNet or QWKnet */
						netmail(str,msg.subj,WM_QUOTE);
					else {
						i=atoi(str);
						if(!i) {
							if(cfg.sub[subnum]->misc&SUB_NAME)
								i=userdatdupe(0,U_NAME,LEN_NAME,str,0);
							else
								i=matchuser(&cfg,str,TRUE /* sysop_alias */); }
						email(i,str2,msg.subj,WM_EMAIL|WM_QUOTE); } }
				break;
			case 'P':   /* Post message on sub-board */
				domsg=0;
				if(!chk_ar(cfg.sub[subnum]->post_ar,&useron))
					bputs(text[CantPostOnSub]);
				else {
					FREE_AND_NULL(post);
					postmsg(subnum,0,0);
				}
				break;
			case 'Q':   /* Quit */
				if(msg.total_hfields)
					smb_freemsgmem(&msg);
				if(post)
					LFREE(post);
				smb_close(&smb);
				smb_stack(&smb,SMB_STACK_POP);
				return(1);
			case 'T':   /* List titles of next ten messages */
				domsg=0;
				if(!smb.msgs)
					break;
				if(smb.curmsg>=smb.msgs-1) {
					 done=1;
					 break; }
				i=smb.curmsg+11;
				if(i>smb.msgs)
					i=smb.msgs;
				listmsgs(subnum,post,smb.curmsg+1,i);
				smb.curmsg=i-1;
				if(subscan[subnum].ptr<post[smb.curmsg].number)
					subscan[subnum].ptr=post[smb.curmsg].number;
				break;
			case 'Y':   /* Your messages */
				domsg=0;
				showposts_toyou(post,0,smb.msgs);
				break;
			case '-':
				if(smb.curmsg>0) smb.curmsg--;
				reread=1;
				break;
			case 'O':   /* Operator commands */
				while(online) {
					if(!(useron.misc&EXPERT))
						menu("sysmscan");
					bprintf("\r\n\1y\1hOperator: \1w");
					strcpy(str,"?CEHMPQUV");
					if(SYSOP)
						strcat(str,"S");
					switch(getkeys(str,0)) {
						case '?':
							if(useron.misc&EXPERT)
								menu("sysmscan");
							continue;
						case 'P':   /* Purge user */
							purgeuser(cfg.sub[subnum]->misc&SUB_NAME
								? userdatdupe(0,U_NAME,LEN_NAME,msg.from,0)
								: matchuser(&cfg,msg.from,FALSE));
							break;
						case 'C':   /* Change message attributes */
							i=chmsgattr(msg.hdr.attr);
							if(msg.hdr.attr==i)
								break;
							if(msg.total_hfields)
								smb_freemsgmem(&msg);
							msg.total_hfields=0;
							msg.idx.offset=0;
							if(loadmsg(&msg,msg.idx.number)) {
								msg.hdr.attr=msg.idx.attr=i;
								if((i=smb_putmsg(&smb,&msg))!=0)
									errormsg(WHERE,ERR_WRITE,smb.file,i);
								smb_unlockmsghdr(&smb,&msg); }
							break;
						case 'E':   /* edit last post */
							FREE_AND_NULL(post);
							editmsg(&msg,subnum);
							break;
						case 'H':   /* View message header */
							msghdr(&msg);
							domsg=0;
							break;
						case 'M':   /* Move message */
							domsg=0;
							FREE_AND_NULL(post);
							if(msg.total_hfields)
								smb_freemsgmem(&msg);
							msg.total_hfields=0;
							msg.idx.offset=0;
							if(!loadmsg(&msg,msg.idx.number)) {
								errormsg(WHERE,ERR_READ,smb.file,msg.idx.number);
								break; }
							sprintf(str,text[DeletePostQ],msg.hdr.number,msg.subj);
							if(movemsg(&msg,subnum) && yesno(str)) {
								msg.idx.attr|=MSG_DELETE;
								msg.hdr.attr=msg.idx.attr;
								if((i=smb_putmsg(&smb,&msg))!=0)
									errormsg(WHERE,ERR_WRITE,smb.file,i); }
							smb_unlockmsghdr(&smb,&msg);
							break;

						case 'Q':
							break;
						case 'S':   /* Save/Append message to another file */
	/*	05/26/95
							if(!yesno(text[SaveMsgToFile]))
								break;
	*/
							bputs(text[FileToWriteTo]);
							if(getstr(str,40,K_LINE|K_UPPER))
								msgtotxt(&msg,str,1,1);
							break;
						case 'U':   /* User edit */
							useredit(cfg.sub[subnum]->misc&SUB_NAME
								? userdatdupe(0,U_NAME,LEN_NAME,msg.from,0)
								: matchuser(&cfg,msg.from,TRUE /* sysop_alias */));
							break;
						case 'V':   /* Validate message */
							if(msg.total_hfields)
								smb_freemsgmem(&msg);
							msg.total_hfields=0;
							msg.idx.offset=0;
							if(loadmsg(&msg,msg.idx.number)) {
								msg.idx.attr|=MSG_VALIDATED;
								msg.hdr.attr=msg.idx.attr;
								if((i=smb_putmsg(&smb,&msg))!=0)
									errormsg(WHERE,ERR_WRITE,smb.file,i);
								smb_unlockmsghdr(&smb,&msg); }
							break;
						default:
							continue; }
					break; }
				break;
			case '.':   /* Thread forward */
				l=msg.hdr.thread_first;
				if(!l) l=msg.hdr.thread_next;
				if(!l) {
					domsg=0;
					break; }
				for(i=0;i<smb.msgs;i++)
					if(l==post[i].number)
						break;
				if(i<smb.msgs)
					smb.curmsg=i;
				break;
			case ',':   /* Thread backwards */
				if(!msg.hdr.thread_orig) {
					domsg=0;
					break; }
				for(i=0;i<smb.msgs;i++)
					if(msg.hdr.thread_orig==post[i].number)
						break;
				if(i<smb.msgs)
					smb.curmsg=i;
				break;
			case '>':   /* Search Title forward */
				for(i=smb.curmsg+1;i<smb.msgs;i++)
					if(post[i].subj==msg.idx.subj)
						break;
				if(i<smb.msgs)
					smb.curmsg=i;
				else
					domsg=0;
				break;
			case '<':   /* Search Title backward */
				for(i=smb.curmsg-1;i>-1;i--)
					if(post[i].subj==msg.idx.subj)
						break;
				if(i>-1)
					smb.curmsg=i;
				else
					domsg=0;
				break;
			case '}':   /* Search Author forward */
				strcpy(str,msg.from);
				for(i=smb.curmsg+1;i<smb.msgs;i++)
					if(post[i].from==msg.idx.from)
						break;
				if(i<smb.msgs)
					smb.curmsg=i;
				else
					domsg=0;
				break;
			case '{':   /* Search Author backward */
				strcpy(str,msg.from);
				for(i=smb.curmsg-1;i>-1;i--)
					if(post[i].from==msg.idx.from)
						break;
				if(i>-1)
					smb.curmsg=i;
				else
					domsg=0;
				break;
			case ']':   /* Search To User forward */
				strcpy(str,msg.to);
				for(i=smb.curmsg+1;i<smb.msgs;i++)
					if(post[i].to==msg.idx.to)
						break;
				if(i<smb.msgs)
					smb.curmsg=i;
				else
					domsg=0;
				break;
			case '[':   /* Search To User backward */
				strcpy(str,msg.to);
				for(i=smb.curmsg-1;i>-1;i--)
					if(post[i].to==msg.idx.to)
						break;
				if(i>-1)
					smb.curmsg=i;
				else
					domsg=0;
				break;
			case 0: /* Carriage return - Next Message */
			case '+':
				if(smb.curmsg<smb.msgs-1) smb.curmsg++;
				else done=1;
				break;
			case '?':
				menu("msgscan");
				domsg=0;
				break;	} }
	if(msg.total_hfields)
		smb_freemsgmem(&msg);
	if(post)
		LFREE(post);
	if(!(org_mode&(SCAN_CONST|SCAN_TOYOU|SCAN_FIND)) && !(cfg.sub[subnum]->misc&SUB_PONLY)
		&& reads && chk_ar(cfg.sub[subnum]->post_ar,&useron)
		&& !(useron.rest&FLAG('P'))) {
		sprintf(str,text[Post],cfg.grp[cfg.sub[subnum]->grp]->sname
			,cfg.sub[subnum]->lname);
		if(!noyes(str))
			postmsg(subnum,0,0); }
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	return(0);
}

/****************************************************************************/
/* This function will search the specified sub-board for messages that      */
/* contain the string 'search'.                                             */
/* Returns number of messages found.                                        */
/****************************************************************************/
int sbbs_t::searchsub(uint subnum, char *search)
{
	int 	i,found;
	long	posts;
	ulong	total;
	post_t	HUGE16 *post;

	if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
		errormsg(WHERE,ERR_OPEN,cfg.sub[subnum]->code,i);
		return(0); }
	total=getposts(&cfg,subnum);
	sprintf(smb.file,"%s%s",cfg.sub[subnum]->data_dir,cfg.sub[subnum]->code);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=subnum;
	if((i=smb_open(&smb))!=0) {
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(0); }
	post=loadposts(&posts,subnum,0,LP_BYSELF|LP_OTHERS);
	bprintf(text[SearchSubFmt]
		,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->lname,posts,total);
	found=searchposts(subnum,post,0,posts,search);
	if(posts)
		LFREE(post);
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	return(found);
}

/****************************************************************************/
/* Will search the messages pointed to by 'msg' for the occurance of the    */
/* string 'search' and display any messages (number of message, author and  */
/* title). 'msgs' is the total number of valid messages.                    */
/* Returns number of messages found.                                        */
/****************************************************************************/
int sbbs_t::searchposts(uint subnum, post_t HUGE16 *post, long start, long posts
	, char *search)
{
	char	HUGE16 *buf,ch;
	long	l,found=0;
	smbmsg_t msg;

	msg.total_hfields=0;
	for(l=start;l<posts && !msgabort();l++) {
		msg.idx.offset=post[l].offset;
		if(!loadmsg(&msg,post[l].number))
			continue;
		smb_unlockmsghdr(&smb,&msg);
		buf=smb_getmsgtxt(&smb,&msg,1);
		if(!buf) {
			smb_freemsgmem(&msg);
			continue; }
		strupr((char *)buf);
		if(strstr((char *)buf,search) || strstr(msg.subj,search)) {
			if(!found)
				CRLF;
			if(msg.hdr.attr&MSG_DELETE)
				ch='-';
			else if((!stricmp(msg.to,useron.alias) || !stricmp(msg.to,useron.name))
				&& !(msg.hdr.attr&MSG_READ))
				ch='!';
			else if(msg.hdr.number>subscan[subnum].ptr)
				ch='*';
			else
				ch=' ';
			bprintf(text[SubMsgLstFmt],l+1
				,(msg.hdr.attr&MSG_ANONYMOUS) && !sub_op(subnum) ? text[Anonymous]
				: msg.from
				,msg.to
				,ch
				,msg.subj);
			found++; }
		FREE(buf);
		smb_freemsgmem(&msg); }

	return(found);
}

/****************************************************************************/
/* Will search the messages pointed to by 'msg' for message to the user on  */
/* Returns number of messages found.                                        */
/****************************************************************************/
void sbbs_t::showposts_toyou(post_t HUGE16 *post, ulong start, long posts)
{
	char	str[128];
	ushort	namecrc,aliascrc,sysop;
	long	l,found;
    smbmsg_t msg;

	strcpy(str,useron.alias);
	strlwr(str);
	aliascrc=crc16(str);
	strcpy(str,useron.name);
	strlwr(str);
	namecrc=crc16(str);
	sysop=crc16("sysop");
	msg.total_hfields=0;
	for(l=start,found=0;l<posts && !msgabort();l++) {

		if((useron.number!=1 || post[l].to!=sysop)
			&& post[l].to!=aliascrc && post[l].to!=namecrc)
			continue;

		if(msg.total_hfields)
			smb_freemsgmem(&msg);
		msg.total_hfields=0;
		msg.idx.offset=post[l].offset;
		if(!loadmsg(&msg,post[l].number))
			continue;
		smb_unlockmsghdr(&smb,&msg);
		if((useron.number==1 && !stricmp(msg.to,"sysop") && !msg.from_net.type)
			|| !stricmp(msg.to,useron.alias) || !stricmp(msg.to,useron.name)) {
			if(!found)
				CRLF;
			found++;
			bprintf(text[SubMsgLstFmt],l+1
				,(msg.hdr.attr&MSG_ANONYMOUS) && !SYSOP
				? text[Anonymous] : msg.from
				,msg.to
				,msg.hdr.attr&MSG_DELETE ? '-' : msg.hdr.attr&MSG_READ ? ' ' : '*'
				,msg.subj); } }

	if(msg.total_hfields)
		smb_freemsgmem(&msg);
}

/****************************************************************************/
/* This function will search the specified sub-board for messages that		*/
/* are sent to the currrent user.											*/
/* returns number of messages found 										*/
/****************************************************************************/
int sbbs_t::searchsub_toyou(uint subnum)
{
	int 	i;
	long	posts;
	ulong	total;
	post_t	HUGE16 *post;

	if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
		errormsg(WHERE,ERR_OPEN,cfg.sub[subnum]->code,i);
		return(0); }
	total=getposts(&cfg,subnum);
	sprintf(smb.file,"%s%s",cfg.sub[subnum]->data_dir,cfg.sub[subnum]->code);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=subnum;
	if((i=smb_open(&smb))!=0) {
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(0); }
	post=loadposts(&posts,subnum,0,0);
	bprintf(text[SearchSubFmt]
		,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->lname,total);
	if(posts) {
		if(post)
			LFREE(post);
		post=loadposts(&posts,subnum,0,LP_BYSELF|LP_OTHERS);
		showposts_toyou(post,0,posts); }
	if(post)
		LFREE(post);
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	return(posts);
}


