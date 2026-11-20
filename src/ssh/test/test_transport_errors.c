/*
 * test_transport_errors.c -- Transport layer error path tests using
 * test enc/mac modules with failure injection.
 *
 * These tests exercise error branches in send_packet, recv_packet,
 * and newkeys using deterministic module failures. Native-provider failure
 * injection and packet validation have separate fixtures.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <threads.h>

#include "dssh_test.h"
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "ssh-trans.h"
#include "ssh-internal.h"
#include "dssh_test_internal.h"
#include "mock_io.h"
#include "test_enc.h"
#include "test_mac.h"
#include "dssh_test_alloc.h"
#include "dssh_test_ossl.h"
#include "session_pair_fixture.h"


/* ================================================================
 * Dispatch callbacks
 * ================================================================ */

static int
mock_tx_dispatch(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata)
{
	if (sess->trans.client)
		return mock_tx_client(buf, bufsz, sess, cbdata);
	else
		return mock_tx_server(buf, bufsz, sess, cbdata);
}

static int
mock_tx_gather_dispatch(const struct dssh_tx_iov *iov, size_t iovcnt,
    dssh_session sess, void *cbdata)
{
	for (size_t i = 0; i < iovcnt; i++) {
		uint8_t *buf = (uint8_t *)(uintptr_t)iov[i].base;
		int      ret = mock_tx_dispatch(buf, iov[i].len, sess, cbdata);

		if (ret < 0)
			return ret;
	}
	return 0;
}

static int
mock_rx_dispatch(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata)
{
	if (sess->trans.client)
		return mock_rx_client(buf, bufsz, sess, cbdata);
	else
		return mock_rx_server(buf, bufsz, sess, cbdata);
}

static int
mock_rxline_dispatch(uint8_t *buf, size_t bufsz,
    size_t *bytes_received, dssh_session sess, void *cbdata)
{
	if (sess->trans.client)
		return mock_rxline_client(buf, bufsz, bytes_received, sess, cbdata);
	else
		return mock_rxline_server(buf, bufsz, bytes_received, sess, cbdata);
}

/* ================================================================
 * Handshake setup with test enc/mac modules
 * ================================================================ */

static int
register_test_algorithms(void)
{
	int res;
	res = dssh_register_curve25519_sha256();
	if (res < 0) return res;
	res = dssh_register_ssh_ed25519();
	if (res < 0) return res;
	res = register_test_enc();
	if (res < 0) return res;
	res = register_test_mac();
	if (res < 0) return res;
	res = dssh_register_none_comp();
	if (res < 0) return res;
	return 0;
}

static int register_oversized_algorithms(void);

static void
handshake_cleanup(struct test_session_pair *ctx)
{
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	test_pair_destroy(ctx);
	test_enc_reset();
	test_mac_reset();
	dssh_test_reset_global_config();
}

void
dssh_test_failure_cleanup(void)
{
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	test_pair_destroy_active();
	test_enc_reset();
	test_mac_reset();
	dssh_test_reset_global_config();
}

static int
handshake_prepare_observed(struct test_session_pair *ctx, bool gather, bool oversized, int *calls)
{
	test_pair_begin(ctx);
	dssh_test_reset_global_config();
	test_enc_reset();
	test_mac_reset();
	int ret = gather ? dssh_transport_set_tx_gather(mock_tx_gather_dispatch) : 0;
	if (ret == 0)
		ret = oversized ? register_oversized_algorithms() : register_test_algorithms();
	if (ret == 0)
		ret = dssh_ed25519_generate_key();
	if (ret == 0)
		ret = dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
		    mock_rxline_dispatch, mock_extra_line_cb);
	if (ret == 0)
		ret = test_pair_open(ctx);
	if (calls != NULL)
		*calls = dssh_test_alloc_count();
	if (ret < 0)
		handshake_cleanup(ctx);
	return ret;
}

static int
handshake_prepare(struct test_session_pair *ctx, bool gather, bool oversized)
{
	return handshake_prepare_observed(ctx, gather, oversized, NULL);
}

