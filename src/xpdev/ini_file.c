/* Functions to create and parse .ini files */

/* $Id$ */
// vi: tabstop=4

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

#include <stdlib.h>		/* strtol */
#include <string.h>		/* strlen */
#include <ctype.h>		/* isdigit */
#include <math.h>		/* fmod */
#include "xpdatetime.h"	/* isoDateTime_t */
#include "datewrap.h"	/* ctime_r */
#include "dirwrap.h"	/* fexist */
#include "filewrap.h"	/* chsize */
#include "ini_file.h"
#if !defined(NO_SOCKET_SUPPORT)
#include "sockwrap.h"
#endif

/* Maximum length of entire line, includes '\0' */
#define INI_MAX_LINE_LEN		(INI_MAX_VALUE_LEN*2)
#define INI_COMMENT_CHAR		';'
#define INI_OPEN_SECTION_CHAR	'['
#define INI_CLOSE_SECTION_CHAR	']'
#define INI_SECTION_NAME_SEP	"|"
#define INI_BIT_SEP				'|'
#define INI_NEW_SECTION			((char*)~0)
#define INI_EOF_DIRECTIVE		"!eof"
#define INI_INCLUDE_DIRECTIVE	"!include"
#define INI_INCLUDE_MAX			10000

static ini_style_t default_style;

void DLLCALL iniSetDefaultStyle(ini_style_t style)
{
	default_style = style;
}

/* These correlate with the LOG_* definitions in syslog.h/gen_defs.h */
static char* logLevelStringList[]
	= {"Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Informational", "Debugging", NULL};

str_list_t DLLCALL iniLogLevelStringList(void)
{
	return(logLevelStringList);
}

static BOOL is_eof(char* str)
{
	return(*str=='!' && stricmp(truncsp(str),INI_EOF_DIRECTIVE)==0);
}

static char* section_name(char* p)
{
	char*	tp;

	SKIP_WHITESPACE(p);
	if(*p!=INI_OPEN_SECTION_CHAR)
		return(NULL);
	p++;
	SKIP_WHITESPACE(p);
	tp=strrchr(p,INI_CLOSE_SECTION_CHAR);
	if(tp==NULL)
		return(NULL);
	*tp=0;
	truncsp(p);

	return(p);
}

static BOOL section_match(const char* name, const char* compare, BOOL case_sensitive)
{
	BOOL found=FALSE;
	str_list_t names=strListSplitCopy(NULL,name,INI_SECTION_NAME_SEP);
	str_list_t comps=strListSplitCopy(NULL,compare,INI_SECTION_NAME_SEP);
	size_t	i,j;
	char*	n;
	char*	c;

	/* Ignore trailing whitespace */
	for(i=0; names[i]!=NULL; i++)
		truncsp(names[i]);
	for(i=0; comps[i]!=NULL; i++)
		truncsp(comps[i]);

	/* Search for matches */
	for(i=0; names[i]!=NULL && !found; i++)
		for(j=0; comps[j]!=NULL && !found; j++) {
			n=names[i];
			SKIP_WHITESPACE(n);
			c=comps[j];
			SKIP_WHITESPACE(c);
			if (case_sensitive)
				found = strcmp(n, c) == 0;
			else
				found = stricmp(n, c) == 0;
		}

	strListFree(&names);
	strListFree(&comps);

	return(found);
}

static BOOL seek_section(FILE* fp, const char* section)
{
	char*	p;
	char	str[INI_MAX_LINE_LEN];

	rewind(fp);

	if(section==ROOT_SECTION)
		return(TRUE);

	/* Perform case-sensitive search first */
	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		if(is_eof(str))
			break;
		if((p=section_name(str))==NULL)
			continue;
		if(section_match(p, section, /* case-sensitive */TRUE))
			return(TRUE);
	}

	/* Then perform case-insensitive search */
	rewind(fp);
	while (!feof(fp)) {
		if (fgets(str, sizeof(str), fp) == NULL)
			break;
		if (is_eof(str))
			break;
		if ((p = section_name(str)) == NULL)
			continue;
		if (section_match(p, section, /* case-sensitive */FALSE))
			return(TRUE);
	}

	return(FALSE);
}

static size_t find_section_index(str_list_t list, const char* section)
{
	char*	p;
	char	str[INI_MAX_VALUE_LEN];
	size_t	i;

	/* Perform case-sensitive search first */
	for (i = 0; list[i] != NULL; i++) {
		SAFECOPY(str, list[i]);
		if(is_eof(str))
			return(strListCount(list));
		if((p=section_name(str))!=NULL && section_match(p, section, /* case-sensitive */TRUE))
			return(i);
	}

	/* Then perform case-insensitive search */
	for (i = 0; list[i] != NULL; i++) {
		SAFECOPY(str, list[i]);
		if (is_eof(str))
			return(strListCount(list));
		if ((p = section_name(str)) != NULL && section_match(p, section, /* case-sensitive */FALSE))
			return(i);
	}

	return(i);
}

static size_t find_section(str_list_t list, const char* section)
{
	size_t	i;

	if(section==ROOT_SECTION)
		return(0);

	i=find_section_index(list,section);
	if(list[i]!=NULL)
		i++;
	return(i);
}

static char* key_name(char* p, char** vp)
{
	char* equal;
	char* colon;
	char* tp;

    *vp=NULL;

	if(p==NULL)
		return(NULL);

	/* Parse value name */
	SKIP_WHITESPACE(p);
	if(*p==INI_COMMENT_CHAR)
		return(NULL);
	if(*p==INI_OPEN_SECTION_CHAR)
		return(INI_NEW_SECTION);
	equal=strchr(p,'=');
	colon=strchr(p,':');
	if(colon==NULL || (equal!=NULL && equal<colon)) {
		*vp=equal;
		colon=NULL;
	} else
		*vp=colon;

	if(*vp==NULL)
		return(NULL);

	*(*vp)=0;
	truncsp(p);

	/* Parse value */
	(*vp)++;
	SKIP_WHITESPACE(*vp);
	if(colon!=NULL) {		/* string literal value */
		truncnl(*vp);		/* "key : value" - truncate new-line chars only */
		if(*(*vp) == '"') {	/* handled quoted-strings here */
			(*vp)++;
			tp = strrchr(*vp, '"');
			if(tp != NULL) {
				*tp = 0;
			}
		}
		c_unescape_str(*vp);
	} else
		truncsp(*vp);		/* "key = value" - truncate all white-space chars */

	return(p);
}

static char* read_value(FILE* fp, const char* section, const char* key, char* value)
{
	char*	p;
	char*	vp=NULL;
	char	str[INI_MAX_LINE_LEN];

	if(fp==NULL)
		return(NULL);

	if(!seek_section(fp,section))
		return(NULL);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		if(is_eof(str))
			break;
		if((p=key_name(str,&vp))==NULL)
			continue;
		if(p==INI_NEW_SECTION)
			break;
		if(stricmp(p,key)!=0)
			continue;
		if(vp==NULL)
			break;
		/* key found */
		sprintf(value,"%.*s",INI_MAX_VALUE_LEN-1,vp);
		return(value);
	}

	return(NULL);
}

