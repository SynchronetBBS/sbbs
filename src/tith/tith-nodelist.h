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

#define TITH_NL_NAME          (1 << 0)
#define TITH_NL_LOCATION      (1 << 1)
#define TITH_NL_SYSOP         (1 << 2)
#define TITH_NL_PHONENUMBER   (1 << 3)
#define TITH_NL_SYSTEMFLAGS   (1 << 4)
#define TITH_NL_PSTNISDNFLAGS (1 << 5)
#define TITH_NL_INTERNETFLAGS (1 << 6)
#define TITH_NL_EMAILFLAGS    (1 << 7)
#define TITH_NL_OTHERFLAGS    (1 << 8)

struct TITH_NodelistEntry {
	enum TITH_NodelistLineType keyword;
	struct TITH_NodelistAddr address;
	char *hubRoute;
	char *hostRoute;
	char *regionRoute;
	char *zoneRoute;
	char *name;
	char *location;
	char *sysop;
	char *phoneNumber;
	char *systemFlags;
	char *pstnIsdnFlags;
	char *internetFlags;
	char *emailFlags;
	char *otherFlags;
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
bool tith_parseNodelistAddr(char *addrStr, struct TITH_NodelistAddr *addr);

/*
 * Compares two struct TITH_NodelistAddr *s
 * Suitable for usage with qsort() and bsearch()
 */
int tith_cmpNodelistAddr(const void *a1, const void *a2);

/*
 * Allocates and loads a nodelist, returns the array, and sets list_size to
 * the size. flags set which fields will be stored in the returned list
 */
struct TITH_NodelistEntry *tith_loadNodelist(const char *fileName, size_t *list_size, uint32_t flags, uint16_t defaultZone);

/*
 * Frees a nodelist created by tith_loadNodelist()
 */
void tith_freeNodelist(struct TITH_NodelistEntry *list, size_t listCount);

/*
 * Finds a nodelist entry in list of size listLen entries.
 * addrStr is parsed as a fidonet address, with defaultZone and defaultNet
 * being used for zone and net respectively if they're not present
 * (use for compressed PATHs and crap like that)
 * 
 * Hopefully the defaults are always zero but YOLO.
 */
struct TITH_NodelistEntry *tith_findNodelistEntry(struct TITH_NodelistEntry *list, size_t listLen, const char *addrStr, uint16_t defaultZone, uint16_t defaultNet);

/*
 * Get a public key for the specified nodelist entry
 */
bool tith_getPublicKey(struct TITH_NodelistEntry *entry, uint8_t *pk);

#endif