static int
handshake_setup_config(struct test_session_pair *ctx, bool gather)
{
	int ret = handshake_prepare(ctx, gather, false);
	if (ret == 0)
		ret = test_pair_run(ctx, dssh_transport_handshake);
	if (ret < 0)
		handshake_cleanup(ctx);
	return ret;
}

static int
handshake_setup(struct test_session_pair *ctx)
{
	return handshake_setup_config(ctx, false);
}

static bool
buffer_is_zero(const uint8_t *buf, size_t len)
{
	volatile const uint8_t *vbuf = buf;

	for (size_t i = 0; i < len; i++) {
		if (vbuf[i] != 0)
			return false;
	}
	return true;
}

/* ================================================================
 * Channel TX-slot fixture
 *
 * Initialize the connection-layer locks and one registered channel
 * without starting a demux thread.  This isolates slot draining and
 * gathering while giving session_set_terminate() the real channel
 * lock topology it must wake.
 * ================================================================ */

struct channel_slot_fixture {
	struct dssh_channel_s  channel;
	struct dssh_channel_id protocol_id;
	struct dssh_channel_s *channels[1];
};

static int
channel_slot_fixture_setup(dssh_session sess, struct channel_slot_fixture *fixture)
{
	memset(fixture, 0, sizeof(*fixture));

	if (mtx_init(&sess->channel_mtx, mtx_plain) != thrd_success)
		return -1;
	if (mtx_init(&sess->accept_mtx, mtx_plain) != thrd_success) {
		mtx_destroy(&sess->channel_mtx);
		return -1;
	}
	if (cnd_init(&sess->accept_cnd) != thrd_success) {
		mtx_destroy(&sess->accept_mtx);
		mtx_destroy(&sess->channel_mtx);
		return -1;
	}
	if (mtx_init(&fixture->channel.buf_mtx, mtx_plain) != thrd_success) {
		cnd_destroy(&sess->accept_cnd);
		mtx_destroy(&sess->accept_mtx);
		mtx_destroy(&sess->channel_mtx);
		return -1;
	}
	if (mtx_init(&fixture->channel.cb_mtx, mtx_plain) != thrd_success) {
		mtx_destroy(&fixture->channel.buf_mtx);
		cnd_destroy(&sess->accept_cnd);
		mtx_destroy(&sess->accept_mtx);
		mtx_destroy(&sess->channel_mtx);
		return -1;
	}
	if (cnd_init(&fixture->channel.poll_cnd) != thrd_success) {
		mtx_destroy(&fixture->channel.cb_mtx);
		mtx_destroy(&fixture->channel.buf_mtx);
		cnd_destroy(&sess->accept_cnd);
		mtx_destroy(&sess->accept_mtx);
		mtx_destroy(&sess->channel_mtx);
		return -1;
	}
	if (alloc_channel_slots(sess, &fixture->channel) < 0) {
		free_tx_slot(&fixture->channel.wa_slot);
		free_tx_slot(&fixture->channel.chan_reply_slot);
		cnd_destroy(&fixture->channel.poll_cnd);
		mtx_destroy(&fixture->channel.cb_mtx);
		mtx_destroy(&fixture->channel.buf_mtx);
		cnd_destroy(&sess->accept_cnd);
		mtx_destroy(&sess->accept_mtx);
		mtx_destroy(&sess->channel_mtx);
		return -1;
	}

	fixture->protocol_id = (struct dssh_channel_id){.refs = 1,
	    .phase = DSSH_CHANNEL_ID_CONFIRMED, .local_assigned = true};
	fixture->channel.sess = sess;
	fixture->channel.protocol_id = &fixture->protocol_id;
	atomic_init(&fixture->channel.tx_refs, 0);
	atomic_init(&fixture->channel.terminate_woken, false);
	fixture->channels[0]  = &fixture->channel;
	sess->channels         = fixture->channels;
	sess->channel_ids      = &fixture->protocol_id;
	sess->channel_count    = 1;
	sess->channel_capacity = 1;
	atomic_store(&sess->conn_initialized, true);
	return 0;
}

