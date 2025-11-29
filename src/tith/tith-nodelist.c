#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base64.h"
#include "tith-common.h"
#include "tith-file.h"
#include "tith-nodelist.h"

static const char *const keywords[] = {
	"",
	"Down",
	"Hold",
	"Host",
	"Hub",
	"Pvt",
	"Region",
	"Zone",
};

static int
bstrcmp(const void *key, const void *val)
{
	return strcmp((const char *)key, *(const char **)val);
}

enum TITH_NodelistLineType
tith_parseNodelistKeyword(const char *kw, struct TITH_NodelistAddr *addr)
{
	const char **kws = bsearch(kw, keywords, sizeof(keywords) / sizeof(keywords[0]), sizeof(keywords[0]), bstrcmp);
	enum TITH_NodelistLineType type = kws ? (enum TITH_NodelistLineType)(kws - keywords) : TYPE_Unknown;
	if (addr) {
		switch (type) {
			case TYPE_Zone:
				addr->zone = 0;
				// Fallthrough
			case TYPE_Region:
			case TYPE_Host:
				addr->net = 0;
				// Fallthrough
			case TYPE_Hub:
			case TYPE_Normal:
			case TYPE_Private:
			case TYPE_Hold:
			case TYPE_Down:
				addr->node = 0;
				// Fallthrough
			default:
				break;
		}
	}
	return (enum TITH_NodelistLineType)type;
}

bool
tith_parseNodelistNodeNumber(const char *str, enum TITH_NodelistLineType type, struct TITH_NodelistAddr *addr)
{
	char *end;
	long val = strtol(str, &end, 10);
	if (val < 1 || val > 32767 || end == NULL || *end)
		return false;
	if (addr) {
		switch(type) {
			case TYPE_Zone:
				addr->zone = (uint16_t)val;
				// Fallthrough
			case TYPE_Region:
			case TYPE_Host:
				addr->net = (uint16_t)val;
				break;
			default:
				addr->node = (uint16_t)val;
				break;
		}
	}
	return true;
}

static bool
checkedStrToUL(const char * restrict nptr, char **restrict endptr, int base, long *l)
{
	if (nptr == NULL)
		return false;
	errno = 0;
	char *end = NULL;
	*l = strtol(nptr, &end, base);
	if (errno)
		return false;
	if (end == NULL)
		return false;
	if (end == nptr)
		return false;
	if (endptr)
		*endptr = end;
	return true;
}

static uint16_t
parseAddrComponent(char *start, char **end, char final, bool *success)
{
	long ret;
	if (success)
		*success = true;
	if (!checkedStrToUL(start, end, 10, &ret) || **end != final) {
		if (success)
			*success = false;
		return 0;
	}
	if (ret < 0 || ret > 32767) {
		if (success)
			*success = false;
		return 0;
	}
	return (uint16_t)ret;
}

bool
tith_parseNodelistAddr(char *line, struct TITH_NodelistAddr *addr)
{
	char *next = line;
	bool ret = false;
	addr->zone = parseAddrComponent(next, &next, ':', &ret);
	if (ret) {
		next++;
		addr->net = parseAddrComponent(next, &next, '/', &ret);
	}
	if (ret) {
		next++;
		addr->node = parseAddrComponent(next, &next, 0, &ret);
	}
	return ret;
}

int
tith_cmpNodelistAddr(const void *a1, const void *a2)
{
	const struct TITH_NodelistAddr *addr1 = a1;
	const struct TITH_NodelistAddr *addr2 = a2;

	if (addr1->zone != addr2->zone)
		return addr1->zone - addr2->zone;
	if (addr1->net != addr2->net)
		return addr1->net - addr2->net;
	if (addr1->node != addr2->node)
		return addr1->node - addr2->node;
	return 0;
}

static char *
getfield(char *line, char sep, char **end)
{
	if (line == NULL)
		return "";
	char *p = strchr(line, sep);
	if (p) {
		if (*p) {
			*end = p + 1;
			*p = 0;
		}
		else
			*end = p;
	}
	else
		*end = NULL;
	return line;
}

static bool
resizeAlloc(struct TITH_NodelistEntry **cur, size_t *sz, size_t *count, size_t msz)
{
	size_t newSize = *sz ? *sz * 2 : 32;
	void *newAlloc = realloc(*cur, msz * newSize);
	if (newAlloc == NULL) {
		free(*cur);
		*cur = NULL;
		*count = 0;
		return false;
	}
	*cur = newAlloc;
	*sz = newSize;
	return true;
}

