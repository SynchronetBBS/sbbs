/* Paired actual DH/Ed25519 methods with bounded memory packet callbacks. */
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
#include "kex/dh-gex-groups.h"

#define PACKET_CAPACITY 4096
static unsigned int fixed_group;
struct dh_pair;
struct endpoint {
	struct dh_pair *pair;
	unsigned side;
	uint8_t received[PACKET_CAPACITY];
};
struct packet {
	uint8_t bytes[PACKET_CAPACITY];
	size_t length;
	bool pending;
};
struct dh_pair {
	dssh_kex method;
	dssh_key_algo key;
	void *state[2];
	struct dssh_kex_result output[2];
	struct dssh_dh_gex_provider provider;
	mtx_t mutex;
	cnd_t condition;
	bool mutex_ready;
	bool condition_ready;
	struct endpoint endpoint[2];
	struct packet outgoing[2];
	struct packet trace[4]; /* request, group, init, reply */
	bool finished[2];
	int status[2];
	unsigned group_index;
	unsigned fixed_group;
	unsigned fault;
	unsigned host_checks;
	unsigned mutations;
	unsigned fault_side;
	uint8_t fingerprint[32];
};

void
dssh_test_failure_cleanup(void)
{
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
}

static bool
setup(void)
{
	dssh_test_reset_global_config();
	int ret = fixed_group == 14 ? dssh_register_dh_group14_sha256()
	    : fixed_group == 16 ? dssh_register_dh_group16_sha512() : dssh_register_dh_gex_sha256();
	return ret == 0 && dssh_register_ssh_ed25519() == 0
	    && dssh_ed25519_generate_key() == 0;
}

static void
release_static(void *owner)
{
	(void)owner;
}

static int
select_group(uint32_t min, uint32_t preferred, uint32_t max,
    struct dssh_dh_gex_group_result *out, void *opaque)
{
	struct dh_pair *pair = opaque;
	static const uint8_t generator = 2;
	const struct rfc3526_group *group = &rfc3526_groups[pair->group_index];
	if (min > preferred || preferred > max || group->bits < min || group->bits > max)
		return DSSH_ERROR_INVALID;
	out->p = (struct dssh_bytes){group->p, group->p_len};
	out->g = (struct dssh_bytes){&generator, 1};
	out->lease = (struct dssh_result_lease){release_static, NULL};
	return 0;
}

static void
pair_release(struct dh_pair *pair)
{
	for (size_t i = 0; i < 2; i++) {
		dssh_result_release(&pair->output[i].lease);
		pair->output[i] = (struct dssh_kex_result){0};
	}
}

static void
pair_destroy(struct dh_pair *pair)
{
	pair_release(pair);
	if (pair->method != NULL) {
		for (size_t i = 0; i < 2; i++)
			pair->method->cleanup(pair->state[i]);
	}
	if (pair->condition_ready)
		cnd_destroy(&pair->condition);
	if (pair->mutex_ready)
		mtx_destroy(&pair->mutex);
}

static bool
pair_create(struct dh_pair *pair)
{
	*pair = (struct dh_pair){.method = gconf.kex_head, .key = gconf.key_algo_head,
	    .fixed_group = fixed_group, .group_index = fixed_group == 16 ? 2U : 0U};
	pair->provider = (struct dssh_dh_gex_provider){select_group, pair};
	if (mtx_init(&pair->mutex, mtx_plain) != thrd_success)
		return false;
	pair->mutex_ready = true;
	if (cnd_init(&pair->condition) != thrd_success)
		return false;
	pair->condition_ready = true;
	for (unsigned i = 0; i < 2; i++) {
		pair->endpoint[i].pair = pair;
		pair->endpoint[i].side = i;
	}
	for (unsigned i = 0; i < 2; i++) {
		if (pair->method->init(i == 0, &pair->provider, &pair->state[i]) != 0)
			return false;
	}
	return true;
}

static void
unlock(struct dh_pair *pair)
{
	if (mtx_unlock(&pair->mutex) != thrd_success)
		abort();
}

