/* Public-only production consumer. POSIX socket I/O belongs to this test
 * application; no library-private header, reset or instrumented library is used. */
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "dssh_test_wait.h"
#include "mock_io.h"

struct peer_side {
	struct mock_io_state *io;
	dssh_session sess;
	dssh_channel channel;
	bool client;
};

static atomic_int event_count;
static atomic_bool invalid_event;

static int
peer_tx(uint8_t *buf, size_t len, dssh_session sess, void *opaque)
{
	struct peer_side *side = opaque;
	return side->client ? mock_tx_client(buf, len, sess, side->io)
	    : mock_tx_server(buf, len, sess, side->io);
}

static int
peer_rx(uint8_t *buf, size_t len, dssh_session sess, void *opaque)
{
	struct peer_side *side = opaque;
	return side->client ? mock_rx_client(buf, len, sess, side->io)
	    : mock_rx_server(buf, len, sess, side->io);
}

static int
peer_rxline(uint8_t *buf, size_t len, size_t *received, dssh_session sess, void *opaque)
{
	struct peer_side *side = opaque;
	return side->client ? mock_rxline_client(buf, len, received, sess, side->io)
	    : mock_rxline_server(buf, len, received, sess, side->io);
}

static dssh_hostkey_decision
peer_verify(const char *name, unsigned bits, const uint8_t *hash,
    const uint8_t *key, size_t key_len, void *opaque)
{
	(void)name;
	(void)bits;
	(void)hash;
	(void)key;
	(void)key_len;
	(void)opaque;
	return DSSH_HOSTKEY_ACCEPT;
}

static int
peer_password(const uint8_t *user, size_t user_len, const uint8_t *password,
    size_t password_len, struct dssh_bytes_result *result, void *opaque)
{
	(void)result;
	(void)opaque;
	return user_len == 4 && password_len == 4 && memcmp(user, "user", 4) == 0
	    && memcmp(password, "pass", 4) == 0 ? DSSH_AUTH_SUCCESS : DSSH_AUTH_FAILURE;
}

static int
peer_exec(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	(void)ch;
	(void)result;
	(void)opaque;
	return params->type == DSSH_CHAN_EXEC && strcmp(params->command, "event") == 0
	    ? 0 : DSSH_ERROR_REJECTED;
}

static void
peer_event(dssh_channel ch, const struct dssh_chan_event *event, void *opaque)
{
	(void)opaque;
	if (event->type != DSSH_EVENT_SIGNAL || event->stdout_pos != 3
	    || event->stderr_pos != 2 || (event->signal.name.len != 4 || event->signal.name.data == NULL
	        || memcmp(event->signal.name.data, "TERM", 4) != 0))
		atomic_store(&invalid_event, true);
	int count = atomic_load(&event_count);
	if (count == 129 && (dssh_chan_get_type(ch) != DSSH_CHAN_EXEC
	    || dssh_chan_get_command(ch) == NULL || dssh_chan_set_event_cb(ch, NULL, NULL) != 0))
		atomic_store(&invalid_event, true);
	atomic_store(&event_count, count + 1);
}

static int
peer_worker(void *opaque)
{
	struct peer_side *side = opaque;
	int ret = dssh_transport_handshake(side->sess);
	if (ret == 0 && side->client)
		ret = dssh_auth_password(side->sess, "user", "pass", NULL, NULL);
	else if (ret == 0) {
		uint8_t user[16];
		size_t len = sizeof(user);
		struct dssh_auth_server_cbs cbs = {
			.methods_str = "password", .password_cb = peer_password,
		};
		ret = dssh_auth_server(side->sess, &cbs, user, &len);
	}
	if (ret == 0)
		ret = dssh_session_start(side->sess);
	if (ret == 0 && side->client) {
		struct dssh_chan_params params;
		ret = dssh_chan_params_init(&params, DSSH_CHAN_EXEC);
		if (ret == 0) {
			ret = dssh_chan_params_set_command(&params, "event");
			if (ret == 0)
				side->channel = dssh_chan_open(side->sess, &params);
			dssh_chan_params_free(&params);
		}
	}
	else if (ret == 0) {
		struct dssh_chan_accept_cbs cbs = {.exec = peer_exec};
		side->channel = dssh_chan_accept(side->sess, &cbs, 5000);
	}
	if (ret == 0 && side->channel == NULL)
		ret = DSSH_ERROR_INIT;
	if (ret == 0 && side->client) {
		int ready = dssh_chan_poll(side->channel, DSSH_POLL_WRITE, 5000);
		if (ready != DSSH_POLL_WRITE)
			ret = DSSH_ERROR_INIT;
	}
	if (ret != 0) {
		mock_io_close_c2s(side->io);
		mock_io_close_s2c(side->io);
	}
	return ret;
}

int
main(void)
{
	if (dssh_register_curve25519_sha256() != 0 || dssh_register_ssh_ed25519() != 0
	    || dssh_register_aes256_ctr() != 0 || dssh_register_hmac_sha2_256() != 0
	    || dssh_register_none_comp() != 0 || dssh_ed25519_generate_key() != 0
	    || dssh_transport_set_callbacks(peer_tx, peer_rx, peer_rxline, NULL) != 0)
		return 2;
	struct mock_io_state io;
	if (mock_io_init(&io, 0) != 0)
		return 2;
	struct peer_side sides[2] = {
		{.io = &io, .client = true}, {.io = &io, .client = false},
	};
	bool valid = true;
	for (size_t i = 0; i < 2; i++) {
		sides[i].sess = dssh_session_init(sides[i].client, 0);
		if (sides[i].sess == NULL) {
			valid = false;
			break;
		}
		if (dssh_session_set_cbdata(sides[i].sess, &sides[i], &sides[i], &sides[i], NULL) != 0)
			valid = false;
	}
	if (valid && (dssh_session_set_hostkey_verify_cb(sides[0].sess, peer_verify, NULL) != 0
	    || dssh_session_set_event_cb(sides[1].sess, peer_event, NULL) != 0))
		valid = false;
	thrd_t workers[2];
	bool started[2] = {false, false};
	for (size_t i = 0; i < 2 && valid; i++) {
		started[i] = thrd_create(&workers[i], peer_worker, &sides[i]) == thrd_success;
		valid = started[i];
	}
	if (!valid) {
		mock_io_close_c2s(&io);
		mock_io_close_s2c(&io);
	}
	for (size_t i = 0; i < 2; i++) {
		if (started[i]) {
			int result;
			if (thrd_join(workers[i], &result) != thrd_success)
				abort();
			valid = valid && result == 0;
		}
	}
	if (valid) {
		static const uint8_t bytes[] = {1, 2, 3};
		valid = dssh_chan_write(sides[0].channel, 0, bytes, 3) == 3
		    && dssh_chan_write(sides[0].channel, 1, bytes, 2) == 2;
		for (unsigned i = 0; i < 130 && valid; i++)
			valid = dssh_chan_send_signal(sides[0].channel, "TERM") == 0;
		valid = valid && dssh_test_wait_int(&event_count, 130)
		    && !atomic_load(&invalid_event)
		    && dssh_chan_poll(sides[1].channel, DSSH_POLL_EVENT, 0) == 0;
	}
	mock_io_close_c2s(&io);
	mock_io_close_s2c(&io);
	dssh_session_cleanup(sides[1].sess);
	dssh_session_cleanup(sides[0].sess);
	mock_io_free(&io);
	if (!valid)
		return 1;
	puts("events=130 queue=0 positions=3/2 production=1");
	return 0;
}
