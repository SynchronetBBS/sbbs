/* Synchronet message base (SMB) message text library routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
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

/* ANSI */
#include <stdlib.h>	/* malloc/realloc/free */
#include <string.h>	/* strlen */

/* SMB-specific */
#include "smblib.h"

char* SMBCALL smb_getmsgtxt(smb_t* smb, smbmsg_t* msg, ulong mode)
{
	char*	buf;
	char*	lzhbuf;
	char*	p;
	char*	str;
	uint16_t	xlat;
	uint 	i;
	int		lzh;	/* BOOL */
	long	l=0,lzhlen,length;

	if((buf=(char*)malloc(sizeof(char)))==NULL) {
		sprintf(smb->last_error
			,"%s malloc failure of %" XP_PRIsize_t "u bytes for buffer"
			,__FUNCTION__, sizeof(char));
		return(NULL);
	}
	*buf=0;

	if(!(mode&GETMSGTXT_NO_HFIELDS)) {
		for(i=0;i<(uint)msg->total_hfields;i++) {			/* comment headers are part of text */
			if(msg->hfield[i].type!=SMB_COMMENT && msg->hfield[i].type!=SMTPSYSMSG)
				continue;
			str=(char*)msg->hfield_dat[i];
			length=strlen(str)+2;	/* +2 for crlf */
			if((p=(char*)realloc(buf,l+length+1))==NULL) {
				sprintf(smb->last_error
					,"%s realloc failure of %ld bytes for comment buffer"
					, __FUNCTION__, l+length+1);
				free(buf);
				return(NULL);
			}
			buf=p;
			l+=sprintf(buf+l,"%s\r\n",str);
		}
		if(l) {	/* Add a blank line after comments */
			if((p=(char*)realloc(buf,l+3))==NULL) {
				sprintf(smb->last_error
					,"%s realloc failure of %ld bytes for comment buffer"
					, __FUNCTION__, l+3);
				free(buf);
				return(NULL);
			}
			buf=p;
			l+=sprintf(buf+l,"\r\n");
		}
		unsigned answers = 0;
		for(i=0;i<(uint)msg->total_hfields;i++) {			/* Poll Answers are part of text */
			if(msg->hfield[i].type!=SMB_POLL_ANSWER)
				continue;
			char tmp[128];
			length = safe_snprintf(tmp, sizeof(tmp), "%2u: %s\r\n", ++answers, (char*)msg->hfield_dat[i]);
			if((p=(char*)realloc(buf,l+length+1))==NULL) {
				sprintf(smb->last_error
					,"%s realloc failure of %ld bytes for comment buffer"
					, __FUNCTION__, l+length+1);
				free(buf);
				return(NULL);
			}
			buf=p;
			memcpy(buf+l, tmp, length);
			l += length;
			buf[l] = 0;
		}
	}

	for(i=0;i<(uint)msg->hdr.total_dfields;i++) {
		if(msg->dfield[i].length<=sizeof(xlat))
			continue;
		switch(msg->dfield[i].type) {
			case TEXT_BODY:
				if(mode&GETMSGTXT_NO_BODY)
					continue;
				break;
			case TEXT_TAIL:
				if(!(mode&GETMSGTXT_TAILS))
					continue;
				break;
			default:	/* ignore other data types */
				continue;
		}
		fseek(smb->sdt_fp,msg->hdr.offset+msg->dfield[i].offset
			,SEEK_SET);
		fread(&xlat,sizeof(xlat),1,smb->sdt_fp);
		lzh=0;
		if(xlat==XLAT_LZH) {
			lzh=1;
			fread(&xlat,sizeof(xlat),1,smb->sdt_fp); 
		}
		if(xlat!=XLAT_NONE) 	/* no other translations currently supported */
			continue;

		length=msg->dfield[i].length-sizeof(xlat);
		if(lzh) {
			length-=sizeof(xlat);
			if(length<1)
				continue;
			if((lzhbuf=(char*)malloc(length))==NULL) {
				sprintf(smb->last_error
					,"%s malloc failure of %ld bytes for LZH buffer"
					, __FUNCTION__, length);
				free(buf);
				return(NULL);
			}
			smb_fread(smb,lzhbuf,length,smb->sdt_fp);
			lzhlen=*(int32_t*)lzhbuf;
			if((p=(char*)realloc(buf,l+lzhlen+3L))==NULL) {
				sprintf(smb->last_error
					,"%s realloc failure of %ld bytes for text buffer"
					, __FUNCTION__, l+lzhlen+3L);
				free(lzhbuf);
				free(buf);
				return(NULL); 
			}
			buf=p;
			lzh_decode((uint8_t *)lzhbuf,length,(uint8_t *)buf+l);
			free(lzhbuf);
			l+=lzhlen; 
		}
		else {
			if((p=(char*)realloc(buf,l+length+3L))==NULL) {
				sprintf(smb->last_error
					,"%s realloc failure of %ld bytes for text buffer"
					, __FUNCTION__, l+length+3L);
				free(buf);
				return(NULL);
			}
			buf=p;
			p=buf+l;
			l+=fread(p,1,length,smb->sdt_fp);
		}
		if(!l)
			continue;
		l--;
		while(l && buf[l]==0) l--;
		l++;
		*(buf+l)='\r';	/* CR */
		l++;
		*(buf+l)='\n';	/* LF */
		l++;
		*(buf+l)=0; 
	}

	if(mode&GETMSGTXT_PLAIN)
		buf = smb_getplaintext(msg, buf);
	return(buf);
}

