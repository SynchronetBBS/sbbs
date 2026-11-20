/* White-box transport lifecycle checks. Synthetic K/H values establish only
 * ownership and callback ordering; real handshakes establish KEX mathematics. */
#ifndef DSSH_TEST_KEX_STATE_CASES_H
#define DSSH_TEST_KEX_STATE_CASES_H

struct kex_trace_state {
	unsigned provider;
	unsigned generation;
};

struct kex_trace_provider {
	unsigned inits;
	unsigned handlers;
	unsigned releases;
	unsigned cleanups;
	unsigned result_shape; /* 1: omitted lease, 2: owner only, 3: NULL owner. */
	int handler_result;
	bool stateless;
	bool replace;
	int init_result;
	unsigned failure_state; /* 0: preserve, 1: publish, 2: release/clear. */
	struct kex_trace_state states[2];
	uint8_t secret[64];
	uint8_t hash[64];
	struct dssh_result_lease *lease;
};

struct kex_trace {
	dssh_session session;
	struct kex_trace_provider providers[2];
	bool client;
	bool valid;
};

static struct kex_trace *kex_trace;

static bool
kex_trace_zero(const uint8_t *data, size_t size)
{
	for (size_t i = 0; i < size; i++) {
		if (data[i] != 0)
			return false;
	}
	return true;
}

static void
kex_trace_callback(bool restricted)
{
	dssh_session sess = kex_trace->session;
	if (restricted && dssh_transport_handshake(sess) != DSSH_ERROR_INVALID)
		kex_trace->valid = false;
	mtx_t *mutexes[] = {&sess->trans.tx_mtx, &sess->trans.tx_queue_mtx};
	for (size_t i = 0; i < sizeof(mutexes) / sizeof(mutexes[0]); i++) {
		int ret = mtx_trylock(mutexes[i]);
		if (ret == thrd_success) {
			if (mtx_unlock(mutexes[i]) != thrd_success)
				abort();
		}
		else
			kex_trace->valid = false;
	}
}

static int
kex_trace_init(unsigned provider, bool client, void *config, void **state)
{
	struct kex_trace_provider *p = &kex_trace->providers[provider];
	kex_trace_callback(true);
	kex_trace->valid = kex_trace->valid && config == p && client == kex_trace->client
	    && kex_trace_zero(p->secret, sizeof(p->secret))
	    && kex_trace_zero(p->hash, sizeof(p->hash));
	if (p->inits == 0 || p->stateless)
		kex_trace->valid = kex_trace->valid && *state == NULL;
	else {
		struct kex_trace_state *old = *state;
		kex_trace->valid = kex_trace->valid && old != NULL && old->provider == provider
		    && old->generation == p->inits;
	}
	if (provider == 1 && p->inits == 0)
		kex_trace->valid = kex_trace->valid && kex_trace->providers[0].cleanups == 1;
	p->inits++;
	if (p->init_result != 0 && p->failure_state == 2) {
		if (*state != NULL) {
			struct kex_trace_state *old = *state;
			*old = (struct kex_trace_state){0};
		}
		*state = NULL;
	}
	else if ((!p->stateless && p->init_result == 0)
	    || (p->init_result != 0 && p->failure_state == 1)) {
		unsigned slot = p->replace && p->inits > 1 ? 1U : 0U;
		struct kex_trace_state *next = &p->states[slot];
		if (*state != NULL && *state != next) {
			struct kex_trace_state *old = *state;
			*old = (struct kex_trace_state){0};
		}
		*next = (struct kex_trace_state){provider, p->inits};
		*state = next;
	}
	return p->init_result;
}

static void
kex_trace_release(void *owner)
{
	struct kex_trace_provider *p = owner;
	kex_trace_callback(true);
	kex_trace->valid = kex_trace->valid && p->lease != NULL
	    && p->lease->release == NULL && p->lease->owner == NULL
	    && kex_trace->session->trans.kex_result.shared_secret.data == NULL
	    && kex_trace->session->trans.kex_result.shared_secret.len == 0
	    && kex_trace->session->trans.kex_result.exchange_hash.data == NULL
	    && kex_trace->session->trans.kex_result.exchange_hash.len == 0;
	p->releases++;
	dssh_cleanse(p->secret, sizeof(p->secret));
	dssh_cleanse(p->hash, sizeof(p->hash));
	dssh_result_release(p->lease);
}

