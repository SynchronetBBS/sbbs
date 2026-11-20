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

struct peer_pair;
struct peer_side {
	struct mock_io_state *io;
	dssh_session sess;
	dssh_channel channel;
	struct peer_pair *pair;
	bool client;
};
struct peer_pair {
	struct mock_io_state io;
	struct peer_side sides[2];
	unsigned kind;
	unsigned callbacks;
	bool valid;
	struct peer_pair *nested;
	dssh_channel outer;
	atomic_bool ready;
};
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
peer_inner(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	(void)result;
	struct peer_pair *pair = opaque;
	const char *outer = dssh_chan_get_command(pair->outer);
	const char *inner = dssh_chan_get_command(ch);
	uint8_t byte = 1;
	pair->valid = pair->valid && outer != NULL && inner != NULL
	    && strcmp(outer, "first") == 0 && strcmp(inner, "nested") == 0
	    && inner == params->command
	    && dssh_chan_write(pair->outer, 0, &byte, 1) == DSSH_ERROR_INVALID
	    && dssh_chan_read(ch, 0, &byte, 1) == DSSH_ERROR_INVALID;
	pair->callbacks++;
	if (pair->callbacks == 1)
		pair->valid = pair->valid && dssh_chan_close(ch, -1) == 0;
	return 0;
}

static int
peer_check(dssh_channel ch, const struct dssh_chan_params *params,
    struct peer_pair *pair, unsigned kind)
{
	bool valid = dssh_chan_get_type(ch) == params->type
	    && dssh_chan_has_pty(ch) == ((params->flags & DSSH_PARAM_HAS_PTY) != 0)
	    && dssh_chan_get_pty(ch) == ((params->flags & DSSH_PARAM_HAS_PTY) != 0 ? params : NULL)
	    && dssh_chan_get_command(ch) == (params->type == DSSH_CHAN_EXEC ? params->command : NULL)
	    && dssh_chan_get_subsystem(ch) == (params->type == DSSH_CHAN_SUBSYSTEM ? params->subsystem : NULL)
	    && dssh_chan_set_event_cb(ch, NULL, NULL) == 0 && dssh_chan_set_max_events(ch, 16) == 0;
	if (pair->kind == 5) {
		pair->outer = ch;
		struct dssh_chan_accept_cbs cbs = {.exec = peer_inner, .cbdata = pair->nested};
		pair->nested->outer = ch;
		valid = valid && dssh_test_wait_bool(&pair->nested->ready, true);
		if (valid)
			pair->nested->sides[1].channel = dssh_chan_accept(pair->nested->sides[1].sess, &cbs, 5000);
		uint8_t byte = 47;
		valid = valid && pair->nested->sides[1].channel != NULL
		    && pair->nested->callbacks == 2 && pair->nested->valid
		    && dssh_chan_poll(pair->nested->sides[1].channel, DSSH_POLL_WRITE, 5000) == DSSH_POLL_WRITE
		    && dssh_chan_write(pair->nested->sides[1].channel, 0, &byte, 1) == 1
		    && dssh_chan_get_command(ch) == params->command
		    && strcmp(dssh_chan_get_command(ch), "first") == 0
		    && dssh_chan_write(ch, 0, &byte, 1) == DSSH_ERROR_INVALID;
	}
	else if (pair->kind == kind && pair->callbacks == 0) {
		pair->callbacks++;
		valid = valid && dssh_chan_close(ch, INT64_MAX) == 0;
	}
	pair->valid = pair->valid && valid;
	return 0;
}

static int
peer_pty(dssh_channel ch, const struct dssh_chan_params *params, void *opaque)
{
	return peer_check(ch, params, opaque, 0);
}
static int
peer_env(dssh_channel ch, const struct dssh_chan_params *params, void *opaque)
{
	return peer_check(ch, params, opaque, 1);
}
static int
peer_shell(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	(void)result;
	return peer_check(ch, params, opaque, 2);
}
static int
peer_exec(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	(void)result;
	return peer_check(ch, params, opaque, 3);
}
static int
peer_subsystem(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	(void)result;
	return peer_check(ch, params, opaque, 4);
}

static dssh_channel
peer_open(struct peer_side *side, bool first)
{
	unsigned kind = side->pair->kind;
	enum dssh_chan_type type = first && kind == 2 ? DSSH_CHAN_SHELL
	    : first && kind == 4 ? DSSH_CHAN_SUBSYSTEM : DSSH_CHAN_EXEC;
	struct dssh_chan_params params;
	if (dssh_chan_params_init(&params, type) != 0)
		return NULL;
	int ret = dssh_chan_params_set_max_window(&params, 128);
	if (ret == 0 && first && kind == 0)
		ret = dssh_chan_params_set_pty(&params, true);
	if (ret == 0 && first && kind == 1)
		ret = dssh_chan_params_add_env(&params, "A", "B");
	if (ret == 0 && type == DSSH_CHAN_EXEC)
		ret = dssh_chan_params_set_command(&params, kind == 6 ? "nested" : first ? "first" : "second");
	if (ret == 0 && type == DSSH_CHAN_SUBSYSTEM)
		ret = dssh_chan_params_set_subsystem(&params, "test");
	dssh_channel ch = ret == 0 ? dssh_chan_open(side->sess, &params) : NULL;
	dssh_chan_params_free(&params);
	return ch;
}

