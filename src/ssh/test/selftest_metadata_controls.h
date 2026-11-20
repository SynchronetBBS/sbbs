/* Scalar overrides stay in caller-owned output; no live library state changes. */
static atomic_bool owner_metadata_fired;
static unsigned owner_metadata_clocks;
static unsigned owner_metadata_queries;
static _Thread_local struct {
	bool active, fail_clock, valid;
	struct timespec now;
	int sleep_result;
	unsigned clocks, sleeps;
	atomic_bool *boolean;
	atomic_int *integer;
} owner_metadata_wait;

static bool
owner_metadata_snapshot(const uint8_t *source, size_t length, uint8_t *destination,
    size_t capacity, size_t *used)
{
	if (strncmp(owner_mode, "metadata-sid-", 13) != 0)
		return selftest_snapshot_id(source, length, destination, capacity, used);
	if (strcmp(owner_mode, "metadata-sid-native") == 0)
		return selftest_snapshot_id(source, length, destination, capacity, used);
	if (atomic_exchange(&owner_metadata_fired, true))
		abort();
	if (strcmp(owner_mode, "metadata-sid-zero") == 0)
		return selftest_snapshot_id(source, 0, destination, capacity, used);
	if (strcmp(owner_mode, "metadata-sid-null") == 0)
		return selftest_snapshot_id(NULL, length, destination, capacity, used);
	if (strcmp(owner_mode, "metadata-sid-oversize") == 0) {
		if (capacity == SIZE_MAX)
			return false;
		return selftest_snapshot_id(source, capacity + 1, destination, capacity, used);
	}
	bool ret = selftest_snapshot_id(source, length, destination, capacity, used);
	if (ret) {
		if (strcmp(owner_mode, "metadata-sid-corrupt") == 0)
			destination[0] ^= 1;
		else if (strcmp(owner_mode, "metadata-sid-length") == 0)
			*used = length - 1;
		else
			abort();
	}
	return ret;
}

static int64_t
owner_metadata_read(uint8_t *buffer, size_t capacity, int64_t result)
{
	if (owner_main && strcmp(owner_mode, "metadata-rekey-corrupt") == 0 && result > 0) {
		size_t count;
		if (buffer == NULL || !selftest_io_count(result, capacity, &count) || count == 0)
			abort();
		if (!atomic_exchange(&owner_metadata_fired, true))
			buffer[0] ^= 1;
	}
	return result;
}

static int
owner_metadata_worker(void *data)
{
	struct server_echo_arg *argument = data;
	int ret = server_echo_thread(data);
	if (ret == 0 && argument->result == 0) {
		argument->result = -1;
		atomic_store(&owner_metadata_fired, true);
	}
	return ret;
}

static thrd_start_t
owner_metadata_worker_function(thrd_start_t function)
{
	return strcmp(owner_mode, "metadata-rekey-worker") == 0 && function == server_echo_thread
	    ? owner_metadata_worker : function;
}

static bool
owner_metadata_runner_clock(struct timespec *value)
{
	bool ret = dssh_test_runner_clock(value);
	if (strncmp(owner_mode, "metadata-clock-", 15) != 0)
		return ret;
	if (owner_metadata_clocks >= 2)
		abort();
	owner_metadata_clocks++;
	unsigned target = strcmp(owner_mode, "metadata-clock-start") == 0 ? 1U : 2U;
	if (ret && owner_metadata_clocks == target) {
		atomic_store(&owner_metadata_fired, true);
		return false;
	}
	return ret;
}

static int
owner_metadata_clock(clockid_t id, struct timespec *value)
{
	int ret = clock_gettime(id, value);
	if (!owner_metadata_wait.active)
		return ret;
	if (owner_metadata_wait.clocks >= 3 || id != CLOCK_MONOTONIC || ret != 0) {
		owner_metadata_wait.valid = false;
		return -1;
	}
	owner_metadata_wait.clocks++;
	if (owner_metadata_wait.fail_clock)
		return -1;
	*value = owner_metadata_wait.now;
	return 0;
}

