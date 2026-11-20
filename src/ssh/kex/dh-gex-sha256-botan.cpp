/* RFC 4419: cached public group, fresh native DH key for every exchange. */
#include <botan/bigint.h>
#include <botan/dh.h>
#include <botan/dl_group.h>
#include <botan/numthry.h>
#include <botan/pubkey.h>
#include <botan/system_rng.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <vector>
#include "deucessh-crypto.h"
#include "deucessh.h"
#include "dh-gex-sha256-ops.h"
#include "dh-gex-groups.h"

struct botan_dhgex_ctx {
	Botan::BigInt p;
	std::vector<uint8_t> p_bytes;
	std::vector<uint8_t> g_bytes;
	std::unique_ptr<Botan::DL_Group> group;
	std::unique_ptr<Botan::DH_PrivateKey> key;
};

static void
botan_reset(void *opaque)
{
	auto *state = static_cast<botan_dhgex_ctx *>(opaque);
	if (state != nullptr)
		state->key.reset();
}

static void
botan_cleanup(void *opaque)
{
	delete static_cast<botan_dhgex_ctx *>(opaque);
}

static int
botan_init(void **out)
{
	try {
		auto state = std::make_unique<botan_dhgex_ctx>();
		*out = state.release();
		return 0;
	}
	catch (const std::bad_alloc&) {
		return DSSH_ERROR_ALLOC;
	}
	catch (...) {
		return DSSH_ERROR_INIT;
	}
}

static bool
group_valid(const Botan::BigInt& p, const Botan::BigInt& g,
    const uint8_t *p_raw, size_t plen, const uint8_t *g_raw, size_t glen)
{
	if (p.bits() < GEX_MIN || p.bits() > GEX_MAX || !p.is_odd() || g <= 1 || g >= p - 1)
		return false;
	if (dssh_dhgex_group_known(p_raw, plen, g_raw, glen))
		return true;
	Botan::BigInt q = p >> 1;
	return Botan::is_prime(p, Botan::system_rng(), 128)
	    && Botan::is_prime(q, Botan::system_rng(), 128);
}

static int
botan_keygen(void *opaque, const uint8_t *p_raw, size_t plen, const uint8_t *g_raw, size_t glen,
    uint8_t *public_key, size_t capacity, size_t *length)
{
	auto *state = static_cast<botan_dhgex_ctx *>(opaque);
	if (state == nullptr || p_raw == nullptr || g_raw == nullptr || public_key == nullptr || length == nullptr
	    || plen == 0 || glen == 0 || plen > DHGEX_RAW_MAX || glen > DHGEX_RAW_MAX)
		return DSSH_ERROR_INVALID;
	botan_reset(state);
	try {
		if (state->group == nullptr || state->p_bytes.size() != plen || state->g_bytes.size() != glen
		    || !std::equal(state->p_bytes.begin(), state->p_bytes.end(), p_raw)
		    || !std::equal(state->g_bytes.begin(), state->g_bytes.end(), g_raw)) {
			/* A failed replacement must never make a mixed cache usable. */
			state->group.reset();
			Botan::BigInt p = Botan::BigInt::from_bytes(std::span<const uint8_t>(p_raw, plen));
			Botan::BigInt g = Botan::BigInt::from_bytes(std::span<const uint8_t>(g_raw, glen));
			if (!group_valid(p, g, p_raw, plen, g_raw, glen))
				return DSSH_ERROR_INVALID;
			auto group = std::make_unique<Botan::DL_Group>(p, g);
			state->p_bytes.assign(p_raw, p_raw + plen);
			state->g_bytes.assign(g_raw, g_raw + glen);
			state->p = std::move(p);
			state->group = std::move(group);
		}
		state->key = std::make_unique<Botan::DH_PrivateKey>(Botan::system_rng(), *state->group);
		const auto& exponent = state->key->get_int_field("x");
		if (exponent <= 1 || exponent >= (state->p >> 1)) {
			botan_reset(state);
			return DSSH_ERROR_INIT;
		}
		const auto& value = state->key->get_int_field("y");
		size_t bytes = value.bytes();
		if (bytes == 0 || bytes > DHGEX_RAW_MAX || bytes > capacity) {
			botan_reset(state);
			return DSSH_ERROR_INVALID;
		}
		value.serialize_to(std::span<uint8_t>(public_key, bytes));
		*length = bytes;
		return 0;
	}
	catch (const std::bad_alloc&) {
		botan_reset(state);
		return DSSH_ERROR_ALLOC;
	}
	catch (...) {
		botan_reset(state);
		return DSSH_ERROR_INIT;
	}
}

static int
botan_derive(void *opaque, const uint8_t *peer, size_t peerlen,
    uint8_t *secret, size_t capacity, size_t *length)
{
	auto *state = static_cast<botan_dhgex_ctx *>(opaque);
	if (state == nullptr || state->key == nullptr || peer == nullptr || secret == nullptr || length == nullptr
	    || peerlen == 0 || peerlen > DHGEX_RAW_MAX)
		return DSSH_ERROR_INVALID;
	int ret = DSSH_ERROR_INIT;
	try {
		Botan::BigInt value = Botan::BigInt::from_bytes(std::span<const uint8_t>(peer, peerlen));
		if (value < 1 || value >= state->p)
			ret = DSSH_ERROR_INVALID;
		else {
			Botan::PK_Key_Agreement agreement(*state->key, Botan::system_rng(), "Raw");
			auto shared = agreement.derive_key(0, peer, peerlen);
			size_t bytes = shared.length();
			if (bytes == 0 || bytes > DHGEX_RAW_MAX || bytes > capacity)
				ret = DSSH_ERROR_INVALID;
			else {
				/* Borrow native secure bytes directly; no bits_of/BigInt/mpint copy. */
				memcpy(secret, shared.begin(), bytes);
				*length = bytes;
				ret = 0;
			}
		}
	}
	catch (const std::bad_alloc&) {
		ret = DSSH_ERROR_ALLOC;
	}
	catch (...) {
		ret = DSSH_ERROR_INIT;
	}
	botan_reset(state);
	return ret;
}

extern "C" DSSH_PRIVATE const struct dhgex_ops dssh_dhgex_backend = {
	.init = botan_init, .reset = botan_reset, .cleanup = botan_cleanup,
	.keygen = botan_keygen, .derive = botan_derive,
};

extern "C" int
dssh_botan_dhgex_handler(const struct dssh_kex_context *ctx, struct dssh_kex_result *out)
{
	return dhgex_handler_impl(ctx, out, &dssh_dhgex_backend);
}
