/* Caller-thread constructor refusals. Real initialized objects and allocations
 * retire through the actual library failure prefix before the API returns. */
struct owner_constructor_case {
	const char *mode;
	bool client;
	char operation;
	unsigned index;
};
static const struct owner_constructor_case owner_constructor_cases[] = {
	{ "constructor-open-malloc-0", true, 'm', 0 },
	{ "constructor-open-malloc-1", true, 'm', 1 },
	{ "constructor-open-malloc-2", true, 'm', 2 },
	{ "constructor-open-malloc-3", true, 'm', 3 },
	{ "constructor-open-malloc-4", true, 'm', 4 },
	{ "constructor-open-malloc-5", true, 'm', 5 },
	{ "constructor-open-malloc-6", true, 'm', 6 },
	{ "constructor-open-realloc-0", true, 'r', 0 },
	{ "constructor-open-realloc-1", true, 'r', 1 },
	{ "constructor-open-realloc-2", true, 'r', 2 },
	{ "constructor-open-mutex-0", true, 'l', 0 },
	{ "constructor-open-mutex-1", true, 'l', 1 },
	{ "constructor-open-condition-0", true, 'c', 0 },
	{ "constructor-accept-malloc-0", false, 'm', 0 },
	{ "constructor-accept-malloc-1", false, 'm', 1 },
	{ "constructor-accept-malloc-2", false, 'm', 2 },
	{ "constructor-accept-realloc-0", false, 'r', 0 },
	{ "constructor-accept-realloc-1", false, 'r', 1 },
	{ "constructor-accept-realloc-2", false, 'r', 2 },
	{ "constructor-accept-mutex-0", false, 'l', 0 },
	{ "constructor-accept-mutex-1", false, 'l', 1 },
	{ "constructor-accept-condition-0", false, 'c', 0 },
};

struct owner_constructor_observation {
	bool started, fired, valid, returned_null;
	unsigned mallocs, reallocs, mutexes, conditions;
	unsigned allocations, frees, session_allocations, live;
	unsigned mutex_created, mutex_destroyed, condition_created, condition_destroyed;
	bool channel_ended, wait_ended;
	struct dssh_test_channel_observation channel;
	struct dssh_test_wait_observation wait;
};
static const struct owner_constructor_case *owner_constructor_selected;
static struct owner_constructor_observation owner_constructor_result;
static _Thread_local struct {
	bool active, channel_pending, wait_pending;
	struct owner_constructor_observation result;
	struct dssh_channel_s *channel;
	void *allocations[16];
	bool mutex[2], condition;
} owner_constructor;

void *__real_dssh_test_malloc(size_t);
void *__real_dssh_test_realloc(void *, size_t);
void __real_dssh_test_free(void *);
int __real_dssh_test_mtx_init(mtx_t *, int);
int __real_dssh_test_cnd_init(cnd_t *);
struct dssh_test_channel_observation __real_dssh_test_channel_observe_end(void);
struct dssh_test_wait_observation __real_dssh_test_wait_observe_end(void);
#ifdef DSSH_THREADS_SHIM
 #define OWNER_REAL_MTX_DESTROY __real_dssh_thread_mtx_destroy
 #define OWNER_WRAP_MTX_DESTROY __wrap_dssh_thread_mtx_destroy
 #define OWNER_REAL_CND_DESTROY __real_dssh_thread_cnd_destroy
 #define OWNER_WRAP_CND_DESTROY __wrap_dssh_thread_cnd_destroy
#else
 #define OWNER_REAL_MTX_DESTROY __real_mtx_destroy
 #define OWNER_WRAP_MTX_DESTROY __wrap_mtx_destroy
 #define OWNER_REAL_CND_DESTROY __real_cnd_destroy
 #define OWNER_WRAP_CND_DESTROY __wrap_cnd_destroy
#endif
void OWNER_REAL_MTX_DESTROY(mtx_t *);
void OWNER_REAL_CND_DESTROY(cnd_t *);

static unsigned
owner_constructor_tick(unsigned *counter)
{
	if (*counter >= 32)
		abort();
	return (*counter)++;
}

static bool
owner_constructor_refuse(char operation, unsigned index)
{
	if (owner_constructor_selected->operation != operation || owner_constructor_selected->index != index)
		return false;
	if (owner_constructor.result.fired)
		abort();
	owner_constructor.result.fired = true;
	return true;
}

static void
owner_constructor_track(void *pointer, bool session_owned)
{
	if (pointer == NULL) {
		owner_constructor.result.valid = false;
		return;
	}
	owner_constructor.result.allocations++;
	if (session_owned) {
		owner_constructor.result.session_allocations++;
		return;
	}
	for (size_t i = 0; i < 16; i++) {
		if (owner_constructor.allocations[i] == NULL) {
			owner_constructor.allocations[i] = pointer;
			owner_constructor.result.live++;
			return;
		}
	}
	owner_constructor.result.valid = false;
}

