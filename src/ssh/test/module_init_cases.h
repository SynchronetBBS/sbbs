/* Direct key-install ownership checks. Module callbacks identify each provider
 * and direction even when its successful context is deliberately NULL. */
struct init_context {
	unsigned provider;
	unsigned slot;
};

struct init_trace {
	void (*observe_init)(void);
	void (*observe_transition)(unsigned provider, unsigned slot, bool cleanup);
	bool stateless[2];
	bool partial;
	bool client;
	unsigned fail_provider;
	unsigned fail_slot;
	int failure;
	unsigned inits[2][4];
	unsigned cleanups[2][4];
	unsigned live;
	bool wrong_owner;
};

static struct init_trace *init_trace;

static int
trace_initialize(unsigned provider, unsigned slot, void **out)
{
	if (init_trace->observe_init != NULL)
		init_trace->observe_init();
	if (init_trace->observe_transition != NULL)
		init_trace->observe_transition(provider, slot, false);
	init_trace->inits[provider][slot]++;
	bool failed = provider == init_trace->fail_provider && slot == init_trace->fail_slot;
	*out = NULL;
	if ((!failed && !init_trace->stateless[provider]) || (failed && init_trace->partial)) {
		struct init_context *ctx = malloc(sizeof(*ctx));
		if (ctx == NULL)
			return DSSH_ERROR_ALLOC;
		ctx->provider = provider;
		ctx->slot = slot;
		init_trace->live++;
		*out = ctx;
	}
	return failed ? init_trace->failure : 0;
}

static void
trace_cleanup(unsigned provider, unsigned slot, void *opaque)
{
	if (init_trace->observe_transition != NULL)
		init_trace->observe_transition(provider, slot, true);
	init_trace->cleanups[provider][slot]++;
	struct init_context *ctx = opaque;
	if (ctx != NULL) {
		if (ctx->provider != provider || ctx->slot != slot || init_trace->live == 0)
			init_trace->wrong_owner = true;
		else
			init_trace->live--;
		free(ctx);
	}
}

#define TRACE_ENC(name, provider, slot) \
	static int name##_init(const uint8_t *key, const uint8_t *iv, bool encrypt, dssh_enc_ctx **out) \
	{ \
		(void)key; \
		(void)iv; \
		if (encrypt != ((slot) == 0 ? init_trace->client : !init_trace->client)) \
			init_trace->wrong_owner = true; \
		void *ctx = NULL; \
		int ret = trace_initialize((provider), (slot), &ctx); \
		*out = ctx; \
		return ret; \
	} \
	static void name##_cleanup(dssh_enc_ctx *ctx) \
	{ \
		trace_cleanup((provider), (slot), ctx); \
	}

#define TRACE_MAC(name, provider, slot) \
	static int name##_init(const uint8_t *key, dssh_mac_ctx **out) \
	{ \
		(void)key; \
		void *ctx = NULL; \
		int ret = trace_initialize((provider), (slot), &ctx); \
		*out = ctx; \
		return ret; \
	} \
	static void name##_cleanup(dssh_mac_ctx *ctx) \
	{ \
		trace_cleanup((provider), (slot), ctx); \
	}

TRACE_ENC(trace_a_c2s, 0, 0)
TRACE_ENC(trace_a_s2c, 0, 1)
TRACE_MAC(trace_a_mac_c2s, 0, 2)
TRACE_MAC(trace_a_mac_s2c, 0, 3)
TRACE_ENC(trace_b_c2s, 1, 0)
TRACE_ENC(trace_b_s2c, 1, 1)
TRACE_MAC(trace_b_mac_c2s, 1, 2)
TRACE_MAC(trace_b_mac_s2c, 1, 3)
#undef TRACE_ENC
#undef TRACE_MAC

struct init_modules {
	dssh_enc enc[2][2];
	dssh_mac mac[2][2];
};

static void
free_init_modules(struct init_modules *modules)
{
	for (unsigned provider = 0; provider < 2; provider++) {
		for (unsigned slot = 0; slot < 2; slot++) {
			free(modules->enc[provider][slot]);
			free(modules->mac[provider][slot]);
		}
	}
}

