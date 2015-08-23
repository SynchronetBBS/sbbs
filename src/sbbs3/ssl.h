#ifndef SBBS_SSL_H
#define SBBS_SSL_H

#include "sbbs.h"	// For DLLEXPORT
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
		#define DLLEXPORT	__declspec(dllexport)
	#else
		#define DLLEXPORT	__declspec(dllimport)
	#endif
	#ifdef __BORLANDC__
		#define DLLCALL __stdcall
	#else
		#define DLLCALL
	#endif
#else	/* !_WIN32 */
	#define DLLEXPORT
	#define DLLCALL
#endif

#define SSL_ESTR_LEN	CRYPT_MAX_TEXTSIZE + 1024 /* File name, line number, status code, and some static text */

#if defined(__cplusplus)
extern "C" {
#endif

DLLEXPORT void DLLCALL free_crypt_attrstr(char *attr);
DLLEXPORT char* DLLCALL get_crypt_attribute(CRYPT_SESSION sess, C_IN CRYPT_ATTRIBUTE_TYPE attr);
DLLEXPORT char* DLLCALL get_crypt_error(CRYPT_SESSION sess);
DLLEXPORT CRYPT_CONTEXT DLLCALL get_ssl_cert(scfg_t *cfg, char *estr);

#if defined(__cplusplus)
}
#endif

#endif
