/* Module-boundary tests with real packet preparation and memory I/O. */
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "dssh_test.h"
#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"
#include "dssh_test_internal.h"
#include "deucessh-crypto.h"
#include "deucessh-algorithms.h"
#include "fixture_writer.h"

struct module_fixture {
	uint8_t signature_storage;
	uint8_t wire[1024];
	size_t used;
	size_t read;
	unsigned stage;
	int result;
	unsigned encrypts;
	unsigned decrypts;
	unsigned reads;
	unsigned rx_calls;
	unsigned writes;
	bool receiving;
	bool fired;
	unsigned handlers;
	unsigned init_at;
	unsigned inits;
	unsigned live;
	unsigned watch;
	unsigned verified;
	unsigned authorized;
	bool signature_watched;
	bool signature_released;
	bool signature_cleared;
	bool availability_test;
	int availability;
	unsigned availability_calls;
	unsigned pubkey_calls;
	unsigned sign_calls;
	unsigned kex_allocations;
	unsigned kex_frees;
	unsigned kex_bad_release;
	struct dssh_result_lease *kex_lease;
};

#include "auth_result_fixture.h"
#include "kex_result_fixture.h"

static struct module_fixture *active;

static int
memory_write(const uint8_t *buf, size_t len)
{
	if (active == NULL || active->used > sizeof(active->wire)
	    || len > sizeof(active->wire) - active->used || (buf == NULL && len != 0)
	    || active->writes == UINT_MAX)
		return DSSH_ERROR_TOOLONG;
	if (len != 0)
		memcpy(&active->wire[active->used], buf, len);
	active->used += len;
	active->writes++;
	if (active->stage == 6) {
		active->fired = true;
		return active->result;
	}
	return 0;
}

static int
memory_tx(uint8_t *buf, size_t len, dssh_session sess, void *opaque)
{
	(void)sess;
	(void)opaque;
	return memory_write(buf, len);
}

static int
memory_rx(uint8_t *buf, size_t len, dssh_session sess, void *opaque)
{
	(void)sess;
	(void)opaque;
	if (active == NULL || active->used > sizeof(active->wire) || active->read > active->used
	    || (buf == NULL && len != 0) || active->rx_calls == UINT_MAX || active->reads == UINT_MAX)
		return DSSH_ERROR_INVALID;
	active->rx_calls++;
	if (len > active->used - active->read)
		return DSSH_ERROR_INIT;
	if (len != 0)
		memcpy(buf, &active->wire[active->read], len);
	active->read += len;
	unsigned call = active->reads++;
	if ((active->stage == 7 && call == 0) || (active->stage == 8 && call == 1)) {
		active->fired = true;
		return active->result;
	}
	return 0;
}

static int
memory_line(uint8_t *buf, size_t len, size_t *received, dssh_session sess, void *opaque)
{
	(void)buf;
	(void)len;
	(void)received;
	(void)sess;
	(void)opaque;
	return DSSH_ERROR_INIT;
}

static int
memory_gather(const struct dssh_tx_iov *iov, size_t count, dssh_session sess, void *opaque)
{
	(void)sess;
	(void)opaque;
	if (active == NULL || (iov == NULL && count != 0))
		return DSSH_ERROR_INVALID;
	for (size_t i = 0; i < count; i++) {
		int ret = memory_write(iov[i].base, iov[i].len);
		if (ret != 0)
			return ret;
	}
	if (active->stage == 9) {
		active->fired = true;
		return active->result;
	}
	return 0;
}

static int
stateless_encrypt(uint8_t *buf, size_t len, dssh_enc_ctx *ctx)
{
	if (active == NULL || ctx != NULL || (buf == NULL && len != 0) || active->encrypts == UINT_MAX)
		return DSSH_ERROR_INVALID;
	active->encrypts++;
	for (size_t i = 0; i < len; i++)
		buf[i] ^= 0x5a;
	if (active->stage == 1) {
		active->fired = true;
		return active->result;
	}
	return 0;
}

static int
stateless_decrypt(uint8_t *buf, size_t len, dssh_enc_ctx *ctx)
{
	if (active == NULL || ctx != NULL || (buf == NULL && len != 0) || active->decrypts == UINT_MAX)
		return DSSH_ERROR_INVALID;
	unsigned call = active->decrypts++;
	for (size_t i = 0; i < len; i++)
		buf[i] ^= 0x5a;
	if ((active->stage == 2 && call == 0) || (active->stage == 3 && call == 1)) {
		active->fired = true;
		return active->result;
	}
	return 0;
}

