/* Exercise actual public outbound snapshot ownership on authenticated sessions. */
static const char snapshot_term[] = "xterm-snapshot-distinct-terminal-value-for-audit";
static const char snapshot_command[] = "snapshot-command-to-verify";
static const char snapshot_subsystem[] = "snapshot-subsystem-to-verify";
static const char *snapshot_names[] = {"COPY_ALPHA", "COPY_BETA"};
static const char *snapshot_values[] = {"value-alpha", "value-beta"};

struct params_snapshot_fixture {
	struct flow_fixture flow;
	bool retired;
	struct conn_ctx ctx;
	struct dssh_chan_accept_cbs cbs;
	struct dssh_chan_params params;
	dssh_channel server_ch;
	unsigned target;
	unsigned attempts;
	unsigned frees;
	unsigned sends;
	bool fail;
	bool sync;
	bool armed;
	bool fired;
	bool sizes_valid;
	bool before_io;
};

static _Thread_local struct params_snapshot_fixture *params_snapshot_current;

static void
params_snapshot_retire(void *data)
{
	struct flow_fixture *flow = data;
	struct params_snapshot_fixture *fixture = flow->data;
	flow_retire(flow);
	accept_phase_test_hook = NULL;
	params_snapshot_current = NULL;
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_chan_params_free(&fixture->params);
	fixture->retired = true;
}

static void
params_snapshot_own(struct params_snapshot_fixture *fixture)
{
	flow_init(&fixture->flow, &fixture->ctx, NULL, fixture);
	fixture->flow.workers.retire = params_snapshot_retire;
}

static int
params_snapshot_tx(dssh_session sess)
{
	(void)sess;
	params_snapshot_current->sends++;
	return 0;
}

static void
params_snapshot_alloc(size_t count, size_t size, void *opaque)
{
	struct params_snapshot_fixture *fixture = opaque;
	if (fixture->fired)
		return;
	if (!fixture->armed) {
		if (count != 1 || size != sizeof(snapshot_term))
			return;
		fixture->armed = true;
		fixture->before_io = fixture->sends == 0;
	}
	const size_t sizes[] = {sizeof(snapshot_term), sizeof(snapshot_command),
	    sizeof(snapshot_subsystem), 3 * sizeof(struct dssh_chan_mode_entry),
	    2 * sizeof(struct dssh_chan_env), sizeof("COPY_ALPHA"),
	    sizeof("value-alpha"), sizeof("COPY_BETA"), sizeof("value-beta")};
	unsigned index = fixture->attempts++;
	if (index < sizeof(sizes) / sizeof(sizes[0]))
		fixture->sizes_valid = fixture->sizes_valid && count == 1 && size == sizes[index];
	if (fixture->sync && index == 8) {
		fixture->fired = true;
		dssh_test_ossl_fail_after(0);
	}
	if (fixture->fail && index == fixture->target) {
		fixture->fired = true;
		dssh_test_alloc_fail_after(0);
	}
}

static void
params_snapshot_free(const void *ptr, void *opaque)
{
	struct params_snapshot_fixture *fixture = opaque;
	if (fixture->fired && ptr != NULL)
		fixture->frees++;
}

static int
params_snapshot_env(dssh_channel ch, const struct dssh_chan_params *params, void *opaque)
{
	(void)ch;
	(void)opaque;
	if (params == NULL || params->env == NULL || params->env_count == 0 || params->env_count > 2)
		return DSSH_ERROR_REJECTED;
	size_t i = params->env_count - 1;
	if (params->env[i].name == NULL || params->env[i].value == NULL)
		return DSSH_ERROR_REJECTED;
	return strcmp(params->env[i].name, snapshot_names[i]) == 0
	    && strcmp(params->env[i].value, snapshot_values[i]) == 0 ? 0 : DSSH_ERROR_REJECTED;
}

static int
params_snapshot_server(void *opaque)
{
	struct params_snapshot_fixture *fixture = opaque;
	dssh_test_alloc_exclude_thread();
	fixture->server_ch = dssh_chan_accept(fixture->ctx.server, &fixture->cbs, 3000);
	return 0;
}

static uint32_t
params_snapshot_zc(dssh_channel ch, int stream, const uint8_t *bytes, size_t len, void *opaque)
{
	(void)ch;
	(void)stream;
	(void)bytes;
	(void)len;
	(void)opaque;
	return 0;
}