static void
channel_slot_fixture_cleanup(dssh_session sess, struct channel_slot_fixture *fixture)
{
	atomic_store(&sess->conn_initialized, false);
	sess->channels         = NULL;
	sess->channel_ids      = NULL;
	sess->channel_count    = 0;
	sess->channel_capacity = 0;

	free_tx_slot(&fixture->channel.wa_slot);
	free_tx_slot(&fixture->channel.chan_reply_slot);
	cnd_destroy(&fixture->channel.poll_cnd);
	mtx_destroy(&fixture->channel.cb_mtx);
	mtx_destroy(&fixture->channel.buf_mtx);
	cnd_destroy(&sess->accept_cnd);
	mtx_destroy(&sess->accept_mtx);
	mtx_destroy(&sess->channel_mtx);
}

static void
arm_channel_wa_slot(struct dssh_channel_s *channel)
{
	channel->window_adjust_pending = 1;
	atomic_store(&channel->wa_slot.ready, true);
}

/* ================================================================
 * Channel TX-slot fatal errors must terminate after channel_mtx is
 * released.  Before the fix, each test self-deadlocks in
 * session_set_terminate(); CTest supplies a bounded timeout.
 * ================================================================ */

static int
test_channel_slot_drain_fatal(void)
{
	struct test_session_pair ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct channel_slot_fixture fixture;
	if (channel_slot_fixture_setup(ctx.client, &fixture) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	bool initially_terminated = dssh_session_is_terminated(ctx.client);
	arm_channel_wa_slot(&fixture.channel);
	test_enc_fail_encrypt_at(0);

	if (tx_acquire(ctx.client, SSH_MSG_IGNORE) < 0) {
		channel_slot_fixture_cleanup(ctx.client, &fixture);
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	int drain_result = drain_tx_slots(ctx.client);
	tx_unlock_with_slots(ctx.client);

	bool terminated = dssh_session_is_terminated(ctx.client);
	bool ready       = atomic_load(&fixture.channel.wa_slot.ready);
	int  lock_result = mtx_trylock(&ctx.client->channel_mtx);
	int  channel_unlock_result = thrd_success;

	if (lock_result == thrd_success)
		channel_unlock_result = mtx_unlock(&ctx.client->channel_mtx);

	test_enc_reset();
	channel_slot_fixture_cleanup(ctx.client, &fixture);
	handshake_cleanup(&ctx);

	ASSERT_FALSE(initially_terminated);
	ASSERT_EQ(drain_result, DSSH_ERROR_TERMINATED);
	ASSERT_TRUE(terminated);
	ASSERT_FALSE(ready);
	ASSERT_EQ(lock_result, thrd_success);
	ASSERT_EQ(channel_unlock_result, thrd_success);
	return TEST_PASS;
}

static int
test_channel_slot_gather_fatal(void)
{
	struct test_session_pair ctx;
	if (handshake_setup_config(&ctx, true) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct channel_slot_fixture fixture;
	if (channel_slot_fixture_setup(ctx.client, &fixture) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	bool initially_terminated = dssh_session_is_terminated(ctx.client);
	arm_channel_wa_slot(&fixture.channel);
	test_enc_fail_encrypt_at(0);
	ctx.client->trans.tx_packet[9] = SSH_MSG_IGNORE;

	if (tx_acquire(ctx.client, SSH_MSG_IGNORE) < 0) {
		channel_slot_fixture_cleanup(ctx.client, &fixture);
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	int gather_result = tx_gather_with_packet(ctx.client,
	    ctx.client->trans.tx_packet, 1);
	tx_unlock_with_slots(ctx.client);

	bool terminated = dssh_session_is_terminated(ctx.client);
	bool ready       = atomic_load(&fixture.channel.wa_slot.ready);
	int  lock_result = mtx_trylock(&ctx.client->channel_mtx);
	int  channel_unlock_result = thrd_success;

	if (lock_result == thrd_success)
		channel_unlock_result = mtx_unlock(&ctx.client->channel_mtx);

	test_enc_reset();
	channel_slot_fixture_cleanup(ctx.client, &fixture);
	handshake_cleanup(&ctx);

	ASSERT_FALSE(initially_terminated);
	ASSERT_EQ(gather_result, DSSH_ERROR_TERMINATED);
	ASSERT_TRUE(terminated);
	ASSERT_FALSE(ready);
	ASSERT_EQ(lock_result, thrd_success);
	ASSERT_EQ(channel_unlock_result, thrd_success);
	return TEST_PASS;
}

/* ================================================================
 * send_packet: encrypt error
 * ================================================================ */

static int
test_send_encrypt_error(void)
{
	struct test_session_pair ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Arm: fail the next encrypt call */
	test_enc_fail_encrypt_at(0);

	uint8_t msg[] = { SSH_MSG_IGNORE, 0 };
	int res = send_packet(ctx.client, msg,
	    sizeof(msg), NULL);
	ASSERT_TRUE(res < 0);

	test_enc_reset();
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * send_packet: MAC generate error
 * ================================================================ */

static int
test_send_mac_error(void)
{
	struct test_session_pair ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	test_mac_fail_generate_at(0);

	uint8_t msg[] = { SSH_MSG_IGNORE, 0 };
	int res = send_packet(ctx.client, msg,
	    sizeof(msg), NULL);
	ASSERT_TRUE(res < 0);

	test_mac_reset();
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * Sensitive commits scrub the shared TX payload before releasing
 * tx_mtx or invoking the application termination callback.
 * ================================================================ */

static int
test_sensitive_commit_success_config(bool gather)
{
	struct test_session_pair ctx;
	if (handshake_setup_config(&ctx, gather) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	size_t   max;
	int      err;
	uint8_t *payload = send_begin(ctx.client, SSH_MSG_IGNORE, &max, &err);

	if (payload == NULL) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	if (max < 32) {
		send_cancel(ctx.client);
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	memset(payload, 0xa5, 32);
	payload[0] = SSH_MSG_IGNORE;

	int  res        = send_commit_sensitive(ctx.client, 32, NULL);
	bool cleansed   = buffer_is_zero(payload, 32);
	int  lock_res   = mtx_trylock(&ctx.client->trans.tx_mtx);
	int  unlock_res = thrd_success;

	if (lock_res == thrd_success)
		unlock_res = mtx_unlock(&ctx.client->trans.tx_mtx);
	handshake_cleanup(&ctx);

	ASSERT_OK(res);
	ASSERT_TRUE(cleansed);
	ASSERT_EQ(lock_res, thrd_success);
	ASSERT_EQ(unlock_res, thrd_success);
	return TEST_PASS;
}

static int
test_sensitive_commit_success(void)
{
	return test_sensitive_commit_success_config(false);
}

static int
test_sensitive_commit_gather_success(void)
{
	return test_sensitive_commit_success_config(true);
}

struct sensitive_terminate_observation {
	size_t payload_len;
	bool   called;
	bool   payload_zero;
};

static void
observe_sensitive_terminate(dssh_session sess, void *cbdata)
{
	struct sensitive_terminate_observation *obs = cbdata;

	obs->called       = true;
	obs->payload_zero = buffer_is_zero(&sess->trans.tx_packet[9], obs->payload_len);
}

static int
test_sensitive_commit_encrypt_error(void)
{
	struct test_session_pair ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	struct sensitive_terminate_observation obs = { .payload_len = 32 };
	if (dssh_session_set_terminate_cb(ctx.client, observe_sensitive_terminate, &obs) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	size_t   max;
	int      err;
	uint8_t *payload = send_begin(ctx.client, SSH_MSG_IGNORE, &max, &err);

	if (payload == NULL) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	if (max < obs.payload_len) {
		send_cancel(ctx.client);
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	memset(payload, 0xa5, obs.payload_len);
	payload[0] = SSH_MSG_IGNORE;
	test_enc_fail_encrypt_at(0);

	int  res        = send_commit_sensitive(ctx.client, obs.payload_len, NULL);
	bool cleansed   = buffer_is_zero(payload, obs.payload_len);
	bool terminated = dssh_session_is_terminated(ctx.client);
	int  lock_res   = mtx_trylock(&ctx.client->trans.tx_mtx);
	int  unlock_res = thrd_success;

	if (lock_res == thrd_success)
		unlock_res = mtx_unlock(&ctx.client->trans.tx_mtx);
	handshake_cleanup(&ctx);

	ASSERT_TRUE(res < 0);
	ASSERT_TRUE(cleansed);
	ASSERT_TRUE(terminated);
	ASSERT_TRUE(obs.called);
	ASSERT_TRUE(obs.payload_zero);
	ASSERT_EQ(lock_res, thrd_success);
	ASSERT_EQ(unlock_res, thrd_success);
	return TEST_PASS;
}

static int
test_sensitive_commit_too_long(void)
{
	struct test_session_pair ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	size_t   max;
	int      err;
	uint8_t *payload = send_begin(ctx.client, SSH_MSG_IGNORE, &max, &err);

	if (payload == NULL) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	if (max == SIZE_MAX) {
		send_cancel(ctx.client);
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	memset(payload, 0xa5, max);
	payload[0]   = SSH_MSG_IGNORE;
	payload[max] = 0x7e;

	int  res              = send_commit_sensitive(ctx.client, max + 1, NULL);
	bool cleansed         = buffer_is_zero(payload, max);
	bool canary_unchanged = payload[max] == 0x7e;
	bool terminated       = dssh_session_is_terminated(ctx.client);
	int  lock_res         = mtx_trylock(&ctx.client->trans.tx_mtx);
	int  unlock_res       = thrd_success;

	if (lock_res == thrd_success)
		unlock_res = mtx_unlock(&ctx.client->trans.tx_mtx);
	handshake_cleanup(&ctx);

	ASSERT_ERR(res, DSSH_ERROR_TOOLONG);
	ASSERT_TRUE(cleansed);
	ASSERT_TRUE(canary_unchanged);
	ASSERT_FALSE(terminated);
	ASSERT_EQ(lock_res, thrd_success);
	ASSERT_EQ(unlock_res, thrd_success);
	return TEST_PASS;
}

/* ================================================================
 * recv_packet: decrypt error on first block
 * ================================================================ */

static int
test_recv_decrypt_error_first_block(void)
{
	struct test_session_pair ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Client sends a valid packet */
	uint8_t msg[] = { SSH_MSG_IGNORE, 0 };
	ASSERT_OK(send_packet(ctx.client, msg,
	    sizeof(msg), NULL));

	/* Arm: fail the first decrypt on the server side */
	test_enc_fail_decrypt_at(0);

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	int res = recv_packet(ctx.server, &msg_type,
	    &payload, &payload_len);
	ASSERT_TRUE(res < 0);

	test_enc_reset();
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * recv_packet: decrypt error on remaining blocks
 * ================================================================ */

static int
test_recv_decrypt_error_remaining(void)
{
	struct test_session_pair ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Send a packet large enough to require a second decrypt call
	 * (payload > blocksize - 5 = 11 bytes) */
	uint8_t msg[64];
	msg[0] = SSH_MSG_IGNORE;
	memset(&msg[1], 0x42, sizeof(msg) - 1);
	ASSERT_OK(send_packet(ctx.client, msg,
	    sizeof(msg), NULL));

	/* Fail the second decrypt call (first block succeeds) */
	test_enc_fail_decrypt_at(1);

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	int res = recv_packet(ctx.server, &msg_type,
	    &payload, &payload_len);
	ASSERT_TRUE(res < 0);

	test_enc_reset();
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * recv_packet: MAC generate error during verification
 * ================================================================ */

static int
test_recv_mac_generate_error(void)
{
	struct test_session_pair ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Client sends a valid packet (MAC is generated successfully) */
	uint8_t msg[] = { SSH_MSG_IGNORE, 0 };
	ASSERT_OK(send_packet(ctx.client, msg,
	    sizeof(msg), NULL));

	/* Arm: fail the next generate call on the server side
	 * (this is the verification generate, not the sender's) */
	test_mac_fail_generate_at(0);

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	int res = recv_packet(ctx.server, &msg_type,
	    &payload, &payload_len);
	ASSERT_TRUE(res < 0);

	test_mac_reset();
	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * recv_packet: MAC mismatch (corruption)
 * ================================================================ */

static int
test_recv_mac_mismatch(void)
{
	struct test_session_pair ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Send with corrupt MAC output -- the sender's MAC will have a
	 * flipped bit, so the receiver's computed MAC won't match. */
	test_mac_set_corrupt(true);

	uint8_t msg[] = { SSH_MSG_IGNORE, 0 };
	ASSERT_OK(send_packet(ctx.client, msg,
	    sizeof(msg), NULL));

	test_mac_set_corrupt(false);

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	int res = recv_packet(ctx.server, &msg_type,
	    &payload, &payload_len);
	ASSERT_EQ(res, DSSH_ERROR_INVALID);

	/* The server should be terminated (disconnect sent) */
	ASSERT_TRUE(dssh_session_is_terminated(ctx.server));

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* Packet framing must validate peer padding even with a valid MAC. Exact
 * malformed-frame coverage lives in the transport/framing and module fixtures;
 * it is not an impossible library-owned state or a reason to omit validation. */

/* ================================================================
 * newkeys: enc init failure
 *
 * We can't easily test this post-handshake because newkeys runs
 * during the handshake itself.  Instead, we arm the init failure
 * BEFORE the handshake -- the handshake will call newkeys which
 * calls enc->init, which will fail.
 * ================================================================ */

static int
test_newkeys_enc_init_failure(void)
{
	struct test_session_pair ctx;
	if (handshake_prepare(&ctx, false, false) < 0)
		return TEST_FAIL;
	test_enc_fail_init_at(0);
	int ret = test_pair_run(&ctx, dssh_transport_handshake);
	bool seen = test_enc_init_failure_seen();
	bool rejected = ctx.client_result == DSSH_ERROR_INIT || ctx.server_result == DSSH_ERROR_INIT;
	handshake_cleanup(&ctx);
	ASSERT_TRUE(ret < 0 && seen && rejected);
	return TEST_PASS;
}

/* ================================================================
 * newkeys: mac init failure
 * ================================================================ */

static int
test_newkeys_mac_init_failure(void)
{
	struct test_session_pair ctx;
	if (handshake_prepare(&ctx, false, false) < 0)
		return TEST_FAIL;
	test_mac_fail_init_at(0);
	int ret = test_pair_run(&ctx, dssh_transport_handshake);
	bool seen = test_mac_init_failure_seen();
	bool rejected = ctx.client_result == DSSH_ERROR_INIT || ctx.server_result == DSSH_ERROR_INIT;
	handshake_cleanup(&ctx);
	ASSERT_TRUE(ret < 0 && seen && rejected);
	return TEST_PASS;
}

/* ================================================================
 * Post-handshake roundtrip: verify test modules work correctly
 * ================================================================ */

static int
test_roundtrip_with_test_modules(void)
{
	struct test_session_pair ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}

	/* Verify the test enc/mac are in use */
	ASSERT_NOT_NULL(ctx.client->trans.enc_c2s_ctx);
	ASSERT_NOT_NULL(ctx.client->trans.mac_c2s_ctx);

	/* Send and receive a packet -- use a non-transport message
	 * because SSH_MSG_IGNORE is silently consumed by recv_packet */
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	ASSERT_OK(send_packet(ctx.client, msg,
	    sizeof(msg), NULL));

	uint8_t msg_type;
	uint8_t *payload;
	size_t payload_len;
	ASSERT_OK(recv_packet(ctx.server, &msg_type,
	    &payload, &payload_len));
	ASSERT_EQ(msg_type, SSH_MSG_SERVICE_REQUEST);

	handshake_cleanup(&ctx);
	return TEST_PASS;
}

/* ================================================================
 * recv_packet: large MAC (128 bytes)
 *
 * Register the oversized test MAC (digest_size=128) to verify that
 * the library correctly handles MACs larger than SHA-256 (32 bytes).
 * MAC verification buffers are dynamically allocated to match the
 * negotiated digest_size, so this should succeed.
 * ================================================================ */

static int
register_oversized_algorithms(void)
{
	int res;
	res = dssh_register_curve25519_sha256();
	if (res < 0) return res;
	res = dssh_register_ssh_ed25519();
	if (res < 0) return res;
	res = register_test_enc();
	if (res < 0) return res;
	res = register_test_mac_oversized();
	if (res < 0) return res;
	res = dssh_register_none_comp();
	if (res < 0) return res;
	return 0;
}

static int
test_recv_mac_too_large(void)
{
	struct test_session_pair ctx;
	if (handshake_prepare(&ctx, false, true) < 0)
		return TEST_FAIL;
	int ret = test_pair_run(&ctx, dssh_transport_handshake);
	if (ret < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	uint8_t msg[] = { SSH_MSG_SERVICE_REQUEST, 0x42 };
	ret = send_packet(ctx.client, msg, sizeof(msg), NULL);
	uint8_t type = 0;
	uint8_t *payload = NULL;
	size_t length = 0;
	if (ret == 0)
		ret = recv_packet(ctx.server, &type, &payload, &length);
	bool valid = ret == 0 && type == SSH_MSG_SERVICE_REQUEST && length == sizeof(msg)
	    && payload != NULL && memcmp(payload, msg, sizeof(msg)) == 0;
	handshake_cleanup(&ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

/* ================================================================
 * send_packet: payload + MAC exceeds buffer
 *
 * With the oversized MAC (128 bytes), a payload near the buffer
 * limit should be rejected by the total + mac_len check.
 * ================================================================ */

static int
test_send_mac_overflow_rejected(void)
{
	struct test_session_pair ctx;
	if (handshake_prepare(&ctx, false, true) < 0)
		return TEST_FAIL;
	int ret = test_pair_run(&ctx, dssh_transport_handshake);
	if (ret < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	size_t length = ctx.client->trans.packet_buf_sz - 120;
	uint8_t *payload = malloc(length);
	if (payload == NULL) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	payload[0] = SSH_MSG_SERVICE_REQUEST;
	memset(&payload[1], 0x42, length - 1);
	ret = send_packet(ctx.client, payload, length, NULL);
	free(payload);
	bool alive = !dssh_session_is_terminated(ctx.client);
	handshake_cleanup(&ctx);
	ASSERT_EQ(ret, DSSH_ERROR_TOOLONG);
	ASSERT_TRUE(alive);
	return TEST_PASS;
}

static bool
fixture_retired(const struct test_session_pair *ctx)
{
	return !ctx->registered && !ctx->io_ready && ctx->client == NULL && ctx->server == NULL
	    && !ctx->owned[0] && !ctx->owned[1] && ctx->created == ctx->joined
	    && atomic_load(&ctx->io.c2s.rfd) == -1 && atomic_load(&ctx->io.c2s.wfd) == -1
	    && atomic_load(&ctx->io.s2c.rfd) == -1 && atomic_load(&ctx->io.s2c.wfd) == -1
	    && test_pair_active == NULL;
}

static int
test_fixture_setup_allocations(void)
{
	bool fault_seen = false;
	for (int point = 0; point < 200; point++) {
		dssh_test_alloc_reset();
		dssh_test_alloc_fail_after(point);
		struct test_session_pair ctx;
		int calls = 0;
		int ret = handshake_prepare_observed(&ctx, false, false, &calls);
		dssh_test_alloc_reset();
		bool injected = calls > point;
		bool valid = ret == 0 || injected;
		if (ret == 0) {
			ret = test_pair_run(&ctx, dssh_transport_handshake);
			valid &= ret == 0 && ctx.created == 2 && ctx.joined == 2;
		}
		handshake_cleanup(&ctx);
		handshake_cleanup(&ctx);
		valid &= fixture_retired(&ctx);
		fault_seen |= injected;
		if (printf("fixture_setup_case point=%d calls=%d injected=%d operation=%d created=%u joined=%u retired=%d\n",
		        point, calls, injected, ret, ctx.created, ctx.joined, fixture_retired(&ctx)) < 0)
			return TEST_FAIL;
		ASSERT_TRUE(valid);
		if (ret == 0 && !injected) {
			ASSERT_TRUE(fault_seen);
			if (printf("fixture_setup_sweep cases=%d complete=1\n", point + 1) < 0)
				return TEST_FAIL;
			return TEST_PASS;
		}
	}
	return TEST_FAIL;
}

static int
test_fixture_ownership(void)
{
	const char *mode = getenv("DSSH_PEER_FAILURE");
	bool plain = mode == NULL || strcmp(mode, "plain") == 0;
	struct test_session_pair ctx;
	int ret = handshake_setup(&ctx);
	bool valid = plain ? ret == 0 : ret < 0;
	handshake_cleanup(&ctx);
	handshake_cleanup(&ctx);
	valid &= fixture_retired(&ctx);
	if (printf("fixture_owner=1 created=%u joined=%u valid=%d\n", ctx.created, ctx.joined, valid) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
fixture_assertion_failure(void)
{
	ASSERT_TRUE(false);
	return TEST_PASS;
}

/* Fresh-process assertion controls call the actual assertion hook while its
 * owner and all worker arguments are still live on this stack. */
static int
fixture_assertion_control(unsigned phase)
{
	struct test_session_pair ctx;
	if (handshake_prepare(&ctx, false, false) < 0)
		return 2;
	ctx.operation = dssh_transport_handshake;
	for (unsigned i = 0; i < phase; i++) {
		if (test_pair_start(&ctx, i) < 0) {
			handshake_cleanup(&ctx);
			return 2;
		}
	}
	int result = fixture_assertion_failure();
	bool valid = result == TEST_FAIL && fixture_retired(&ctx)
	    && ctx.created == phase && ctx.joined == phase;
	handshake_cleanup(&ctx);
	if (printf("fixture_assertion=1 phase=%u created=%u joined=%u valid=%d\n",
	        phase, ctx.created, ctx.joined, valid) < 0)
		return 2;
	return valid ? 1 : 2;
}

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	{ "fixture/setup_allocations", test_fixture_setup_allocations },
	{ "fixture/ownership", test_fixture_ownership },
	/* Verify test modules work */
	{ "test_modules/roundtrip",            test_roundtrip_with_test_modules },

	/* send_packet error paths */
	{ "send/encrypt_error",                test_send_encrypt_error },
	{ "send/mac_error",                    test_send_mac_error },
	{ "send/sensitive_success",            test_sensitive_commit_success },
	{ "send/sensitive_gather_success",     test_sensitive_commit_gather_success },
	{ "send/sensitive_encrypt_error",      test_sensitive_commit_encrypt_error },
	{ "send/sensitive_too_long",           test_sensitive_commit_too_long },
	{ "send/channel_slot_drain_fatal",     test_channel_slot_drain_fatal },
	{ "send/channel_slot_gather_fatal",    test_channel_slot_gather_fatal },

	/* recv_packet error paths */
	{ "recv/decrypt_error_first_block",    test_recv_decrypt_error_first_block },
	{ "recv/decrypt_error_remaining",      test_recv_decrypt_error_remaining },
	{ "recv/mac_generate_error",           test_recv_mac_generate_error },
	{ "recv/mac_mismatch",                 test_recv_mac_mismatch },

	/* recv_packet error paths (cont.) */
	{ "recv/mac_too_large",                test_recv_mac_too_large },

	/* send_packet MAC overflow */
	{ "send/mac_overflow_rejected",        test_send_mac_overflow_rejected },

	/* newkeys error paths */
	{ "newkeys/enc_init_failure",          test_newkeys_enc_init_failure },
	{ "newkeys/mac_init_failure",          test_newkeys_mac_init_failure },
};

#define main transport_errors_main
DSSH_TEST_MAIN(tests)
#undef main

int
main(int argc, char **argv)
{
	if (argc == 3 && strcmp(argv[1], "--fixture-assert") == 0) {
		if (strlen(argv[2]) != 1 || argv[2][0] < '0' || argv[2][0] > '2')
			return 2;
		unsigned phase = (unsigned)(argv[2][0] - '0');
		return fixture_assertion_control(phase);
	}
	return transport_errors_main(argc, argv);
}
