#ifndef TITH_CONFIG_HEADER
#define TITH_CONFIG_HEADER

#include "hydro/hydrogen.h"
#include "tith-common.h"
#include "tith-nodelist.h"

#define TITH_FADDR_MAXLEN 36

struct TITH_Node {
	const char *FTNaddress; // TTS-0003 format
	bool hasSecretKey;
	hydro_sign_keypair kp;
};

struct TITH_ConfigNodelist {
	char *domain;
	size_t nodelistLength;
	struct TITH_NodelistEntry *list;
};

struct TITH_Config {
	const char *outbound;
	const char *inbound;
	size_t nodeLists;
	struct TITH_ConfigNodelist *nodeList;
	size_t nodes;
	struct TITH_Node node[];
};

extern thread_local struct TITH_Config *cfg;

void tith_readConfig(const char *configFile);
struct TITH_Node * tith_getNode(struct TITH_Config * restrict cfg, struct TITH_TLV * restrict addr);
void tith_freeConfig(void);
void tith_configGetPublicKey(const struct TITH_TLV *addr, uint8_t *pk);

#endif