static int
peer_worker(void *opaque)
{
	struct peer_side *side = opaque;
	int ret = dssh_transport_handshake(side->sess);
	if (ret == 0 && side->client)
		ret = dssh_auth_password(side->sess, "user", "pass", NULL, NULL);
	else if (ret == 0) {
		struct dssh_auth_server_cbs cbs = {.methods_str = "password", .password_cb = peer_password};
		ret = dssh_auth_server(side->sess, &cbs, NULL, NULL);
	}
	if (ret == 0)
		ret = dssh_session_start(side->sess);
	if (ret == 0 && side->pair->kind == 6 && !side->client) {
		atomic_store(&side->pair->ready, true);
		return 0;
	}
	if (ret == 0 && side->client) {
		side->channel = peer_open(side, true);
		if (side->pair->kind != 5) {
			if (side->channel != NULL)
				ret = DSSH_ERROR_INVALID;
			else
				side->channel = peer_open(side, false);
		}
		if (ret == 0 && side->channel == NULL)
			ret = DSSH_ERROR_INIT;
		if (ret == 0 && side->pair->kind == 6) {
			uint8_t byte = 0;
			int ready = dssh_chan_poll(side->channel, DSSH_POLL_READ, 5000);
			if ((ready & DSSH_POLL_READ) == 0 || dssh_chan_read(side->channel, 0, &byte, 1) != 1 || byte != 47)
				ret = DSSH_ERROR_INIT;
		}
	}
	else if (ret == 0) {
		struct dssh_chan_accept_cbs cbs = {.pty_req = peer_pty, .env = peer_env,
		    .shell = peer_shell, .exec = peer_exec, .subsystem = peer_subsystem, .cbdata = side->pair};
		side->channel = dssh_chan_accept(side->sess, &cbs, 5000);
		if (side->channel == NULL || !side->pair->valid)
			ret = DSSH_ERROR_INIT;
	}
	if (ret != 0) {
		fprintf(stderr, "peer kind=%u client=%u ret=%d ch=%u\n",
		    side->pair->kind, side->client ? 1U : 0U, ret,
		    side->channel != NULL ? 1U : 0U);
		mock_io_close_c2s(side->io);
		mock_io_close_s2c(side->io);
	}
	return ret;
}

int
main(int argc, char **argv)
{
	if (argc != 2 || strlen(argv[1]) != 1 || argv[1][0] < '0' || argv[1][0] > '5')
		return 2;
	unsigned kind = (unsigned)(argv[1][0] - '0');
	if (dssh_register_curve25519_sha256() != 0 || dssh_register_ssh_ed25519() != 0
	    || dssh_register_aes256_ctr() != 0 || dssh_register_hmac_sha2_256() != 0
	    || dssh_register_none_comp() != 0 || dssh_ed25519_generate_key() != 0
	    || dssh_transport_set_callbacks(peer_tx, peer_rx, peer_rxline, NULL) != 0)
		return 2;
	struct peer_pair pairs[2] = {{.kind = kind, .valid = true}, {.kind = 6, .valid = true}};
	pairs[0].nested = &pairs[1];
	size_t count = kind == 5 ? 2 : 1;
	bool valid = true;
	for (size_t p = 0; p < count; p++) {
		atomic_init(&pairs[p].ready, false);
		if (mock_io_init(&pairs[p].io, 0) != 0)
			return 2;
		for (size_t i = 0; i < 2; i++) {
			struct peer_side *side = &pairs[p].sides[i];
			side->io = &pairs[p].io;
			side->pair = &pairs[p];
			side->client = i == 0;
			side->sess = dssh_session_init(side->client, 0);
			if (side->sess == NULL)
				return 2;
			if (dssh_session_set_cbdata(side->sess, side, side, side, NULL) != 0
			    || dssh_session_set_timeout(side->sess, 5000) != 0)
				valid = false;
		}
		if (dssh_session_set_hostkey_verify_cb(pairs[p].sides[0].sess, peer_verify, NULL) != 0)
			valid = false;
	}
	thrd_t workers[4];
	bool started[4] = {false};
	for (size_t i = 0; i < count * 2 && valid; i++) {
		started[i] = thrd_create(&workers[i], peer_worker, &pairs[i / 2].sides[i % 2]) == thrd_success;
		valid = started[i];
	}
	if (!valid) {
		for (size_t p = 0; p < count; p++) {
			mock_io_close_c2s(&pairs[p].io);
			mock_io_close_s2c(&pairs[p].io);
		}
	}
	for (size_t i = 0; i < count * 2; i++) {
		if (started[i]) {
			int result;
			if (thrd_join(workers[i], &result) != thrd_success)
				abort();
			valid = valid && result == 0;
		}
	}
	valid = valid && pairs[0].valid && (kind == 5 ? pairs[1].valid && pairs[1].callbacks == 2
	    : pairs[0].callbacks == 1);
	for (size_t p = 0; p < count; p++) {
		mock_io_close_c2s(&pairs[p].io);
		mock_io_close_s2c(&pairs[p].io);
		dssh_session_cleanup(pairs[p].sides[1].sess);
		dssh_session_cleanup(pairs[p].sides[0].sess);
		mock_io_free(&pairs[p].io);
	}
	if (!valid)
		return 1;
	printf("setup_contract=1 kind=%u nested=%u production=1\n", kind, kind == 5 ? 1U : 0U);
	return 0;
}