static bool
params_snapshot_values_valid(dssh_channel ch, const struct dssh_chan_params *source)
{
	const struct dssh_chan_params *copy = dssh_chan_get_pty(ch);
	if (copy == NULL || copy->term == NULL || copy->command == NULL || copy->subsystem == NULL
	    || copy->env == NULL || copy->modes == NULL || copy->term == source->term || copy->command == source->command
	    || copy->subsystem == source->subsystem || copy->env == source->env || copy->modes == source->modes)
		return false;
	if (dssh_chan_get_type(ch) != source->type || !dssh_chan_has_pty(ch)
	    || strcmp(copy->term, snapshot_term) != 0 || strcmp(copy->command, snapshot_command) != 0
	    || strcmp(copy->subsystem, snapshot_subsystem) != 0 || copy->flags != source->flags
	    || copy->max_window != 4096 || copy->cols != 101 || copy->rows != 47
	    || copy->wpx != 808 || copy->hpx != 752 || copy->mode_count != 3 || copy->env_count != 2)
		return false;
	const char *retained = source->type == DSSH_CHAN_EXEC
	    ? dssh_chan_get_command(ch) : dssh_chan_get_subsystem(ch);
	const char *expected = source->type == DSSH_CHAN_EXEC ? snapshot_command : snapshot_subsystem;
	if (retained == NULL || strcmp(retained, expected) != 0)
		return false;
	for (size_t i = 0; i < copy->mode_count; i++) {
		if (copy->modes[i].opcode != source->modes[i].opcode
		    || copy->modes[i].value != source->modes[i].value)
			return false;
	}
	for (size_t i = 0; i < copy->env_count; i++) {
		if (copy->env[i].name == NULL || copy->env[i].value == NULL
		    || copy->env[i].name == source->env[i].name || copy->env[i].value == source->env[i].value
		    || strcmp(copy->env[i].name, snapshot_names[i]) != 0
		    || strcmp(copy->env[i].value, snapshot_values[i]) != 0)
			return false;
	}
	return true;
}

