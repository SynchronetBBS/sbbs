/* RFC 4419: retained native DH containers; fresh exponent for every exchange. */
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <stdlib.h>
#include <string.h>
#include "deucessh-algorithms.h"
#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#include "dh-gex-groups.h"
#include "dh-gex-sha256-ops.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

DSSH_TESTABLE int
dhgex_validate_group(const BIGNUM *p, const BIGNUM *g, BN_CTX *bnctx,
    const uint8_t *p_raw, size_t p_len, const uint8_t *g_raw, size_t g_len)
{
	int bits = BN_num_bits(p);
	if (bits < GEX_MIN || bits > GEX_MAX || !BN_is_odd(p)
	    || BN_cmp(g, BN_value_one()) <= 0)
		return DSSH_ERROR_INVALID;
	BIGNUM *limit = BN_dup(p);
	if (limit == NULL)
		return DSSH_ERROR_ALLOC;
	int ret = DSSH_ERROR_INVALID;
	if (BN_sub_word(limit, 1) != 1) {
		ret = DSSH_ERROR_INIT;
		goto done;
	}
	if (BN_cmp(g, limit) >= 0)
		goto done;
	if (dssh_dhgex_group_known(p_raw, p_len, g_raw, g_len)) {
		ret = 0;
		goto done;
	}
	if (BN_rshift1(limit, limit) != 1) {
		ret = DSSH_ERROR_INIT;
		goto done;
	}
	if (BN_check_prime(p, bnctx, NULL) != 1 || BN_check_prime(limit, bnctx, NULL) != 1)
		goto done;
	ret = 0;
done:
	BN_free(limit);
	return ret;
}

struct ossl_dhgex_ctx {
	BIGNUM *p;
	BIGNUM *g;
	BIGNUM *exponent;
	BIGNUM *public_key;
	BIGNUM *peer;
	BIGNUM *secret;
	BIGNUM *range;
	bool ready;
};

static void
ossl_reset(void *opaque)
{
	struct ossl_dhgex_ctx *state = opaque;
	if (state == NULL)
		return;
	/* BN_clear erases the owned native capacity while retaining its allocation. */
	BN_clear(state->exponent);
	BN_clear(state->secret);
	state->ready = false;
}

static void
ossl_cleanup(void *opaque)
{
	struct ossl_dhgex_ctx *state = opaque;
	if (state == NULL)
		return;
	BN_free(state->p);
	BN_free(state->g);
	BN_free(state->public_key);
	BN_free(state->peer);
	BN_free(state->range);
	BN_clear_free(state->exponent);
	BN_clear_free(state->secret);
	dssh_cleanse(state, sizeof(*state));
	free(state);
}

static int
ossl_init(void **out)
{
	struct ossl_dhgex_ctx *state = malloc(sizeof(*state));
	if (state == NULL)
		return DSSH_ERROR_ALLOC;
	*state = (struct ossl_dhgex_ctx){0};
	state->p = BN_new();
	state->g = BN_new();
	state->exponent = BN_new();
	state->public_key = BN_new();
	state->peer = BN_new();
	state->secret = BN_new();
	state->range = BN_new();
	if (state->p == NULL || state->g == NULL || state->exponent == NULL
	    || state->public_key == NULL || state->peer == NULL || state->secret == NULL || state->range == NULL) {
		ossl_cleanup(state);
		return DSSH_ERROR_ALLOC;
	}
	BN_set_flags(state->exponent, BN_FLG_CONSTTIME);
	*out = state;
	return 0;
}

static int
write_raw(const BIGNUM *value, uint8_t *out, size_t capacity, size_t *length)
{
	int count = BN_num_bytes(value);
	if (count <= 0 || count > DHGEX_RAW_MAX)
		return DSSH_ERROR_INVALID;
	size_t size = (size_t)count; /* Positive and capped at 1024. */
	if (size > capacity)
		return DSSH_ERROR_TOOLONG;
	if (BN_bn2bin(value, out) != count)
		return DSSH_ERROR_INIT;
	*length = size;
	return 0;
}