static int
owner_metadata_sleep(const struct timespec *request, struct timespec *remaining)
{
	if (!owner_metadata_wait.active)
		return thrd_sleep(request, remaining);
	if (request == NULL || request->tv_sec != 0 || request->tv_nsec != 1000000
	    || remaining != NULL || owner_metadata_wait.sleeps >= 1) {
		owner_metadata_wait.valid = false;
		return -2;
	}
	owner_metadata_wait.sleeps++;
	int ret = thrd_sleep(request, remaining);
	if (ret != 0 && ret != -1) {
		owner_metadata_wait.valid = false;
		return ret;
	}
	if (owner_metadata_wait.boolean != NULL)
		atomic_store(owner_metadata_wait.boolean, true);
	if (owner_metadata_wait.integer != NULL)
		atomic_store(owner_metadata_wait.integer, 1);
	return owner_metadata_wait.sleep_result;
}

static const char *
owner_metadata_query(const char *name, const char *native, dssh_session sess, enum dssh_direction direction)
{
	if (strncmp(owner_mode, "metadata-getter-", 16) != 0)
		return native;
	if (owner_metadata_queries >= 64)
		abort();
	owner_metadata_queries++;
	if (native != NULL && !atomic_load(&owner_metadata_fired)) {
		if (strcmp(owner_mode + 16, name) == 0) {
			atomic_store(&owner_metadata_fired, true);
			return NULL;
		}
		if (strcmp(owner_mode, "metadata-getter-direction") == 0 && strcmp(name, "mac") == 0
		    && !sess->trans.client && direction == DSSH_SERVER_TO_CLIENT) {
			atomic_store(&owner_metadata_fired, true);
			return "unexpected";
		}
	}
	return native;
}

static const char *owner_metadata_kex(dssh_session s)
{
	return owner_metadata_query("kex", dssh_transport_get_kex_name(s), s, DSSH_CLIENT_TO_SERVER);
}
static const char *owner_metadata_key(dssh_session s)
{
	return owner_metadata_query("key", dssh_transport_get_hostkey_name(s), s, DSSH_CLIENT_TO_SERVER);
}
static const char *owner_metadata_enc(dssh_session s, enum dssh_direction d)
{
	return owner_metadata_query("enc", dssh_transport_get_enc_name(s, d), s, d);
}
static const char *owner_metadata_mac(dssh_session s, enum dssh_direction d)
{
	return owner_metadata_query("mac", dssh_transport_get_mac_name(s, d), s, d);
}
static const char *owner_metadata_comp(dssh_session s, enum dssh_direction d)
{
	return owner_metadata_query("comp", dssh_transport_get_comp_name(s, d), s, d);
}

