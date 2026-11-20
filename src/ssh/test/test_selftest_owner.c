/* Exercise the actual self-test owner with private, caller-only controls.
 * Include its source once; library and provider calls retain their real paths. */
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "dssh_test.h"
#include "ssh-internal.h"
#include "selftest_metadata.h"
#include "dssh_test_internal.h"

static void owner_rekey_arm(dssh_session, uint64_t);
static unsigned owner_rekey_count(dssh_session);
static int owner_create(thrd_t *, thrd_start_t, void *);
static int owner_join(thrd_t, int *);
static int owner_callbacks(dssh_transport_io_cb, dssh_transport_io_cb,
    dssh_transport_rxline_cb, dssh_transport_extra_line_cb);
static int owner_cbdata(dssh_session, void *, void *, void *, void *);
static int owner_trust(dssh_session, dssh_hostkey_verify_cb, void *);
static dssh_session owner_session(bool, size_t);
static void owner_cleanup(dssh_session);
static int owner_close(int);
static int owner_param_init(struct dssh_chan_params *, enum dssh_chan_type);
static void owner_param_free(struct dssh_chan_params *);
static int owner_command(struct dssh_chan_params *, const char *);
static int owner_term(struct dssh_chan_params *, const char *);
static int owner_pty(struct dssh_chan_params *, bool);
static int owner_size(struct dssh_chan_params *, uint32_t, uint32_t, uint32_t, uint32_t);
static dssh_channel owner_channel_open(dssh_session, const struct dssh_chan_params *);
static dssh_channel owner_channel_accept(dssh_session, const struct dssh_chan_accept_cbs *, int);
static int owner_channel_close(dssh_channel, int64_t);
static int owner_poll(dssh_channel, int, int);
static int64_t owner_write(dssh_channel, int, const uint8_t *, size_t);
static int64_t owner_read(dssh_channel, int, uint8_t *, size_t);
static int owner_log(dssh_session, dssh_log_cb, void *);
static int owner_timeout(dssh_session, int);
static int owner_terminate(dssh_session, dssh_terminate_cb, void *);
static int owner_socket_poll(struct pollfd *, nfds_t, int);
static ssize_t owner_socket_send(int, const void *, size_t, int);
static ssize_t owner_socket_recv(int, void *, size_t, int);
static int owner_callback_unlock(mtx_t *);
static int owner_break(dssh_session, dssh_chan_break_cb, void *);
static int owner_env(struct dssh_chan_params *, const char *, const char *);
static void owner_wire_server_retired(void);
static void owner_constructor_begin(bool);
static void owner_constructor_end(dssh_channel);
static bool owner_metadata_snapshot(const uint8_t *, size_t, uint8_t *, size_t, size_t *);
static int owner_metadata_clock(clockid_t, struct timespec *);
static int owner_metadata_sleep(const struct timespec *, struct timespec *);
static bool owner_metadata_runner_clock(struct timespec *);
static int64_t owner_metadata_read(uint8_t *, size_t, int64_t);
static thrd_start_t owner_metadata_worker_function(thrd_start_t);
static const char *owner_metadata_kex(dssh_session);
static const char *owner_metadata_key(dssh_session);
static const char *owner_metadata_enc(dssh_session, enum dssh_direction);
static const char *owner_metadata_mac(dssh_session, enum dssh_direction);
static const char *owner_metadata_comp(dssh_session, enum dssh_direction);

/* Capture the existing native/test redirections before overriding only calls
 * in the included fixture. No library demux create/join is intercepted. */
static int native_create(thrd_t *t, thrd_start_t f, void *a)
{
	return thrd_create(t, f, a);
}
static int native_join(thrd_t t, int *r)
{
	return thrd_join(t, r);
}
static int native_unlock(mtx_t *mutex)
{
	return mtx_unlock(mutex);
}

