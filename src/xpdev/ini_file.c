/* ini_file.c */

/* Functions to parse ini files */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include <stdlib.h>		/* strtol */
#include <string.h>		/* strlen */
#include <ctype.h>		/* isdigit */
#include "sockwrap.h"	/* inet_addr */
#include "ini_file.h"

#define MAX_LINE_LEN	256		/* includes '\0' */
#define MAX_VALUE_LEN	128		/* includes '\0' */

/****************************************************************************/
/* Truncates white-space chars off end of 'str'								*/
/****************************************************************************/
static void truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=SP) c--;
	str[c]=0;
}

static BOOL find_section(FILE* fp, const char* section)
{
	char*	p;
	char*	tp;
	char	str[MAX_LINE_LEN];

	rewind(fp);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		p=str;
		while(*p && *p<=' ') p++;
		if(*p!='[')
			continue;
		p++;
		tp=strchr(p,']');
		if(tp==NULL)
			continue;
		*tp=0;
		if(stricmp(p,section)==0)
			return(TRUE);
	}
	return(FALSE);
}

static char* get_value(FILE* fp, const char* section, const char* key)
{
	char*	p;
	char*	tp;
	char	str[MAX_LINE_LEN];
	static char value[MAX_VALUE_LEN];

	if(fp==NULL)
		return(NULL);

	if(!find_section(fp,section))
		return(NULL);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		p=str;
		while(*p && *p<=' ') p++;
		if(*p==';')
			continue;
		if(*p=='[')
			break;
		tp=strchr(p,'=');
		if(tp==NULL)
			continue;
		*tp=0;
		truncsp(p);
		if(stricmp(p,key)!=0)
			continue;
		/* key found */
		p=tp+1;
		while(*p && *p<=' ') p++;
		truncsp(p);
		SAFECOPY(value,p);
		return(value);
	}

	return(NULL);
}

char* iniGetString(FILE* fp, const char* section, const char* key, const char* deflt)
{
	char* value;

	if((value=get_value(fp,section,key))==NULL)
		return((char*)deflt);

	if(*value==0)		/* blank value */
		return((char*)deflt);

	return(value);
}

char** iniGetStringList(FILE* fp, const char* section, const char* key
						 ,const char* sep, const char* deflt)
{
	char*	value;
	char**	lp;
	char**	np;
	char*	token;
	char	list[MAX_VALUE_LEN];
	ulong	items=0;

	if((value=get_value(fp,section,key))==NULL || *value==0 /* blank */)
		value=(char*)deflt;

	SAFECOPY(list,value);

	if((lp=malloc(sizeof(char*)))==NULL)
		return(NULL);

	token=strtok(list,sep);
	while(token!=NULL) {
		truncsp(token);
		if((np=realloc(lp,sizeof(char*)*(items+2)))==NULL)
			break;
		lp=np;
		if((lp[items]=malloc(strlen(token)+1))==NULL)
			break;
		strcpy(lp[items++],token);
		token=strtok(NULL,sep);
	}

	lp[items]=NULL;	/* terminate list */

	return(lp);
}

void* iniFreeStringList(char** list)
{
	ulong	i;

	if(list==NULL)
		return(NULL);

	for(i=0;list[i]!=NULL;i++)
		free(list[i]);

	free(list);
	return(NULL);
}

void* iniFreeNamedStringList(named_string_t** list)
{
	ulong	i;

	if(list==NULL)
		return(NULL);

	for(i=0;list[i]!=NULL;i++) {
		if(list[i]->name!=NULL)
			free(list[i]->name);
		if(list[i]->value!=NULL)
			free(list[i]->value);
		free(list[i]);
	}

	free(list);
	return(NULL);
}

char** iniGetSectionList(FILE* fp)
{
	char*	p;
	char*	tp;
	char**	lp;
	char**	np;
	char	str[MAX_LINE_LEN];
	ulong	items=0;

	if((lp=malloc(sizeof(char*)))==NULL)
		return(NULL);

	*lp=NULL;

	if(fp==NULL)
		return(lp);

	rewind(fp);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		p=str;
		while(*p && *p<=' ') p++;
		if(*p!='[')
			continue;
		p++;
		tp=strchr(p,']');
		if(tp==NULL)
			continue;
		*tp=0;
		if((np=realloc(lp,sizeof(char*)*(items+2)))==NULL)
			break;
		lp=np;
		if((lp[items]=malloc(strlen(p)+1))==NULL)
			break;
		strcpy(lp[items++],p);
	}

	lp[items]=NULL;	/* terminate list */

	return(lp);
}