static int
stateless_mac(const uint8_t *buf, size_t len, uint8_t *out, dssh_mac_ctx *ctx)
{
	if (active == NULL || ctx != NULL || out == NULL || (buf == NULL && len != 0))
		return DSSH_ERROR_INVALID;
	memset(out, 0, 16);
	for (size_t i = 0; i < len; i++)
		out[i % 16] ^= buf[i];
	if ((active->stage == 4 && !active->receiving) || (active->stage == 5 && active->receiving)) {
		active->fired = true;
		return active->result;
	}
	return 0;
}

static bool
packet_case(bool client_sender, unsigned stage, int result)
{
	dssh_test_reset_global_config();
	struct module_fixture fixture = { .stage = stage, .result = result };
	active = &fixture;
	bool valid = dssh_transport_set_callbacks(memory_tx, memory_rx, memory_line, NULL) == 0;
	if (stage == 9)
		valid = valid && dssh_transport_set_tx_gather(memory_gather) == 0;
	if (sizeof(struct dssh_enc_s) > SIZE_MAX - sizeof("stateless")) {
		active = NULL;
		dssh_test_reset_global_config();
		return false;
	}
	dssh_enc enc = calloc(1, sizeof(*enc) + sizeof("stateless"));
	if (sizeof(struct dssh_mac_s) > SIZE_MAX - sizeof("stateless")) {
		free(enc);
		active = NULL;
		dssh_test_reset_global_config();
		return false;
	}
	dssh_mac mac = calloc(1, sizeof(*mac) + sizeof("stateless"));
	dssh_session sender = dssh_session_init(client_sender, 0);
	dssh_session receiver = dssh_session_init(!client_sender, 0);
	valid = valid && enc != NULL && mac != NULL && sender != NULL && receiver != NULL;
	if (valid) {
		memcpy(enc->name, "stateless", sizeof("stateless"));
		memcpy(mac->name, "stateless", sizeof("stateless"));
		enc->blocksize = 16;
		enc->bytes_per_key = UINT64_MAX;
		enc->encrypt = stateless_encrypt;
		enc->decrypt = stateless_decrypt;
		mac->digest_size = 16;
		mac->generate = stateless_mac;
		dssh_session peers[] = { sender, receiver };
		for (size_t i = 0; i < sizeof(peers) / sizeof(peers[0]); i++) {
			peers[i]->trans.enc_c2s_selected = enc;
			peers[i]->trans.enc_s2c_selected = enc;
			peers[i]->trans.mac_c2s_selected = mac;
			peers[i]->trans.mac_s2c_selected = mac;
			peers[i]->trans.enc_c2s_active = enc;
			peers[i]->trans.enc_s2c_active = enc;
			peers[i]->trans.mac_c2s_active = mac;
			peers[i]->trans.mac_s2c_active = mac;
			int buffers = reserve_mac_buffers(peers[i], 16);
			valid = buffers == 0 && peers[i]->trans.rx_mac_buf != NULL
			    && peers[i]->trans.rx_mac_computed != NULL && peers[i]->trans.rx_mac_capacity == 16 && valid;
		}
	}
	if (valid) {
		/* More than one block, with transport overhead and a MAC. */
		uint8_t payload[40];
		memset(payload, 0x33, sizeof(payload));
		int ret = send_packet(sender, payload, sizeof(payload), NULL);
		bool rx_fault = stage == 2 || stage == 3 || stage == 5 || stage == 7 || stage == 8;
		int expected = result > 0 ? DSSH_ERROR_INVALID : result;
		if (stage != 0 && !rx_fault)
			valid = ret == expected && dssh_session_is_terminated(sender)
			    && !tx_owner_is_current(sender);
		else {
			valid = ret == 0 && fixture.encrypts == 1 && fixture.writes == 1;
			fixture.receiving = true;
			uint8_t type;
			uint8_t *received = NULL;
			size_t length = 0;
			if (valid) {
				ret = recv_packet(receiver, &type, &received, &length);
				if (stage == 0)
					valid = ret == 0 && fixture.decrypts == 2 && type == payload[0] && received != NULL && length == sizeof(payload)
					    && memcmp(received, payload, sizeof(payload)) == 0
					    && !dssh_session_is_terminated(receiver);
				else
					valid = ret == expected && dssh_session_is_terminated(receiver);
			}
		}
	}
	valid = fixture.fired == (stage != 0) && valid;
	dssh_session_cleanup(receiver);
	dssh_session_cleanup(sender);
	free(enc);
	free(mac);
	active = NULL;
	dssh_test_reset_global_config();
	if (!valid && fprintf(stderr, "sender=%d stage=%u result=%d enc=%u dec=%u read=%u write=%u\n",
		    client_sender, stage, result, fixture.encrypts, fixture.decrypts, fixture.reads, fixture.writes) < 0)
		return false;
	return valid;
}

