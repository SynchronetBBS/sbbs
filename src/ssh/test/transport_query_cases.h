#ifndef DSSH_TRANSPORT_QUERY_CASES_H
#define DSSH_TRANSPORT_QUERY_CASES_H

static const char *(*const directional_queries[])(dssh_session, enum dssh_direction) = {
	dssh_transport_get_enc_name, dssh_transport_get_mac_name, dssh_transport_get_comp_name
};
static const enum dssh_direction query_directions[] = {
	DSSH_CLIENT_TO_SERVER, DSSH_SERVER_TO_CLIENT
};

static int
register_query_algorithms(void)
{
	int ret = register_all_algorithms();
	if (ret == 0)
		ret = dssh_register_aes128_cbc();
	if (ret == 0)
		ret = dssh_test_register_growth_comp();
	return ret;
}

static void
publish_query_directions(dssh_session sess)
{
	atomic_store(&sess->trans.enc_c2s_selected, gconf.enc_head);
	atomic_store(&sess->trans.enc_s2c_selected, gconf.enc_head->next);
	atomic_store(&sess->trans.mac_c2s_selected, gconf.mac_head);
	atomic_store(&sess->trans.mac_s2c_selected, gconf.mac_head->next);
	atomic_store(&sess->trans.comp_c2s_selected, gconf.comp_head);
	atomic_store(&sess->trans.comp_s2c_selected, gconf.comp_head->next);
}

struct query_io_probe {
	unsigned calls;
	bool valid;
};

static int
query_io(uint8_t *bytes, size_t length, dssh_session sess, void *data)
{
	(void)bytes;
	(void)length;
	struct query_io_probe *probe = data;
	probe->calls++;
	for (size_t method = 0; method < 3; method++) {
		for (size_t direction = 0; direction < 2; direction++)
			probe->valid = probe->valid && directional_queries[method](sess, query_directions[direction]) == NULL;
	}
	return DSSH_ERROR_INIT;
}

static int
test_query_arguments(void)
{
	bool valid = true;
	unsigned calls = 0;
	for (unsigned role = 0; role < 2; role++) {
		dssh_test_reset_global_config();
		int ret = register_query_algorithms();
		if (ret == 0)
			ret = dssh_transport_set_callbacks(query_io, query_io, NULL, NULL);
		dssh_session sess = ret == 0 ? dssh_session_init(role != 0, 0) : NULL;
		if (sess == NULL) {
			dssh_test_reset_global_config();
			return TEST_FAIL;
		}
		const char *names[3][2] = {
			{gconf.enc_head->name, gconf.enc_head->next->name},
			{gconf.mac_head->name, gconf.mac_head->next->name},
			{gconf.comp_head->name, gconf.comp_head->next->name}
		};
		enum dssh_direction invalid[] = {(enum dssh_direction)-1, (enum dssh_direction)2};
		for (size_t method = 0; method < 3; method++) {
			for (size_t direction = 0; direction < 2; direction++) {
				valid = valid && directional_queries[method](NULL, query_directions[direction]) == NULL
				    && directional_queries[method](sess, query_directions[direction]) == NULL
				    && directional_queries[method](sess, invalid[direction]) == NULL;
			}
		}
		publish_query_directions(sess);
		for (size_t method = 0; method < 3; method++) {
			for (size_t direction = 0; direction < 2; direction++)
				valid = valid && directional_queries[method](sess, query_directions[direction]) == names[method][direction]
				    && directional_queries[method](sess, invalid[direction]) == NULL;
		}
		struct query_io_probe probe = {.valid = true};
		ret = dssh_session_set_cbdata(sess, &probe, &probe, NULL, NULL);
		if (ret == 0)
			ret = version_tx(sess);
		valid = valid && ret == DSSH_ERROR_INIT && probe.calls == 1 && probe.valid
		    && dssh_session_is_terminated(sess);
		calls += probe.calls;
		for (size_t method = 0; method < 3; method++) {
			for (size_t direction = 0; direction < 2; direction++)
				valid = valid && directional_queries[method](sess, query_directions[direction]) == names[method][direction];
		}
		dssh_session_cleanup(sess);
		dssh_test_reset_global_config();
	}
	printf("direction_queries roles=2 methods=3 directions=2 io_calls=%u valid=%u\n", calls, valid ? 1U : 0U);
	ASSERT_TRUE(valid && calls == 2);
	return TEST_PASS;
}