static bool
make_init_modules(struct init_modules *modules, bool zero_key)
{
	const dssh_enc_init enc_init_fn[2][2] = {
		{ trace_a_c2s_init, trace_a_s2c_init }, { trace_b_c2s_init, trace_b_s2c_init },
	};
	const dssh_enc_cleanup enc_free_fn[2][2] = {
		{ trace_a_c2s_cleanup, trace_a_s2c_cleanup }, { trace_b_c2s_cleanup, trace_b_s2c_cleanup },
	};
	const dssh_mac_init mac_init_fn[2][2] = {
		{ trace_a_mac_c2s_init, trace_a_mac_s2c_init }, { trace_b_mac_c2s_init, trace_b_mac_s2c_init },
	};
	const dssh_mac_cleanup mac_free_fn[2][2] = {
		{ trace_a_mac_c2s_cleanup, trace_a_mac_s2c_cleanup }, { trace_b_mac_c2s_cleanup, trace_b_mac_s2c_cleanup },
	};
	memset(modules, 0, sizeof(*modules));
	if (sizeof(struct dssh_enc_s) > SIZE_MAX - sizeof("init-trace")
	    || sizeof(struct dssh_mac_s) > SIZE_MAX - sizeof("init-trace"))
		return false;
	for (unsigned provider = 0; provider < 2; provider++) {
		for (unsigned slot = 0; slot < 2; slot++) {
			dssh_enc enc = calloc(1, sizeof(*enc) + sizeof("init-trace"));
			dssh_mac mac = calloc(1, sizeof(*mac) + sizeof("init-trace"));
			modules->enc[provider][slot] = enc;
			modules->mac[provider][slot] = mac;
			if (enc == NULL || mac == NULL)
				return false;
			memcpy(enc->name, "init-trace", sizeof("init-trace"));
			memcpy(mac->name, "init-trace", sizeof("init-trace"));
			enc->blocksize = 16;
			enc->key_size = 1;
			enc->bytes_per_key = UINT64_MAX;
			enc->init = enc_init_fn[provider][slot];
			enc->cleanup = enc_free_fn[provider][slot];
			mac->key_size = zero_key ? 0 : 1;
			mac->digest_size = 16;
			mac->init = mac_init_fn[provider][slot];
			mac->cleanup = mac_free_fn[provider][slot];
		}
	}
	return true;
}

static void
select_init_modules(dssh_session sess, const struct init_modules *modules, unsigned provider)
{
	sess->trans.enc_c2s_selected = modules->enc[provider][0];
	sess->trans.enc_s2c_selected = modules->enc[provider][1];
	sess->trans.mac_c2s_selected = modules->mac[provider][0];
	sess->trans.mac_s2c_selected = modules->mac[provider][1];
}

static bool
check_init_active(dssh_session sess, const struct init_modules *modules, unsigned provider,
    unsigned slot, bool expected)
{
	if (slot == 0)
		return sess->trans.enc_c2s_active == (expected ? modules->enc[provider][0] : NULL);
	if (slot == 1)
		return sess->trans.enc_s2c_active == (expected ? modules->enc[provider][1] : NULL);
	if (slot == 2)
		return sess->trans.mac_c2s_active == (expected ? modules->mac[provider][0] : NULL);
	return sess->trans.mac_s2c_active == (expected ? modules->mac[provider][1] : NULL);
}

static int
install_init_modules(dssh_session sess, const struct init_modules *modules, bool initial)
{
	const uint8_t secret[] = { 0, 0, 0, 1, 1 };
	return derive_and_apply_keys(sess, secret, sizeof(secret),
	    initial ? NULL : modules->enc[0][0], initial ? NULL : modules->enc[0][1],
	    initial ? NULL : modules->mac[0][0], initial ? NULL : modules->mac[0][1]);
}

