#ifndef TITH_COMMON_HEADER
#define TITH_COMMON_HEADER

#include <stdnoreturn.h>

#include "hydro/hydrogen.h"
#include "tith.h"

#define TITH_OPTIONAL 0
#define TITH_REQUIRED 1

extern hydro_kx_session_keypair sessionKeyPair;
extern bool encrypting;

noreturn void tith_logError(const char *str);
void tith_freeTLV(struct TITH_TLV *tlv);
struct TITH_TLV * tith_getCmd(void);
struct TITH_TLV * tith_allocCmd(enum TITH_Type type);
void tith_addData(struct TITH_TLV *cmd, enum TITH_Type type, uint64_t len, void *data);
void tith_sendCmd(struct TITH_TLV *cmd);
void validateAddress(const char *addr);
void tith_validateCmd(struct TITH_TLV *tlv, enum TITH_Type command, int numargs, ...);
char *StrDup(const char *str);

#endif