static size_t get_value(str_list_t list, const char* section, const char* key, char* value, char** vpp)
{
	char    str[INI_MAX_LINE_LEN];
	char*	p;
	char*	vp;
	size_t	i;

	if(value!=NULL)
		value[0]=0;
	if(vpp!=NULL)
		*vpp=NULL;
	if(list==NULL)
		return 0;

	for(i=find_section(list, section); list[i]!=NULL; i++) {
		SAFECOPY(str,list[i]);
		if(is_eof(str))
			break;
		if((p=key_name(str,&vp))==NULL)
			continue;
		if(p==INI_NEW_SECTION)
			break;
		if(stricmp(p,key)!=0)
			continue;
		if(value!=NULL)
			sprintf(value,"%.*s",INI_MAX_VALUE_LEN-1,vp);
		if(vpp!=NULL)
			*vpp=list[i] + (vp - str);
		return(i);
	}

	return(i);
}

BOOL DLLCALL iniSectionExists(str_list_t list, const char* section)
{
	size_t	i;

	if(section==ROOT_SECTION)
		return(TRUE);

	i=find_section_index(list,section);
	return(list[i]!=NULL);
}

str_list_t DLLCALL	iniGetSection(str_list_t list, const char *section)
{
	size_t		i;
	str_list_t	retval;
	char		*p;

	if(list==NULL)
		return(NULL);

	if((retval=strListInit())==NULL)
		return(NULL);

	i=find_section(list,section);
	if(list[i]!=NULL) {
		strListPush(&retval, list[i]);
		for(i++;list[i]!=NULL;i++) {
			p=list[i];
			SKIP_WHITESPACE(p);
			if(*p==INI_OPEN_SECTION_CHAR)
				break;
			strListPush(&retval, list[i]);
		}
	}
	return(retval);
}

BOOL DLLCALL iniKeyExists(str_list_t list, const char* section, const char* key)
{
	size_t	i;

	if(list==NULL)
		return(FALSE);

	i=get_value(list, section, key, NULL, NULL);

	if(list[i]==NULL || *(list[i])==INI_OPEN_SECTION_CHAR)
		return(FALSE);

	return(TRUE);
}

BOOL DLLCALL iniValueExists(str_list_t list, const char* section, const char* key)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	return(vp!=NULL && *vp!=0);
}

BOOL DLLCALL iniRemoveKey(str_list_t* list, const char* section, const char* key)
{
	size_t	i;
	char*	vp=NULL;

	i=get_value(*list, section, key, NULL, &vp);

	if(vp==NULL)
		return(FALSE);

	return(strListDelete(list,i));
}

BOOL DLLCALL iniRemoveValue(str_list_t* list, const char* section, const char* key)
{
	char*	vp=NULL;

	get_value(*list, section, key, NULL, &vp);

	if(vp==NULL)
		return(FALSE);

	*vp=0;
	return(TRUE);
}

BOOL DLLCALL iniRemoveSection(str_list_t* list, const char* section)
{
	size_t	i;

	i=find_section_index(*list,section);
	if((*list)[i]==NULL)	/* not found */
		return(FALSE);
	do {
		strListDelete(list,i);
	} while((*list)[i]!=NULL && *(*list)[i]!=INI_OPEN_SECTION_CHAR);

	return(TRUE);
}

BOOL DLLCALL iniRemoveSections(str_list_t* list, const char* prefix)
{
	str_list_t sections = iniGetSectionList(*list, prefix);
	const char* section;

	while((section = strListPop(&sections)) != NULL)
		if(!iniRemoveSection(list, section))
			return(FALSE);

	strListFree(&sections);

	return(TRUE);
}

BOOL DLLCALL iniRenameSection(str_list_t* list, const char* section, const char* newname)
{
	char	str[INI_MAX_LINE_LEN];
	size_t	i;

	if(section==ROOT_SECTION)
		return(FALSE);

	if(stricmp(section, newname)!=0) {
		i=find_section_index(*list,newname);
		if((*list)[i]!=NULL)	/* duplicate */
			return(FALSE);
	}

	i=find_section_index(*list,section);
	if((*list)[i]==NULL)	/* not found */
		return(FALSE);

	SAFEPRINTF(str,"[%s]",newname);
	return(strListReplace(*list, i, str)!=NULL);
}

static size_t ini_add_section(str_list_t* list, const char* section
					,ini_style_t* style, size_t index)
{
	char	str[INI_MAX_LINE_LEN];

	if(section==ROOT_SECTION)
		return(0);

	if((*list)[index]!=NULL)
		return(index);

	if(style==NULL)
		style=&default_style;
	if(index > 0 && style->section_separator!=NULL)
		strListAppend(list, style->section_separator, index++);
	SAFEPRINTF(str,"[%s]",section);
	strListAppend(list, str, index);

	return(index);
}

size_t DLLCALL iniAddSection(str_list_t* list, const char* section, ini_style_t* style)
{
	if(section==ROOT_SECTION)
		return(0);

	return ini_add_section(list,section,style,find_section_index(*list, section));
}

size_t DLLCALL iniAppendSection(str_list_t* list, const char* section, ini_style_t* style)
{
	if(section==ROOT_SECTION)
		return(0);

	return ini_add_section(list,section,style,strListCount(*list));
}

static BOOL str_contains_ctrl_char(const char* str)
{
	while(*str) {
		if(*(unsigned char*)str < ' ')
			return TRUE;
		str++;
	}
	return FALSE;
}

static char* ini_set_string(str_list_t* list, const char* section, const char* key, const char* value, BOOL literal
				 ,ini_style_t* style)
{
	char	str[INI_MAX_LINE_LEN];
	char	litstr[INI_MAX_VALUE_LEN];
	char	curval[INI_MAX_VALUE_LEN];
	const char*	value_separator;
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
	if(style->literal_separator==NULL)
		style->literal_separator=":";
	if(value==NULL)
		value="";
	if(literal) {
		char cstr[INI_MAX_VALUE_LEN];
		SAFEPRINTF(litstr, "\"%s\"", c_escape_str(value, cstr, sizeof(cstr)-1, /* ctrl_only: */FALSE));
		value = litstr;
		value_separator = style->literal_separator;
	} else
		value_separator = style->value_separator;
	safe_snprintf(str, sizeof(str), "%s%-*s%s%s"
		,style->key_prefix, style->key_len, key, value_separator, value);
	i=get_value(*list, section, key, curval, NULL);
	if((*list)[i]==NULL || *(*list)[i]==INI_OPEN_SECTION_CHAR) {
        while(i && *(*list)[i-1]==0) i--;   /* Insert before blank lines, not after */
		return strListInsert(list, str, i);
    }

	if(strcmp(curval,value)==0)
		return((*list)[i]);	/* no change */

	return strListReplace(*list, i, str);
}
char* DLLCALL iniSetString(str_list_t* list, const char* section, const char* key, const char* value
				 ,ini_style_t* style)
{
	BOOL literal = value != NULL && (str_contains_ctrl_char(value) || *value==' ' || *lastchar(value)==' ');
		
	return ini_set_string(list, section, key, value, literal, style);
}

