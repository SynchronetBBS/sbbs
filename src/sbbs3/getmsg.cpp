/* getmsg.cpp */

/* Synchronet message retrieval functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

/***********************************************************************/
/* Functions that do i/o with messages (posts/mail/auto) or their data */
/***********************************************************************/

#include "sbbs.h"

#define LZH TRUE

void lfputs(char HUGE16 *buf, FILE *fp)
{
	while(*buf) {
		fputc(*buf,fp);
		buf++; }
}

/****************************************************************************/
/* Loads an SMB message from the open msg base the fastest way possible 	*/
/* first by offset, and if that's the wrong message, then by number.        */
/* Returns 1 if the message was loaded and left locked, otherwise			*/
/* !WARNING!: If you're going to write the msg index back to disk, you must */
/* Call this function with a msg->idx.offset of 0 (so msg->offset will be	*/
/* initialized correctly)													*/
/****************************************************************************/
int sbbs_t::loadmsg(smbmsg_t *msg, ulong number)
{
	char str[128];
	int i;

	if(msg->idx.offset) {				/* Load by offset if specified */

		if((i=smb_lockmsghdr(&smb,msg))!=0) {
			errormsg(WHERE,ERR_LOCK,smb.file,i,smb.last_error);
			return(0); 
		}

		i=smb_getmsghdr(&smb,msg);
		if(!i && msg->hdr.number==number)
			return(1);

		/* Wrong offset  */

		if(!i) {
			smb_freemsgmem(msg);
			msg->total_hfields=0; 
		}

		smb_unlockmsghdr(&smb,msg); 
	}

	msg->hdr.number=number;
	if((i=smb_getmsgidx(&smb,msg))!=0)				 /* Message is deleted */
		return(0);
	if((i=smb_lockmsghdr(&smb,msg))!=0) {
		errormsg(WHERE,ERR_LOCK,smb.file,i,smb.last_error);
		return(0); 
	}
	if((i=smb_getmsghdr(&smb,msg))!=0) {
		sprintf(str,"(%06lX) #%lu/%lu %s",msg->idx.offset,msg->idx.number
			,number,smb.file);
		smb_unlockmsghdr(&smb,msg);
		errormsg(WHERE,ERR_READ,str,i,smb.last_error);
		return(0); 
	}
	return(msg->total_hfields);
}


void sbbs_t::show_msgattr(ushort attr)
{

	bprintf(text[MsgAttr]
		,attr&MSG_PRIVATE	? "Private  "   :nulstr
		,attr&MSG_READ		? "Read  "      :nulstr
		,attr&MSG_DELETE	? "Deleted  "   :nulstr
		,attr&MSG_KILLREAD	? "Kill  "      :nulstr
		,attr&MSG_ANONYMOUS ? "Anonymous  " :nulstr
		,attr&MSG_LOCKED	? "Locked  "    :nulstr
		,attr&MSG_PERMANENT ? "Permanent  " :nulstr
		,attr&MSG_MODERATED ? "Moderated  " :nulstr
		,attr&MSG_VALIDATED ? "Validated  " :nulstr
		,attr&MSG_REPLIED	? "Replied  "	:nulstr
		,nulstr
		,nulstr
		,nulstr
		,nulstr
		,nulstr
		,nulstr
		);
}