static void
kex_trace_null_release(void *owner)
{
	if (owner != NULL)
		kex_trace->valid = false;
	kex_trace_release(&kex_trace->providers[0]);
}

static int
kex_trace_handler(unsigned provider, const struct dssh_kex_context *ctx,
    struct dssh_kex_result *out)
{
	struct kex_trace_provider *p = &kex_trace->providers[provider];
	kex_trace_callback(false);
	kex_trace->valid = kex_trace->valid && ctx->config == p
	    && ctx->state == kex_trace->session->trans.kex_state
	    && ctx->client == kex_trace->client && out->lease.release == NULL
	    && out->lease.owner == NULL && out->shared_secret.data == NULL
	    && out->shared_secret.len == 0 && out->exchange_hash.data == NULL
	    && out->exchange_hash.len == 0;
	if (p->stateless)
		kex_trace->valid = kex_trace->valid && ctx->state == NULL;
	else {
		struct kex_trace_state *state = ctx->state;
		kex_trace->valid = kex_trace->valid && state != NULL
		    && state->provider == provider && state->generation == p->inits;
	}
	p->handlers++;
	memset(p->secret, 0x31, sizeof(p->secret));
	memset(p->hash, 0x32, sizeof(p->hash));
	out->shared_secret = (struct dssh_bytes){p->secret, 32};
	out->exchange_hash = (struct dssh_bytes){p->hash, 32};
	out->lease = (struct dssh_result_lease){kex_trace_release, p};
	if (p->result_shape == 1)
		out->lease = (struct dssh_result_lease){0};
	else if (p->result_shape == 2)
		out->lease = (struct dssh_result_lease){.owner = p};
	else if (p->result_shape == 3)
		out->lease = (struct dssh_result_lease){.release = kex_trace_null_release};
	p->lease = &out->lease;
	return p->handler_result;
}

static void
kex_trace_cleanup(unsigned provider, void *state)
{
	struct kex_trace_provider *p = &kex_trace->providers[provider];
	kex_trace_callback(true);
	bool unleased = p->result_shape == 1 || p->result_shape == 2;
	if (unleased && p->handlers != 0) {
		/* No release function means ownership was never granted. The
		 * producer must reclaim its own storage, without consumer writes. */
		for (size_t i = 0; i < sizeof(p->secret); i++)
			kex_trace->valid = kex_trace->valid && p->secret[i] == 0x31;
		for (size_t i = 0; i < sizeof(p->hash); i++)
			kex_trace->valid = kex_trace->valid && p->hash[i] == 0x32;
		dssh_cleanse(p->secret, sizeof(p->secret));
		dssh_cleanse(p->hash, sizeof(p->hash));
	}
	kex_trace->valid = kex_trace->valid && p->releases == (unleased ? 0 : p->handlers)
	    && kex_trace_zero(p->secret, sizeof(p->secret))
	    && kex_trace_zero(p->hash, sizeof(p->hash))
	    && kex_trace->session->trans.kex_active == NULL
	    && kex_trace->session->trans.kex_state == NULL;
	p->cleanups++;
	if (state != NULL) {
		struct kex_trace_state *owned = state;
		kex_trace->valid = kex_trace->valid && owned->provider == provider;
		*owned = (struct kex_trace_state){0};
	}
}

#define KEX_TRACE_CALLBACKS(name, provider) \
	static int name##_init(bool client, void *config, void **state) \
	{ \
		return kex_trace_init((provider), client, config, state); \
	} \
	static int name##_handler(const struct dssh_kex_context *ctx, struct dssh_kex_result *out) \
	{ \
		return kex_trace_handler((provider), ctx, out); \
	} \
	static void name##_cleanup(void *state) \
	{ \
		kex_trace_cleanup((provider), state); \
	}

KEX_TRACE_CALLBACKS(kex_a, 0)
KEX_TRACE_CALLBACKS(kex_b, 1)
#undef KEX_TRACE_CALLBACKS

static void
kex_trace_methods(struct dssh_kex_s *a, struct dssh_kex_s *b)
{
	*a = (struct dssh_kex_s){.init = kex_a_init, .handler = kex_a_handler,
	    .cleanup = kex_a_cleanup, .config = &kex_trace->providers[0], .hash_name = "SHA-256"};
	*b = (struct dssh_kex_s){.init = kex_b_init, .handler = kex_b_handler,
	    .cleanup = kex_b_cleanup, .config = &kex_trace->providers[1], .hash_name = "SHA-256"};
}

