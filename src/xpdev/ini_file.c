/* ini_file.c */

/* Functions to parse ini files */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include "filewrap.h"	/* chsize */
#include "ini_file.h"

/* Maximum length of entire line, includes '\0' */
#define INI_MAX_LINE_LEN	(INI_MAX_VALUE_LEN*2)

#define NEW_SECTION	((char*)~0)

static ini_style_t default_style;

/****************************************************************************/
/* Truncates white-space chars off end of 'str'								*/
/****************************************************************************/
static void truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=' ') c--;
	str[c]=0;
}

static char* section_name(char* p)
{
	char*	tp;

	SKIP_WHITESPACE(p);
	if(*p!='[')
		return(NULL);
	p++;
	SKIP_WHITESPACE(p);
	tp=strchr(p,']');
	if(tp==NULL)
		return(NULL);
	*tp=0;
	truncsp(p);

	return(p);
}

static BOOL find_section(FILE* fp, const char* section)
{
	char*	p;
	char	str[INI_MAX_LINE_LEN];

	rewind(fp);

	if(section==ROOT_SECTION)
		return(TRUE);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		if((p=section_name(str))==NULL)
			continue;
		if(stricmp(p,section)==0)
			return(TRUE);
	}
	return(FALSE);
}

static size_t find_section_index(str_list_t list, const char* section)
{
	char*	p;
	char	str[INI_MAX_VALUE_LEN];
	size_t	i;

	if(section==ROOT_SECTION)
		return(0);

	for(i=0; list[i]!=NULL; i++) {
		SAFECOPY(str,list[i]);
		if((p=section_name(str))!=NULL && stricmp(p,section)==0)
			return(i+1);
	}

	return(i);
}

static char* key_name(char* p, char** vp)
{
	/* Parse value name */
	SKIP_WHITESPACE(p);
	if(*p==';')
		return(NULL);
	if(*p=='[')
		return(NEW_SECTION);
	*vp=strchr(p,'=');
	if(*vp==NULL)
		return(NULL);
	*(*vp)=0;
	truncsp(p);

	/* Parse value */
	(*vp)++;
	SKIP_WHITESPACE(*vp);
	truncsp(*vp);

	return(p);
}

static char* get_value(FILE* fp, const char* section, const char* key, char* value)
{
	char*	p;
	char*	vp;
	char	str[INI_MAX_LINE_LEN];

	if(fp==NULL)
		return(NULL);

	if(!find_section(fp,section))
		return(NULL);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		if((p=key_name(str,&vp))==NULL)
			continue;
		if(p==NEW_SECTION)
			break;
		if(stricmp(p,key)!=0)
			continue;
		/* key found */
		sprintf(value,"%.*s",INI_MAX_VALUE_LEN-1,vp);
		return(value);
	}

	return(NULL);
}

static size_t find_value_index(str_list_t list, const char* section, const char* key, char* value)
{
	char	str[INI_MAX_LINE_LEN];
	char*	p;
	char*	vp;
	size_t	i;

	value[0]=0;
	for(i=find_section_index(list, section); list[i]!=NULL; i++) {
		SAFECOPY(str, list[i]);
		if((p=key_name(str,&vp))==NULL)
			continue;
		if(p==NEW_SECTION)
			break;
		if(stricmp(p,key)!=0)
			continue;
		strcpy(value,vp);
		return(i);
	}

	return(i);
}

size_t iniAddSection(str_list_t* list, const char* section
					,ini_style_t* style)
{
	char	str[INI_MAX_LINE_LEN];
	size_t	i;

	if(section==ROOT_SECTION)
		return(0);

	i=find_section_index(*list, section);
	if((*list)[i]==NULL) {
		if(style->section_separator!=NULL)
			strListAppend(list, style->section_separator, i++);
		sprintf(str,"[%s]",section);
		strListAppend(list, str, i);
	}

	return(i);
}

char* iniSetString(str_list_t* list, const char* section, const char* key, const char* value
				 ,ini_style_t* style)
{
	char	str[INI_MAX_LINE_LEN];
	char	curval[INI_MAX_VALUE_LEN];
	size_t	i;

	if(style==NULL)
		style=&default_style;

	iniAddSection(list, section, style);

	if(key==NULL)
		return(NULL);
	if(style->key_prefix==NULL)
		style->key_prefix="";
	if(style->value_separator==NULL)
		style->value_separator="=";
	sprintf(str, "%s%-*s%s%s", style->key_prefix, style->key_len, key, style->value_separator, value);
	i=find_value_index(*list, section, key, curval);
	if((*list)[i]==NULL)
		return strListInsert(list, str, i);

	if(strcmp(curval,value)==0)
		return((*list)[i]);	/* no change */

	return strListReplace(*list, i, str);
}

