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
#include "sockwrap.h"   /* inet_pton, AF_INET6 (xp_inet_pton shim on Windows) */

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
	char        buf[FINDSTR_MAX_LINE_LEN + 1];
	char*       p = (char*)pattern;
	char*       last;
	const char* splat;
	size_t      len;
	bool        found = false;

	if (pattern == NULL)
		return false;

	if (*p == ';')       /* comment */
		return false;

	if (*p == '!')   {   /* reverse-match */
		found = true;
		p++;
	}

	if (search == NULL)
		return found;

	SAFECOPY(buf, p);
	p = buf;

	truncsp(p);
	len = strlen(p);
	if (len > 0) {
		last = p + len - 1;
		if (*last == '~') {
			*last = '\0';
			if (strcasestr(search, p) != NULL)
				found = !found;
		}

		else if (*last == '^') {
			if (strnicmp(p, search, len - 1) == 0)
				found = !found;
		}

		else if ((splat = strchr(p, '*')) != NULL) {
			int left = splat - p;
			int right = len - (left + 1);
			int slen = strlen(search);
			if (slen < left + right)
				return found;
			if (strnicmp(search, p, left) == 0
			    && strnicmp(p + left + 1, search + (slen - right), right) == 0)
				found = !found;
		}

		else if (stricmp(p, search) == 0)
			found = !found;
	}
	return found;
}

static uint32_t encode_ipv4_address(unsigned int byte[])
{
	if (byte[0] > 0xff || byte[1] > 0xff || byte[2] > 0xff || byte[3] > 0xff)
		return 0;
	return (byte[0] << 24) | (byte[1] << 16) | (byte[2] << 8) | byte[3];
}

static uint32_t parse_ipv4_address(const char* str)
{
	unsigned int byte[4];

	if (str == NULL)
		return 0;
	if (sscanf(str, "%u.%u.%u.%u", &byte[0], &byte[1], &byte[2], &byte[3]) != 4)
		return 0;
	return encode_ipv4_address(byte);
}

/* Parses an IPv4 CIDR pattern like "192.0.2.0/24" (optionally prefixed with
 * '!' for reverse-match). Returns true on success; the network address goes
 * into `*network` and the prefix length into `*subnet` (0..32). Uses bool +
 * out-param rather than a uint32_t-with-0-sentinel so that legitimate patterns
 * with a 0.0.0.0 network (e.g. "0.0.0.0/0" — match any IPv4) parse correctly. */
static bool parse_cidr(const char* p, uint32_t* network, unsigned* subnet)
{
	unsigned int byte[4];

	if (*p == '!')
		p++;

	*network = 0;
	*subnet = 0;
	if (sscanf(p, "%u.%u.%u.%u/%u", &byte[0], &byte[1], &byte[2], &byte[3], subnet) != 5)
		return false;
	if (*subnet > 32)
		return false;
	if (byte[0] > 0xff || byte[1] > 0xff || byte[2] > 0xff || byte[3] > 0xff)
		return false;
	*network = (byte[0] << 24) | (byte[1] << 16) | (byte[2] << 8) | byte[3];
	return true;
}

static bool is_cidr_match(const char *p, uint32_t ip_addr, uint32_t cidr, unsigned subnet)
{
	bool match = false;

	if (*p == '!')
		match = true;

	/* subnet==0 means "match anything" but a 32-bit `>> 32` is undefined
	 * behavior; subnet>32 should never be possible (parse_cidr rejects it)
	 * but cap defensively. */
	if (subnet > 32)
		subnet = 32;
	if (subnet == 0 || ((ip_addr ^ cidr) >> (32 - subnet)) == 0)
		match = !match;

	return match;
}

/* Returns true if `str` parses as a valid IPv6 address, in which case `addr`
 * receives the 16-byte network-order representation. Distinct from the IPv4
 * parser, which returns 0 to signal both "couldn't parse" and "0.0.0.0";
 * here we use bool so that the unspecified address (::) is representable. */
static bool parse_ipv6_address(const char* str, uint8_t addr[16])
{
	if (str == NULL || strchr(str, ':') == NULL)
		return false;
	return inet_pton(AF_INET6, str, addr) == 1;
}

/* Parses an IPv6 CIDR pattern like "2001:db8::/32" (optionally prefixed with
 * '!' for reverse-match). Returns true on success and fills `addr` + `subnet`.
 * subnet range is 0..128. The pattern is rejected unless it contains both
 * ':' and '/' so that plain IPv4 CIDRs and plain text never falsely match. */
