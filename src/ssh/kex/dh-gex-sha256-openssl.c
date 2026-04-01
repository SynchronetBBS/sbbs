// RFC 4419: Diffie-Hellman Group Exchange with SHA-256 — OpenSSL backend

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <stdlib.h>
#include <string.h>

#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "deucessh.h"
#include "kex/dh-gex-groups.h"
#include "kex/dh-gex-sha256-ops.h"
#include "kex/dh-gex-sha256.h"
#ifdef DSSH_TESTING
 #include "ssh-internal.h"
#endif

/* ----------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------- */

static int64_t
parse_bn_mpint(const uint8_t *buf, size_t bufsz, BIGNUM **bn)
{
	if (bufsz < 4)
		return DSSH_ERROR_PARSE;

	uint32_t len;

	if (dssh_parse_uint32(buf, bufsz, &len) < 4)
		return DSSH_ERROR_PARSE;
	if (len > bufsz - 4)
		return DSSH_ERROR_PARSE;
	if (len > INT_MAX)
		return DSSH_ERROR_INVALID;
	int ilen = (int)len;

	*bn = BN_bin2bn(&buf[4], ilen, NULL);
	if (*bn == NULL)
		return DSSH_ERROR_ALLOC;
	return 4 + len;
}

/*
 * RFC 4253 s8: Values of e or f not in [1, p-1] MUST NOT be
 * sent or accepted.
 */
static bool
dh_value_valid(const BIGNUM *val, const BIGNUM *p)
{
	if (BN_is_zero(val) || BN_is_negative(val))
		return false;
	if (BN_cmp(val, p) >= 0)
		return false;
	return true;
}

/* ----------------------------------------------------------------
 * Internal DH context for the ops vtable
 * ---------------------------------------------------------------- */

struct ossl_dhgex_ctx {
	BIGNUM *p;
	BIGNUM *g;
	BIGNUM *x;       /* secret exponent (client or server) */
	BIGNUM *our_pub; /* e (client) or f (server) */
	BN_CTX *bnctx;
};

/* ----------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------- */

/*
 * Serialize a BIGNUM to a malloc'd SSH mpint byte array
 * (4-byte length prefix + data with sign-bit padding).
 * Returns 0 on success, negative DSSH_ERROR_* on failure.
 */
static int
serialize_bn_to_mpint(const BIGNUM *bn, uint8_t **out, size_t *out_len)
{
	int bn_bytes = BN_num_bytes(bn);

	if (bn_bytes < 0)
		return DSSH_ERROR_PARSE;
#if INT_MAX > SIZE_MAX
	if (bn_bytes > SIZE_MAX)
		return DSSH_ERROR_INVALID;
#endif
	size_t   bn_sz = (size_t)bn_bytes;
	uint8_t *tmp   = malloc(bn_sz);

	if (tmp == NULL)
		return DSSH_ERROR_ALLOC;
	BN_bn2bin(bn, tmp);

	bool need_pad = (bn_bytes > 0 && (tmp[0] & DSSH_MPINT_SIGN_BIT));

	if (bn_sz > UINT32_MAX - 1) {
		OPENSSL_cleanse(tmp, bn_sz);
		free(tmp);
		return DSSH_ERROR_INVALID;
	}
	uint32_t bn_u32    = (uint32_t)bn_sz;
	uint32_t mpint_len = bn_u32 + (need_pad ? 1 : 0);

	/* Total wire size: 4-byte length prefix + mpint_len */
#if SIZE_MAX < UINT32_MAX
	if (mpint_len > SIZE_MAX - 4) {
		OPENSSL_cleanse(tmp, bn_sz);
		free(tmp);
		return DSSH_ERROR_INVALID;
	}
#endif
	size_t total = 4 + (size_t)mpint_len;

	uint8_t *buf = malloc(total);
	if (buf == NULL) {
		OPENSSL_cleanse(tmp, bn_sz);
		free(tmp);
		return DSSH_ERROR_ALLOC;
	}

	size_t pos = 0;
	int    ret = dssh_serialize_uint32(mpint_len, buf, total, &pos);

	if (ret < 0) {
		OPENSSL_cleanse(tmp, bn_sz);
		free(tmp);
		free(buf);
		return ret;
	}
	if (need_pad)
		buf[pos++] = 0;
	memcpy(&buf[pos], tmp, bn_sz);
	pos += bn_sz;

	OPENSSL_cleanse(tmp, bn_sz);
	free(tmp);

	*out     = buf;
	*out_len = pos;
	return 0;
}

