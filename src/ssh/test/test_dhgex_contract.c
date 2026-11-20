/* Shared DH-GEX protocol/ownership tests. Deterministic private operations
 * isolate the protocol; real bignum/provider tests are separate evidence. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dssh_test.h"
#include "kex/workspace.h"
#include "dssh_test_alloc.h"
#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "kex/dh-gex-sha256.h"
#include "kex/dh-gex-sha256-ops.h"
#include "kex/dh-gex-groups.h"

struct allocation {
	void *ptr;
	size_t length;
	bool secret;
};

struct gex_fixture {
	uint8_t signature_storage;
	struct allocation owned[24];
	unsigned count;
	unsigned live;
	unsigned uncleared;
	unsigned providers;
	unsigned group_start;
	unsigned group_end;
	unsigned group_releases;
	struct dssh_result_lease *group_lease;
	uint8_t group_inline[256];
	uint8_t group_generator;
	unsigned keygens;
	unsigned derives;
	unsigned signs;
	unsigned verifies;
	unsigned sends;
	unsigned receives;
	unsigned ctx_frees;
	unsigned native_mode;
	unsigned native_inits;
	unsigned native_cleanups;
	unsigned secret_shape;
	uint8_t *secret_storage;
	size_t secret_capacity;
	unsigned provider_shape;
	unsigned provider_mask;
	int provider_result;
	unsigned operation;
	unsigned partial_mask;
	int operation_result;
	unsigned wire_target;
	unsigned wire_shape;
	unsigned host_fault;
	int host_result;
	uint8_t message[1100];
	bool client;
	bool bad;
};

#include "auth_result_fixture.h"

static struct gex_fixture *active;

/* Independently serialized 332-byte RFC 4419 transcript; the oracle script
 * and complete bytes are retained in build-audit-logs/dhgex-transcript-oracle.*.
 * This is a protocol hash answer, not a DH primitive known answer. */
static const uint8_t expected_hash[32] = {
	0x4f, 0x6b, 0xa3, 0xfd, 0x9b, 0x1e, 0x67, 0x67,
	0x80, 0x95, 0x13, 0x79, 0xa6, 0x1a, 0x4f, 0xea,
	0x89, 0x04, 0xcb, 0x15, 0x04, 0xff, 0x30, 0x46,
	0xa2, 0xd2, 0xef, 0xcd, 0x3f, 0xb3, 0xea, 0xb7,
};

/* Independent Python hashlib/SSH byte-serialization answers for K shapes 9..12. */
static const uint8_t range_hashes[4][32] = {
	{0x0c, 0xc7, 0xbf, 0xe7, 0x33, 0x5a, 0xec, 0xab, 0x8f, 0xb5, 0xa6, 0xae, 0x14, 0xf4, 0x7b, 0x8b, 0x4b, 0xa8, 0x80, 0xd0, 0xdb, 0x42, 0xa1, 0x6e, 0xbd, 0xd3, 0x97, 0xad, 0x87, 0xfe, 0x40, 0xaf},
	{0x33, 0x62, 0x88, 0x3a, 0xdd, 0x1b, 0x14, 0x0e, 0xf8, 0x01, 0x10, 0x53, 0xd0, 0x8a, 0x74, 0xf8, 0x21, 0x31, 0x7a, 0x3f, 0x73, 0x40, 0x79, 0x46, 0x1e, 0xea, 0x74, 0xdc, 0x4c, 0xf0, 0x2d, 0x6a},
	{0x80, 0x94, 0x29, 0x23, 0x35, 0x4d, 0xbd, 0xa3, 0x74, 0xca, 0x0e, 0x85, 0xa4, 0x7d, 0xbd, 0xba, 0xc1, 0x5d, 0x00, 0x8a, 0x3a, 0x2a, 0x4c, 0xe8, 0xda, 0xfc, 0xcc, 0x20, 0x90, 0xc7, 0xfc, 0x3e},
	{0x29, 0x56, 0x4e, 0xbb, 0xf6, 0x1a, 0x77, 0x6f, 0xf7, 0x3b, 0xb2, 0xa4, 0x6f, 0x09, 0x79, 0xe8, 0x22, 0xa4, 0xea, 0x56, 0xf2, 0x9e, 0x22, 0xf9, 0xa7, 0x6e, 0x83, 0x11, 0x8a, 0xd8, 0x31, 0xfd},
};