char* DLLCALL iniSetStringLiteral(str_list_t* list, const char* section, const char* key, const char* value
				 ,ini_style_t* style)
{
	return ini_set_string(list, section, key, value, /* literal: */TRUE, style);
}

char* DLLCALL iniSetInteger(str_list_t* list, const char* section, const char* key, long value
					,ini_style_t* style)
{
	char	str[INI_MAX_VALUE_LEN];

	SAFEPRINTF(str,"%ld",value);
	return iniSetString(list, section, key, str, style);
}

char* DLLCALL iniSetShortInt(str_list_t* list, const char* section, const char* key, ushort value
					,ini_style_t* style)
{
	char	str[INI_MAX_VALUE_LEN];

	SAFEPRINTF(str,"%hu",value);
	return iniSetString(list, section, key, str, style);
}

char* DLLCALL iniSetLongInt(str_list_t* list, const char* section, const char* key, ulong value
					,ini_style_t* style)
{
	char	str[INI_MAX_VALUE_LEN];

	SAFEPRINTF(str,"%lu",value);
	return iniSetString(list, section, key, str, style);
}

char* DLLCALL iniSetHexInt(str_list_t* list, const char* section, const char* key, ulong value
					,ini_style_t* style)
{
	char	str[INI_MAX_VALUE_LEN];

	SAFEPRINTF(str,"0x%lx",value);
	return iniSetString(list, section, key, str, style);
}

char* DLLCALL iniSetFloat(str_list_t* list, const char* section, const char* key, double value
					,ini_style_t* style)
{
	char	str[INI_MAX_VALUE_LEN];

	SAFEPRINTF(str,"%g",value);
	return iniSetString(list, section, key, str, style);
}

char* DLLCALL iniSetBytes(str_list_t* list, const char* section, const char* key, ulong unit
					,int64_t value, ini_style_t* style)
{
	char	str[INI_MAX_VALUE_LEN];

	if(value==0)
		SAFECOPY(str,"0");
	else
		switch(unit) {
			case 1024*1024*1024:
				SAFEPRINTF(str,"%"PRIi64"G",value);
				break;
			case 1024*1024:
				SAFEPRINTF(str,"%"PRIi64"M",value);
				break;
			case 1024:
				SAFEPRINTF(str,"%"PRIi64"K",value);
				break;
			default:
				if(unit<1)
					unit=1;
				byte_count_to_str(value*unit, str, sizeof(str));
		}

	return iniSetString(list, section, key, str, style);
}

char* DLLCALL iniSetDuration(str_list_t* list, const char* section, const char* key
					,double value, ini_style_t* style)
{
	char	str[INI_MAX_VALUE_LEN];

	return iniSetString(list, section, key, duration_to_str(value, str, sizeof(str)), style);
}


#if !defined(NO_SOCKET_SUPPORT)
char* DLLCALL iniSetIpAddress(str_list_t* list, const char* section, const char* key, ulong value
					,ini_style_t* style)
{
	struct in_addr in_addr;
	in_addr.s_addr=htonl(value);
	return iniSetString(list, section, key, inet_ntoa(in_addr), style);
}

char* DLLCALL iniSetIp6Address(str_list_t* list, const char* section, const char* key, struct in6_addr value
					,ini_style_t* style)
{
	char				addrstr[INET6_ADDRSTRLEN];
	union xp_sockaddr	addr = {{0}};

	addr.in6.sin6_addr = value;
	addr.in6.sin6_family = AF_INET6;
	inet_addrtop(&addr, addrstr, sizeof(addrstr));
	return iniSetString(list, section, key, addrstr, style);
}
#endif

char* DLLCALL iniSetBool(str_list_t* list, const char* section, const char* key, BOOL value
					,ini_style_t* style)
{
	return iniSetString(list, section, key, value ? "true":"false", style);
}

char* DLLCALL iniSetDateTime(str_list_t* list, const char* section, const char* key
					 ,BOOL include_time, time_t value, ini_style_t* style)
{
	char	str[INI_MAX_VALUE_LEN];
	char	tstr[32];
	char*	p;

	if(value==0)
		SAFECOPY(str,"Never");
	else if((p=ctime_r(&value,tstr))==NULL)
		SAFEPRINTF(str,"0x%lx",value);
	else if(!include_time)	/* reformat into "Mon DD YYYY" */
		safe_snprintf(str,sizeof(str),"%.3s %.2s %.4s"		,p+4,p+8,p+20);
	else					/* reformat into "Mon DD YYYY HH:MM:SS" */
		safe_snprintf(str,sizeof(str),"%.3s %.2s %.4s %.8s"	,p+4,p+8,p+20,p+11);

	return iniSetString(list, section, key, str, style);
}

char* DLLCALL iniSetEnum(str_list_t* list, const char* section, const char* key, str_list_t names, unsigned value
					,ini_style_t* style)
{
	if(value < strListCount(names))
		return iniSetString(list, section, key, names[value], style);

	return iniSetLongInt(list, section, key, value, style);
}

char* DLLCALL iniSetEnumList(str_list_t* list, const char* section, const char* key
					,const char* sep, str_list_t names, unsigned* val_list, unsigned count, ini_style_t* style)
{
	char	value[INI_MAX_VALUE_LEN];
	size_t	i;
	size_t	name_count;

	value[0]=0;

	if(sep==NULL)
		sep=",";

	if(val_list!=NULL) {
		name_count = strListCount(names);
		for(i=0; i < count; i++) {
			if(value[0])
				strcat(value,sep);
			if(val_list[i] < name_count)
				strcat(value, names[val_list[i]]);
			else
				sprintf(value + strlen(value), "%u", val_list[i]);
		}
	}

	return iniSetString(list, section, key, value, style);
}

char* DLLCALL iniSetNamedInt(str_list_t* list, const char* section, const char* key, named_long_t* names
					 ,long value, ini_style_t* style)
{
	size_t	i;

	for(i=0;names[i].name!=NULL;i++)
		if(names[i].value==value)
			return iniSetString(list, section, key, names[i].name, style);

	return iniSetInteger(list, section, key, value, style);
}

char* DLLCALL iniSetNamedHexInt(str_list_t* list, const char* section, const char* key, named_ulong_t* names
					 ,ulong value, ini_style_t* style)
{
	size_t	i;

	for(i=0;names[i].name!=NULL;i++)
		if(names[i].value==value)
			return iniSetString(list, section, key, names[i].name, style);

	return iniSetHexInt(list, section, key, value, style);
}

char* DLLCALL iniSetNamedLongInt(str_list_t* list, const char* section, const char* key, named_ulong_t* names
					 ,ulong value, ini_style_t* style)
{
	size_t	i;

	for(i=0;names[i].name!=NULL;i++)
		if(names[i].value==value)
			return iniSetString(list, section, key, names[i].name, style);

	return iniSetLongInt(list, section, key, value, style);
}

