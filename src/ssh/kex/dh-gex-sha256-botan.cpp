// RFC 4419: Diffie-Hellman Group Exchange with SHA-256 (Botan3 native C++ backend)

#include <botan/bigint.h>
#include <botan/numthry.h>
#include <botan/system_rng.h>

#include <cstdlib>
#include <cstring>

#include "deucessh-crypto.h"
#include "deucessh.h"
#include "dh-gex-sha256-ops.h"

static int64_t
parse_mp_mpint(const uint8_t *buf, size_t bufsz, Botan::BigInt *mp)
{
	if (bufsz < 4)
		return DSSH_ERROR_PARSE;

	uint32_t len;

	if (dssh_parse_uint32(buf, bufsz, &len) < 4)
		return DSSH_ERROR_PARSE;
	if (len > bufsz - 4)
		return DSSH_ERROR_PARSE;

	*mp = Botan::BigInt::from_bytes(std::span<const uint8_t>(&buf[4], len));
	return 4 + len;
}

static bool
dh_value_valid(const Botan::BigInt& val, const Botan::BigInt& p)
{
	if (val.is_zero())
		return false;
	if (val.is_negative())
		return false;
	if (val >= p)
		return false;
	return true;
}

struct botan_dhgex_ctx {
	Botan::BigInt p;
	Botan::BigInt g;
	Botan::BigInt x;
	Botan::BigInt our_pub;
};