/*
 * Extract raw big-endian bytes from a BIGNUM (for shared_secret).
 * Returns 0 on success.  Output is malloc'd; caller frees.
 */
static int
bn_to_raw(const BIGNUM *bn, uint8_t **out, size_t *out_len)
{
	int bn_bytes = BN_num_bytes(bn);

	if (bn_bytes < 0)
		return DSSH_ERROR_PARSE;
#if INT_MAX > SIZE_MAX
	if (bn_bytes > SIZE_MAX)
		return DSSH_ERROR_INVALID;
#endif
	size_t bn_sz = (size_t)bn_bytes;

	uint8_t *buf = malloc(bn_sz);
	if (buf == NULL)
		return DSSH_ERROR_ALLOC;
	BN_bn2bin(bn, buf);

	*out     = buf;
	*out_len = bn_sz;
	return 0;
}

/* ----------------------------------------------------------------
 * Ops vtable implementations
 * ---------------------------------------------------------------- */

static int
ossl_client_keygen(const uint8_t *group_payload, size_t group_len, size_t *consumed, uint8_t **e_mpint,
    size_t *e_mpint_len, void **dh_ctx)
{
	BIGNUM *p     = NULL;
	BIGNUM *g     = NULL;
	BIGNUM *x     = NULL;
	BIGNUM *e     = NULL;
	BN_CTX *bnctx = NULL;
	int     ret;

	/* Parse p from group payload */
	int64_t pv = parse_bn_mpint(group_payload, group_len, &p);
	if (pv < 0) {
		ret = (int)pv;
		return ret;
	}
#if SIZE_MAX < INT64_MAX
	if (pv > SIZE_MAX) {
		BN_free(p);
		return DSSH_ERROR_INVALID;
	}
#endif
	size_t p_sz = (size_t)pv;

	if (p_sz >= group_len) {
		BN_free(p);
		return DSSH_ERROR_PARSE;
	}

	/* Parse g from group payload */
	pv = parse_bn_mpint(&group_payload[p_sz], group_len - p_sz, &g);
	if (pv < 0) {
		BN_free(p);
		ret = (int)pv;
		return ret;
	}
#if SIZE_MAX < INT64_MAX
	if (pv > SIZE_MAX) {
		BN_free(p);
		BN_free(g);
		return DSSH_ERROR_INVALID;
	}
#endif
	size_t g_sz = (size_t)pv;

	if (g_sz > SIZE_MAX - p_sz) {
		BN_free(p);
		BN_free(g);
		return DSSH_ERROR_INVALID;
	}
	*consumed = p_sz + g_sz;

	/* Generate x, compute e = g^x mod p */
	bnctx = BN_CTX_new();
	if (bnctx == NULL) {
		ret = DSSH_ERROR_ALLOC;
		goto fail;
	}
	e = BN_new();
	if (e == NULL) {
		ret = DSSH_ERROR_ALLOC;
		goto fail;
	}
	x = BN_new();
	if (x == NULL) {
		ret = DSSH_ERROR_ALLOC;
		goto fail;
	}
	if (BN_rand(x, BN_num_bits(p) - 1, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY) != 1) {
		ret = DSSH_ERROR_INIT;
		goto fail;
	}
	if (BN_mod_exp(e, g, x, p, bnctx) != 1) {
		ret = DSSH_ERROR_INIT;
		goto fail;
	}

	/* Serialize e to mpint */
	ret = serialize_bn_to_mpint(e, e_mpint, e_mpint_len);
	if (ret < 0)
		goto fail;

	/* Build context for client_derive */
	struct ossl_dhgex_ctx *ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		free(*e_mpint);
		*e_mpint = NULL;
		ret      = DSSH_ERROR_ALLOC;
		goto fail;
	}
	ctx->p       = p;
	ctx->g       = g;
	ctx->x       = x;
	ctx->our_pub = e;
	ctx->bnctx   = bnctx;
	*dh_ctx      = ctx;
	return 0;

fail:
	BN_clear_free(x);
	BN_free(e);
	BN_free(p);
	BN_free(g);
	BN_CTX_free(bnctx);
	return ret;
}

