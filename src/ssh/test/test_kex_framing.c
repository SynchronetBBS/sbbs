/* Shared-handler framing tests. Deterministic crypto/host-key adapters isolate
 * parsing and cleanup; production crypto/peer tests remain separate evidence. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dssh_test.h"
#include "dssh_test_alloc.h"
#include "fixture_writer.h"
#include <limits.h>
#include "kex/workspace.h"
#include "deucessh-crypto.h"
#include "deucessh-kex.h"
#include "kex/curve25519-sha256-ops.h"
#include "kex/hybrid-pq-kex-ops.h"

struct framing_fixture {
	uint8_t signature_storage;
	uint8_t message[1300];
	size_t length;
	const struct hybrid_pq_params *params;
	unsigned live;
	unsigned agreements;
	unsigned kems;
	unsigned signatures;
	unsigned pubkeys;
	unsigned sign_calls;
	unsigned verifications;
	unsigned sends;
	unsigned host_fault;
	unsigned backend_mode;
	unsigned backend_inits;
	unsigned backend_cleanups;
	uint8_t *raw_key;
	size_t raw_key_capacity;
	unsigned raw_key_releases;
	int host_result;
	bool client;
	bool oracle_valid;
};

#include "auth_result_fixture.h"

static struct framing_fixture *active;

struct framing_answer {
	size_t length;
	uint8_t secret[64];
	uint8_t hash[2][64]; /* server, client */
};

/* Input-defined answers for the deterministic fixture, not native primitive
 * vectors. K/H are compared while their actual producer lease is live. */
