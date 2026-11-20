#ifndef DSSH_TRANSPORT_MODULE_CASES_H
#define DSSH_TRANSPORT_MODULE_CASES_H

static int
stateless_registration(void)
{
	int ret = dssh_test_register_stateless();
	if (ret != 0)
		return ret;
	return dssh_test_register_growth_comp();
}

static bool
stateless_epoch(dssh_session sess, const char *enc, const char *mac)
{
	bool enc_null = strcmp(enc, DSSH_TEST_STATELESS_NAME) == 0;
	bool mac_null = strcmp(mac, DSSH_TEST_STATELESS_NAME) == 0;
	return sess->trans.enc_c2s_active != NULL && sess->trans.enc_s2c_active != NULL
	    && sess->trans.mac_c2s_active != NULL && sess->trans.mac_s2c_active != NULL
	    && strcmp(sess->trans.enc_c2s_active->name, enc) == 0
	    && strcmp(sess->trans.enc_s2c_active->name, enc) == 0
	    && strcmp(sess->trans.mac_c2s_active->name, mac) == 0
	    && strcmp(sess->trans.mac_s2c_active->name, mac) == 0
	    && (sess->trans.enc_c2s_ctx == NULL) == enc_null
	    && (sess->trans.enc_s2c_ctx == NULL) == enc_null
	    && (sess->trans.mac_c2s_ctx == NULL) == mac_null
	    && (sess->trans.mac_s2c_ctx == NULL) == mac_null
	    && sess->trans.comp_c2s_active != NULL && sess->trans.comp_s2c_active != NULL
	    && strcmp(sess->trans.comp_c2s_active->name, "test-growth") == 0
	    && strcmp(sess->trans.comp_s2c_active->name, "test-growth") == 0;
}

static int
test_modules_stateless(void)
{
	const char *encs[] = { "aes256-ctr", DSSH_TEST_STATELESS_NAME };
	const char *macs[] = { test_using_hmac256() ? "hmac-sha2-256" : "hmac-sha2-512",
	    DSSH_TEST_STATELESS_NAME };
	const char *comp = "test-growth";
	const uint8_t payload[] = { SSH_MSG_CHANNEL_DATA, 1, 2, 3, 4, 0xff, 0, 0xa5 };
	/* All independent cipher/MAC choices before and after an actual rekey. */
	for (unsigned mask = 0; mask < 16; mask++) {
		ASSERT_OK(dssh_test_stateless_reset());
		ASSERT_OK(dssh_test_comp_reset());
		unsigned ei = mask & 1;
		unsigned mi = (mask >> 1) & 1;
		unsigned ef = (mask >> 2) & 1;
		unsigned mf = (mask >> 3) & 1;
		struct handshake_ctx pair;
		extra_algorithm_registration = stateless_registration;
		int ret = handshake_setup_filtered(&pair, false, comp, encs[ei], macs[mi]);
		extra_algorithm_registration = NULL;
		if (ret != 0) {
			handshake_cleanup(&pair);
			return TEST_FAIL;
		}
		bool valid = stateless_epoch(pair.client, encs[ei], macs[mi])
		    && stateless_epoch(pair.server, encs[ei], macs[mi])
		    && compression_exchange(&pair, true, payload, sizeof(payload))
		    && compression_exchange(&pair, false, payload, sizeof(payload));
		struct dssh_test_stateless_stats stats;
		dssh_test_stateless_stats(&stats);
		valid = valid && stats.enc_init == 4 * ei && stats.enc_cleanup == 0
		    && stats.mac_init == 4 * mi && stats.mac_cleanup == 0 && stats.errors == 0;
		uint8_t *received[] = { pair.client->trans.rx_mac_buf, pair.server->trans.rx_mac_buf };
		uint8_t *computed[] = { pair.client->trans.rx_mac_computed, pair.server->trans.rx_mac_computed };
		size_t capacity = pair.client->trans.rx_mac_capacity;
		valid = valid && capacity == pair.server->trans.rx_mac_capacity;
		if (valid)
			valid = dssh_session_set_enc_filter(pair.client, &encs[ef], 1) == 0
			    && dssh_session_set_enc_filter(pair.server, &encs[ef], 1) == 0
			    && dssh_session_set_mac_filter(pair.client, &macs[mf], 1) == 0
			    && dssh_session_set_mac_filter(pair.server, &macs[mf], 1) == 0
			    && compression_rekey_pair(&pair);
		if (valid) {
			size_t needed = pair.client->trans.mac_s2c_active->digest_size;
			valid = stateless_epoch(pair.client, encs[ef], macs[mf])
			    && stateless_epoch(pair.server, encs[ef], macs[mf])
			    && pair.client->trans.rx_mac_capacity == (needed > capacity ? needed : capacity)
			    && pair.server->trans.rx_mac_capacity == pair.client->trans.rx_mac_capacity;
			/* Old pointer values are inspected only when the buffers were retained. */
			if (needed <= capacity)
				valid = valid && received[0] == pair.client->trans.rx_mac_buf
				    && received[1] == pair.server->trans.rx_mac_buf
				    && computed[0] == pair.client->trans.rx_mac_computed
				    && computed[1] == pair.server->trans.rx_mac_computed;
		}
		if (valid)
			valid = compression_exchange(&pair, true, payload, sizeof(payload))
			    && compression_exchange(&pair, false, payload, sizeof(payload));
		dssh_test_stateless_stats(&stats);
		valid = valid && stats.enc_init == 4 * (ei + ef) && stats.enc_cleanup == 4 * ei
		    && stats.mac_init == 4 * (mi + mf) && stats.mac_cleanup == 4 * mi && stats.errors == 0;
		if (ei + ef != 0)
			valid = valid && stats.encrypt > 0 && stats.decrypt > 0;
		if (mi + mf != 0)
			valid = valid && stats.generate > 0;
		struct dssh_test_comp_stats growth;
		dssh_test_comp_stats(&growth);
		valid = valid && growth.allocations == 4 && growth.resets == 4
		    && growth.cleanups == 0 && growth.live == 4 && growth.owner_errors == 0;
		handshake_cleanup(&pair);
		dssh_test_stateless_stats(&stats);
		dssh_test_comp_stats(&growth);
		valid = valid && stats.enc_init == stats.enc_cleanup && stats.mac_init == stats.mac_cleanup
		    && stats.errors == 0 && growth.cleanups == 4 && growth.live == 0 && growth.owner_errors == 0;
		if (!valid)
			fprintf(stderr, "  stateless module transition mask %u failed\n", mask);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

#endif