#undef thrd_create
#undef thrd_join
#define thrd_create owner_create
#define thrd_join owner_join
#define dssh_transport_set_callbacks owner_callbacks
#define dssh_session_set_cbdata owner_cbdata
#define dssh_session_set_hostkey_verify_cb owner_trust
#define dssh_session_init owner_session
#define dssh_session_cleanup owner_cleanup
#define close owner_close
#define dssh_chan_params_init owner_param_init
#define dssh_chan_params_free owner_param_free
#define dssh_chan_params_set_command owner_command
#define dssh_chan_params_set_term owner_term
#define dssh_chan_params_set_pty owner_pty
#define dssh_chan_params_set_size owner_size
#define dssh_chan_open owner_channel_open
#define dssh_chan_accept owner_channel_accept
#define dssh_chan_close owner_channel_close
#define dssh_chan_poll owner_poll
#define dssh_chan_write owner_write
#define dssh_chan_read owner_read
#define dssh_session_set_log_cb owner_log
#define dssh_session_set_timeout owner_timeout
#define dssh_session_set_terminate_cb owner_terminate
#define dssh_session_set_break_cb owner_break
#define dssh_chan_params_add_env owner_env
#define selftest_snapshot_id owner_metadata_snapshot
#define clock_gettime owner_metadata_clock
#define thrd_sleep owner_metadata_sleep
#define dssh_test_runner_clock owner_metadata_runner_clock
#define dssh_transport_get_kex_name owner_metadata_kex
#define dssh_transport_get_hostkey_name owner_metadata_key
#define dssh_transport_get_enc_name owner_metadata_enc
#define dssh_transport_get_mac_name owner_metadata_mac
#define dssh_transport_get_comp_name owner_metadata_comp
#define poll owner_socket_poll
#define send owner_socket_send
#define recv owner_socket_recv
#undef mtx_unlock
#define mtx_unlock owner_callback_unlock
#define dssh_test_arm_rx_bytes_before_count owner_rekey_arm
#define dssh_test_rekeys_completed owner_rekey_count
#define main selftest_suite_main
#include "test_selftest.c"
#undef main
#undef dssh_test_arm_rx_bytes_before_count
#undef dssh_test_rekeys_completed
#undef selftest_snapshot_id
#undef clock_gettime
#undef thrd_sleep
#undef dssh_test_runner_clock
#undef dssh_transport_get_kex_name
#undef dssh_transport_get_hostkey_name
#undef dssh_transport_get_enc_name
#undef dssh_transport_get_mac_name
#undef dssh_transport_get_comp_name
#undef dssh_session_set_break_cb
#undef dssh_chan_params_add_env
#undef mtx_unlock
#undef poll
#undef send
#undef recv
#undef dssh_session_set_log_cb
#undef dssh_session_set_timeout
#undef dssh_session_set_terminate_cb
#undef close
#undef dssh_chan_params_init
#undef dssh_chan_params_free
#undef dssh_chan_params_set_command
#undef dssh_chan_params_set_term
#undef dssh_chan_params_set_pty
#undef dssh_chan_params_set_size
#undef dssh_chan_open
#undef dssh_chan_accept
#undef dssh_chan_close
#undef dssh_chan_poll
#undef dssh_chan_write
#undef dssh_chan_read
#undef dssh_session_cleanup
#undef dssh_session_init
#undef dssh_session_set_hostkey_verify_cb
#undef dssh_session_set_cbdata
#undef dssh_transport_set_callbacks
#undef thrd_join
#undef thrd_create

static const char *owner_mode;
static unsigned owner_created;
static unsigned owner_joined;
static unsigned owner_attempts;
static unsigned owner_join_attempts;
static unsigned owner_setters;
static unsigned owner_sessions;
static unsigned owner_released;
static unsigned owner_closed;
static unsigned owner_param_calls;
static unsigned owner_param_native;
static unsigned owner_param_frees;
static unsigned owner_open_attempts;
static bool owner_param_clean = true;
static bool owner_param_fired;
static atomic_uint owner_channels_created;
static atomic_uint owner_channels_closed;
static atomic_bool owner_channel_ready;
static _Thread_local bool owner_main;
static bool owner_io_armed;
static bool owner_io_fired;
static unsigned owner_write_calls;
static unsigned owner_read_calls;
static bool owner_standalone_fired;
static bool owner_standalone_borrow_valid;
static struct log_test_state *owner_log_borrow;
static dssh_terminate_cb owner_terminate_borrow;
static void *owner_terminate_data;
static unsigned owner_terminate_calls;

#define owner_note(...) do { \
	if (fprintf(stderr, __VA_ARGS__) < 0 || fflush(stderr) != 0) \
		_Exit(7); \
} while (0)

/* The original assertion hook tears down the registered owner while its
 * callback storage is still live. Keep no owner or stack borrow afterward. */
static int
owner_standalone_failure(const char *mode, int ret)
{
	if (ret != 0 || !owner_main || strcmp(owner_mode, mode) != 0)
		return ret;
	if (owner_standalone_fired)
		abort();
	owner_standalone_fired = true;
	owner_note("SELFTEST_STANDALONE_FAILURE mode=%s native=0 reported=%d\n",
	    owner_mode, DSSH_ERROR_INIT);
	return DSSH_ERROR_INIT;
}