static int
test_kex_state_ownership(void)
{
	/* Two roles, both stateless/stateful providers, same-owner reset (with
	 * replacement), replacement, and selection that never initializes. */
	for (unsigned mode = 0; mode < 16; mode++) {
		for (unsigned action = 0; action < 6; action++) {
			dssh_test_reset_global_config();
			struct kex_trace trace = {.client = (mode & 1) != 0, .valid = true};
			trace.providers[0].stateless = (mode & 2) != 0;
			trace.providers[1].stateless = (mode & 4) != 0;
			trace.providers[0].replace = (mode & 8) != 0;
			kex_trace = &trace;
			struct dssh_kex_s a, b;
			kex_trace_methods(&a, &b);
			dssh_session sess = dssh_session_init(trace.client, 0);
			trace.session = sess;
			bool valid = sess != NULL;
			if (valid) {
				sess->trans.kex_selected = &a;
				valid = kex(sess) == 0 && sess->trans.kex_active == &a;
				void *original = sess->trans.kex_state;
				if (action == 0) {
					int ret = kex(sess);
					bool changed = trace.providers[0].replace && !trace.providers[0].stateless;
					valid = valid && ret == 0 && sess->trans.kex_active == &a
					    && (changed ? sess->trans.kex_state != original : sess->trans.kex_state == original)
					    && trace.providers[0].releases == 1 && trace.providers[0].cleanups == 0;
				}
				else {
					sess->trans.kex_selected = &b;
					if (action == 1)
						valid = valid && kex(sess) == 0 && sess->trans.kex_active == &b;
					else if (action == 3) {
						b.init = NULL;
						valid = valid && kex(sess) == DSSH_ERROR_INIT
						    && sess->trans.kex_active == NULL && sess->trans.kex_state == NULL;
					}
					else if (action >= 4) {
						trace.providers[1].init_result = action == 4 ? -77 : 1;
						trace.providers[1].failure_state = action == 4 ? 0U : 1U;
						valid = valid && kex(sess) == (action == 4 ? -77 : DSSH_ERROR_INVALID)
						    && sess->trans.kex_active == NULL && sess->trans.kex_state == NULL;
					}
				}
			}
			dssh_session_cleanup(sess);
			unsigned first = action == 0 ? 2U : 1U;
			unsigned second = action == 1 || action >= 4 ? 1U : 0U;
			unsigned second_handlers = action == 1 ? 1U : 0U;
			unsigned second_cleanups = action == 1 || action == 5 ? 1U : 0U;
			valid = valid && trace.valid && trace.providers[0].inits == first
			    && trace.providers[0].handlers == first && trace.providers[0].releases == first
			    && trace.providers[0].cleanups == 1 && trace.providers[1].inits == second
			    && trace.providers[1].handlers == second_handlers
			    && trace.providers[1].releases == second_handlers
			    && trace.providers[1].cleanups == second_cleanups;
			kex_trace = NULL;
			dssh_test_reset_global_config();
			if (!valid)
				fprintf(stderr, "KEX owner mode=%u action=%u\n", mode, action);
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}

static int
test_kex_state_init_failure(void)
{
	const int failures[] = {-77, 1};
	for (unsigned mode = 0; mode < 8; mode++) {
		for (unsigned failure_state = 0; failure_state < 3; failure_state++) {
			for (size_t f = 0; f < sizeof(failures) / sizeof(failures[0]); f++) {
				dssh_test_reset_global_config();
				bool warm = (mode & 2) != 0;
				struct kex_trace trace = {.client = (mode & 1) != 0, .valid = true};
				trace.providers[0].stateless = (mode & 4) != 0;
				kex_trace = &trace;
				struct dssh_kex_s a, b;
				kex_trace_methods(&a, &b);
				dssh_session sess = dssh_session_init(trace.client, 0);
				trace.session = sess;
				bool valid = sess != NULL;
				if (valid) {
					sess->trans.kex_selected = &a;
					if (warm)
						valid = kex(sess) == 0;
					trace.providers[0].init_result = failures[f];
					trace.providers[0].failure_state = failure_state;
					int ret = kex(sess);
					valid = valid && ret == (failures[f] > 0 ? DSSH_ERROR_INVALID : failures[f])
					    && sess->trans.kex_active == NULL && sess->trans.kex_state == NULL
					    && sess->trans.kex_result.lease.release == NULL;
				}
				dssh_session_cleanup(sess);
				unsigned handlers = warm ? 1U : 0U;
				unsigned cleanups = warm || failure_state == 1 ? 1U : 0U;
				valid = valid && trace.valid && trace.providers[0].inits == (warm ? 2U : 1U)
				    && trace.providers[0].handlers == handlers && trace.providers[0].releases == handlers
				    && trace.providers[0].cleanups == cleanups;
				kex_trace = NULL;
				dssh_test_reset_global_config();
				if (!valid)
					fprintf(stderr, "KEX failure mode=%u state=%u result=%d\n", mode, failure_state, failures[f]);
				ASSERT_TRUE(valid);
			}
		}
	}
	return TEST_PASS;
}

static int
test_kex_state_result_lease(void)
{
	const int results[] = {0, -77, 1};
	for (unsigned mode = 0; mode < 4; mode++) {
		for (unsigned shape = 0; shape < 4; shape++) {
			for (size_t r = 0; r < sizeof(results) / sizeof(results[0]); r++) {
				dssh_test_reset_global_config();
				struct kex_trace trace = {.client = (mode & 1) != 0, .valid = true};
				trace.providers[0].stateless = (mode & 2) != 0;
				trace.providers[0].result_shape = shape;
				trace.providers[0].handler_result = results[r];
				kex_trace = &trace;
				struct dssh_kex_s a, b;
				kex_trace_methods(&a, &b);
				dssh_session sess = dssh_session_init(trace.client, 0);
				trace.session = sess;
				bool valid = sess != NULL;
				if (valid) {
					sess->trans.kex_selected = &a;
					bool unleased = shape == 1 || shape == 2;
					int expected = results[r] > 0 || (unleased && results[r] == 0)
					    ? DSSH_ERROR_INVALID : results[r];
					int ret = kex(sess);
					valid = valid && ret == expected;
					if (ret != 0)
						valid = valid && sess->trans.kex_active == NULL && sess->trans.kex_state == NULL
						    && sess->trans.kex_result.lease.release == NULL
						    && sess->trans.kex_result.lease.owner == NULL;
				}
				dssh_session_cleanup(sess);
				unsigned releases = shape == 1 || shape == 2 ? 0U : 1U;
				valid = valid && trace.valid && trace.providers[0].releases == releases
				    && trace.providers[0].cleanups == 1;
				kex_trace = NULL;
				dssh_test_reset_global_config();
				if (!valid)
					fprintf(stderr, "KEX lease mode=%u shape=%u result=%d\n", mode, shape, results[r]);
				ASSERT_TRUE(valid);
			}
		}
	}
	return TEST_PASS;
}

static int
test_kex_state_newkeys_terminated(void)
{
	dssh_test_reset_global_config();
	struct kex_trace trace = {.client = true, .valid = true};
	kex_trace = &trace;
	struct dssh_kex_s a, b;
	kex_trace_methods(&a, &b);
	dssh_session sess = dssh_session_init(true, 0);
	trace.session = sess;
	bool valid = sess != NULL;
	if (valid) {
		sess->trans.kex_selected = &a;
		valid = kex(sess) == 0;
		session_set_terminate(sess);
		int ret = newkeys(sess);
		valid = valid && ret == DSSH_ERROR_TERMINATED && trace.providers[0].releases == 1
		    && trace.providers[0].cleanups == 1 && sess->trans.kex_active == NULL
		    && sess->trans.kex_state == NULL;
	}
	dssh_session_cleanup(sess);
	valid = valid && trace.valid && trace.providers[0].releases == 1 && trace.providers[0].cleanups == 1;
	kex_trace = NULL;
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static void
kex_trace_cipher_init(void)
{
	struct kex_trace_provider *p = &kex_trace->providers[0];
	kex_trace->valid = kex_trace->valid && p->releases == 1 && p->cleanups == 0
	    && kex_trace_zero(p->secret, sizeof(p->secret)) && kex_trace_zero(p->hash, sizeof(p->hash))
	    && kex_trace->session->trans.kex_result.lease.release == NULL;
}

static bool
kex_derivation_case(bool client, bool clock_failure, int allocation, bool *succeeded)
{
	dssh_test_reset_global_config();
	struct kex_trace trace = {.client = client, .valid = true};
	kex_trace = &trace;
	struct dssh_kex_s a, b;
	kex_trace_methods(&a, &b);
	struct init_trace cipher_trace = {.client = client, .fail_slot = 4,
	    .observe_init = kex_trace_cipher_init};
	init_trace = &cipher_trace;
	struct init_modules modules;
	bool valid = make_init_modules(&modules, false);
	dssh_session sess = dssh_session_init(client, 0);
	trace.session = sess;
	valid = valid && sess != NULL;
	uint8_t session_id[32] = {1};
	int ret = DSSH_ERROR_INIT;
	if (valid) {
		sess->trans.kex_selected = &a;
		valid = kex(sess) == 0;
		if (valid) {
			sess->trans.session_id = session_id;
			sess->trans.session_id_sz = sizeof(session_id);
			select_init_modules(sess, &modules, 0);
			if (clock_failure)
				dssh_test_clock_fail_after(0);
			if (allocation >= 0)
				dssh_test_alloc_fail_after(allocation);
			ret = install_init_modules(sess, &modules, true);
			dssh_test_alloc_reset();
			dssh_test_clock_reset();
			valid = valid && ret == (clock_failure ? DSSH_ERROR_INIT
			    : (allocation >= 0 && ret != 0 ? DSSH_ERROR_ALLOC : 0))
			    && trace.providers[0].releases == 1 && trace.providers[0].cleanups == 0
			    && sess->trans.kex_result.lease.release == NULL && sess->trans.kex_active == &a;
		}
		sess->trans.session_id = NULL;
	}
	*succeeded = ret == 0;
	dssh_session_cleanup(sess);
	valid = valid && trace.valid && trace.providers[0].cleanups == 1 && cipher_trace.live == 0
	    && !cipher_trace.wrong_owner;
	free_init_modules(&modules);
	init_trace = NULL;
	kex_trace = NULL;
	dssh_test_reset_global_config();
	if (!valid)
		fprintf(stderr, "KEX derivation client=%d clock=%d allocation=%d ret=%d\n",
		    client, clock_failure, allocation, ret);
	return valid;
}

static int
test_kex_state_derivation_release(void)
{
	for (unsigned role = 0; role < 2; role++) {
		bool succeeded = false;
		ASSERT_TRUE(kex_derivation_case(role != 0, true, -1, &succeeded) && !succeeded);
		ASSERT_TRUE(kex_derivation_case(role != 0, false, -1, &succeeded) && succeeded);
		for (int allocation = 0; allocation < 64; allocation++) {
			ASSERT_TRUE(kex_derivation_case(role != 0, false, allocation, &succeeded));
			if (succeeded)
				break;
		}
		ASSERT_TRUE(succeeded);
	}
	return TEST_PASS;
}

static int
test_kex_required_callbacks(void)
{
	for (unsigned missing = 0; missing < 5; missing++) {
		dssh_test_reset_global_config();
		static const char name[] = "kex-contract";
		if (sizeof(struct dssh_kex_s) > SIZE_MAX - sizeof(name))
			return TEST_FAIL;
		dssh_kex method = malloc(sizeof(*method) + sizeof(name));
		ASSERT_NOT_NULL(method);
		*method = (struct dssh_kex_s){.init = test_kex_stateless_init,
		    .handler = test_kex_static_handler, .cleanup = test_kex_stateless_cleanup, .hash_name = "SHA-256"};
		memcpy(method->name, name, sizeof(name));
		if (missing == 0)
			method->init = NULL;
		else if (missing == 1)
			method->handler = NULL;
		else if (missing == 2)
			method->cleanup = NULL;
		else if (missing == 3)
			method->hash_name = NULL;
		int ret = dssh_transport_register_kex(method);
		bool valid = ret == (missing == 4 ? 0 : DSSH_ERROR_INVALID);
		int configuration = 17;
		bool published = gconf.kex_head == method;
		if (!published)
			free(method);
		else {
			/* Keep the borrowed configuration alive through the reset below. */
			valid = valid && dssh_kex_set_ctx(name, &configuration) == 0
			    && gconf.kex_head == method && method->config == &configuration;
		}
		dssh_test_reset_global_config();
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

#endif
