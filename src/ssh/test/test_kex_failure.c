/* Real transport handshakes/rekeys; method callbacks supply controlled failures.
 * Peer diagnostic records are emitted only after parsing the actual wire packet. */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "dssh_test.h"
#include "dssh_test_internal.h"
#include "ssh-internal.h"
#include "mock_io.h"
#include "test_dhgex_provider.h"

enum failure_kind {
	FAIL_NONE, FAIL_INIT, FAIL_HANDLER, FAIL_POSITIVE, FAIL_RESULT,
	FAIL_SHORT, FAIL_LONG, FAIL_ZERO, FAIL_TYPE, FAIL_SIGNATURE,
	FAIL_CIPHERTEXT, FAIL_CANONICAL, FAIL_CANCEL, FAIL_PEER
};

struct failure_pair;
struct failure_side {
	struct failure_pair *pair;
	dssh_session session;
	void *inner;
	bool initialized;
	bool faulted;
	bool queue_armed;
	bool valid;
	unsigned generation;
	unsigned cleanups;
	unsigned mutations;
	unsigned tx_after_fault;
	unsigned disconnects;
	uint32_t reason;
	int result;
};

struct failure_pair {
	struct mock_io_state io;
	struct failure_side sides[2];
	dssh_kex delegate;
	enum failure_kind kind;
	bool target_client;
	bool target_rekey;
	bool running_rekey;
	int tx_result;
};

struct failure_method {
	const char *name;
	int (*register_method)(void);
};

static const struct failure_method methods[] = {
	{"curve25519-sha256", dssh_register_curve25519_sha256},
	{"diffie-hellman-group-exchange-sha256", dssh_register_dh_gex_sha256},
	{"diffie-hellman-group14-sha256", dssh_register_dh_group14_sha256},
	{"diffie-hellman-group16-sha512", dssh_register_dh_group16_sha512},
	{"sntrup761x25519-sha512", dssh_register_sntrup761x25519_sha512},
	{"mlkem768x25519-sha256", dssh_register_mlkem768x25519_sha256},
};

static unsigned completed_cases;

void
dssh_test_failure_cleanup(void)
{
}

static bool
target(struct failure_side *side)
{
	return side->session->trans.client == side->pair->target_client
	    && side->generation == (side->pair->target_rekey ? 2U : 1U);
}

static bool
unlocked(dssh_session session)
{
	mtx_t *locks[] = {&session->trans.tx_mtx, &session->trans.tx_queue_mtx};
	for (size_t i = 0; i < sizeof(locks) / sizeof(locks[0]); i++) {
		if (mtx_trylock(locks[i]) != thrd_success)
			return false;
		if (mtx_unlock(locks[i]) != thrd_success)
			return false;
	}
	return true;
}

static void
mark_fault(struct failure_side *side)
{
	side->faulted = true;
	if (side->session->trans.session_id == NULL)
		return;
	/* Queue real connection output after rekey has begun. It must never
	 * follow the failure's final transport packet when ownership is released. */
	dssh_session session = side->session;
	if (mtx_lock(&session->trans.tx_queue_mtx) != thrd_success) {
		side->valid = false;
		return;
	}
	struct dssh_tx_slot *slot = &session->trans.global_reply_slot;
	if (slot->buf == NULL || atomic_load(&slot->ready))
		side->valid = false;
	else {
		slot->buf[9] = SSH_MSG_REQUEST_FAILURE;
		slot->payload_len = 1;
		atomic_store(&slot->ready, true);
		session->trans.tx_slots_pending = true;
		side->queue_armed = true;
	}
	if (mtx_unlock(&session->trans.tx_queue_mtx) != thrd_success)
		side->valid = false;
}

static int
failure_tx(uint8_t *bytes, size_t length, dssh_session session, void *opaque)
{
	struct failure_side *side = opaque;
	if (side->faulted) {
		side->tx_after_fault++;
		side->valid = side->valid && unlocked(session)
		    && session->trans.kex_active == NULL && session->trans.kex_state == NULL
		    && session->trans.kex_result.shared_secret.data == NULL
		    && session->trans.kex_result.exchange_hash.data == NULL
		    && session->trans.kex_result.lease.release == NULL && side->cleanups == 1;
		if (side->pair->tx_result != 0)
			return side->pair->tx_result;
	}
	return session->trans.client ? mock_tx_client(bytes, length, session, &side->pair->io)
	    : mock_tx_server(bytes, length, session, &side->pair->io);
}

static int
failure_rx(uint8_t *bytes, size_t length, dssh_session session, void *opaque)
{
	struct failure_side *side = opaque;
	return session->trans.client ? mock_rx_client(bytes, length, session, &side->pair->io)
	    : mock_rx_server(bytes, length, session, &side->pair->io);
}