static int
owner_log(dssh_session sess, dssh_log_cb callback, void *data)
{
	int ret = dssh_session_set_log_cb(sess, callback, data);
	if (ret == 0 && owner_main && strcmp(owner_mode, "standalone-log") == 0) {
		if (callback != log_test_cb || data == NULL)
			abort();
		owner_log_borrow = data;
	}
	return owner_standalone_failure("standalone-log", ret);
}

static int
owner_timeout(dssh_session sess, int timeout)
{
	int ret = dssh_session_set_timeout(sess, timeout);
	if (sess == NULL)
		return ret;
	if (ret == 0 && owner_main && strcmp(owner_mode, "standalone-clock") == 0)
		owner_standalone_borrow_valid = true;
	return owner_standalone_failure("standalone-clock", ret);
}

static void
owner_terminate_callback(dssh_session sess, void *data)
{
	dssh_terminate_cb callback = owner_terminate_borrow;
	bool matches = data != NULL && data == owner_terminate_data;
	owner_terminate_borrow = NULL;
	owner_terminate_data = NULL;
	if (!matches || callback != terminate_test_cb || owner_terminate_calls != 0)
		abort();
	int *count = data;
	bool valid = *count == 0;
	callback(sess, data);
	owner_terminate_calls++;
	owner_standalone_borrow_valid = valid && *count == 1;
}

static int
owner_terminate(dssh_session sess, dssh_terminate_cb callback, void *data)
{
	if (!owner_main || strcmp(owner_mode, "standalone-terminate") != 0)
		return dssh_session_set_terminate_cb(sess, callback, data);
	if (callback != terminate_test_cb || data == NULL)
		abort();
	int ret = dssh_session_set_terminate_cb(sess, owner_terminate_callback, data);
	if (ret == 0) {
		owner_terminate_borrow = callback;
		owner_terminate_data = data;
	}
	return owner_standalone_failure("standalone-terminate", ret);
}

static bool
owner_selected(const char *prefix, unsigned index, const char *suffix)
{
	size_t length = strlen(prefix);
	return index < 6 && strncmp(owner_mode, prefix, length) == 0
	    && owner_mode[length] == '0' + index
	    && strcmp(owner_mode + length + 1, suffix) == 0;
}

static bool
owner_parameter_fault(unsigned index, const char *suffix)
{
	const char *prefix = strncmp(owner_mode, "builder-shell-", 14) == 0
	    ? "builder-shell-" : "builder-exec-";
	if (!owner_selected(prefix, index, suffix))
		return false;
	owner_param_fired = true;
	owner_note("SELFTEST_PARAMETER_FAILURE mode=%s index=%u phase=%s\n",
	    owner_mode, index, suffix);
	return true;
}

static unsigned
owner_parameter_index(void)
{
	if (owner_param_calls >= 3)
		abort();
	return owner_param_calls++;
}

static int
owner_parameter_finish(unsigned index, int ret)
{
	if (ret == 0 && owner_parameter_fault(index, "-after"))
		return DSSH_ERROR_ALLOC;
	return ret;
}

static int
owner_param_init(struct dssh_chan_params *params, enum dssh_chan_type type)
{
	unsigned index = owner_parameter_index();
	if (owner_parameter_fault(index, "-before"))
		return DSSH_ERROR_ALLOC;
	owner_param_native++;
	return owner_parameter_finish(index, dssh_chan_params_init(params, type));
}

static int
owner_command(struct dssh_chan_params *params, const char *command)
{
	unsigned index = owner_parameter_index();
	if (owner_parameter_fault(index, "-before"))
		return DSSH_ERROR_ALLOC;
	owner_param_native++;
	return owner_parameter_finish(index, dssh_chan_params_set_command(params, command));
}

static int
owner_term(struct dssh_chan_params *params, const char *term)
{
	unsigned index = owner_parameter_index();
	if (owner_parameter_fault(index, "-before"))
		return DSSH_ERROR_ALLOC;
	owner_param_native++;
	return owner_parameter_finish(index, dssh_chan_params_set_term(params, term));
}

static int
owner_pty(struct dssh_chan_params *params, bool enable)
{
	unsigned index = owner_parameter_index();
	if (owner_parameter_fault(index, "-before"))
		return DSSH_ERROR_ALLOC;
	owner_param_native++;
	return owner_parameter_finish(index, dssh_chan_params_set_pty(params, enable));
}