static bool
owner_metadata_boundaries(void)
{
	const uint8_t source[] = {1, 2, 3};
	uint8_t output[4] = {9, 9, 9, 9};
	const uint8_t untouched[4] = {9, 9, 9, 9};
	size_t used = 99;
	bool valid = !selftest_snapshot_id(NULL, 3, output, 4, &used)
	    && !selftest_snapshot_id(source, 0, output, 4, &used)
	    && !selftest_snapshot_id(source, 5, output, 4, &used)
	    && !selftest_snapshot_id(source, 3, NULL, 4, &used)
	    && !selftest_snapshot_id(source, 3, output, 4, NULL)
	    && used == 99 && memcmp(output, untouched, sizeof(output)) == 0
	    && selftest_snapshot_id(source, 3, output, 4, &used)
	    && used == 3 && memcmp(output, source, 3) == 0 && output[3] == 9;
	for (unsigned mode = 0; mode < 15; mode++) {
		struct timespec start = {.tv_sec = 100, .tv_nsec = 500};
		owner_metadata_wait.now = start;
		owner_metadata_wait.clocks = owner_metadata_wait.sleeps = 0;
		owner_metadata_wait.valid = true;
		owner_metadata_wait.fail_clock = mode == 10;
		owner_metadata_wait.sleep_result = mode == 12 ? -1 : mode == 13 ? -2 : mode == 14 ? 1 : 0;
		owner_metadata_wait.active = true;
		if (mode == 1 || mode == 2) {
			owner_metadata_wait.now.tv_sec = 105;
			owner_metadata_wait.now.tv_nsec = mode == 1 ? 499 : 500;
		}
		if (mode == 3)
			owner_metadata_wait.now.tv_sec = 106;
		if (mode == 4)
			owner_metadata_wait.now.tv_sec = 99;
		if (mode == 5)
			owner_metadata_wait.now.tv_nsec = 499;
		if (mode == 6)
			start.tv_nsec = -1;
		if (mode == 7)
			start.tv_nsec = 1000000000L;
		if (mode == 8)
			owner_metadata_wait.now.tv_nsec = -1;
		if (mode == 9)
			owner_metadata_wait.now.tv_nsec = 1000000000L;
		bool expected = mode == 0 || mode == 1 || mode == 12;
		bool result = dssh_test_wait_tick(mode == 11 ? NULL : &start);
		owner_metadata_wait.active = false;
		unsigned clocks = mode == 6 || mode == 7 || mode == 11 ? 0U : 1U;
		unsigned sleeps = mode == 0 || mode == 1 || mode >= 12 ? 1U : 0U;
		valid &= result == expected && owner_metadata_wait.valid
		    && owner_metadata_wait.clocks == clocks && owner_metadata_wait.sleeps == sleeps;
	}
	atomic_bool boolean;
	atomic_int integer;
	atomic_init(&boolean, false);
	atomic_init(&integer, 0);
	owner_metadata_wait.now = (struct timespec){.tv_sec = 100, .tv_nsec = 0};
	owner_metadata_wait.sleep_result = 0;
	owner_metadata_wait.fail_clock = false;
	owner_metadata_wait.active = true;
	owner_metadata_wait.clocks = owner_metadata_wait.sleeps = 0;
	owner_metadata_wait.boolean = &boolean;
	valid &= dssh_test_wait_bool(&boolean, true) && owner_metadata_wait.clocks == 2
	    && owner_metadata_wait.sleeps == 1 && atomic_load(&boolean);
	owner_metadata_wait.boolean = NULL;
	owner_metadata_wait.clocks = owner_metadata_wait.sleeps = 0;
	owner_metadata_wait.integer = &integer;
	valid &= dssh_test_wait_int(&integer, 1) && owner_metadata_wait.clocks == 2
	    && owner_metadata_wait.sleeps == 1 && atomic_load(&integer) == 1;
	owner_metadata_wait.integer = NULL;
	for (unsigned mode = 0; mode < 6; mode++) {
		owner_metadata_wait.clocks = owner_metadata_wait.sleeps = 0;
		owner_metadata_wait.now.tv_nsec = mode < 2 ? -1 : 0;
		owner_metadata_wait.fail_clock = mode >= 2 && mode < 4;
		bool result = mode % 2 == 0 ? dssh_test_wait_bool(&boolean, true)
		    : dssh_test_wait_int(&integer, 1);
		valid &= result == (mode >= 4) && owner_metadata_wait.clocks == 1
		    && owner_metadata_wait.sleeps == 0;
	}
	owner_metadata_wait.active = false;
	owner_note("SELFTEST_METADATA_BOUNDARIES snapshots=6 waits=15 predicates=8 valid=%u\n", valid ? 1U : 0U);
	return valid;
}

static int
owner_metadata_control(void)
{
	atomic_init(&owner_metadata_fired, false);
	if (strcmp(owner_mode, "metadata-boundaries") == 0)
		return owner_metadata_boundaries() ? 0 : 2;
	bool getters = strncmp(owner_mode, "metadata-getter-", 16) == 0;
	bool clocks = strncmp(owner_mode, "metadata-clock-", 15) == 0;
	bool sid = strncmp(owner_mode, "metadata-sid-", 13) == 0;
	bool native = strcmp(owner_mode, "metadata-rekey-native") == 0 || strcmp(owner_mode, "metadata-sid-native") == 0;
	int host = getters ? test_self_handshake_curve25519()
	    : clocks ? test_open_channel_timeout() : sid ? test_self_rekey_preserves_channels() : test_self_rekey_manual();
	unsigned workers = getters || clocks ? 4U : 5U;
	bool valid = host == (native ? TEST_PASS : TEST_FAIL)
	    && atomic_load(&owner_metadata_fired) == !native
	    && g_active_ctx == NULL && owner_created == workers && owner_joined == workers
	    && owner_sessions == 2 && owner_released == 2 && owner_closed == 2
	    && atomic_load(&owner_channels_created) == atomic_load(&owner_channels_closed)
	    && owner_param_clean;
	owner_note("SELFTEST_METADATA mode=%s host=%d fired=%u clocks=%u queries=%u created=%u joined=%u sessions=%u released=%u fds=%u channels=%u closed=%u valid=%u\n",
	    owner_mode, host, atomic_load(&owner_metadata_fired) ? 1U : 0U,
	    owner_metadata_clocks, owner_metadata_queries, owner_created, owner_joined,
	    owner_sessions, owner_released, owner_closed, atomic_load(&owner_channels_created),
	    atomic_load(&owner_channels_closed), valid ? 1U : 0U);
	return valid ? 0 : 2;
}