/****************************************************************************/
/* Displays a message header to the screen                                  */
/****************************************************************************/
void sbbs_t::show_msghdr(smbmsg_t* msg)
{
	char	str[MAX_PATH+1];
	char	*sender=NULL;
	int 	i;

	current_msg=msg;
	attr(LIGHTGRAY);
	if(useron.misc&CLRSCRN)
		outchar(FF);
	else
		CRLF;

	sprintf(str,"%smenu/msghdr.*", cfg.text_dir);
	if(fexist(str)) {
		menu("msghdr");
		current_msg=NULL;
		return; 
	}

	bprintf(text[MsgSubj],msg->subj);
	if(msg->hdr.attr)
		show_msgattr(msg->hdr.attr);

	bprintf(text[MsgTo],msg->to);
	if(msg->to_ext)
		bprintf(text[MsgToExt],msg->to_ext);
	if(msg->to_net.addr)
		bprintf(text[MsgToNet],net_addr(&msg->to_net));
	if(!(msg->hdr.attr&MSG_ANONYMOUS) || SYSOP) {
		bprintf(text[MsgFrom],msg->from);
		if(msg->from_ext)
			bprintf(text[MsgFromExt],msg->from_ext);
		if(msg->from_net.addr && !strchr(msg->from,'@'))
			bprintf(text[MsgFromNet],net_addr(&msg->from_net)); 
	}
	bprintf(text[MsgDate]
		,timestr((time_t *)&msg->hdr.when_written.time)
		,zonestr(msg->hdr.when_written.zone));

	CRLF;

	for(i=0;i<msg->total_hfields;i++) {
		if(msg->hfield[i].type==SENDER)
			sender=(char *)msg->hfield_dat[i];
		if(msg->hfield[i].type==FORWARDED && sender)
			bprintf(text[ForwardedFrom],sender
				,timestr((time_t *)msg->hfield_dat[i])); }

	/* Debug stuff
	if(SYSOP) {
		bprintf("\1n\1c\r\nAux  : \1h%08lX",msg->hdr.auxattr);
		bprintf("\1n\1c\r\nNum  : \1h%lu",msg->hdr.number); }
	*/

	CRLF;
	current_msg=NULL;
}

#if !LZH

/****************************************************************************/
/* Displays message header and text (if not deleted)                        */
/****************************************************************************/
void sbbs_t::show_msg(smbmsg_t msg, long mode)
{
	ushort xlat;
    int i;

	show_msghdr(msg);
	for(i=0;i<msg.hdr.total_dfields;i++)
		switch(msg.dfield[i].type) {
			case TEXT_BODY:
			case TEXT_TAIL:
				smb_fseek(smb.sdt_fp,msg.hdr.offset+msg.dfield[i].offset
					,SEEK_SET);
				smb_fread(&xlat,sizeof(xlat),1,smb.sdt_fp);
				if(xlat!=XLAT_NONE) 		/* no translations supported */
					continue;
				putmsg_fp(smb.sdt_fp,msg.dfield[i].length-sizeof(xlat),mode);
				CRLF;
				break; }
}

#else

/****************************************************************************/
/* Displays message header and text (if not deleted)                        */
/****************************************************************************/
void sbbs_t::show_msg(smbmsg_t* msg, long mode)
{
	uchar *inbuf,*lzhbuf;
	ushort xlat;
	int i,lzh;
	long lzhlen,length;

	show_msghdr(msg);
	for(i=0;i<msg->hdr.total_dfields;i++)
		switch(msg->dfield[i].type) {
			case TEXT_BODY:
			case TEXT_TAIL:
				if(msg->dfield[i].length < sizeof(xlat))	/* Invalid length */
					continue;
				fseek(smb.sdt_fp,msg->hdr.offset+msg->dfield[i].offset
					,SEEK_SET);
				fread(&xlat,sizeof(xlat),1,smb.sdt_fp);
				lzh=0;
				if(xlat==XLAT_LZH) {
					lzh=1;
					fread(&xlat,sizeof(xlat),1,smb.sdt_fp); }
				if(xlat!=XLAT_NONE) 		/* no translations supported */
					continue;
				if(lzh) {
					length=msg->dfield[i].length-4;
					if((inbuf=(uchar *)MALLOC(length))==NULL) {
						errormsg(WHERE,ERR_ALLOC,nulstr,length);
						continue; }
					fread(inbuf,length,1,smb.sdt_fp);
					lzhlen=*(long *)inbuf;
	/**
					if(SYSOP)
					bprintf("Decoding %lu bytes of LZH into %lu bytes of text "
						"(%d%% compression)"
						,length,lzhlen
						,(int)(((float)(lzhlen-length)/lzhlen)*100.0));
	**/
					if((lzhbuf=(uchar *)MALLOC(lzhlen+2L))==NULL) {
						FREE(inbuf);
						errormsg(WHERE,ERR_ALLOC,nulstr,lzhlen+2L);
						continue; }
					lzh_decode(inbuf,length,lzhbuf);
					lzhbuf[lzhlen]=0;
	//				  CRLF;
					putmsg((char *)lzhbuf,P_NOATCODES);
					FREE(lzhbuf);
					FREE(inbuf); }
				else
					putmsg_fp(smb.sdt_fp,msg->dfield[i].length-sizeof(xlat),mode);
				CRLF;
				break; 
	}
}