static int
failure_rxline(uint8_t *bytes, size_t length, size_t *received, dssh_session session, void *opaque)
{
	struct failure_side *side = opaque;
	return session->trans.client ? mock_rxline_client(bytes, length, received, session, &side->pair->io)
	    : mock_rxline_server(bytes, length, received, session, &side->pair->io);
}

static dssh_log_action
failure_log(const struct dssh_log_record *record, void *opaque)
{
	struct failure_side *side = opaque;
	if (record->source == DSSH_LOG_SOURCE_PEER_DISCONNECT) {
		side->disconnects++;
		side->reason = record->ssh_reason_code;
		side->valid = side->valid && record->language_len == 0;
	}
	return DSSH_LOG_LOCAL_ONLY;
}

static int
failure_init(bool client, void *config, void **state)
{
	struct failure_pair *pair = config;
	struct failure_side *side = &pair->sides[client ? 0U : 1U];
	side->valid = side->valid && (*state == NULL || *state == side) && unlocked(side->session);
	side->generation++;
	*state = side;
	if (target(side) && pair->kind == FAIL_INIT) {
		mark_fault(side);
		return DSSH_ERROR_ALLOC;
	}
	int ret = pair->delegate->init(client, pair->delegate->config, &side->inner);
	side->initialized = side->initialized || ret == 0 || side->inner != NULL;
	return ret;
}

static void
failure_cleanup(void *opaque)
{
	struct failure_side *side = opaque;
	if (side == NULL)
		return;
	side->valid = side->valid && unlocked(side->session);
	side->cleanups++;
	if (side->initialized)
		side->pair->delegate->cleanup(side->inner);
	side->inner = NULL;
	side->initialized = false;
}

struct receive_proxy {
	struct failure_side *side;
	const struct dssh_kex_context *original;
};

static int
proxy_send(const uint8_t *packet, size_t length, void *opaque)
{
	struct receive_proxy *proxy = opaque;
	return proxy->original->send(packet, length, proxy->original->io_ctx);
}

static int
proxy_recv(uint8_t *type, uint8_t **packet, size_t *length, void *opaque)
{
	struct receive_proxy *proxy = opaque;
	struct failure_side *side = proxy->side;
	int ret = proxy->original->recv(type, packet, length, proxy->original->io_ctx);
	if (ret != 0 || !target(side) || side->mutations != 0)
		return ret;
	enum failure_kind kind = side->pair->kind;
	if (kind < FAIL_SHORT || kind > FAIL_CANONICAL)
		return 0;
	mark_fault(side);
	side->mutations++;
	if (kind == FAIL_TYPE) {
		*type = UINT8_C(250);
		return 0;
	}
	if (kind == FAIL_SIGNATURE) {
		if (*length == 0)
			return DSSH_ERROR_PARSE;
		(*packet)[*length - 1] ^= 1;
		return 0;
	}
	size_t pos = 1;
	if (side->session->trans.client) {
		if (!dssh_buffer_has(*length, pos, 4))
			return DSSH_ERROR_PARSE;
		uint32_t host_length = DSSH_GET_U32(&(*packet)[pos]);
		pos += 4;
		if (host_length > *length - pos)
			return DSSH_ERROR_PARSE;
		size_t host_size = (size_t)host_length;
		pos += host_size;
	}
	if (!dssh_buffer_has(*length, pos, 4))
		return DSSH_ERROR_PARSE;
	uint32_t public_length = DSSH_GET_U32(&(*packet)[pos]);
	if (kind == FAIL_SHORT || kind == FAIL_LONG) {
		if (public_length == 0 || public_length == UINT32_MAX)
			return DSSH_ERROR_PARSE;
		if (kind == FAIL_SHORT)
			public_length--;
		else
			public_length++;
		return dssh_serialize_uint32(public_length, *packet, *length, &pos);
	}
	pos += 4;
	if (public_length > *length - pos || public_length < 32)
		return DSSH_ERROR_PARSE;
	size_t public_size = (size_t)public_length;
	if (kind == FAIL_ZERO)
		memset(&(*packet)[pos + public_size - 32], 0, 32);
	else if (kind == FAIL_CIPHERTEXT)
		(*packet)[pos] ^= 1;
	else if (kind == FAIL_CANONICAL)
		memset(&(*packet)[pos], 0xff, 3);
	return 0;
}