static int
cmpNL(const void *a, const void *b)
{
	const struct TITH_NodelistEntry *e1 = a;
	const struct TITH_NodelistEntry *e2 = b;

	return tith_cmpNodelistAddr(&e1->address, &e2->address);
}

static int
cmpNLk(const void *a, const void *b)
{
	const struct TITH_NodelistAddr *key = a;
	const struct TITH_NodelistEntry *e = b;

	return tith_cmpNodelistAddr(key, &e->address);
}

bool
tith_getPublicKey(struct TITH_NodelistEntry *entry, uint8_t *pk)
{
	if (entry == NULL)
		return false;
	if (entry->internetFlags == NULL)
		return false;
	char *ifs = tith_strDup(entry->internetFlags);
	char *next = ifs;
	bool ret = false;
	for (;;) {
		char *field = getfield(next, ',', &next);
		if (field[0] == 0)
			break;
		if (strncmp(field, "IIH:", 4))
			continue;
		char *lc = strrchr(field, ':');
		lc++;
		if (b64_decode(pk, hydro_sign_SECRETKEYBYTES, lc))
			ret = true;
		break;
	}
	free(ifs);
	return ret;
}

struct TITH_NodelistEntry *
tith_findNodelistEntry(struct TITH_NodelistEntry *list, size_t listLen, const char *addrStr, uint16_t defaultZone, uint16_t defaultNet)
{
	struct TITH_NodelistAddr addr = {
		.zone = defaultZone,
		.net = defaultNet ? defaultNet : defaultZone
	};
	int expect = -1;
	// Parse any kind of whackadoodle 5D or less address
	char *p = strchr(addrStr, '#');
	if (p) {
		addrStr = p + 1;
		expect = 0;
	}
	else {
		switch (*addrStr) {
			case ':':
				expect = 1;
				addrStr++;
				break;
			case '/':
				expect = 2;
				addrStr++;
				break;
			case '.':
				expect = 3;
				addrStr++;
				return NULL;
		}
	}
	char *end = NULL;
	long val;
	do {
		if (!checkedStrToUL(addrStr, &end, 10, &val) || val < 0 || val > INT16_MAX)
			return NULL;
		switch (*end) {
			case ':':
				if (expect == 0 || expect == -1) {
					if (val == 0)
						return NULL;
					addr.zone = (uint16_t)val;
					addr.net = addr.zone;
				}
				else
					return NULL;
				expect = 1;
				addrStr = end + 1;
				break;
			case '/':
				if (expect == 1 || expect == -1) {
					if (val == 0)
						return NULL;
					addr.net = (uint16_t)val;
				}
				else
					return NULL;
				expect = 2;
				addrStr = end + 1;
				break;
			case '.':
				if (expect == 2 || expect == -1)
					addr.node = (uint16_t)val;
				else
					return NULL;
				expect = 3;
				addrStr = end + 1;
				break;
			case '@':
			case 0:
				switch (expect) {
					case 0:
						if (val == 0)
							return NULL;
						addr.zone = (uint16_t)val;
						addr.net = addr.zone;
						break;
					case 1:
						if (val == 0)
							return NULL;
						addr.net = (uint16_t)val;
						break;
					case 2:
						addr.node = (uint16_t)val;
						break;
					case 3:
						if (val)
							return NULL;
						break;
					default:
						return NULL;
				}
				end = NULL;
		}
	} while (end && *end);
	if (addr.net == 0 || addr.zone == 0)
		return NULL;
	// Ok, we've parsed the hellspawn, now find it.
	return bsearch(&addr, list, listLen, sizeof(*list), cmpNLk);
}

static char *
addrString(struct TITH_NodelistAddr *addr)
{
	char tmpAddr[18];

	printf(tmpAddr, "%hu:%hu/%hu", addr->zone, addr->net, addr->node);
	return tith_strDup(tmpAddr);
}

