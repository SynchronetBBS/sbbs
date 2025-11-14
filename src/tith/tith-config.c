#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "base64.h"
#include "tith-common.h"
#include "tith-config.h"

static char *
removeWhitespace(char *str)
{
	while(isspace(*str))
		str++;
	size_t end = strlen(str);
	while (end > 0 && isspace(str[end - 1])) {
		str[end - 1] = 0;
		end--;
	}
	return str;
}

static void
splitKeyValue(char *str, char **key, char **val)
{
	char *p = strchr(str, '=');
	if (p == NULL)
		tith_logError("No equals sign in config line");
	*p = 0;
	p++;
	*key = removeWhitespace(str);
	*val = removeWhitespace(p);
}

static void
addNode(struct TITH_Config **config, const char *key, const char *value)
{
	tith_validateAddress(key);
	for (size_t i = 0; i < (*config)->nodes; i++) {
		if (strcmp((*config)->node[i].FTNaddress, key) == 0)
			tith_logError("Duplicate node in config");
	}
	struct TITH_Config *newConfig = realloc(*config, offsetof(struct TITH_Config, node) + sizeof(struct TITH_Node) * ((*config)->nodes + 1));
	if (newConfig == NULL)
		tith_logError("realloc() failure");
	*config = newConfig;
	(*config)->node[(*config)->nodes].FTNaddress = tith_strDup(key);
	if ((*config)->node[(*config)->nodes].FTNaddress == NULL)
		tith_logError("tith_strDup() failed in addNode()");
	b64_decode((*config)->node[(*config)->nodes].kp.pk, sizeof((*config)->node[(*config)->nodes].kp.pk), value);
	char *p = strchr(value, ',');
	if (p) {
		p++;
		b64_decode((*config)->node[(*config)->nodes].kp.sk, sizeof((*config)->node[(*config)->nodes].kp.sk), p);
		(*config)->node[(*config)->nodes].hasSecretKey = true;
	}
	(*config)->nodes++;
}

static int
cmpAddrs(const void *a1, const void *a2)
{
	return strcmp(a1, a2);
}

struct TITH_Config *
tith_readConfig(const char *configFile)
{
	if (configFile == NULL)
		configFile = "tith.cfg";
	FILE *fp = fopen(configFile, "r");
	if (fp == NULL)
		tith_logError("Unable to open config file");
	struct TITH_Config *ret = calloc(1, offsetof(struct TITH_Config, node));
	if (ret == NULL)
		tith_logError("malloc() failure");
	for (;;) {
		char line[1024];
		if (fgets(line, sizeof(line), fp) == NULL) {
			if (feof(fp))
				break;
			tith_logError("Error reading config line");
		}
		char *p = line;
		while (isspace(*p))
			p++;
		if (!*p)
			continue;
		if (*p == ';')
			continue;
		char *key;
		char *val;
		splitKeyValue(line, &key, &val);
		if (strcmp(key, "Outbound") == 0) {
			if (ret->outbound)
				tith_logError("Multiple Outbounds in config");
			ret->outbound = tith_strDup(val);
			if (ret->outbound == NULL)
				tith_logError("tith_strDup() failed in tith_readConfig()");
		}
		else if (strcmp(key, "Inbound") == 0) {
			if (ret->inbound)
				tith_logError("Multiple Inbounds in config");
			ret->inbound = tith_strDup(val);
			if (ret->inbound == NULL)
				tith_logError("tith_strDup() failed in tith_readConfig()");
		}
		else
			addNode(&ret, key, val);
	}
	if (ret->nodes)
		qsort(ret->node, ret->nodes, sizeof(ret->node[0]), cmpAddrs);
	return ret;
}

static int
cmpAddrsTLV(const void *key, const void *node)
{
	struct TITH_TLV *k = (struct TITH_TLV *)key;
	struct TITH_Node *n = (struct TITH_Node *)node;

	size_t nlen = strlen(n->FTNaddress);
	size_t clen = nlen < k->length ? nlen : k->length;
	int mres = memcmp(k->value, n->FTNaddress, clen);
	if (mres)
		return mres;
	if (nlen < k->length)
		return 1;
	if (nlen > k->length)
		return -1;
	return 0;
}

struct TITH_Node *
tith_getNode(struct TITH_Config * restrict cfg, struct TITH_TLV * restrict addr)
{
	return bsearch(addr, cfg->node, cfg->nodes, sizeof(cfg->node[0]), cmpAddrsTLV);
}