static int
failure_handler(const struct dssh_kex_context *ctx, struct dssh_kex_result *out)
{
	struct failure_side *side = ctx->state;
	struct failure_pair *pair = side->pair;
	if (pair->kind == FAIL_PEER && ctx->client != pair->target_client
	    && side->generation == (pair->target_rekey ? 2U : 1U)) {
		int ret = dssh_transport_disconnect(side->session, DSSH_DISCONNECT_BY_APPLICATION, "peer shutdown");
		return ret < 0 ? ret : DSSH_ERROR_TERMINATED;
	}
	if (target(side)) {
		if (pair->kind == FAIL_HANDLER || pair->kind == FAIL_POSITIVE || pair->kind == FAIL_CANCEL) {
			mark_fault(side);
			if (pair->kind == FAIL_CANCEL)
				dssh_session_terminate(side->session);
			return pair->kind == FAIL_POSITIVE ? 1 : DSSH_ERROR_PARSE;
		}
	}
	struct receive_proxy proxy = {side, ctx};
	struct dssh_kex_context inner = *ctx;
	inner.state = side->inner;
	inner.config = pair->delegate->config;
	inner.send = proxy_send;
	inner.recv = proxy_recv;
	inner.io_ctx = &proxy;
	int ret = pair->delegate->handler(&inner, out);
	if (ret == 0 && target(side) && pair->kind == FAIL_RESULT) {
		mark_fault(side);
		out->exchange_hash.len = 0;
	}
	return ret;
}

static int
failure_worker(void *opaque)
{
	struct failure_side *side = opaque;
	side->result = side->pair->running_rekey ? rekey(side->session) : dssh_transport_handshake(side->session);
	if (side->result != 0) {
		/* Preserve already-written bytes for the peer, then deliver EOF.
		 * Full bidirectional shutdown would discard the evidence under test. */
		if (side->session->trans.client)
			mock_io_close_c2s_write(&side->pair->io);
		else
			mock_io_close_s2c_write(&side->pair->io);
	}
	return 0;
}

static bool
exchange(struct failure_pair *pair)
{
	thrd_t client, server;
	if (thrd_create(&client, failure_worker, &pair->sides[0]) != thrd_success)
		return false;
	if (thrd_create(&server, failure_worker, &pair->sides[1]) != thrd_success) {
		mock_io_close_c2s(&pair->io);
		mock_io_close_s2c(&pair->io);
		if (thrd_join(client, NULL) != thrd_success)
			abort();
		return false;
	}
	int cj = thrd_join(client, NULL);
	int sj = thrd_join(server, NULL);
	if (cj != thrd_success || sj != thrd_success)
		abort();
	return true;
}