static int
memory_send(const uint8_t *data, size_t length, void *opaque)
{
	struct endpoint *endpoint = opaque;
	struct dh_pair *pair = endpoint->pair;
	unsigned side = endpoint->side;
	if (length == 0 || length > PACKET_CAPACITY)
		return DSSH_ERROR_TOOLONG;
	unsigned slot;
	if (pair->fixed_group != 0) {
		if (data[0] != 30 && data[0] != 31)
			return DSSH_ERROR_PARSE;
		slot = data[0] == 30 ? 2U : 3U;
	}
	else {
		switch (data[0]) {
			case 34:
				slot = 0;
				break;
			case 31:
				slot = 1;
				break;
			case 32:
				slot = 2;
				break;
			case 33:
				slot = 3;
				break;
			default:
				return DSSH_ERROR_PARSE;
		}
	}
	if (pair->fault == 6 && slot == 3)
		return DSSH_ERROR_INIT;
	if (mtx_lock(&pair->mutex) != thrd_success)
		abort();
	struct packet *packet = &pair->outgoing[side];
	while (packet->pending && !pair->finished[1 - side]) {
		if (cnd_wait(&pair->condition, &pair->mutex) != thrd_success)
			abort();
	}
	if (pair->finished[1 - side]) {
		unlock(pair);
		return DSSH_ERROR_INIT;
	}
	memcpy(packet->bytes, data, length);
	packet->length = length;
	packet->pending = true;
	memcpy(pair->trace[slot].bytes, data, length);
	pair->trace[slot].length = length;
	if (cnd_broadcast(&pair->condition) != thrd_success)
		abort();
	unlock(pair);
	return 0;
}

static int
replace_public(struct endpoint *endpoint, size_t *length)
{
	struct dh_pair *pair = endpoint->pair;
	const struct rfc3526_group *group = &rfc3526_groups[pair->group_index];
	uint8_t *message = endpoint->received;
	size_t pos = 1;
	if (*length < 5)
		return DSSH_ERROR_INIT;
	if (message[0] == 31) {
		if (message[1] != 0 || message[2] != 0)
			return DSSH_ERROR_INIT;
		size_t host_length = message[3];
		host_length = host_length * 256 + message[4];
		if (host_length > *length - 5)
			return DSSH_ERROR_INIT;
		pos = 5 + host_length;
	}
	if (pos > *length || *length - pos < 4 || message[pos] != 0 || message[pos + 1] != 0)
		return DSSH_ERROR_INIT;
	size_t old_length = message[pos + 2];
	old_length = old_length * 256 + message[pos + 3];
	if (old_length > *length - pos - 4)
		return DSSH_ERROR_INIT;
	uint8_t value[514] = {0};
	size_t count = 0;
	if (pair->fault == 9) {
		value[0] = 1;
		count = 1;
	}
	else if (pair->fault >= 10 && pair->fault <= 12) {
		count = 1 + group->p_len;
		memcpy(&value[1], group->p, group->p_len);
		if (pair->fault == 10)
			value[count - 1]--; /* These fixed primes end in FF. */
		if (pair->fault == 12) {
			for (size_t i = count; i != 0; i--) {
				if (value[i - 1] != UINT8_MAX) {
					value[i - 1]++;
					break;
				}
				value[i - 1] = 0;
			}
		}
	}
	else if (pair->fault == 13) {
		value[0] = 0x80;
		count = 1;
	}
	else if (pair->fault == 14) {
		value[1] = 2; /* Unnecessary positive sign byte. */
		count = 2;
	}
	/* fault8 uses the canonical zero mpint. All fixture bounds fit16 bits. */
	size_t tail = *length - pos - 4 - old_length;
	if (count > PACKET_CAPACITY - pos - 4 || tail > PACKET_CAPACITY - pos - 4 - count)
		return DSSH_ERROR_INIT;
	memmove(&message[pos + 4 + count], &message[pos + 4 + old_length], tail);
	memcpy(&message[pos + 4], value, count);
	uint8_t high = (uint8_t)(count >> 8);
	uint8_t low = (uint8_t)(count & 255);
	message[pos + 2] = high;
	message[pos + 3] = low;
	*length = pos + 4 + count + tail;
	pair->mutations++;
	return 0;
}

