#ifndef SBBS_SSL_H
#define SBBS_SSL_H

#include <cryptlib.h>
#include "scfgdefs.h"

#define SSL_ESTR_LEN	CRYPT_MAX_TEXTSIZE + 1024 /* File name, line number, status code, and some static text */

CRYPT_CONTEXT get_ssl_cert(scfg_t *cfg, char *estr);

#endif