static const uint8_t *
expected_digest(void)
{
	return active->secret_shape >= 9 ? range_hashes[active->secret_shape - 9] : expected_hash;
}

static void *
tracked_new(size_t length, bool secret)
{
	if (active->count >= sizeof(active->owned) / sizeof(active->owned[0]))
		return NULL;
	void *ptr = malloc(length);
	if (ptr == NULL)
		return NULL;
	active->owned[active->count++] = (struct allocation){ ptr, length, secret };
	active->live++;
	memset(ptr, 0x42, length);
	return ptr;
}

static void
observe_free(const void *ptr, void *data)
{
	struct gex_fixture *fixture = data;
	if (ptr == NULL)
		return;
	for (unsigned i = 0; i < fixture->count; i++) {
		struct allocation *a = &fixture->owned[i];
		if (a->ptr != NULL && a->ptr == ptr) {
			if (a->secret) {
				const uint8_t *bytes = ptr;
				for (size_t j = 0; j < a->length; j++) {
					if (bytes[j] != 0) {
						fixture->uncleared++;
						break;
					}
				}
			}
			a->ptr = NULL;
			fixture->live--;
			break;
		}
	}
}

static bool
put_u32(uint8_t *out, size_t capacity, size_t *pos, uint32_t value)
{
	return dssh_serialize_uint32(value, out, capacity, pos) == 0;
}

static size_t
put_group(uint8_t *out, size_t capacity, size_t *pos)
{
	size_t start = *pos;
	if (!put_u32(out, capacity, pos, 257) || capacity - *pos < 257)
		return 0;
	out[(*pos)++] = 0;
	memcpy(&out[*pos], rfc3526_grp14_p, 256);
	*pos += 256;
	return *pos - start;
}

static size_t
put_small(uint8_t *out, size_t capacity, size_t *pos, uint8_t value)
{
	if (!put_u32(out, capacity, pos, 1) || *pos == capacity)
		return 0;
	out[(*pos)++] = value;
	return 5;
}

static void
release_group(void *owner)
{
	struct gex_fixture *f = owner;
	if (f->group_lease == NULL || f->group_lease->release != NULL || f->group_lease->owner != NULL)
		f->bad = true;
	dssh_result_release(f->group_lease);
	f->group_releases++;
	for (unsigned i = f->group_start; i < f->group_end; i++) {
		struct allocation *a = &f->owned[i];
		if (a->ptr != NULL) {
			void *ptr = a->ptr;
			dssh_cleanse(ptr, a->length);
			observe_free(ptr, f);
			free(ptr);
		}
	}
}

static int
finish_group(struct gex_fixture *f, struct dssh_dh_gex_group_result *out,
    const uint8_t *p, size_t plen, const uint8_t *g, size_t glen, int status)
{
	f->group_end = f->count;
	out->p = (struct dssh_bytes){p, plen};
	out->g = (struct dssh_bytes){g, glen};
	return status;
}