char** iniGetKeyList(FILE* fp, const char* section)
{
	char*	p;
	char*	tp;
	char**	lp;
	char**	np;
	char	str[MAX_LINE_LEN];
	ulong	items=0;

	if((lp=malloc(sizeof(char*)))==NULL)
		return(NULL);

	*lp=NULL;

	if(fp==NULL)
		return(lp);

	rewind(fp);

	if(!find_section(fp,section))
		return(lp);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		p=str;
		while(*p && *p<=' ') p++;
		if(*p==';')
			continue;
		if(*p=='[')
			break;
		tp=strchr(p,'=');
		if(tp==NULL)
			continue;
		*tp=0;
		truncsp(p);
		if((np=realloc(lp,sizeof(char*)*(items+2)))==NULL)
			break;
		lp=np;
		if((lp[items]=malloc(strlen(p)+1))==NULL)
			break;
		strcpy(lp[items++],p);
	}

	lp[items]=NULL;	/* terminate list */

	return(lp);
}

named_string_t**
iniGetNamedStringList(FILE* fp, const char* section)
{
	char*	p;
	char*	name;
	char*	value;
	char*	tp;
	char	str[MAX_LINE_LEN];
	ulong	items=0;
	named_string_t** lp;
	named_string_t** np;

	if((lp=malloc(sizeof(named_string_t*)))==NULL)
		return(NULL);

	*lp=NULL;

	if(fp==NULL)
		return(lp);

	rewind(fp);

	if(!find_section(fp,section))
		return(lp);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		p=str;
		while(*p && *p<=' ') p++;
		if(*p==';')
			continue;
		if(*p=='[')
			break;
		tp=strchr(p,'=');
		if(tp==NULL)
			continue;
		*tp=0;
		truncsp(p);
		name=p;
		p=tp+1;
		while(*p && *p<=' ') p++;
		truncsp(p);
		value=p;
		if((np=realloc(lp,sizeof(named_string_t*)*(items+2)))==NULL)
			break;
		lp=np;
		if((lp[items]=malloc(sizeof(named_string_t)))==NULL)
			break;
		if((lp[items]->name=malloc(strlen(name)+1))==NULL)
			break;
		strcpy(lp[items]->name,name);
		if((lp[items]->value=malloc(strlen(value)+1))==NULL)
			break;
		strcpy(lp[items]->value,value);
		items++;
	}

	lp[items]=NULL;	/* terminate list */

	return(lp);
}

/* These functions read a single key of the specified type */

long iniGetInteger(FILE* fp, const char* section, const char* key, long deflt)
{
	char* value;

	if((value=get_value(fp,section,key))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(strtol(value,NULL,0));
}

ushort iniGetShortInt(FILE* fp, const char* section, const char* key, ushort deflt)
{
	return((ushort)iniGetInteger(fp, section, key, deflt));
}

ulong iniGetIpAddress(FILE* fp, const char* section, const char* key, ulong deflt)
{
	char* value;

	if((value=get_value(fp,section,key))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	if(strchr(value,'.')==NULL)
		return(strtol(value,NULL,0));

	return(ntohl(inet_addr(value)));
}

double iniGetFloat(FILE* fp, const char* section, const char* key, double deflt)
{
	char* value;

	if((value=get_value(fp,section,key))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(atof(value));
}

BOOL iniGetBool(FILE* fp, const char* section, const char* key, BOOL deflt)
{
	char* value;

	if((value=get_value(fp,section,key))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	if(!stricmp(value,"TRUE"))
		return(TRUE);
	if(!stricmp(value,"FALSE"))
		return(FALSE);

	return(strtol(value,NULL,0));
}

ulong iniGetBitField(FILE* fp, const char* section, const char* key, 
						ini_bitdesc_t* bitdesc, ulong deflt)
{
	int		b,i;
	char*	p;
	char*	tp;
	char*	value;
	ulong	v=0;

	if((value=get_value(fp,section,key))==NULL)
		return(deflt);

	if(isdigit(value[0]))
		return(strtoul(value,NULL,0));

	p=value;
	for(b=0;*p && b<32;b++) {
		tp=strchr(p,'|');
		if(tp!=NULL)
			*tp=0;
		truncsp(p);

		for(i=0;bitdesc[i].name;i++)
			if(!stricmp(bitdesc[i].name,p))
				break;

		if(bitdesc[i].name)
			v|=bitdesc[i].bit;

		if(tp==NULL)
			break;

		p=tp+1;
		while(*p && *p<=' ') p++;
	}

	return(v);
}
