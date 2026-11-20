/* Private native context shared by the built-in OpenSSL key modules. */
#ifndef DSSH_KEY_CONTEXT_OPENSSL_H
#define DSSH_KEY_CONTEXT_OPENSSL_H

#include <openssl/evp.h>
#include "signature-result.h"

struct dssh_openssl_key_context {
	struct dssh_signature_pool *signatures;
	EVP_PKEY *pkey;
	uint8_t *pub_blob;
	size_t pub_blob_len;
};

#endif
