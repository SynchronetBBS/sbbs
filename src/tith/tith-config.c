#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>

#include "base64.h"
#include "tith-common.h"
#include "tith-config.h"
#include "tith-file.h"

thread_local struct TITH_Config *cfg;

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
	const struct TITH_Node *n1 = a1;
	const struct TITH_Node *n2 = a2;
	return strcmp(n1->FTNaddress, n2->FTNaddress);
}

static bool
resizeAlloc(struct TITH_ConfigNodelist **cur, size_t *sz, size_t *count, size_t msz)
{
	size_t newSize = *sz ? *sz * 2 : 2;
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

void
tith_readConfig(const char *configFile)
{
	if (configFile == NULL)
		configFile = "tith.cfg";
	if (cfg)
		tith_freeConfig();
	FILE *fp = fopen(configFile, "r");
	if (fp == NULL)
		tith_logError("Unable to open config file");
	cfg = calloc(1, offsetof(struct TITH_Config, node));
	if (cfg == NULL)
		tith_logError("malloc() failure");
	size_t nodeListSz = 0;
	for (;;) {
		char *line = tith_readLine(fp);
		if (line == NULL) {
			if (feof(fp))
				break;
			tith_logError("Error reading config line");
		}
		char *p = line;
		while (isspace(*p))
			p++;
		if (!*p) {
			free(line);
			continue;
		}
		if (*p == ';') {
			free(line);
			continue;
		}
		char *key;
		char *val;
		splitKeyValue(line, &key, &val);
		if (strcmp(key, "Outbound") == 0) {
			if (cfg->outbound)
				tith_logError("Multiple Outbounds in config");
			cfg->outbound = tith_strDup(val);
			if (cfg->outbound == NULL)
				tith_logError("tith_strDup() failed in tith_readConfig()");
		}
		else if (strcmp(key, "Inbound") == 0) {
			if (cfg->inbound)
				tith_logError("Multiple Inbounds in config");
			cfg->inbound = tith_strDup(val);
			if (cfg->inbound == NULL)
				tith_logError("tith_strDup() failed in tith_readConfig()");
		}
		else if (strncmp(key, "Nodelist,", 9) == 0) {
			if (!resizeAlloc(&cfg->nodeList, &nodeListSz, &cfg->nodeLists, sizeof(struct TITH_ConfigNodelist)))
				tith_logError("Failed to create storage for nodelist");
			struct TITH_ConfigNodelist *nl = &cfg->nodeList[cfg->nodeLists++];
			nl->domain = tith_strDup(&key[9]);
			nl->list = tith_loadNodelist(val, &nl->nodelistLength, TITH_NL_INTERNETFLAGS | TITH_NL_SYSOP | TITH_NL_LOCATION | TITH_NL_NAME | TITH_NL_SYSTEMFLAGS, 0);
		}
		else
			addNode(&cfg, key, val);
		free(line);
	}
	if (cfg->nodes)
		qsort(cfg->node, cfg->nodes, sizeof(cfg->node[0]), cmpAddrs);
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

void tith_freeConfig(void)
{
	if (cfg == NULL)
		return;
	free((void*)cfg->inbound);
	free((void*)cfg->outbound);
	free(cfg->nodeList);
	for (size_t nl = 0; nl < cfg->nodeLists; nl++) {
		free(cfg->nodeList[nl].domain);
		free(cfg->nodeList[nl].list);
	}
	free(cfg->nodeList);
	for (size_t node = 0; node < cfg->nodes; node++)
		free((void*)cfg->node[node].FTNaddress);
	free(cfg);
}
