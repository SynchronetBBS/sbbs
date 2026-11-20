#ifndef DSSH_TEST_DIRECT_KEX_FIXTURE_H
#define DSSH_TEST_DIRECT_KEX_FIXTURE_H

#include "session_pair_fixture.h"

static int
direct_kex_negotiate(dssh_session sess)
{
	int ret = version_exchange(sess);
	if (ret == 0)
		ret = kexinit(sess);
	return ret;
}

static int
direct_kex_prepare(struct test_session_pair *pair, bool gex)
{
	test_pair_begin(pair);
	dssh_test_reset_global_config();
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	int ret = gex ? dssh_register_dh_gex_sha256() : dssh_register_curve25519_sha256();
	if (ret == 0)
		ret = dssh_register_ssh_ed25519();
	if (ret == 0)
		ret = dssh_register_aes256_ctr();
	if (ret == 0)
		ret = test_register_mac_algos();
	if (ret == 0)
		ret = dssh_register_none_comp();
	if (ret == 0)
		ret = dssh_ed25519_generate_key();
	if (ret == 0)
		ret = dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
		    mock_rxline_dispatch, mock_extra_line_cb);
	if (ret == 0)
		ret = test_pair_open(pair);
	return ret;
}

static bool
direct_kex_negotiated(struct test_session_pair *pair, bool gex)
{
	dssh_session sessions[] = {pair->client, pair->server};
	enum dssh_direction directions[] = {DSSH_CLIENT_TO_SERVER, DSSH_SERVER_TO_CLIENT};
	const char *expected = gex ? "diffie-hellman-group-exchange-sha256" : "curve25519-sha256";
	for (size_t i = 0; i < sizeof(sessions) / sizeof(sessions[0]); i++) {
		const char *kex_name = dssh_transport_get_kex_name(sessions[i]);
		const char *key = dssh_transport_get_hostkey_name(sessions[i]);
		if (kex_name == NULL || key == NULL || strcmp(kex_name, expected) != 0
		    || strcmp(key, "ssh-ed25519") != 0)
			return false;
		for (size_t j = 0; j < sizeof(directions) / sizeof(directions[0]); j++) {
			const char *enc = dssh_transport_get_enc_name(sessions[i], directions[j]);
			const char *mac = dssh_transport_get_mac_name(sessions[i], directions[j]);
			const char *comp = dssh_transport_get_comp_name(sessions[i], directions[j]);
			if (enc == NULL || mac == NULL || comp == NULL || strcmp(enc, "aes256-ctr") != 0
			    || strcmp(mac, test_using_hmac256() ? "hmac-sha2-256" : "hmac-sha2-512") != 0
			    || strcmp(comp, "none") != 0)
				return false;
		}
	}
	return pair->created == 2 && pair->joined == 2
	    && pair->client_result == 0 && pair->server_result == 0;
}

static void
direct_kex_dispose(struct test_session_pair *pair)
{
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	test_pair_destroy(pair);
	dssh_test_reset_global_config();
}

static int
direct_kex_setup(struct test_session_pair *pair, bool gex)
{
	int ret = direct_kex_prepare(pair, gex);
	if (ret == 0)
		ret = test_pair_run(pair, direct_kex_negotiate);
	if (ret == 0 && !direct_kex_negotiated(pair, gex))
		ret = DSSH_ERROR_INVALID;
	if (ret != 0) {
		direct_kex_dispose(pair);
		return ret;
	}
	dssh_session client = pair->client;
	pair->client = NULL;
	dssh_session_cleanup(client);
	return 0;
}

/* Append a complete aligned plaintext packet. Publish the cursor only after
 * validating every extent and completing serialization. */
static int
build_plaintext_packet_t(const uint8_t *payload, size_t payload_len,
    uint8_t *buf, size_t capacity, size_t *position)
{
	if (buf == NULL || position == NULL || (payload == NULL && payload_len != 0)
	    || *position > capacity)
		return DSSH_ERROR_INVALID;
	if (payload_len > SIZE_MAX - 16)
		return DSSH_ERROR_TOOLONG;
	size_t minimum = payload_len + 9;
	size_t total = (minimum + 7) / 8 * 8;
	if (total > capacity - *position)
		return DSSH_ERROR_TOOLONG;
#if SIZE_MAX > UINT32_MAX
	if (total - 4 > UINT32_MAX)
		return DSSH_ERROR_TOOLONG;
#endif
	size_t padding = total - 5 - payload_len;
	uint32_t packet_length = (uint32_t)(total - 4);
	uint8_t padding_length = (uint8_t)padding; /* Alignment fixes this at4..11. */
	size_t cursor = *position;
	int ret = dssh_serialize_uint32(packet_length, buf, capacity, &cursor);
	if (ret != 0)
		return ret;
	buf[cursor++] = padding_length;
	if (payload_len != 0)
		memcpy(&buf[cursor], payload, payload_len);
	cursor += payload_len;
	memset(&buf[cursor], 0, padding);
	cursor += padding;
	*position = cursor;
	return 0;
}

/* A fixture failure has its own status; a rejected KEX is only *operation.
 * Callback data always names the pair's persistent IO object, never a local
 * iteration object. Constructor/free transitions occur after all workers join. */
static int
direct_kex_run(struct test_session_pair *pair, const uint8_t *wire, size_t length,
    int allocation_failure, int *operation)
{
	if (operation == NULL || (wire == NULL && length != 0) || pair->server == NULL
	    || pair->owned[0] || pair->owned[1] || pair->created != pair->joined)
		return DSSH_ERROR_INVALID;
	*operation = DSSH_ERROR_INIT;
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	if (pair->io_ready) {
		pair->io_ready = false;
		mock_io_free(&pair->io);
	}
	if (mock_io_init(&pair->io, 0) != 0)
		return DSSH_ERROR_INIT;
	pair->io_ready = true;
	if (length != 0 && mock_io_inject(&pair->io.c2s, wire, length) != length)
		return DSSH_ERROR_INIT;
	mock_io_close_c2s_write(&pair->io);
	atomic_store(&pair->server->terminate, false);
	if (allocation_failure >= 0)
		dssh_test_alloc_fail_after(allocation_failure);
	*operation = kex(pair->server);
	dssh_test_alloc_reset();
	pair->io_ready = false;
	mock_io_free(&pair->io);
	return 0;
}

#endif