static int
test_packet_results(void)
{
	const int results[] = { 1, DSSH_ERROR_INIT, DSSH_ERROR_TOOLONG, DSSH_ERROR_REKEY_NEEDED };
	for (unsigned role = 0; role < 2; role++) {
		ASSERT_TRUE(packet_case(role != 0, 0, 0));
		for (unsigned stage = 1; stage <= 9; stage++) {
			for (size_t result = 0; result < sizeof(results) / sizeof(results[0]); result++)
				ASSERT_TRUE(packet_case(role != 0, stage, results[result]));
		}
	}
	return TEST_PASS;
}

static int
enc_init(const uint8_t *key, const uint8_t *iv, bool direction, dssh_enc_ctx **out)
{
	(void)key;
	(void)iv;
	(void)direction;
	*out = malloc(1);
	if (*out == NULL)
		return DSSH_ERROR_ALLOC;
	active->live++;
	return active->inits++ == active->init_at ? active->result : 0;
}

static int
mac_init(const uint8_t *key, dssh_mac_ctx **out)
{
	(void)key;
	*out = malloc(1);
	if (*out == NULL)
		return DSSH_ERROR_ALLOC;
	active->live++;
	return active->inits++ == active->init_at ? active->result : 0;
}

static void
enc_cleanup(dssh_enc_ctx *ctx)
{
	if (ctx != NULL) {
		if (active == NULL || active->live == 0)
			abort();
		active->live--;
		free(ctx);
	}
}

static void
mac_cleanup(dssh_mac_ctx *ctx)
{
	if (ctx != NULL) {
		if (active == NULL || active->live == 0)
			abort();
		active->live--;
		free(ctx);
	}
}

/* Admission must leave a rejected producer-owned definition and the existing
 * registry untouched. Zero-key/stateless definitions retain their contract. */
static int
test_keyed_registration(void)
{
	unsigned checked = 0;
	unsigned mismatches = 0;
	for (unsigned keyed = 0; keyed < 2; keyed++) {
		for (unsigned mask = 0; mask < 8; mask++) {
			dssh_test_reset_global_config();
			ASSERT_EQ(dssh_register_none_enc(), 0);
			dssh_enc head = gconf.enc_head;
			ASSERT_NOT_NULL(head);
			if (sizeof(struct dssh_enc_s) > SIZE_MAX - sizeof("admission")) {
				dssh_test_reset_global_config();
				return TEST_FAIL;
			}
			dssh_enc candidate = calloc(1, sizeof(*candidate) + sizeof("admission"));
			ASSERT_NOT_NULL(candidate);
			memcpy(candidate->name, "admission", sizeof("admission"));
			candidate->key_size = keyed != 0 ? 16 : 0;
			candidate->blocksize = 16;
			candidate->bytes_per_key = UINT64_MAX;
			candidate->init = (mask & 1U) != 0 ? enc_init : NULL;
			candidate->encrypt = (mask & 2U) != 0 ? stateless_encrypt : NULL;
			candidate->decrypt = (mask & 4U) != 0 ? stateless_decrypt : NULL;
			bool accepted = keyed == 0 || mask == 7;
			int expected = accepted ? 0 : DSSH_ERROR_INVALID;
			dssh_test_alloc_fail_after(0);
			int ret = dssh_transport_register_enc(candidate);
			int calls = dssh_test_alloc_count();
			dssh_test_alloc_reset();
			bool valid = ret == expected && calls == 0 && gconf.enc_head == head
			    && candidate->next == NULL && candidate->key_size == (keyed != 0 ? 16 : 0)
			    && candidate->init == ((mask & 1U) != 0 ? enc_init : NULL)
			    && candidate->encrypt == ((mask & 2U) != 0 ? stateless_encrypt : NULL)
			    && candidate->decrypt == ((mask & 4U) != 0 ? stateless_decrypt : NULL)
			    && strcmp(candidate->name, "admission") == 0;
			bool published = head->next == candidate;
			if (published)
				valid = ret == 0 && gconf.enc_tail == candidate && gconf.enc_entries == 2 && valid;
			else {
				valid = ret != 0 && gconf.enc_tail == head && head->next == NULL && gconf.enc_entries == 1 && valid;
				free(candidate);
			}
			if (!valid) {
				fprintf(stderr, "enc admission key=%u mask=%u result=%d expected=%d\n", keyed, mask, ret, expected);
				mismatches++;
			}
			checked++;
			dssh_test_reset_global_config();
		}
		for (unsigned tagged = 0; tagged < 2; tagged++) {
			for (unsigned mask = 0; mask < 4; mask++) {
				dssh_test_reset_global_config();
				ASSERT_EQ(dssh_register_none_mac(), 0);
				dssh_mac head = gconf.mac_head;
				ASSERT_NOT_NULL(head);
				if (sizeof(struct dssh_mac_s) > SIZE_MAX - sizeof("admission")) {
					dssh_test_reset_global_config();
					return TEST_FAIL;
				}
				dssh_mac candidate = calloc(1, sizeof(*candidate) + sizeof("admission"));
				ASSERT_NOT_NULL(candidate);
				memcpy(candidate->name, "admission", sizeof("admission"));
				candidate->key_size = keyed != 0 ? 16 : 0;
				candidate->digest_size = tagged != 0 ? 16 : 0;
				candidate->init = (mask & 1U) != 0 ? mac_init : NULL;
				candidate->generate = (mask & 2U) != 0 ? stateless_mac : NULL;
				bool accepted = (keyed == 0 || (mask & 1U) != 0) && (tagged == 0 || (mask & 2U) != 0);
				int expected = accepted ? 0 : DSSH_ERROR_INVALID;
				dssh_test_alloc_fail_after(0);
				int ret = dssh_transport_register_mac(candidate);
				int calls = dssh_test_alloc_count();
				dssh_test_alloc_reset();
				bool valid = ret == expected && calls == 0 && gconf.mac_head == head
				    && candidate->next == NULL && candidate->key_size == (keyed != 0 ? 16 : 0)
				    && candidate->digest_size == (tagged != 0 ? 16 : 0)
				    && candidate->init == ((mask & 1U) != 0 ? mac_init : NULL)
				    && candidate->generate == ((mask & 2U) != 0 ? stateless_mac : NULL)
				    && strcmp(candidate->name, "admission") == 0;
				bool published = head->next == candidate;
				if (published)
					valid = ret == 0 && gconf.mac_tail == candidate && gconf.mac_entries == 2 && valid;
				else {
					valid = ret != 0 && gconf.mac_tail == head && head->next == NULL && gconf.mac_entries == 1 && valid;
					free(candidate);
				}
				if (!valid) {
					fprintf(stderr, "mac admission key=%u tag=%u mask=%u result=%d expected=%d\n",
					    keyed, tagged, mask, ret, expected);
					mismatches++;
				}
				checked++;
				dssh_test_reset_global_config();
			}
		}
	}
	if (fprintf(stderr, "keyed_admission cases=%u mismatches=%u\n", checked, mismatches) < 0
	    || fflush(stderr) != 0)
		return TEST_FAIL;
	ASSERT_EQ_U(checked, 32);
	ASSERT_EQ_U(mismatches, 0);
	return TEST_PASS;
}