struct query_snapshot_probe {
	dssh_session sess;
	dssh_kex kex;
	dssh_key_algo key;
	atomic_bool entered;
	atomic_bool stop;
	_Atomic uint32_t cycles;
};

/* Registry definitions remain alive until the publisher joins. The worker
 * publishes different directions independently; query calls are snapshots. */
static int
query_snapshot_publish(void *opaque)
{
	struct query_snapshot_probe *probe = opaque;
	while (!atomic_load(&probe->stop)) {
		atomic_store(&probe->sess->trans.kex_selected, NULL);
		atomic_store(&probe->sess->trans.key_algo_selected, NULL);
		atomic_store(&probe->sess->trans.enc_c2s_selected, NULL);
		atomic_store(&probe->sess->trans.enc_s2c_selected, NULL);
		atomic_store(&probe->sess->trans.mac_c2s_selected, NULL);
		atomic_store(&probe->sess->trans.mac_s2c_selected, NULL);
		atomic_store(&probe->sess->trans.comp_c2s_selected, NULL);
		atomic_store(&probe->sess->trans.comp_s2c_selected, NULL);
		atomic_store(&probe->sess->trans.kex_selected, probe->kex);
		atomic_store(&probe->sess->trans.key_algo_selected, probe->key);
		publish_query_directions(probe->sess);
		if (atomic_load(&probe->cycles) != UINT32_MAX)
			atomic_fetch_add(&probe->cycles, 1);
		atomic_store(&probe->entered, true);
	}
	return 0;
}

static int
test_query_snapshot(void)
{
	dssh_test_reset_global_config();
	ASSERT_OK(register_query_algorithms());
	ASSERT_OK(dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
	    mock_rxline_dispatch, mock_extra_line_cb));
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);
	struct query_snapshot_probe probe = { .sess = sess, .kex = gconf.kex_head,
	    .key = gconf.key_algo_head };
	atomic_init(&probe.entered, false);
	atomic_init(&probe.stop, false);
	atomic_init(&probe.cycles, 0);
	const char *(*getters[])(dssh_session) = {
	    dssh_transport_get_kex_name, dssh_transport_get_hostkey_name
	};
	const char *names[] = { probe.kex->name, probe.key->name };
	const char *directional_names[3][2] = {
		{gconf.enc_head->name, gconf.enc_head->next->name},
		{gconf.mac_head->name, gconf.mac_head->next->name},
		{gconf.comp_head->name, gconf.comp_head->next->name}
	};
	thrd_t worker;
	int created = thrd_create(&worker, query_snapshot_publish, &probe);
	bool valid = created == thrd_success;
	if (valid)
		valid = dssh_test_wait_bool(&probe.entered, true);
	uint32_t checks = 0;
	for (uint32_t round = 0; round < UINT32_C(100000) && valid; round++) {
		for (size_t i = 0; i < sizeof(getters) / sizeof(getters[0]); i++) {
			const char *name = getters[i](sess);
			checks++;
			if (name != NULL && name != names[i])
				valid = false;
		}
		for (size_t method = 0; method < 3; method++) {
			for (size_t direction = 0; direction < 2; direction++) {
				const char *name = directional_queries[method](sess, query_directions[direction]);
				checks++;
				if (name != NULL && name != directional_names[method][direction])
					valid = false;
			}
		}
	}
	atomic_store(&probe.stop, true);
	if (created == thrd_success && thrd_join(worker, NULL) != thrd_success)
		abort();
	uint32_t cycles = atomic_load(&probe.cycles);
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	ASSERT_EQ_U(checks, UINT32_C(800000));
	ASSERT_TRUE(cycles > 0);
	return TEST_PASS;
}

#endif