struct TITH_NodelistEntry *
tith_loadNodelist(const char *fileName, size_t *list_size, uint32_t flags, uint16_t defaultZone)
{
	struct TITH_NodelistEntry *ret = NULL;
	FILE *fp = fopen(fileName, "rb");
	if (fp == NULL)
		goto fail;
	struct TITH_NodelistAddr addr = {
		.zone = defaultZone
	};
	size_t retSz = 0;
	size_t retCount = 0;
	char *hubRoute = NULL;
	char *hostRoute = NULL;
	char *regionRoute = NULL;
	char *zoneRoute = NULL;
	for (;;) {
		char *line = tith_readLine(fp);
		if (line == NULL)
			break;
		if (line[0] != ';') {
			if (retCount + 1 >= retSz) {
				if (!resizeAlloc(&ret, &retSz, &retCount, sizeof(*ret)))
					break;
			}
			struct TITH_NodelistEntry *entry = &ret[retCount];
			memset(entry, 0, sizeof(*entry));
			char *next = line;
			char *field = getfield(next, '\t', &next);
			entry->keyword = tith_parseNodelistKeyword(field, &addr);
			if (entry->keyword == TYPE_Unknown)
				goto fail;
			if (entry->keyword == TYPE_Normal || entry->keyword == TYPE_Private || entry->keyword == TYPE_Hold) {
				if (hubRoute)
					entry->hubRoute = tith_strDup(hubRoute);
				if (hostRoute)
					entry->hostRoute = tith_strDup(hostRoute);
				if (regionRoute)
					entry->regionRoute = tith_strDup(regionRoute);
				if (zoneRoute)
					entry->zoneRoute = tith_strDup(zoneRoute);
			}
			else if (entry->keyword != TYPE_Down) {
				free(hubRoute);
				hubRoute = NULL;
				if (entry->keyword != TYPE_Hub) {
					free(hostRoute);
					hostRoute = NULL;
					if (entry->keyword != TYPE_Host) {
						free(regionRoute);
						regionRoute = NULL;
						if (entry->keyword != TYPE_Region) {
							free(zoneRoute);
							zoneRoute = NULL;
						}
					}
				}
			}
			field = getfield(next, '\t', &next);
			if(!tith_parseNodelistNodeNumber(field, entry->keyword, &addr))
				goto abortLine;
			retCount++;
			memcpy(&entry->address, &addr, sizeof(entry->address));
			if (entry->keyword == TYPE_Hub)
				hubRoute = addrString(&entry->address);
			if (entry->keyword == TYPE_Host)
				hostRoute = addrString(&entry->address);
			if (entry->keyword == TYPE_Region)
				regionRoute = addrString(&entry->address);
			if (entry->keyword == TYPE_Zone)
				zoneRoute = addrString(&entry->address);
			field = getfield(next, '\t', &next);
			if (flags & TITH_NL_NAME)
				entry->name = tith_strDup(field);
			field = getfield(next, '\t', &next);
			if (flags & TITH_NL_LOCATION)
				entry->location = tith_strDup(field);
			field = getfield(next, '\t', &next);
			if (flags & TITH_NL_SYSOP)
				entry->sysop = tith_strDup(field);
			field = getfield(next, '\t', &next);
			if (flags & TITH_NL_PHONENUMBER)
				entry->phoneNumber = tith_strDup(field);
			field = getfield(next, '\t', &next);
			if (flags & TITH_NL_SYSTEMFLAGS)
				entry->systemFlags = tith_strDup(field);
			field = getfield(next, '\t', &next);
			if (flags & TITH_NL_PSTNISDNFLAGS)
				entry->pstnIsdnFlags = tith_strDup(field);
			field = getfield(next, '\t', &next);
			if (flags & TITH_NL_INTERNETFLAGS)
				entry->internetFlags = tith_strDup(field);
			field = getfield(next, '\t', &next);
			if (flags & TITH_NL_EMAILFLAGS)
				entry->emailFlags = tith_strDup(field);
			field = getfield(next, '\t', &next);
			if (flags & TITH_NL_OTHERFLAGS)
				entry->otherFlags = tith_strDup(field);
		}
abortLine:
		free(line);
	}
	if (!feof(fp))
		goto fail;
	fclose(fp);
	*list_size = retCount;
	if (ret) {
		struct TITH_NodelistEntry *small = realloc(ret, retCount * sizeof(struct TITH_NodelistEntry));
		if (small)
			ret = small;
	}
	qsort(ret, retCount, sizeof(*ret), cmpNL);
	return ret;

fail:
	if (fp)
		fclose(fp);
	free(ret);
	*list_size = 0;
	return NULL;
}

void
tith_freeNodelist(struct TITH_NodelistEntry *list, size_t listCount)
{
	for (size_t i = 0; i < listCount; i++) {
		free(list[i].emailFlags);
		free(list[i].hostRoute);
		free(list[i].internetFlags);
		free(list[i].location);
		free(list[i].name);
		free(list[i].otherFlags);
		free(list[i].phoneNumber);
		free(list[i].pstnIsdnFlags);
		free(list[i].sysop);
		free(list[i].systemFlags);
	}
	free(list);
}
