#ifndef TITH_NODELIST_HEADER
#define TITH_NODELIST_HEADER

#include <stdint.h>

struct TITH_NodelistAddr {
	uint16_t zone;
	uint16_t net;
	uint16_t node;
};

enum TITH_NodelistLineType {
	TYPE_Normal,
	TYPE_Down,
	TYPE_Hold,
	TYPE_Host,
	TYPE_Hub,
	TYPE_Private,
	TYPE_Region,
	TYPE_Zone,
	TYPE_Unknown
};

/*
 * Parses the first field of a nodelist and returns a TITH_NodelistLineType,
 * resetting appropriate values in addr if specified.
 */
enum TITH_NodelistLineType tith_parseNodelistKeyword(const char *kw, struct TITH_NodelistAddr *addr);

/*
 * Parses the second field of a nodelist and updates appropriate files in addr
 * if specified.
 * 
 * Returns false if it failed, or true on success
 */
bool tith_parseNodelistNodeNumber(const char *str, enum TITH_NodelistLineType type, struct TITH_NodelistAddr *addr);

/*
 * Parses the string addrStr into addr
 * 
 * Returns false on failure
 */
bool
tith_parseNodelistAddr(char *addrStr, struct TITH_NodelistAddr *addr);

#endif
