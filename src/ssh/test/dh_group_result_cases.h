#ifndef DSSH_TEST_DH_GROUP_RESULT_CASES_H
#define DSSH_TEST_DH_GROUP_RESULT_CASES_H

#include "dh_group_result_provider.h"

struct dh_group_release_probe {
	dssh_session session;
	unsigned calls;
	bool valid;
};

static void
observe_dh_group_release(void *data)
{
	struct dh_group_release_probe *probe = data;
	dssh_session sess = probe->session != NULL ? probe->session : compression_callback_session;
	probe->calls++;
	if (sess == NULL) {
		probe->valid = false;
		return;
	}
	probe->valid = probe->valid && dssh_auth_set_banner(sess, "forbidden", NULL) == DSSH_ERROR_INVALID;
	int ret = mtx_trylock(&sess->trans.tx_mtx);
	if (ret == thrd_success) {
		if (mtx_unlock(&sess->trans.tx_mtx) != thrd_success)
			probe->valid = false;
	}
	else
		probe->valid = false;
	ret = mtx_trylock(&sess->trans.tx_queue_mtx);
	if (ret == thrd_success) {
		if (mtx_unlock(&sess->trans.tx_queue_mtx) != thrd_success)
			probe->valid = false;
	}
	else
		probe->valid = false;
}

static bool
dh_group_storage_zero(const struct test_dh_group_provider *provider)
{
	for (size_t i = 0; i < sizeof(provider->slots) / sizeof(provider->slots[0]); i++) {
		const struct test_dh_group_slot *slot = &provider->slots[i];
		for (size_t j = 0; j < sizeof(slot->p); j++) {
			if (slot->p[j] != 0)
				return false;
		}
		for (size_t j = 0; j < sizeof(slot->g); j++) {
			if (slot->g[j] != 0)
				return false;
		}
	}
	return true;
}

