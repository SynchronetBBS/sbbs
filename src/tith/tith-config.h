#ifndef TITH_CONFIG_HEADER
#define TITH_CONFIG_HEADER

#include "hydro/hydrogen.h"

#define TITH_FADDR_MAXLEN 36

struct TITH_Node {
	const char *FTNaddress; // Full FSC-0067 format
	bool hasSecretKey;
	hydro_kx_keypair kp;
};

struct TITH_Config {
	const char *outbound;
	const char *inbound;
	size_t nodes;
	struct TITH_Node node[];
};

struct TITH_Config * tith_readConfig(const char *configFile);
struct TITH_Node * tith_getNode(struct TITH_Config * restrict cfg, struct TITH_TLV * restrict addr);

#endif