static bool parse_ipv6_cidr(const char* p, uint8_t addr[16], unsigned* subnet)
{
	char        buf[FINDSTR_MAX_LINE_LEN + 1];
	const char* slash;
	size_t      addr_len;
	unsigned    n;

	if (*p == '!')
		p++;

	*subnet = 0;
	if (strchr(p, ':') == NULL)
		return false;
	if ((slash = strchr(p, '/')) == NULL)
		return false;
	addr_len = slash - p;
	if (addr_len == 0 || addr_len >= sizeof(buf))
		return false;
	memcpy(buf, p, addr_len);
	buf[addr_len] = '\0';
	if (inet_pton(AF_INET6, buf, addr) != 1)
		return false;
	if (sscanf(slash + 1, "%u", &n) != 1 || n > 128)
		return false;
	*subnet = n;
	return true;
}

static bool is_ipv6_cidr_match(const char* p, const uint8_t ip[16], const uint8_t cidr[16], unsigned subnet)
{
	bool     match = false;
	unsigned full_bytes;
	unsigned remaining_bits;

	if (*p == '!')
		match = true;

	if (subnet > 128)
		subnet = 128;
	full_bytes = subnet / 8;
	remaining_bits = subnet % 8;

	if (full_bytes > 0 && memcmp(ip, cidr, full_bytes) != 0)
		return match;
	if (remaining_bits != 0) {
		uint8_t mask = (uint8_t)(0xff << (8 - remaining_bits));
		if ((ip[full_bytes] & mask) != (cidr[full_bytes] & mask))
			return match;
	}
	return !match;
}

/* Carries the pre-parsed forms of an input search-string so the per-pattern
 * loop can dispatch to the right CIDR matcher without re-parsing each time. */
typedef struct {
	uint32_t v4;       /* 0 = not parseable as IPv4 (same sentinel as parse_ipv4_address) */
	uint8_t  v6[16];   /* valid only if has_v6 */
	bool     has_v6;
} findstr_ip_t;

static void parse_ip(const char* str, findstr_ip_t* out)
{
	out->v4 = parse_ipv4_address(str);
	out->has_v6 = (out->v4 == 0) && parse_ipv6_address(str, out->v6);
}

static bool findstr_compare(const char* str, const findstr_ip_t* ip, const char* pattern, char* metadata)
{
	uint32_t cidr4;
	uint8_t  cidr6[16];
	unsigned subnet;
	char     buf[FINDSTR_MAX_LINE_LEN + 1];
	char*    p;

	SAFECOPY(buf, pattern);
	if ((p = strchr(buf, '\t')) != NULL) {
		*p = '\0';
		p++;
		if (metadata != NULL)
			snprintf(metadata, FINDSTR_MAX_LINE_LEN, "%s", p);
	} else {
		if (metadata != NULL)
			*metadata = '\0';
	}
	if (ip != NULL) {
		if (ip->v4 != 0 && parse_cidr(pattern, &cidr4, &subnet))
			return is_cidr_match(pattern, ip->v4, cidr4, subnet);
		if (ip->has_v6 && parse_ipv6_cidr(pattern, cidr6, &subnet))
			return is_ipv6_cidr_match(pattern, ip->v6, cidr6, subnet);
	}
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
	size_t        index;
	bool          found = false;
	char*         p;
	findstr_ip_t  ip1 = {0}, ip2 = {0};

	if (list == NULL)
		return false;
	parse_ip(str1, &ip1);
	if (str2 != NULL)
		parse_ip(str2, &ip2);
	for (index = 0; list[index] != NULL; index++) {
		p = list[index];
		if (*p == '\0')
			continue;
		found = findstr_compare(str1, &ip1, p, metadata);
		if (!found && str2 != NULL)
			found = findstr_compare(str2, &ip2, p, metadata);
		if (found != (*p == '!'))
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
	char          str[FINDSTR_MAX_LINE_LEN + 1];
	bool          found = false;
	FILE*         fp;
	findstr_ip_t  ip1 = {0}, ip2 = {0};

	if (fname == NULL || *fname == '\0')
		return false;

	if ((fp = fnopen(NULL, fname, O_RDONLY)) == NULL)
		return false;

	parse_ip(str1, &ip1);
	if (str2 != NULL)
		parse_ip(str2, &ip2);
	while (!feof(fp) && !ferror(fp)) {
		if (!fgets(str, sizeof(str), fp))
			break;
		char* p = str;
		SKIP_WHITESPACE(p);
		if (*p == '\0')
			continue;
		c_unescape_str(p);
		found = findstr_compare(str1, &ip1, p, metadata);
		if (!found && str2 != NULL)
			found = findstr_compare(str2, &ip2, p, metadata);
		if (found != (*p == '!'))
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
	FILE*      fp;
	str_list_t list;

	if ((fp = fnopen(NULL, fname, O_RDONLY)) == NULL)
		return NULL;

	list = strListReadFile(fp, NULL, FINDSTR_MAX_LINE_LEN);
	strListModifyEach(list, process_findstr_item, /* cbdata: */ NULL);

	fclose(fp);

	return list;
}