static int
group_provider(uint32_t min, uint32_t preferred, uint32_t max,
    struct dssh_dh_gex_group_result *out, void *data)
{
	struct gex_fixture *f = data;
	uint8_t *p_bytes = NULL, *g_bytes = NULL;
	size_t p_length = 0, g_length = 0;
	uint8_t **p = &p_bytes, **g = &g_bytes;
	size_t *plen = &p_length, *glen = &g_length;
	f->group_start = f->count;
	f->group_lease = &out->lease;
	out->lease.release = release_group;
	out->lease.owner = f;
	f->providers++;
	if (min > preferred || preferred > max)
		f->bad = true;
	if (f->provider_shape == 12 || f->provider_shape == 13) {
		memcpy(f->group_inline, rfc3526_grp14_p, sizeof(f->group_inline));
		f->group_generator = 2;
		out->p = (struct dssh_bytes){f->group_inline, sizeof(f->group_inline)};
		out->g = (struct dssh_bytes){&f->group_generator, 1};
		out->lease.release = NULL;
		if (f->provider_shape == 12)
			out->lease.owner = NULL;
		return 0;
	}
	unsigned mask = f->provider_result != 0 ? f->provider_mask : 3;
	if ((mask & 1) != 0) {
		size_t size = f->provider_shape == 9 ? 1024 : 256;
		*p = tracked_new(size, false);
		if (*p == NULL)
			return finish_group(f, out, p_bytes, p_length, g_bytes, g_length, DSSH_ERROR_ALLOC);
		memcpy(*p, size == 256 ? rfc3526_grp14_p : rfc3526_grp18_p, size);
		*plen = size;
	}
	if ((mask & 2) != 0) {
		*g = tracked_new(1, false);
		if (*g == NULL)
			return finish_group(f, out, p_bytes, p_length, g_bytes, g_length, DSSH_ERROR_ALLOC);
		(*g)[0] = 2;
		*glen = 1;
	}
	if (mask == 4) {
		*p = tracked_new(256, false);
		if (*p == NULL)
			return finish_group(f, out, p_bytes, p_length, g_bytes, g_length, DSSH_ERROR_ALLOC);
		memcpy(*p, rfc3526_grp14_p, 256);
		*g = *p;
		*plen = 256;
		*glen = 256;
	}
	/* Malformed success outputs remain actual owned allocations. */
	if (f->provider_shape == 1) {
		observe_free(*p, f);
		free(*p);
		*p = NULL;
	}
	if (f->provider_shape == 2) {
		observe_free(*g, f);
		free(*g);
		*g = NULL;
	}
	if (f->provider_shape == 3)
		*plen = 0;
	if (f->provider_shape == 4)
		*glen = 0;
	if (f->provider_shape == 5)
		*plen = GEX_MAX / 8 + 1;
	if (f->provider_shape == 6)
		*glen = GEX_MAX / 8 + 1;
	if (f->provider_shape == 7) {
		/* The omitted g allocation is released by the fixture itself. */
		observe_free(*g, f);
		free(*g);
		*g = *p;
	}
	if (f->provider_shape == 8)
		*plen = 255;
	if (f->provider_shape == 11)
		memset(*p, 0, *plen);
	return finish_group(f, out, p_bytes, p_length, g_bytes, g_length, f->provider_result);
}

static int
host_pubkey(const uint8_t **out, size_t *length, dssh_key_algo_ctx *ctx)
{
	(void)ctx;
	static const uint8_t key = 7;
	if (active->host_fault == 4)
		return 0;
	if (active->host_fault != 6)
		*out = &key;
	if (active->host_fault != 5)
		*length = active->host_fault == 7 ? SIZE_MAX : 1;
	return active->host_fault == 1 ? active->host_result : 0;
}

static void
release_host_signature(void *owner)
{
	uint8_t *bytes = owner;
	dssh_cleanse(bytes, 1);
	observe_free(bytes, active);
	free(bytes);
}

static int
host_sign(struct dssh_bytes_result *output_result, const uint8_t *data, size_t data_length, dssh_key_algo_ctx *ctx)
{
	uint8_t *fixture_output = NULL;
	size_t fixture_length = 0;
	uint8_t **out = &fixture_output;
	size_t *length = &fixture_length;

	(void)ctx;
	if (data_length != sizeof(expected_hash) || memcmp(data, expected_digest(), sizeof(expected_hash)) != 0)
		active->bad = true;
	active->signs++;
	if (active->host_fault == 12 || active->host_fault == 13) {
		active->signature_storage = 7;
		if (active->host_fault == 12) {
			fixture_output = &active->signature_storage;
			fixture_length = 1;
		}
		else
			output_result->lease.owner = active;
		return test_auth_output_finish(output_result, fixture_output, fixture_length, 0);
	}
	if (active->host_fault == 8)
		return test_auth_output_finish(output_result, fixture_output, fixture_length, 0);
	if (active->host_fault == 10) {
		*length = 1;
		return test_auth_output_finish(output_result, fixture_output, fixture_length, 0);
	}
	*out = tracked_new(1, false);
	if (*out == NULL)
		return test_auth_output_finish(output_result, fixture_output, fixture_length, DSSH_ERROR_ALLOC);
	(*out)[0] = 7;
	output_result->lease.release = release_host_signature;
	output_result->lease.owner = fixture_output;
	if (active->host_fault != 9)
		*length = active->host_fault == 11 ? SIZE_MAX : 1;
	return test_auth_output_finish(output_result, fixture_output, fixture_length, active->host_fault == 2 ? active->host_result : 0);
}

