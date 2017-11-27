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
#include "base64.h"

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

enum content_transfer_encoding {
	CONTENT_TRANFER_ENCODING_NONE,
	CONTENT_TRANFER_ENCODING_BASE64,
	CONTENT_TRANFER_ENCODING_QUOTED_PRINTABLE,
	CONTENT_TRANFER_ENCODING_OTHER
};

/* Decode quoted-printable content-transfer-encoded text */
/* Ignores (strips) unsupported ctrl chars and non-ASCII chars */
/* Does not enforce 76 char line length limit */
char* qp_decode(char* buf)
{
	uchar*	p=(uchar*)buf;
	uchar*	dest=p;

	for(;;p++) {
		if(*p==0) {
			*dest++='\r';
			*dest++='\n';
			break;
		}
		if(*p==' ' || (*p>='!' && *p<='~' && *p!='=') || *p=='\t'|| *p=='\r'|| *p=='\n')
			*dest++=*p;
		else if(*p=='=') {
			p++;
			if(*p==0) 	/* soft link break */
				break;
			if(isxdigit(*p) && isxdigit(*(p+1))) {
				char hex[3];
				hex[0]=*p;
				hex[1]=*(p+1);
				hex[2]=0;
				/* ToDo: what about encoded NULs and the like? */
				*dest++=(uchar)strtoul(hex,NULL,16);
				p++;
			} else {	/* bad encoding */
				*dest++='=';
				*dest++=*p;
			}
		}
	}
	*dest=0;
	return buf;
}

static enum content_transfer_encoding mime_getxferencoding(char* beg, char* end)
{
	char* p = beg;

	while(p < end) {
		SKIP_WHITESPACE(p);
		if(strnicmp(p, "content-transfer-encoding:", 26) != 0) {
			FIND_CHAR(p, '\n');
			continue;
		}
		p += 26;
		SKIP_WHITESPACE(p);
		if(strnicmp(p, "base64", 6) == 0)
			return CONTENT_TRANFER_ENCODING_BASE64;
		if(strnicmp(p, "quoted-printable", 16) == 0)
			return CONTENT_TRANFER_ENCODING_QUOTED_PRINTABLE;
		if(strnicmp(p, "7bit", 4) == 0 || strnicmp(p, "8bit", 4) == 0 || strnicmp(p, "binary", 6) == 0)
			return CONTENT_TRANFER_ENCODING_NONE;
		return CONTENT_TRANFER_ENCODING_OTHER;
	}

	return CONTENT_TRANFER_ENCODING_NONE;
}

/* ToDo: parse and return the "modification-date" value */
static BOOL mime_getattachment(char* beg, char* end, char* attachment)
{
	char fname[MAX_PATH+1];
	char* p = beg;

	while(p < end) {
		SKIP_WHITESPACE(p);
		if(strnicmp(p, "content-disposition:", 20) != 0) {
			FIND_CHAR(p, '\n');
			continue;
		}
		p += 20;
		SKIP_WHITESPACE(p);
		if(strnicmp(p, "inline", 6) == 0) {
			FIND_CHAR(p, '\n');
			continue;
		}
		char* filename = strstr(p, "filename=");
		if(filename == NULL) {
			FIND_CHAR(p, '\n');
			continue;
		}
		filename += 9;
		char* term;
		if(*filename == '"') {
			filename++;
			term = strchr(filename, '"');
		} else
			term = strchr(filename, ';');
		if(term == NULL) {
			term = filename;
			FIND_WHITESPACE(term);
		}
		memcpy(fname, filename, term - filename);
		fname[term - filename] = 0;
		strcpy(attachment, getfname(fname));
		return TRUE;
	}
	return FALSE;
}

/* Find the specified content-type in a MIME-encoded message body, recursively */
static char* mime_getcontent(char* buf, const char* content_type, const char* content_match
	,int depth, enum content_transfer_encoding* encoding, char* attachment, int index)
{
	char*	txt;
	char*	p;
	char	boundary[256];
	char	match1[128];
	char	match2[128];
	int		match_len = 0;
	int		found = 0;
	
	if(content_match != NULL) {
		match_len = sprintf(match1, "Content-Type: %s;", content_match);
					sprintf(match2, "Content-Type: %s\r", content_match);
	}

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
		if((match_len && strncmp(txt, match1, match_len) && strncmp(txt, match2, match_len))
			|| (attachment != NULL && !mime_getattachment(txt, p, attachment))) {
			if((p = mime_getcontent(p, txt, content_match, depth + 1, encoding, attachment, index)) != NULL)
				return p;
			continue;
		}
		if(found++ != index) {
			if((p = mime_getcontent(p, txt, content_match, depth + 1, encoding, attachment, index)) != NULL)
				return p;
			continue;
		}
		if(encoding != NULL)
			*encoding = mime_getxferencoding(txt, p);
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
	enum content_transfer_encoding xfer_encoding = CONTENT_TRANFER_ENCODING_NONE;

	for(i=0;i<msg->total_hfields;i++) { 
		if(msg->hfield[i].type==RFC822HEADER) { 
			if(strnicmp((char*)msg->hfield_dat[i],"Content-Type:",13)==0) {
				content_type=msg->hfield_dat[i];
				break;
			}
        }
    }
	if(content_type == NULL)	/* not MIME */
		return buf;
	txt = mime_getcontent(buf, content_type, "text/plain", 0, &xfer_encoding
		,/* attachment: */NULL, /* index: */0);
	if(txt != NULL) {
		memmove(buf, txt, strlen(txt)+1);
		if(*buf == 0)
			return buf;
		if(xfer_encoding == CONTENT_TRANFER_ENCODING_QUOTED_PRINTABLE)
			qp_decode(buf);
		else if(xfer_encoding == CONTENT_TRANFER_ENCODING_BASE64) {
			char* decoded = strdup(buf);
			if(decoded == NULL)
				return buf;
			if(b64_decode(decoded, strlen(decoded), buf, strlen(buf)) > 0)
				strcpy(buf, decoded);
			free(decoded);
		}
	}

	return buf;
}

/* Get just an attachment (just one) from MIME-encoded message body */
uint8_t* SMBCALL smb_getattachment(smbmsg_t* msg, char* buf, char* filename, uint32_t* filelen, int index)
{
	int		i;
	char*	txt;
	char*	content_type = NULL;
	enum content_transfer_encoding xfer_encoding = CONTENT_TRANFER_ENCODING_NONE;

	for(i=0;i<msg->total_hfields;i++) { 
		if(msg->hfield[i].type==RFC822HEADER) { 
			if(strnicmp((char*)msg->hfield_dat[i],"Content-Type:",13)==0) {
				content_type=msg->hfield_dat[i];
				break;
			}
        }
    }
	if(content_type == NULL)	/* not MIME */
		return NULL;
	txt = mime_getcontent(buf, content_type, /* match-type: */NULL, 0, &xfer_encoding
		,/* attachment: */filename, index);
	if(txt != NULL && xfer_encoding == CONTENT_TRANFER_ENCODING_BASE64) {
		memmove(buf, txt, strlen(txt)+1);
		int result = b64_decode(buf, strlen(buf), buf, strlen(buf));
		if(result < 1)
			return NULL;
		*filelen = result;
		return buf;
	}

	return NULL;	/* No attachment */
}
