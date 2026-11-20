#ifndef DSSH_TEST_TRANSPORT_PAIR_CASES_H
#define DSSH_TEST_TRANSPORT_PAIR_CASES_H

#include "session_pair_fixture.h"

static int
transport_pair_prepare(struct test_session_pair *pair, unsigned kind)
{
	test_pair_begin(pair);
	dssh_test_reset_global_config();
	if (kind > 4)
		return DSSH_ERROR_INVALID;
	int ret = 0;
	if (kind == 2) {
		atomic_store(&gather_invoked, 0);
		ret = dssh_transport_set_tx_gather(test_gather_cb);
	}
	if (ret == 0)
		ret = kind >= 2 ? register_all_algorithms() : dssh_register_none_comp();
	if (ret == 0 && kind >= 2)
		ret = test_generate_host_key();
	if (ret == 0)
		ret = dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
		    kind == 3 ? NULL : mock_rxline_dispatch, mock_extra_line_cb);
	if (ret == 0)
		ret = test_pair_open(pair);
	return ret;
}

static void
transport_pair_finish(struct test_session_pair *pair)
{
	test_pair_destroy(pair);
	dssh_test_reset_global_config();
}

static bool
transport_pair_retired(struct test_session_pair *pair)
{
	return !pair->registered && !pair->io_ready && pair->client == NULL && pair->server == NULL
	    && !pair->owned[0] && !pair->owned[1] && pair->created == pair->joined
	    && atomic_load(&pair->io.c2s.rfd) == -1 && atomic_load(&pair->io.c2s.wfd) == -1
	    && atomic_load(&pair->io.s2c.rfd) == -1 && atomic_load(&pair->io.s2c.wfd) == -1
	    && test_pair_active == NULL;
}

static bool
transport_pair_versions(struct test_session_pair *pair)
{
	const char *client_remote = dssh_transport_get_remote_version(pair->client);
	const char *server_remote = dssh_transport_get_remote_version(pair->server);
	size_t client_length = pair->client->trans.id_str_sz;
	size_t server_length = pair->server->trans.id_str_sz;
	return client_remote != NULL && server_remote != NULL
	    && client_length > 8 && client_length < sizeof(pair->client->trans.id_str)
	    && server_length > 8 && server_length < sizeof(pair->server->trans.id_str)
	    && pair->client->trans.remote_id_str_sz == server_length
	    && pair->server->trans.remote_id_str_sz == client_length
	    && memcmp(client_remote, pair->server->trans.id_str, server_length + 1) == 0
	    && memcmp(server_remote, pair->client->trans.id_str, client_length + 1) == 0;
}

static bool
transport_pair_exchange(dssh_session sender, dssh_session receiver, bool gather, uint8_t type)
{
	uint8_t message[] = {type, 0, 0, 0, 12,
	    's', 's', 'h', '-', 'u', 's', 'e', 'r', 'a', 'u', 't', 'h'};
	if (gather)
		atomic_store(&gather_invoked, 0);
	int ret = send_packet(sender, message, sizeof(message), NULL);
	uint8_t received_type = 0;
	uint8_t *payload = NULL;
	size_t length = 0;
	if (ret == 0)
		ret = recv_packet(receiver, &received_type, &payload, &length);
	return ret == 0 && received_type == type && length == sizeof(message)
	    && memcmp(payload, message, sizeof(message)) == 0
	    && (!gather || atomic_load(&gather_invoked) != 0);
}

static bool
transport_pair_algorithms(struct test_session_pair *pair)
{
	dssh_session sessions[] = {pair->client, pair->server};
	enum dssh_direction directions[] = {DSSH_CLIENT_TO_SERVER, DSSH_SERVER_TO_CLIENT};
	struct {
		const char *(*query)(dssh_session, enum dssh_direction);
		const char *expected;
	} queries[] = {
		{dssh_transport_get_enc_name, "aes256-ctr"},
		{dssh_transport_get_mac_name, test_using_hmac256() ? "hmac-sha2-256" : "hmac-sha2-512"},
		{dssh_transport_get_comp_name, "none"},
	};
	for (size_t i = 0; i < sizeof(sessions) / sizeof(sessions[0]); i++) {
		const char *kex = dssh_transport_get_kex_name(sessions[i]);
		const char *key = dssh_transport_get_hostkey_name(sessions[i]);
		if (kex == NULL || key == NULL || strcmp(kex, test_kex_algo_name()) != 0
		    || strcmp(key, test_key_algo_name()) != 0)
			return false;
		for (size_t j = 0; j < sizeof(directions) / sizeof(directions[0]); j++) {
			for (size_t k = 0; k < sizeof(queries) / sizeof(queries[0]); k++) {
				const char *name = queries[k].query(sessions[i], directions[j]);
				if (name == NULL || strcmp(name, queries[k].expected) != 0)
					return false;
			}
		}
	}
	return true;
}

