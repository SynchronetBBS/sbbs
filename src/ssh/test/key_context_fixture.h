#ifndef DSSH_TEST_KEY_CONTEXT_FIXTURE_H
#define DSSH_TEST_KEY_CONTEXT_FIXTURE_H

#include <stdlib.h>
#include "deucessh-key-algo.h"

#ifdef DSSH_CRYPTO_OPENSSL
#include "key_algo/key-context-openssl.h"
static inline dssh_key_algo_ctx *
test_empty_key_context(void)
{
	struct dssh_openssl_key_context *context = calloc(1, sizeof(*context));
	dssh_key_algo_ctx *opaque = (dssh_key_algo_ctx *)context;
	return opaque;
}
#else
/* Construct the actual native C++ type, never guess unique_ptr's layout. */
dssh_key_algo_ctx *test_empty_key_context(void);
int test_warm_native_key_context(dssh_key_algo_ctx *context, unsigned int slots);
#endif

#endif