void *
__wrap_dssh_test_malloc(size_t size)
{
	if (!owner_constructor.active)
		return __real_dssh_test_malloc(size);
	unsigned index = owner_constructor_tick(&owner_constructor.result.mallocs);
	unsigned channel_index = owner_constructor_selected->client ? 1U : 0U;
	if ((index == channel_index && size != sizeof(struct dssh_channel_s))
	    || (owner_constructor_selected->client && index == 0 && size != sizeof(struct dssh_channel_id)))
		owner_constructor.result.valid = false;
	if (owner_constructor_refuse('m', index))
		return NULL;
	void *pointer = __real_dssh_test_malloc(size);
	bool session_owned = owner_constructor_selected->client && index == 0;
	owner_constructor_track(pointer, session_owned);
	if (index == channel_index)
		owner_constructor.channel = pointer;
	return pointer;
}

void *
__wrap_dssh_test_realloc(void *old, size_t size)
{
	if (!owner_constructor.active)
		return __real_dssh_test_realloc(old, size);
	unsigned index = owner_constructor_tick(&owner_constructor.result.reallocs);
	/* Fresh slots and the fresh registry candidate all start NULL. */
	if (old != NULL || index > 2) {
		owner_constructor.result.valid = false;
		return NULL;
	}
	if (owner_constructor_refuse('r', index))
		return NULL;
	void *pointer = __real_dssh_test_realloc(old, size);
	/* Registry publication transfers the third allocation to the session. */
	owner_constructor_track(pointer, index == 2);
	return pointer;
}

void
__wrap_dssh_test_free(void *pointer)
{
	if (owner_constructor.active && pointer != NULL) {
		for (size_t i = 0; i < 16; i++) {
			if (owner_constructor.allocations[i] == pointer) {
				owner_constructor.allocations[i] = NULL;
				if (owner_constructor.result.live == 0)
					abort();
				owner_constructor.result.live--;
				owner_constructor.result.frees++;
				break;
			}
		}
		if (pointer == owner_constructor.channel) {
			if (owner_constructor.mutex[0] || owner_constructor.mutex[1] || owner_constructor.condition)
				owner_constructor.result.valid = false;
			owner_constructor.channel = NULL;
		}
	}
	__real_dssh_test_free(pointer);
}

static int
owner_constructor_mutex(mtx_t *mutex)
{
	struct dssh_channel_s *channel = owner_constructor.channel;
	if (channel == NULL)
		return -1;
	return mutex == &channel->buf_mtx ? 0 : mutex == &channel->cb_mtx ? 1 : -1;
}

int
__wrap_dssh_test_mtx_init(mtx_t *mutex, int type)
{
	if (!owner_constructor.active)
		return __real_dssh_test_mtx_init(mutex, type);
	unsigned index = owner_constructor_tick(&owner_constructor.result.mutexes);
	int which = owner_constructor_mutex(mutex);
	if (which < 0 || type != mtx_plain || index > 1) {
		owner_constructor.result.valid = false;
		return thrd_error;
	}
	if (owner_constructor_refuse('l', index))
		return thrd_error;
	int ret = __real_dssh_test_mtx_init(mutex, type);
	if (ret == thrd_success) {
		owner_constructor.mutex[which] = true;
		owner_constructor.result.mutex_created++;
	}
	else
		owner_constructor.result.valid = false;
	return ret;
}

int
__wrap_dssh_test_cnd_init(cnd_t *condition)
{
	if (!owner_constructor.active)
		return __real_dssh_test_cnd_init(condition);
	unsigned index = owner_constructor_tick(&owner_constructor.result.conditions);
	if (owner_constructor.channel == NULL || condition != &owner_constructor.channel->poll_cnd || index != 0) {
		owner_constructor.result.valid = false;
		return thrd_error;
	}
	if (owner_constructor_refuse('c', index))
		return thrd_error;
	int ret = __real_dssh_test_cnd_init(condition);
	if (ret == thrd_success) {
		owner_constructor.condition = true;
		owner_constructor.result.condition_created++;
	}
	else
		owner_constructor.result.valid = false;
	return ret;
}

void
OWNER_WRAP_MTX_DESTROY(mtx_t *mutex)
{
	if (owner_constructor.active) {
		int which = owner_constructor_mutex(mutex);
		if (which < 0 || !owner_constructor.mutex[which]) {
			owner_constructor.result.valid = false;
			return;
		}
		owner_constructor.mutex[which] = false;
		owner_constructor.result.mutex_destroyed++;
	}
	OWNER_REAL_MTX_DESTROY(mutex);
}

void
OWNER_WRAP_CND_DESTROY(cnd_t *condition)
{
	if (owner_constructor.active) {
		if (owner_constructor.channel == NULL || condition != &owner_constructor.channel->poll_cnd
		    || !owner_constructor.condition) {
			owner_constructor.result.valid = false;
			return;
		}
		owner_constructor.condition = false;
		owner_constructor.result.condition_destroyed++;
	}
	OWNER_REAL_CND_DESTROY(condition);
}

