#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gen_defs.h"

/****************************************************************************/
/* Converts an ASCII Hex string into an ulong                               */
/* by Steve Deppe (Ille Homine Albe)										*/
/****************************************************************************/
/* Copied from str_util.c */
ulong ahtoul(char *str)
{
    ulong l,val=0;

	while((l=(*str++)|0x20)!=0x20)
		val=(l&0xf)+(l>>6&1)*9+val*16;
	return(val);
}

/****************************************************************************/
/* Reads special TEXT.DAT printf style text lines, splicing multiple lines, */
/* replacing escaped characters, and allocating the memory					*/
/****************************************************************************/
/* Copied from load_cfg.c with comment pointer added */
char *readtext(FILE *stream, char **comment_ret)
{
	char buf[2048],str[2048],tmp[256],*p,*p2;
	char comment[2048], *cp;
	int i,j,k;

	if(!fgets(buf,256,stream))
		return(NULL);
	if(buf[0]=='#')
		return(NULL);
	p=strrchr(buf,'"');
	if(!p) {
		return(NULL); 
	}
	comment[0]=0;
	if(*(p+1)=='\\') {	/* merge multiple lines */
		for(cp=p+2; *cp && isspace(*cp); cp++);
		strcat(comment, cp);
		while(strlen(buf)<2000) {
			if(!fgets(str,255,stream))
				return(NULL);
			p2=strchr(str,'"');
			if(!p2)
				continue;
			strcpy(p,p2+1);
			p=strrchr(p,'"');
			if(p && *(p+1)=='\\') {
				for(cp=p+2; *cp && isspace(*cp); cp++);
				strcat(comment, cp);
				continue;
			}
			break; 
		}
	}
	for(cp=p+2; *cp && isspace(*cp); cp++);
	strcat(comment, cp);
	cp=strchr(comment, 0);
	if(cp && cp > comment) {
		cp--;
		while(cp > comment && isspace(*cp)) {
			*(cp--)=0;
		}
	}

	*(p)=0;
	k=strlen(buf);
	for(i=1,j=0;i<k;j++) {
		if(buf[i]=='\\')	{ /* escape */
			i++;
			if(isdigit(buf[i])) {
				str[j]=atoi(buf+i); 	/* decimal, NOT octal */
				if(isdigit(buf[++i]))	/* skip up to 3 digits */
					if(isdigit(buf[++i]))
						i++;
				continue; 
			}
			switch(buf[i++]) {
				case '\\':
					str[j]='\\';
					break;
				case '?':
					str[j]='?';
					break;
				case 'x':
					tmp[0]=buf[i++];        /* skip next character */
					tmp[1]=0;
					if(isxdigit(buf[i])) {  /* if another hex digit, skip too */
						tmp[1]=buf[i++];
						tmp[2]=0; 
					}
					str[j]=(char)ahtoul(tmp);
					break;
				case '\'':
					str[j]='\'';
					break;
				case '"':
					str[j]='"';
					break;
				case 'r':
					str[j]=CR;
					break;
				case 'n':
					str[j]=LF;
					break;
				case 't':
					str[j]=TAB;
					break;
				case 'b':
					str[j]=BS;
					break;
				case 'a':
					str[j]=BEL;
					break;
				case 'f':
					str[j]=FF;
					break;
				case 'v':
					str[j]=11;	/* VT */
					break;
				default:
					str[j]=buf[i];
					break; 
			}
			continue; 
		}
		str[j]=buf[i++]; 
	}
	str[j]=0;
	if((p=(char *)calloc(1,j+2))==NULL) { /* +1 for terminator, +1 for YNQX line */
		fprintf(stderr,"Error allocating %u bytes of memory from text.dat",j);
		return(NULL); 
	}
	strcpy(p,str);
	if(comment_ret)
		*comment_ret=strdup(comment);
	return(p);
}

char *format_as_cstr(char *orig)
{
	int		len=0;
	int		outpos=0;
	char	*ret=NULL;
	char	*in;
	char	*tmp;
	char	hex[32];

	if(!orig)
		return(NULL);
	for(in=orig; *in; in++) {
		if(outpos >= len-7) {
			len += strlen(in)+7;
			tmp=realloc(ret, len);
			if(tmp==NULL) {
				free(ret);
				return(NULL);
			}
			ret=tmp;
		}
		if(*in < ' ' || *in > '~') {
			sprintf(hex, "%02x", (unsigned char)*in);
			ret[outpos++]='\\';
			ret[outpos++]='x';
			ret[outpos++]=hex[0];
			ret[outpos++]=hex[1];
			ret[outpos++]='"';
			ret[outpos++]='"';
		}
		else
			ret[outpos++]=*in;
	}
	if(in==orig) {
		ret=(char *)malloc(1);
	}
	if(ret)
		ret[outpos]=0;
	return(ret);
}

