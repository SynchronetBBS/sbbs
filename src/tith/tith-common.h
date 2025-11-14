#ifndef TITH_COMMON_HEADER
#define TITH_COMMON_HEADER

#include <setjmp.h>
#include <stdnoreturn.h>
#include <threads.h>

#include "hydro/hydrogen.h"
#include "tith.h"

#define TITH_OPTIONAL 0
#define TITH_REQUIRED 1

extern thread_local hydro_kx_session_keypair tith_sessionKeyPair;
extern thread_local bool tith_encrypting;
extern thread_local void *tith_handle;
extern thread_local jmp_buf tith_exitJmpBuf;
extern thread_local struct TITH_TLV *tith_cmd;

noreturn void tith_logError(const char *str);
void tith_getCmd(void);
void tith_allocCmd(enum TITH_Type type);
void tith_addData(enum TITH_Type type, uint64_t len, void *data);
void tith_sendCmd(void);
void tith_validateAddress(const char *addr);
void tith_validateCmd(enum TITH_Type command, int numargs, ...);
char *tith_strDup(const char *str);
void tith_cleanup(void);
void tith_pushAlloc(void *ptr);
void *tith_popAlloc(void);
void tith_freeCmd(void);

#endif
