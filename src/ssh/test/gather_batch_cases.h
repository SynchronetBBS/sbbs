/* A real encrypted peer checks each packet after a bounded gathered send.
 * Assertions follow worker/session/descriptor retirement on every path. */
#ifndef DSSH_GATHER_BATCH_CASES_H
#define DSSH_GATHER_BATCH_CASES_H

struct gather_batch_probe {
	dssh_session target;
	unsigned calls, ordinary, packets, written;
	size_t largest, bytes, retained;
	unsigned failure;
	bool armed, refill;
	atomic_bool entered, completed, cancelled;
	int refill_result;
};
static struct gather_batch_probe *gather_batch_probe;

static void
gather_batch_ordinary(dssh_session sess)
{
	struct gather_batch_probe *probe = gather_batch_probe;
	if (probe != NULL && probe->armed && probe->target == sess)
		probe->ordinary++;
}

static int
gather_batch_callback(const struct dssh_tx_iov *iov, size_t count,
    dssh_session sess, void *data)
{
	struct gather_batch_probe *probe = gather_batch_probe;
	bool observed = probe != NULL && probe->armed && probe->target == sess;
	if (observed) {
		probe->calls++;
		if (count > probe->largest)
			probe->largest = count;
		if (count == 0 || count > 17 || count > UINT_MAX - probe->packets)
			return DSSH_ERROR_INVALID;
		unsigned packets = (unsigned)count;
		probe->packets += packets;
		if (count >= 2) {
			if (iov[1].len > SIZE_MAX - iov[0].len)
				return DSSH_ERROR_INVALID;
			probe->retained = iov[0].len + iov[1].len;
		}
		if (probe->refill) {
			atomic_store(&probe->entered, true);
			if (!dssh_test_wait_bool(&probe->completed, true))
				return DSSH_ERROR_INIT;
		}
		if (probe->failure == 1)
			return 1;
		if (probe->failure == 2)
			return DSSH_ERROR_REKEY_NEEDED;
	}
	for (size_t i = 0; i < count; i++) {
		if (iov[i].base == NULL || iov[i].len == 0)
			return DSSH_ERROR_INVALID;
		if (observed) {
			if (iov[i].len > SIZE_MAX - probe->bytes)
				return DSSH_ERROR_INVALID;
			probe->bytes += iov[i].len;
		}
		uint8_t *wire = (uint8_t *)iov[i].base;
		int ret = sess->trans.client ? mock_tx_client(wire, iov[i].len, sess, data)
		    : mock_tx_server(wire, iov[i].len, sess, data);
		if (ret != 0)
			return ret;
		if (observed) {
			probe->written++;
			if (probe->failure == 3)
				return DSSH_ERROR_INIT;
		}
	}
	return 0;
}

static int
gather_batch_register(void)
{
	int ret = dssh_transport_set_tx_gather(gather_batch_callback);
	if (ret == 0)
		ret = dssh_test_register_optional_comp();
	if (ret == 0)
		ret = dssh_register_aes128_cbc();
	return ret;
}

static int
gather_batch_refill(void *data)
{
	struct gather_batch_probe *probe = data;
	bool ready = dssh_test_wait_bool(&probe->entered, true);
	probe->refill_result = DSSH_ERROR_INIT;
	if (ready && !atomic_load(&probe->cancelled)) {
		uint8_t reply = SSH_MSG_REQUEST_FAILURE;
		probe->refill_result = send_to_slot(probe->target,
		    &probe->target->trans.global_reply_slot, &reply, 1);
	}
	atomic_store(&probe->completed, true);
	return 0;
}

static bool
gather_batch_receive(struct handshake_ctx *ctx, uint32_t first, unsigned packets)
{
	static const uint8_t caller[] = {SSH_MSG_CHANNEL_DATA, 0, 0, 0, 3, 0, 0, 0, 4, 21, 22, 23, 24};
	for (unsigned i = 0; i < packets; i++) {
		uint8_t type = 0;
		uint8_t *payload = NULL;
		size_t length = 0;
		if (recv_packet(ctx->server, &type, &payload, &length) != 0
		    || ctx->server->trans.last_rx_seq != first + i)
			return false;
		if (i == 2) {
			if (type != caller[0] || length != sizeof(caller)
			    || memcmp(payload, caller, sizeof(caller)) != 0)
				return false;
		}
		else {
			uint8_t expected = i == 0 ? SSH_MSG_REQUEST_SUCCESS : SSH_MSG_REQUEST_FAILURE;
			if (type != expected || length != 1 || payload[0] != expected)
				return false;
		}
	}
	return true;
}

