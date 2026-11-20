/* Actual Curve25519/Ed25519 handlers with in-memory method packet callbacks.
 * A synchronous client receive drives the real server handler. This isolates
 * method allocation counts from the transport/KDF's own allocations. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "dssh_test.h"
#include "dssh_test_alloc.h"
#include "key_context_fixture.h"
#include "dssh_test_ossl.h"
#include "dssh_test_internal.h"
#include "deucessh-algorithms.h"
#include "deucessh-crypto.h"
#include "kex/curve25519-sha256-ops.h"

struct curve_pair {
	dssh_kex method;
	dssh_key_algo key;
	void *client;
	void *server;
	struct dssh_kex_result client_result;
	struct dssh_kex_result server_result;
	uint8_t init[37];
	uint8_t reply[256];
	uint8_t fingerprint[32];
	size_t reply_len;
	unsigned host_checks;
	unsigned fault;
	int server_status;
	bool valid;
};

void
dssh_test_failure_cleanup(void)
{
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
}

static bool
zero_bytes(const uint8_t *bytes, size_t length)
{
	for (size_t i = 0; i < length; i++) {
		if (bytes[i] != 0)
			return false;
	}
	return true;
}

static bool
setup_algorithms(void)
{
	dssh_test_reset_global_config();
	return dssh_register_curve25519_sha256() == 0 && dssh_register_ssh_ed25519() == 0
	    && dssh_ed25519_generate_key() == 0;
}

static bool
pair_create(struct curve_pair *pair)
{
	*pair = (struct curve_pair){.method = gconf.kex_head, .key = gconf.key_algo_head};
	return pair->method->init(true, pair->method->config, &pair->client) == 0
	    && pair->method->init(false, pair->method->config, &pair->server) == 0;
}

static void
pair_release(struct curve_pair *pair)
{
	dssh_result_release(&pair->client_result.lease);
	dssh_result_release(&pair->server_result.lease);
	pair->client_result = (struct dssh_kex_result){0};
	pair->server_result = (struct dssh_kex_result){0};
}

static void
pair_destroy(struct curve_pair *pair)
{
	pair_release(pair);
	if (pair->method != NULL) {
		pair->method->cleanup(pair->client);
		pair->method->cleanup(pair->server);
	}
	pair->client = NULL;
	pair->server = NULL;
}

static int
server_receive(uint8_t *type, uint8_t **payload, size_t *length, void *opaque)
{
	struct curve_pair *pair = opaque;
	*type = SSH_MSG_KEX_ECDH_INIT;
	*payload = pair->init;
	*length = pair->fault == 2 ? sizeof(pair->init) - 1 : sizeof(pair->init);
	return 0;
}

static int
server_send(const uint8_t *payload, size_t length, void *opaque)
{
	struct curve_pair *pair = opaque;
	if (length > sizeof(pair->reply))
		return DSSH_ERROR_TOOLONG;
	memcpy(pair->reply, payload, length);
	pair->reply_len = length;
	return pair->fault == 3 ? DSSH_ERROR_INIT : 0;
}

static int
client_send(const uint8_t *payload, size_t length, void *opaque)
{
	struct curve_pair *pair = opaque;
	if (length != sizeof(pair->init))
		return DSSH_ERROR_PARSE;
	memcpy(pair->init, payload, length);
	return 0;
}

static struct dssh_kex_context
pair_context(struct curve_pair *pair, bool client)
{
	static const uint8_t kexinit[] = {20};
	struct dssh_kex_context ctx = {
		.v_c = "client", .v_c_len = 6, .v_s = "server", .v_s_len = 6,
		.i_c = kexinit, .i_c_len = sizeof(kexinit), .i_s = kexinit, .i_s_len = sizeof(kexinit),
		.key_algo = pair->key, .config = pair->method->config,
		.state = client ? pair->client : pair->server, .client = client, .io_ctx = pair,
	};
	return ctx;
}

static int
client_receive(uint8_t *type, uint8_t **payload, size_t *length, void *opaque)
{
	struct curve_pair *pair = opaque;
	struct dssh_kex_context ctx = pair_context(pair, false);
	ctx.send = server_send;
	ctx.recv = server_receive;
	pair->server_status = pair->method->handler(&ctx, &pair->server_result);
	if (pair->server_status < 0)
		return pair->server_status;
	if (pair->fault == 5 && pair->reply_len > 0)
		pair->reply[pair->reply_len - 1] ^= 1;
	*type = pair->fault == 1 ? SSH_MSG_KEX_ECDH_INIT : SSH_MSG_KEX_ECDH_REPLY;
	*payload = pair->reply;
	*length = pair->reply_len;
	return 0;
}

static dssh_hostkey_decision
host_verify(const char *name, unsigned int bits, const uint8_t *fingerprint,
    const uint8_t *key, size_t length, void *opaque)
{
	struct curve_pair *pair = opaque;
	pair->valid = pair->valid && strcmp(name, "ssh-ed25519") == 0 && bits != 0 && key != NULL && length == 51;
	memcpy(pair->fingerprint, fingerprint, sizeof(pair->fingerprint));
	pair->host_checks++;
	return pair->fault == 4 ? DSSH_HOSTKEY_REJECT : DSSH_HOSTKEY_ACCEPT;
}

static int
pair_run(struct curve_pair *pair)
{
	pair->reply_len = 0;
	pair->host_checks = 0;
	pair->server_status = DSSH_ERROR_INIT;
	pair->valid = true;
	struct dssh_kex_context ctx = pair_context(pair, true);
	ctx.send = client_send;
	ctx.recv = client_receive;
	ctx.hostkey_verify = host_verify;
	ctx.hostkey_verify_cbdata = pair;
	return pair->method->handler(&ctx, &pair->client_result);
}

static bool
pair_reset(struct curve_pair *pair)
{
	pair_release(pair);
	return pair->method->init(true, pair->method->config, &pair->client) == 0
	    && pair->method->init(false, pair->method->config, &pair->server) == 0;
}

/* Independent transcript serialization for these exact fixed-size fixtures.
 * Ed25519's public blob is 51 bytes; both X25519 public values are 32 bytes. */