#endif

void sbbs_t::quotemsg(smbmsg_t* msg, int tails)
{
	char	str[256];
	char	tmp[13];

	strcpy(tmp,"QUOTES.TXT");
	if(useron.xedit && cfg.xedit[useron.xedit-1]->misc&XTRN_LWRCASE)
		strlwr(tmp);
	sprintf(str,"%s%s",cfg.node_dir,tmp);
	remove(str);
	msgtotxt(msg,str,0,tails);
}


/****************************************************************************/
/* Writes message header and text data to a text file						*/
/****************************************************************************/
void sbbs_t::msgtotxt(smbmsg_t* msg, char *str, int header, int tails)
{
	char	HUGE16 *buf;
	int 	i;
	FILE	*out;

	if((out=fnopen(&i,str,O_WRONLY|O_CREAT|O_APPEND))==NULL) {
		errormsg(WHERE,ERR_OPEN,str,0);
		return; }
	if(header) {
		fprintf(out,"\r\n");
		fprintf(out,"Subj : %s\r\n",msg->subj);
		fprintf(out,"To   : %s",msg->to);
		if(msg->to_ext)
			fprintf(out," #%s",msg->to_ext);
		if(msg->to_net.addr)
			fprintf(out," (%s)",net_addr(&msg->to_net));
		fprintf(out,"\r\nFrom : %s",msg->from);
		if(msg->from_ext && !(msg->hdr.attr&MSG_ANONYMOUS))
			fprintf(out," #%s",msg->from_ext);
		if(msg->from_net.addr)
			fprintf(out," (%s)",net_addr(&msg->from_net));
		fprintf(out,"\r\nDate : %.24s %s"
			,timestr((time_t *)&msg->hdr.when_written.time)
			,zonestr(msg->hdr.when_written.zone));
		fprintf(out,"\r\n\r\n"); }

	buf=smb_getmsgtxt(&smb,msg,tails);
	if(buf!=NULL) {
		lfputs(buf,out);
		LFREE(buf); 
	} else if(smb_getmsgdatlen(msg)>2)
		errormsg(WHERE,ERR_READ,smb.file,smb_getmsgdatlen(msg));
	fclose(out);
}

