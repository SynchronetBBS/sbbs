/* base64.c */

/* Base64 encoding/decoding routines */

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

#include <stdlib.h>
#include <string.h>
#include "base64.h"

static const char * base64alphabet = 
 "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

char * b64_decode(char *target, const char *source, size_t tlen, size_t slen)
{
	const char	*read;
	char	*write;
	char	*tend;
	const char	*send;
	int		bits=0;
	int		working=0;
	char *	i;

	write=target;
	read=source;
	tend=target+tlen;
	send=source+slen;
	for(;write<tend && read<send;read++) {
		working<<=6;
		i=strchr(base64alphabet,(char)*read);
		if(i==NULL) {
			break;
		}
		if(*i=='=') i=(char*)base64alphabet; /* pad char */
		working |= (i-base64alphabet);
		bits+=6;
		if(bits>8) {
			*(write++)=(char)((working&(0xFF<<(bits-8)))>>(bits-8));
			bits-=8;
		}
	}
	if(bits)
		*(write++)=(char)((working&(0xFF<<(bits-8)))>>(bits-8));
	if(write == tend)  {
		*(--write)=0;
		return(NULL);
	}
	*write=0;
	return(target);
}

static int add_char(char *pos, char ch, int done, char *end)
{
	if(pos>=end)  {
		return(1);
	}
	if(done)
		*pos=base64alphabet[64];
	else
		*pos=base64alphabet[ch];
	return(0);
}

char * b64_encode(char *target, const char *source, size_t tlen, size_t slen)  {
	const char	*read;
	char	*write;
	char	*tend;
	const char	*send;
	char	*tmpbuf;
	int		done=0;
	char	enc;
	int		buf=0;
	
	read=source;
	if(source==target)  {
		tmpbuf=(char *)malloc(tlen);
		if(tmpbuf==NULL)
			return(NULL);
		write=tmpbuf;
	}
	else
		write=target;

	tend=write+tlen;
	send=read+slen;
	for(;(read < send) && !done;)  {
		enc=*(read++);
		buf=(enc & 0x03)<<4;
		enc=(enc&0xFC)>>2;
		if(add_char(write++, enc, done, tend))  {
			if(target==source)
				free(tmpbuf);
			return(NULL);
		}
		enc=buf|((*read & 0xF0) >> 4);
		if(add_char(write++, enc, done, tend))  {
			if(target==source)
				free(tmpbuf);
			return(NULL);
		}
		if(read==send)
			done=1;
		buf=(*(read++)<<2)&0x3C;
		enc=buf|((*read & 0xC0)>>6);
		if(add_char(write++, enc, done, tend))  {
			if(target==source)
				free(tmpbuf);
			return(NULL);
		}
		if(read==send)
			done=1;
		enc=((int)*(read++))&0x3F;
		if(add_char(write++, enc, done, tend))  {
			if(target==source)
				free(tmpbuf);
			return(NULL);
		}
		if(read==send)
			done=1;
	}
	if(write<tend)
		*write=0;
	if(target==source)  {
		memcpy(target,tmpbuf,tlen);
		free(tmpbuf);
	}
	return(target);
}
