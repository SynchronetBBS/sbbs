/* Public-header application linked to the production library. The closed
 * socketpair and trust callback belong to this fixture, not an SSH client UI. */
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "deucessh.h"
#include "deucessh-auth.h"
#include "deucessh-algorithms.h"
#include "mock_io.h"

#if defined(DSSH_AUTH_NONE_ACCEPTED) || defined(DSSH_AUTH_METHODS_AVAILABLE) || defined(DSSH_ERROR_AUTH_REJECTED)
#error Retired client authentication interfaces must not be exported
#endif

_Static_assert(_Generic(&dssh_auth_begin,
    int (*)(dssh_session, const char *): 1, default: 0), "authentication action signature");
_Static_assert(_Generic(&dssh_auth_get_methods,
    int64_t (*)(dssh_session, char *, size_t): 1, default: 0), "pure methods getter signature");

struct action_pair;
struct action_side {
	struct action_pair *pair;
	dssh_session sess;
	bool client;
	bool callback_checks;
	atomic_uint io_calls;
};

struct action_pair {
	struct mock_io_state io;
	struct action_side sides[2];
	struct dssh_auth_server_cbs cbs;
	bool probe;
	unsigned none_calls, passwords, interactive;
};

static int
action_tx(uint8_t *buf, size_t length, dssh_session sess, void *opaque)
{
	struct action_side *side = opaque;
	atomic_fetch_add(&side->io_calls, 1);
	side->callback_checks &= dssh_auth_get_methods(sess, NULL, 0) == DSSH_ERROR_INVALID
	    && dssh_auth_begin(sess, "user") == DSSH_ERROR_INVALID;
	return side->client ? mock_tx_client(buf, length, sess, &side->pair->io)
	    : mock_tx_server(buf, length, sess, &side->pair->io);
}

static int
action_rx(uint8_t *buf, size_t length, dssh_session sess, void *opaque)
{
	struct action_side *side = opaque;
	atomic_fetch_add(&side->io_calls, 1);
	side->callback_checks &= dssh_auth_get_methods(sess, NULL, 0) == DSSH_ERROR_INVALID
	    && dssh_auth_begin(sess, "user") == DSSH_ERROR_INVALID;
	return side->client ? mock_rx_client(buf, length, sess, &side->pair->io)
	    : mock_rx_server(buf, length, sess, &side->pair->io);
}

static int
action_line(uint8_t *buf, size_t length, size_t *received, dssh_session sess, void *opaque)
{
	struct action_side *side = opaque;
	atomic_fetch_add(&side->io_calls, 1);
	side->callback_checks &= dssh_auth_get_methods(sess, NULL, 0) == DSSH_ERROR_INVALID
	    && dssh_auth_begin(sess, "user") == DSSH_ERROR_INVALID;
	return side->client ? mock_rxline_client(buf, length, received, sess, &side->pair->io)
	    : mock_rxline_server(buf, length, received, sess, &side->pair->io);
}

static dssh_hostkey_decision
action_trust(const char *algorithm, unsigned bits, const uint8_t *hash,
    const uint8_t *key, size_t key_len, void *opaque)
{
	(void)algorithm;
	(void)bits;
	(void)hash;
	(void)key;
	(void)key_len;
	(void)opaque;
	return DSSH_HOSTKEY_ACCEPT;
}

static int
action_none(const uint8_t *user, size_t length, void *opaque)
{
	(void)user;
	(void)length;
	struct action_pair *pair = opaque;
	pair->none_calls++;
	return DSSH_AUTH_FAILURE;
}

static int
action_password(const uint8_t *user, size_t user_len, const uint8_t *password,
    size_t password_len, struct dssh_bytes_result *out, void *opaque)
{
	(void)out;
	struct action_pair *pair = opaque;
	pair->passwords++;
	if (user_len != 4 || password_len != 4 || memcmp(user, "user", 4) != 0
	    || memcmp(password, "pass", 4) != 0)
		return DSSH_AUTH_FAILURE;
	pair->cbs.methods_str = "keyboard-interactive";
	return DSSH_AUTH_PARTIAL;
}

static int
action_interactive(const uint8_t *user, size_t user_len, uint32_t count,
    const uint8_t **responses, const size_t *lengths,
    struct dssh_auth_challenge *out, void *opaque)
{
	(void)responses;
	(void)lengths;
	(void)out;
	struct action_pair *pair = opaque;
	pair->interactive++;
	return user_len == 4 && memcmp(user, "user", 4) == 0 && count == 0
	    && pair->passwords == 1 ? DSSH_AUTH_SUCCESS : DSSH_AUTH_FAILURE;
}

static int
action_unexpected_prompt(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len, uint32_t count,
    const uint8_t **prompts, const size_t *lengths, const bool *echo,
    struct dssh_auth_responses *out, void *opaque)
{
	(void)name;
	(void)name_len;
	(void)instruction;
	(void)instruction_len;
	(void)count;
	(void)prompts;
	(void)lengths;
	(void)echo;
	(void)out;
	(void)opaque;
	return DSSH_ERROR_INVALID;
}