static int
owner_size(struct dssh_chan_params *params, uint32_t cols, uint32_t rows,
    uint32_t wpx, uint32_t hpx)
{
	unsigned index = owner_parameter_index();
	if (owner_parameter_fault(index, "-before"))
		return DSSH_ERROR_ALLOC;
	owner_param_native++;
	return owner_parameter_finish(index, dssh_chan_params_set_size(params, cols, rows, wpx, hpx));
}

static bool
owner_parameters_empty(const struct dssh_chan_params *params)
{
	return params->term == NULL && params->command == NULL && params->subsystem == NULL
	    && params->env == NULL && params->env_count == 0 && params->env_capacity == 0
	    && params->modes == NULL && params->mode_count == 0 && params->mode_capacity == 0;
}

static void
owner_param_free(struct dssh_chan_params *params)
{
	dssh_chan_params_free(params);
	if (owner_param_frees >= 2)
		abort();
	owner_param_frees++;
	owner_param_clean &= owner_parameters_empty(params);
}

static dssh_channel
owner_channel_open(dssh_session session, const struct dssh_chan_params *params)
{
	if (owner_open_attempts >= 1)
		abort();
	owner_open_attempts++;
	owner_constructor_begin(true);
	dssh_channel ch = dssh_chan_open(session, params);
	owner_constructor_end(ch);
	if (ch != NULL)
		atomic_fetch_add(&owner_channels_created, 1);
	return ch;
}

static dssh_channel
owner_channel_accept(dssh_session session, const struct dssh_chan_accept_cbs *cbs, int timeout_ms)
{
	owner_constructor_begin(false);
	dssh_channel ch = dssh_chan_accept(session, cbs, timeout_ms);
	owner_constructor_end(ch);
	if (ch != NULL)
		atomic_fetch_add(&owner_channels_created, 1);
	return ch;
}

static int
owner_channel_close(dssh_channel ch, int64_t code)
{
	int ret = dssh_chan_close(ch, code);
	if (ret == 0 || ret == DSSH_ERROR_TERMINATED)
		atomic_fetch_add(&owner_channels_closed, 1);
	return ret;
}

static int
owner_poll(dssh_channel ch, int events, int timeout_ms)
{
	if (owner_main && owner_io_armed && !owner_io_fired) {
		const char *prefix = events == DSSH_POLL_WRITE ? "stream-write-poll-" : "stream-read-poll-";
		size_t length = strlen(prefix);
		if (strncmp(owner_mode, prefix, length) == 0) {
			const char *kind = owner_mode + length;
			owner_io_fired = true;
			if (strcmp(kind, "zero") == 0)
				return 0;
			if (strcmp(kind, "negative") == 0)
				return DSSH_ERROR_INVALID;
			if (strcmp(kind, "wrong") == 0)
				return events == DSSH_POLL_WRITE ? DSSH_POLL_READ : DSSH_POLL_WRITE;
			abort();
		}
	}
	return dssh_chan_poll(ch, events, timeout_ms);
}

static int64_t
owner_write(dssh_channel ch, int stream, const uint8_t *buf, size_t length)
{
	if (!owner_main || !owner_io_armed)
		return dssh_chan_write(ch, stream, buf, length);
	if (owner_write_calls == UINT_MAX)
		abort();
	owner_write_calls++;
	if (!owner_io_fired && strncmp(owner_mode, "stream-write-", 13) == 0) {
		owner_io_fired = true;
		if (strcmp(owner_mode, "stream-write-zero") == 0)
			return 0;
		if (strcmp(owner_mode, "stream-write-negative") == 0)
			return DSSH_ERROR_TERMINATED;
		if (strcmp(owner_mode, "stream-write-oversize") == 0)
			return INT64_MAX;
		abort();
	}
	if (strcmp(owner_mode, "stream-partial") == 0 && length > 3) {
		length = 3;
		owner_io_fired = true;
	}
	return dssh_chan_write(ch, stream, buf, length);
}