static int
memory_receive(uint8_t *type, uint8_t **payload, size_t *length, void *opaque)
{
	struct endpoint *endpoint = opaque;
	struct dh_pair *pair = endpoint->pair;
	unsigned side = endpoint->side;
	if (mtx_lock(&pair->mutex) != thrd_success)
		abort();
	struct packet *packet = &pair->outgoing[1 - side];
	while (!packet->pending && !pair->finished[1 - side]) {
		if (cnd_wait(&pair->condition, &pair->mutex) != thrd_success)
			abort();
	}
	if (!packet->pending) {
		unlock(pair);
		return DSSH_ERROR_INIT;
	}
	memcpy(endpoint->received, packet->bytes, packet->length);
	*length = packet->length;
	packet->pending = false;
	if (cnd_broadcast(&pair->condition) != thrd_success)
		abort();
	unlock(pair);
	*payload = endpoint->received;
	*type = endpoint->received[0];
	uint8_t request_type = pair->fixed_group != 0 ? UINT8_C(30) : UINT8_C(34);
	uint8_t init_type = pair->fixed_group != 0 ? UINT8_C(30) : UINT8_C(32);
	uint8_t reply_type = pair->fixed_group != 0 ? UINT8_C(31) : UINT8_C(33);
	if (pair->fault == 1 && *type == request_type)
		*type = 32;
	if (pair->fault == 2 && *type == 31)
		(*length)--;
	if (pair->fault == 3 && *type == init_type && *length < PACKET_CAPACITY)
		endpoint->received[(*length)++] = 0xff;
	if (pair->fault == 4 && *type == reply_type)
		endpoint->received[*length - 1] ^= 1;
	if (pair->fault == 7 && *type == reply_type && *length < PACKET_CAPACITY)
		endpoint->received[(*length)++] = 0xff;
	if (pair->fixed_group != 0 && pair->fault >= 8 && pair->fault <= 14 && side == pair->fault_side)
		return replace_public(endpoint, length);
	return 0;
}

static dssh_hostkey_decision
host_verify(const char *name, unsigned int bits, const uint8_t *fingerprint,
    const uint8_t *key, size_t length, void *opaque)
{
	struct dh_pair *pair = opaque;
	pair->host_checks++;
	if (strcmp(name, "ssh-ed25519") != 0 || bits == 0 || key == NULL || length != 51)
		return DSSH_HOSTKEY_REJECT;
	memcpy(pair->fingerprint, fingerprint, sizeof(pair->fingerprint));
	return pair->fault == 5 ? DSSH_HOSTKEY_REJECT : DSSH_HOSTKEY_ACCEPT;
}

static int
run_endpoint(void *opaque)
{
	struct endpoint *endpoint = opaque;
	struct dh_pair *pair = endpoint->pair;
	unsigned side = endpoint->side;
	static const uint8_t kexinit[] = {20};
	struct dssh_kex_context ctx = {
		.v_c = "client", .v_c_len = 6, .v_s = "server", .v_s_len = 6,
		.i_c = kexinit, .i_c_len = 1, .i_s = kexinit, .i_s_len = 1,
		.key_algo = pair->key, .config = &pair->provider, .state = pair->state[side],
		.send = memory_send, .recv = memory_receive, .io_ctx = endpoint, .client = side == 0,
		.hostkey_verify = host_verify, .hostkey_verify_cbdata = pair,
	};
	pair->status[side] = pair->method->handler(&ctx, &pair->output[side]);
	if (mtx_lock(&pair->mutex) != thrd_success)
		abort();
	pair->finished[side] = true;
	if (cnd_broadcast(&pair->condition) != thrd_success)
		abort();
	unlock(pair);
	return 0;
}

static int
pair_run(struct dh_pair *pair)
{
	pair->host_checks = 0;
	pair->mutations = 0;
	for (size_t i = 0; i < 2; i++) {
		pair->finished[i] = false;
		pair->outgoing[i].pending = false;
		pair->status[i] = DSSH_ERROR_INIT;
	}
	thrd_t server;
	if (thrd_create(&server, run_endpoint, &pair->endpoint[1]) != thrd_success)
		return DSSH_ERROR_INIT;
	int ret = run_endpoint(&pair->endpoint[0]);
	if (thrd_join(server, NULL) != thrd_success)
		abort();
	if (ret != 0)
		return ret;
	return pair->status[0] != 0 ? pair->status[0] : pair->status[1];
}