static int
params_snapshot_case(bool zc, unsigned target)
{
	struct params_snapshot_fixture fixture = {.target = target, .fail = target < 9, .sync = target == 10, .sizes_valid = true};
	ASSERT_EQ(conn_setup_no_start(&fixture.ctx), 0);
	params_snapshot_own(&fixture);
	ASSERT_TRUE(storage_checkpoint(ST_PARAMS_EMPTY));
	ASSERT_EQ(dssh_session_set_timeout(fixture.ctx.client, 3000), 0);
	ASSERT_EQ(dssh_session_set_timeout(fixture.ctx.server, 3000), 0);
	dssh_test_alloc_exclude_new_threads();
	ASSERT_EQ(dssh_session_start(fixture.ctx.client), 0);
	ASSERT_EQ(dssh_session_start(fixture.ctx.server), 0);
	ASSERT_EQ(dssh_chan_params_init(&fixture.params, zc ? DSSH_CHAN_SUBSYSTEM : DSSH_CHAN_EXEC), 0);
	ASSERT_EQ(dssh_chan_params_set_pty(&fixture.params, true), 0);
	ASSERT_EQ(dssh_chan_params_set_max_window(&fixture.params, 4096), 0);
	ASSERT_EQ(dssh_chan_params_set_term(&fixture.params, snapshot_term), 0);
	ASSERT_EQ(dssh_chan_params_set_command(&fixture.params, snapshot_command), 0);
	ASSERT_EQ(dssh_chan_params_set_subsystem(&fixture.params, snapshot_subsystem), 0);
	ASSERT_EQ(dssh_chan_params_set_size(&fixture.params, 101, 47, 808, 752), 0);
	ASSERT_EQ(dssh_chan_params_set_mode(&fixture.params, 1, UINT32_MAX), 0);
	ASSERT_EQ(dssh_chan_params_set_mode(&fixture.params, 53, 1), 0);
	ASSERT_EQ(dssh_chan_params_set_mode(&fixture.params, 128, 9600), 0);
	for (size_t i = 0; i < 2; i++)
		ASSERT_EQ(dssh_chan_params_add_env(&fixture.params, snapshot_names[i], snapshot_values[i]), 0);
	ASSERT_TRUE(storage_checkpoint(ST_PARAMS_BUILT));
	fixture.cbs = accept_cbs_all;
	fixture.cbs.env = params_snapshot_env;
	ASSERT_EQ(channel_worker_start(&fixture.flow.workers, 0, params_snapshot_server, &fixture), 0);
	ASSERT_TRUE(storage_checkpoint(ST_PARAMS_STARTED));
	params_snapshot_current = &fixture;
	accept_phase_test_hook = params_snapshot_tx;
	dssh_test_alloc_observe_allocations(params_snapshot_alloc, &fixture);
	dssh_test_alloc_observe_frees(params_snapshot_free, &fixture);
	dssh_channel ch = zc ? dssh_chan_zc_open(fixture.ctx.client, &fixture.params, params_snapshot_zc, NULL)
	    : dssh_chan_open(fixture.ctx.client, &fixture.params);
	accept_phase_test_hook = NULL;
	unsigned attempts = fixture.attempts;
	unsigned frees = fixture.frees;
	unsigned sends = fixture.sends;
	bool valid = fixture.armed && fixture.before_io && fixture.sizes_valid;
	if (fixture.fail)
		valid = valid && fixture.fired && ch == NULL && fixture.ctx.client->channel_count == 0
		    && attempts == target + 1 && frees == target + 1 && sends == 0;
	else if (fixture.sync)
		valid = valid && fixture.fired && ch == NULL && fixture.ctx.client->channel_count == 0
		    && attempts == 9 && frees == 10 && sends == 0 && dssh_test_ossl_count() == 1;
	else
		valid = valid && ch != NULL && attempts >= 9;
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	ASSERT_TRUE(valid);
	ASSERT_TRUE(storage_checkpoint(ST_PARAMS_ATTEMPT));
	if (valid && (fixture.fail || fixture.sync))
		ch = zc ? dssh_chan_zc_open(fixture.ctx.client, &fixture.params, params_snapshot_zc, NULL)
		    : dssh_chan_open(fixture.ctx.client, &fixture.params);
	if (!valid || ch == NULL) {
		mock_io_close_c2s(&fixture.ctx.io);
		mock_io_close_s2c(&fixture.ctx.io);
	}
	channel_worker_join(&fixture.flow.workers, 0);
	ASSERT_EQ(fixture.flow.workers.results[0], 0);
	ASSERT_TRUE(storage_checkpoint(ST_PARAMS_JOINED));
	valid = valid && ch != NULL && fixture.server_ch != NULL
	    && !dssh_session_is_terminated(fixture.ctx.client)
	    && !dssh_session_is_terminated(fixture.ctx.server);
	if (valid) {
		valid = params_snapshot_values_valid(ch, &fixture.params);
		ASSERT_TRUE(valid);
		ASSERT_TRUE(storage_checkpoint(ST_PARAMS_COPIED));
		/* Mutating and releasing the caller's builder must leave the retained
		 * command/subsystem, environment and modes completely independent. */
		fixture.params.term[0] = 'q';
		fixture.params.command[0] = 'q';
		fixture.params.subsystem[0] = 'q';
		fixture.params.env[0].name[0] = 'q';
		fixture.params.modes[0].value = 0;
		dssh_chan_params_free(&fixture.params);
		const struct dssh_chan_params *copy = dssh_chan_get_pty(ch);
		valid = valid && copy != NULL && copy->term != NULL && copy->command != NULL
		    && copy->subsystem != NULL && strcmp(copy->term, snapshot_term) == 0
		    && strcmp(copy->command, snapshot_command) == 0 && strcmp(copy->subsystem, snapshot_subsystem) == 0
		    && copy->env_count == 2 && copy->env != NULL && copy->env[0].name != NULL
		    && strcmp(copy->env[0].name, snapshot_names[0]) == 0
		    && copy->mode_count == 3 && copy->modes != NULL && copy->modes[0].value == UINT32_MAX;
	}
	ASSERT_TRUE(valid);
	flow_finish(&fixture.flow);
	ASSERT_TRUE(fixture.retired);
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}

#define PARAMS_SNAPSHOT_TEST(name, zc, target) \
static int name(void) \
{ \
	return params_snapshot_case(zc, target); \
}
PARAMS_SNAPSHOT_TEST(test_params_stream_snapshot, false, 9)
PARAMS_SNAPSHOT_TEST(test_params_zc_snapshot, true, 9)
PARAMS_SNAPSHOT_TEST(test_params_stream_sync, false, 10)
PARAMS_SNAPSHOT_TEST(test_params_zc_sync, true, 10)
PARAMS_SNAPSHOT_TEST(test_params_stream_term, false, 0)
PARAMS_SNAPSHOT_TEST(test_params_stream_command, false, 1)
PARAMS_SNAPSHOT_TEST(test_params_stream_subsystem, false, 2)
PARAMS_SNAPSHOT_TEST(test_params_stream_modes, false, 3)
PARAMS_SNAPSHOT_TEST(test_params_stream_env_array, false, 4)
PARAMS_SNAPSHOT_TEST(test_params_stream_env_name1, false, 5)
PARAMS_SNAPSHOT_TEST(test_params_stream_env_value1, false, 6)
PARAMS_SNAPSHOT_TEST(test_params_stream_env_name2, false, 7)
PARAMS_SNAPSHOT_TEST(test_params_stream_env_value2, false, 8)
PARAMS_SNAPSHOT_TEST(test_params_zc_term, true, 0)
PARAMS_SNAPSHOT_TEST(test_params_zc_command, true, 1)
PARAMS_SNAPSHOT_TEST(test_params_zc_subsystem, true, 2)
PARAMS_SNAPSHOT_TEST(test_params_zc_modes, true, 3)
PARAMS_SNAPSHOT_TEST(test_params_zc_env_array, true, 4)
PARAMS_SNAPSHOT_TEST(test_params_zc_env_name1, true, 5)
PARAMS_SNAPSHOT_TEST(test_params_zc_env_value1, true, 6)
PARAMS_SNAPSHOT_TEST(test_params_zc_env_name2, true, 7)
PARAMS_SNAPSHOT_TEST(test_params_zc_env_value2, true, 8)
#undef PARAMS_SNAPSHOT_TEST

