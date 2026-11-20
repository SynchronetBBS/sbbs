#ifndef DSSH_KEY_AVAILABILITY_CASES_H
#define DSSH_KEY_AVAILABILITY_CASES_H

static const int availability_results[] = { -77, DSSH_ERROR_INIT, DSSH_ERROR_TOOLONG, 0, 1, 2, INT_MAX };

static int
test_key_availability_auth(void)
{
	bool valid = true;
	size_t count = sizeof(availability_results) / sizeof(availability_results[0]);
	for (size_t i = 0; i <= count; i++) {
		dssh_test_reset_global_config();
		int available = i < count ? availability_results[i] : 0;
		struct module_fixture f = { .availability_test = true, .availability = available, .stage = 4 };
		active = &f;
		dssh_key_algo key = register_auth_module();
		bool matched = key != NULL;
		if (key != NULL && i == count)
			key->haskey = NULL;
		dssh_session sess = dssh_session_init(true, 0);
		matched = matched && sess != NULL;
		if (matched) {
			sess->handshake_complete = true;
			sess->auth_service_requested = true;
			int ret = dssh_auth_publickey(sess, "user", "contract-key");
			int expected = available < 0 ? available : (available == 0 ? DSSH_ERROR_INIT : DSSH_ERROR_INVALID);
			matched = ret == expected && f.availability_calls == (i == count ? 0U : 1U)
			    && f.pubkey_calls == (available > 0 ? 1U : 0U) && f.writes == 0;
			/* The positive controls stop at a deliberately unset public-key pair. */
			if (!matched)
				fprintf(stderr, "availability auth: value=%d absent=%d result=%d expected=%d calls=%u pubkeys=%u\n",
				    available, i == count, ret, expected, f.availability_calls, f.pubkey_calls);
		}
		dssh_session_cleanup(sess);
		active = NULL;
		dssh_test_reset_global_config();
		valid = valid && matched;
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

/* This offer-only fixture mirrors the real algorithm lists while supplying
 * the opposite role's extension indicator. It does not run the KEX method. */
static int
availability_offer_tx(uint8_t *buf, size_t len, dssh_session sess, void *opaque)
{
	(void)opaque;
	if (active->used != 0 || len < 5 || buf[4] > len - 5)
		return DSSH_ERROR_INVALID;
	int ret = memory_write(buf, len);
	if (ret != 0)
		return ret;
	struct dssh_name_list lists[10];
	bool follows;
	ret = parse_peer_kexinit(&active->wire[5], len - 5 - buf[4], lists, &follows);
	if (ret != 0)
		return ret;
	const char *ours = sess->trans.client ? ",ext-info-c" : ",ext-info-s";
	size_t suffix = strlen(ours);
	if (lists[0].len < suffix
	    || memcmp(&lists[0].data[lists[0].len - suffix], ours, suffix) != 0)
		return DSSH_ERROR_INVALID;
	/* The parsed bounded view points inside this fixture's writable wire. */
	size_t offset = (size_t)(lists[0].data - active->wire);
	active->wire[offset + lists[0].len - 1] = sess->trans.client ? 's' : 'c';
	return 0;
}

static int
test_key_availability_offer(void)
{
	bool valid = true;
	size_t count = sizeof(availability_results) / sizeof(availability_results[0]);
	for (unsigned mode = 0; mode < 4; mode++) {
		bool client = (mode & 1) != 0;
		for (size_t i = 0; i <= count; i++) {
			dssh_test_reset_global_config();
			int available = i < count ? availability_results[i] : 0;
			struct module_fixture f = { .availability_test = true, .availability = available };
			active = &f;
			dssh_key_algo key = register_auth_module();
			bool matched = key != NULL && dssh_register_curve25519_sha256() == 0
			    && dssh_register_none_enc() == 0 && dssh_register_none_mac() == 0
			    && dssh_register_none_comp() == 0
			    && dssh_transport_set_callbacks(availability_offer_tx, memory_rx, memory_line, NULL) == 0;
			if (key != NULL && i == count)
				key->haskey = NULL;
			dssh_session sess = dssh_session_init(client, 0);
			matched = matched && sess != NULL;
			if (matched && (mode & 2) != 0) {
				const char *name = "contract-key";
				matched = dssh_session_set_key_algo_filter(sess, &name, 1) == 0;
			}
			if (matched) {
				int ret = kexinit(sess);
				bool offered = client || available > 0;
				/* The stream mirrors algorithms and supplies the peer role. */
				int expected = offered ? 0 : (available < 0 ? available : DSSH_ERROR_INIT);
				matched = ret == expected && f.writes == (offered ? 1U : 0U)
				    && f.availability_calls == (client || i == count ? 0U : 1U) && f.pubkey_calls == 0;
				if (matched && offered) {
					struct dssh_name_list lists[10];
					bool follows;
					matched = parse_peer_kexinit(sess->trans.our_kexinit, sess->trans.our_kexinit_sz,
					    lists, &follows) == 0 && lists[1].len == 12
					    && memcmp(lists[1].data, "contract-key", 12) == 0;
				}
				if (!matched)
					fprintf(stderr, "availability offer: client=%d filter=%u value=%d absent=%d result=%d expected=%d calls=%u writes=%u\n",
					    client, mode & 2, available, i == count, ret, expected, f.availability_calls, f.writes);
			}
			dssh_session_cleanup(sess);
			active = NULL;
			dssh_test_reset_global_config();
			valid = valid && matched;
		}
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#endif