static bool
init_ownership_case(bool client, bool old_stateless, bool new_stateless,
    bool zero_key, bool rekey, unsigned fail_slot, bool partial, int failure, int allocation)
{
	dssh_test_reset_global_config();
	struct init_trace trace = {
		.stateless = { old_stateless, new_stateless }, .client = client,
		.fail_provider = rekey ? 1 : 0, .fail_slot = fail_slot,
		.partial = partial, .failure = failure,
	};
	init_trace = &trace;
	struct init_modules modules;
	bool valid = make_init_modules(&modules, zero_key);
	dssh_session sess = dssh_session_init(client, 0);
	valid = valid && sess != NULL;
	struct dssh_kex_s method = { .hash_name = "SHA-256" };
	uint8_t hash[32] = {0};
	unsigned provider = rekey ? 1 : 0;
	if (valid) {
		sess->trans.kex_selected = &method;
		sess->trans.kex_result.exchange_hash.data = hash;
		sess->trans.kex_result.exchange_hash.len = sizeof(hash);
		sess->trans.session_id = hash;
		sess->trans.session_id_sz = sizeof(hash);
		select_init_modules(sess, &modules, 0);
		if (rekey) {
			valid = install_init_modules(sess, &modules, true) == 0;
			sess->trans.kex_result.exchange_hash = (struct dssh_bytes){hash, sizeof(hash)};
			select_init_modules(sess, &modules, 1);
		}
		int ret = DSSH_ERROR_INIT;
		if (valid) {
			if (allocation >= 0)
				dssh_test_alloc_fail_after(allocation);
			ret = install_init_modules(sess, &modules, !rekey);
			dssh_test_alloc_reset();
		}
		int expected = allocation >= 0 ? DSSH_ERROR_ALLOC
		    : (fail_slot == 4 ? 0 : (failure > 0 ? DSSH_ERROR_INVALID : failure));
		valid = valid && ret == expected;
		for (unsigned slot = 0; slot < 4; slot++) {
			bool reached = allocation < 0 && (slot <= fail_slot || fail_slot == 4);
			bool initialized = allocation < 0 && (slot < fail_slot || fail_slot == 4
			    || (slot == fail_slot && partial));
			valid = valid && trace.inits[provider][slot] == (reached ? 1U : 0U);
			if (rekey && allocation >= 0)
				valid = valid && check_init_active(sess, &modules, 0, slot, true)
				    && trace.cleanups[0][slot] == 0;
			else {
				valid = valid && check_init_active(sess, &modules, provider, slot, initialized);
				if (rekey)
					valid = valid && trace.cleanups[0][slot] == 1;
			}
		}
	}
	if (sess != NULL) {
		sess->trans.kex_result.exchange_hash.data = NULL;
		sess->trans.session_id = NULL;
		dssh_session_cleanup(sess);
	}
	for (unsigned slot = 0; slot < 4; slot++) {
		unsigned expected = allocation >= 0 ? 0
		    : ((slot < fail_slot || fail_slot == 4 || (slot == fail_slot && partial)) ? 1U : 0U);
		valid = valid && trace.cleanups[provider][slot] == expected;
		if (rekey)
			valid = valid && trace.cleanups[0][slot] == 1;
	}
	valid = valid && trace.live == 0 && !trace.wrong_owner;
	free_init_modules(&modules);
	init_trace = NULL;
	dssh_test_reset_global_config();
	if (!valid)
		fprintf(stderr, "init ownership: client=%d old=%d new=%d zero_key=%d rekey=%d fail=%u partial=%d result=%d alloc=%d\n",
		    client, old_stateless, new_stateless, zero_key, rekey, fail_slot, partial, failure, allocation);
	return valid;
}