static int
test_init_results(void)
{
	const int results[] = { 1, DSSH_ERROR_INIT, DSSH_ERROR_TOOLONG, DSSH_ERROR_REKEY_NEEDED };
	for (unsigned stage = 0; stage < 4; stage++) {
		for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); i++) {
			dssh_test_reset_global_config();
			struct module_fixture fixture = { .init_at = stage, .result = results[i] };
			active = &fixture;
			struct dssh_enc_s enc = { .init = enc_init, .cleanup = enc_cleanup, .key_size = 1, .blocksize = 16 };
			struct dssh_mac_s mac = { .init = mac_init, .cleanup = mac_cleanup, .key_size = 1, .digest_size = 16 };
			struct dssh_kex_s method = { .hash_name = "SHA-256" };
			dssh_session sess = dssh_session_init(true, 0);
			if (sess == NULL) {
				active = NULL;
				dssh_test_reset_global_config();
				return TEST_FAIL;
			}
			sess->trans.kex_selected = &method;
			sess->trans.enc_c2s_selected = &enc;
			sess->trans.enc_s2c_selected = &enc;
			sess->trans.mac_c2s_selected = &mac;
			sess->trans.mac_s2c_selected = &mac;
			uint8_t hash[32] = {0};
			sess->trans.kex_result.exchange_hash.data = hash;
			sess->trans.kex_result.exchange_hash.len = sizeof(hash);
			sess->trans.session_id = hash;
			sess->trans.session_id_sz = sizeof(hash);
			const uint8_t secret[] = { 0, 0, 0, 1, 1 };
			int ret = derive_and_apply_keys(sess, secret, sizeof(secret), NULL, NULL, NULL, NULL);
			bool valid = ret == (results[i] > 0 ? DSSH_ERROR_INVALID : results[i])
			    && fixture.inits == stage + 1;
			sess->trans.kex_result.exchange_hash.data = NULL;
			sess->trans.session_id = NULL;
			dssh_session_cleanup(sess);
			valid = valid && fixture.live == 0;
			if (!valid)
				fprintf(stderr, "init stage=%u result=%d ret=%d calls=%u live=%u\n",
				    stage, results[i], ret, fixture.inits, fixture.live);
			active = NULL;
			dssh_test_reset_global_config();
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}

