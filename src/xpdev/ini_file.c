/* ini_file.c */

/* Functions to parse ini files */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
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
		if(fgets(str,sizeof(str)-1,fp)==NULL)
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

	if(!find_section(fp,section))
		return(NULL);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str)-1,fp)==NULL)
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

char* DLLCALL
iniReadString(FILE* fp, const char* section, const char* key, const char* deflt)
{
	char* value;

	if((value=get_value(fp,section,key))==NULL)
		return((char*)deflt);

	return(value);
}

long DLLCALL
iniReadInteger(FILE* fp, const char* section, const char* key, long deflt)
{
	char* value;

	if((value=get_value(fp,section,key))==NULL)
		return(deflt);

	return(strtol(value,NULL,0));
}

ushort DLLCALL
iniReadShortInt(FILE* fp, const char* section, const char* key, ushort deflt)
{
	return((ushort)iniReadInteger(fp, section, key, deflt));
}

ulong DLLCALL
iniReadIpAddress(FILE* fp, const char* section, const char* key, ulong deflt)
{
	char* value;

	if((value=get_value(fp,section,key))==NULL)
		return(deflt);

	if(strchr(value,'.')==NULL)
		return(strtol(value,NULL,0));

	return(inet_addr(value));
}

double DLLCALL
iniReadFloat(FILE* fp, const char* section, const char* key, double deflt)
{
	char* value;

	if((value=get_value(fp,section,key))==NULL)
		return(deflt);

	return(atof(value));
}

BOOL DLLCALL 
iniReadBool(FILE* fp, const char* section, const char* key, BOOL deflt)
{
	char* value;

	if((value=get_value(fp,section,key))==NULL)
		return(deflt);

	if(!stricmp(value,"TRUE"))
		return(TRUE);
	if(!stricmp(value,"FALSE"))
		return(FALSE);

	return(strtol(value,NULL,0));
}

ulong DLLCALL
iniReadBitField(FILE* fp, const char* section, const char* key, 
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