char* DLLCALL iniSetNamedFloat(str_list_t* list, const char* section, const char* key, named_double_t* names
					 ,double value, ini_style_t* style)
{
	size_t	i;

	for(i=0;names[i].name!=NULL;i++)
		if(names[i].value==value)
			return iniSetString(list, section, key, names[i].name, style);

	return iniSetFloat(list, section, key, value, style);
}

char* DLLCALL iniSetBitField(str_list_t* list, const char* section, const char* key
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

char* DLLCALL iniSetStringList(str_list_t* list, const char* section, const char* key
					,const char* sep, str_list_t val_list, ini_style_t* style)
{
	char	value[INI_MAX_VALUE_LEN];

	if(sep==NULL)
		sep=",";

	return iniSetString(list, section, key, strListCombine(val_list, value, sizeof(value), sep), style);
}

static char* default_value(const char* deflt, char* value)
{
	if(deflt!=NULL && deflt!=value && value!=NULL)
		sprintf(value,"%.*s",INI_MAX_VALUE_LEN-1,deflt);

	return((char*)deflt);
}

char* DLLCALL iniReadString(FILE* fp, const char* section, const char* key, const char* deflt, char* value)
{
	if(read_value(fp,section,key,value)==NULL || *value==0 /* blank */)
		return default_value(deflt,value);

	return(value);
}

char* DLLCALL iniGetString(str_list_t list, const char* section, const char* key, const char* deflt, char* value)
{
	char*	vp=NULL;

	get_value(list, section, key, value, &vp);

	if(vp==NULL || *vp==0 /* blank value or missing key */)
		return default_value(deflt,value);

	if(value != NULL)	/* return the modified (trimmed) value */
		return value;

	return(vp);
}

char* DLLCALL iniPopKey(str_list_t* list, const char* section, const char* key, char* value)
{
	size_t i;

	if(list==NULL || *list==NULL)
		return NULL;

	i=get_value(*list, section, key, value, NULL);

	if((*list)[i]==NULL)
		return NULL;

	strListDelete(list,i);

	return(value);
}

char* DLLCALL iniReadExistingString(FILE* fp, const char* section, const char* key, const char* deflt, char* value)
{
	if(read_value(fp,section,key,value)==NULL)
		return(NULL);

	if(*value==0 /* blank */)
		return default_value(deflt,value);

	return(value);
}

char* DLLCALL iniGetExistingString(str_list_t list, const char* section, const char* key, const char* deflt, char* value)
{
	if(!iniKeyExists(list, section, key))
		return(NULL);

	return iniGetString(list, section, key, deflt, value);
}

static str_list_t splitList(char* list, const char* sep)
{
	char*		token;
	char*		tmp;
	ulong		items=0;
	str_list_t	lp;

	if((lp=strListInit())==NULL)
		return(NULL);

	if(sep==NULL)
		sep=",";

	token=strtok_r(list,sep,&tmp);
	while(token!=NULL) {
		SKIP_WHITESPACE(token);
		truncsp(token);
		if(strListAppend(&lp,token,items++)==NULL)
			break;
		token=strtok_r(NULL,sep,&tmp);
	}

	return(lp);
}

str_list_t DLLCALL iniReadStringList(FILE* fp, const char* section, const char* key
						 ,const char* sep, const char* deflt)
{
	char*	value;
	char	buf[INI_MAX_VALUE_LEN];
	char	list[INI_MAX_VALUE_LEN];

	if((value=read_value(fp,section,key,buf))==NULL || *value==0 /* blank */)
		value=(char*)deflt;

	if(value==NULL)
		return(NULL);

	SAFECOPY(list,value);

	return(splitList(list,sep));
}

str_list_t DLLCALL iniGetStringList(str_list_t list, const char* section, const char* key
						 ,const char* sep, const char* deflt)
{
	char	value[INI_MAX_VALUE_LEN];

	get_value(list, section, key, value, NULL);

	if(*value==0 /* blank value or missing key */) {
		if(deflt==NULL)
			return(NULL);
		SAFECOPY(value,deflt);
	}

	return(splitList(value,sep));
}

void* DLLCALL iniFreeStringList(str_list_t list)
{
	strListFree(&list);
	return(list);
}

void* DLLCALL iniFreeNamedStringList(named_string_t** list)
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

str_list_t DLLCALL iniReadSectionList(FILE* fp, const char* prefix)
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
		if(is_eof(str))
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

str_list_t DLLCALL iniGetSectionList(str_list_t list, const char* prefix)
{
	char*	p;
	char	str[INI_MAX_LINE_LEN];
	ulong	i,items=0;
	str_list_t	lp;

	if((lp=strListInit())==NULL)
		return(NULL);

	if(list==NULL)
		return(lp);

	for(i=0; list[i]!=NULL; i++) {
		SAFECOPY(str,list[i]);
		if(is_eof(str))
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

size_t DLLCALL iniGetSectionCount(str_list_t list, const char* prefix)
{
	char*	p;
	char	str[INI_MAX_LINE_LEN];
	size_t	i,items=0;

	if(list==NULL)
		return(0);

	for(i=0; list[i]!=NULL; i++) {
		SAFECOPY(str,list[i]);
		if(is_eof(str))
			break;
		if((p=section_name(str))==NULL)
			continue;
		if(prefix!=NULL)
			if(strnicmp(p,prefix,strlen(prefix))!=0)
				continue;
		items++;
	}

	return(items);
}

size_t DLLCALL iniReadSectionCount(FILE* fp, const char* prefix)
{
	char*	p;
	char	str[INI_MAX_LINE_LEN];
	ulong	items=0;

	if(fp==NULL)
		return(0);

	rewind(fp);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		if(is_eof(str))
			break;
		if((p=section_name(str))==NULL)
			continue;
		if(prefix!=NULL)
			if(strnicmp(p,prefix,strlen(prefix))!=0)
				continue;
		items++;
	}

	return(items);
}


str_list_t DLLCALL iniReadKeyList(FILE* fp, const char* section)
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

	if(!seek_section(fp,section))
		return(lp);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		if(is_eof(str))
			break;
		if((p=key_name(str,&vp))==NULL)
			continue;
		if(p==INI_NEW_SECTION)
			break;
		if(strListAppend(&lp,p,items++)==NULL)
			break;
	}

	return(lp);
}

str_list_t DLLCALL iniGetKeyList(str_list_t list, const char* section)
{
	char*	p;
	char*	vp;
	char	str[INI_MAX_LINE_LEN];
	ulong	i,items=0;
	str_list_t	lp;

	if((lp=strListInit())==NULL)
		return(NULL);

	if(list==NULL)
		return(lp);

	for(i=find_section(list,section);list[i]!=NULL;i++) {
		SAFECOPY(str,list[i]);
		if(is_eof(str))
			break;
		if((p=key_name(str,&vp))==NULL)
			continue;
		if(p==INI_NEW_SECTION)
			break;
		if(strListAppend(&lp,p,items++)==NULL)
			break;
	}

	return(lp);
}