static bool
action_methods(struct action_side *side, const char *expected)
{
	char methods[64];
	/* The two fixed lists in this application are shorter than 64 bytes. */
	int64_t needed = (int64_t)(strlen(expected) + 1);
	unsigned calls = atomic_load(&side->io_calls);
	return dssh_auth_get_methods(side->sess, NULL, 0) == needed
	    && dssh_auth_get_methods(side->sess, methods, sizeof(methods)) == needed
	    && strcmp(methods, expected) == 0 && atomic_load(&side->io_calls) == calls;
}

static int
action_worker(void *opaque)
{
	struct action_side *side = opaque;
	int ret = dssh_transport_handshake(side->sess);
	if (ret == 0 && !side->client)
		ret = dssh_auth_server(side->sess, &side->pair->cbs, NULL, NULL);
	else if (ret == 0) {
		if (dssh_auth_get_methods(side->sess, NULL, 0) != DSSH_ERROR_INIT)
			ret = DSSH_ERROR_INVALID;
		if (ret == 0 && side->pair->probe) {
			ret = dssh_auth_begin(side->sess, "user");
			ret = ret == DSSH_AUTH_CONTINUE && action_methods(side, "password") ? 0 : DSSH_ERROR_INVALID;
		}
		if (ret == 0) {
			ret = dssh_auth_password(side->sess, "user", "pass", NULL, NULL);
			ret = ret == DSSH_AUTH_PARTIAL && action_methods(side, "keyboard-interactive") ? 0 : DSSH_ERROR_INVALID;
		}
		if (ret == 0) {
			ret = dssh_auth_keyboard_interactive(side->sess, "user", action_unexpected_prompt, NULL);
			if (ret == 0 && !action_methods(side, ""))
				ret = DSSH_ERROR_INVALID;
		}
	}
	if (ret != 0) {
		mock_io_close_c2s(&side->pair->io);
		mock_io_close_s2c(&side->pair->io);
	}
	return ret;
}

int
main(int argc, char **argv)
{
	if (argc != 2 || (strcmp(argv[1], "begin") != 0 && strcmp(argv[1], "direct") != 0))
		return 2;
	if (dssh_register_curve25519_sha256() != 0 || dssh_register_ssh_ed25519() != 0
	    || dssh_register_aes256_ctr() != 0 || dssh_register_hmac_sha2_256() != 0
	    || dssh_register_none_comp() != 0 || dssh_ed25519_generate_key() != 0
	    || dssh_transport_set_callbacks(action_tx, action_rx, action_line, NULL) != 0)
		return 2;
	struct action_pair pair = { .probe = strcmp(argv[1], "begin") == 0 };
	pair.cbs = (struct dssh_auth_server_cbs){ .methods_str = "password", .none_cb = action_none,
	    .password_cb = action_password, .keyboard_interactive_cb = action_interactive, .cbdata = &pair };
	bool valid = mock_io_init(&pair.io, 0) == 0;
	for (unsigned i = 0; i < 2 && valid; i++) {
		struct action_side *side = &pair.sides[i];
		side->pair = &pair;
		side->client = i == 0;
		side->callback_checks = true;
		atomic_init(&side->io_calls, 0);
		side->sess = dssh_session_init(side->client, 0);
		valid = side->sess != NULL && dssh_session_set_cbdata(side->sess, side, side, side, NULL) == 0
		    && dssh_session_set_timeout(side->sess, 5000) == 0;
	}
	if (valid)
		valid = dssh_session_set_hostkey_verify_cb(pair.sides[0].sess, action_trust, NULL) == 0;
	if (valid)
		valid = dssh_auth_get_methods(pair.sides[1].sess, NULL, 0) == DSSH_ERROR_INVALID;
	thrd_t workers[2];
	bool started[2] = {false};
	for (unsigned i = 0; i < 2 && valid; i++) {
		started[i] = thrd_create(&workers[i], action_worker, &pair.sides[i]) == thrd_success;
		valid = started[i];
	}
	if (!valid) {
		mock_io_close_c2s(&pair.io);
		mock_io_close_s2c(&pair.io);
	}
	for (unsigned i = 0; i < 2; i++) {
		if (!started[i])
			continue;
		int result;
		if (thrd_join(workers[i], &result) != thrd_success)
			abort();
		valid &= result == 0;
	}
	valid &= pair.none_calls == (pair.probe ? 1U : 0U) && pair.passwords == 1 && pair.interactive == 1;
	valid &= pair.sides[0].callback_checks && pair.sides[1].callback_checks;
	dssh_session_cleanup(pair.sides[1].sess);
	dssh_session_cleanup(pair.sides[0].sess);
	mock_io_free(&pair.io);
	printf("auth_actions mode=%s none=%u passwords=%u interactive=%u valid=%d\n",
	    argv[1], pair.none_calls, pair.passwords, pair.interactive, valid);
	return valid ? 0 : 1;
}