static int
host_verify(const uint8_t *key, size_t key_length, const uint8_t *sig, size_t sig_length,
    const uint8_t *data, size_t data_length)
{
	active->verifies++;
	if (key_length != 1 || key[0] != 7 || sig_length != 1 || sig[0] != 7
	    || data_length != sizeof(expected_hash) || memcmp(data, expected_digest(), sizeof(expected_hash)) != 0)
		active->bad = true;
	return active->host_fault == 3 ? active->host_result : 0;
}

static int
fixture_send(const uint8_t *message, size_t length, void *data)
{
	struct gex_fixture *f = data;
	unsigned call = f->sends++;
	uint8_t expected = f->client
	    ? (call == 0 ? SSH_MSG_KEX_DH_GEX_REQUEST : SSH_MSG_KEX_DH_GEX_INIT)
	    : (call == 0 ? SSH_MSG_KEX_DH_GEX_GROUP : SSH_MSG_KEX_DH_GEX_REPLY);
	if ((!f->client && f->group_releases != 1)
	    || length == 0 || message[0] != expected || call > 1)
		f->bad = true;
	return 0;
}

static int
fixture_recv(uint8_t *type, uint8_t **message, size_t *length, void *data)
{
	struct gex_fixture *f = data;
	unsigned call = f->receives++;
	if (call > 1)
		return DSSH_ERROR_PARSE;
	size_t pos = 0;
	bool valid = true;
	if (f->client && call == 0) {
		f->message[pos++] = SSH_MSG_KEX_DH_GEX_GROUP;
		valid = put_group(f->message, sizeof(f->message), &pos) != 0
		    && put_small(f->message, sizeof(f->message), &pos, 2) != 0;
	}
	else if (!f->client && call == 0) {
		f->message[pos++] = SSH_MSG_KEX_DH_GEX_REQUEST;
		valid = put_u32(f->message, sizeof(f->message), &pos, f->provider_shape == 10 ? 4096 : 2048)
		    && put_u32(f->message, sizeof(f->message), &pos, 4096)
		    && put_u32(f->message, sizeof(f->message), &pos, f->provider_shape == 9 ? 4096 : 8192);
	}
	else if (f->client) {
		f->message[pos++] = SSH_MSG_KEX_DH_GEX_REPLY;
		valid = put_small(f->message, sizeof(f->message), &pos, 7) != 0
		    && put_small(f->message, sizeof(f->message), &pos, 2) != 0
		    && put_small(f->message, sizeof(f->message), &pos, 7) != 0;
	}
	else {
		f->message[pos++] = SSH_MSG_KEX_DH_GEX_INIT;
		valid = put_small(f->message, sizeof(f->message), &pos, 2) != 0;
	}
	if (!valid)
		return DSSH_ERROR_INVALID;
	if (call == f->wire_target && f->wire_shape == 1)
		pos--;
	if (call == f->wire_target && f->wire_shape == 2)
		f->message[pos++] = 0xff;
	*type = f->message[0];
	*message = f->message;
	*length = pos;
	return 0;
}

struct fixture_native {
	void *exponent;
	void *temporary;
};

static int
native_init(void **out)
{
	active->native_inits++;
	if (active->native_mode == 1 || active->native_mode == 2)
		return active->native_mode == 1 ? 0 : DSSH_ERROR_INIT;
	struct fixture_native *state = tracked_new(sizeof(*state), false);
	if (state == NULL)
		return DSSH_ERROR_ALLOC;
	*state = (struct fixture_native){0};
	*out = state;
	return active->native_mode == 3 ? DSSH_ERROR_INIT : (active->native_mode == 4 ? 1 : 0);
}

static void
native_reset(void *opaque)
{
	struct fixture_native *state = opaque;
	if (state == NULL)
		return;
	void *values[] = {state->exponent, state->temporary};
	state->exponent = NULL;
	state->temporary = NULL;
	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
		if (values[i] != NULL) {
			dssh_cleanse(values[i], 1);
			observe_free(values[i], active);
			free(values[i]);
		}
	}
}

static void
native_cleanup(void *state)
{
	active->native_cleanups++;
	if (state != NULL) {
		native_reset(state);
		active->ctx_frees++;
		observe_free(state, active);
		free(state);
	}
}