static const struct framing_answer framing_answers[] = {
	{ .length = 32, .secret = {
		0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
		0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
		0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
		0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
	}, .hash = {
		{
			0xa6, 0x38, 0x92, 0xac, 0xa1, 0xf8, 0x84, 0x7f,
			0xe4, 0xe2, 0x81, 0x56, 0x59, 0xad, 0xc7, 0xe6,
			0xee, 0xc6, 0xb9, 0x88, 0xa3, 0x8e, 0xff, 0x22,
			0xda, 0xd8, 0x5a, 0x4c, 0xd4, 0x43, 0xbb, 0x05,
		},
		{
			0xd9, 0xcb, 0xd3, 0xdf, 0x29, 0xbd, 0xce, 0x5a,
			0xa7, 0x0d, 0xa7, 0x01, 0x40, 0x47, 0x17, 0x99,
			0x9a, 0xf7, 0x81, 0x12, 0xac, 0xc3, 0xaf, 0x35,
			0x33, 0x8b, 0xb8, 0x2f, 0x27, 0x4c, 0x29, 0xfb,
		},
	} },
	{ .length = 64, .secret = {
		0x54, 0x85, 0xcd, 0x7a, 0x87, 0xcd, 0x72, 0xd4,
		0xe2, 0x84, 0x79, 0xcd, 0xba, 0xa0, 0xc3, 0xb2,
		0x1a, 0x83, 0xa7, 0xcd, 0xd3, 0x9e, 0x4a, 0xae,
		0x44, 0x0b, 0x7e, 0x9f, 0xaa, 0x1f, 0x97, 0x3a,
		0xd5, 0xad, 0x24, 0x7c, 0xde, 0x88, 0x10, 0xff,
		0xcc, 0xdb, 0xdb, 0xa9, 0x61, 0x34, 0x53, 0x42,
		0xfd, 0x27, 0x9b, 0xf4, 0xfd, 0x78, 0xf8, 0x0d,
		0x6b, 0xae, 0xcf, 0x11, 0x2b, 0x2d, 0xc6, 0x7b,
	}, .hash = {
		{
			0x2e, 0x3d, 0xb5, 0xd7, 0x50, 0x80, 0xb0, 0xe4,
			0x96, 0xc7, 0x4c, 0x04, 0xd3, 0x2c, 0x0f, 0x64,
			0x7d, 0xe7, 0xe8, 0xa1, 0xdb, 0x98, 0x34, 0x60,
			0x0f, 0xfd, 0x2e, 0x3d, 0x58, 0xd9, 0xb7, 0x6d,
			0x97, 0x82, 0x66, 0xad, 0x3a, 0xe3, 0xb2, 0x17,
			0xc4, 0x33, 0xd2, 0xd1, 0x04, 0xaf, 0xe7, 0x6b,
			0xc9, 0xbf, 0x2f, 0x54, 0xcf, 0x2e, 0xc6, 0xff,
			0x8c, 0x78, 0x1a, 0x78, 0xd3, 0xf5, 0xcf, 0x8c,
		},
		{
			0x56, 0xfa, 0x02, 0xea, 0x28, 0x1b, 0x3b, 0xf4,
			0x08, 0xdc, 0xb4, 0x5e, 0x5a, 0x14, 0xa8, 0x8e,
			0x8b, 0x2c, 0x15, 0x6d, 0x7a, 0x5e, 0xfe, 0x0c,
			0xaa, 0xb4, 0x96, 0x40, 0xf7, 0x53, 0xfd, 0x82,
			0xef, 0xfe, 0xd8, 0x6c, 0x48, 0xd0, 0xeb, 0x62,
			0x25, 0x20, 0x22, 0x0a, 0x1f, 0x29, 0x2b, 0xee,
			0xf7, 0x41, 0x41, 0x55, 0xef, 0xfa, 0xfb, 0x2f,
			0x30, 0x98, 0x61, 0x34, 0xf8, 0x3b, 0xe7, 0xc9,
		},
	} },
	{ .length = 32, .secret = {
		0x47, 0x40, 0x5c, 0x92, 0x8e, 0xed, 0xa6, 0xb9,
		0x7e, 0x15, 0x3e, 0xb8, 0x09, 0x7d, 0x77, 0xbe,
		0x0e, 0x37, 0x33, 0x25, 0xc0, 0x3b, 0xe3, 0x05,
		0x71, 0xc2, 0x8c, 0xdc, 0x26, 0x3e, 0x76, 0x9d,
	}, .hash = {
		{
			0x3e, 0x7d, 0xf6, 0x8a, 0x24, 0x00, 0x00, 0x4f,
			0xc6, 0xea, 0x9b, 0x2f, 0xbd, 0x8b, 0x21, 0xac,
			0x3f, 0xa4, 0x71, 0xdb, 0x61, 0xb8, 0x42, 0x88,
			0xca, 0xc9, 0x83, 0x31, 0xeb, 0x6b, 0xf8, 0x67,
		},
		{
			0x24, 0xc9, 0x94, 0x6b, 0x03, 0x9f, 0x8c, 0xac,
			0xda, 0xf7, 0xbf, 0xea, 0x00, 0xb4, 0x50, 0x68,
			0xc5, 0x4b, 0x29, 0xe8, 0xe7, 0xd1, 0x68, 0xfa,
			0x59, 0xbd, 0xfc, 0xb0, 0x44, 0x33, 0x7e, 0xb9,
		},
	} },
};

static const struct framing_answer *
framing_answer(const struct hybrid_pq_params *p)
{
	if (p == NULL)
		return &framing_answers[0];
	if (p->digest_len == 64 && p->q_c_len == 1190 && p->q_s_len == 1071)
		return &framing_answers[1];
	if (p->digest_len == 32 && p->q_c_len == 1216 && p->q_s_len == 1120)
		return &framing_answers[2];
	return NULL;
}

static bool
framing_hash(const uint8_t *data, size_t length)
{
	const struct framing_answer *answer = framing_answer(active->params);
	bool valid = answer != NULL && data != NULL && length == answer->length
	    && memcmp(data, answer->hash[active->client ? 1 : 0], length) == 0;
	active->oracle_valid = active->oracle_valid && valid;
	return valid;
}

static int
new_private(void **out)
{
	if (out == NULL || active == NULL || active->live == UINT_MAX)
		return DSSH_ERROR_INVALID;
	*out = malloc(1);
	if (*out == NULL)
		return DSSH_ERROR_ALLOC;
	active->live++;
	return 0;
}