static void
append_string(uint8_t *bytes, size_t *pos, const void *data, uint8_t length)
{
	bytes[(*pos)++] = 0;
	bytes[(*pos)++] = 0;
	bytes[(*pos)++] = 0;
	bytes[(*pos)++] = length;
	memcpy(&bytes[*pos], data, length);
	*pos += length;
}

static bool
pair_verify(const struct curve_pair *pair)
{
	const struct dssh_kex_result *a = &pair->client_result;
	const struct dssh_kex_result *b = &pair->server_result;
	if (!pair->valid || pair->server_status != 0 || pair->host_checks != 1
	    || a->shared_secret.len == 0 || a->shared_secret.len > 32
	    || a->shared_secret.len != b->shared_secret.len || a->exchange_hash.len != 32 || b->exchange_hash.len != 32
	    || a->shared_secret.data == b->shared_secret.data || a->exchange_hash.data == b->exchange_hash.data
	    || memcmp(a->shared_secret.data, b->shared_secret.data, a->shared_secret.len) != 0
	    || memcmp(a->exchange_hash.data, b->exchange_hash.data, 32) != 0)
		return false;
	/* Check framing without calling the production parser/serializer. */
	const uint8_t prefix[] = {31, 0, 0, 0, 51};
	const uint8_t public_length[] = {0, 0, 0, 32};
	if (pair->reply_len < 92 || memcmp(pair->reply, prefix, sizeof(prefix)) != 0
	    || memcmp(&pair->reply[56], public_length, sizeof(public_length)) != 0)
		return false;
	uint8_t transcript[256], expected[32], fingerprint[32];
	size_t pos = 0;
	const uint8_t kexinit[] = {20};
	append_string(transcript, &pos, "client", 6);
	append_string(transcript, &pos, "server", 6);
	append_string(transcript, &pos, kexinit, 1);
	append_string(transcript, &pos, kexinit, 1);
	append_string(transcript, &pos, &pair->reply[5], 51);
	append_string(transcript, &pos, &pair->init[5], 32);
	append_string(transcript, &pos, &pair->reply[60], 32);
	uint8_t encoded_secret[33];
	bool pad = (a->shared_secret.data[0] & 0x80) != 0;
	size_t secret_length = a->shared_secret.len + (pad ? 1U : 0U);
	if (pad)
		encoded_secret[0] = 0;
	memcpy(&encoded_secret[pad ? 1U : 0U], a->shared_secret.data, a->shared_secret.len);
	uint8_t secret_length_u8 = (uint8_t)secret_length; /* Proven at most 33. */
	append_string(transcript, &pos, encoded_secret, secret_length_u8);
	bool valid = dssh_hash_oneshot("SHA-256", transcript, pos, expected, sizeof(expected)) == 0
	    && dssh_hash_oneshot("SHA-256", &pair->reply[5], 51, fingerprint, sizeof(fingerprint)) == 0
	    && memcmp(expected, a->exchange_hash.data, sizeof(expected)) == 0
	    && memcmp(fingerprint, pair->fingerprint, sizeof(fingerprint)) == 0;
	dssh_cleanse(transcript, sizeof(transcript));
	dssh_cleanse(encoded_secret, sizeof(encoded_secret));
	return valid;
}