struct dssh_test_channel_observation
__wrap_dssh_test_channel_observe_end(void)
{
	struct dssh_test_channel_observation result = __real_dssh_test_channel_observe_end();
	if (owner_constructor.channel_pending) {
		owner_constructor.channel_pending = false;
		owner_constructor_result.channel_ended = true;
		owner_constructor_result.channel = result;
	}
	return result;
}

struct dssh_test_wait_observation
__wrap_dssh_test_wait_observe_end(void)
{
	struct dssh_test_wait_observation result = __real_dssh_test_wait_observe_end();
	if (owner_constructor.wait_pending) {
		owner_constructor.wait_pending = false;
		owner_constructor_result.wait_ended = true;
		owner_constructor_result.wait = result;
	}
	return result;
}

static void
owner_constructor_begin(bool client)
{
	if (owner_constructor_selected == NULL || owner_constructor_selected->client != client)
		return;
	if (owner_constructor.active || owner_constructor_result.started)
		abort();
	owner_constructor.result = (struct owner_constructor_observation){ .started = true, .valid = true };
	owner_constructor.active = true;
}

static void
owner_constructor_end(dssh_channel channel)
{
	if (!owner_constructor.active)
		return;
	owner_constructor.active = false;
	struct owner_constructor_observation *result = &owner_constructor.result;
	result->returned_null = channel == NULL;
	result->valid &= result->live == 0 && owner_constructor.channel == NULL
	    && result->session_allocations <= result->allocations
	    && result->allocations - result->session_allocations == result->frees
	    && !owner_constructor.mutex[0] && !owner_constructor.mutex[1] && !owner_constructor.condition
	    && result->mutex_created == result->mutex_destroyed
	    && result->condition_created == result->condition_destroyed;
	owner_constructor_result = *result;
	/* Drop every observer borrow before the original owner can retire. */
	for (size_t i = 0; i < 16; i++)
		owner_constructor.allocations[i] = NULL;
	owner_constructor.channel = NULL;
	owner_constructor.channel_pending = true;
	owner_constructor.wait_pending = true;
}

static int
owner_constructor_control(void)
{
	for (size_t i = 0; i < sizeof(owner_constructor_cases) / sizeof(owner_constructor_cases[0]); i++) {
		if (strcmp(owner_mode, owner_constructor_cases[i].mode) == 0)
			owner_constructor_selected = &owner_constructor_cases[i];
	}
	if (owner_constructor_selected == NULL)
		return 2;
	bool client = owner_constructor_selected->client;
	int host = client ? test_open_channel_timeout() : test_setup_recv_timeout();
	struct owner_constructor_observation *result = &owner_constructor_result;
	unsigned attempts = owner_constructor_selected->operation == 'm' ? result->mallocs
	    : owner_constructor_selected->operation == 'r' ? result->reallocs
	    : owner_constructor_selected->operation == 'l' ? result->mutexes : result->conditions;
	bool late = !client && owner_constructor_selected->operation == 'm' && owner_constructor_selected->index == 2;
	bool valid = host == TEST_FAIL && result->started && result->fired && result->valid
	    && result->returned_null && result->channel_ended && result->wait_ended
	    && attempts == owner_constructor_selected->index + 1
	    && !result->channel.seen && !dssh_test_channel_expired(result->wait, result->channel)
	    && !result->wait.failed && result->wait.timed_out == late
	    && g_active_ctx == NULL && owner_created == (client ? 4U : 5U)
	    && owner_joined == owner_created && owner_join_attempts == owner_created
	    && owner_sessions == 2 && owner_released == 2 && owner_closed == 2
	    && atomic_load(&owner_channels_created) == 0 && atomic_load(&owner_channels_closed) == 0
	    && owner_param_clean;
	owner_note("SELFTEST_CONSTRUCTOR mode=%s host=%d fired=%u mallocs=%u reallocs=%u mutexes=%u conditions=%u allocations=%u frees=%u session_allocations=%u live=%u mutex_created=%u mutex_destroyed=%u condition_created=%u condition_destroyed=%u channel_seen=%u wait_native=%u wait_expired=%u wait_failed=%u created=%u joined=%u sessions=%u released=%u fds=%u valid=%u\n",
	    owner_mode, host, result->fired ? 1U : 0U, result->mallocs, result->reallocs,
	    result->mutexes, result->conditions, result->allocations, result->frees,
	    result->session_allocations, result->live, result->mutex_created, result->mutex_destroyed,
	    result->condition_created, result->condition_destroyed, result->channel.seen ? 1U : 0U,
	    result->wait.native ? 1U : 0U, result->wait.timed_out ? 1U : 0U, result->wait.failed ? 1U : 0U,
	    owner_created, owner_joined, owner_sessions, owner_released, owner_closed, valid ? 1U : 0U);
	return valid ? 0 : 2;
}