int main(int argc, char **argv)
{
	FILE			*text_dat;
	char			*p;
	char			*cstr;
	char			*comment;
	char			*macro;
	unsigned long	lno;
	int				i=0;
	FILE			*text_h;
	FILE			*text_js;
	FILE			*text_defaults_c;

	if((text_dat=fopen("../../ctrl/text.dat","r"))==NULL) {
		fprintf(stderr,"Can't open text.dat!\n");
		return(1);
	}
	if((text_h=fopen("text.h", "w"))==NULL) {
		fprintf(stderr,"Can't open text.h!\n");
		return(1);
	}
	fputs("/* text.h */\n",text_h);
	fputs("\n",text_h);
	fputs("/* Synchronet static text string constants */\n",text_h);
	fputs("\n",text_h);
	fputs("/* $Id$ */\n",text_h);
	fputs("\n",text_h);
	fputs("/****************************************************************************\n",text_h);
	fputs(" * @format.tab-size 4		(Plain Text/Source Code File Header)			*\n",text_h);
	fputs(" * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*\n",text_h);
	fputs(" *																			*\n",text_h);
	fputs(" * Copyright 2008 Rob Swindell - http://www.synchro.net/copyright.html		*\n",text_h);
	fputs(" *																			*\n",text_h);
	fputs(" * This program is free software; you can redistribute it and/or			*\n",text_h);
	fputs(" * modify it under the terms of the GNU General Public License				*\n",text_h);
	fputs(" * as published by the Free Software Foundation; either version 2			*\n",text_h);
	fputs(" * of the License, or (at your option) any later version.					*\n",text_h);
	fputs(" * See the GNU General Public License for more details: gpl.txt or			*\n",text_h);
	fputs(" * http://www.fsf.org/copyleft/gpl.html										*\n",text_h);
	fputs(" *																			*\n",text_h);
	fputs(" * Anonymous FTP access to the most recent released source is available at	*\n",text_h);
	fputs(" * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*\n",text_h);
	fputs(" *																			*\n",text_h);
	fputs(" * Anonymous CVS access to the development source and modification history	*\n",text_h);
	fputs(" * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*\n",text_h);
	fputs(" * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*\n",text_h);
	fputs(" *     (just hit return, no password is necessary)							*\n",text_h);
	fputs(" * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*\n",text_h);
	fputs(" *																			*\n",text_h);
	fputs(" * For Synchronet coding style and modification guidelines, see				*\n",text_h);
	fputs(" * http://www.synchro.net/source.html										*\n",text_h);
	fputs(" *																			*\n",text_h);
	fputs(" * You are encouraged to submit any modifications (preferably in Unix diff	*\n",text_h);
	fputs(" * format) via e-mail to mods@synchro.net									*\n",text_h);
	fputs(" *																			*\n",text_h);
	fputs(" * Note: If this box doesn't appear square, then you need to fix your tabs.	*\n",text_h);
	fputs(" ****************************************************************************/\n",text_h);
	fputs("\n",text_h);
	fputs("/****************************************************************************/\n",text_h);
	fputs("/* Macros for elements of the array of pointers (text[]) to static text		*/\n",text_h);
	fputs("/* Auto-generated from CTRL\\TEXT.DAT										*/\n",text_h);
	fputs("/****************************************************************************/\n",text_h);
	fputs("\n",text_h);
	fputs("#ifndef _TEXT_H\n",text_h);
	fputs("#define _TEXT_H\n",text_h);
	fputs("\n",text_h);
	fputs("enum {\n",text_h);
	if((text_js=fopen("../../exec/text.js", "w"))==NULL) {
		fprintf(stderr,"Can't open text.js!\n");
		return(1);
	}
	fputs("/* text.js */\n",text_js);
	fputs("\n",text_js);
	fputs("/* Synchronet static text string constants */\n",text_js);
	fputs("\n",text_js);
	fputs("/* $Id$ */\n",text_js);
	fputs("\n",text_js);
	fputs("/****************************************************************************\n",text_js);
	fputs(" * @format.tab-size 4		(Plain Text/Source Code File Header)			*\n",text_js);
	fputs(" * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*\n",text_js);
	fputs(" *																			*\n",text_js);
	fputs(" * Copyright 2008 Rob Swindell - http://www.synchro.net/copyright.html		*\n",text_js);
	fputs(" *																			*\n",text_js);
	fputs(" * This program is free software; you can redistribute it and/or			*\n",text_js);
	fputs(" * modify it under the terms of the GNU General Public License				*\n",text_js);
	fputs(" * as published by the Free Software Foundation; either version 2			*\n",text_js);
	fputs(" * of the License, or (at your option) any later version.					*\n",text_js);
	fputs(" * See the GNU General Public License for more details: gpl.txt or			*\n",text_js);
	fputs(" * http://www.fsf.org/copyleft/gpl.html										*\n",text_js);
	fputs(" *																			*\n",text_js);
	fputs(" * Anonymous FTP access to the most recent released source is available at	*\n",text_js);
	fputs(" * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*\n",text_js);
	fputs(" *																			*\n",text_js);
	fputs(" * Anonymous CVS access to the development source and modification history	*\n",text_js);
	fputs(" * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*\n",text_js);
	fputs(" * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*\n",text_js);
	fputs(" *     (just hit return, no password is necessary)							*\n",text_js);
	fputs(" * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*\n",text_js);
	fputs(" *																			*\n",text_js);
	fputs(" * For Synchronet coding style and modification guidelines, see				*\n",text_js);
	fputs(" * http://www.synchro.net/source.html										*\n",text_js);
	fputs(" *																			*\n",text_js);
	fputs(" * You are encouraged to submit any modifications (preferably in Unix diff	*\n",text_js);
	fputs(" * format) via e-mail to mods@synchro.net									*\n",text_js);
	fputs(" *																			*\n",text_js);
	fputs(" * Note: If this box doesn't appear square, then you need to fix your tabs.	*\n",text_js);
	fputs(" ****************************************************************************/\n",text_js);
	fputs("\n",text_js);
	fputs("/****************************************************************************/\n",text_js);
	fputs("/* Values for elements of the array of pointers (bbs.text()) to static text	*/\n",text_js);
	fputs("/* Auto-generated from CTRL\\TEXT.DAT										*/\n",text_js);
	fputs("/****************************************************************************/\n",text_js);
	fputs("\n",text_js);
	if((text_defaults_c=fopen("text_defaults.c","w"))==NULL) {
		fprintf(stderr,"Can't open text_defaults.c!\n");
		return(1);
	}
	fputs("/* text_defaults.c */\n",text_defaults_c);
	fputs("\n",text_defaults_c);
	fputs("/* Synchronet default text strings */\n",text_defaults_c);
	fputs("\n",text_defaults_c);
	fputs("/* $Id$ */\n",text_defaults_c);
	fputs("\n",text_defaults_c);
	fputs("/****************************************************************************\n",text_defaults_c);
	fputs(" * @format.tab-size 4		(Plain Text/Source Code File Header)			*\n",text_defaults_c);
	fputs(" * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*\n",text_defaults_c);
	fputs(" *																			*\n",text_defaults_c);
	fputs(" * Auto-generated from CTRL\\TEXT.DAT										*\n",text_defaults_c);
	fputs(" ****************************************************************************/\n",text_defaults_c);
	fputs("\n",text_defaults_c);
	fputs("#include \"text_defaults.h\"\n",text_defaults_c);
	fputs("\n",text_defaults_c);
	fputs("const char * const text_defaults[TOTAL_TEXT]={\n",text_defaults_c);
	do {
		i++;
		p=readtext(text_dat, &comment);
		if(p!=NULL) {
			cstr=format_as_cstr(p);
			if(cstr==NULL) {
				fprintf(stderr,"Error creating C string! for %d\n", i+1);
			}
			lno=strtoul(comment, &macro, 10);
			while(isspace(*macro))
				macro++;
			if(lno != i) {
				fprintf(stderr,"Mismatch! %s has %d... should be %d\n", comment, lno, i);
			}
			fprintf(text_h, "\t%c%s\n", i==1?' ':',', macro);
			fprintf(text_js, "var %s=%d;\n", macro, i);
			fprintf(text_defaults_c, "\t%c\"%s\"\n", i==1?' ':',', cstr);
		}
	} while(p != NULL);
	fclose(text_dat);
	
	fputs("\n",text_h);
	fputs("\t,TOTAL_TEXT\n",text_h);
	fputs("};\n",text_h);
	fputs("\n",text_h);
	fputs("#endif\n",text_h);
	fclose(text_h);
	fputs("\n",text_h);
	fprintf(text_h, "var TOTAL_TEXT=%d;\n",i);
	fclose(text_js);
	fputs("};\n",text_defaults_c);
	fclose(text_defaults_c);
}