static const uint8_t *
secret_base(const struct dssh_kex_result *result)
{
	/* Built-in workspace owns exactly 32 raw bytes before canonical trimming. */
	return result->shared_secret.data - (32 - result->shared_secret.len);
}

static int
test_native_reuse(void)
{
	bool valid = setup_algorithms();
	struct curve_pair pair = {0};
	valid = valid && pair_create(&pair);
	uint8_t previous_public[2][32] = {{0}};
	const uint8_t *secret[2] = {NULL}, *hash[2] = {NULL};
	void *client_state = pair.client, *server_state = pair.server;
	for (unsigned round = 0; valid && round < 4; round++) {
		if (round != 0) {
			valid = pair_reset(&pair) && pair.client == client_state && pair.server == server_state;
			dssh_test_alloc_fail_after(0);
		}
		int ret = pair_run(&pair);
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		valid = valid && ret == 0 && pair_verify(&pair);
		if (!valid)
			break;
		const struct dssh_kex_result *results[] = {&pair.client_result, &pair.server_result};
		for (size_t i = 0; i < 2; i++) {
			const uint8_t *base = secret_base(results[i]);
			if (round != 0)
				valid = valid && base == secret[i] && results[i]->exchange_hash.data == hash[i];
			secret[i] = base;
			hash[i] = results[i]->exchange_hash.data;
		}
		if (round != 0)
			valid = valid && allocations == 0 && memcmp(previous_public[0], &pair.init[5], 32) != 0
			    && memcmp(previous_public[1], &pair.reply[60], 32) != 0;
		memcpy(previous_public[0], &pair.init[5], 32);
		memcpy(previous_public[1], &pair.reply[60], 32);
		pair_release(&pair);
		for (size_t i = 0; i < 2; i++)
			valid = valid && zero_bytes(secret[i], 32) && zero_bytes(hash[i], 32);
	}
	pair_destroy(&pair);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_method_errors(void)
{
	for (unsigned fault = 1; fault <= 5; fault++) {
		bool valid = setup_algorithms();
		struct curve_pair pair = {0};
		valid = valid && pair_create(&pair) && pair_run(&pair) == 0 && pair_verify(&pair) && pair_reset(&pair);
		if (valid) {
			pair.fault = fault;
			int ret = pair_run(&pair);
			valid = ret < 0 && pair.client_result.lease.release == NULL
			    && pair.client_result.shared_secret.data == NULL && pair.client_result.exchange_hash.data == NULL;
			if (fault == 2 || fault == 3)
				valid = valid && pair.server_result.lease.release == NULL;
			pair.fault = 0;
			valid = valid && pair_reset(&pair) && pair_run(&pair) == 0 && pair_verify(&pair);
		}
		pair_destroy(&pair);
		dssh_test_reset_global_config();
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_allocations(void)
{
	bool succeeded = false;
	for (int allocation = 0; allocation < 32; allocation++) {
		bool valid = setup_algorithms();
		struct curve_pair pair = {0};
		valid = valid && pair_create(&pair);
		int ret = DSSH_ERROR_INIT;
		if (valid) {
			dssh_test_alloc_fail_after(allocation);
			ret = pair_run(&pair);
			dssh_test_alloc_reset();
			valid = ret == 0 ? pair_verify(&pair) : ret == DSSH_ERROR_ALLOC
			    && pair.client_result.lease.release == NULL;
			valid = valid && pair_reset(&pair) && pair_run(&pair) == 0 && pair_verify(&pair);
		}
		pair_destroy(&pair);
		dssh_test_reset_global_config();
		ASSERT_TRUE(valid);
		if (ret == 0) {
			succeeded = true;
			break;
		}
	}
	ASSERT_TRUE(succeeded);
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_provider_failures(void)
{
	bool succeeded = false;
	for (int call = 0; call < 256; call++) {
		bool valid = setup_algorithms();
		struct curve_pair pair = {0};
		valid = valid && pair_create(&pair) && pair_run(&pair) == 0 && pair_verify(&pair) && pair_reset(&pair);
		int ret = DSSH_ERROR_INIT;
		if (valid) {
			dssh_test_ossl_fail_after(call);
			ret = pair_run(&pair);
			dssh_test_ossl_reset();
			valid = ret == 0 ? pair_verify(&pair) : ret < 0 && pair.client_result.lease.release == NULL;
			valid = valid && pair_reset(&pair) && pair_run(&pair) == 0 && pair_verify(&pair);
		}
		pair_destroy(&pair);
		dssh_test_reset_global_config();
		if (!valid)
			fprintf(stderr, "native Curve25519 provider call %d ret %d\n", call, ret);
		ASSERT_TRUE(valid);
		if (ret == 0) {
			succeeded = true;
			break;
		}
	}
	ASSERT_TRUE(succeeded);
	return TEST_PASS;
}
#endif

#define CURVE_WORKERS 8
struct pair_worker {
	struct curve_pair pair;
	int result;
};

static int
pair_worker_run(void *opaque)
{
	struct pair_worker *worker = opaque;
	worker->result = pair_run(&worker->pair);
	return 0;
}

static int
test_parallel_sessions(void)
{
	bool valid = setup_algorithms();
	struct pair_worker workers[CURVE_WORKERS] = {0};
	unsigned count = 0;
	for (unsigned i = 0; valid && i < CURVE_WORKERS; i++) {
		valid = pair_create(&workers[i].pair);
		count++;
	}
	/* Warm sufficient independent signature capacity for every simultaneous
	 * worker, independently of the first wave's scheduling/peak overlap. */
	struct dssh_bytes_result signatures[CURVE_WORKERS] = {0};
	const uint8_t digest[32] = {1};
	for (unsigned i = 0; valid && i < CURVE_WORKERS; i++)
		valid = gconf.key_algo_head->sign(&signatures[i], digest, sizeof(digest), gconf.key_algo_head->ctx) == 0;
	for (unsigned i = 0; i < CURVE_WORKERS; i++)
		dssh_result_release(&signatures[i].lease);
#ifdef DSSH_CRYPTO_BOTAN
	/* Held result buffers do not retain the temporary native operation keys.
	 * Warm every native owner before the zero-allocation parallel waves. */
	valid = test_warm_native_key_context(gconf.key_algo_head->ctx, CURVE_WORKERS) == 0 && valid;
#endif
	const uint8_t *secrets[CURVE_WORKERS][2] = {{0}}, *hashes[CURVE_WORKERS][2] = {{0}};
	uint8_t public_values[CURVE_WORKERS][2][32] = {{{0}}};
	for (unsigned wave = 0; valid && wave < 3; wave++) {
		if (wave != 0) {
			dssh_test_alloc_fail_after(0);
			for (unsigned i = 0; i < count; i++)
				valid = pair_reset(&workers[i].pair) && valid;
		}
		thrd_t threads[CURVE_WORKERS];
		unsigned started = 0;
		for (unsigned i = 0; valid && i < count; i++) {
			if (thrd_create(&threads[i], pair_worker_run, &workers[i]) != thrd_success) {
				valid = false;
				break;
			}
			started++;
		}
		for (unsigned i = 0; i < started; i++) {
			if (thrd_join(threads[i], NULL) != thrd_success)
				abort();
		}
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		if (wave != 0)
			valid = valid && allocations == 0;
		for (unsigned i = 0; i < started; i++) {
			struct curve_pair *pair = &workers[i].pair;
			bool matched = workers[i].result == 0 && pair_verify(pair);
			valid = valid && matched;
			if (!matched)
				continue;
			const struct dssh_kex_result *results[] = {&pair->client_result, &pair->server_result};
			for (unsigned side = 0; side < 2; side++) {
				const uint8_t *base = secret_base(results[side]);
				if (wave != 0)
					valid = valid && base == secrets[i][side] && results[side]->exchange_hash.data == hashes[i][side];
				secrets[i][side] = base;
				hashes[i][side] = results[side]->exchange_hash.data;
				for (unsigned j = 0; j < i; j++)
					valid = valid && base != secrets[j][side] && hashes[i][side] != hashes[j][side];
			}
			if (wave != 0)
				valid = valid && memcmp(public_values[i][0], &pair->init[5], 32) != 0
				    && memcmp(public_values[i][1], &pair->reply[60], 32) != 0;
			memcpy(public_values[i][0], &pair->init[5], 32);
			memcpy(public_values[i][1], &pair->reply[60], 32);
		}
		/* All sixteen results remain live until every producer has exited. */
		for (unsigned i = 0; i < count; i++) {
			pair_release(&workers[i].pair);
			if (workers[i].result == 0) {
				for (unsigned side = 0; side < 2; side++)
					valid = valid && secrets[i][side] != NULL && hashes[i][side] != NULL
					    && zero_bytes(secrets[i][side], 32) && zero_bytes(hashes[i][side], 32);
			}
		}
	}
	for (unsigned i = 0; i < count; i++)
		pair_destroy(&workers[i].pair);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid && count == CURVE_WORKERS);
	return TEST_PASS;
}

struct retired_probe {
	const uint8_t *buffers[4];
	struct dssh_result_lease *leases[4];
	unsigned frees;
	bool valid;
};

static void
observe_retired_free(const void *pointer, void *opaque)
{
	struct retired_probe *probe = opaque;
	for (size_t i = 0; i < 4; i++) {
		if (probe->buffers[i] != NULL && pointer == probe->buffers[i]) {
			probe->valid = probe->valid && zero_bytes(probe->buffers[i], 32)
			    && probe->leases[i]->release == NULL && probe->leases[i]->owner == NULL;
			probe->buffers[i] = NULL;
			probe->frees++;
		}
	}
}

static int
test_retired_native_state(void)
{
	bool valid = setup_algorithms();
	struct curve_pair pair = {0};
	valid = valid && pair_create(&pair) && pair_run(&pair) == 0 && pair_verify(&pair);
	if (valid) {
		struct retired_probe probe = {.buffers = {
			secret_base(&pair.client_result), pair.client_result.exchange_hash.data,
			secret_base(&pair.server_result), pair.server_result.exchange_hash.data,
		}, .leases = {
			&pair.client_result.lease, &pair.client_result.lease,
			&pair.server_result.lease, &pair.server_result.lease,
		}, .valid = true};
		pair.method->cleanup(pair.client);
		pair.method->cleanup(pair.server);
		pair.client = NULL;
		pair.server = NULL;
		valid = pair_verify(&pair);
		pair.client_result.shared_secret.len = SIZE_MAX;
		pair.server_result.exchange_hash.len = 0;
		dssh_test_alloc_observe_frees(observe_retired_free, &probe);
		pair_release(&pair);
		dssh_test_alloc_observe_frees(NULL, NULL);
		valid = valid && probe.valid && probe.frees == 4;
	}
	pair_destroy(&pair);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_constructor_provider_failures(void)
{
	bool succeeded = false;
	for (int call = 0; call < 32; call++) {
		void *state = NULL;
		dssh_test_ossl_fail_after(call);
		int ret = curve25519_init(true, NULL, &state);
		dssh_test_ossl_reset();
		bool valid = ret == 0 ? state != NULL : ret < 0 && state == NULL;
		curve25519_cleanup(state);
		ASSERT_TRUE(valid);
		if (ret == 0) {
			succeeded = true;
			break;
		}
	}
	ASSERT_TRUE(succeeded);
	return TEST_PASS;
}
#endif

static struct dssh_test_entry tests[] = {
	{"curve/native_reuse", test_native_reuse},
	{"curve/method_errors", test_method_errors},
	{"curve/allocations", test_allocations},
	{"curve/parallel_sessions", test_parallel_sessions},
	{"curve/retired_native_state", test_retired_native_state},
#ifdef DSSH_CRYPTO_OPENSSL
	{"curve/provider_failures", test_provider_failures},
	{"curve/constructor_provider_failures", test_constructor_provider_failures},
#endif
};

DSSH_TEST_MAIN(tests)