named_string_t** DLLCALL
iniReadNamedStringList(FILE* fp, const char* section)
{
	char*	name;
	char*	value;
	char	str[INI_MAX_LINE_LEN];
	ulong	items=0;
	named_string_t** lp;
	named_string_t** np;

	if(fp==NULL)
		return(NULL);

	rewind(fp);

	if(!seek_section(fp,section))
		return(NULL);

	/* New behavior, if section exists but is empty, return single element array (terminator only) */
	if((lp=(named_string_t**)malloc(sizeof(named_string_t*)))==NULL)
		return(NULL);

	while(!feof(fp)) {
		if(fgets(str,sizeof(str),fp)==NULL)
			break;
		if(is_eof(str))
			break;
		if((name=key_name(str,&value))==NULL)
			continue;
		if(name==INI_NEW_SECTION)
			break;
		if((np=(named_string_t**)realloc(lp,sizeof(named_string_t*)*(items+2)))==NULL)
			break;
		lp=np;
		if((lp[items]=(named_string_t*)malloc(sizeof(named_string_t)))==NULL)
			break;
		if((lp[items]->name=strdup(name))==NULL)
			break;
		if((lp[items]->value=strdup(value))==NULL)
			break;
		items++;
	}

	lp[items]=NULL;	/* terminate list */

	return(lp);
}

named_string_t** DLLCALL
iniGetNamedStringList(str_list_t list, const char* section)
{
	char*	name;
	char*	value;
	char	str[INI_MAX_LINE_LEN];
	ulong	i,items=0;
	named_string_t** lp;
	named_string_t** np;

	if(list==NULL)
		return(NULL);

	i=find_section(list,section);
	if(list[i]==NULL)
		return(NULL);

	/* New behavior, if section exists but is empty, return single element array (terminator only) */
	if((lp=(named_string_t**)malloc(sizeof(named_string_t*)))==NULL)
		return(NULL);

	for(;list[i]!=NULL;i++) {
		SAFECOPY(str,list[i]);
		if(is_eof(str))
			break;
		if((name=key_name(str,&value))==NULL)
			continue;
		if(name==INI_NEW_SECTION)
			break;
		if((np=(named_string_t**)realloc(lp,sizeof(named_string_t*)*(items+2)))==NULL)
			break;
		lp=np;
		if((lp[items]=(named_string_t*)malloc(sizeof(named_string_t)))==NULL)
			break;
		if((lp[items]->name=strdup(name))==NULL)
			break;
		if((lp[items]->value=strdup(value))==NULL)
			break;
		items++;
	}

	lp[items]=NULL;	/* terminate list */

	return(lp);
}


/* These functions read a single key of the specified type */

static BOOL isTrue(const char* value)
{
	char*	str;
	char*	p;
	BOOL	is_true;
	
	if(!isalpha(*value))
		return FALSE;

	if((str=strdup(value)) == NULL)
		return FALSE;

	/* Truncate value at first space, tab or semicolon for purposes of checking for special boolean words. */
	/* This allows comments or white-space to immediately follow a special boolean word: "True", "Yes", or "On" */
	p=str;
	FIND_CHARSET(p, "; \t");
	*p=0;
	
	is_true = (stricmp(str,"TRUE")==0 || stricmp(str,"YES")==0 || stricmp(str,"ON")==0);
	free(str);
	return is_true;
}

static long parseInteger(const char* value)
{
	if(isTrue(value))
		return(TRUE);

	return(strtol(value,NULL,0));
}

static ulong parseLongInteger(const char* value)
{
	if(isTrue(value))
		return(TRUE);

	return(strtoul(value,NULL,0));
}

static BOOL parseBool(const char* value)
{
	return(INT_TO_BOOL(parseInteger(value)));
}

long DLLCALL iniReadInteger(FILE* fp, const char* section, const char* key, long deflt)
{
	char*	value;
	char	buf[INI_MAX_VALUE_LEN];

	if((value=read_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(parseInteger(value));
}

long DLLCALL iniGetInteger(str_list_t list, const char* section, const char* key, long deflt)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0)	/* blank value or missing key */
		return(deflt);

	return(parseInteger(vp));
}

ushort DLLCALL iniReadShortInt(FILE* fp, const char* section, const char* key, ushort deflt)
{
	return((ushort)iniReadInteger(fp, section, key, deflt));
}

ushort DLLCALL iniGetShortInt(str_list_t list, const char* section, const char* key, ushort deflt)
{
	return((ushort)iniGetInteger(list, section, key, deflt));
}

ulong DLLCALL iniReadLongInt(FILE* fp, const char* section, const char* key, ulong deflt)
{
	char*	value;
	char	buf[INI_MAX_VALUE_LEN];

	if((value=read_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(parseLongInteger(value));
}

ulong DLLCALL iniGetLongInt(str_list_t list, const char* section, const char* key, ulong deflt)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0)	/* blank value or missing key */
		return(deflt);

	return(parseLongInteger(vp));
}

int64_t DLLCALL iniReadBytes(FILE* fp, const char* section, const char* key, ulong unit, int64_t deflt)
{
	char*	value;
	char	buf[INI_MAX_VALUE_LEN];

	if((value=read_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(parse_byte_count(value,unit));
}

int64_t DLLCALL iniGetBytes(str_list_t list, const char* section, const char* key, ulong unit, int64_t deflt)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0)	/* blank value or missing key */
		return(deflt);

	return(parse_byte_count(vp,unit));
}

double DLLCALL iniReadDuration(FILE* fp, const char* section, const char* key, double deflt)
{
	char*	value;
	char	buf[INI_MAX_VALUE_LEN];

	if((value=read_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(parse_duration(value));
}

double DLLCALL iniGetDuration(str_list_t list, const char* section, const char* key, double deflt)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0)	/* blank value or missing key */
		return(deflt);

	return(parse_duration(vp));
}

#if !defined(NO_SOCKET_SUPPORT)

