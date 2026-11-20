/* Installed-production C17 application. Reuse public-only socket/auth fixtures;
 * no library-private header, instrumentation or source copy is involved. */
#define main existing_accept_application_main
#include "test_accept_callback_peer.c"
#undef main

struct public_break_capture {
	struct dssh_chan_break_request requests[32];
	atomic_uint count;
	atomic_bool bad;
};
struct public_break_job {
	struct peer_side *side;
};

static bool
public_break_action(dssh_channel channel, const struct dssh_chan_break_request *request, void *opaque)
{
	struct public_break_capture *capture = opaque;
	unsigned count = atomic_load(&capture->count);
	if (count >= 32 || request == NULL) {
		atomic_store(&capture->bad, true);
		return false;
	}
	capture->requests[count] = *request;
	if (dssh_chan_get_command(channel) == NULL || dssh_chan_shutwr(channel) != DSSH_ERROR_INVALID)
		atomic_store(&capture->bad, true);
	atomic_store(&capture->count, count + 1);
	return request->length >= 500 && request->length <= 3000;
}

static bool
public_break_other(dssh_channel channel, const struct dssh_chan_break_request *request, void *opaque)
{
	return public_break_action(channel, request, opaque);
}

static int
public_break_accept(dssh_channel channel, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	(void)channel;
	(void)params;
	(void)result;
	(void)opaque;
	return 0;
}

static int
public_break_worker(void *opaque)
{
	struct public_break_job *job = opaque;
	struct peer_side *side = job->side;
	int result = dssh_transport_handshake(side->sess);
	if (result == 0 && side->client)
		result = dssh_auth_password(side->sess, "user", "pass", NULL, NULL);
	else if (result == 0) {
		struct dssh_auth_server_cbs cbs = {.methods_str = "password", .password_cb = peer_password};
		result = dssh_auth_server(side->sess, &cbs, NULL, NULL);
	}
	if (result == 0)
		result = dssh_session_start(side->sess);
	if (result == 0) {
		if (side->client)
			side->channel = peer_open(side, false);
		else {
			struct dssh_chan_accept_cbs cbs = {.exec = public_break_accept};
			side->channel = dssh_chan_accept(side->sess, &cbs, 5000);
		}
		if (side->channel == NULL)
			result = DSSH_ERROR_INIT;
	}
	if (result != 0) {
		mock_io_close_c2s(side->io);
		mock_io_close_s2c(side->io);
	}
	return result;
}

int
main(void)
{
	if (dssh_register_curve25519_sha256() != 0 || dssh_register_ssh_ed25519() != 0
	    || dssh_ed25519_generate_key() != 0 || dssh_register_aes256_ctr() != 0
	    || dssh_register_hmac_sha2_256() != 0 || dssh_register_none_comp() != 0
	    || dssh_transport_set_callbacks(peer_tx, peer_rx, peer_rxline, NULL) != 0)
		return 2;
	struct peer_pair pair = {.kind = 99};
	struct public_break_capture first[2] = {0}, second[2] = {0};
	struct public_break_job jobs[2];
	if (mock_io_init(&pair.io, 0) != 0)
		return 2;
	bool valid = true;
	for (size_t i = 0; i < 2; i++) {
		atomic_init(&first[i].count, 0);
		atomic_init(&first[i].bad, false);
		atomic_init(&second[i].count, 0);
		atomic_init(&second[i].bad, false);
		struct peer_side *side = &pair.sides[i];
		side->io = &pair.io;
		side->pair = &pair;
		side->client = i == 0;
		side->sess = dssh_session_init(side->client, 0);
		jobs[i] = (struct public_break_job){.side = side};
		valid = side->sess != NULL && valid;
		if (side->sess != NULL)
			valid = dssh_session_set_cbdata(side->sess, side, side, side, NULL) == 0
			    && dssh_session_set_timeout(side->sess, 5000) == 0
			    && dssh_session_set_break_cb(side->sess, public_break_action, &first[i]) == 0 && valid;
	}
	if (valid)
		valid = dssh_session_set_hostkey_verify_cb(pair.sides[0].sess, peer_verify, NULL) == 0;
	thrd_t workers[2];
	bool started[2] = {false, false};
	for (size_t i = 0; i < 2 && valid; i++) {
		started[i] = thrd_create(&workers[i], public_break_worker, &jobs[i]) == thrd_success;
		valid = started[i];
	}
	if (!valid) {
		mock_io_close_c2s(&pair.io);
		mock_io_close_s2c(&pair.io);
	}
	for (size_t i = 0; i < 2; i++) {
		if (started[i]) {
			int result;
			if (thrd_join(workers[i], &result) != thrd_success)
				abort();
			valid = result == 0 && valid;
		}
	}
	unsigned cases = 0;
	static const uint32_t durations[] = {0, 1, 499, 500, 3000, 3001, UINT32_MAX};
	for (size_t side = 0; side < 2 && valid; side++) {
		size_t sender = side == 0 ? 1 : 0;
		size_t position = 0;
		for (unsigned mode = 0; mode < 3 && valid; mode++) {
			struct public_break_capture *capture = mode == 1 ? &second[side] : &first[side];
			if (mode != 0)
				valid = dssh_chan_set_break_cb(pair.sides[side].channel,
				    mode == 2 ? NULL : public_break_other, mode == 2 ? NULL : capture) == 0;
			for (size_t i = 0; i < sizeof(durations) / sizeof(durations[0]) && valid; i++) {
				unsigned before = atomic_load(&capture->count);
				uint8_t marker = 0xa7, received = 0;
				valid = dssh_chan_send_break(pair.sides[sender].channel, durations[i]) == 0
				    && dssh_chan_poll(pair.sides[sender].channel, DSSH_POLL_WRITE, 5000) == DSSH_POLL_WRITE
				    && dssh_chan_write(pair.sides[sender].channel, 0, &marker, 1) == 1
				    && dssh_chan_poll(pair.sides[side].channel, DSSH_POLL_READ, 5000) == DSSH_POLL_READ
				    && dssh_chan_read(pair.sides[side].channel, 0, &received, 1) == 1 && received == marker;
				unsigned count = atomic_load(&capture->count);
				valid = count == before + (mode == 2 ? 0U : 1U) && !atomic_load(&capture->bad) && valid;
				if (mode != 2 && count == before + 1 && before < 32) {
					const struct dssh_chan_break_request *request = &capture->requests[before];
					valid = request->length == durations[i] && request->stdout_pos == position
					    && request->stderr_pos == 0 && !request->want_reply && valid;
				}
				position++;
				cases++;
			}
		}
	}
	mock_io_close_c2s(&pair.io);
	mock_io_close_s2c(&pair.io);
	for (size_t i = 0; i < 2; i++) {
		if (pair.sides[i].sess != NULL)
			dssh_session_cleanup(pair.sides[i].sess);
	}
	mock_io_free(&pair.io);
	int printed = printf("break_public_api cases=%u roles=2 defaults=1 replacement=1 removal=1 production=1 valid=%u\n",
	    cases, valid ? 1U : 0U);
	return valid && cases == 42 && printed >= 0 ? 0 : 1;
}