static bool
run_case(size_t method, bool client, bool rekeying, enum failure_kind kind, int tx_result)
{
	dssh_test_reset_global_config();
	struct dssh_kex_s delegate = {0};
	struct failure_pair pair = {.kind = kind, .target_client = client,
	    .target_rekey = rekeying, .tx_result = tx_result, .delegate = &delegate};
	if (mock_io_init(&pair.io, 0) < 0)
		return false;
	bool valid = methods[method].register_method() == 0 && test_register_key_algos() == 0
	    && test_generate_host_key() == 0 && dssh_register_aes256_ctr() == 0
	    && dssh_register_hmac_sha2_256() == 0 && dssh_register_none_comp() == 0
	    && dssh_transport_set_callbacks(failure_tx, failure_rx, failure_rxline, mock_extra_line_cb) == 0;
	if (valid) {
		dssh_kex definition = gconf.kex_head;
		delegate = *definition;
		definition->init = failure_init;
		definition->handler = failure_handler;
		definition->cleanup = failure_cleanup;
		definition->config = &pair;
	}
	for (size_t i = 0; i < 2 && valid; i++) {
		struct failure_side *side = &pair.sides[i];
		side->pair = &pair;
		side->valid = true;
		side->session = dssh_session_init(i == 0, 0);
		valid = side->session != NULL
		    && dssh_session_set_cbdata(side->session, side, side, side, &pair.io) == 0
		    && dssh_session_set_log_cb(side->session, failure_log, side) == 0;
		if (valid && i == 0)
			valid = dssh_session_set_hostkey_verify_cb(side->session, dssh_test_accept_hostkey, NULL) == 0;
	}
	valid = valid && exchange(&pair);
	if (valid && rekeying) {
		valid = pair.sides[0].result == 0 && pair.sides[1].result == 0;
		pair.running_rekey = true;
		valid = valid && exchange(&pair);
	}
	struct failure_side *failed = &pair.sides[client ? 0U : 1U];
	struct failure_side *peer = &pair.sides[client ? 1U : 0U];
	if (kind == FAIL_NONE)
		valid = valid && failed->result == 0 && peer->result == 0
		    && failed->disconnects == 0 && peer->disconnects == 0;
	else if (valid) {
		valid = failed->result < 0 && peer->result < 0
		    && dssh_session_is_terminated(failed->session) && dssh_session_is_terminated(peer->session);
		if (kind == FAIL_CANCEL)
			valid = valid && failed->tx_after_fault == 0 && peer->disconnects == 0;
		else if (kind == FAIL_PEER)
			valid = valid && failed->tx_after_fault == 0 && failed->disconnects == 1
			    && failed->reason == DSSH_DISCONNECT_BY_APPLICATION && peer->disconnects == 0;
		else {
			valid = valid && failed->tx_after_fault == 1;
			if (tx_result != 0)
				valid = valid && failed->result == (tx_result > 0 ? DSSH_ERROR_INVALID : tx_result)
				    && peer->disconnects == 0;
			else
				valid = valid && peer->disconnects == 1 && peer->reason == DSSH_DISCONNECT_KEY_EXCHANGE_FAILED;
			if (kind == FAIL_INIT)
				valid = valid && failed->result == DSSH_ERROR_ALLOC;
			if (kind == FAIL_HANDLER)
				valid = valid && (tx_result != 0 || failed->result == DSSH_ERROR_PARSE);
			if (kind == FAIL_POSITIVE || kind == FAIL_RESULT)
				valid = valid && failed->result == DSSH_ERROR_INVALID;
			if (kind >= FAIL_SHORT && kind <= FAIL_CANONICAL)
				valid = valid && failed->mutations == 1;
		}
		for (size_t i = 0; i < 2; i++)
			valid = valid && pair.sides[i].session->trans.kex_active == NULL
			    && pair.sides[i].session->trans.kex_state == NULL;
		if (rekeying && kind != FAIL_PEER)
			valid = valid && failed->queue_armed
			    && atomic_load(&failed->session->trans.global_reply_slot.ready);
	}
	for (size_t i = 0; i < 2; i++) {
		if (pair.sides[i].session != NULL)
			dssh_session_cleanup(pair.sides[i].session);
		valid = valid && pair.sides[i].valid && !pair.sides[i].initialized;
	}
	mock_io_free(&pair.io);
	dssh_test_reset_global_config();
	if (!valid) {
		/* Every private caller supplies an enum value in [0, FAIL_PEER]. */
		unsigned int kind_u = (unsigned int)kind;
		fprintf(stderr, "kex failure case: method=%s client=%d rekey=%d kind=%u tx=%d results=%d/%d writes=%u disconnects=%u/%u reasons=%" PRIu32 "/%" PRIu32 "\n",
		    methods[method].name, client, rekeying, kind_u, tx_result, failed->result, peer->result,
		    failed->tx_after_fault, failed->disconnects, peer->disconnects, failed->reason, peer->reason);
	}
	else
		completed_cases++;
	return valid;
}

static int
case_count(const char *group, unsigned expected)
{
	printf("kex_failure/%s cases=%u\n", group, completed_cases);
	ASSERT_EQ_U(completed_cases, expected);
	return TEST_PASS;
}

static int
test_control(void)
{
	completed_cases = 0;
	for (size_t m = 0; m < sizeof(methods) / sizeof(methods[0]); m++)
		ASSERT_TRUE(run_case(m, true, true, FAIL_NONE, 0));
	return case_count("control", 6);
}

static int
test_module_failures(void)
{
	completed_cases = 0;
	for (size_t m = 0; m < sizeof(methods) / sizeof(methods[0]); m++) {
		for (unsigned role = 0; role < 2; role++) {
			for (unsigned phase = 0; phase < 2; phase++) {
				for (enum failure_kind kind = FAIL_INIT; kind <= FAIL_RESULT; kind++)
					ASSERT_TRUE(run_case(m, role != 0, phase != 0, kind, 0));
			}
		}
	}
	return case_count("module", 96);
}

static int
test_inputs(void)
{
	completed_cases = 0;
	const size_t selected[] = {0, 4, 5};
	for (size_t i = 0; i < sizeof(selected) / sizeof(selected[0]); i++) {
		size_t m = selected[i];
		for (unsigned phase = 0; phase < 2; phase++) {
			for (unsigned role = 0; role < 2; role++) {
				for (enum failure_kind kind = FAIL_SHORT; kind <= FAIL_TYPE; kind++)
					ASSERT_TRUE(run_case(m, role != 0, phase != 0, kind, 0));
			}
			ASSERT_TRUE(run_case(m, true, phase != 0, FAIL_SIGNATURE, 0));
			if (m != 0)
				ASSERT_TRUE(run_case(m, true, phase != 0, FAIL_CIPHERTEXT, 0));
			if (m == 5)
				ASSERT_TRUE(run_case(m, false, phase != 0, FAIL_CANONICAL, 0));
		}
	}
	return case_count("inputs", 60);
}