static void
free_private(void *priv)
{
	if (priv != NULL) {
		if (active == NULL || active->live == 0)
			abort();
		active->live--;
		free(priv);
	}
}

static int
x_keygen(uint8_t pub[32], void **priv)
{
	if (pub == NULL)
		return DSSH_ERROR_INVALID;
	memset(pub, 0x11, 32);
	return new_private(priv);
}

static int
curve_backend_init(void **backend)
{
	*backend = NULL;
	active->backend_inits++;
	if (active->backend_mode == 1 || active->backend_mode == 3 || active->backend_mode == 4) {
		int ret = new_private(backend);
		if (ret < 0)
			return ret;
	}
	if (active->backend_mode == 2 || active->backend_mode == 3)
		return DSSH_ERROR_INIT;
	if (active->backend_mode == 4)
		return 1;
	return 0;
}

static void
curve_backend_cleanup(void *backend)
{
	active->backend_cleanups++;
	free_private(backend);
}

static int
curve_keygen(void *backend, uint8_t pub[32], void **priv)
{
	(void)backend;
	return x_keygen(pub, priv);
}

static int
curve_derive(void *priv, const uint8_t *peer, size_t len, uint8_t out[32])
{
	(void)peer;
	if (active == NULL || priv == NULL || peer == NULL || out == NULL || len != 32
	    || active->agreements == UINT_MAX)
		return DSSH_ERROR_INVALID;
	memset(out, 0x42, 32);
	active->agreements++;
	return 0;
}

static int
pq_keygen(uint8_t *pub, uint8_t *storage, size_t capacity, void **priv)
{
	if (active == NULL || active->params == NULL || pub == NULL || priv == NULL)
		return DSSH_ERROR_INVALID;
	memset(pub, 0x11, active->params->pq_pk_len);
	if (storage != NULL) {
		if (capacity != active->params->pq_sk_len)
			return DSSH_ERROR_INVALID;
		memset(storage, 0x71, capacity);
		active->raw_key = storage;
		active->raw_key_capacity = capacity;
		*priv = storage;
		return 0;
	}
	return new_private(priv);
}

static void
pq_free_key(void *priv)
{
	if (priv != NULL && priv == active->raw_key)
		active->raw_key_releases++;
	else
		free_private(priv);
}

static int
pq_encaps(uint8_t *ct, uint8_t *ss, const uint8_t *pub, size_t len)
{
	(void)pub;
	if (active == NULL || active->params == NULL || pub == NULL || ct == NULL || ss == NULL
	    || active->kems == UINT_MAX || len != active->params->pq_pk_len)
		return DSSH_ERROR_INVALID;
	memset(ct, 0x11, active->params->pq_ct_len);
	memset(ss, 0x22, active->params->pq_ss_len);
	active->kems++;
	return 0;
}

static int
pq_decaps(uint8_t *ss, const uint8_t *ct, size_t len, void *priv)
{
	(void)ct;
	if (active == NULL || active->params == NULL || priv == NULL || ct == NULL || ss == NULL
	    || active->kems == UINT_MAX || len != active->params->pq_ct_len)
		return DSSH_ERROR_INVALID;
	memset(ss, 0x22, active->params->pq_ss_len);
	active->kems++;
	return 0;
}

static int
host_pubkey(const uint8_t **out, size_t *len, dssh_key_algo_ctx *ctx)
{
	(void)ctx;
	if (active == NULL || out == NULL || len == NULL || active->pubkeys == UINT_MAX)
		return DSSH_ERROR_INVALID;
	active->pubkeys++;
	static const uint8_t key = 7;
	if (active->host_fault == 4)
		return 0;
	if (active->host_fault == 5) {
		*out = &key;
		return 0;
	}
	if (active->host_fault == 6) {
		*len = 1;
		return 0;
	}
	if (active->host_fault == 7) {
		*out = &key;
		*len = SIZE_MAX;
		return 0;
	}
	*out = &key;
	*len = 1;
	return active->host_fault == 1 ? active->host_result : 0;
}