int DLLCALL iniGetSocketOptions(str_list_t list, const char* section, SOCKET sock
						 ,char* error, size_t errlen)
{
	int			i;
	int			result;
	char*		name;
	BYTE*		vp;
	socklen_t	len;
	int			option;
	int			level;
	int			value;
	int			type=0;	// Assignment is to silence Valgrind.
	LINGER		linger;
	socket_option_t* socket_options=getSocketOptionList();
#ifdef IPPROTO_IPV6
	union xp_sockaddr	addr;
#endif

	len=sizeof(type);
	if((result=getsockopt(sock, SOL_SOCKET, SO_TYPE, (char*)&type, &len)) != 0) {
		safe_snprintf(error,errlen,"%d getting socket type", ERROR_VALUE);
		return(result);
	}
#ifdef IPPROTO_IPV6
	len=sizeof(addr);
	if((result=getsockname(sock, &addr.addr, &len)) != 0) {
		safe_snprintf(error,errlen,"%d getting socket name", ERROR_VALUE);
		return(result);
	}
#endif
	for(i=0;socket_options[i].name!=NULL;i++) {
		if(socket_options[i].type != 0 
				&& socket_options[i].type != type)
			continue;
#ifdef IPPROTO_IPV6
		if(addr.addr.sa_family != AF_INET6 && socket_options[i].level == IPPROTO_IPV6)
			continue;
#endif
		name = socket_options[i].name;
		if(!iniValueExists(list, section, name))
			continue;
		value=iniGetInteger(list, section, name, 0);

		vp=(BYTE*)&value;
		len=sizeof(value);

		level	= socket_options[i].level;
		option	= socket_options[i].value;

		if(option == SO_LINGER) {
			if(value) {
				linger.l_onoff = TRUE;
				linger.l_linger = value;
			} else {
				ZERO_VAR(linger);
			}
			vp=(BYTE*)&linger;
			len=sizeof(linger);
		}

		if((result=setsockopt(sock,level,option,(const char *)vp,len)) != 0) {
			safe_snprintf(error,errlen,"%d setting socket option (%s, %d) to %d"
				,ERROR_VALUE, name, option, value);
			return(result);
		}
	}

	return(0);
}

static ulong parseIpAddress(const char* value)
{
	if(strchr(value,'.')==NULL)
		return(strtol(value,NULL,0));

	return(ntohl(inet_addr(value)));
}

static struct in6_addr parseIp6Address(const char* value)
{
	struct addrinfo hints = {0};
	struct addrinfo *res, *cur;
	struct in6_addr ret = {{{0}}};

	hints.ai_flags = AI_NUMERICHOST|AI_PASSIVE;
	if(getaddrinfo(value, NULL, &hints, &res))
		return ret;

	for(cur = res; cur; cur++) {
		if(cur->ai_addr->sa_family == AF_INET6)
			break;
	}
	if(!cur) {
		freeaddrinfo(res);
		return ret;
	}
	memcpy(&ret, &((struct sockaddr_in6 *)(cur->ai_addr))->sin6_addr, sizeof(ret));
	freeaddrinfo(res);
	return ret;
}

ulong DLLCALL iniReadIpAddress(FILE* fp, const char* section, const char* key, ulong deflt)
{
	char	buf[INI_MAX_VALUE_LEN];
	char*	value;

	if((value=read_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(parseIpAddress(value));
}

struct in6_addr DLLCALL iniReadIp6Address(FILE* fp, const char* section, const char* key, struct in6_addr deflt)
{
	char	buf[INI_MAX_VALUE_LEN];
	char*	value;

	if((value=read_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(parseIp6Address(value));
}

ulong DLLCALL iniGetIpAddress(str_list_t list, const char* section, const char* key, ulong deflt)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0)		/* blank value or missing key */
		return(deflt);

	return(parseIpAddress(vp));
}

struct in6_addr DLLCALL iniGetIp6Address(str_list_t list, const char* section, const char* key, struct in6_addr deflt)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0)		/* blank value or missing key */
		return(deflt);

	return(parseIp6Address(vp));
}

#endif	/* !NO_SOCKET_SUPPORT */

char* DLLCALL iniFileName(char* dest, size_t maxlen, const char* indir, const char* infname)
{
	char	dir[MAX_PATH+1];
	char	fname[MAX_PATH+1];
	char	ext[MAX_PATH+1];
	char*	p;

	SAFECOPY(dir,indir);
	backslash(dir);
	SAFECOPY(fname,infname);
	ext[0]=0;
	if((p=getfext(fname))!=NULL) {
		SAFECOPY(ext,p);
		*p=0;
	}

#if !defined(NO_SOCKET_SUPPORT)
	{
		char hostname[128];

		if(gethostname(hostname,sizeof(hostname))==0) {
			safe_snprintf(dest,maxlen,"%s%s.%s%s",dir,fname,hostname,ext);
			if(fexistcase(dest))		/* path/file.host.domain.ini */
				return(dest);
			if((p=strchr(hostname,'.'))!=NULL) {
				*p=0;
				safe_snprintf(dest,maxlen,"%s%s.%s%s",dir,fname,hostname,ext);
				if(fexistcase(dest))	/* path/file.host.ini */
					return(dest);
			}
		}
	}
#endif

	safe_snprintf(dest,maxlen,"%s%s.%s%s",dir,fname,PLATFORM_DESC,ext);
	if(fexistcase(dest))	/* path/file.platform.ini */
		return(dest);

	safe_snprintf(dest,maxlen,"%s%s%s",dir,fname,ext);
	fexistcase(dest);	/* path/file.ini */
	return(dest);
}

double DLLCALL iniReadFloat(FILE* fp, const char* section, const char* key, double deflt)
{
	char	buf[INI_MAX_VALUE_LEN];
	char*	value;

	if((value=read_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(atof(value));
}

double DLLCALL iniGetFloat(str_list_t list, const char* section, const char* key, double deflt)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0)		/* blank value or missing key */
		return(deflt);

	return(atof(vp));
}

BOOL DLLCALL iniReadBool(FILE* fp, const char* section, const char* key, BOOL deflt)
{
	char	buf[INI_MAX_VALUE_LEN];
	char*	value;

	if((value=read_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(parseBool(value));
}

BOOL DLLCALL iniGetBool(str_list_t list, const char* section, const char* key, BOOL deflt)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0)		/* blank value or missing key */
		return(deflt);

	return(parseBool(vp));
}

static BOOL validDate(struct tm* tm)
{
	return(tm->tm_mon && tm->tm_mon<=12
		&& tm->tm_mday && tm->tm_mday<=31);
}

static time_t fixedDateTime(struct tm* tm, const char* tstr, char pm)
{
	if(tm->tm_year<70)
		tm->tm_year+=100;	/* 05 == 2005 (not 1905) and 70 == 1970 (not 2070) */
	else if(tm->tm_year>1900)
		tm->tm_year-=1900;
	if(tm->tm_mon)
		tm->tm_mon--;		/* zero-based month field */

	/* hh:mm:ss [p] */
	sscanf(tstr,"%u:%u:%u",&tm->tm_hour,&tm->tm_min,&tm->tm_sec);
	if(tm->tm_hour < 12 && (toupper(pm)=='P' || strchr(tstr,'p') || strchr(tstr,'P')))
		tm->tm_hour += 12;	/* pm, correct for 24 hour clock */

	tm->tm_isdst=-1;	/* auto-detect */

	return(mktime(tm));
}

static int getMonth(const char* month)
{
	char *mon[]={"Jan","Feb","Mar","Apr","May","Jun"
            ,"Jul","Aug","Sep","Oct","Nov","Dec",NULL};
	int i;

	for(i=0;mon[i]!=NULL;i++)
		if(strnicmp(month,mon[i],3)==0)
			return(i+1);

	return(atoi(month));
}