static int
keygen(void *opaque, const uint8_t *p, size_t plen, const uint8_t *g, size_t glen,
    uint8_t *out, size_t capacity, size_t *length)
{
	struct fixture_native *state = opaque;
	active->keygens++;
	if (plen != 256 || glen != 1 || g[0] != 2 || memcmp(p, rfc3526_grp14_p, 256) != 0 || capacity != 256)
		active->bad = true;
	bool fail = active->operation == (active->client ? 1U : 2U);
	if (!fail || (active->partial_mask & 1) != 0) {
		out[0] = 2;
		*length = 1;
	}
	if (!fail || (active->partial_mask & 2) != 0) {
		state->exponent = tracked_new(1, true);
		if (state->exponent == NULL)
			return DSSH_ERROR_ALLOC;
	}
	if (fail && (active->partial_mask & 4) != 0)
		memset(out, 0x42, capacity);
	if (fail && (active->partial_mask & 8) != 0) {
		state->temporary = tracked_new(1, true);
		if (state->temporary == NULL)
			return DSSH_ERROR_ALLOC;
	}
	return fail ? active->operation_result : 0;
}

static int
derive(void *opaque, const uint8_t *peer, size_t peerlen,
    uint8_t *raw, size_t capacity, size_t *length)
{
	struct fixture_native *state = opaque;
	active->derives++;
	if (state->exponent == NULL || peerlen != 1 || peer[0] != 2 || capacity != 256)
		return DSSH_ERROR_INIT;
	active->secret_storage = raw;
	active->secret_capacity = capacity;
	bool fail = active->operation == 3;
	if (!fail || (active->partial_mask & 1) != 0) {
		/* Mark unreported trailing capacity too, independently of returned len. */
		memset(raw, 0x42, capacity);
		raw[0] = 3;
		*length = 1;
		unsigned shape = active->secret_shape;
		if (shape == 1 || shape == 7) {
			memset(raw, 0, capacity);
			raw[capacity - 1] = shape == 7 ? 3 : 0;
			*length = capacity;
		}
		if (shape == 2)
			raw[0] = 1;
		if (shape == 3 || shape == 4 || shape == 5 || shape == 9) {
			memcpy(raw, rfc3526_grp14_p, 256);
			*length = 256;
			if (shape == 3 || shape == 9)
				raw[255] = shape == 3 ? 254 : 253;
			if (shape == 5) {
				for (size_t i = 256; i != 0; i--) {
					if (raw[i - 1] != 255) {
						raw[i - 1]++;
						break;
					}
					raw[i - 1] = 0;
				}
			}
		}
		if (shape == 6)
			*length = SIZE_MAX;
		if (shape == 8)
			*length = 0;
		if (shape == 10 || shape == 11)
			raw[0] = shape == 10 ? 128 : 127;
		if (shape == 12) {
			raw[0] = 1;
			raw[1] = 0;
			*length = 2;
		}
	}
	if (fail && (active->partial_mask & 2) != 0)
		memset(raw, 0x42, capacity);
	return fail ? active->operation_result : 0;
}

static const struct dhgex_ops ops = {
	.init = native_init, .reset = native_reset, .cleanup = native_cleanup,
	.keygen = keygen, .derive = derive,
};

static bool
secret_correct(const struct gex_fixture *f, const struct dssh_bytes *secret)
{
	if (f->secret_shape == 9)
		return memcmp(secret->data, rfc3526_grp14_p, 255) == 0 && secret->data[255] == 253;
	if (f->secret_shape == 12)
		return secret->data[0] == 1 && secret->data[1] == 0;
	uint8_t first = f->secret_shape == 10 ? 128 : (f->secret_shape == 11 ? 127 : 3);
	return secret->data[0] == first;
}

