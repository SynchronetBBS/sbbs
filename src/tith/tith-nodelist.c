#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
