/* uucode.c */

/* Unix-to-unix encoding/decoding routines */

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

int uudecode(unsigned char *target, size_t tlen, const unsigned char *source, size_t slen)
{
	int		i;
	char	ch;
	size_t	rd=0;
	size_t	wr=0;
	size_t	block;
	size_t	len;
	unsigned char cell[4];

	if(slen==0)
		slen=strlen(source);
	while(rd<slen && wr<tlen) {
		ch=source[rd++];
		if(ch<' ')
			continue;
		len=(ch-' ')&0x3f;
		if(len<=0 || rd>=slen)
			break;
		block=0;
		while(block<len && wr<tlen && rd<slen) {
			/* Remove space bias */
			for(i=0;i<sizeof(cell);i++) {
				cell[i]=source[rd++];
				if(cell[i]>=' ') cell[i]-=' ';
			}
			/* Convert block of 4 6-bit chars into 3 8-bit chars */
			target[wr]=(cell[0]&0x3f)<<2;		/* lower 6 (s1) to upper 6 (d1) */
			target[wr++]|=(cell[1]&0x30)>>4;	/* upper 2 (s2) to lower 2 (d1) */
			target[wr]=(cell[1]&0x0f)<<4;		/* lower 4 (s2) to upper 4 (d2) */
			target[wr++]|=(cell[2]&0x3c)>>2;	/* upper 4 (s3) to lower 4 (d2) */
			target[wr]=(cell[2]&0x03)<<6;		/* lower 2 (s3) to upper 2 (d3) */
			target[wr++]|=cell[3]&0x3f;			/* lower 6 (s4) to lower 6 (d3) */
			block+=3;
		}
		if(block!=len)
			return(-1);
		while(rd<slen && source[rd]>' ')
			rd++;	/* find whitespace (line termination) */
		while(rd<slen && source[rd]!=0 && source[rd]<=' ') 
			rd++;	/* skip whitespace separating blocks/lines */
	}

	return(wr);
}

#define BIAS(b) if((b)==0) (b)='`'; else (b)+=' ';

int uuencode(unsigned char *target, size_t tlen, const unsigned char *source, size_t slen)
{
	size_t	rd=0;
	size_t	wr=0;
	size_t	block;
	size_t	len;

	if(slen==0)
		slen=strlen(source);

	if(tlen<3)
		return(-1);
	tlen-=3;	/* reserve room for terminator */
	while(rd<=slen && wr<tlen) {
		len=45;
		if(rd+len>slen)
			len=slen-rd;
		BIAS(len);
		target[wr++]=len;

		block=0;
		while(block<len && wr<tlen && rd<slen) {
			target[wr]=source[rd]>>2;			/* upper 6 (s1) to lower 6 (d1) */
			BIAS(target[wr]); wr++;
			target[wr]=(source[rd++]&0x03)<<4;	/* lower 2 (s1) to upper 2 (d2) */
			target[wr]|=source[rd]>>4;			/* upper 4 (s2) to lower 4 (d2) */
			BIAS(target[wr]); wr++;
			target[wr]=(source[rd++]&0x0f)<<2;	/* lower 4 (s2) to upper 4 (d3) */
			target[wr]|=source[rd]>>6;			/* upper 2 (s3) to lower 2 (d3) */
			BIAS(target[wr]); wr++;
			target[wr]=source[rd++]&0x3f;		/* lower 6 (s3) to lower 6 (d4) */
			BIAS(target[wr]); wr++;
			block+=3;
		}
		if(wr<tlen) {
			target[wr++]='\r';
			target[wr++]='\n';
		}
		if(rd>=slen)
			break;
	}

	if(wr<tlen)
		target[wr++]=0;
	return(wr);
}

#ifdef UUDECODE_TEST

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
		fprintf(stderr,"usage: uudecode infile\n");
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
		if(strncmp(str,"begin ",6)==0) {
			p=str+7;
			while(*p && isdigit(*p)) p++;	/* skip mode */
			while(*p && *p<=' ') p++;
			if((out=fopen(p,"wb"))==NULL) {
				perror(p);
				return 1;
			}
			fprintf(stderr,"Creating %s\n",p);
			line=1;
			continue;
		}
		if(strcmp(str,"end")==0) {
			if(out!=NULL) {
				fclose(out);
				out=NULL;
			}
			continue;
		}
		if(out==NULL)
			continue;
		len=uudecode(buf,sizeof(buf),str,0);
		if(len<1)
			break;
		fwrite(buf,len,1,out);
		line++;
	}

	return 0;
}
#elif defined(UUENCODE_TEST)

int main(int argc, char**argv)
{
	char	str[1024];
	char	buf[256];
	FILE*	in;
	int		len;

	if(argc<2) {
		fprintf(stderr,"usage: uuencode infile\n");
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
		len=uuencode(str,sizeof(str),buf,len);
		if(len<1)
			break;
		printf("%.*s",len,str);
	}

	return 0;
}

#endif


