#ifndef SBBS_SSL_H
#define SBBS_SSL_H

#include "dllexport.h"
#include <cryptlib.h>
#include "scfgdefs.h"

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif
#ifdef _WIN32
	#ifdef SBBS_EXPORTS
		#define DLLEXPORT   __declspec(dllexport)
	#else
		#define DLLEXPORT   __declspec(dllimport)
	#endif
	#ifdef __BORLANDC__
		#define DLLCALL
	#else
		#define DLLCALL
	#endif
#else   /* !_WIN32 */
	#define DLLEXPORT
	#define DLLCALL
#endif

#define SSL_ESTR_LEN    CRYPT_MAX_TEXTSIZE + 1024 /* File name, line number, status code, and some static text */

#if defined(__cplusplus)
extern "C" {
#endif

DLLEXPORT void DLLCALL free_crypt_attrstr(char *attr);
DLLEXPORT char* DLLCALL get_binary_crypt_attribute(CRYPT_HANDLE sess, C_IN CRYPT_ATTRIBUTE_TYPE attr, size_t *sz);
DLLEXPORT char* DLLCALL get_crypt_attribute(CRYPT_HANDLE sess, C_IN CRYPT_ATTRIBUTE_TYPE attr);
DLLEXPORT char* DLLCALL get_crypt_error(CRYPT_HANDLE sess);
DLLEXPORT bool DLLCALL do_cryptInit(int (*lprintf)(int level, const char* fmt, ...));
DLLEXPORT bool DLLCALL is_crypt_initialized(void);
DLLEXPORT bool DLLCALL get_crypt_error_string(int status, CRYPT_HANDLE sess, char **estr, const char *action, int *level);
DLLEXPORT int add_private_key(scfg_t *cfg, int (*lprintf)(int level, const char* fmt, ...), CRYPT_SESSION csess);
DLLEXPORT int destroy_session(int (*lprintf)(int level, const char* fmt, ...), CRYPT_SESSION csess);
DLLEXPORT bool ssl_sync(scfg_t *scfg, int (*lprintf)(int level, const char* fmt, ...));

#if defined(__cplusplus)
}
#endif

#endif