static bool
pair_reset(struct dh_pair *pair)
{
	pair_release(pair);
	return pair->method->init(true, &pair->provider, &pair->state[0]) == 0
	    && pair->method->init(false, &pair->provider, &pair->state[1]) == 0;
}

/* Independent bounded serialization: no production parser/serializer is used. */
static void
append_string(uint8_t *out, size_t *pos, const uint8_t *data, size_t length)
{
	/* All fixtures are <=1029 bytes; entire transcript fits PACKET_CAPACITY. */
	uint8_t high = (uint8_t)(length >> 8);
	uint8_t low = (uint8_t)(length & 255);
	out[(*pos)++] = 0;
	out[(*pos)++] = 0;
	out[(*pos)++] = high;
	out[(*pos)++] = low;
	memcpy(&out[*pos], data, length);
	*pos += length;
}

static bool
read_string(const uint8_t *data, size_t length, size_t *pos, struct dssh_bytes *bytes)
{
	if (*pos > length || length - *pos < 4 || data[*pos] != 0 || data[*pos + 1] != 0)
		return false;
	size_t count = data[*pos + 2];
	count = count * 256 + data[*pos + 3];
	*pos += 4;
	if (count > length - *pos)
		return false;
	*bytes = (struct dssh_bytes){&data[*pos], count};
	*pos += count;
	return true;
}

static bool
pair_verify(const struct dh_pair *pair)
{
	const struct rfc3526_group *group = &rfc3526_groups[pair->group_index];
	const struct dssh_kex_result *a = &pair->output[0], *b = &pair->output[1];
	size_t hash_length = pair->fixed_group == 16 ? 64U : 32U;
	if (pair->status[0] != 0 || pair->status[1] != 0 || pair->host_checks != 1
	    || a->shared_secret.len == 0 || a->shared_secret.len > group->p_len
	    || a->shared_secret.len != b->shared_secret.len || a->exchange_hash.len != hash_length || b->exchange_hash.len != hash_length
	    || a->shared_secret.data == b->shared_secret.data || a->exchange_hash.data == b->exchange_hash.data
	    || memcmp(a->shared_secret.data, b->shared_secret.data, a->shared_secret.len) != 0
	    || memcmp(a->exchange_hash.data, b->exchange_hash.data, hash_length) != 0)
		return false;
	const uint8_t request[] = {34, 0,0,8,0, 0,0,16,0, 0,0,32,0};
	if (pair->fixed_group == 0
	    && (pair->trace[0].length != sizeof(request) || memcmp(pair->trace[0].bytes, request, sizeof(request)) != 0))
		return false;
	struct dssh_bytes p, g, e, host, f, signature;
	size_t pos = 1;
	if (pair->fixed_group != 0) {
		if (pair->trace[0].length != 0 || pair->trace[1].length != 0
		    || pair->trace[2].bytes[0] != 30 || pair->trace[3].bytes[0] != 31)
			return false;
	}
	else if (!read_string(pair->trace[1].bytes, pair->trace[1].length, &pos, &p)
	    || !read_string(pair->trace[1].bytes, pair->trace[1].length, &pos, &g)
	    || pos != pair->trace[1].length || p.len != group->p_len + 1 || p.data[0] != 0
	    || memcmp(&p.data[1], group->p, group->p_len) != 0 || g.len != 1 || g.data[0] != 2)
		return false;
	pos = 1;
	if (!read_string(pair->trace[2].bytes, pair->trace[2].length, &pos, &e) || pos != pair->trace[2].length)
		return false;
	pos = 1;
	if (!read_string(pair->trace[3].bytes, pair->trace[3].length, &pos, &host)
	    || !read_string(pair->trace[3].bytes, pair->trace[3].length, &pos, &f)
	    || !read_string(pair->trace[3].bytes, pair->trace[3].length, &pos, &signature)
	    || pos != pair->trace[3].length || host.len != 51 || signature.len == 0)
		return false;
	uint8_t transcript[PACKET_CAPACITY], hash[64], fingerprint[32], encoded[1025];
	pos = 0;
	const uint8_t client[] = {'c','l','i','e','n','t'}, server[] = {'s','e','r','v','e','r'}, init[] = {20};
	append_string(transcript, &pos, client, sizeof(client));
	append_string(transcript, &pos, server, sizeof(server));
	append_string(transcript, &pos, init, 1);
	append_string(transcript, &pos, init, 1);
	append_string(transcript, &pos, host.data, host.len);
	if (pair->fixed_group == 0) {
		memcpy(&transcript[pos], &request[1], 12);
		pos += 12;
		append_string(transcript, &pos, p.data, p.len);
		append_string(transcript, &pos, g.data, g.len);
	}
	append_string(transcript, &pos, e.data, e.len);
	append_string(transcript, &pos, f.data, f.len);
	size_t secret_length = a->shared_secret.len;
	bool pad = (a->shared_secret.data[0] & 128) != 0;
	if (pad)
		encoded[0] = 0;
	memcpy(&encoded[pad ? 1U : 0U], a->shared_secret.data, secret_length);
	secret_length += pad ? 1U : 0U;
	append_string(transcript, &pos, encoded, secret_length);
	bool valid = dssh_hash_oneshot(pair->fixed_group == 16 ? "SHA-512" : "SHA-256", transcript, pos, hash, hash_length) == 0
	    && dssh_hash_oneshot("SHA-256", host.data, host.len, fingerprint, sizeof(fingerprint)) == 0
	    && memcmp(hash, a->exchange_hash.data, hash_length) == 0 && memcmp(fingerprint, pair->fingerprint, 32) == 0;
	dssh_cleanse(transcript, sizeof(transcript));
	dssh_cleanse(encoded, sizeof(encoded));
	return valid;
}