static time_t parseDateTime(const char* value)
{
	char	month[INI_MAX_VALUE_LEN];
	char	tstr[INI_MAX_VALUE_LEN];
	char	pm=0;
	time_t	t;
	struct tm tm;
	struct tm curr_tm;
	isoDate_t	isoDate;
	isoTime_t	isoTime;

	ZERO_VAR(tm);
	tstr[0]=0;

	/* Use current month and year as default */
	t=time(NULL);
	if(localtime_r(&t,&curr_tm)!=NULL) {
		tm.tm_mon=curr_tm.tm_mon+1;	/* convert to one-based (reversed later) */
		tm.tm_year=curr_tm.tm_year;
	}

	/* CCYYMMDDTHHMMSS <--- ISO-8601 date and time format */
	if(sscanf(value,"%uT%u"
		,&isoDate,&isoTime)>=2)
		return(isoDateTime_to_time(isoDate,isoTime));

	/* DD.MM.[CC]YY [time] [p] <-- Euro/Canadian numeric date format */
	if(sscanf(value,"%u.%u.%u %s %c"
		,&tm.tm_mday,&tm.tm_mon,&tm.tm_year,tstr,&pm)>=2
		&& validDate(&tm))
		return(fixedDateTime(&tm,tstr,pm));

	/* MM/DD/[CC]YY [time] [p] <-- American numeric date format */
	if(sscanf(value,"%u%*c %u%*c %u %s %c"
		,&tm.tm_mon,&tm.tm_mday,&tm.tm_year,tstr,&pm)>=2
		&& validDate(&tm))
		return(fixedDateTime(&tm,tstr,pm));

	/* DD[-]Mon [CC]YY [time] [p] <-- Perversion of RFC822 date format */
	if(sscanf(value,"%u%*c %s %u %s %c"
		,&tm.tm_mday,month,&tm.tm_year,tstr,&pm)>=2
		&& (tm.tm_mon=getMonth(month))!=0
		&& validDate(&tm))
		return(fixedDateTime(&tm,tstr,pm));

	/* Wday, DD Mon YYYY [time] <-- IETF standard (RFC2822) date format */
	if(sscanf(value,"%*s %u %s %u %s"
		,&tm.tm_mday,month,&tm.tm_year,tstr)>=2
		&& (tm.tm_mon=getMonth(month))!=0
		&& validDate(&tm))
		return(fixedDateTime(&tm,tstr,0));

	/* Mon DD[,] [CC]YY [time] [p] <-- Preferred date format */
	if(sscanf(value,"%s %u%*c %u %s %c"
		,month,&tm.tm_mday,&tm.tm_year,tstr,&pm)>=2
		&& (tm.tm_mon=getMonth(month))!=0
		&& validDate(&tm))
		return(fixedDateTime(&tm,tstr,pm));

	/* Wday Mon DD YYYY [time] <-- JavaScript (SpiderMonkey) Date.toString() format */
	if(sscanf(value,"%*s %s %u %u %s"
		,month,&tm.tm_mday,&tm.tm_year,tstr)>=2
		&& (tm.tm_mon=getMonth(month))!=0
		&& validDate(&tm))
		return(fixedDateTime(&tm,tstr,0));

	/* Wday Mon DD [time] YYYY <-- ctime() format */
	if(sscanf(value,"%*s %s %u %s %u"
		,month,&tm.tm_mday,tstr,&tm.tm_year)>=2
		&& (tm.tm_mon=getMonth(month))!=0
		&& validDate(&tm))
		return(fixedDateTime(&tm,tstr,0));

	if((t=xpDateTime_to_time(isoDateTimeStr_parse(value))) != INVALID_TIME)
		return t;

	return(strtoul(value,NULL,0));
}

time_t DLLCALL iniReadDateTime(FILE* fp, const char* section, const char* key, time_t deflt)
{
	char	buf[INI_MAX_VALUE_LEN];
	char*	value;

	if((value=read_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(parseDateTime(value));
}

time_t DLLCALL iniGetDateTime(str_list_t list, const char* section, const char* key, time_t deflt)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0)		/* blank value or missing key */
		return(deflt);

	return(parseDateTime(vp));
}

static unsigned parseEnum(const char* value, str_list_t names)
{
	unsigned i,count;
	char val[INI_MAX_VALUE_LEN];
	char* p=val;

	/* Strip trailing words (enums must be a single word with no white-space) */
	/* to support comments following enum values */
	SAFECOPY(val,value);
	FIND_WHITESPACE(p);
	*p=0;

	if((count=strListCount(names)) == 0)
		return 0;

	/* Look for exact matches first */
	for(i=0; i<count; i++)
		if(stricmp(names[i],val)==0)
			return(i);

	/* Look for partial matches second */
	for(i=0; i<count; i++)
		if(strnicmp(names[i],val,strlen(val))==0)
			return(i);

    i=strtoul(val,NULL,0);
	if(i>=count)
		i=count-1;
	return i;
}

unsigned* DLLCALL parseEnumList(const char* values, const char* sep, str_list_t names, unsigned* count)
{
	char*		vals;
	str_list_t	list;
	unsigned*	enum_list;
	size_t		i;

	*count=0;

	if(values==NULL)
		return NULL;

	if((vals=strdup(values)) == NULL)
		return NULL;

	list=splitList(vals, sep);

	free(vals);

	if((*count=strListCount(list)) < 1) {
		strListFree(&list);
		return NULL;
	}

	if((enum_list=(unsigned *)malloc((*count)*sizeof(unsigned)))!=NULL) {
		for(i=0;i<*count;i++)
			enum_list[i]=parseEnum(list[i], names);
	}

	strListFree(&list);

	return enum_list;
}

unsigned DLLCALL iniReadEnum(FILE* fp, const char* section, const char* key, str_list_t names, unsigned deflt)
{
	char	buf[INI_MAX_VALUE_LEN];
	char*	value;

	if((value=read_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(parseEnum(value,names));
}

unsigned* DLLCALL iniReadEnumList(FILE* fp, const char* section, const char* key
						 ,str_list_t names, unsigned* cp
						 ,const char* sep, const char* deflt)
{
	char*		value;
	char		buf[INI_MAX_VALUE_LEN];
	unsigned	count;

	if(cp==NULL)
		cp=&count;

	*cp=0;

	if((value=read_value(fp,section,key,buf))==NULL || *value==0 /* blank */)
		value=(char*)deflt;

	return(parseEnumList(value, sep, names, cp));
}

unsigned DLLCALL iniGetEnum(str_list_t list, const char* section, const char* key, str_list_t names, unsigned deflt)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0)		/* blank value or missing key */
		return(deflt);

	return(parseEnum(vp,names));
}

unsigned* DLLCALL iniGetEnumList(str_list_t list, const char* section, const char* key
						 ,str_list_t names, unsigned* cp, const char* sep, const char* deflt)
{
	char*		vp=NULL;
	unsigned	count;

	if(cp==NULL)
		cp=&count;

	*cp=0;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0 /* blank value or missing key */) {
		if(deflt==NULL)
			return(NULL);
		vp=(char*)deflt;
	}
	return(parseEnumList(vp, sep, names, cp));
}

