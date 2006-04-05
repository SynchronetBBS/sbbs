/* qwktomsg.cpp */

/* Synchronet QWK to SMB message conversion routine */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2006 Rob Swindell - http://www.synchro.net/copyright.html		*
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
/* Converts a QWK message packet into a message.							*/
/****************************************************************************/
bool sbbs_t::qwktomsg(FILE *qwk_fp, char *hdrblk, char fromhub, uint subnum
	, uint touser)
{
	char*		body;
	char*		tail;
	char*		header;
	char		str[256],col=0,lastch=0,*p,qwkbuf[QWK_BLOCK_LEN+1];
	uint 		i,j,k,lzh=0,skip=0;
	long		bodylen,taillen;
	bool		header_cont=false;
	bool		success=false;
	ulong		block,blocks;
	smbmsg_t	msg;
	struct		tm tm;
	ushort		xlat=XLAT_NONE;
	int			storage=SMB_SELFPACK;
	long		dupechk_hashes=SMB_HASH_SOURCE_ALL;

	memset(&msg,0,sizeof(smbmsg_t));		/* Initialize message header */
	msg.hdr.version=smb_ver();

	blocks=atol(hdrblk+116);
	if(blocks<2) {
		errormsg(WHERE,ERR_CHK,"QWK packet header blocks",blocks);
		return(false);
	}

	if(subnum!=INVALID_SUB
		&& (hdrblk[0]=='*' || hdrblk[0]=='+' || cfg.sub[subnum]->misc&SUB_PONLY))
		msg.hdr.attr|=MSG_PRIVATE;
	if(subnum!=INVALID_SUB && cfg.sub[subnum]->misc&SUB_AONLY)
		msg.hdr.attr|=MSG_ANONYMOUS;
	if(subnum==INVALID_SUB && cfg.sys_misc&SM_DELREADM)
		msg.hdr.attr|=MSG_KILLREAD;
	if((fromhub || useron.rest&FLAG('Q')) &&
		(hdrblk[0]=='*' || hdrblk[0]=='-' || hdrblk[0]=='`'))
		msg.hdr.attr|=MSG_READ;

	if(subnum!=INVALID_SUB && !fromhub && cfg.sub[subnum]->mod_ar[0]
		&& chk_ar(cfg.sub[subnum]->mod_ar,&useron))
		msg.hdr.attr|=MSG_MODERATED;
	if(subnum!=INVALID_SUB && !fromhub && cfg.sub[subnum]->misc&SUB_SYSPERM
		&& sub_op(subnum))
		msg.hdr.attr|=MSG_PERMANENT;

	memset(&tm,0,sizeof(tm));
	tm.tm_mon = ((hdrblk[8]&0xf)*10)+(hdrblk[9]&0xf);
	if(tm.tm_mon>0) tm.tm_mon--;	/* zero based */
	tm.tm_mday=((hdrblk[11]&0xf)*10)+(hdrblk[12]&0xf);
	tm.tm_year=((hdrblk[14]&0xf)*10)+(hdrblk[15]&0xf);
	if(tm.tm_year<Y2K_2DIGIT_WINDOW)
		tm.tm_year+=100;
	tm.tm_hour=((hdrblk[16]&0xf)*10)+(hdrblk[17]&0xf);
	tm.tm_min=((hdrblk[19]&0xf)*10)+(hdrblk[20]&0xf);
	tm.tm_sec=0;
	tm.tm_isdst=-1;	/* Do not adjust for DST */

	msg.hdr.when_written.time=mktime(&tm);
	if(!(useron.rest&FLAG('Q')) && !fromhub)
		msg.hdr.when_written.zone=sys_timezone(&cfg);
	msg.hdr.when_imported.time=time(NULL);
	msg.hdr.when_imported.zone=sys_timezone(&cfg);

	hdrblk[116]=0;	// don't include number of blocks in "re: msg number"
	if(!(useron.rest&FLAG('Q')) && !fromhub)
		msg.hdr.thread_back=atol((char *)hdrblk+108);

	if(subnum==INVALID_SUB) { 		/* E-mail */
		if(cfg.sys_misc&SM_FASTMAIL)
			storage=SMB_FASTALLOC;

		/* duplicate message-IDs must be allowed in mail database */
		dupechk_hashes&=~(1<<SMB_HASH_SOURCE_MSG_ID);

		username(&cfg,touser,str);
		smb_hfield_str(&msg,RECIPIENT,str);
		sprintf(str,"%u",touser);
		smb_hfield_str(&msg,RECIPIENTEXT,str); 
	} else {
		if(cfg.sub[subnum]->misc&SUB_HYPER)
			storage = SMB_HYPERALLOC;
		else if(cfg.sub[subnum]->misc&SUB_FAST)
			storage = SMB_FASTALLOC;

		if(cfg.sub[subnum]->misc&SUB_LZH)
			xlat=XLAT_LZH;

		sprintf(str,"%25.25s",(char *)hdrblk+21);     /* To user */
		truncsp(str);
		smb_hfield_str(&msg,RECIPIENT,str);
		if(cfg.sub[subnum]->misc&SUB_LZH)
			xlat=XLAT_LZH;
	}

	sprintf(str,"%25.25s",hdrblk+71);   /* Subject */
	truncsp(str);
	smb_hfield_str(&msg,SUBJECT,str);

	/********************************/
	/* Convert the QWK message text */
	/********************************/

	if((header=(char *)calloc((blocks-1L)*QWK_BLOCK_LEN*2L,sizeof(char)))==NULL) {
		smb_freemsgmem(&msg);
		errormsg(WHERE,ERR_ALLOC,"QWK msg header",(blocks-1L)*QWK_BLOCK_LEN*2L);
		return(false); 
	}

	bodylen=0;
	if((body=(char *)malloc((blocks-1L)*QWK_BLOCK_LEN*2L))==NULL) {
		free(header);
		smb_freemsgmem(&msg);
		errormsg(WHERE,ERR_ALLOC,"QWK msg body",(blocks-1L)*QWK_BLOCK_LEN*2L);
		return(false); 
	}

	taillen=0;
	if((tail=(char *)malloc((blocks-1L)*QWK_BLOCK_LEN*2L))==NULL) {
		free(header);
		free(body);
		smb_freemsgmem(&msg);
		errormsg(WHERE,ERR_ALLOC,"QWK msg tail",(blocks-1L)*QWK_BLOCK_LEN*2L);
		return(false); 
	}

	memset(qwkbuf,0,sizeof(qwkbuf));

	for(block=1;block<blocks;block++) {
		if(!fread(qwkbuf,1,QWK_BLOCK_LEN,qwk_fp))
			break;
		for(k=0;k<QWK_BLOCK_LEN;k++) {
			if(qwkbuf[k]==0)
				continue;
			if(bodylen==0 && (qwkbuf[k]=='@' || header_cont)) {
				if((p=strchr(qwkbuf+k, QWK_NEWLINE))!=NULL)
					*p=0;
				strcat(header, qwkbuf+k);
				strcat(header, "\n");
				if(p==NULL) {
					header_cont=true;
					break;
				}
				k+=strlen(qwkbuf+k);
				header_cont=false;
				continue;
			}
			if(!taillen && qwkbuf[k]==' ' && col==3 && bodylen>=3
				&& body[bodylen-3]=='-' && body[bodylen-2]=='-'
				&& body[bodylen-1]=='-') {
				bodylen-=3;
				strcpy(tail,"--- ");
				taillen=4;
				col++;
				continue; 
			}
			if(qwkbuf[k]==QWK_NEWLINE) {		/* expand QWK_NEWLINE to crlf */
				if(!taillen && col==3 && bodylen>=3 && body[bodylen-3]=='-'
					&& body[bodylen-2]=='-' && body[bodylen-1]=='-') {
					bodylen-=3;
					strcpy(tail,"---");
					taillen=3; 
				}
				col=0;
				if(taillen) {
					tail[taillen++]=CR;
					tail[taillen++]=LF; 
				}
				else {
					body[bodylen++]=CR;
					body[bodylen++]=LF; 
				}
				continue; 
			}
			/* beep restrict */
			if(!fromhub && qwkbuf[k]==BEL && useron.rest&FLAG('B'))   
				continue;
			/* ANSI restriction */
			if(!fromhub && (qwkbuf[k]==1 || qwkbuf[k]==ESC)
				&& useron.rest&FLAG('A'))
				continue;
			if(qwkbuf[k]!=1 && lastch!=1)
				col++;
			if(lastch==CTRL_A && !validattr(qwkbuf[k])) {
				if(taillen) taillen--;
				else		bodylen--;
				lastch=0;
				continue; 
			}
			lastch=qwkbuf[k];
			if(taillen)
				tail[taillen++]=qwkbuf[k];
			else
				body[bodylen++]=qwkbuf[k]; 
		} 
	}

	while(bodylen && body[bodylen-1]==' ') bodylen--; /* remove trailing spaces */
	if(bodylen>=2 && body[bodylen-2]==CR && body[bodylen-1]==LF)
		bodylen-=2;

	while(taillen && tail[taillen-1]<=' ') taillen--; /* remove trailing garbage */

	skip=0;
	if(useron.rest&FLAG('Q') || fromhub) {      /* QWK Net */
		if(!strnicmp(header,"@VIA:",5)) {
			if(!fromhub)
				set_qwk_flag(QWK_VIA);
			p=strchr(header, '\n');
			if(p) {
				*p=0;
				skip=strlen(header)+1; 
			}
			truncsp(header);
			p=header+5; 					/* Skip "@VIA:" */
			while(*p && *p<=' ') p++;		/* Skip any spaces */
			if(route_circ(p,cfg.sys_id)) {
				free(header);
				free(body);
				free(tail);
				smb_freemsgmem(&msg);
				bprintf("\r\nCircular message path: %s\r\n",p);
				sprintf(str,"Circular message path: %s from %s"
					,p,fromhub ? cfg.qhub[fromhub-1]->id:useron.alias);
				errorlog(str);
				return(false); 
			}
			sprintf(str,"%s/%s"
				,fromhub ? cfg.qhub[fromhub-1]->id : useron.alias,p);
			strupr(str);
			update_qwkroute(str); 
		}
		else {
			if(fromhub)
				strcpy(str,cfg.qhub[fromhub-1]->id);
			else
				strcpy(str,useron.alias); 
		}
		strupr(str);
		j=NET_QWK;
		smb_hfield(&msg,SENDERNETTYPE,2,&j);
		smb_hfield_str(&msg,SENDERNETADDR,str);
		sprintf(str,"%25.25s",hdrblk+46);  /* From user */
		truncsp(str);
	} else {
		sprintf(str,"%u",useron.number);
		smb_hfield_str(&msg,SENDEREXT,str);
		if((uint)subnum!=INVALID_SUB && cfg.sub[subnum]->misc&SUB_NAME)
			strcpy(str,useron.name);
		else
			strcpy(str,useron.alias);
	}
	smb_hfield_str(&msg,SENDER,str);

	if(!strnicmp(header+skip,"@MSGID:",7)) {
		if(!fromhub)
			set_qwk_flag(QWK_MSGID);
		p=strchr(header+skip, '\n');
		i=skip;
		if(p) {
			*p=0;
			skip+=strlen(header+i)+1; 
		}
		p=header+i+7;					/* Skip "@MSGID:" */
		while(*p && *p<=' ') p++;		/* Skip any spaces */
		truncstr(p," ");				/* Truncate at first space char */
		smb_hfield_str(&msg,RFC822MSGID,p);
	}
	if(!strnicmp(header+skip,"@REPLY:",7)) {
		if(!fromhub)
			set_qwk_flag(QWK_MSGID);
		p=strchr(header+skip, '\n');
		i=skip;
		if(p) {
			*p=0;
			skip+=strlen(header+i)+1; 
		}
		p=header+i+7;					/* Skip "@REPLY:" */
		while(*p && *p<=' ') p++;		/* Skip any spaces */
		truncstr(p," ");				/* Truncate at first space char */
		smb_hfield_str(&msg,RFC822REPLYID,p);
	}
	if(!strnicmp(header+skip,"@TZ:",4)) {
		if(!fromhub)
			set_qwk_flag(QWK_TZ);
		p=strchr(header+skip, '\n');
		i=skip;
		if(p) {
			*p=0;
			skip+=strlen(header+i)+1; 
		}
		p=header+i+4;					/* Skip "@TZ:" */
		while(*p && *p<=' ') p++;		/* Skip any spaces */
		msg.hdr.when_written.zone=(short)ahtoul(p); 
	}
	if(!strnicmp(header+skip,"@REPLYTO:",9)) {
		p=strchr(header+skip, '\n');
		i=skip;
		if(p) {
			*p=0;
			skip+=strlen(header+i)+1; 
		}
		p=header+i+9;					/* Skip "@REPLYTO:" */
		while(*p && *p<=' ') p++;		/* Skip any spaces */
		smb_hfield_str(&msg,REPLYTO,p);
	}
	free(header);

	/* smb_addmsg required ASCIIZ strings */
	body[bodylen]=0;
	tail[taillen]=0;

	if(online==ON_REMOTE)
		bputs(text[WritingIndx]);

	if(smb.status.max_crcs==0)	/* no CRC checking means no body text dupe checking */
		dupechk_hashes&=~(1<<SMB_HASH_SOURCE_BODY);

	if((i=smb_addmsg(&smb,&msg,storage,dupechk_hashes,xlat,(uchar*)body,(uchar*)tail))==SMB_SUCCESS)
		success=true;
	else if(i==SMB_DUPE_MSG) {
		bprintf("\r\n!%s\r\n",smb.last_error);
		if(!fromhub) {
			if(subnum==INVALID_SUB) {
				sprintf(str,"%s duplicate e-mail attempt (%s)",useron.alias,smb.last_error);
				logline("E!",str); 
			} else {
				sprintf(str,"%s duplicate message attempt in %s %s (%s)"
					,useron.alias
					,cfg.grp[cfg.sub[subnum]->grp]->sname
					,cfg.sub[subnum]->lname
					,smb.last_error);
				logline("P!",str); 
			}
		}
	}
	else 
		errormsg(WHERE,ERR_WRITE,smb.file,i,smb.last_error);

	smb_freemsgmem(&msg);

	free(body);
	free(tail);

	return(success);
}