static int
ossl_client_derive(void *dh_ctx, const uint8_t *f_buf, size_t f_bufsz, int64_t *f_consumed, uint8_t **k_raw,
    size_t *k_raw_len, uint8_t **k_mpint, size_t *k_mpint_len)
{
	struct ossl_dhgex_ctx *ctx = dh_ctx;
	BIGNUM                *f   = NULL;
	BIGNUM                *k   = NULL;
	int                    ret;

	/* Parse f */
	int64_t pv = parse_bn_mpint(f_buf, f_bufsz, &f);
	if (pv < 0) {
		ret = (int)pv;
		return ret;
	}
	*f_consumed = pv;

	/* Validate f in [1, p-1] */
	if (!dh_value_valid(f, ctx->p)) {
		BN_free(f);
		return DSSH_ERROR_INVALID;
	}

	/* Compute K = f^x mod p */
	k = BN_new();
	if (k == NULL) {
		BN_free(f);
		return DSSH_ERROR_ALLOC;
	}
	if (BN_mod_exp(k, f, ctx->x, ctx->p, ctx->bnctx) != 1) {
		BN_free(f);
		BN_clear_free(k);
		return DSSH_ERROR_INIT;
	}
	BN_free(f);

	/* Clear secret exponent */
	BN_clear_free(ctx->x);
	ctx->x = NULL;

	/* Extract raw K for shared_secret */
	ret = bn_to_raw(k, k_raw, k_raw_len);
	if (ret < 0) {
		BN_clear_free(k);
		return ret;
	}

	/* Serialize K to mpint for hash */
	ret = serialize_bn_to_mpint(k, k_mpint, k_mpint_len);
	BN_clear_free(k);
	if (ret < 0) {
		OPENSSL_cleanse(*k_raw, *k_raw_len);
		free(*k_raw);
		*k_raw = NULL;
		return ret;
	}

	return 0;
}

static int
ossl_server_keygen(const uint8_t *p_bytes, size_t p_len, const uint8_t *g_bytes, size_t g_len, uint8_t **p_mpint,
    size_t *p_mpint_len, uint8_t **g_mpint, size_t *g_mpint_len, uint8_t **f_mpint, size_t *f_mpint_len,
    void **dh_ctx)
{
	BIGNUM *p     = NULL;
	BIGNUM *g     = NULL;
	BIGNUM *y     = NULL;
	BIGNUM *f     = NULL;
	BN_CTX *bnctx = NULL;
	int     ret;

	if (p_len > INT_MAX || g_len > INT_MAX)
		return DSSH_ERROR_INVALID;
	int p_ilen = (int)p_len;
	int g_ilen = (int)g_len;

	p = BN_bin2bn(p_bytes, p_ilen, NULL);
	if (p == NULL)
		return DSSH_ERROR_ALLOC;
	g = BN_bin2bn(g_bytes, g_ilen, NULL);
	if (g == NULL) {
		BN_free(p);
		return DSSH_ERROR_ALLOC;
	}

	bnctx = BN_CTX_new();
	if (bnctx == NULL) {
		ret = DSSH_ERROR_ALLOC;
		goto fail;
	}
	f = BN_new();
	if (f == NULL) {
		ret = DSSH_ERROR_ALLOC;
		goto fail;
	}
	y = BN_new();
	if (y == NULL) {
		ret = DSSH_ERROR_ALLOC;
		goto fail;
	}

	/* Generate y, compute f = g^y mod p */
	if (BN_rand(y, BN_num_bits(p) - 1, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY) != 1) {
		ret = DSSH_ERROR_INIT;
		goto fail;
	}
	if (BN_mod_exp(f, g, y, p, bnctx) != 1) {
		ret = DSSH_ERROR_INIT;
		goto fail;
	}

	/* Serialize p, g, f to mpint */
	ret = serialize_bn_to_mpint(p, p_mpint, p_mpint_len);
	if (ret < 0)
		goto fail;
	ret = serialize_bn_to_mpint(g, g_mpint, g_mpint_len);
	if (ret < 0) {
		free(*p_mpint);
		*p_mpint = NULL;
		goto fail;
	}
	ret = serialize_bn_to_mpint(f, f_mpint, f_mpint_len);
	if (ret < 0) {
		free(*p_mpint);
		*p_mpint = NULL;
		free(*g_mpint);
		*g_mpint = NULL;
		goto fail;
	}

	/* Build context for server_derive: store y as x */
	struct ossl_dhgex_ctx *ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		free(*p_mpint);
		*p_mpint = NULL;
		free(*g_mpint);
		*g_mpint = NULL;
		free(*f_mpint);
		*f_mpint = NULL;
		ret      = DSSH_ERROR_ALLOC;
		goto fail;
	}
	ctx->p       = p;
	ctx->g       = g;
	ctx->x       = y;
	ctx->our_pub = f;
	ctx->bnctx   = bnctx;
	*dh_ctx      = ctx;
	return 0;