static int
test_reuse(void)
{
	bool valid = setup();
	struct dh_pair pair = {0};
	valid = valid && pair_create(&pair);
	void *states[2] = {pair.state[0], pair.state[1]}, *owners[2] = {NULL};
	uint8_t previous[2][PACKET_CAPACITY] = {{0}};
	size_t lengths[2] = {0};
	for (unsigned round = 0; valid && round < 4; round++) {
		if (round != 0) {
			valid = pair_reset(&pair);
			dssh_test_alloc_fail_after(0);
		}
		int ret = pair_run(&pair);
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		valid = valid && ret == 0 && pair_verify(&pair);
		for (size_t side = 0; valid && side < 2; side++) {
			const struct packet *packet = &pair.trace[side == 0 ? 2 : 3];
			struct dssh_bytes public_key, ignored;
			size_t pos = 1;
			if (side != 0)
				valid = read_string(packet->bytes, packet->length, &pos, &ignored);
			valid = valid && read_string(packet->bytes, packet->length, &pos, &public_key);
			if (!valid)
				break;
			if (round != 0)
				valid = valid && allocations == 0 && pair.state[side] == states[side]
				    && pair.output[side].lease.owner == owners[side]
				    && (public_key.len != lengths[side] || memcmp(public_key.data, previous[side], public_key.len) != 0);
			owners[side] = pair.output[side].lease.owner;
			memcpy(previous[side], public_key.data, public_key.len);
			lengths[side] = public_key.len;
		}
	}
	pair_destroy(&pair);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_failures(void)
{
	for (unsigned fault = 1; fault <= 7; fault++) {
		bool valid = setup();
		struct dh_pair pair = {0};
		valid = valid && pair_create(&pair) && pair_run(&pair) == 0 && pair_verify(&pair) && pair_reset(&pair);
		pair.fault = fault;
		valid = valid && pair_run(&pair) < 0;
		pair.fault = 0;
		valid = valid && pair_reset(&pair) && pair_run(&pair) == 0 && pair_verify(&pair);
		pair_destroy(&pair);
		dssh_test_reset_global_config();
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_group_growth(void)
{
	bool valid = setup();
	struct dh_pair pair = {0};
	valid = valid && pair_create(&pair);
	for (unsigned round = 0; valid && round < 4; round++) {
		pair.group_index = round == 1 || round == 2 ? 1U : 0U;
		if (round != 0)
			valid = pair_reset(&pair);
		if (round >= 2)
			dssh_test_alloc_fail_after(0);
		valid = valid && pair_run(&pair) == 0;
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		valid = valid && pair_verify(&pair) && (round < 2 || allocations == 0);
	}
	pair_destroy(&pair);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_retired_state(void)
{
	bool valid = setup();
	struct dh_pair pair = {0};
	valid = valid && pair_create(&pair) && pair_run(&pair) == 0 && pair_verify(&pair);
	if (valid) {
		for (size_t i = 0; i < 2; i++) {
			pair.method->cleanup(pair.state[i]);
			pair.state[i] = NULL;
		}
		valid = pair_verify(&pair);
		pair.output[0].shared_secret.len = SIZE_MAX;
		pair.output[1].exchange_hash.len = 0;
	}
	pair_destroy(&pair);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#define DH_WORKERS 8
struct dh_worker {
	struct dh_pair pair;
	int result;
};

static int
run_pair_worker(void *opaque)
{
	struct dh_worker *worker = opaque;
	worker->result = pair_run(&worker->pair);
	return 0;
}

static int
test_parallel_sessions(void)
{
	bool valid = setup();
	struct dh_worker workers[DH_WORKERS] = {0};
	unsigned count = 0;
	for (unsigned i = 0; valid && i < DH_WORKERS; i++) {
		valid = pair_create(&workers[i].pair);
		count++;
	}
	struct dssh_bytes_result signatures[DH_WORKERS] = {0};
	const uint8_t digest[32] = {1};
	for (unsigned i = 0; valid && i < DH_WORKERS; i++)
		valid = gconf.key_algo_head->sign(&signatures[i], digest, sizeof(digest), gconf.key_algo_head->ctx) == 0;
	for (unsigned i = 0; i < DH_WORKERS; i++)
		dssh_result_release(&signatures[i].lease);
#ifdef DSSH_CRYPTO_BOTAN
	/* Held result buffers do not retain the temporary native operation keys.
	 * Warm every native owner before the zero-allocation parallel waves. */
	valid = test_warm_native_key_context(gconf.key_algo_head->ctx, DH_WORKERS) == 0 && valid;
#endif
	void *owners[DH_WORKERS][2] = {{0}};
	uint8_t previous[DH_WORKERS][2][1025] = {{{0}}};
	size_t lengths[DH_WORKERS][2] = {{0}};
	for (unsigned wave = 0; valid && wave < 3; wave++) {
		if (wave != 0) {
			dssh_test_alloc_fail_after(0);
			for (unsigned i = 0; i < count; i++)
				valid = pair_reset(&workers[i].pair) && valid;
		}
		thrd_t threads[DH_WORKERS];
		unsigned started = 0;
		for (unsigned i = 0; valid && i < count; i++) {
			if (thrd_create(&threads[i], run_pair_worker, &workers[i]) != thrd_success) {
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
		valid = valid && (wave == 0 || allocations == 0);
		for (unsigned i = 0; i < started; i++) {
			struct dh_pair *pair = &workers[i].pair;
			bool matched = workers[i].result == 0 && pair_verify(pair);
			valid = valid && matched;
			if (!matched)
				continue;
			for (unsigned side = 0; side < 2; side++) {
				const struct packet *packet = &pair->trace[side == 0 ? 2 : 3];
				struct dssh_bytes public_key, ignored;
				size_t pos = 1;
				matched = side == 0 || read_string(packet->bytes, packet->length, &pos, &ignored);
				matched = matched && read_string(packet->bytes, packet->length, &pos, &public_key)
				    && public_key.len <= sizeof(previous[i][side]);
				valid = valid && matched;
				if (!matched)
					continue;
				if (wave != 0)
					valid = valid && pair->output[side].lease.owner == owners[i][side]
					    && (public_key.len != lengths[i][side]
					    || memcmp(public_key.data, previous[i][side], public_key.len) != 0);
				owners[i][side] = pair->output[side].lease.owner;
				memcpy(previous[i][side], public_key.data, public_key.len);
				lengths[i][side] = public_key.len;
				for (unsigned j = 0; j < i; j++)
					valid = valid && owners[i][side] != owners[j][side];
			}
		}
		/* All sixteen results remain live until all producer threads have exited. */
		for (unsigned i = 0; i < count; i++)
			pair_release(&workers[i].pair);
	}
	for (unsigned i = 0; i < count; i++)
		pair_destroy(&workers[i].pair);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid && count == DH_WORKERS);
	return TEST_PASS;
}

static int
test_allocations(void)
{
	for (unsigned phase = 0; phase < 2; phase++) {
		bool succeeded = false;
		for (int call = 0; call < 32; call++) {
			bool valid = setup();
			struct dh_pair pair = {0};
			if (phase == 0)
				dssh_test_alloc_fail_after(call);
			bool created = valid && pair_create(&pair);
			dssh_test_alloc_reset();
			valid = valid && pair.mutex_ready && pair.condition_ready && (phase == 0 || created);
			int ret = DSSH_ERROR_ALLOC;
			if (created) {
				if (phase == 1)
					dssh_test_alloc_fail_after(call);
				ret = pair_run(&pair);
				dssh_test_alloc_reset();
				valid = ret == 0 ? pair_verify(&pair) : ret < 0;
			}
			valid = valid && pair_reset(&pair) && pair_run(&pair) == 0 && pair_verify(&pair);
			pair_destroy(&pair);
			dssh_test_reset_global_config();
			ASSERT_TRUE(valid);
			if (created && (phase == 0 || ret == 0)) {
				succeeded = true;
				break;
			}
		}
		ASSERT_TRUE(succeeded);
	}
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_provider_failures(void)
{
	bool succeeded = false;
	for (int call = 0; call < 256; call++) {
		bool valid = setup();
		struct dh_pair pair = {0};
		valid = valid && pair_create(&pair) && pair_run(&pair) == 0 && pair_verify(&pair) && pair_reset(&pair);
		int ret = DSSH_ERROR_INIT;
		if (valid) {
			dssh_test_ossl_fail_after(call);
			ret = pair_run(&pair);
			dssh_test_ossl_reset();
			valid = ret == 0 ? pair_verify(&pair) : ret < 0;
			valid = valid && pair_reset(&pair) && pair_run(&pair) == 0 && pair_verify(&pair);
		}
		pair_destroy(&pair);
		dssh_test_reset_global_config();
		if (!valid)
			fprintf(stderr, "DH native provider call %d returned %d\n", call, ret);
		ASSERT_TRUE(valid);
		if (ret == 0) {
			succeeded = true;
			break;
		}
	}
	ASSERT_TRUE(succeeded);
	return TEST_PASS;
}

static int
test_constructor_failures(void)
{
	bool valid = setup();
	ASSERT_TRUE(valid);
	for (unsigned role = 0; role < 2; role++) {
		bool succeeded = false;
		for (int call = 0; call < 32; call++) {
			void *state = NULL;
			dssh_test_ossl_fail_after(call);
			int ret = gconf.kex_head->init(role == 0, NULL, &state);
			dssh_test_ossl_reset();
			valid = ret == 0 ? state != NULL : ret < 0 && state == NULL;
			gconf.kex_head->cleanup(state);
			ASSERT_TRUE(valid);
			if (ret == 0) {
				succeeded = true;
				break;
			}
		}
		ASSERT_TRUE(succeeded);
	}
	dssh_test_reset_global_config();
	return TEST_PASS;
}
#endif

static int
test_fixed_bounds(void)
{
	for (unsigned side = 0; side < 2; side++) {
		for (unsigned fault = 8; fault <= 14; fault++) {
			bool valid = setup();
			struct dh_pair pair = {0};
			valid = valid && pair_create(&pair) && pair_run(&pair) == 0 && pair_verify(&pair) && pair_reset(&pair);
			pair.fault = fault;
			pair.fault_side = side;
			valid = valid && pair_run(&pair) < 0 && pair.mutations == 1
			    && pair.status[side] < 0 && pair.output[side].lease.release == NULL
			    && pair.output[side].shared_secret.data == NULL && pair.output[side].exchange_hash.data == NULL;
			pair.fault = 0;
			valid = valid && pair_reset(&pair) && pair_run(&pair) == 0 && pair_verify(&pair);
			pair_destroy(&pair);
			dssh_test_reset_global_config();
			printf("fixed DH bounds: group=%u side=%u case=%u valid=%u\n", fixed_group, side, fault, valid ? 1U : 0U);
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}

static int
test_group14_bounds(void)
{
	fixed_group = 14;
	int ret = test_fixed_bounds();
	fixed_group = 0;
	return ret;
}

static int
test_group16_bounds(void)
{
	fixed_group = 16;
	int ret = test_fixed_bounds();
	fixed_group = 0;
	return ret;
}

static int
test_group14_native_reuse(void)
{
	fixed_group = 14;
	int ret = test_reuse();
	fixed_group = 0;
	return ret;
}
static int
test_group14_failures(void)
{
	fixed_group = 14;
	int ret = test_failures();
	fixed_group = 0;
	return ret;
}
static int
test_group14_retired_state(void)
{
	fixed_group = 14;
	int ret = test_retired_state();
	fixed_group = 0;
	return ret;
}
static int
test_group14_parallel_sessions(void)
{
	fixed_group = 14;
	int ret = test_parallel_sessions();
	fixed_group = 0;
	return ret;
}
static int
test_group14_allocations(void)
{
	fixed_group = 14;
	int ret = test_allocations();
	fixed_group = 0;
	return ret;
}
#ifdef DSSH_CRYPTO_OPENSSL
static int
test_group14_provider_failures(void)
{
	fixed_group = 14;
	int ret = test_provider_failures();
	fixed_group = 0;
	return ret;
}
#endif
#ifdef DSSH_CRYPTO_OPENSSL
static int
test_group14_constructor_failures(void)
{
	fixed_group = 14;
	int ret = test_constructor_failures();
	fixed_group = 0;
	return ret;
}
#endif
static int
test_group16_native_reuse(void)
{
	fixed_group = 16;
	int ret = test_reuse();
	fixed_group = 0;
	return ret;
}
static int
test_group16_failures(void)
{
	fixed_group = 16;
	int ret = test_failures();
	fixed_group = 0;
	return ret;
}
static int
test_group16_retired_state(void)
{
	fixed_group = 16;
	int ret = test_retired_state();
	fixed_group = 0;
	return ret;
}
static int
test_group16_parallel_sessions(void)
{
	fixed_group = 16;
	int ret = test_parallel_sessions();
	fixed_group = 0;
	return ret;
}
static int
test_group16_allocations(void)
{
	fixed_group = 16;
	int ret = test_allocations();
	fixed_group = 0;
	return ret;
}
#ifdef DSSH_CRYPTO_OPENSSL
static int
test_group16_provider_failures(void)
{
	fixed_group = 16;
	int ret = test_provider_failures();
	fixed_group = 0;
	return ret;
}
#endif
#ifdef DSSH_CRYPTO_OPENSSL
static int
test_group16_constructor_failures(void)
{
	fixed_group = 16;
	int ret = test_constructor_failures();
	fixed_group = 0;
	return ret;
}
#endif

static struct dssh_test_entry tests[] = {
	{"group14/bounds", test_group14_bounds},
	{"group16/bounds", test_group16_bounds},
	{"group14/native_reuse", test_group14_native_reuse},
	{"group14/failures", test_group14_failures},
	{"group14/retired_state", test_group14_retired_state},
	{"group14/parallel_sessions", test_group14_parallel_sessions},
	{"group14/allocations", test_group14_allocations},
#ifdef DSSH_CRYPTO_OPENSSL
	{"group14/provider_failures", test_group14_provider_failures},
#endif
#ifdef DSSH_CRYPTO_OPENSSL
	{"group14/constructor_failures", test_group14_constructor_failures},
#endif
	{"group16/native_reuse", test_group16_native_reuse},
	{"group16/failures", test_group16_failures},
	{"group16/retired_state", test_group16_retired_state},
	{"group16/parallel_sessions", test_group16_parallel_sessions},
	{"group16/allocations", test_group16_allocations},
#ifdef DSSH_CRYPTO_OPENSSL
	{"group16/provider_failures", test_group16_provider_failures},
#endif
#ifdef DSSH_CRYPTO_OPENSSL
	{"group16/constructor_failures", test_group16_constructor_failures},
#endif

	{"dh/parallel_sessions", test_parallel_sessions},
	{"dh/allocations", test_allocations},
#ifdef DSSH_CRYPTO_OPENSSL
	{"dh/provider_failures", test_provider_failures}, {"dh/constructor_failures", test_constructor_failures},
#endif
	{"dh/native_reuse", test_reuse}, {"dh/failures", test_failures},
	{"dh/group_growth", test_group_growth}, {"dh/retired_state", test_retired_state},
};
DSSH_TEST_MAIN(tests)