static int
host_sign(struct dssh_bytes_result *output_result, const uint8_t *data, size_t data_len, dssh_key_algo_ctx *ctx)
{
	uint8_t *fixture_output = NULL;
	size_t fixture_length = 0;
	uint8_t **out = &fixture_output;
	size_t *len = &fixture_length;

	(void)ctx;
	if (active == NULL || active->sign_calls == UINT_MAX)
		return DSSH_ERROR_INVALID;
	active->sign_calls++;
	if (!framing_hash(data, data_len))
		return test_auth_output_finish(output_result, fixture_output, fixture_length, DSSH_ERROR_INVALID);
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
		*len = 1;
		return test_auth_output_finish(output_result, fixture_output, fixture_length, 0);
	}
	*out = test_auth_output_alloc(output_result, 1);
	if (*out == NULL)
		return test_auth_output_finish(output_result, fixture_output, fixture_length, DSSH_ERROR_ALLOC);
	(*out)[0] = 7;
	if (active->host_fault == 9)
		return test_auth_output_finish(output_result, fixture_output, fixture_length, 0);
	if (active->host_fault == 11) {
		*len = SIZE_MAX;
		return test_auth_output_finish(output_result, fixture_output, fixture_length, 0);
	}
	*len = 1;
	active->signatures++;
	return test_auth_output_finish(output_result, fixture_output, fixture_length, active->host_fault == 2 ? active->host_result : 0);
}

static int
host_verify(const uint8_t *key, size_t key_len, const uint8_t *sig, size_t sig_len,
    const uint8_t *data, size_t data_len)
{
	bool valid = key != NULL && sig != NULL && key_len == 1 && key[0] == 7 && sig_len == 1 && sig[0] == 7
	    && framing_hash(data, data_len);
	active->oracle_valid = active->oracle_valid && valid;
	if (!valid || active->verifications == UINT_MAX)
		return DSSH_ERROR_INVALID;
	active->verifications++;
	return active->host_fault == 3 ? active->host_result : 0;
}