static int
ossl_keygen(void *opaque, const uint8_t *p, size_t plen, const uint8_t *g, size_t glen,
    uint8_t *public_key, size_t capacity, size_t *length)
{
	struct ossl_dhgex_ctx *state = opaque;
	if (state == NULL || p == NULL || g == NULL || public_key == NULL || length == NULL
	    || plen == 0 || glen == 0 || plen > DHGEX_RAW_MAX || glen > DHGEX_RAW_MAX)
		return DSSH_ERROR_INVALID;
	ossl_reset(state);
	int psize = (int)plen; /* At most 1024, fits the smallest C17 int. */
	int gsize = (int)glen;
	if (BN_bin2bn(p, psize, state->p) == NULL || BN_bin2bn(g, gsize, state->g) == NULL)
		return DSSH_ERROR_ALLOC;
	/* Per-operation arithmetic scratch is retired through its native owner.
	 * BN_CTX_end alone does not erase all cached intermediate bignums. */
	BN_CTX *scratch = BN_CTX_new();
	if (scratch == NULL)
		return DSSH_ERROR_ALLOC;
	int ret = dhgex_validate_group(state->p, state->g, scratch, p, plen, g, glen);
	if (ret < 0)
		goto done;
	/* OpenSSL 3.0 releases ignore the requested strength on one range-
	 * sampling path. Check the same default private DRBG used by scratch
	 * before accepting any exponent from BN_priv_rand_range_ex. */
	EVP_RAND_CTX *random = RAND_get0_private(NULL);
	if (random == NULL || EVP_RAND_get_strength(random) < 256) {
		ret = DSSH_ERROR_INIT;
		goto done;
	}
	if (BN_rshift1(state->range, state->p) != 1 || BN_sub_word(state->range, 2) != 1
	    || BN_priv_rand_range_ex(state->exponent, state->range, 256, scratch) != 1
	    || BN_add_word(state->exponent, 2) != 1) {
		ret = DSSH_ERROR_INIT;
		goto done;
	}
	if (BN_mod_exp(state->public_key, state->g, state->exponent, state->p, scratch) != 1) {
		ret = DSSH_ERROR_INIT;
		goto done;
	}
	ret = write_raw(state->public_key, public_key, capacity, length);
	if (ret == 0)
		state->ready = true;
done:
	BN_CTX_free(scratch);
	if (ret < 0)
		ossl_reset(state);
	return ret;
}

static int
ossl_derive(void *opaque, const uint8_t *peer, size_t peerlen,
    uint8_t *secret, size_t capacity, size_t *length)
{
	struct ossl_dhgex_ctx *state = opaque;
	if (state == NULL || !state->ready || peer == NULL || secret == NULL || length == NULL
	    || peerlen == 0 || peerlen > DHGEX_RAW_MAX)
		return DSSH_ERROR_INVALID;
	int count = (int)peerlen; /* At most 1024. */
	int ret = DSSH_ERROR_ALLOC;
	BN_CTX *scratch = NULL;
	if (BN_bin2bn(peer, count, state->peer) == NULL)
		goto done;
	if (BN_is_zero(state->peer) || BN_is_negative(state->peer) || BN_cmp(state->peer, state->p) >= 0) {
		ret = DSSH_ERROR_INVALID;
		goto done;
	}
	scratch = BN_CTX_new();
	if (scratch == NULL)
		goto done;
	if (BN_mod_exp(state->secret, state->peer, state->exponent, state->p, scratch) != 1) {
		ret = DSSH_ERROR_INIT;
		goto done;
	}
	/* The shared protocol validates the raw K range before hashing/publication. */
	ret = write_raw(state->secret, secret, capacity, length);
done:
	BN_CTX_free(scratch);
	ossl_reset(state);
	return ret;
}

DSSH_PRIVATE const struct dhgex_ops dssh_dhgex_backend = {
	.init = ossl_init, .reset = ossl_reset, .cleanup = ossl_cleanup,
	.keygen = ossl_keygen, .derive = ossl_derive,
};

DSSH_TESTABLE int
dhgex_handler(const struct dssh_kex_context *ctx, struct dssh_kex_result *out)
{
	return dhgex_handler_impl(ctx, out, &dssh_dhgex_backend);
}

DSSH_PUBLIC int
dssh_register_dh_gex_sha256(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + DHGEX_KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next      = NULL;
	kex->init      = dhgex_init;
	kex->handler   = dhgex_handler;
	kex->cleanup   = dhgex_cleanup;
	kex->flags     = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE;
	kex->hash_name = "SHA-256";
	kex->config       = &default_provider;
	memcpy(kex->name, DHGEX_KEX_NAME, DHGEX_KEX_NAME_LEN + 1);
	int ret = dssh_transport_register_kex(kex);
	if (ret < 0)
		free(kex);
	return ret;
}