/****************************************************************************/
/* Returns message number posted at or after time							*/
/****************************************************************************/
ulong sbbs_t::getmsgnum(uint subnum, time_t t)
{
    int     i;
	ulong	l,total,bot,top;
	smbmsg_t msg;

	if(!t)
		return(0);

	sprintf(smb.file,"%s%s",cfg.sub[subnum]->data_dir,cfg.sub[subnum]->code);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=subnum;
	if((i=smb_open(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(0); }

	total=filelength(fileno(smb.sid_fp))/sizeof(idxrec_t);

	if(!total) {		   /* Empty base */
		smb_close(&smb);
		return(0); }

	if((i=smb_locksmbhdr(&smb))!=0) {
		smb_close(&smb);
		errormsg(WHERE,ERR_LOCK,smb.file,i,smb.last_error);
		return(0); }

	if((i=smb_getlastidx(&smb,&msg.idx))!=0) {
		smb_close(&smb);
		errormsg(WHERE,ERR_READ,smb.file,i,smb.last_error);
		return(0); }

	if((time_t)msg.idx.time<=t) {
		smb_close(&smb);
		return(msg.idx.number); }

	bot=0;
	top=total;
	l=total/2; /* Start at middle index */
	clearerr(smb.sid_fp);
	while(1) {
		fseek(smb.sid_fp,l*sizeof(idxrec_t),SEEK_SET);
		if(!fread(&msg.idx,sizeof(idxrec_t),1,smb.sid_fp))
			break;
		if(bot==top-1)
			break;
		if((time_t)msg.idx.time>t) {
			top=l;
			l=bot+((top-bot)/2);
			continue; }
		if((time_t)msg.idx.time<t) {
			bot=l;
			l=top-((top-bot)/2);
			continue; }
		break; }
	smb_close(&smb);
	return(msg.idx.number);
}

/****************************************************************************/
/* Returns the time of the message number pointed to by 'ptr'               */
/****************************************************************************/
time_t sbbs_t::getmsgtime(uint subnum, ulong ptr)
{
	int 	i;
	smbmsg_t msg;
	idxrec_t lastidx;

	sprintf(smb.file,"%s%s",cfg.sub[subnum]->data_dir,cfg.sub[subnum]->code);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=subnum;
	if((i=smb_open(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(0); }
	if(!filelength(fileno(smb.sid_fp))) {			/* Empty base */
		smb_close(&smb);
		return(0); }
	msg.offset=0;
	msg.hdr.number=0;
	if(smb_getmsgidx(&smb,&msg)) {				/* Get first message index */
		smb_close(&smb);
		return(0); }
	if(!ptr || msg.idx.number>=ptr) {			/* ptr is before first message */
		smb_close(&smb);
		return(msg.idx.time); } 				/* so return time of first msg */

	if(smb_getlastidx(&smb,&lastidx)) { 			 /* Get last message index */
		smb_close(&smb);
		return(0); }
	if(lastidx.number<ptr) {					/* ptr is after last message */
		smb_close(&smb);
		return(lastidx.time); } 				/* so return time of last msg */

	msg.idx.time=0;
	msg.hdr.number=ptr;
	if(!smb_getmsgidx(&smb,&msg)) {
		smb_close(&smb);
		return(msg.idx.time); }

	if(ptr-msg.idx.number < lastidx.number-ptr) {
		msg.offset=0;
		msg.idx.number=0;
		while(msg.idx.number<ptr) {
			msg.hdr.number=0;
			if(smb_getmsgidx(&smb,&msg) || msg.idx.number>=ptr)
				break;
			msg.offset++; }
		smb_close(&smb);
		return(msg.idx.time); }

	ptr--;
	while(ptr) {
		msg.hdr.number=ptr;
		if(!smb_getmsgidx(&smb,&msg))
			break;
		ptr--; }
	smb_close(&smb);
	return(msg.idx.time);
}


/****************************************************************************/
/* Returns the total number of msgs in the sub-board and sets 'ptr' to the  */
/* number of the last message in the sub (0) if no messages.				*/
/****************************************************************************/
ulong sbbs_t::getlastmsg(uint subnum, ulong *ptr, time_t *t)
{
	int 		i;
	ulong		total;
	idxrec_t	idx;

	if(ptr)
		(*ptr)=0;
	if(t)
		(*t)=0;
	if(subnum>=cfg.total_subs)
		return(0);

	sprintf(smb.file,"%s%s",cfg.sub[subnum]->data_dir,cfg.sub[subnum]->code);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=subnum;
	if((i=smb_open(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(0); }

	if(!filelength(fileno(smb.sid_fp))) {			/* Empty base */
		smb_close(&smb);
		return(0); }
	if((i=smb_locksmbhdr(&smb))!=0) {
		smb_close(&smb);
		errormsg(WHERE,ERR_LOCK,smb.file,i,smb.last_error);
		return(0); }
	if((i=smb_getlastidx(&smb,&idx))!=0) {
		smb_close(&smb);
		errormsg(WHERE,ERR_READ,smb.file,i,smb.last_error);
		return(0); }
	total=filelength(fileno(smb.sid_fp))/sizeof(idxrec_t);
	smb_unlocksmbhdr(&smb);
	smb_close(&smb);
	if(ptr)
		(*ptr)=idx.number;
	if(t)
		(*t)=idx.time;
	return(total);
}

/****************************************************************************/
/* Retrieve a message by RFC822 message-ID									*/
/****************************************************************************/
BOOL DLLCALL get_msg_by_id(scfg_t* scfg, smb_t* smb, char* id, smbmsg_t* msg)
{
	ulong		n;
	int			ret;

	for(n=0;n<smb->status.last_msg;n++) {
		memset(msg,0,sizeof(smbmsg_t));
		msg->offset=n;
		if(smb_getmsgidx(smb, msg)!=0)
			break;

		if(smb_lockmsghdr(smb,msg)!=0)
			continue;

		ret=smb_getmsghdr(smb,msg);

		smb_unlockmsghdr(smb,msg); 

		if(ret!=SMB_SUCCESS)
			continue;

		if(strcmp(get_msgid(scfg,smb->subnum,msg),id)==0)
			return(TRUE);

		smb_freemsgmem(msg);
	}

	return(FALSE);
}