fail:
	BN_clear_free(y);
	BN_free(f);
	BN_free(p);
	BN_free(g);
	BN_CTX_free(bnctx);
	return ret;
}

static int
ossl_server_derive(void *dh_ctx, const uint8_t *e_buf, size_t e_bufsz, int64_t *e_consumed, uint8_t **k_raw,
    size_t *k_raw_len, uint8_t **k_mpint, size_t *k_mpint_len)
{
	struct ossl_dhgex_ctx *ctx = dh_ctx;
	BIGNUM                *e   = NULL;
	BIGNUM                *k   = NULL;
	int                    ret;

	/* Parse e */
	int64_t pv = parse_bn_mpint(e_buf, e_bufsz, &e);
	if (pv < 0) {
		ret = (int)pv;
		return ret;
	}
	*e_consumed = pv;

	/* Validate e in [1, p-1] */
	if (!dh_value_valid(e, ctx->p)) {
		BN_free(e);
		return DSSH_ERROR_INVALID;
	}

	/* Compute K = e^y mod p */
	k = BN_new();
	if (k == NULL) {
		BN_free(e);
		return DSSH_ERROR_ALLOC;
	}
	if (BN_mod_exp(k, e, ctx->x, ctx->p, ctx->bnctx) != 1) {
		BN_free(e);
		BN_clear_free(k);
		return DSSH_ERROR_INIT;
	}
	BN_free(e);

	/* Clear secret exponent */
	BN_clear_free(ctx->x);
	ctx->x = NULL;

	/* Extract raw K for shared_secret */
	ret = bn_to_raw(k, k_raw, k_raw_len);
	if (ret < 0) {
		BN_clear_free(k);
		return ret;
	}

	/* Serialize K to mpint for hash */
	ret = serialize_bn_to_mpint(k, k_mpint, k_mpint_len);
	BN_clear_free(k);
	if (ret < 0) {
		OPENSSL_cleanse(*k_raw, *k_raw_len);
		free(*k_raw);
		*k_raw = NULL;
		return ret;
	}

	return 0;
}

static void
ossl_free_ctx(void *dh_ctx)
{
	if (dh_ctx == NULL)
		return;

	struct ossl_dhgex_ctx *ctx = dh_ctx;

	BN_free(ctx->p);
	BN_free(ctx->g);
	BN_free(ctx->our_pub);
	BN_clear_free(ctx->x);
	BN_CTX_free(ctx->bnctx);
	free(ctx);
}

/* ----------------------------------------------------------------
 * Static ops struct
 * ---------------------------------------------------------------- */

static const struct dhgex_ops ossl_dhgex_ops = {
    .client_keygen = ossl_client_keygen,
    .client_derive = ossl_client_derive,
    .server_keygen = ossl_server_keygen,
    .server_derive = ossl_server_derive,
    .free_ctx      = ossl_free_ctx,
};

/* ----------------------------------------------------------------
 * KEX handler wrapper and registration
 * ---------------------------------------------------------------- */

DSSH_TESTABLE int
dhgex_handler(struct dssh_kex_context *kctx)
{
	return dhgex_handler_impl(kctx, &ossl_dhgex_ops);
}

static void
kex_cleanup(void *kex_data)
{
	(void)kex_data;
}

DSSH_PUBLIC int
dssh_register_dh_gex_sha256(void)
{
	struct dssh_kex_s *kex = malloc(sizeof(*kex) + DHGEX_KEX_NAME_LEN + 1);

	if (kex == NULL)
		return DSSH_ERROR_ALLOC;
	kex->next      = NULL;
	kex->handler   = dhgex_handler;
	kex->cleanup   = kex_cleanup;
	kex->flags     = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE;
	kex->hash_name = "SHA-256";
	kex->ctx       = &default_provider;
	memcpy(kex->name, DHGEX_KEX_NAME, DHGEX_KEX_NAME_LEN + 1);
	return dssh_transport_register_kex(kex);
}