/* Callers establish four readable bytes within the checked packet extent. */
static uint32_t
framing_u32(const uint8_t *data)
{
	return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
	    | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

static int
fixture_send(const uint8_t *payload, size_t len, void *opaque)
{
	struct framing_fixture *fixture = opaque;
	if (fixture == NULL)
		return DSSH_ERROR_INVALID;
	size_t public_size = fixture->params == NULL ? 32
	    : (fixture->client ? fixture->params->q_c_len : fixture->params->q_s_len);
	size_t prefix = fixture->client ? 5 : 10;
	size_t suffix = fixture->client ? 0 : 5;
	bool valid = payload != NULL && public_size <= 1216 && len == prefix + public_size + suffix
	    && payload[0] == (fixture->client ? SSH_MSG_KEX_ECDH_INIT : SSH_MSG_KEX_ECDH_REPLY);
	if (valid && !fixture->client)
		valid = framing_u32(payload + 1) == 1 && payload[5] == 7;
	if (valid) {
		valid = framing_u32(payload + prefix - 4) == public_size;
		for (size_t i = 0; i < public_size; i++)
			valid = payload[prefix + i] == 0x11 && valid;
		if (!fixture->client)
			valid = framing_u32(payload + prefix + public_size) == 1
			    && payload[len - 1] == 7 && valid;
	}
	fixture->oracle_valid = fixture->oracle_valid && valid;
	if (!valid || fixture->sends == UINT_MAX)
		return DSSH_ERROR_INVALID;
	fixture->sends++;
	return 0;
}

static int
fixture_recv(uint8_t *type, uint8_t **payload, size_t *len, void *opaque)
{
	struct framing_fixture *fixture = opaque;
	if (fixture == NULL || type == NULL || payload == NULL || len == NULL
	    || fixture->length == 0 || fixture->length > sizeof(fixture->message))
		return DSSH_ERROR_INVALID;
	*type = fixture->message[0];
	*payload = fixture->message;
	*len = fixture->length;
	return 0;
}

static const struct dssh_x25519_ops curve_ops = {
	.init = curve_backend_init, .cleanup = curve_backend_cleanup,
	.keygen = curve_keygen, .derive = curve_derive, .free_priv = free_private,
};

static const struct hybrid_pq_ops hybrid_ops = {
	.x25519 = &curve_ops,
	.pq_keygen = pq_keygen, .pq_encaps = pq_encaps,
	.pq_decaps = pq_decaps, .pq_free_sk = pq_free_key,
};

static const struct hybrid_pq_params params[] = {
	{ .hash_name = "SHA-512", .digest_len = 64, .pq_pk_len = 1158,
	    .pq_ct_len = 1039, .pq_ss_len = 32, .q_c_len = 1190, .q_s_len = 1071 },
	{ .hash_name = "SHA-256", .digest_len = 32, .pq_pk_len = 1184,
	    .pq_ct_len = 1088, .pq_ss_len = 32, .q_c_len = 1216, .q_s_len = 1120 },
};

static int
framing_case_options(const struct hybrid_pq_params *p, bool client, unsigned fault, int result)
{
	if (sizeof(struct dssh_key_algo_s) > SIZE_MAX - sizeof("framing-key"))
		return TEST_FAIL;
	dssh_key_algo ka = calloc(1, sizeof(*ka) + sizeof("framing-key"));
	ASSERT_NOT_NULL(ka);
	memcpy(ka->name, "framing-key", sizeof("framing-key"));
	ka->pubkey = host_pubkey;
	ka->sign = host_sign;
	ka->verify = host_verify;
	bool valid = true;
	unsigned shapes = fault == 0 ? 3 : 1;
	for (unsigned shape = 0; shape < shapes; shape++) {
		struct framing_fixture fixture = {
			.params = p, .client = client, .host_fault = fault, .host_result = result, .oracle_valid = true,
		};
		active = &fixture;
		struct fixture_writer writer = {.data = fixture.message,
		    .capacity = sizeof(fixture.message), .valid = true};
		const uint8_t type = client ? SSH_MSG_KEX_ECDH_REPLY : SSH_MSG_KEX_ECDH_INIT;
		const uint8_t key = 7;
		bool built = fixture_writer_bytes(&writer, &type, 1);
		if (client)
			built = built && fixture_writer_string(&writer, &key, 1);
		size_t public_size = p == NULL ? 32 : (client ? p->q_s_len : p->q_c_len);
		built = built && fixture_writer_repeated_string(&writer, 0x11, public_size);
		if (client)
			built = built && fixture_writer_string(&writer, &key, 1);
		if (shape == 2) {
			const uint8_t trailing = 0xff;
			built = built && fixture_writer_bytes(&writer, &trailing, 1);
		}
		if (!built || (shape == 1 && writer.used == 0)) {
			valid = false;
			active = NULL;
			break;
		}
		fixture.length = writer.used;
		if (shape == 1)
			fixture.length--;
		void *state = NULL;
		int ret = p == NULL ? curve25519_init_impl(client, NULL, &state, &curve_ops)
		                : hybrid_pq_init_impl(client, NULL, &state, p, &hybrid_ops);
		if (ret != 0 || state == NULL) {
			if (p == NULL)
				curve25519_cleanup(state);
			else
				hybrid_pq_cleanup(state);
			valid = false;
			active = NULL;
			break;
		}
		struct dssh_kex_result output = {0};
		struct dssh_kex_context kctx = {
			.v_c = "client", .v_c_len = 6, .v_s = "server", .v_s_len = 6,
			.i_c = fixture.message, .i_c_len = 1, .i_s = fixture.message, .i_s_len = 1,
			.key_algo = ka, .state = state, .send = fixture_send, .recv = fixture_recv,
			.io_ctx = &fixture, .client = client,
		};
		ret = p == NULL ? curve25519_handler_impl(&kctx, &output, &curve_ops)
		                     : hybrid_pq_handler_impl(&kctx, &output, p, &hybrid_ops);
		bool matched = fixture.live == 0 && fixture.oracle_valid
		    && fixture.pubkeys == (client ? 0U : 1U);
		if (p != NULL && client && p->pq_sk_len != 0) {
			matched = matched && fixture.raw_key != NULL && fixture.raw_key_capacity == p->pq_sk_len
			    && fixture.raw_key_releases == 1;
			if (fixture.raw_key != NULL && fixture.raw_key_capacity == p->pq_sk_len) {
				for (size_t i = 0; i < fixture.raw_key_capacity; i++)
					matched = matched && fixture.raw_key[i] == 0;
			}
		}
		if (fault >= 12)
			matched = matched && fixture.signature_storage == 7;
		if (fault != 0) {
			int expected = fault >= 4 || result > 0 ? DSSH_ERROR_INVALID : result;
			matched = matched && ret == expected && output.shared_secret.data == NULL
			    && output.exchange_hash.data == NULL && output.lease.release == NULL && output.lease.owner == NULL
			    && fixture.sends == (client ? 1U : 0U)
			    && fixture.sign_calls == (!client && (fault == 2 || fault >= 8) ? 1U : 0U)
			    && fixture.verifications == (fault == 3 ? 1U : 0U);
		}
		else if (shape == 0) {
			const struct framing_answer *answer = framing_answer(p);
			matched = matched && ret == 0 && answer != NULL && output.lease.release != NULL
			    && output.shared_secret.data != NULL && output.exchange_hash.data != NULL
			    && output.shared_secret.len == answer->length && output.exchange_hash.len == answer->length
			    && memcmp(output.shared_secret.data, answer->secret, answer->length) == 0
			    && memcmp(output.exchange_hash.data, answer->hash[client ? 1 : 0], answer->length) == 0
			    && fixture.sends == 1 && fixture.agreements == 1 && fixture.kems == (p == NULL ? 0U : 1U)
			    && fixture.signatures == (client ? 0U : 1U) && fixture.sign_calls == (client ? 0U : 1U)
			    && fixture.verifications == (client ? 1U : 0U);
		}
		else
			matched = matched && ret == DSSH_ERROR_PARSE
			    && output.shared_secret.data == NULL && output.exchange_hash.data == NULL
			    && fixture.agreements == 0 && fixture.kems == 0
			    && fixture.signatures == 0 && fixture.sign_calls == 0 && fixture.verifications == 0
			    && fixture.sends == (client ? 1U : 0U);
		if (!matched)
			fprintf(stderr, "client=%d hybrid=%d shape=%u ret=%d live=%u agree=%u kem=%u sign=%u verify=%u\n",
			    client, p != NULL, shape, ret, fixture.live, fixture.agreements,
			    fixture.kems, fixture.signatures, fixture.verifications);
		dssh_result_release(&output.lease);
		matched = output.lease.release == NULL && output.lease.owner == NULL && matched;
		fixture.raw_key = NULL;
		fixture.raw_key_capacity = 0;
		if (p == NULL)
			curve25519_cleanup(state);
		else
			hybrid_pq_cleanup(state);
		valid = valid && matched;
		active = NULL;
	}
	active = NULL;
	free(ka);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
framing_case(const struct hybrid_pq_params *p, bool client)
{
	return framing_case_options(p, client, 0, 0);
}

static int
host_callback_results(void)
{
	const struct hybrid_pq_params *methods[] = { NULL, &params[0], &params[1] };
	const int results[] = { DSSH_ERROR_INIT, 1 };
	for (size_t method = 0; method < sizeof(methods) / sizeof(methods[0]); method++) {
		for (unsigned fault = 1; fault <= 3; fault++) {
			for (size_t result = 0; result < sizeof(results) / sizeof(results[0]); result++) {
				ASSERT_EQ(framing_case_options(methods[method], fault == 3, fault, results[result]), TEST_PASS);
			}
		}
	}
	return TEST_PASS;
}

static int
host_callback_outputs(void)
{
	const struct hybrid_pq_params *methods[] = { NULL, &params[0], &params[1] };
	for (size_t method = 0; method < sizeof(methods) / sizeof(methods[0]); method++) {
		for (unsigned fault = 4; fault <= 13; fault++) {
#if SIZE_MAX <= UINT32_MAX
			/* A borrowed one-byte span cannot represent this oversized-prefix
			 * fixture on a host where every size_t already fits uint32. */
			if (fault == 7)
				continue;
#endif
			ASSERT_EQ(framing_case_options(methods[method], false, fault, 0), TEST_PASS);
		}
	}
	return TEST_PASS;
}

static int
curve_client(void)
{
	return framing_case(NULL, true);
}

static int
curve_server(void)
{
	return framing_case(NULL, false);
}

static int
sntrup_client(void)
{
	return framing_case(&params[0], true);
}

static int
sntrup_server(void)
{
	return framing_case(&params[0], false);
}

static int
mlkem_client(void)
{
	return framing_case(&params[1], true);
}

static int
mlkem_server(void)
{
	return framing_case(&params[1], false);
}

static int
test_backend_initialization(void)
{
	const struct hybrid_pq_params *methods[] = {NULL, &params[0], &params[1]};
	for (size_t method = 0; method < sizeof(methods) / sizeof(methods[0]); method++) {
		for (unsigned role = 0; role < 2; role++) {
			for (unsigned mode = 0; mode < 5; mode++) {
				struct framing_fixture fixture = {.backend_mode = mode};
				active = &fixture;
				void *state = NULL;
				int ret = methods[method] == NULL
				    ? curve25519_init_impl(role != 0, NULL, &state, &curve_ops)
				    : hybrid_pq_init_impl(role != 0, NULL, &state, methods[method], &hybrid_ops);
				int expected = mode >= 2 ? (mode == 4 ? DSSH_ERROR_INVALID : DSSH_ERROR_INIT) : 0;
				bool valid = ret == expected && fixture.backend_inits == 1 && (ret != 0 || state != NULL);
				if (valid && ret == 0) {
					void *original = state;
					dssh_test_alloc_fail_after(0);
					ret = methods[method] == NULL
					    ? curve25519_init_impl(role != 0, NULL, &state, &curve_ops)
					    : hybrid_pq_init_impl(role != 0, NULL, &state, methods[method], &hybrid_ops);
					int allocations = dssh_test_alloc_count();
					dssh_test_alloc_reset();
					valid = valid && ret == 0 && state == original && allocations == 0
					    && fixture.backend_inits == 1 && fixture.backend_cleanups == 0;
				}
				else
					valid = valid && state == NULL;
				if (methods[method] == NULL)
					curve25519_cleanup(state);
				else
					hybrid_pq_cleanup(state);
				valid = valid && fixture.live == 0 && fixture.backend_cleanups == (mode == 2 ? 0U : 1U);
				active = NULL;
				ASSERT_TRUE(valid);
			}
		}
	}
	return TEST_PASS;
}

static int
test_hybrid_raw_key_storage(void)
{
	for (size_t method = 0; method < sizeof(params) / sizeof(params[0]); method++) {
		struct hybrid_pq_params raw = params[method];
		raw.pq_sk_len = 64;
		ASSERT_EQ(framing_case(&raw, true), TEST_PASS);
	}
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{ "methods/backend_initialization", test_backend_initialization },
	{ "hybrid/raw_key_storage", test_hybrid_raw_key_storage },
	{ "host/callback_results", host_callback_results },
	{ "host/callback_outputs", host_callback_outputs },
	{ "curve/client", curve_client }, { "curve/server", curve_server },
	{ "sntrup/client", sntrup_client }, { "sntrup/server", sntrup_server },
	{ "mlkem/client", mlkem_client }, { "mlkem/server", mlkem_server },
};

void
dssh_test_failure_cleanup(void)
{
	/* Contexts and result leases have already retired; end the stack borrow. */
	active = NULL;
	dssh_test_alloc_reset();
}

DSSH_TEST_MAIN(tests)
