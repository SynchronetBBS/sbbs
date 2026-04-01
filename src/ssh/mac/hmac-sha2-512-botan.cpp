/*
 * mac/hmac-sha2-512-botan.cpp -- HMAC-SHA-512 via Botan3 native C++ API.
 */

#include <cstdlib>
#include <memory>

#include <botan/mac.h>

#include "deucessh.h"

#define HMAC_SHA2_512_KEY_SIZE 64

struct dssh_mac_ctx {
	std::unique_ptr<Botan::MessageAuthenticationCode> mac;
};

extern "C" int
dssh_botan_hmac512_init(const uint8_t *key, dssh_mac_ctx **out)
{
	try {
		auto mac = Botan::MessageAuthenticationCode::create_or_throw(
		    "HMAC(SHA-512)");
		mac->set_key(key, HMAC_SHA2_512_KEY_SIZE);

		dssh_mac_ctx *cbd = new (std::nothrow) dssh_mac_ctx;

		if (cbd == NULL)
			return DSSH_ERROR_ALLOC;
		cbd->mac = std::move(mac);
		*out = cbd;
		return 0;
	} catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" int
dssh_botan_hmac512_generate(const uint8_t *buf, size_t bufsz,
    uint8_t *outbuf, dssh_mac_ctx *ctx)
{
	if (ctx == NULL)
		return DSSH_ERROR_INIT;

	try {
		ctx->mac->update(buf, bufsz);
		ctx->mac->final(outbuf);
		return 0;
	} catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" void
dssh_botan_hmac512_cleanup(dssh_mac_ctx *ctx)
{
	try {
		delete ctx;
	} catch (...) {
	}
}