static int
serialize_mp_to_mpint(const Botan::BigInt& bn, uint8_t **out, size_t *out_len)
{
	auto   raw      = bn.serialize();
	size_t bn_bytes = raw.size();

	bool need_pad = (bn_bytes > 0 && (raw[0] & DSSH_MPINT_SIGN_BIT));

	if (bn_bytes > UINT32_MAX - 1)
		return DSSH_ERROR_INVALID;
	uint32_t bn_u32         = static_cast<uint32_t>(bn_bytes);
	uint32_t mpint_data_len = bn_u32 + (need_pad ? 1 : 0);
	size_t   total          = 4 + mpint_data_len;

	uint8_t *buf = static_cast<uint8_t *>(malloc(total));

	if (buf == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos  = 0;
	int    sret = dssh_serialize_uint32(mpint_data_len, buf, total, &pos);

	if (sret < 0) {
		free(buf);
		return sret;
	}
	if (need_pad)
		buf[pos++] = 0;
	memcpy(&buf[pos], raw.data(), bn_bytes);

	*out     = buf;
	*out_len = total;
	return 0;
}

static int
mp_to_raw(const Botan::BigInt& bn, uint8_t **out, size_t *out_len)
{
	auto raw = bn.serialize();

	uint8_t *buf = static_cast<uint8_t *>(malloc(raw.size()));

	if (buf == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(buf, raw.data(), raw.size());

	*out     = buf;
	*out_len = raw.size();
	return 0;
}

static int
botan_client_keygen(const uint8_t *group_payload, size_t group_len, size_t *consumed, uint8_t **e_mpint,
    size_t *e_mpint_len, void **dh_ctx)
{
	Botan::BigInt p, g;

	int64_t pv = parse_mp_mpint(group_payload, group_len, &p);

	if (pv < 0)
		return static_cast<int>(pv);
#if SIZE_MAX < INT64_MAX
	if (pv > SIZE_MAX)
		return DSSH_ERROR_INVALID;
#endif
	size_t p_consumed = static_cast<size_t>(pv);

	if (p_consumed >= group_len)
		return DSSH_ERROR_PARSE;

	pv = parse_mp_mpint(group_payload + p_consumed, group_len - p_consumed, &g);
	if (pv < 0)
		return static_cast<int>(pv);
#if SIZE_MAX < INT64_MAX
	if (pv > SIZE_MAX)
		return DSSH_ERROR_INVALID;
#endif
	size_t g_consumed = static_cast<size_t>(pv);

	if (g_consumed > SIZE_MAX - p_consumed)
		return DSSH_ERROR_INVALID;
	*consumed = p_consumed + g_consumed;

	size_t p_bits = p.bits();

	if (p_bits < 2)
		return DSSH_ERROR_INVALID;
	size_t x_bits = p_bits - 1;

	Botan::BigInt x = Botan::BigInt::random_integer(Botan::system_rng(), Botan::BigInt::power_of_2(x_bits - 1),
	    Botan::BigInt::power_of_2(x_bits));

	Botan::BigInt e = Botan::power_mod(g, x, p);

	int ret = serialize_mp_to_mpint(e, e_mpint, e_mpint_len);
	if (ret < 0)
		return ret;

	botan_dhgex_ctx *ctx = new (std::nothrow) botan_dhgex_ctx;

	if (ctx == NULL) {
		free(*e_mpint);
		*e_mpint = NULL;
		return DSSH_ERROR_ALLOC;
	}
	ctx->p       = std::move(p);
	ctx->g       = std::move(g);
	ctx->x       = std::move(x);
	ctx->our_pub = std::move(e);
	*dh_ctx      = ctx;
	return 0;
}

static int
botan_client_derive(void *dh_ctx, const uint8_t *f_buf, size_t f_bufsz, int64_t *f_consumed, uint8_t **k_raw,
    size_t *k_raw_len, uint8_t **k_mpint, size_t *k_mpint_len)
{
	botan_dhgex_ctx *ctx = static_cast<botan_dhgex_ctx *>(dh_ctx);
	Botan::BigInt    f;

	int64_t pv = parse_mp_mpint(f_buf, f_bufsz, &f);

	if (pv < 0)
		return static_cast<int>(pv);
	*f_consumed = pv;

	if (!dh_value_valid(f, ctx->p))
		return DSSH_ERROR_INVALID;

	Botan::BigInt k = Botan::power_mod(f, ctx->x, ctx->p);

	ctx->x = 0;

	int ret = mp_to_raw(k, k_raw, k_raw_len);
	if (ret < 0)
		return ret;

	ret = serialize_mp_to_mpint(k, k_mpint, k_mpint_len);
	if (ret < 0) {
		dssh_cleanse(*k_raw, *k_raw_len);
		free(*k_raw);
		*k_raw = NULL;
		return ret;
	}

	return 0;
}

static int
botan_server_keygen(const uint8_t *p_bytes, size_t p_len, const uint8_t *g_bytes, size_t g_len, uint8_t **p_mpint,
    size_t *p_mpint_len, uint8_t **g_mpint, size_t *g_mpint_len, uint8_t **f_mpint, size_t *f_mpint_len,
    void **dh_ctx)
{
	Botan::BigInt p = Botan::BigInt::from_bytes(std::span<const uint8_t>(p_bytes, p_len));
	Botan::BigInt g = Botan::BigInt::from_bytes(std::span<const uint8_t>(g_bytes, g_len));

	size_t p_bits = p.bits();

	if (p_bits < 2)
		return DSSH_ERROR_INVALID;
	size_t y_bits = p_bits - 1;

	Botan::BigInt y = Botan::BigInt::random_integer(Botan::system_rng(), Botan::BigInt::power_of_2(y_bits - 1),
	    Botan::BigInt::power_of_2(y_bits));

	Botan::BigInt f = Botan::power_mod(g, y, p);

	int ret = serialize_mp_to_mpint(p, p_mpint, p_mpint_len);
	if (ret < 0)
		return ret;

	ret = serialize_mp_to_mpint(g, g_mpint, g_mpint_len);
	if (ret < 0) {
		free(*p_mpint);
		*p_mpint = NULL;
		return ret;
	}

	ret = serialize_mp_to_mpint(f, f_mpint, f_mpint_len);
	if (ret < 0) {
		free(*p_mpint);
		*p_mpint = NULL;
		free(*g_mpint);
		*g_mpint = NULL;
		return ret;
	}

	botan_dhgex_ctx *ctx = new (std::nothrow) botan_dhgex_ctx;

	if (ctx == NULL) {
		free(*p_mpint);
		*p_mpint = NULL;
		free(*g_mpint);
		*g_mpint = NULL;
		free(*f_mpint);
		*f_mpint = NULL;
		return DSSH_ERROR_ALLOC;
	}
	ctx->p       = std::move(p);
	ctx->g       = std::move(g);
	ctx->x       = std::move(y);
	ctx->our_pub = std::move(f);
	*dh_ctx      = ctx;
	return 0;
}

static int
botan_server_derive(void *dh_ctx, const uint8_t *e_buf, size_t e_bufsz, int64_t *e_consumed, uint8_t **k_raw,
    size_t *k_raw_len, uint8_t **k_mpint, size_t *k_mpint_len)
{
	botan_dhgex_ctx *ctx = static_cast<botan_dhgex_ctx *>(dh_ctx);
	Botan::BigInt    e;

	int64_t pv = parse_mp_mpint(e_buf, e_bufsz, &e);

	if (pv < 0)
		return static_cast<int>(pv);
	*e_consumed = pv;

	if (!dh_value_valid(e, ctx->p))
		return DSSH_ERROR_INVALID;

	Botan::BigInt k = Botan::power_mod(e, ctx->x, ctx->p);

	ctx->x = 0;

	int ret = mp_to_raw(k, k_raw, k_raw_len);
	if (ret < 0)
		return ret;

	ret = serialize_mp_to_mpint(k, k_mpint, k_mpint_len);
	if (ret < 0) {
		dssh_cleanse(*k_raw, *k_raw_len);
		free(*k_raw);
		*k_raw = NULL;
		return ret;
	}

	return 0;
}

static void
botan_free_ctx(void *dh_ctx)
{
	delete static_cast<botan_dhgex_ctx *>(dh_ctx);
}

static const struct dhgex_ops botan_dhgex_ops = {
    .client_keygen = botan_client_keygen,
    .client_derive = botan_client_derive,
    .server_keygen = botan_server_keygen,
    .server_derive = botan_server_derive,
    .free_ctx      = botan_free_ctx,
};

extern "C" int
dssh_botan_dhgex_handler(struct dssh_kex_context *kctx)
{
	try {
		return dhgex_handler_impl(kctx, &botan_dhgex_ops);
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

extern "C" void
dssh_botan_dhgex_cleanup(void *kex_data)
{
	(void)kex_data;
}
