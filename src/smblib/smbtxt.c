/* smbtxt.c */

/* Synchronet message base (SMB) message text library routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
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
			,"malloc failure of %u bytes for buffer"
			,sizeof(char));
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
					,"realloc failure of %ld bytes for comment buffer"
					,l+length+1);
				return(buf);
			}
			buf=p;
			l+=sprintf(buf+l,"%s\r\n",str);
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
					,"malloc failure of %ld bytes for LZH buffer"
					,length);
				return(buf);
			}
			smb_fread(smb,lzhbuf,length,smb->sdt_fp);
			lzhlen=*(long*)lzhbuf;
			if((p=(char*)realloc(buf,l+lzhlen+3L))==NULL) {
				sprintf(smb->last_error
					,"realloc failure of %ld bytes for text buffer"
					,l+lzhlen+3L);
				free(lzhbuf);
				return(buf); 
			}
			buf=p;
			lzh_decode((char*)lzhbuf,length,(char*)buf+l);
			free(lzhbuf);
			l+=lzhlen; 
		}
		else {
			if((p=(char*)realloc(buf,l+length+3L))==NULL) {
				sprintf(smb->last_error
					,"realloc failure of %ld bytes for text buffer"
					,l+length+3L);
				return(buf);
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

	return(buf);
}

void SMBCALL smb_freemsgtxt(char* buf)
{
	if(buf!=NULL)
		free(buf);
}