static int64_t
owner_read(dssh_channel ch, int stream, uint8_t *buf, size_t length)
{
	if (!owner_main || !owner_io_armed)
		return owner_metadata_read(buf, length, dssh_chan_read(ch, stream, buf, length));
	if (owner_read_calls == UINT_MAX)
		abort();
	owner_read_calls++;
	if (!owner_io_fired && strncmp(owner_mode, "stream-read-", 12) == 0) {
		owner_io_fired = true;
		if (strcmp(owner_mode, "stream-read-eof") == 0)
			return 0;
		if (strcmp(owner_mode, "stream-read-negative") == 0)
			return DSSH_ERROR_TERMINATED;
		if (strcmp(owner_mode, "stream-read-oversize") == 0)
			return INT64_MAX;
		abort();
	}
	if (strcmp(owner_mode, "stream-partial") == 0 && length > 3) {
		length = 3;
		owner_io_fired = true;
	}
	int64_t ret = dssh_chan_read(ch, stream, buf, length);
	if (!owner_io_fired && strcmp(owner_mode, "stream-corrupt") == 0
	    && ret > 0 && length != 0 && buf != NULL) {
		buf[0] ^= 1;
		owner_io_fired = true;
	}
	return ret;
}

static int
owner_create(thrd_t *thread, thrd_start_t function, void *argument)
{
	if (owner_attempts >= 5)
		abort();
	unsigned index = owner_attempts++;
	if (owner_selected("create-", index, "")) {
		owner_note("SELFTEST_CREATE_FAILURE index=%u\n", index);
		return thrd_error;
	}
	int ret = native_create(thread, owner_metadata_worker_function(function), argument);
	if (ret == thrd_success)
		owner_created++;
	return ret;
}

static int
owner_join(thrd_t thread, int *result)
{
	if (owner_join_attempts >= 5)
		abort();
	unsigned index = owner_join_attempts++;
	if (owner_selected("join-", index, "-retained")) {
		owner_note("SELFTEST_JOIN_FAILURE index=%u retained=1\n", index);
		return thrd_error;
	}
	int ret = native_join(thread, result);
	owner_note("SELFTEST_NATIVE_JOIN index=%u success=%u\n", index,
	    ret == thrd_success ? 1U : 0U);
	if (ret == thrd_success) {
		owner_joined++;
		if (owner_selected("join-", index, "-released")) {
			owner_note("SELFTEST_JOIN_FAILURE index=%u retained=0\n", index);
			return thrd_error;
		}
	}
	return ret;
}

static bool
owner_setter_refusal(unsigned index, const char *suffix)
{
	if (!owner_selected("setter-", index, suffix))
		return false;
	owner_note("SELFTEST_SETTER_FAILURE index=%u phase=%s\n", index, suffix);
	return true;
}

static int
owner_callbacks(dssh_transport_io_cb tx, dssh_transport_io_cb rx,
    dssh_transport_rxline_cb line, dssh_transport_extra_line_cb extra)
{
	unsigned index = owner_setters++;
	if (owner_setter_refusal(index, "-before"))
		return DSSH_ERROR_INIT;
	int ret = dssh_transport_set_callbacks(tx, rx, line, extra);
	if (ret == 0 && owner_setter_refusal(index, "-after"))
		return DSSH_ERROR_INIT;
	return ret;
}

static int
owner_cbdata(dssh_session sess, void *tx, void *rx, void *line, void *extra)
{
	unsigned index = owner_setters++;
	if (owner_setter_refusal(index, "-before"))
		return DSSH_ERROR_INIT;
	int ret = dssh_session_set_cbdata(sess, tx, rx, line, extra);
	if (ret == 0 && owner_setter_refusal(index, "-after"))
		return DSSH_ERROR_INIT;
	return ret;
}

static int
owner_trust(dssh_session sess, dssh_hostkey_verify_cb callback, void *data)
{
	unsigned index = owner_setters++;
	if (owner_setter_refusal(index, "-before"))
		return DSSH_ERROR_INIT;
	int ret = dssh_session_set_hostkey_verify_cb(sess, callback, data);
	if (ret == 0 && owner_setter_refusal(index, "-after"))
		return DSSH_ERROR_INIT;
	return ret;
}

static dssh_session
owner_session(bool client, size_t extra)
{
	dssh_session sess = dssh_session_init(client, extra);
	if (sess != NULL) {
		if (owner_sessions >= 2)
			abort();
		owner_sessions++;
	}
	return sess;
}

