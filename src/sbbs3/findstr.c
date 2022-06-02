/* Synchronet find string routines */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "genwrap.h"
#include "findstr.h"

/****************************************************************************/
/* Pattern matching string search of 'insearchof' in 'pattern'.				*/
/* pattern matching is case-insensitive										*/
/* patterns beginning with ';' are comments (never match)					*/
/* patterns beginning with '!' are reverse-matched (returns FALSE if match)	*/
/* patterns ending in '~' will match string anywhere (sub-string search)	*/
/* patterns ending in '^' will match left string fragment only				*/
/* patterns including '*' must match both left and right string fragments	*/
/* all other patterns are exact-match checking								*/
/****************************************************************************/
BOOL findstr_in_string(const char* search, const char* pattern)
{
	char	buf[256];
	char*	p;
	char*	last;
	const char*	splat;
	size_t	len;
	BOOL	found = FALSE;

	if(pattern == NULL || search == NULL)
		return FALSE;

	SAFECOPY(buf, pattern);
	p = buf;

	if(*p == ';')		/* comment */
		return FALSE;

	if(*p == '!')	{	/* reverse-match */
		found = TRUE;
		p++;
	}

	truncsp(p);
	len = strlen(p);
	if(len > 0) {
		last = p + len - 1;
		if(*last == '~') {
			*last = '\0';
			if(strcasestr(search, p) != NULL)
				found = !found; 
		}

		else if(*last == '^') {
			if(strnicmp(p, search, len - 1) == 0)
				found = !found; 
		}

		else if((splat = strchr(p, '*')) != NULL) {
			int left = splat - p;
			int right = len - (left + 1);
			int slen = strlen(search);
			if(slen < left + right)
				return found;
			if(strnicmp(search, p, left) == 0
				&& strnicmp(p + left + 1, search + (slen - right), right) == 0)
				found = !found;
		}

		else if(stricmp(p, search) == 0)
			found = !found; 
	} 
	return found;
}

static uint32_t encode_ipv4_address(unsigned int byte[])
{
	if(byte[0] > 0xff || byte[1] > 0xff || byte[2] > 0xff || byte[3] > 0xff)
		return 0;
	return (byte[0]<<24) | (byte[1]<<16) | (byte[2]<<8) | byte[3];
}

static uint32_t parse_ipv4_address(const char* str)
{
	unsigned int byte[4];

	if(sscanf(str, "%u.%u.%u.%u", &byte[0], &byte[1], &byte[2], &byte[3]) != 4)
		return 0;
	return encode_ipv4_address(byte);
}

static uint32_t parse_cidr(const char* p, unsigned* subnet)
{
	unsigned int byte[4];

	if(*p == '!')
		p++;

	*subnet = 0;
	if(sscanf(p, "%u.%u.%u.%u/%u", &byte[0], &byte[1], &byte[2], &byte[3], subnet) != 5 || *subnet > 32)
		return 0;
	return encode_ipv4_address(byte);
}

static BOOL is_cidr_match(const char *p, uint32_t ip_addr, uint32_t cidr, unsigned subnet)
{
	BOOL	match = FALSE;

	if(*p == '!')
		match = TRUE;

	if(((ip_addr ^ cidr) >> (32-subnet)) == 0)
		match = !match;

	return match;
}

/****************************************************************************/
/* Pattern matching string search of 'insearchof' in 'list'.				*/
/****************************************************************************/
BOOL findstr_in_list(const char* insearchof, str_list_t list)
{
	size_t	index;
	BOOL	found=FALSE;
	char*	p;
	uint32_t ip_addr, cidr;
	unsigned subnet;

	if(list==NULL || insearchof==NULL)
		return FALSE;
	ip_addr = parse_ipv4_address(insearchof);
	for(index=0; list[index]!=NULL; index++) {
		p=list[index];
//		SKIP_WHITESPACE(p);
		if(ip_addr != 0 && (cidr = parse_cidr(p, &subnet)) != 0)
			found = is_cidr_match(p, ip_addr, cidr, subnet);
		else
			found = findstr_in_string(insearchof,p);
		if(found != (*p=='!'))
			break;
	}
	return found;
}

/****************************************************************************/
/* Pattern matching string search of 'insearchof' in 'fname'.				*/
/****************************************************************************/
BOOL findstr(const char* insearchof, const char* fname)
{
	char		str[256];
	BOOL		found=FALSE;
	FILE*		fp;
	uint32_t	ip_addr, cidr;
	unsigned	subnet;

	if(insearchof==NULL || fname==NULL || *fname == '\0')
		return FALSE;

	if((fp=fopen(fname,"r"))==NULL)
		return FALSE; 

	ip_addr = parse_ipv4_address(insearchof);
	while(!feof(fp) && !ferror(fp) && !found) {
		if(!fgets(str,sizeof(str),fp))
			break;
		char* p = str;
		SKIP_WHITESPACE(p);
		c_unescape_str(p);
		if(ip_addr !=0 && (cidr = parse_cidr(p, &subnet)) != 0)
			found = is_cidr_match(p, ip_addr, cidr, subnet);
		else
			found = findstr_in_string(insearchof, p);
	}

	fclose(fp);
	return found;
}

static char* process_findstr_item(size_t index, char *str, void* cbdata)
{
	SKIP_WHITESPACE(str);
	return c_unescape_str(str);
}

/****************************************************************************/
str_list_t findstr_list(const char* fname)
{
	FILE*	fp;
	str_list_t	list;

	if((fp=fopen(fname,"r"))==NULL)
		return NULL;

	list=strListReadFile(fp, NULL, /* Max line length: */255);
	strListModifyEach(list, process_findstr_item, /* cbdata: */NULL);

	fclose(fp);

	return list;
}

