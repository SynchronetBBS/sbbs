/* smbtxt.c */

/* Synchronet message base (SMB) message text library routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include <malloc.h>

/* SMB-specific */
#include "smblib.h"

char HUGE16*  SMBCALL smb_getmsgtxt(smb_t* smb, smbmsg_t* msg, ulong mode)
{
	char	HUGE16* buf=NULL,HUGE16* lzhbuf,HUGE16* p;
	ushort	xlat;
	int 	i,lzh;
	long	l=0,lzhlen,length;

	for(i=0;i<msg->hdr.total_dfields;i++) {
		if(!(msg->dfield[i].type==TEXT_BODY
			|| (mode&GETMSGTXT_TAILS && msg->dfield[i].type==TEXT_TAIL))
			|| msg->dfield[i].length<=2L)
			continue;
		fseek(smb->sdt_fp,msg->hdr.offset+msg->dfield[i].offset
			,SEEK_SET);
		fread(&xlat,2,1,smb->sdt_fp);
		lzh=0;
		if(xlat==XLAT_LZH) {
			lzh=1;
			fread(&xlat,2,1,smb->sdt_fp); 
		}
		if(xlat!=XLAT_NONE) 	/* no other translations currently supported */
			continue;

		length=msg->dfield[i].length-2L;
		if(lzh) {
			length-=2;
			if(length<1)
				continue;
			if((lzhbuf=LMALLOC(length))==NULL) {
				sprintf(smb->last_error
					,"malloc failure of %ld bytes for LZH buffer"
					,length);
				return(buf);
			}
			smb_fread(lzhbuf,length,smb->sdt_fp);
			lzhlen=*(long*)lzhbuf;
			if((p=REALLOC(buf,l+lzhlen+3L))==NULL) {
				sprintf(smb->last_error
					,"realloc failure of %ld bytes for text buffer"
					,l+lzhlen+3L);
				FREE(lzhbuf);
				return(buf); 
			}
			buf=p;
			lzh_decode((char*)lzhbuf,length,(char*)buf+l);
			FREE(lzhbuf);
			l+=lzhlen; 
		}
		else {
			if((p=REALLOC(buf,l+length+3L))==NULL) {
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
		*(buf+l)=CR;
		l++;
		*(buf+l)=LF;
		l++;
		*(buf+l)=0; 
	}
	return(buf);
}

void SMBCALL smb_freemsgtxt(char HUGE16* buf)
{
	if(buf!=NULL)
		FREE(buf);
}