static void
owner_cleanup(dssh_session sess)
{
	if (owner_joined != owner_created
	    || atomic_load(&owner_channels_created) != atomic_load(&owner_channels_closed)
	    || !owner_param_clean) {
		owner_note("SELFTEST_PREMATURE_RETIRE\n");
		abort();
	}
	bool owned = sess != NULL;
	bool server = owned && !sess->trans.client;
	struct log_test_state *log = owned ? owner_log_borrow : NULL;
	if (owned)
		owner_log_borrow = NULL;
	bool log_valid = log != NULL && log->valid;
	dssh_session_cleanup(sess);
	if (server)
		owner_wire_server_retired();
	if (log != NULL)
		owner_standalone_borrow_valid = log_valid && log->valid;
	if (owned) {
		if (owner_released >= owner_sessions)
			abort();
		owner_released++;
	}
}

static int
owner_close(int fd)
{
	if (owner_joined != owner_created || owner_released != owner_sessions) {
		owner_note("SELFTEST_PREMATURE_RETIRE\n");
		abort();
	}
	int ret = close(fd);
	if (ret == 0) {
		if (owner_closed >= 2)
			abort();
		owner_closed++;
	}
	return ret;
}

static bool
owner_retired(const struct selftest_ctx *ctx)
{
	return !ctx->registered && g_active_ctx == NULL
	    && !ctx->setup_thread_active[0] && !ctx->setup_thread_active[1]
	    && !ctx->server_thread_active && ctx->client == NULL && ctx->server == NULL
	    && ctx->client_fd == -1 && ctx->server_fd == -1
	    && owner_joined == owner_created && owner_released == owner_sessions
	    && owner_closed == 2 && !ctx->params_active && owner_parameters_empty(&ctx->params)
	    && ctx->client_channel == NULL && ctx->server_channel == NULL
	    && owner_param_clean
	    && atomic_load(&owner_channels_created) == atomic_load(&owner_channels_closed);
}

static int
owner_assertion(void)
{
	ASSERT_TRUE(false);
	return TEST_PASS;
}

/* Remains blocked until the assertion hook cancels its borrowed session. */
static int
owner_body(void *argument)
{
	struct selftest_ctx *ctx = argument;
	uint8_t byte;
	return socket_rx(&byte, sizeof(byte), ctx->server, &ctx->server_fd);
}

static int
owner_finish(struct selftest_ctx *ctx, bool valid)
{
	selftest_cleanup(ctx);
	selftest_cleanup(ctx);
	valid &= owner_retired(ctx);
	owner_note("SELFTEST_RESOURCES mode=%s params_calls=%u params_native=%u params_frees=%u params_clean=%u open_attempts=%u channels=%u closed=%u\n",
	    owner_mode, owner_param_calls, owner_param_native, owner_param_frees,
	    owner_param_clean ? 1U : 0U, owner_open_attempts,
	    atomic_load(&owner_channels_created), atomic_load(&owner_channels_closed));
	owner_note("SELFTEST_OWNER mode=%s created=%u joined=%u sessions=%u released=%u closed=%u valid=%u\n",
	    owner_mode, owner_created, owner_joined, owner_sessions, owner_released,
	    owner_closed, valid ? 1U : 0U);
	return valid ? 0 : 2;
}

static int
owner_channel_body(void *argument)
{
	struct selftest_ctx *ctx = argument;
	dssh_channel ch = selftest_accept(ctx, &selftest_accept_cbs, 30000);
	atomic_store(&owner_channel_ready, true);
	if (ch == NULL)
		return -1;
	bool complete = selftest_echo(ch);
	int ret = selftest_channel_close(ctx, false, ch, 0);
	return complete && ret == 0 ? 0 : -1;
}

#include "selftest_socket_controls.h"
#include "selftest_wire_controls.h"
#include "selftest_constructor_controls.h"
#include "selftest_metadata_controls.h"
#include "selftest_rekey_controls.h"

static bool
owner_extent_boundaries(void)
{
	static const struct {
		int64_t value;
		size_t capacity;
		bool accepted;
		size_t expected;
	} cases[] = {
		{ -1, 8, false, 0 }, { 0, 0, true, 0 }, { 0, 8, true, 0 },
		{ 1, 0, false, 0 }, { 1, 1, true, 1 }, { 4096, 4095, false, 0 },
		{ 4096, 4096, true, 4096 }, { 4097, 4096, false, 0 },
		{ INT64_MAX, 4096, false, 0 },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		size_t count = 42;
		bool accepted = selftest_io_count(cases[i].value, cases[i].capacity, &count);
		if (accepted != cases[i].accepted
		    || count != (accepted ? cases[i].expected : 42))
			return false;
	}
	owner_note("SELFTEST_EXTENTS cases=9 valid=1\n");
	return true;
}

