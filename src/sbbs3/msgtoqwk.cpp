/* msgtoqwk.cpp */

/* Synchronet message to QWK format conversion routine */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2008 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#define MAX_MSGNUM	0x7FFFFFUL	// only 7 (decimal) digits allowed for msg num 

/****************************************************************************/
/* Converts message 'msg' to QWK format, writing to file 'qwk_fp'.          */
/* mode determines how to handle Ctrl-A codes								*/
/****************************************************************************/
ulong sbbs_t::msgtoqwk(smbmsg_t* msg, FILE *qwk_fp, long mode, int subnum
	, int conf, FILE* hdrs)
{
	char	str[512],from[512],to[512],ch=0,tear=0,tearwatch=0,*buf,*p;
	char	msgid[256];
	char 	tmp[512];
	long	l,size=0,offset;
	int 	i;
	ushort	hfield_type;
	struct	tm	tm;
	smbmsg_t	remsg;
	time_t	tt;

	offset=ftell(qwk_fp);
	if(hdrs!=NULL) {
		fprintf(hdrs,"[%x]\n",offset);

		/* Message-IDs */
		fprintf(hdrs,"Message-ID:  %s\n",get_msgid(&cfg,subnum,msg,msgid,sizeof(msgid)));
		if(msg->reply_id!=NULL)
			fprintf(hdrs,"In-Reply-To: %s\n",msg->reply_id);

		/* Time/Date/Zone info */
		fprintf(hdrs,"WhenWritten:  %-20s %04hx\n"
			,xpDateTime_to_isoDateTimeStr(
				time_to_xpDateTime(msg->hdr.when_written.time,smb_tzutc(msg->hdr.when_written.zone))
				,/* separators: */"","","", /* precision: */0
				,str,sizeof(str))
			,msg->hdr.when_written.zone
			);
		fprintf(hdrs,"WhenImported: %-20s %04hx\n"
			,xpDateTime_to_isoDateTimeStr(
				time_to_xpDateTime(msg->hdr.when_imported.time,smb_tzutc(msg->hdr.when_imported.zone))
				,/* separators: */"","","", /* precision: */0
				,str,sizeof(str))
			,msg->hdr.when_imported.zone
			);
		fprintf(hdrs,"WhenExported: %-20s %04hx\n"
			,xpDateTime_to_isoDateTimeStr(
				xpDateTime_now()
				,/* separators: */"","","", /* precision: */0
				,str,sizeof(str))
			,sys_timezone(&cfg)
			);
		fprintf(hdrs,"ExportedFrom: %s %s %lu\n"
			,cfg.sys_id
			,subnum==INVALID_SUB ? "mail":cfg.sub[subnum]->code
			,msg->hdr.number
			);

		/* SENDER */
		fprintf(hdrs,"%s: %s\n",smb_hfieldtype(SENDER),msg->from);
		if(msg->from_net.type)
			fprintf(hdrs,"%s: %s\n",smb_hfieldtype(SENDERNETADDR),smb_netaddrstr(&msg->from_net,tmp));
		if((p=(char*)smb_get_hfield(msg,hfield_type=SENDERIPADDR,NULL))!=NULL)
			fprintf(hdrs,"%s: %s\n",smb_hfieldtype(hfield_type),p);
		if((p=(char*)smb_get_hfield(msg,hfield_type=SENDERHOSTNAME,NULL))!=NULL)
			fprintf(hdrs,"%s: %s\n",smb_hfieldtype(hfield_type),p);
		if((p=(char*)smb_get_hfield(msg,hfield_type=SENDERPROTOCOL,NULL))!=NULL)
			fprintf(hdrs,"%s: %s\n",smb_hfieldtype(hfield_type),p);
		if(msg->from_org!=NULL)
			fprintf(hdrs,"Organization: %s\n",msg->from_org);
		else if(msg->from_net.type==NET_NONE)
			fprintf(hdrs,"Organization: %s\n",cfg.sys_name);

		/* Reply-To */
		if((p=(char*)smb_get_hfield(msg,RFC822REPLYTO,NULL))==NULL) {
			if(msg->replyto_net.type==NET_INTERNET)
				p=(char*)msg->replyto_net.addr;
			else if(msg->replyto!=NULL)
				p=msg->replyto;
		}
		if(p!=NULL)
			fprintf(hdrs,"Reply-To: %s\n",p);	/* use original RFC822 header field */

		/* SUBJECT */
		fprintf(hdrs,"%s: %s\n",smb_hfieldtype(SUBJECT),msg->subj);

		/* RECIPIENT */
		fprintf(hdrs,"%s: %s\n",smb_hfieldtype(RECIPIENT),msg->to);
		if(msg->to_net.type)
			fprintf(hdrs,"%s: %s\n",smb_hfieldtype(RECIPIENTNETADDR),smb_netaddrstr(&msg->to_net,tmp));

		/* FidoNet */
		if((p=(char*)smb_get_hfield(msg,hfield_type=FIDOAREA,NULL))!=NULL)	
			fprintf(hdrs,"%s: %s\n", smb_hfieldtype(hfield_type), p);
		if((p=(char*)smb_get_hfield(msg,hfield_type=FIDOSEENBY,NULL))!=NULL)
			fprintf(hdrs,"%s: %s\n", smb_hfieldtype(hfield_type), p);
		if((p=(char*)smb_get_hfield(msg,hfield_type=FIDOPATH,NULL))!=NULL)
			fprintf(hdrs,"%s: %s\n", smb_hfieldtype(hfield_type), p);
		if((p=(char*)smb_get_hfield(msg,hfield_type=FIDOMSGID,NULL))!=NULL)
			fprintf(hdrs,"%s: %s\n", smb_hfieldtype(hfield_type), p);
		if((p=(char*)smb_get_hfield(msg,hfield_type=FIDOREPLYID,NULL))!=NULL)
			fprintf(hdrs,"%s: %s\n", smb_hfieldtype(hfield_type), p);
		if((p=(char*)smb_get_hfield(msg,hfield_type=FIDOPID,NULL))!=NULL)	
			fprintf(hdrs,"%s: %s\n", smb_hfieldtype(hfield_type), p);
		if((p=(char*)smb_get_hfield(msg,hfield_type=FIDOFLAGS,NULL))!=NULL)	
			fprintf(hdrs,"%s: %s\n", smb_hfieldtype(hfield_type), p);
		if((p=(char*)smb_get_hfield(msg,hfield_type=FIDOTID,NULL))!=NULL)	
			fprintf(hdrs,"%s: %s\n", smb_hfieldtype(hfield_type), p);

		/* USENET */
		if((p=(char*)smb_get_hfield(msg,hfield_type=USENETPATH,NULL))!=NULL)
			fprintf(hdrs,"%s: %s\n", smb_hfieldtype(hfield_type), p);
		if((p=(char*)smb_get_hfield(msg,hfield_type=USENETNEWSGROUPS,NULL))!=NULL)
			fprintf(hdrs,"%s: %s\n", smb_hfieldtype(hfield_type), p);

		/* RFC822 header fields: */
		for(i=0;i<msg->total_hfields;i++)
			if(msg->hfield[i].type==RFC822HEADER)
				fprintf(hdrs,"%s\n",truncsp_lines((char*)msg->hfield_dat[i]));

		/* Blank line: */
		fprintf(hdrs,"\n");
	}
	memset(str,' ',QWK_BLOCK_LEN);
	fwrite(str,QWK_BLOCK_LEN,1,qwk_fp);		/* Init header to space */

	if(msg->from_net.addr && (uint)subnum==INVALID_SUB) {
		if(mode&QM_TO_QNET)
			sprintf(from,"%.128s",msg->from);
		else if(msg->from_net.type==NET_FIDO)
			sprintf(from,"%.128s@%.128s"
				,msg->from,smb_faddrtoa((faddr_t *)msg->from_net.addr,tmp));
		else if(msg->from_net.type==NET_INTERNET || strchr((char*)msg->from_net.addr,'@')!=NULL)
			sprintf(from,"%.128s",(char*)msg->from_net.addr);
		else
			sprintf(from,"%.128s@%.128s",msg->from,(char*)msg->from_net.addr);
		if(strlen(from)>25) {
			size+=fprintf(qwk_fp,"From: %.128s%c%c",from,QWK_NEWLINE,QWK_NEWLINE);
			sprintf(from,"%.128s",msg->from); 
		} 
	}
	else {
		sprintf(from,"%.128s",msg->from);
		if(msg->hdr.attr&MSG_ANONYMOUS && !SYSOP)	   /* from user */
			SAFECOPY(from,text[Anonymous]); 
	}

	if(msg->to_net.addr && (uint)subnum==INVALID_SUB) {
		if(msg->to_net.type==NET_FIDO)
			sprintf(to,"%.128s@%s",msg->to,smb_faddrtoa((faddr_t *)msg->to_net.addr,tmp));
		else if(msg->to_net.type==NET_INTERNET)
			sprintf(to,"%.128s",(char*)msg->to_net.addr);
		else if(msg->to_net.type==NET_QWK) {
			if(mode&QM_TO_QNET) {
				p=strchr((char *)msg->to_net.addr,'/');
				if(p) { 	/* Another hop */
					p++;
					SAFECOPY(to,"NETMAIL");
					size+=fprintf(qwk_fp,"%.128s@%.128s%c",msg->to,p,QWK_NEWLINE);
				}
				else
					sprintf(to,"%.128s",msg->to); }
			else
				sprintf(to,"%.128s@%.128s",msg->to,(char*)msg->to_net.addr); }
		else
			sprintf(to,"%.128s@%.128s",msg->to,(char*)msg->to_net.addr);
		if(strlen(to)>25) {
			size+=fprintf(qwk_fp,"To: %.128s%c%c",to,QWK_NEWLINE,QWK_NEWLINE);
			if(msg->to_net.type==NET_QWK)
				SAFECOPY(to,"NETMAIL");
			else
				sprintf(to,"%.128s",msg->to); 
		} 
	}
	else
		sprintf(to,"%.128s",msg->to);

	if(msg->from_net.type==NET_QWK && mode&QM_VIA && !msg->forwarded)
		size+=fprintf(qwk_fp,"@VIA: %s%c"
			,(char*)msg->from_net.addr,QWK_NEWLINE);
	
	if(mode&QM_MSGID && (uint)subnum!=INVALID_SUB) {
		size+=fprintf(qwk_fp,"@MSGID: %s%c"
			,get_msgid(&cfg,subnum,msg,msgid,sizeof(msgid)),QWK_NEWLINE);

		if(msg->reply_id) {
			SAFECOPY(tmp,msg->reply_id);
			truncstr(tmp," ");
			size+=fprintf(qwk_fp,"@REPLY: %s%c"
				,tmp,QWK_NEWLINE);
		} else if(msg->hdr.thread_back) {
			memset(&remsg,0,sizeof(remsg));
			remsg.hdr.number=msg->hdr.thread_back;
			if(smb_getmsgidx(&smb, &remsg))
				size+=fprintf(qwk_fp,"@REPLY: <%s>%c",smb.last_error,QWK_NEWLINE);
			else
				size+=fprintf(qwk_fp,"@REPLY: %s%c"
					,get_msgid(&cfg,subnum,&remsg,msgid,sizeof(msgid))
					,QWK_NEWLINE);
		}
	}

	if(msg->hdr.when_written.zone && mode&QM_TZ)
		size+=fprintf(qwk_fp,"@TZ: %04x%c",msg->hdr.when_written.zone,QWK_NEWLINE);

	if(msg->replyto!=NULL && mode&QM_REPLYTO)
		size+=fprintf(qwk_fp,"@REPLYTO: %s%c"
			,msg->replyto,QWK_NEWLINE);

	p=0;
	for(i=0;i<msg->total_hfields;i++) {
		if(msg->hfield[i].type==SENDER)
			p=(char *)msg->hfield_dat[i];
		if(msg->hfield[i].type==FORWARDED && p) {
			size+=fprintf(qwk_fp,"Forwarded from %s on %s%c",p
				,timestr(*(time32_t *)msg->hfield_dat[i])
				,QWK_NEWLINE);
		} 
	}

	buf=smb_getmsgtxt(&smb,msg,GETMSGTXT_ALL);
	if(!buf)
		return(0);

	for(l=0;buf[l];l++) {
		ch=buf[l];

		if(ch==LF) {
			if(tear)
				tear++; 				/* Count LFs after tearline */
			if(tear>3)					/* more than two LFs after the tear */
				tear=0;
			if(tearwatch==4) {			/* watch for LF---LF */
				tear=1;
				tearwatch=0; }
			else if(!tearwatch)
				tearwatch=1;
			else
				tearwatch=0;
			ch=QWK_NEWLINE;
			fputc(ch,qwk_fp);		  /* Replace LF with funky char */
			size++;
			continue; }

		if(ch==CR) {					/* Ignore CRs */
			if(tearwatch<4) 			/* LF---CRLF is okay */
				tearwatch=0;			/* LF-CR- is not okay */
			continue; }

		if(ch==' ' && tearwatch==4) {	/* watch for "LF--- " */
			tear=1;
			tearwatch=0; }

		if(ch=='-') {                   /* watch for "LF---" */
			if(l==0 || (tearwatch && tearwatch<4))
				tearwatch++;
			else
				tearwatch=0; }
		else
			tearwatch=0;

		if((uint)subnum!=INVALID_SUB && cfg.sub[subnum]->misc&SUB_ASCII) {
			if(ch<' ' && ch!=1)
				ch='.';
			else if((uchar)ch>0x7f)
				ch='*'; }

		if(ch==QWK_NEWLINE)					/* funky char */
			ch='*';

		if(ch==CTRL_A) {
			ch=buf[++l];
			if(!ch)
				break;
			if(mode&A_EXPAND) {
				str[0]=0;
				switch(toupper(ch)) {		/* non-color codes */
					case 'L':
						SAFECOPY(str,"\x1b[2J\x1b[H");
						break;
					case 'W':
						SAFECOPY(str,ansi(LIGHTGRAY));
						break;
					case 'K':
						SAFECOPY(str,ansi(BLACK));
						break;
					case 'H':
						SAFECOPY(str,ansi(HIGH));
						break;
					case 'I':
						SAFECOPY(str,ansi(BLINK));
						break;
					case 'N':   /* Normal */
						SAFECOPY(str,ansi(ANSI_NORMAL));
						break;
					case 'R':                               /* Color codes */
						SAFECOPY(str,ansi(RED));
						break;
					case 'G':
						SAFECOPY(str,ansi(GREEN));
						break;
					case 'B':
						SAFECOPY(str,ansi(BLUE));
						break;
					case 'C':
						SAFECOPY(str,ansi(CYAN));
						break;
					case 'M':
						SAFECOPY(str,ansi(MAGENTA));
						break;
					case 'Y':   /* Yellow */
						SAFECOPY(str,ansi(BROWN));
						break;
					case '0':
						SAFECOPY(str,ansi(BG_BLACK));
						break;
					case '1':
						SAFECOPY(str,ansi(BG_RED));
						break;
					case '2':
						SAFECOPY(str,ansi(BG_GREEN));
						break;
					case '3':
						SAFECOPY(str,ansi(BG_BROWN));
						break;
					case '4':
						SAFECOPY(str,ansi(BG_BLUE));
						break;
					case '5':
						SAFECOPY(str,ansi(BG_MAGENTA));
						break;
					case '6':
						SAFECOPY(str,ansi(BG_CYAN));
						break; 
					case '7':
						SAFECOPY(str,ansi(BG_LIGHTGRAY));
						break;
				}
				if(str[0])
					size+=fwrite(str,sizeof(char),strlen(str),qwk_fp);
				continue; 
			} 						/* End Expand */

			if(mode&A_LEAVE) {
				fputc(1,qwk_fp);
				fputc(ch,qwk_fp);
				size+=2L; }
			else									/* Strip */
				if(toupper(ch)=='L') {
					fputc(FF,qwk_fp);
					size++; }
			continue; } 							/* End of Ctrl-A shit */
		fputc(ch,qwk_fp);
		size++; }

	free(buf);
	if(ch!=QWK_NEWLINE) {
		fputc(QWK_NEWLINE,qwk_fp); 		/* make sure it ends in CRLF */
		size++; }

	if(mode&QM_TAGLINE && !(cfg.sub[subnum]->misc&SUB_NOTAG)) {
		if(!tear)										/* no tear line */
			SAFEPRINTF(str,"\1n---%c",QWK_NEWLINE);        /* so add one */
		else
			SAFECOPY(str,"\1n");
		if(cfg.sub[subnum]->misc&SUB_ASCII) ch='*';
		else ch='þ';
		safe_snprintf(tmp,sizeof(tmp)," %c \1g%.10s\1n %c %.127s%c"
			,ch,VERSION_NOTICE,ch,cfg.sub[subnum]->tagline,QWK_NEWLINE);
		strcat(str,tmp);
		if(!(mode&A_LEAVE))
			remove_ctrl_a(str,NULL);
		size+=fwrite(str,sizeof(char),strlen(str),qwk_fp);
	}

	while(size%QWK_BLOCK_LEN) {				 /* Pad with spaces */
		size++;
		fputc(' ',qwk_fp); }

	tt=msg->hdr.when_written.time;
	if(localtime_r(&tt,&tm)==NULL)
		memset(&tm,0,sizeof(tm));

	safe_snprintf(tmp,sizeof(tmp),"%02u-%02u-%02u%02u:%02u"
		,tm.tm_mon+1,tm.tm_mday,TM_YEAR(tm.tm_year)
		,tm.tm_hour,tm.tm_min);

	if(msg->hdr.attr&MSG_PRIVATE) {
		if(msg->hdr.attr&MSG_READ)
			ch='*'; /* private, read */
		else
			ch='+'; /* private, unread */ }
	else {
		if(msg->hdr.attr&MSG_READ)
			ch='-'; /* public, read */
		else
			ch=' '; /* public, unread */ }


	safe_snprintf(str,sizeof(str),"%c%-7lu%-13.13s%-25.25s"
		"%-25.25s%-25.25s%12s%-8lu%-6lu\xe1%c%c%c%c%c"
		,ch                     /* message status flag */
		,mode&QM_REP ? (ulong)conf /* conference or */
			: msg->hdr.number&MAX_MSGNUM	/* message number */
		,tmp					/* date and time */
		,to 					/* To: */
		,from					/* From: */
		,msg->subj              /* Subject */
		,nulstr                 /* Password */
		,msg->hdr.thread_back&MAX_MSGNUM   /* Message Re: Number */
		,(size/QWK_BLOCK_LEN)+1	/* Number of blocks */
		,(char)conf&0xff        /* Conference number lo byte */
		,(ushort)conf>>8		/*					 hi byte */
		,' '                     /* not used */
		,' '                     /* not used */
		,useron.rest&FLAG('Q') ? '*' : ' '     /* Net tag line */
		);

	fseek(qwk_fp,offset,SEEK_SET);
	fwrite(str,QWK_BLOCK_LEN,1,qwk_fp);
	fseek(qwk_fp,size,SEEK_CUR);

	return(size);
}