static void
observe_kex_result(const void *storage, size_t size, void *data)
{
	struct module_fixture *fixture = data;
	fixture->kex_frees++;
	if (fixture->kex_lease == NULL || fixture->kex_lease->release != NULL
	    || fixture->kex_lease->owner != NULL)
		fixture->kex_bad_release++;
	const uint8_t *bytes = storage;
	for (size_t i = 0; i < size; i++) {
		if (bytes[i] != 0) {
			fixture->kex_bad_release++;
			break;
		}
	}
	dssh_result_release(fixture->kex_lease);
	if (fixture->kex_frees == fixture->kex_allocations)
		fixture->kex_lease = NULL;
}

static uint8_t *
allocate_kex_result(struct dssh_kex_result *out)
{
	uint8_t *storage = test_auth_lease_alloc(&out->lease, 32);
	if (storage != NULL) {
		struct test_auth_output *owner = out->lease.owner;
		owner->observe_release = observe_kex_result;
		owner->observe_data = active;
		active->kex_allocations++;
	}
	return storage;
}

static int
handler(const struct dssh_kex_context *ctx, struct dssh_kex_result *out)
{
	(void)ctx;
	if (active == NULL || out == NULL || active->handlers == UINT_MAX)
		return DSSH_ERROR_INVALID;
	active->handlers++;
	active->kex_lease = &out->lease;
	uint8_t *secret = NULL, *hash = NULL;
	if (active->stage != 1)
		secret = allocate_kex_result(out);
	if (active->stage != 2)
		hash = allocate_kex_result(out);
	if ((active->stage != 1 && secret == NULL) || (active->stage != 2 && hash == NULL))
		return DSSH_ERROR_ALLOC;
	if (secret != NULL)
		memset(secret, 0x42, 32);
	if (hash != NULL)
		memset(hash, 0x33, 32);
	out->shared_secret = (struct dssh_bytes){secret, 32};
	out->exchange_hash = (struct dssh_bytes){hash, 32};
	if (active->stage == 3)
		out->shared_secret.len = 0;
	if (active->stage == 4)
		out->exchange_hash.len = 0;
	if (active->stage == 5)
		out->shared_secret.len = SIZE_MAX;
	if (active->stage == 6)
		out->exchange_hash.len = 31;
	if (active->stage == 7)
		out->exchange_hash.data = out->shared_secret.data;
	if (active->stage == 8)
		out->exchange_hash.len = SIZE_MAX;
	return active->result;
}

static int
test_handler_results(void)
{
	const int results[] = { 0, 1, DSSH_ERROR_INIT };
	for (unsigned watch = 0; watch < 2; watch++) {
		for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); i++) {
			dssh_test_reset_global_config();
			struct module_fixture fixture = { .result = results[i], .watch = watch };
			active = &fixture;
			dssh_session sess = dssh_session_init(true, 0);
			ASSERT_NOT_NULL(sess);
			struct dssh_kex_s method = { .init = test_kex_stateless_init, .handler = handler,
			    .cleanup = test_kex_stateless_cleanup, .hash_name = "SHA-256" };
			sess->trans.kex_selected = &method;
			int ret = kex(sess);
			bool valid = ret == (results[i] > 0 ? DSSH_ERROR_INVALID : results[i])
			    && fixture.handlers == 1 && fixture.kex_allocations == 2;
			if (results[i] != 0)
				valid = valid && sess->trans.kex_result.shared_secret.data == NULL && sess->trans.kex_result.exchange_hash.data == NULL;
			dssh_session_cleanup(sess);
			valid = valid && fixture.kex_allocations == fixture.kex_frees && fixture.kex_bad_release == 0
			    && fixture.kex_lease == NULL;
			dssh_test_alloc_reset();
			active = NULL;
			dssh_test_reset_global_config();
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}