/* Return 3 only for the older setup/worker-only modes below. */
static int
owner_extra_control(struct selftest_ctx *ctx)
{
	bool builder = strncmp(owner_mode, "builder-", 8) == 0;
	bool parameters = strcmp(owner_mode, "assert-params") == 0;
	bool extents = strcmp(owner_mode, "extent-boundaries") == 0;
	if (builder || parameters || extents) {
		if (selftest_prepare(ctx) != 0)
			return 2;
		if (extents)
			return owner_finish(ctx, owner_extent_boundaries());
		if (builder) {
			dssh_channel ch = strncmp(owner_mode, "builder-shell-", 14) == 0
			    ? open_shell(ctx, "xterm", 80, 24) : open_exec(ctx, "command");
			return owner_finish(ctx, ch == NULL && owner_param_fired
			    && owner_open_attempts == 0 && !ctx->params_active);
		}
		if (selftest_params_init(ctx, DSSH_CHAN_EXEC) != 0
		    || dssh_chan_params_set_command(&ctx->params, "pending-command") != 0
		    || dssh_chan_params_set_term(&ctx->params, "pending-term") != 0
		    || dssh_chan_params_add_env(&ctx->params, "LANG", "C") != 0
		    || dssh_chan_params_set_mode(&ctx->params, 1, 3) != 0)
			return owner_finish(ctx, false);
		bool allocated = ctx->params.command != NULL && ctx->params.term != NULL
		    && ctx->params.env_count == 1 && ctx->params.mode_count == 1;
		int result = owner_assertion();
		return owner_finish(ctx, allocated && result == TEST_FAIL && owner_retired(ctx));
	}
	bool channel = strcmp(owner_mode, "assert-channel") == 0;
	bool tail = strcmp(owner_mode, "pattern-tail") == 0;
	bool stream = strncmp(owner_mode, "stream-", 7) == 0;
	if (!channel && !tail && !stream)
		return 3;
	if (selftest_setup(ctx) != 0)
		return 2;
	if (dssh_session_start(ctx->client) != 0 || dssh_session_start(ctx->server) != 0
	    || selftest_start_thread(ctx, owner_channel_body, ctx) != 0)
		return owner_finish(ctx, false);
	dssh_channel ch = open_exec(ctx, "stream-control");
	if (ch == NULL || !dssh_test_wait_bool(&owner_channel_ready, true)
	    || atomic_load(&owner_channels_created) != 2)
		return owner_finish(ctx, false);
	if (channel) {
		int result = owner_assertion();
		return owner_finish(ctx, result == TEST_FAIL && owner_retired(ctx));
	}
	owner_io_armed = true;
	uint32_t completed = 0;
	bool complete;
	if (tail || strcmp(owner_mode, "stream-corrupt") == 0) {
		uint32_t total = tail ? UINT32_C(4097) : UINT32_C(65536);
		complete = selftest_pattern_roundtrip(ch, total, &completed);
	}
	else {
		const uint8_t payload[] = "partial-transfer-payload";
		uint8_t received[sizeof(payload) - 1];
		complete = selftest_write_all(ch, payload, sizeof(payload) - 1, 5000)
		    && selftest_read_exact(ch, received, sizeof(received), 5000)
		    && memcmp(payload, received, sizeof(received)) == 0;
		if (complete)
			completed = sizeof(payload) - 1;
	}
	owner_io_armed = false;
	bool expected_success = tail || strcmp(owner_mode, "stream-partial") == 0;
	bool valid = expected_success ? complete : !complete && owner_io_fired;
	owner_note("SELFTEST_STREAM mode=%s fired=%u writes=%u reads=%u completed=%" PRIu32 " result=%u\n",
	    owner_mode, owner_io_fired ? 1U : 0U, owner_write_calls, owner_read_calls,
	    completed, complete ? 1U : 0U);
	return owner_finish(ctx, valid);
}