static int
test_send_errors(void)
{
	completed_cases = 0;
	const int results[] = {DSSH_ERROR_INIT, DSSH_ERROR_TOOLONG, 1};
	for (size_t m = 0; m < sizeof(methods) / sizeof(methods[0]); m++) {
		for (unsigned phase = 0; phase < 2; phase++) {
			for (unsigned role = 0; role < 2; role++) {
				for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); i++)
					ASSERT_TRUE(run_case(m, role != 0, phase != 0, FAIL_HANDLER, results[i]));
			}
		}
	}
	return case_count("send_errors", 72);
}

static int
test_stopped(void)
{
	completed_cases = 0;
	for (size_t m = 0; m < sizeof(methods) / sizeof(methods[0]); m++) {
		for (unsigned phase = 0; phase < 2; phase++) {
			for (unsigned role = 0; role < 2; role++) {
				ASSERT_TRUE(run_case(m, role != 0, phase != 0, FAIL_CANCEL, 0));
				ASSERT_TRUE(run_case(m, role != 0, phase != 0, FAIL_PEER, 0));
			}
		}
	}
	return case_count("stopped", 48);
}

struct shutdown_probe {
	struct mock_io_state *io;
	struct mock_io_pipe *pipe;
	const uint8_t *bytes;
	size_t length;
	size_t written;
	bool client;
	bool called;
};

static void
write_then_shutdown(void *opaque)
{
	struct shutdown_probe *probe = opaque;
	probe->called = true;
	probe->written = mock_io_inject(probe->pipe, probe->bytes, probe->length);
	if (probe->client)
		mock_io_close_s2c_write(probe->io);
	else
		mock_io_close_c2s_write(probe->io);
}

static int
test_io_shutdown(void)
{
	static const uint8_t bytes[] = {1, 2, 0, 255};
	static const uint8_t line[] = "SSH-2.0-peer\r\n";
	completed_cases = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned mode = 0; mode < 2; mode++) {
			struct mock_io_state io;
			ASSERT_TRUE(mock_io_init(&io, 0) == 0);
			struct shutdown_probe probe = {.io = &io, .pipe = role != 0 ? &io.s2c : &io.c2s,
			    .bytes = mode != 0 ? line : bytes,
			    .length = mode != 0 ? sizeof(line) - 1 : sizeof(bytes), .client = role != 0};
			uint8_t output[64], remaining[64];
			memset(output, 0x5a, sizeof(output));
			size_t received = 0;
			mock_io_set_read_retry_hook(write_then_shutdown, &probe);
			int ret;
			if (mode != 0)
				ret = role != 0 ? mock_rxline_client(output, sizeof(output), &received, NULL, &io)
				    : mock_rxline_server(output, sizeof(output), &received, NULL, &io);
			else
				ret = role != 0 ? mock_rx_client(output, probe.length, NULL, &io)
				    : mock_rx_server(output, probe.length, NULL, &io);
			mock_io_set_read_retry_hook(NULL, NULL);
			size_t pending = ret < 0 ? mock_io_drain(probe.pipe, remaining, sizeof(remaining)) : 0;
			bool valid = ret == 0 && probe.called && probe.written == probe.length
			    && (mode == 0 || received == probe.length)
			    && memcmp(output, probe.bytes, probe.length) == 0;
			if (valid) {
				uint8_t extra;
				valid = (role != 0 ? mock_rx_client(&extra, 1, NULL, &io)
				    : mock_rx_server(&extra, 1, NULL, &io)) < 0;
			}
			for (size_t i = probe.length; i < sizeof(output); i++)
				valid = valid && output[i] == 0x5a;
			mock_io_free(&io);
			if (!valid)
				fprintf(stderr, "native shutdown role=%u line=%u result=%d written=%zu pending=%zu hook=%d\n",
				    role, mode, ret, probe.written, pending, probe.called);
			ASSERT_TRUE(valid);
			completed_cases++;
		}
	}
	return case_count("io_shutdown", 4);
}

static struct dssh_test_entry tests[] = {
	{"kex_failure/control", test_control},
	{"kex_failure/module", test_module_failures},
	{"kex_failure/inputs", test_inputs},
	{"kex_failure/send_errors", test_send_errors},
	{"kex_failure/stopped", test_stopped},
	{"kex_failure/io_shutdown", test_io_shutdown},
};

DSSH_TEST_MAIN(tests)