/* kind: normal/reuse, hard budget, callback failure, allocation target, refill.
 * The optional allocation sweep reaches every target and ends only at a full
 * uninjected exchange; the ordinary second round must reuse existing storage. */
static bool
gather_batch_case(unsigned kind, unsigned detail, bool cbc)
{
	struct gather_batch_probe probe = {.refill_result = DSSH_ERROR_INIT};
	atomic_init(&probe.entered, false);
	atomic_init(&probe.completed, false);
	atomic_init(&probe.cancelled, false);
	gather_batch_probe = &probe;
	extra_algorithm_registration = gather_batch_register;
	struct handshake_ctx ctx;
	int setup = handshake_setup_filtered(&ctx, false, NULL,
	    cbc ? "aes128-cbc" : "aes256-ctr", NULL);
	extra_algorithm_registration = NULL;
	bool valid = setup == 0;
	bool created = false;
	thrd_t worker;
	probe.target = ctx.client;
	probe.armed = valid;
	probe.failure = kind == 2 ? detail : 0;
	probe.refill = kind == 4;
	compression_tx_observer = gather_batch_ordinary;
	if (valid && probe.refill) {
		created = thrd_create(&worker, gather_batch_refill, &probe) == thrd_success;
		valid = created;
	}
	unsigned rounds = kind == 0 ? 2U : 1U;
	for (unsigned round = 0; round < rounds && valid; round++) {
		probe.calls = probe.ordinary = probe.packets = probe.written = 0;
		probe.largest = probe.bytes = probe.retained = 0;
		static const uint8_t caller[] = {SSH_MSG_CHANNEL_DATA, 0, 0, 0, 3, 0, 0, 0, 4, 21, 22, 23, 24};
		size_t capacity = 0;
		int error = DSSH_ERROR_INIT;
		uint8_t *buffer = send_begin(ctx.client, caller[0], &capacity, &error);
		valid = buffer != NULL && capacity >= sizeof(caller);
		uint32_t sequence = ctx.client->trans.tx_seq;
		uint_fast32_t count = atomic_load(&ctx.client->trans.tx_since_rekey);
		uint_fast64_t bytes = atomic_load(&ctx.client->trans.tx_bytes_since_rekey);
		uint64_t received_bytes = ctx.server->trans.rx_bytes_since_rekey;
		if (valid) {
			memcpy(buffer, caller, sizeof(caller));
			uint8_t success = SSH_MSG_REQUEST_SUCCESS;
			uint8_t failure = SSH_MSG_REQUEST_FAILURE;
			valid = send_to_slot(ctx.client, &ctx.client->trans.global_reply_slot, &success, 1) == 0
			    && send_to_slot(ctx.client, &ctx.client->trans.open_fail_slot, &failure, 1) == 0;
		}
		if (!valid) {
			send_cancel(ctx.client);
			break;
		}
		if (kind == 1)
			atomic_store(&ctx.client->trans.tx_since_rekey, DSSH_REKEY_HARD_LIMIT - 2);
		if ((kind == 0 && round == 1) || kind == 3) {
			int point = kind == 3 ? (int)detail : 0;
			dssh_test_alloc_fail_after(point);
		}
		uint32_t actual_sequence = 0;
		int result = send_commit(ctx.client, sizeof(caller), &actual_sequence);
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		if (kind == 1) {
			valid = result == DSSH_ERROR_REKEY_NEEDED && !ctx.client->terminate
			    && probe.calls == 1 && probe.largest == 2 && probe.ordinary == 0
			    && ctx.client->trans.tx_seq == sequence + 2
			    && atomic_load(&ctx.client->trans.tx_since_rekey) == DSSH_REKEY_HARD_LIMIT
			    && !atomic_load(&ctx.client->trans.global_reply_slot.ready)
			    && !atomic_load(&ctx.client->trans.open_fail_slot.ready);
			atomic_store(&ctx.client->trans.tx_since_rekey, count + 2);
			if (valid)
				valid = gather_batch_receive(&ctx, sequence, 2);
		}
		else if (kind == 2) {
			int expected = detail == 1 ? DSSH_ERROR_INVALID
			    : detail == 2 ? DSSH_ERROR_REKEY_NEEDED : DSSH_ERROR_INIT;
			valid = result == expected && ctx.client->terminate
			    && probe.calls == 1 && probe.largest == 3 && probe.ordinary == 0
			    && probe.written == (detail == 3 ? 1U : 0U);
			if (valid && detail == 3)
				valid = gather_batch_receive(&ctx, sequence, 1);
		}
		else if (kind == 3 && detail == 0) {
			/* The queued-control drainer reports its final session state. */
			valid = result == DSSH_ERROR_TERMINATED && ctx.client->terminate && allocations == 1
			    && probe.calls == 0 && probe.ordinary == 0
			    && ctx.client->trans.tx_gather_buf == NULL;
		}
		else {
			unsigned packets = probe.refill ? 4U : 3U;
			valid = result == 0 && actual_sequence == sequence + 2
			    && probe.calls == 1 && probe.largest == 3 && probe.packets == 3
			    && probe.ordinary == (probe.refill ? 1U : 0U)
			    && atomic_load(&ctx.client->trans.tx_since_rekey) == count + packets;
			if (kind == 0 && round == 1)
				valid = valid && allocations == 0;
			if (kind == 3)
				valid = valid && detail == 1 && allocations == 1;
			if (valid)
				valid = gather_batch_receive(&ctx, sequence, packets);
		}
		if (kind != 2 && !(kind == 3 && detail == 0)) {
			valid = valid && !ctx.client->terminate
			    && ctx.server->trans.rx_bytes_since_rekey >= received_bytes
			    && atomic_load(&ctx.client->trans.tx_bytes_since_rekey) >= bytes
			    && atomic_load(&ctx.client->trans.tx_bytes_since_rekey) - bytes
			        == ctx.server->trans.rx_bytes_since_rekey - received_bytes
			    && !tx_owner_is_current(ctx.client);
		}
		if (ctx.client->trans.tx_gather_buf != NULL)
			valid = valid && probe.retained <= ctx.client->trans.tx_gather_capacity
			    && compression_bytes_zero(ctx.client->trans.tx_gather_buf, probe.retained);
	}
	atomic_store(&probe.cancelled, true);
	atomic_store(&probe.entered, true);
	if (created && thrd_join(worker, NULL) != thrd_success)
		abort();
	if (probe.refill)
		valid = valid && created && probe.refill_result == 0;
	probe.armed = false;
	compression_tx_observer = NULL;
	if (printf("gather_batch kind=%u detail=%u cbc=%u setup=%d calls=%u largest=%zu ordinary=%u written=%u valid=%u\n",
	    kind, detail, cbc ? 1U : 0U, setup, probe.calls, probe.largest, probe.ordinary,
	    probe.written, valid ? 1U : 0U) < 0)
		valid = false;
	handshake_cleanup(&ctx);
	gather_batch_probe = NULL;
	return valid;
}