static int
params_input_case(bool zc)
{
	struct params_snapshot_fixture fixture = {0};
	ASSERT_EQ(conn_setup_no_start(&fixture.ctx), 0);
	params_snapshot_own(&fixture);
	ASSERT_TRUE(storage_checkpoint(ST_PARAMS_EMPTY));
	ASSERT_EQ(dssh_session_set_timeout(fixture.ctx.client, 3000), 0);
	ASSERT_EQ(dssh_session_set_timeout(fixture.ctx.server, 3000), 0);
	ASSERT_EQ(dssh_session_start(fixture.ctx.client), 0);
	ASSERT_EQ(dssh_session_start(fixture.ctx.server), 0);
	ASSERT_EQ(dssh_chan_params_init(&fixture.params, DSSH_CHAN_EXEC), 0);
	ASSERT_EQ(dssh_chan_params_set_command(&fixture.params, snapshot_command), 0);
	ASSERT_TRUE(storage_checkpoint(ST_PARAMS_BUILT));
	struct dssh_chan_env env[] = {{NULL, NULL}};
	enum dssh_chan_type invalid_type = (enum dssh_chan_type)999;
	bool valid = true;
	params_snapshot_current = &fixture;
	accept_phase_test_hook = params_snapshot_tx;
	for (unsigned i = 0; i < 14; i++) {
		struct dssh_chan_params trial = fixture.params;
		switch (i) {
			case 0:
				trial.type = invalid_type;
				break;
			case 1:
				trial.command = NULL;
				break;
			case 2:
				trial.type = DSSH_CHAN_SUBSYSTEM;
				break;
			case 3:
				trial.flags = DSSH_PARAM_HAS_PTY;
				trial.term = NULL;
				break;
			case 4:
				trial.flags = UINT32_MAX;
				break;
			case 5:
				trial.type = DSSH_CHAN_SUBSYSTEM;
				trial.flags = DSSH_PARAM_ACCEPT_EARLY_DATA;
				break;
			case 6:
				trial.env_count = 1;
				break;
			case 7:
				trial.mode_count = 1;
				break;
			case 8:
				trial.env_capacity = 1;
				break;
			case 9:
				trial.mode_capacity = 1;
				break;
			case 10:
				trial.env_count = 1;
				trial.env_capacity = 1;
				trial.env = env;
				break;
			case 11:
				trial.env_count = 1;
				trial.env_capacity = 1;
				trial.env = env;
				env[0].name = fixture.params.term;
				break;
			case 12:
				trial.env_capacity = SIZE_MAX;
				break;
			case 13:
				trial.mode_capacity = SIZE_MAX;
				break;
		}
		dssh_test_alloc_fail_after(0);
		dssh_channel ch = zc ? dssh_chan_zc_open(fixture.ctx.client, &trial, params_snapshot_zc, NULL)
		    : dssh_chan_open(fixture.ctx.client, &trial);
		valid = valid && ch == NULL && dssh_test_alloc_count() == 0 && fixture.sends == 0;
		dssh_test_alloc_reset();
	}
	char *term = fixture.params.term;
	dssh_test_alloc_fail_after(0);
	int ret = dssh_chan_params_init(&fixture.params, invalid_type);
	valid = valid && ret == DSSH_ERROR_INVALID && dssh_test_alloc_count() == 0
	    && fixture.params.term == term;
	dssh_test_alloc_reset();
	accept_phase_test_hook = NULL;
	ASSERT_TRUE(valid);
	ASSERT_TRUE(storage_checkpoint(ST_PARAMS_INPUT));
	flow_finish(&fixture.flow);
	ASSERT_TRUE(fixture.retired);
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}

static int test_params_stream_validation(void)
{
	return params_input_case(false);
}

static int test_params_zc_validation(void)
{
	return params_input_case(true);
}