static bool
transport_pair_observe(struct test_session_pair *pair, unsigned kind)
{
	bool valid = pair->created == 2 && pair->joined == 2
	    && pair->client_result == 0 && pair->server_result == 0
	    && transport_pair_versions(pair);
	if (valid && kind >= 2) {
		valid = transport_pair_algorithms(pair);
		if (valid)
			valid = transport_pair_exchange(pair->client, pair->server, kind == 2,
			    SSH_MSG_SERVICE_REQUEST);
		if (valid)
			valid = transport_pair_exchange(pair->server, pair->client, kind == 2,
			    SSH_MSG_SERVICE_ACCEPT);
	}
	return valid;
}

static bool
transport_pair_case(unsigned kind)
{
	struct test_session_pair pair;
	int ret = transport_pair_prepare(&pair, kind);
	if (ret == 0)
		ret = test_pair_run(&pair, kind < 2 ? version_exchange : dssh_transport_handshake);
	bool valid = ret == 0 && transport_pair_observe(&pair, kind);
	transport_pair_finish(&pair);
	valid = valid && transport_pair_retired(&pair);
	return valid;
}

static void
transport_pair_assertion_cleanup(void)
{
	if (test_pair_active != NULL) {
		test_pair_destroy_active();
		dssh_test_reset_global_config();
	}
}

#ifdef DSSH_TRANSPORT_PAIR_CONTROLS
static int
transport_pair_control_kind(unsigned *kind)
{
	const char *value = getenv("DSSH_TRANSPORT_PAIR_KIND");
	if (value == NULL || strlen(value) != 1 || value[0] < '0' || value[0] > '3')
		return DSSH_ERROR_INVALID;
	unsigned parsed = (unsigned)(value[0] - '0');
	*kind = parsed;
	return 0;
}

static int
test_transport_pair_ownership(void)
{
	unsigned kind;
	if (transport_pair_control_kind(&kind) != 0)
		return TEST_FAIL;
	struct test_session_pair pair;
	int ret = transport_pair_prepare(&pair, kind);
	if (ret == 0)
		ret = test_pair_run(&pair, kind < 2 ? version_exchange : dssh_transport_handshake);
	const char *mode = getenv("DSSH_PEER_FAILURE");
	bool valid = false;
	if (mode != NULL && strcmp(mode, "plain") == 0)
		valid = ret == 0 && transport_pair_observe(&pair, kind);
	else if (mode != NULL && strcmp(mode, "tx") == 0)
		valid = ret < 0 && pair.created == 2;
	else if (mode != NULL && (strcmp(mode, "create-0") == 0 || strcmp(mode, "create-1") == 0))
		valid = ret == DSSH_ERROR_INIT;
	transport_pair_finish(&pair);
	valid = valid && transport_pair_retired(&pair);
	int printed = printf("fixture_owner=1 created=%u joined=%u valid=%d\n",
	    pair.created, pair.joined, valid);
	ASSERT_TRUE(valid && printed > 0);
	return TEST_PASS;
}

static int
transport_pair_assertion_failure(void)
{
	ASSERT_TRUE(false);
	return TEST_PASS;
}

static int
transport_pair_assertion_control(unsigned phase)
{
	unsigned kind;
	if (phase > 2 || transport_pair_control_kind(&kind) != 0)
		return 2;
	struct test_session_pair pair;
	int ret = transport_pair_prepare(&pair, kind);
	pair.operation = kind < 2 ? version_exchange : dssh_transport_handshake;
	for (unsigned i = 0; ret == 0 && i < phase; i++)
		ret = test_pair_start(&pair, i);
	if (ret != 0) {
		transport_pair_finish(&pair);
		return 2;
	}
	ret = transport_pair_assertion_failure();
	bool valid = ret == TEST_FAIL && transport_pair_retired(&pair);
	int printed = printf("fixture_assertion=1 phase=%u created=%u joined=%u valid=%d\n",
	    phase, pair.created, pair.joined, valid);
	return valid && printed > 0 ? 1 : 2;
}
#endif

#endif