static int
test_handler_outputs(void)
{
	for (unsigned stage = 1; stage <= 8; stage++) {
#if SIZE_MAX <= UINT32_MAX
		if (stage == 5 || stage == 8)
			continue;
#endif
		dssh_test_reset_global_config();
		struct module_fixture fixture = { .stage = stage };
		active = &fixture;
		dssh_session sess = dssh_session_init(true, 0);
		if (sess == NULL) {
			active = NULL;
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		struct dssh_kex_s method = { .init = test_kex_stateless_init, .handler = handler,
			    .cleanup = test_kex_stateless_cleanup, .hash_name = "SHA-256" };
		sess->trans.kex_selected = &method;
		int ret = kex(sess);
		int expected = stage == 7 ? 0 : DSSH_ERROR_INVALID;
		bool valid = ret == expected && fixture.handlers == 1
		    && fixture.kex_allocations == (stage == 1 || stage == 2 ? 1U : 2U);
		if (ret != 0)
			valid = valid && sess->trans.kex_result.shared_secret.data == NULL
			    && sess->trans.kex_result.exchange_hash.data == NULL;
		dssh_session_cleanup(sess);
		valid = valid && fixture.kex_allocations == fixture.kex_frees && fixture.kex_bad_release == 0
			    && fixture.kex_lease == NULL;
		dssh_test_alloc_reset();
		active = NULL;
		dssh_test_reset_global_config();
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
auth_pubkey(const uint8_t **out, size_t *len, dssh_key_algo_ctx *ctx)
{
	(void)ctx;
	active->pubkey_calls++;
	static const uint8_t key = 7;
	if (active->stage == 4)
		return 0;
	if (active->stage != 6)
		*out = &key;
	if (active->stage != 5)
		*len = active->stage == 7 ? SIZE_MAX : 1;
	return active->stage == 1 ? active->result : 0;
}

static void
observe_signature_release(const void *storage, size_t length, void *data)
{
	struct module_fixture *fixture = data;
	const uint8_t *bytes = storage;
	fixture->signature_released = true;
	fixture->signature_cleared = true;
	for (size_t i = 0; i < length; i++) {
		if (bytes[i] != 0)
			fixture->signature_cleared = false;
	}
}

static int
auth_sign(struct dssh_bytes_result *output_result, const uint8_t *data, size_t data_len, dssh_key_algo_ctx *ctx)
{
	active->sign_calls++;
	uint8_t *fixture_output = NULL;
	size_t fixture_length = 0;
	uint8_t **out = &fixture_output;
	size_t *len = &fixture_length;

	(void)ctx;
	(void)data;
	(void)data_len;
	if (active->stage == 12 || active->stage == 13) {
		active->signature_storage = 7;
		if (active->stage == 12) {
			fixture_output = &active->signature_storage;
			fixture_length = 1;
		}
		else
			output_result->lease.owner = active;
		return test_auth_output_finish(output_result, fixture_output, fixture_length, 0);
	}
	if (active->stage == 8)
		return test_auth_output_finish(output_result, fixture_output, fixture_length, 0);
	if (active->stage == 10) {
		*len = 1;
		return test_auth_output_finish(output_result, fixture_output, fixture_length, 0);
	}
	*out = test_auth_output_alloc(output_result, 1);
	if (*out == NULL)
		return test_auth_output_finish(output_result, fixture_output, fixture_length, DSSH_ERROR_ALLOC);
	(*out)[0] = 7;
	struct test_auth_output *owner = output_result->lease.owner;
	owner->observe_release = observe_signature_release;
	owner->observe_data = active;
	active->signature_watched = true;
	if (active->stage != 9)
		*len = active->stage == 11 ? SIZE_MAX : 1;
	return test_auth_output_finish(output_result, fixture_output, fixture_length, active->stage == 2 ? active->result : 0);
}

static int
auth_verify(const uint8_t *key, size_t key_len, const uint8_t *sig, size_t sig_len,
    const uint8_t *data, size_t data_len)
{
	(void)key;
	(void)key_len;
	(void)sig;
	(void)sig_len;
	(void)data;
	(void)data_len;
	active->verified++;
	return active->result;
}

static int
auth_haskey(dssh_key_algo_ctx *ctx)
{
	(void)ctx;
	active->availability_calls++;
	return active->availability_test ? active->availability : 1;
}

static dssh_key_algo
register_auth_module(void)
{
	if (sizeof(struct dssh_key_algo_s) > SIZE_MAX - sizeof("contract-key"))
		return NULL;
	dssh_key_algo ka = calloc(1, sizeof(*ka) + sizeof("contract-key"));
	if (ka == NULL)
		return NULL;
	memcpy(ka->name, "contract-key", sizeof("contract-key"));
	ka->pubkey = auth_pubkey;
	ka->sign = auth_sign;
	ka->verify = auth_verify;
	ka->haskey = auth_haskey;
	ka->flags = DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
	if (dssh_transport_register_key_algo(ka) != 0) {
		free(ka);
		return NULL;
	}
	return ka;
}

static int
test_auth_client_results(void)
{
	const unsigned stages[] = { 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
	const int results[] = { DSSH_ERROR_INIT, 1 };
	for (size_t i = 0; i < sizeof(stages) / sizeof(stages[0]); i++) {
#if SIZE_MAX <= UINT32_MAX
		if (stages[i] == 7)
			continue;
#endif
		unsigned variants = stages[i] <= 2 ? 2 : 1;
		for (unsigned variant = 0; variant < variants; variant++) {
			dssh_test_reset_global_config();
			dssh_test_alloc_reset();
			struct module_fixture fixture = { .stage = stages[i], .result = results[variant] };
			active = &fixture;
			bool valid = dssh_transport_set_callbacks(memory_tx, memory_rx, memory_line, NULL) == 0;
			valid = valid && register_auth_module() != NULL;
			dssh_session sess = dssh_session_init(true, 0);
			valid = valid && sess != NULL;
			if (valid) {
				sess->trans.session_id = malloc(1);
				valid = sess->trans.session_id != NULL;
				if (valid) {
					sess->trans.session_id[0] = 7;
					sess->trans.session_id_sz = 1;
					sess->handshake_complete = true;
					sess->auth_service_requested = true;
					int ret = dssh_auth_publickey(sess, "user", "contract-key");
					int expected = stages[i] <= 2 && variant == 0 ? DSSH_ERROR_INIT : DSSH_ERROR_INVALID;
					unsigned signatures = stages[i] == 2 || stages[i] >= 8 ? 1U : 0U;
					valid = ret == expected && fixture.writes == 0
					    && fixture.availability_calls == 1 && fixture.pubkey_calls == 1
					    && fixture.sign_calls == signatures;
				}
			}
			dssh_session_cleanup(sess);
			if (stages[i] >= 12)
				valid = valid && fixture.signature_storage == 7;
			if (fixture.signature_watched)
				valid = fixture.signature_released && fixture.signature_cleared && valid;
			dssh_test_alloc_reset();
			active = NULL;
			dssh_test_reset_global_config();
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}

static int
accept_publickey(const uint8_t *user, size_t user_len, const char *algorithm,
    const uint8_t *key, size_t key_len, bool signed_request, void *opaque)
{
	(void)user;
	(void)user_len;
	(void)algorithm;
	(void)key;
	(void)key_len;
	(void)signed_request;
	struct module_fixture *fixture = opaque;
	fixture->authorized++;
	return DSSH_AUTH_SUCCESS;
}

static int
test_auth_server_results(void)
{
	const int results[] = { 0, DSSH_ERROR_INIT, 1 };
	for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); i++) {
		dssh_test_reset_global_config();
		struct module_fixture fixture = { .result = results[i] };
		active = &fixture;
		bool valid = dssh_transport_set_callbacks(memory_tx, memory_rx, memory_line, NULL) == 0;
		valid = valid && register_auth_module() != NULL;
		dssh_session sess = dssh_session_init(false, 0);
		valid = valid && sess != NULL;
		if (valid) {
			sess->trans.session_id = malloc(1);
			valid = sess->trans.session_id != NULL;
			if (valid) {
				sess->trans.session_id[0] = 7;
				sess->trans.session_id_sz = 1;
			}
		}
		if (valid) {
			uint8_t request[64];
			struct fixture_writer writer = {.data = request, .capacity = sizeof(request), .valid = true};
			const uint8_t signed_request = 1;
			const uint8_t key = 7;
			const uint8_t name[] = "contract-key";
			valid = fixture_writer_bytes(&writer, &signed_request, 1)
			    && fixture_writer_string(&writer, name, sizeof(name) - 1)
			    && fixture_writer_string(&writer, &key, 1)
			    && fixture_writer_string(&writer, &key, 1);
			struct dssh_auth_server_cbs cbs = {
				.methods_str = "publickey", .publickey_cb = accept_publickey, .cbdata = &fixture,
			};
			const uint8_t username[] = "user";
			int ret = valid ? handle_auth_publickey(sess, &cbs, username, sizeof(username) - 1,
			    request, writer.used, 0) : DSSH_ERROR_INVALID;
			bool success = results[i] == 0;
			/* Handler outcomes: one means completed, zero means continue. */
			valid = valid && ret == (success ? 1 : 0) && fixture.verified == 1
			    && fixture.authorized == (success ? 1U : 0U) && fixture.writes == 1
			    && fixture.used > 5
			    && fixture.wire[5] == (success ? SSH_MSG_USERAUTH_SUCCESS : SSH_MSG_USERAUTH_FAILURE);
		}
		dssh_session_cleanup(sess);
		active = NULL;
		dssh_test_reset_global_config();
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_fixture_writer(void)
{
	uint8_t frame[10];
	memset(frame, 0xa5, sizeof(frame));
	struct fixture_writer writer = {.data = frame + 1, .capacity = 8, .valid = true};
	const uint8_t input[] = {1, 2, 3, 4};
	const uint8_t expected[] = {0, 0, 0, 4, 1, 2, 3, 4};
	bool valid = fixture_writer_string(&writer, input, sizeof(input)) && writer.used == 8
	    && frame[0] == 0xa5 && frame[9] == 0xa5 && memcmp(frame + 1, expected, sizeof(expected)) == 0;
	uint8_t saved[sizeof(frame)];
	memcpy(saved, frame, sizeof(saved));
	valid = !fixture_writer_u32(&writer, 7) && !writer.valid && writer.used == 8
	    && !fixture_writer_bytes(&writer, NULL, 0) && memcmp(frame, saved, sizeof(frame)) == 0 && valid;
	for (size_t capacity = 0; capacity < sizeof(expected); capacity++) {
		memset(frame, 0xa5, sizeof(frame));
		writer = (struct fixture_writer){.data = frame + 1, .capacity = capacity, .valid = true};
		bool rejected = !fixture_writer_string(&writer, input, sizeof(input)) && !writer.valid && writer.used == 0;
		for (size_t i = 0; i < sizeof(frame); i++)
			rejected = frame[i] == 0xa5 && rejected;
		valid = rejected && valid;
	}
	writer = (struct fixture_writer){.data = frame + 1, .capacity = 4, .valid = true};
	const uint8_t empty[] = {0, 0, 0, 0};
	valid = fixture_writer_string(&writer, NULL, 0) && writer.used == 4
	    && memcmp(frame + 1, empty, sizeof(empty)) == 0 && valid;
	writer = (struct fixture_writer){.valid = true};
	valid = fixture_writer_bytes(&writer, NULL, 0) && !fixture_writer_string(&writer, NULL, 1) && valid;
	writer = (struct fixture_writer){.data = frame, .capacity = sizeof(frame), .used = SIZE_MAX, .valid = true};
	valid = !fixture_writer_repeated_string(&writer, 0x11, 1) && !writer.valid && valid;
	writer = (struct fixture_writer){.data = frame, .capacity = sizeof(frame), .valid = true};
	valid = !fixture_writer_repeated_string(&writer, 0x11, SIZE_MAX) && writer.used == 0 && !writer.valid && valid;
	writer = (struct fixture_writer){.data = frame + 1, .capacity = 8, .valid = true};
	valid = fixture_writer_repeated_string(&writer, 0x11, 4) && writer.used == 8 && valid;
	const uint8_t repeated[] = {0, 0, 0, 4, 0x11, 0x11, 0x11, 0x11};
	valid = memcmp(frame + 1, repeated, sizeof(repeated)) == 0 && valid;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#include "module_init_cases.h"
#include "packet_bounds_cases.h"
#include "key_availability_cases.h"
#include "kex_state_cases.h"

static struct dssh_test_entry tests[] = {
	{ "fixture/packet_writer", test_fixture_writer },
	{ "registration/keyed_callbacks", test_keyed_registration },
	{ "kex/state_ownership", test_kex_state_ownership },
	{ "kex/state_init_failure", test_kex_state_init_failure },
	{ "kex/result_lease", test_kex_state_result_lease },
	{ "kex/newkeys_terminated", test_kex_state_newkeys_terminated },
	{ "kex/derivation_release", test_kex_state_derivation_release },
	{ "kex/required_callbacks", test_kex_required_callbacks },
	{ "key_availability/auth", test_key_availability_auth },
	{ "key_availability/offer", test_key_availability_offer },
	{ "packet/metadata_bounds", test_packet_metadata_bounds },
	{ "packet/receive_extents", test_packet_receive_extents },
	{ "packet/receive_length_refusal", test_packet_receive_length_refusal },
	{ "packet/receive_block_bounds", test_receive_block_bounds },
	{ "packet/slot_size_bounds", test_slot_size_bounds },
	{ "handler/digest_failure", test_handler_digest_failure },
	{ "init/mac_buffer_reuse", test_mac_buffer_reuse },
	{ "init/publication", test_init_publication },
	{ "init/rekey_ownership", test_init_rekey_ownership },
	{ "init/preparation_failures", test_init_preparation_failures },
	{ "init/lifecycle", test_init_lifecycle },
	{ "packet/results", test_packet_results },
	{ "auth/client", test_auth_client_results },
	{ "auth/server", test_auth_server_results },
	{ "init/results", test_init_results },
	{ "handler/results", test_handler_results },
	{ "handler/outputs", test_handler_outputs },
};

void
dssh_test_failure_cleanup(void)
{
	/* Every session/context owner is retired before assertions; release all
	 * remaining stack borrows and test-global injection/registration state. */
	active = NULL;
	init_trace = NULL;
	init_lifecycle = NULL;
	packet_input = NULL;
	kex_trace = NULL;
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_test_clock_reset();
	dssh_test_reset_global_config();
}

DSSH_TEST_MAIN(tests)
