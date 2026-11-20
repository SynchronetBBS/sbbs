#ifndef DSSH_TEST_KEX_RESULT_FIXTURE_H
#define DSSH_TEST_KEX_RESULT_FIXTURE_H

#include "deucessh-kex.h"

static inline int
test_kex_stateless_init(bool client, void *config, void **state)
{
	(void)client;
	(void)config;
	if (state == NULL || *state != NULL)
		return DSSH_ERROR_INVALID;
	return 0;
}

static inline void
test_kex_stateless_cleanup(void *state)
{
	(void)state;
}

static inline void
test_kex_static_release(void *owner)
{
	(void)owner;
}

/* Valid metadata for registry/ownership scaffolding, not a KEX math oracle. */
static inline int
test_kex_static_handler(const struct dssh_kex_context *ctx, struct dssh_kex_result *out)
{
	(void)ctx;
	static const uint8_t secret[32] = {1};
	static const uint8_t hash[32] = {2};
	out->shared_secret = (struct dssh_bytes){secret, sizeof(secret)};
	out->exchange_hash = (struct dssh_bytes){hash, sizeof(hash)};
	out->lease.release = test_kex_static_release;
	return 0;
}

#endif
