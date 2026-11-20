/* Private native C++ context; never included by installed module headers. */
#ifndef DSSH_KEY_CONTEXT_BOTAN_H
#define DSSH_KEY_CONTEXT_BOTAN_H

#include <memory>
#include <botan/pk_keys.h>
#include "signature-result.h"
#include "native-key-botan.h"

enum dssh_botan_key_kind {
	DSSH_BOTAN_KEY_NONE,
	DSSH_BOTAN_KEY_ED25519,
	DSSH_BOTAN_KEY_RSA,
};

struct dssh_botan_key_context {
	struct dssh_signature_pool *signatures = nullptr;
	/* Only unpublished cache construction uses this sole bootstrap owner. */
	std::unique_ptr<Botan::Private_Key> privkey;
	struct dssh_native_key_pool *keys = nullptr;
	dssh_botan_key_kind kind = DSSH_BOTAN_KEY_NONE;
	uint8_t *pub_blob = nullptr;
	size_t pub_blob_len = 0;
	~dssh_botan_key_context() noexcept
	{
		dssh_native_key_pool_destroy(keys);
	}
};

#endif