void SMBCALL smb_freemsgtxt(char* buf)
{
	if(buf!=NULL)
		free(buf);
}

/* Get just the plain-text portion of a MIME-encoded message body, recursively */
static char* mime_getplaintext(char* buf, const char* content_type, int depth)
{
	char*	txt;
	char*	p;
	char	boundary[256];

	if(depth > 2)
		return NULL;
	if(content_type == NULL)	/* Not MIME-encoded */
		return NULL;
	content_type += 13;
	SKIP_WHITESPACE(content_type);
	if(strstr(content_type, "multipart/alternative;") == content_type)
		content_type += 22;
	else if(strstr(content_type, "multipart/mixed;") == content_type)
		content_type +=16;
	else
		return NULL;
	p = strstr(content_type, "boundary=");
	if(p == NULL)
		return NULL;
	p += 9;
	if(*p == '"')
		p++;
	SAFEPRINTF(boundary, "--%s", p);
	if((p = strchr(boundary,'"')) != NULL)
		*p = 0;
	txt = buf;
	while((p = strstr(txt, boundary)) != NULL) {
		txt = p+strlen(boundary);
		SKIP_WHITESPACE(txt);
		if(strncmp(txt, "Content-Type:", 13) != 0)
			continue;
		p = strstr(txt, "\r\n\r\n");	/* End of header */
		if(p==NULL)
			continue;
		if(strncmp(txt, "Content-Type: text/plain;", 25) 
			&& strncmp(txt,"Content-Type: text/plain\r", 25)) {
			if((p = mime_getplaintext(p, txt, depth + 1)) != NULL)
				return p;
			continue;
		}
		txt = p;
		SKIP_WHITESPACE(txt);
		if((p = strstr(txt, boundary)) != NULL)
			*p = 0;
		return txt;
	}
	return NULL;
}


/* Get just the plain-text portion of a MIME-encoded message body */
char* SMBCALL smb_getplaintext(smbmsg_t* msg, char* buf)
{
	int		i;
	char*	txt;
	char*	content_type = NULL;

	for(i=0;i<msg->total_hfields;i++) { 
		if(msg->hfield[i].type==RFC822HEADER) { 
			if(strnicmp((char*)msg->hfield_dat[i],"Content-Type:",13)==0) {
				content_type=msg->hfield_dat[i];
				break;
			}
        }
    }
	txt = mime_getplaintext(buf, content_type, 0);
	if(txt != NULL)
		memmove(buf, txt, strlen(txt)+1);
	return buf;
}