static int
test_gather_batch(void)
{
	for (unsigned cbc = 0; cbc < 2; cbc++) {
		bool valid = gather_batch_case(0, 0, cbc != 0);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_gather_batch_budget(void)
{
	bool valid = gather_batch_case(1, 0, false);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_gather_batch_failures(void)
{
	for (unsigned detail = 1; detail < 4; detail++) {
		bool valid = gather_batch_case(2, detail, false);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_gather_batch_allocation(void)
{
	for (unsigned detail = 0; detail < 2; detail++) {
		bool valid = gather_batch_case(3, detail, false);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_gather_batch_refill(void)
{
	bool valid = gather_batch_case(4, 0, false);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_transport_test_layout(void)
{
	size_t actual[6];
	dssh_test_session_layout(actual);
	const size_t expected[6] = {
		sizeof(struct dssh_transport_state_s), sizeof(struct dssh_session_s),
		offsetof(struct dssh_transport_state_s, id_str),
		offsetof(struct dssh_transport_state_s, remote_id_str),
		offsetof(struct dssh_session_s, initialized),
		offsetof(struct dssh_session_s, timeout_ms),
	};
	bool valid = true;
	for (size_t i = 0; i < 6; i++) {
		if (actual[i] != expected[i])
			valid = false;
	}
	if (printf("test_layout trans=%zu session=%zu id=%zu peer=%zu initialized=%zu timeout=%zu valid=%u\n",
	    actual[0], actual[1], actual[2], actual[3], actual[4], actual[5], valid ? 1U : 0U) < 0)
		valid = false;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#endif