static int
test_init_publication(void)
{
	bool valid = true;
	for (unsigned mode = 0; mode < 16; mode++) {
		for (unsigned slot = 0; slot <= 4; slot++) {
			if (slot == 4 && (mode & 8) != 0)
				continue;
			for (unsigned result = 0; result < (slot == 4 ? 1U : 2U); result++) {
				bool matched = init_ownership_case((mode & 1) != 0, (mode & 2) != 0, false,
				    (mode & 4) != 0, false, slot, (mode & 8) != 0,
				    result == 0 ? DSSH_ERROR_INIT : 1, -1);
				valid = valid && matched;
			}
		}
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_init_rekey_ownership(void)
{
	bool valid = true;
	for (unsigned mode = 0; mode < 32; mode++) {
		for (unsigned slot = 0; slot <= 4; slot++) {
			if (slot == 4 && (mode & 16) != 0)
				continue;
			for (unsigned result = 0; result < (slot == 4 ? 1U : 2U); result++) {
				bool matched = init_ownership_case((mode & 1) != 0, (mode & 2) != 0,
				    (mode & 4) != 0, (mode & 8) != 0, true, slot, (mode & 16) != 0,
				    result == 0 ? DSSH_ERROR_INIT : 1, -1);
				valid = valid && matched;
			}
		}
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_init_preparation_failures(void)
{
	bool valid = true;
	const int failures[] = { 0, 6 };
	for (unsigned mode = 0; mode < 8; mode++) {
		for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
			bool matched = init_ownership_case((mode & 1) != 0, (mode & 2) != 0,
			    false, (mode & 4) != 0, true, 4, false, 0, failures[i]);
			valid = valid && matched;
		}
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

struct init_lifecycle_observation {
	dssh_session sess;
	unsigned provider;
	unsigned slot;
	unsigned calls;
	bool cleanup;
	bool teardown;
	bool stop;
	bool valid;
};

static struct init_lifecycle_observation *init_lifecycle;

static void
observe_init_lifecycle(unsigned provider, unsigned slot, bool cleanup)
{
	struct init_lifecycle_observation *state = init_lifecycle;
	if (provider != state->provider || slot != state->slot || cleanup != state->cleanup)
		return;
	state->calls++;
	if (state->calls != 1) {
		state->valid = false;
		return;
	}
	dssh_session sess = state->sess;
	mtx_t *locks[] = { &sess->mtx, &sess->trans.tx_mtx, &sess->trans.tx_queue_mtx };
	for (size_t i = 0; i < sizeof(locks) / sizeof(locks[0]); i++) {
		int ret = mtx_trylock(locks[i]);
		if (ret == thrd_success) {
			if (mtx_unlock(locks[i]) != thrd_success)
				abort();
		}
		else
			state->valid = false;
	}
	unsigned live = init_trace->live;
	unsigned released = init_trace->cleanups[provider][slot];
	state->valid &= tx_owner_is_current(sess) == !state->teardown;
	state->valid &= atomic_load(&sess->lifecycle_busy) == state->teardown;
	if (state->stop)
		dssh_session_stop(sess);
	else
		dssh_session_cleanup(sess);
	/* The outer owner still owns every context. A lifecycle call may request
	 * termination here, but cannot recursively release or free the session. */
	state->valid &= dssh_session_is_terminated(sess)
	    && init_trace->live == live && init_trace->cleanups[provider][slot] == released
	    && tx_owner_is_current(sess) == !state->teardown
	    && atomic_load(&sess->lifecycle_busy) == state->teardown;
}

static bool
init_lifecycle_case(bool client, bool old_stateless, bool new_stateless,
    bool stop, unsigned phase, unsigned slot)
{
	dssh_test_reset_global_config();
	struct init_trace trace = {
		.stateless = { old_stateless, new_stateless }, .client = client, .fail_slot = 4,
	};
	init_trace = &trace;
	struct init_modules modules;
	bool valid = make_init_modules(&modules, false);
	dssh_session sess = dssh_session_init(client, 0);
	valid &= sess != NULL;
	struct dssh_kex_s method = { .hash_name = "SHA-256" };
	uint8_t hash[32] = {0};
	struct init_lifecycle_observation observation = {
		.sess = sess, .provider = phase == 1 ? 1U : 0U, .slot = slot,
		.cleanup = phase >= 2, .teardown = phase == 3, .stop = stop, .valid = true,
	};
	init_lifecycle = &observation;
	if (valid) {
		sess->trans.kex_selected = &method;
		sess->trans.session_id = hash;
		sess->trans.session_id_sz = sizeof(hash);
		sess->trans.kex_result.exchange_hash = (struct dssh_bytes){hash, sizeof(hash)};
		select_init_modules(sess, &modules, 0);
		if (phase != 0)
			valid = install_init_modules(sess, &modules, true) == 0;
		if (valid) {
			trace.observe_transition = observe_init_lifecycle;
			if (phase != 3) {
				sess->trans.kex_result.exchange_hash = (struct dssh_bytes){hash, sizeof(hash)};
				if (phase != 0)
					select_init_modules(sess, &modules, 1);
				/* NEWKEYS owns the engine, with no physical TX mutex held.
				 * Exercise that same ownership around the private installer. */
				int ret = tx_acquire(sess, SSH_MSG_NEWKEYS);
				valid = ret == 0;
				if (ret == 0) {
					ret = install_init_modules(sess, &modules, phase == 0);
					valid &= ret == 0 && sess->terminate;
					tx_unlock_with_slots(sess);
					valid &= !tx_owner_is_current(sess)
					    && !atomic_load(&sess->trans.tx_owner_active);
				}
			}
		}
	}
	if (sess != NULL) {
		sess->trans.kex_result.exchange_hash = (struct dssh_bytes){0};
		sess->trans.session_id = NULL;
		dssh_session_cleanup(sess);
	}
	valid &= observation.valid && observation.calls == 1 && trace.live == 0 && !trace.wrong_owner;
	for (unsigned provider = 0; provider < 2; provider++) {
		for (unsigned direction = 0; direction < 4; direction++)
			valid &= trace.inits[provider][direction] == trace.cleanups[provider][direction];
	}
	free_init_modules(&modules);
	init_lifecycle = NULL;
	init_trace = NULL;
	dssh_test_reset_global_config();
	if (!valid)
		fprintf(stderr, "init lifecycle: client=%d old=%d new=%d stop=%d phase=%u slot=%u calls=%u\n",
		    client, old_stateless, new_stateless, stop, phase, slot, observation.calls);
	return valid;
}

static int
test_init_lifecycle(void)
{
	unsigned failures = 0;
	unsigned cases = 0;
	for (unsigned mode = 0; mode < 16; mode++) {
		for (unsigned phase = 0; phase < 4; phase++) {
			if ((phase == 0 || phase == 3) && (mode & 4) != 0)
				continue;
			for (unsigned slot = 0; slot < 4; slot++) {
				bool valid = init_lifecycle_case((mode & 1) != 0, (mode & 2) != 0,
				    (mode & 4) != 0, (mode & 8) != 0, phase, slot);
				if (!valid)
					failures++;
				cases++;
			}
		}
	}
	if (printf("init_lifecycle cases=%u failures=%u\n", cases, failures) < 0 || fflush(stdout) != 0)
		return TEST_FAIL;
	ASSERT_EQ_U(cases, 192);
	ASSERT_EQ_U(failures, 0);
	return TEST_PASS;
}

static bool
mac_bytes_equal(const uint8_t *buf, size_t len, uint8_t value)
{
	for (size_t i = 0; i < len; i++) {
		if (buf[i] != value)
			return false;
	}
	return true;
}

static int
test_mac_buffer_reuse(void)
{
	const uint16_t sizes[] = { 16, 16, 0, 8, 32, 16, 0, 64, 8 };
	for (unsigned watch = 0; watch < 2; watch++) {
		dssh_test_reset_global_config();
		dssh_session sess = dssh_session_init(true, 0);
		ASSERT_NOT_NULL(sess);
		bool valid = true;
		for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
			size_t capacity = sess->trans.rx_mac_capacity;
			uint8_t *watched = watch == 0 ? sess->trans.rx_mac_buf : sess->trans.rx_mac_computed;
			if (capacity > 0) {
				memset(sess->trans.rx_mac_buf, 0x11, capacity);
				memset(sess->trans.rx_mac_computed, 0x22, capacity);
			}
			dssh_test_alloc_watch_free(watched, capacity);
			bool reuse = sizes[i] <= capacity;
			if (reuse)
				dssh_test_alloc_fail_after(0);
			int ret = reserve_mac_buffers(sess, sizes[i]);
			int allocations = dssh_test_alloc_count();
			bool cleared = false;
			bool freed = dssh_test_alloc_watched_free(&cleared);
			bool matched = ret == 0;
			if (reuse) {
				matched = matched && allocations == 0 && !freed
				    && sess->trans.rx_mac_capacity == capacity;
				if (matched) {
					uint8_t *current = watch == 0 ? sess->trans.rx_mac_buf : sess->trans.rx_mac_computed;
					matched = current == watched;
				}
			}
			else
				matched = matched && sess->trans.rx_mac_capacity == sizes[i]
				    && (capacity == 0 || (freed && cleared));
			dssh_test_alloc_reset();
			if (matched)
				matched = mac_bytes_equal(sess->trans.rx_mac_buf, sess->trans.rx_mac_capacity, 0)
				    && mac_bytes_equal(sess->trans.rx_mac_computed, sess->trans.rx_mac_capacity, 0);
			valid = valid && matched;
			if (!matched)
				break;
		}
		if (valid) {
			memset(sess->trans.rx_mac_buf, 0x33, sess->trans.rx_mac_capacity);
			memset(sess->trans.rx_mac_computed, 0x44, sess->trans.rx_mac_capacity);
			dssh_test_alloc_watch_free(watch == 0 ? sess->trans.rx_mac_buf : sess->trans.rx_mac_computed,
			    sess->trans.rx_mac_capacity);
		}
		dssh_session_cleanup(sess);
		bool cleared = false;
		valid = dssh_test_alloc_watched_free(&cleared) && cleared && valid;
		dssh_test_alloc_reset();
		dssh_test_reset_global_config();
		ASSERT_TRUE(valid);
	}

	for (unsigned watch = 0; watch < 2; watch++) {
		for (int failure = 0; failure < 2; failure++) {
			dssh_test_reset_global_config();
			dssh_session sess = dssh_session_init(true, 0);
			ASSERT_NOT_NULL(sess);
			int ret = reserve_mac_buffers(sess, 16);
			bool valid = ret == 0;
			if (valid) {
				uint8_t *watched = watch == 0 ? sess->trans.rx_mac_buf : sess->trans.rx_mac_computed;
				memset(sess->trans.rx_mac_buf, 0x11, 16);
				memset(sess->trans.rx_mac_computed, 0x22, 16);
				dssh_test_alloc_watch_free(watched, 16);
				dssh_test_alloc_fail_after(failure);
				ret = reserve_mac_buffers(sess, 32);
				bool freed = dssh_test_alloc_watched_free(NULL);
				valid = ret == DSSH_ERROR_ALLOC && !freed && sess->trans.rx_mac_capacity == 16;
				if (valid) {
					uint8_t *current = watch == 0 ? sess->trans.rx_mac_buf : sess->trans.rx_mac_computed;
					uint8_t value = watch == 0 ? 0x11 : 0x22;
					valid = current == watched && mac_bytes_equal(current, 16, value);
				}
				dssh_test_alloc_reset();
			}
			dssh_session_cleanup(sess);
			dssh_test_reset_global_config();
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}

static int
test_handler_digest_failure(void)
{
#ifdef DSSH_CRYPTO_OPENSSL
	const unsigned modes = 4;
#else
	const unsigned modes = 2;
#endif
	for (unsigned watch = 0; watch < 2; watch++) {
		for (unsigned mode = 0; mode < modes; mode++) {
			dssh_test_reset_global_config();
			struct module_fixture fixture = { .watch = watch };
			active = &fixture;
			dssh_session sess = dssh_session_init(true, 0);
			ASSERT_NOT_NULL(sess);
			const char *name = mode == 0 ? NULL : (mode == 1 ? "DeuceSSH-Unsupported-Audit-Hash" : "SHA-256");
			struct dssh_kex_s method = { .init = test_kex_stateless_init, .handler = handler,
			    .cleanup = test_kex_stateless_cleanup, .hash_name = name };
			sess->trans.kex_selected = &method;
			if (mode == 2)
				dssh_test_alloc_fail_after(0);
			if (mode == 3)
				dssh_test_ossl_fail_after(0);
			int ret = kex(sess);
			int expected = mode == 0 ? DSSH_ERROR_INVALID : (mode == 2 ? DSSH_ERROR_ALLOC : DSSH_ERROR_INIT);
			bool valid = ret == expected && sess->trans.kex_result.shared_secret.data == NULL && sess->trans.kex_result.exchange_hash.data == NULL;
			if (mode == 2)
				valid = valid && dssh_test_alloc_count() == 1;
			if (mode == 3)
				valid = valid && dssh_test_ossl_count() == 1;
			dssh_session_cleanup(sess);
			valid = valid && fixture.kex_allocations == fixture.kex_frees && fixture.kex_bad_release == 0
			    && fixture.kex_lease == NULL;
			dssh_test_alloc_reset();
			dssh_test_ossl_reset();
			active = NULL;
			dssh_test_reset_global_config();
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}