static long parseNamedInt(const char* value, named_long_t* names)
{
	unsigned i;

	/* Look for exact matches first */
	for(i=0; names[i].name!=NULL; i++)
		if(stricmp(names[i].name,value)==0)
			return(names[i].value);

	/* Look for partial matches second */
	for(i=0; names[i].name!=NULL; i++)
		if(strnicmp(names[i].name,value,strlen(value))==0)
			return(names[i].value);

	return(parseInteger(value));
}

long DLLCALL iniReadNamedInt(FILE* fp, const char* section, const char* key
					 ,named_long_t* names, long deflt)
{
	char	buf[INI_MAX_VALUE_LEN];
	char*	value;

	if((value=read_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(parseNamedInt(value,names));
}

long DLLCALL iniGetNamedInt(str_list_t list, const char* section, const char* key
					,named_long_t* names, long deflt)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0)		/* blank value or missing key */
		return(deflt);

	return(parseNamedInt(vp,names));
}

static ulong parseNamedLongInt(const char* value, named_ulong_t* names)
{
	unsigned i;

	/* Look for exact matches first */
	for(i=0; names[i].name!=NULL; i++)
		if(stricmp(names[i].name,value)==0)
			return(names[i].value);

	/* Look for partial matches second */
	for(i=0; names[i].name!=NULL; i++)
		if(strnicmp(names[i].name,value,strlen(value))==0)
			return(names[i].value);

	return(parseLongInteger(value));
}

ulong DLLCALL iniReadNamedLongInt(FILE* fp, const char* section, const char* key
					 ,named_ulong_t* names, ulong deflt)
{
	char	buf[INI_MAX_VALUE_LEN];
	char*	value;

	if((value=read_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(parseNamedLongInt(value,names));
}

ulong DLLCALL iniGetNamedLongInt(str_list_t list, const char* section, const char* key
					,named_ulong_t* names, ulong deflt)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0)		/* blank value or missing key */
		return(deflt);

	return(parseNamedLongInt(vp,names));
}

static double parseNamedFloat(const char* value, named_double_t* names)
{
	unsigned i;

	/* Look for exact matches first */
	for(i=0; names[i].name!=NULL; i++)
		if(stricmp(names[i].name,value)==0)
			return(names[i].value);

	/* Look for partial matches second */
	for(i=0; names[i].name!=NULL; i++)
		if(strnicmp(names[i].name,value,strlen(value))==0)
			return(names[i].value);

	return(atof(value));
}

double DLLCALL iniReadNamedFloat(FILE* fp, const char* section, const char* key
					 ,named_double_t* names, double deflt)
{
	char	buf[INI_MAX_VALUE_LEN];
	char*	value;

	if((value=read_value(fp,section,key,buf))==NULL)
		return(deflt);

	if(*value==0)		/* blank value */
		return(deflt);

	return(parseNamedFloat(value,names));
}

double DLLCALL iniGetNamedFloat(str_list_t list, const char* section, const char* key
					,named_double_t* names, double deflt)
{
	char*	vp=NULL;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL || *vp==0)		/* blank value or missing key */
		return(deflt);

	return(parseNamedFloat(vp,names));
}

static ulong parseBitField(char* value, ini_bitdesc_t* bitdesc)
{
	int		i;
	char*	p;
	char*	tp;
	ulong	v=0;

	for(p=value;*p;) {
		tp=strchr(p,INI_BIT_SEP);
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

ulong DLLCALL iniReadBitField(FILE* fp, const char* section, const char* key,
						ini_bitdesc_t* bitdesc, ulong deflt)
{
	char*	value;
	char	buf[INI_MAX_VALUE_LEN];

	if((value=read_value(fp,section,key,buf))==NULL)	/* missing key */
		return(deflt);

	return(parseBitField(value,bitdesc));
}

ulong DLLCALL iniGetBitField(str_list_t list, const char* section, const char* key,
						ini_bitdesc_t* bitdesc, ulong deflt)
{
	char*	vp=NULL;;

	get_value(list, section, key, NULL, &vp);

	if(vp==NULL)		/* missing key */
		return(deflt);

	return(parseBitField(vp,bitdesc));
}

FILE* DLLCALL iniOpenFile(const char* fname, BOOL create)
{
	char* mode="r+";

	if(create && !fexist(fname))
		mode="w+";

	return(fopen(fname,mode));
}

BOOL DLLCALL iniCloseFile(FILE* fp)
{
	return(fclose(fp)==0);
}

str_list_t DLLCALL iniReadFile(FILE* fp)
{
	char		str[INI_MAX_LINE_LEN];
	char*		p;
	size_t		i;
	size_t		inc_len;
	size_t		inc_counter=0;
	str_list_t	list;
	FILE*		insert_fp=NULL;

	if(fp!=NULL)
		rewind(fp);

	list = strListReadFile(fp, NULL, INI_MAX_LINE_LEN);
	if(list==NULL)
		return(NULL);

	/* Look for !include directives */
	inc_len=strlen(INI_INCLUDE_DIRECTIVE);
	for(i=0; list[i]!=NULL; i++) {
		if(strnicmp(list[i],INI_INCLUDE_DIRECTIVE,inc_len)==0) {
			p=list[i]+inc_len;
			FIND_WHITESPACE(p);
			SKIP_WHITESPACE(p);
			truncsp(p);
			if(inc_counter >= INI_INCLUDE_MAX)
				SAFEPRINTF2(str, ";%s - MAXIMUM INCLUDES REACHED: %u", list[i], INI_INCLUDE_MAX);
			else if((insert_fp=fopen(p,"r"))==NULL)
				SAFEPRINTF2(str, ";%s - FAILURE: %s", list[i], STRERROR(errno));
			else
				SAFEPRINTF(str, ";%s", list[i]);
			strListReplace(list, i, str);
			if(insert_fp!=NULL) {
				strListInsertFile(insert_fp, &list, i+1, INI_MAX_LINE_LEN);
				fclose(insert_fp);
				insert_fp=NULL;
				inc_counter++;
			}
		}
	}

	/* truncate new-line chars off end of strings */
	for(i=0; list[i]!=NULL; i++)
		truncnl(list[i]);

	return(list);
}

BOOL DLLCALL iniWriteFile(FILE* fp, const str_list_t list)
{
	size_t		count;

	rewind(fp);

	if(chsize(fileno(fp),0)!=0)	/* truncate */
		return(FALSE);

	count = strListWriteFile(fp,list,"\n");

	return(count == strListCount(list));
}

#ifdef INI_FILE_TEST
void main(int argc, char** argv)
{
	int			i;
	size_t		l;
	char		str[128];
	FILE*		fp;
	str_list_t	list;

	for(i=1;i<argc;i++) {
		if((fp=iniOpenFile(argv[i],FALSE)) == NULL)
			continue;
		if((list=iniReadFile(fp)) != NULL) {
			printf("%s\n",iniGetString(list," test | bogus ","key","default",str));
			strListFree(&list);
		}
		fclose(fp);
	}
}
#endif
