#ifndef SBBS_SSL_H
#define SBBS_SSL_H

#include "sbbs.h"	// For DLLEXPORT
#include <cryptlib.h>
#include "scfgdefs.h"
#include "wrapdll.h"

#define SSL_ESTR_LEN	CRYPT_MAX_TEXTSIZE + 1024 /* File name, line number, status code, and some static text */

DLLEXPORT void* DLLCALL free_crypt_attrstr(char *attr);
DLLEXPORT char* DLLCALL get_crypt_attribute(CRYPT_SESSION sess, C_IN CRYPT_ATTRIBUTE_TYPE attr);
DLLEXPORT char* DLLCALL get_crypt_error(CRYPT_SESSION sess);
DLLEXPORT CRYPT_CONTEXT DLLCALL get_ssl_cert(scfg_t *cfg, char *estr);

#endif