static int
test_dhgex_provider_contract(void)
{
	const int expected[] = {0, -77, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID,
	    DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID,
	    DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, DSSH_ERROR_INVALID,
	    DSSH_ERROR_INVALID, DSSH_ERROR_INVALID, 0, -77};
	for (unsigned mode = 0; mode < sizeof(expected) / sizeof(expected[0]); mode++) {
		struct dhgex_server_ctx ctx;
		if (dhgex_server_setup(&ctx) < 0)
			return TEST_FAIL;
		struct test_dh_group_provider state;
		test_dh_group_init(&state, rfc3526_grp14_p, 256);
		state.mode = mode;
		struct dh_group_release_probe probe = {.session = ctx.server, .valid = true};
		state.observe_release = observe_dh_group_release;
		state.observe_data = &probe;
		struct dssh_dh_gex_provider provider = {test_dh_group_select, &state};
		void *original = ctx.server->trans.kex_selected->config;
		ctx.server->trans.kex_selected->config = &provider;
		uint8_t request[] = {34, 0, 0, 8, 0, 0, 0, 16, 0, 0, 0, 32, 0};
		uint8_t init[] = {32, 0, 0, 0, 1, 2};
		uint8_t wire[64];
		size_t length = 0;
		ASSERT_EQ(build_plaintext_packet_t(request, sizeof(request), wire, sizeof(wire), &length), 0);
		ASSERT_EQ(build_plaintext_packet_t(init, sizeof(init), wire, sizeof(wire), &length), 0);
		unsigned static_before = test_dh_group_static_releases();
		unsigned rounds = mode == 0 ? 2 : 1;
		bool valid = true;
		for (unsigned round = 0; round < rounds; round++) {
			int ret = DSSH_ERROR_INIT;
			ASSERT_EQ(dhgex_server_run(&ctx, wire, length, &ret), 0);
			bool owned = mode != 9 && mode != 10 && mode != 14;
			unsigned releases = owned ? round + 1 : 0;
			valid = valid && ret == expected[mode] && atomic_load(&state.valid)
			    && atomic_load(&state.calls) == round + 1
			    && atomic_load(&state.releases) == releases && probe.calls == releases && probe.valid;
			if (owned)
				valid = valid && dh_group_storage_zero(&state);
			if (mode == 9 || mode == 10)
				valid = valid && state.slots[0].p[0] == rfc3526_grp14_p[0] && state.slots[0].g[0] == 2;
			if (mode == 14)
				valid = valid && test_dh_group_static_releases() == static_before + 1;
		}
		if (mode == 0) {
			/* Force a valid larger group so retained group storage must grow.
			 * A fully warmed same-size method no longer has to allocate. */
			state.prime = rfc3526_grp15_p;
			state.prime_len = 384;
			group_test_alloc_failure = 0;
			int ret = DSSH_ERROR_INIT;
			ASSERT_EQ(dhgex_server_run(&ctx, wire, length, &ret), 0);
			group_test_alloc_failure = -1;
			state.prime = rfc3526_grp14_p;
			state.prime_len = 256;
			valid = valid && ret == DSSH_ERROR_ALLOC
			    && atomic_load(&state.calls) == 3 && atomic_load(&state.releases) == 3
			    && probe.calls == 3 && probe.valid && dh_group_storage_zero(&state);
			struct control_tx_failure failure = {.sess = ctx.server};
			control_tx_failure = &failure;
			ASSERT_EQ(dhgex_server_run(&ctx, wire, length, &ret), 0);
			control_tx_failure = NULL;
			valid = valid && ret < 0 && failure.calls == 1
			    && atomic_load(&state.calls) == 4 && atomic_load(&state.releases) == 4
			    && probe.calls == 4 && probe.valid && dh_group_storage_zero(&state);
		}
		ctx.server->trans.kex_selected->config = original;
		test_dh_group_dispose(&state);
		dhgex_server_teardown(&ctx);
		if (!valid)
			fprintf(stderr, "DH group producer mode %u failed\n", mode);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static struct dssh_dh_gex_provider *configured_dh_group_provider;

static int
register_dh_group_provider(void)
{
	if (!test_using_dhgex()) {
		int ret = dssh_register_dh_gex_sha256();
		if (ret < 0)
			return ret;
	}
	return dssh_kex_set_ctx("diffie-hellman-group-exchange-sha256", configured_dh_group_provider);
}

static void
observe_dh_group_tx(dssh_session sess)
{
	if (!sess->trans.client && configured_dh_group_provider != NULL) {
		struct test_dh_group_provider *state = configured_dh_group_provider->cbdata;
		if (atomic_load(&state->calls) != atomic_load(&state->releases) || !dh_group_storage_zero(state))
			atomic_store(&state->valid, false);
	}
}

static int
test_dhgex_group_result_handshake(void)
{
	struct test_dh_group_provider state;
	test_dh_group_init(&state, rfc3526_grp14_p, 256);
	struct dh_group_release_probe probe = {.valid = true};
	state.observe_release = observe_dh_group_release;
	state.observe_data = &probe;
	struct dssh_dh_gex_provider provider = {test_dh_group_select, &state};
	bool valid = true;
	configured_dh_group_provider = &provider;
	extra_algorithm_registration = register_dh_group_provider;
	handshake_kex_filter = "diffie-hellman-group-exchange-sha256";
	dh_group_tx_observer = observe_dh_group_tx;
	for (unsigned round = 0; round < 2; round++) {
		struct handshake_ctx ctx;
		int ret = handshake_setup(&ctx);
		valid = valid && ret == 0 && atomic_load(&state.valid)
		    && atomic_load(&state.calls) == round + 1 && atomic_load(&state.releases) == round + 1
		    && probe.calls == round + 1 && probe.valid && dh_group_storage_zero(&state);
		if (ret == 0) {
			valid = valid && ctx.client->trans.session_id_sz == 32
			    && ctx.server->trans.session_id_sz == 32
			    && memcmp(ctx.client->trans.session_id, ctx.server->trans.session_id, 32) == 0
			    && dssh_auth_set_banner(ctx.server, "allowed", NULL) == 0;
			uint8_t marker = SSH_MSG_USERAUTH_SUCCESS;
			valid = valid && send_packet(ctx.client, &marker, 1, NULL) == 0;
			if (valid) {
				uint8_t type, *payload;
				size_t length;
				valid = recv_packet(ctx.server, &type, &payload, &length) == 0
				    && type == marker && length == 1;
			}
		}
		handshake_cleanup(&ctx);
		if (!valid)
			break;
	}
	dh_group_tx_observer = NULL;
	handshake_kex_filter = NULL;
	extra_algorithm_registration = NULL;
	configured_dh_group_provider = NULL;
	test_dh_group_dispose(&state);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

struct dh_group_worker {
	struct test_dh_group_provider *provider;
	struct dssh_dh_gex_group_result result;
	int status;
};

static int
produce_dh_group(void *data)
{
	struct dh_group_worker *worker = data;
	worker->status = test_dh_group_select(2048, 4096, 8192, &worker->result, worker->provider);
	return 0;
}

static int
test_dhgex_group_result_reuse(void)
{
	struct test_dh_group_provider state;
	test_dh_group_init(&state, rfc3526_grp14_p, 256);
	bool valid = true;
	for (unsigned round = 0; round < 3; round++) {
		struct dh_group_worker workers[2] = {{.provider = &state}, {.provider = &state}};
		thrd_t threads[2];
		unsigned started = 0;
		dssh_test_alloc_fail_after(0);
		for (unsigned i = 0; i < 2; i++) {
			if (thrd_create(&threads[i], produce_dh_group, &workers[i]) != thrd_success) {
				valid = false;
				break;
			}
			started++;
		}
		for (unsigned i = 0; i < started; i++) {
			if (thrd_join(threads[i], NULL) != thrd_success)
				abort();
		}
		int producer_allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		valid = valid && producer_allocations == 0 && started == 2 && workers[0].status == 0 && workers[1].status == 0
		    && workers[0].result.p.data != workers[1].result.p.data;
		for (unsigned i = 0; i < started; i++) {
			struct dssh_dh_gex_group_result *group = &workers[i].result;
			valid = valid && group->p.len == 256 && group->g.len == 1
			    && group->p.data != NULL && group->g.data != NULL;
			if (valid)
				valid = memcmp(group->p.data, rfc3526_grp14_p, 256) == 0 && group->g.data[0] == 2;
		}
		struct dssh_dh_gex_group_result extra = {0};
		dssh_test_alloc_fail_after(0);
		int ret = test_dh_group_select(2048, 4096, 8192, &extra, &state);
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		valid = valid && ret == DSSH_ERROR_ALLOC && allocations == 0
		    && extra.p.data == NULL && extra.g.data == NULL && extra.lease.release == NULL;
		/* Also release an unexpected success if worker creation failed. */
		dssh_result_release(&extra.lease);
		for (unsigned i = 0; i < started; i++) {
			workers[i].result.p.len = SIZE_MAX;
			workers[i].result.g.len = SIZE_MAX;
			dssh_result_release(&workers[i].result.lease);
			dssh_result_release(&workers[i].result.lease);
		}
		valid = valid && dh_group_storage_zero(&state) && atomic_load(&state.valid);
		if (!valid)
			break;
	}
	valid = valid && atomic_load(&state.releases) == 6;
	test_dh_group_dispose(&state);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#endif
