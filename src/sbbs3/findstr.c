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
#include "nopen.h"

/****************************************************************************/
/* Pattern matching string search of 'search' in 'pattern'.					*/
/* pattern matching is case-insensitive										*/
/* patterns beginning with ';' are comments (never match)					*/
/* patterns beginning with '!' are reverse-matched (returns false if match)	*/
/* patterns ending in '~' will match string anywhere (sub-string search)	*/
/* patterns ending in '^' will match left string fragment only				*/
/* patterns including '*' must match both left and right string fragments	*/
/* all other patterns are exact-match checking								*/
/****************************************************************************/
bool findstr_in_string(const char* search, const char* pattern)
{
	char	buf[FINDSTR_MAX_LINE_LEN + 1];
	char*	p = (char*)pattern;
	char*	last;
	const char*	splat;
	size_t	len;
	bool	found = false;

	if(pattern == NULL)
		return false;

	if(*p == ';')		/* comment */
		return false;

	if(*p == '!')	{	/* reverse-match */
		found = true;
		p++;
	}

	if(search == NULL)
		return found;

	SAFECOPY(buf, p);
	p = buf;

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

	if(str == NULL)
		return 0;
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

static bool is_cidr_match(const char *p, uint32_t ip_addr, uint32_t cidr, unsigned subnet)
{
	bool	match = false;

	if(*p == '!')
		match = true;

	if(((ip_addr ^ cidr) >> (32-subnet)) == 0)
		match = !match;

	return match;
}

static bool findstr_compare(const char* str, uint32_t ip_addr, const char* pattern, char* metadata)
{
	uint32_t cidr;
	unsigned subnet;
	char buf[FINDSTR_MAX_LINE_LEN + 1];
	char* p;

	SAFECOPY(buf, pattern);
	if((p = strchr(buf, '\t')) != NULL) {
		*p = '\0';
		p++;
		if(metadata != NULL)
			snprintf(metadata, FINDSTR_MAX_LINE_LEN, "%s", p);
	} else {
		if(metadata != NULL)
			*metadata = '\0';
	}
	if(ip_addr != 0 && (cidr = parse_cidr(pattern, &subnet)) != 0)
		return is_cidr_match(pattern, ip_addr, cidr, subnet);
	return findstr_in_string(str, buf);
}

/****************************************************************************/
/* Pattern matching string search of 'insearchof' in 'list'.				*/
/****************************************************************************/
bool findstr_in_list(const char* insearchof, str_list_t list, char* metadata)
{
	return find2strs_in_list(insearchof, NULL, list, metadata);
}

/****************************************************************************/
/* Pattern matching string search of 'str1' or 'str2' in 'list'.			*/
/****************************************************************************/
bool find2strs_in_list(const char* str1, const char* str2, str_list_t list, char* metadata)
{
	size_t	index;
	bool	found=false;
	char*	p;
	uint32_t ip_addr1, ip_addr2;

	if(list == NULL)
		return false;
	ip_addr1 = parse_ipv4_address(str1);
	ip_addr2 = parse_ipv4_address(str2);
	for(index=0; list[index]!=NULL; index++) {
		p=list[index];
		if(*p == '\0')
			continue;
		found = findstr_compare(str1, ip_addr1, p, metadata);
		if(!found && str2 != NULL)
			found = findstr_compare(str2, ip_addr2, p, metadata);
		if(found != (*p=='!'))
			break;
	}
	return found;
}

/****************************************************************************/
/* Pattern matching string search of 'insearchof' in 'fname'.				*/
/****************************************************************************/
bool findstr(const char* insearchof, const char* fname)
{
	return find2strs(insearchof, NULL, fname, NULL);
}

/****************************************************************************/
/* Pattern matching string search of 'str1' or 'str2' in 'fname'.			*/
/****************************************************************************/
bool find2strs(const char* str1, const char* str2, const char* fname, char* metadata)
{
	char		str[FINDSTR_MAX_LINE_LEN + 1];
	bool		found=false;
	FILE*		fp;
	uint32_t	ip_addr1, ip_addr2;

	if(fname == NULL || *fname == '\0')
		return false;

	if ((fp = fnopen(NULL, fname, O_RDONLY)) == NULL)
		return false; 

	ip_addr1 = parse_ipv4_address(str1);
	ip_addr2 = parse_ipv4_address(str2);
	while(!feof(fp) && !ferror(fp)) {
		if(!fgets(str,sizeof(str),fp))
			break;
		char* p = str;
		SKIP_WHITESPACE(p);
		if(*p == '\0')
			continue;
		c_unescape_str(p);
		found = findstr_compare(str1, ip_addr1, p, metadata);
		if(!found && str2 != NULL)
			found = findstr_compare(str2, ip_addr2, p, metadata);
		if(found != (*p=='!'))
			break;
	}

	fclose(fp);
	return found;
}

static char* process_findstr_item(size_t index, char *str, void* cbdata)
{
	SKIP_WHITESPACE(str);
	truncnl(str);
	return c_unescape_str(str);
}

/****************************************************************************/
str_list_t findstr_list(const char* fname)
{
	FILE*	fp;
	str_list_t	list;

	if ((fp = fnopen(NULL, fname, O_RDONLY)) == NULL)
		return NULL;

	list=strListReadFile(fp, NULL, FINDSTR_MAX_LINE_LEN);
	strListModifyEach(list, process_findstr_item, /* cbdata: */NULL);

	fclose(fp);

	return list;
}