char* iniSetInteger(str_list_t* list, const char* section, const char* key, long value
					,ini_style_t* style)
{
	char	str[INI_MAX_VALUE_LEN];

	sprintf(str,"%ld",value);
	return iniSetString(list, section, key, str, style);
}

char* iniSetShortInt(str_list_t* list, const char* section, const char* key, ushort value
					,ini_style_t* style)
{
	char	str[INI_MAX_VALUE_LEN];

	sprintf(str,"%hu",value);
	return iniSetString(list, section, key, str, style);
}

char* iniSetHexInt(str_list_t* list, const char* section, const char* key, ulong value
					,ini_style_t* style)
{
	char	str[INI_MAX_VALUE_LEN];

	sprintf(str,"0x%lx",value);
	return iniSetString(list, section, key, str, style);
}

char* iniSetFloat(str_list_t* list, const char* section, const char* key, double value
					,ini_style_t* style)
{
	char	str[INI_MAX_VALUE_LEN];

	sprintf(str,"%g",value);
	return iniSetString(list, section, key, str, style);
}

char* iniSetIpAddress(str_list_t* list, const char* section, const char* key, ulong value
					,ini_style_t* style)
{
	struct in_addr in_addr;
	in_addr.s_addr=value;
	return iniSetString(list, section, key, inet_ntoa(in_addr), style);
}

char* iniSetBool(str_list_t* list, const char* section, const char* key, BOOL value
					,ini_style_t* style)
{
	return iniSetString(list, section, key, value ? "true":"false", style);
}

char* iniSetBitField(str_list_t* list, const char* section, const char* key
					 ,ini_bitdesc_t* bitdesc, ulong value, ini_style_t* style)
{
	char	str[INI_MAX_VALUE_LEN];
	int		i;

	if(style==NULL)
		style=&default_style;
	if(style->bit_separator==NULL)
		style->bit_separator="|";
	str[0]=0;
	for(i=0;bitdesc[i].name;i++) {
		if((value&bitdesc[i].bit)==0)
			continue;
		if(str[0])
			strcat(str,style->bit_separator);
		strcat(str,bitdesc[i].name);
		value&=~bitdesc[i].bit;
	}
	if(value) {	/* left over bits? */
		if(str[0])
			strcat(str,style->bit_separator);
		sprintf(str+strlen(str), "0x%lX", value);
	}
	return iniSetString(list, section, key, str, style);
}

char* iniSetStringList(str_list_t* list, const char* section, const char* key
					,const char* sep, str_list_t val_list, ini_style_t* style)
{
	char	value[INI_MAX_VALUE_LEN];
	size_t	i;

	value[0]=0;

	if(sep==NULL)
		sep=",";

	if(val_list!=NULL)
		for(i=0; val_list[i]!=NULL; i++) {
			if(value[0])
				strcat(value,sep);
			strcat(value,val_list[i]);
		}

	return iniSetString(list, section, key, value, style);
}

char* iniGetString(FILE* fp, const char* section, const char* key, const char* deflt, char* value)
{
	if(get_value(fp,section,key,value)==NULL || *value==0 /* blank */) {
		if(deflt==NULL)
			return(NULL);
		sprintf(value,"%.*s",INI_MAX_VALUE_LEN-1,deflt);
	}

	return(value);
}

str_list_t iniGetStringList(FILE* fp, const char* section, const char* key
						 ,const char* sep, const char* deflt)
{
	char*	value;
	char	buf[INI_MAX_VALUE_LEN];
	char*	token;
	char	list[INI_MAX_VALUE_LEN];
	ulong	items=0;
	str_list_t	lp;

	if((value=get_value(fp,section,key,buf))==NULL || *value==0 /* blank */)
		value=(char*)deflt;

	SAFECOPY(list,value);

	if((lp=strListInit())==NULL)
		return(NULL);

	if(sep==NULL)
		sep=",";

	token=strtok(list,sep);
	while(token!=NULL) {
		truncsp(token);
		if(strListAppend(&lp,token,items++)==NULL)
			break;
		token=strtok(NULL,sep);
	}
	return(lp);
}