int
main(int argc, char **argv)
{
	if (argc != 2 || signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return 2;
	owner_mode = argv[1];
	owner_main = true;
	atomic_init(&owner_channels_created, 0);
	atomic_init(&owner_channels_closed, 0);
	atomic_init(&owner_channel_ready, false);
	if (strncmp(owner_mode, "rekey-", 6) == 0)
		return owner_rekey_control();
	if (strncmp(owner_mode, "metadata-", 9) == 0)
		return owner_metadata_control();
	if (strncmp(owner_mode, "constructor-", 12) == 0)
		return owner_constructor_control();
	if (strncmp(owner_mode, "wire-", 5) == 0)
		return owner_wire_control();
	if (strncmp(owner_mode, "socket-", 7) == 0
	    || strncmp(owner_mode, "callback-unlock-", 16) == 0)
		return owner_socket_control();
	if (strncmp(owner_mode, "standalone-", 11) == 0) {
		int result;
		if (strcmp(owner_mode, "standalone-log") == 0)
			result = test_log_api();
		else if (strcmp(owner_mode, "standalone-clock") == 0)
			result = test_set_timeout();
		else if (strcmp(owner_mode, "standalone-terminate") == 0)
			result = test_set_terminate_cb();
		else
			return 2;
		bool valid = result == TEST_FAIL && owner_standalone_fired
		    && owner_standalone_borrow_valid && g_active_ctx == NULL
		    && owner_log_borrow == NULL && owner_terminate_borrow == NULL
		    && owner_terminate_data == NULL
		    && owner_sessions == 1 && owner_released == 1 && owner_closed == 0
		    && owner_attempts == 0 && owner_created == 0 && owner_joined == 0
		    && owner_join_attempts == 0 && owner_param_calls == 0
		    && owner_open_attempts == 0 && owner_param_clean
		    && atomic_load(&owner_channels_created) == 0
		    && atomic_load(&owner_channels_closed) == 0
		    && owner_terminate_calls == (strcmp(owner_mode, "standalone-terminate") == 0 ? 1U : 0U);
		owner_note("SELFTEST_STANDALONE mode=%s fired=%u sessions=%u released=%u fds=%u borrowed=%u active=%u valid=%u\n",
		    owner_mode, owner_standalone_fired ? 1U : 0U, owner_sessions,
		    owner_released, owner_closed, owner_standalone_borrow_valid ? 1U : 0U,
		    g_active_ctx != NULL ? 1U : 0U, valid ? 1U : 0U);
		return valid ? 0 : 2;
	}
	struct selftest_ctx ctx;
	int extra = owner_extra_control(&ctx);
	if (extra != 3)
		return extra;
	if (strncmp(owner_mode, "assert-hs-", 10) == 0
	    || strncmp(owner_mode, "assert-auth-", 12) == 0) {
		bool auth = strncmp(owner_mode, "assert-auth-", 12) == 0;
		const char *prefix = auth ? "assert-auth-" : "assert-hs-";
		unsigned count;
		for (count = 0; count < 3; count++) {
			if (owner_selected(prefix, count, ""))
				break;
		}
		if (count >= 3 || selftest_prepare(&ctx) != 0)
			return 2;
		if (auth && (selftest_run_setup_pair(&ctx, client_handshake_thread,
		        server_handshake_thread) != 0 || ctx.client_hs_result != 0
		        || ctx.server_hs_result != 0))
			return owner_finish(&ctx, false);
		thrd_start_t operations[] = { auth ? client_auth_thread : client_handshake_thread,
		    auth ? server_auth_thread : server_handshake_thread };
		for (unsigned i = 0; i < count; i++) {
			if (selftest_start_setup_thread(&ctx, i, operations[i]) != 0)
				return owner_finish(&ctx, false);
		}
		int result = owner_assertion();
		return owner_finish(&ctx, result == TEST_FAIL && owner_retired(&ctx));
	}
	int ret = selftest_setup(&ctx);
	if (ret != 0)
		return owner_finish(&ctx, strncmp(owner_mode, "setter-", 7) == 0
		    || strncmp(owner_mode, "create-", 7) == 0);
	if (strncmp(owner_mode, "setter-", 7) == 0)
		return owner_finish(&ctx, false);
	ret = selftest_start_thread(&ctx, owner_body, &ctx);
	if (strcmp(owner_mode, "assert-body") == 0) {
		if (ret != 0)
			return owner_finish(&ctx, false);
		int result = owner_assertion();
		return owner_finish(&ctx, result == TEST_FAIL && owner_retired(&ctx));
	}
	bool valid = owner_selected("create-", 4, "") ? ret < 0 : ret == 0;
	if (ret == 0) {
		/* A live handle may not be overwritten by a second start. */
		valid &= selftest_start_thread(&ctx, owner_body, &ctx) < 0;
		selftest_cancel(&ctx);
		selftest_join(&ctx.server_thread, &ctx.server_thread_active);
		selftest_join(&ctx.server_thread, &ctx.server_thread_active);
	}
	return owner_finish(&ctx, valid);
}