static bool
run_case(struct gex_fixture *f, int expected)
{
	active = f;
	dssh_test_alloc_reset();
	dssh_test_alloc_observe_frees(observe_free, f);
	dssh_key_algo key = calloc(1, sizeof(*key) + sizeof("gex-contract-key"));
	if (key == NULL) {
		dssh_test_alloc_reset();
		active = NULL;
		return false;
	}
	memcpy(key->name, "gex-contract-key", sizeof("gex-contract-key"));
	key->pubkey = host_pubkey;
	key->sign = host_sign;
	key->verify = host_verify;
	struct dssh_dh_gex_provider provider = { group_provider, f };
	const uint8_t ic[] = { 20, 1, 2 }, is[] = { 20, 3, 4 };
	void *state = NULL;
	if (dhgex_init_impl(f->client, &provider, &state, &ops) < 0) {
		dssh_test_alloc_reset();
		free(key);
		active = NULL;
		return false;
	}
	struct dssh_kex_result output = {0};
	struct dssh_kex_context ctx = {
		.v_c = "client", .v_c_len = 6, .v_s = "server", .v_s_len = 6,
		.i_c = ic, .i_c_len = sizeof(ic), .i_s = is, .i_s_len = sizeof(is),
		.key_algo = key, .config = &provider, .state = state, .send = fixture_send,
		.recv = fixture_recv, .io_ctx = f, .client = f->client,
	};
	int ret = dhgex_handler_impl(&ctx, &output, &ops);
	bool valid = ret == expected && !f->bad;
	if (expected == 0)
		valid = valid && output.shared_secret.data != NULL
		    && output.shared_secret.len == (f->secret_shape == 9 ? 256U : (f->secret_shape == 12 ? 2U : 1U))
		    && secret_correct(f, &output.shared_secret)
		    && output.exchange_hash.data != NULL && output.exchange_hash.len == 32
		    && memcmp(output.exchange_hash.data, expected_digest(), sizeof(expected_hash)) == 0
		    && f->keygens == 1 && f->derives == 1 && f->sends == 2 && f->receives == 2
		    && f->signs == (f->client ? 0U : 1U) && f->verifies == (f->client ? 1U : 0U);
	else
		valid = valid && output.shared_secret.data == NULL && output.exchange_hash.data == NULL;
	dssh_result_release(&output.lease);
	if (f->secret_storage != NULL) {
		for (size_t i = 0; i < f->secret_capacity; i++)
			valid = valid && f->secret_storage[i] == 0;
	}
	dhgex_cleanup(state);
	valid = valid && f->live == 0 && f->uncleared == 0;
	if (f->providers != 0) {
		unsigned releases = f->provider_shape >= 12 ? 0 : 1;
		valid = valid && f->group_releases == releases;
		if (f->provider_shape >= 12)
			valid = valid && f->group_generator == 2
			    && memcmp(f->group_inline, rfc3526_grp14_p, sizeof(f->group_inline)) == 0;
	}
	if (f->provider_result != 0 || f->provider_shape != 0)
		valid = valid && f->providers == 1 && f->keygens == 0;
	if (!valid)
		fprintf(stderr, "DH contract: client=%d provider=%u/%u/%d operation=%u/%u/%d host=%u/%d wire=%u/%u ret=%d expected=%d live=%u uncleared=%u bad=%d\n",
		    f->client, f->provider_shape, f->provider_mask, f->provider_result,
		    f->operation, f->partial_mask, f->operation_result, f->host_fault, f->host_result,
		    f->wire_target, f->wire_shape, ret, expected, f->live, f->uncleared, f->bad);
	/* Recover resources left by a failing baseline without touching freed values. */
	dssh_test_alloc_reset();
	for (unsigned i = 0; i < f->count; i++) {
		if (f->owned[i].ptr != NULL) {
			dssh_cleanse(f->owned[i].ptr, f->owned[i].length);
			free(f->owned[i].ptr);
			f->owned[i].ptr = NULL;
		}
	}
	free(key);
	active = NULL;
	return valid;
}

