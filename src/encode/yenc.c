/* yenc.c */

/* yEnc encoding/decoding routines */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "yenc.h"

#define YENC_BIAS			42
#define YENC_ESCAPE_CHAR	'='
#define YENC_ESCAPE_BIAS	64
	
int ydecode(char *target, size_t tlen, const char *source, size_t slen)
{
	char	ch;
	size_t	rd=0;
	size_t	wr=0;

	if(slen==0)
		slen=strlen(source);
	while(rd<slen && wr<tlen) {
		ch=source[rd++];
		if(ch==YENC_ESCAPE_CHAR && rd<slen)
			ch=source[rd++]-YENC_ESCAPE_BIAS;
		ch-=YENC_BIAS;
		target[wr++]=ch;
	}

	return(wr);
}

int yencode(char *target, size_t tlen, const char *source, size_t slen)
{
	char	ch;
	size_t	rd=0;
	size_t	wr=0;
	int		done=0;

	if(slen==0)
		slen=strlen(source);
	while(rd<=slen && wr<tlen && !done) {
		ch=source[rd++];
		ch+=YENC_BIAS;
		switch(ch) {
			case 0:
			case '\r':
			case '\n':
			case YENC_ESCAPE_CHAR:
				if(wr+1>=tlen) {	/* no room for escaped char */
					done=1;
					continue;
				}
				ch+=YENC_ESCAPE_BIAS;
				target[wr++]=YENC_ESCAPE_CHAR;
				break;
		}
		target[wr++]=ch;
	}

	if(wr<tlen)
		target[wr++]=0;
	return(wr);
}

#if defined(YDECODE_TEST)

static char* truncstr(char* str, const char* set)
{
	char* p;

	p=strpbrk(str,set);
	if(p!=NULL)
		*p=0;

	return(p);
}

int main(int argc, char**argv)
{
	char	str[1024];
	char	buf[256];
	char*	p;
	FILE*	in;
	FILE*	out=NULL;
	int		len;
	int		line;

	if(argc<2) {
		fprintf(stderr,"usage: %s infile\n",argv[0]);
		return 1;
	}

	if((in=fopen(argv[1],"rb"))==NULL) {
		perror(argv[1]);
		return 1;
	}

	while(!feof(in)) {
		memset(str,0,sizeof(str));
		if(fgets(str,sizeof(str),in)==NULL)
			break;
		truncstr(str,"\r\n");
		if(strncmp(str,"=ybegin ",8)==0) {
			p=strstr(str,"name=");
			p+=5;
			if((out=fopen(p,"wb"))==NULL) {
				perror(p);
				return 1;
			}
			fprintf(stderr,"Creating %s\n",p);
			line=1;
			continue;
		}
		if(strncmp(str,"=yend ",6)==0) {
			if(out!=NULL) {
				fclose(out);
				out=NULL;
			}
			continue;
		}
		if(out==NULL)
			continue;
		len=ydecode(buf,sizeof(buf),str,0);
		if(len<0) {
			fprintf(stderr,"!Error decoding: %s\n",str);
			break;
		}
		fwrite(buf,len,1,out);
		line++;
	}

	return 0;
}
#elif defined(YENCODE_TEST)

int main(int argc, char**argv)
{
	char	str[1024];
	char	buf[256];
	FILE*	in;
	int		len;

	if(argc<2) {
		fprintf(stderr,"usage: %s infile\n",argv[0]);
		return 1;
	}

	if((in=fopen(argv[1],"rb"))==NULL) {
		perror(argv[1]);
		return 1;
	}

	while(!feof(in)) {
		len=fread(buf,1,45,in);
		if(len<0)
			break;
		len=yencode(str,sizeof(str),buf,len);
		if(len<1)
			break;
		printf("%.*s",len,str);
	}

	return 0;
}

#endif