void* iniFreeStringList(str_list_t list)
{
	strListFree(&list);
	return(list);
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

str_list_t iniGetSectionList(FILE* fp, const char* prefix)
{
	char*	p;
	char	str[INI_MAX_LINE_LEN];
	ulong	items=0;
	str_list_t	lp;

	if((lp=strListInit())==NULL)
		return(NULL);

	if(fp==NULL)
		return(lp);

	rewind(fp);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		if((p=section_name(str))==NULL)
			continue;
		if(prefix!=NULL)
			if(strnicmp(p,prefix,strlen(prefix))!=0)
				continue;
		if(strListAppend(&lp,p,items++)==NULL)
			break;
	}

	return(lp);
}

str_list_t iniGetKeyList(FILE* fp, const char* section)
{
	char*	p;
	char*	vp;
	char	str[INI_MAX_LINE_LEN];
	ulong	items=0;
	str_list_t	lp;

	if((lp=strListInit())==NULL)
		return(NULL);

	if(fp==NULL)
		return(lp);

	rewind(fp);

	if(!find_section(fp,section))
		return(lp);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		if((p=key_name(str,&vp))==NULL)
			continue;
		if(p==NEW_SECTION)
			break;
		if(strListAppend(&lp,p,items++)==NULL)
			break;
	}

	return(lp);
}

named_string_t**
iniGetNamedStringList(FILE* fp, const char* section)
{
	char*	name;
	char*	value;
	char	str[INI_MAX_LINE_LEN];
	ulong	items=0;
	named_string_t** lp;
	named_string_t** np;

	if((lp=(named_string_t**)malloc(sizeof(named_string_t*)))==NULL)
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
		if((name=key_name(str,&value))==NULL)
			continue;
		if(name==NEW_SECTION)
			break;
		if((np=(named_string_t**)realloc(lp,sizeof(named_string_t*)*(items+2)))==NULL)
			break;
		lp=np;
		if((lp[items]=(named_string_t*)malloc(sizeof(named_string_t)))==NULL)
			break;
		if((lp[items]->name=(char*)malloc(strlen(name)+1))==NULL)
			break;
		strcpy(lp[items]->name,name);
		if((lp[items]->value=(char*)malloc(strlen(value)+1))==NULL)
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
	char*	value;
	char	buf[INI_MAX_VALUE_LEN];

	if((value=get_value(fp,section,key,buf))==NULL)
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
	char	buf[INI_MAX_VALUE_LEN];
	char*	value;

	if((value=get_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	if(strchr(value,'.')==NULL)
		return(strtol(value,NULL,0));

	return(ntohl(inet_addr(value)));
}

double iniGetFloat(FILE* fp, const char* section, const char* key, double deflt)
{
	char	buf[INI_MAX_VALUE_LEN];
	char*	value;

	if((value=get_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(atof(value));
}

BOOL iniGetBool(FILE* fp, const char* section, const char* key, BOOL deflt)
{
	char	buf[INI_MAX_VALUE_LEN];
	char*	value;

	if((value=get_value(fp,section,key,buf))==NULL)
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
	int		i;
	char*	p;
	char*	tp;
	char*	value;
	char	buf[INI_MAX_VALUE_LEN];
	ulong	v=0;

	if((value=get_value(fp,section,key,buf))==NULL)
		return(deflt);

	for(p=value;*p;) {
		tp=strchr(p,'|');
		if(tp!=NULL)
			*tp=0;
		truncsp(p);

		for(i=0;bitdesc[i].name;i++)
			if(!stricmp(bitdesc[i].name,p))
				break;

		if(bitdesc[i].name)
			v|=bitdesc[i].bit;
		else
			v|=strtoul(p,NULL,0);

		if(tp==NULL)
			break;

		p=tp+1;
		SKIP_WHITESPACE(p);
	}

	return(v);
}

str_list_t iniReadFile(FILE* fp)
{
	size_t		i;
	str_list_t	list;
	
	rewind(fp);

	list = strListReadFile(fp, NULL, INI_MAX_LINE_LEN);
	if(list!=NULL) {
		/* truncate the white-space off end of strings */
		for(i=0; list[i]!=NULL; i++)
			truncsp(list[i]);
	}

	return(list);
}

BOOL iniWriteFile(FILE* fp, const str_list_t list)
{
	size_t		count;

	rewind(fp);

	if(chsize(fileno(fp),0)!=0)	/* truncate */
		return(FALSE);

	count = strListWriteFile(fp,list,"\n");

	return(count == strListCount(list));
}