static int
test_provider_results(void)
{
	const int results[] = { -77, DSSH_ERROR_INIT, DSSH_ERROR_TOOLONG, DSSH_ERROR_REKEY_NEEDED, 1 };
	bool valid = true;
	for (size_t r = 0; r < sizeof(results) / sizeof(results[0]); r++) {
		for (unsigned mask = 0; mask <= 4; mask++) {
			struct gex_fixture f = { .provider_result = results[r], .provider_mask = mask };
			bool matched = run_case(&f, results[r] > 0 ? DSSH_ERROR_INVALID : results[r]);
			valid = valid && matched;
		}
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_provider_outputs(void)
{
	bool valid = true;
	for (unsigned shape = 0; shape <= 13; shape++) {
		struct gex_fixture f = { .provider_shape = shape };
		bool matched = run_case(&f, shape == 0 ? 0 : DSSH_ERROR_INVALID);
		valid = valid && matched;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_operation_errors(void)
{
	const int errors[] = { -77, DSSH_ERROR_ALLOC, 1 };
	bool valid = true;
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned operation = 1; operation <= 3; operation++) {
			if ((operation == 1 && role == 0) || (operation == 2 && role != 0))
				continue;
			unsigned masks = operation == 2 ? 16 : 4;
			for (unsigned mask = 0; mask < masks; mask++) {
				for (size_t e = 0; e < sizeof(errors) / sizeof(errors[0]); e++) {
					struct gex_fixture f = { .client = role != 0, .operation = operation,
					    .operation_result = errors[e], .partial_mask = mask };
					bool matched = run_case(&f, errors[e] > 0 ? DSSH_ERROR_INVALID : errors[e]);
					valid = valid && matched;
				}
			}
		}
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_framing(void)
{
	bool valid = true;
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned target = 0; target < 2; target++) {
			for (unsigned shape = 0; shape < 3; shape++) {
				if (shape == 0 && target != 0)
					continue;
				struct gex_fixture f = { .client = role != 0, .wire_target = target, .wire_shape = shape };
				bool matched = run_case(&f, shape == 0 ? 0 : DSSH_ERROR_PARSE);
				valid = valid && matched;
			}
		}
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_host_results(void)
{
	const int errors[] = { -77, 1 };
	bool valid = true;
	for (unsigned fault = 1; fault <= 3; fault++) {
		for (size_t e = 0; e < sizeof(errors) / sizeof(errors[0]); e++) {
			struct gex_fixture f = { .client = fault == 3, .host_fault = fault, .host_result = errors[e] };
			bool matched = run_case(&f, errors[e] > 0 ? DSSH_ERROR_INVALID : errors[e]);
			valid = valid && matched;
		}
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_host_outputs(void)
{
	bool valid = true;
	for (unsigned fault = 4; fault <= 13; fault++) {
#if SIZE_MAX <= UINT32_MAX
		if (fault == 7)
			continue;
#endif
		struct gex_fixture f = { .host_fault = fault };
		bool matched = run_case(&f, DSSH_ERROR_INVALID);
		valid = valid && matched;
		if (fault >= 12)
			valid = valid && f.signature_storage == 7;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_secret_ranges(void)
{
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned shape = 0; shape <= 12; shape++) {
			struct gex_fixture f = {.client = role != 0, .secret_shape = shape};
			bool accepted = shape == 0 || shape == 7 || shape >= 9;
			ASSERT_TRUE(run_case(&f, accepted ? 0 : DSSH_ERROR_INVALID));
		}
	}
	return TEST_PASS;
}

static int
test_native_initialization(void)
{
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned mode = 0; mode < 5; mode++) {
			struct gex_fixture f = {.native_mode = mode};
			active = &f;
			void *state = NULL;
			int ret = dhgex_init_impl(role != 0, NULL, &state, &ops);
			int expected = mode < 2 ? 0 : (mode == 4 ? DSSH_ERROR_INVALID : DSSH_ERROR_INIT);
			bool valid = ret == expected && f.native_inits == 1;
			if (ret == 0) {
				void *original = state;
				dssh_test_alloc_fail_after(0);
				ret = dhgex_init_impl(role != 0, NULL, &state, &ops);
				int allocations = dssh_test_alloc_count();
				dssh_test_alloc_reset();
				valid = valid && ret == 0 && state == original && allocations == 0 && f.native_inits == 1;
			}
			else
				valid = valid && state == NULL;
			dhgex_cleanup(state);
			valid = valid && f.live == 0 && f.native_cleanups == (mode == 2 ? 0U : 1U);
			active = NULL;
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{ "gex/native_initialization", test_native_initialization },
	{ "gex/secret_ranges", test_secret_ranges },
	{ "gex/provider_results", test_provider_results },
	{ "gex/provider_outputs", test_provider_outputs },
	{ "gex/operation_errors", test_operation_errors },
	{ "gex/framing", test_framing },
	{ "gex/host_results", test_host_results },
	{ "gex/host_outputs", test_host_outputs },
};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
