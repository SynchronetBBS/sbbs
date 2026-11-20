#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "ssh-internal.h"

static _Thread_local struct dssh_session_scope *active_session_scope;
static _Thread_local bool result_callback_active;

#ifdef DSSH_TESTING
DSSH_PRIVATE int
dssh_test_unlock_boundary(mtx_t *mutex)
{
	return dssh_mtx_unlock(mutex);
}
#endif

DSSH_PRIVATE bool
dssh_result_callback_active(void)
{
	return result_callback_active;
}

DSSH_PUBLIC void
dssh_result_release(struct dssh_result_lease *lease)
{
	if (lease == NULL)
		return;
	struct dssh_result_lease saved = *lease;
	lease->release = NULL;
	lease->owner = NULL;
	if (saved.release != NULL) {
		bool previous = result_callback_active;
		result_callback_active = true;
		saved.release(saved.owner);
		result_callback_active = previous;
	}
}

DSSH_PRIVATE void
session_scope_enter(struct dssh_session_scope *scope, struct dssh_session_s *sess)
{
	scope->sess = sess;
	scope->previous = active_session_scope;
	scope->termination_callback = false;
	active_session_scope = scope;
}

DSSH_PRIVATE void
session_scope_leave(struct dssh_session_scope *scope)
{
	active_session_scope = scope->previous;
}

DSSH_PRIVATE bool
session_scope_active(struct dssh_session_s *sess)
{
	for (struct dssh_session_scope *scope = active_session_scope; scope != NULL; scope = scope->previous) {
		if (scope->sess == sess)
			return true;
	}
	return false;
}

DSSH_PRIVATE bool
session_termination_callback_active(struct dssh_session_s *sess)
{
	for (struct dssh_session_scope *scope = active_session_scope; scope != NULL; scope = scope->previous) {
		if (scope->sess == sess && scope->termination_callback)
			return true;
	}
	return false;
}

/* Public handshake/authentication entries are serialized before start. An
 * outer operation or initialized connection layer excludes another receiver,
 * including a stopped worker whose resources still await the owned join. */
DSSH_PRIVATE bool
session_receive_api_conflict(struct dssh_session_s *sess)
{
	return session_scope_active(sess) || sess->conn_initialized;
}

/* I/O callback validation is performed by the public lifecycle wrappers. */
DSSH_PRIVATE bool
session_lifecycle_begin(struct dssh_session_s *sess)
{
	if (session_scope_active(sess) || session_on_demux_thread(sess) || tx_owner_is_current(sess)) {
		session_set_terminate(sess);
		return false;
	}
	return !atomic_exchange_explicit(&sess->lifecycle_busy, true, memory_order_acq_rel);
}

/* Use the timespec UTC seconds representation consistently, rather than
 * assuming the separate calendar time() representation is linear seconds. */
DSSH_PRIVATE int
dssh_clock_read(struct timespec *ts)
{
	struct timespec now;
	if (timespec_get(&now, TIME_UTC) != TIME_UTC)
		return DSSH_ERROR_INIT;
	long double seconds = (long double)now.tv_sec;
	if (now.tv_nsec < 0 || now.tv_nsec >= 1000000000L || !isfinite(seconds))
		return DSSH_ERROR_INVALID;
	*ts = now;
	return 0;
}

/* Convert a positive timeout, preserving the caller's output on failure. */
DSSH_PRIVATE int
dssh_deadline_from_ms(struct timespec *ts, intmax_t timeout_ms)
{
	if (timeout_ms <= 0)
		return DSSH_ERROR_INVALID;
	struct timespec now;

	int ret = dssh_clock_read(&now);
	if (ret < 0)
		return ret;
	if (!dssh_timespec_add_ms(&now, timeout_ms))
		return DSSH_ERROR_INVALID;
	*ts = now;
	return 0;
}

DSSH_PUBLIC const char *
dssh_strerror(int error_code)
{
	switch (error_code) {
		case DSSH_ERROR_NONE:
			return "No error";
		case DSSH_ERROR_PARSE:
			return "Malformed packet or field";
		case DSSH_ERROR_INVALID:
			return "Invalid value";
		case DSSH_ERROR_ALLOC:
			return "Memory allocation failure";
		case DSSH_ERROR_INIT:
			return "Initialization or cryptographic failure";
		case DSSH_ERROR_TERMINATED:
			return "Session terminated";
		case DSSH_ERROR_TOOLATE:
			return "Operation is no longer allowed";
		case DSSH_ERROR_TOOMANY:
			return "Too many registered algorithms";
		case DSSH_ERROR_TOOLONG:
			return "Data exceeds a buffer or protocol limit";
		case DSSH_ERROR_MUST_BE_NULL:
			return "Linked-list next pointer must be NULL";
		case DSSH_ERROR_NOMORE:
			return "No more items are available";
		case DSSH_ERROR_REKEY_NEEDED:
			return "Key exchange is required before continuing";
		case DSSH_ERROR_REJECTED:
			return "Channel operation rejected";
		case DSSH_ERROR_TIMEOUT:
			return "Operation timed out";
		default:
			return "Unknown DeuceSSH error";
	}
}

DSSH_PRIVATE bool
dssh_log_enabled(struct dssh_session_s *sess, dssh_log_level level)
{
	if (sess == NULL || sess->log_cb == NULL)
		return false;
	return level <= atomic_load_explicit(&sess->log_level, memory_order_relaxed);
}

static void
queue_log_forward(struct dssh_session_s *sess, bool always_display, const uint8_t *message, size_t message_len)
{
	struct dssh_log_forward_queue *queue = sess->log_forward;

	if (queue == NULL || sess->terminate || sess->log_mirror_suppressed)
		return;
	if (mtx_trylock(&queue->mtx) != thrd_success)
		return;
	if (queue->count < DSSH_LOG_FORWARD_MAX) {
		size_t idx = (queue->head + queue->count) % DSSH_LOG_FORWARD_MAX;
		struct dssh_log_forward_entry *entry = &queue->entries[idx];

		entry->always_display = always_display;
		entry->message_len    = message_len;
		if (message_len > 0)
			memcpy(entry->message, message, message_len);
		queue->count++;
	}
	dssh_mtx_unlock(&queue->mtx);
}

static void
log_emit(struct dssh_session_s *sess, dssh_log_level level, dssh_log_source source, int error_code,
    uint32_t ssh_reason_code, bool always_display, bool truncated, const uint8_t *message, size_t message_len,
    const uint8_t *language, size_t language_len, bool forward)
{
	if (!dssh_log_enabled(sess, level))
		return;
	uint8_t msg_copy[DSSH_LOG_MESSAGE_MAX];
	uint8_t lang_copy[DSSH_LOG_LANGUAGE_MAX];
	size_t  mlen = message_len;
	size_t  llen = language_len;

	if (mlen > DSSH_LOG_MESSAGE_MAX) {
		mlen      = DSSH_LOG_MESSAGE_MAX;
		truncated = true;
	}
	if (llen > DSSH_LOG_LANGUAGE_MAX) {
		llen      = DSSH_LOG_LANGUAGE_MAX;
		truncated = true;
	}
	if (mlen > 0 && message != NULL)
		memcpy(msg_copy, message, mlen);
	else
		mlen = 0;
	if (llen > 0 && language != NULL)
		memcpy(lang_copy, language, llen);
	else
		llen = 0;
	struct dssh_log_record record = {
	    .level           = level,
	    .source          = source,
	    .error_code      = error_code,
	    .ssh_reason_code = ssh_reason_code,
	    .always_display  = always_display,
	    .truncated       = truncated,
	    .message         = mlen > 0 ? msg_copy : NULL,
	    .message_len     = mlen,
	    .language        = llen > 0 ? lang_copy : NULL,
	    .language_len    = llen,
	};
	struct dssh_session_scope scope;
	session_scope_enter(&scope, sess);
	dssh_log_action action = sess->log_cb(&record, sess->log_cbdata);
	session_scope_leave(&scope);

	if (!forward || source != DSSH_LOG_SOURCE_LIBRARY)
		return;
	if (action == DSSH_LOG_SEND_DEBUG)
		queue_log_forward(sess, false, msg_copy, mlen);
	else if (action == DSSH_LOG_SEND_DEBUG_ALWAYS_DISPLAY)
		queue_log_forward(sess, true, msg_copy, mlen);
}

DSSH_PRIVATE void
dssh_log_emit(struct dssh_session_s *sess, dssh_log_level level, dssh_log_source source, int error_code,
    uint32_t ssh_reason_code, bool always_display, bool truncated, const uint8_t *message, size_t message_len,
    const uint8_t *language, size_t language_len)
{
	log_emit(sess, level, source, error_code, ssh_reason_code, always_display, truncated,
	    message, message_len, language, language_len, true);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 5, 0)))
#endif
static void
log_emitv(struct dssh_session_s *sess, dssh_log_level level, int error_code,
    bool forward, const char *format, va_list ap)
{
	char message[DSSH_LOG_MESSAGE_MAX + 1];
	int result = vsnprintf(message, sizeof(message), format, ap);
	if (result < 0) {
		static const uint8_t failed[] = "Diagnostic formatting failed";

		log_emit(sess, level, DSSH_LOG_SOURCE_LIBRARY, error_code, 0, false, false, failed,
		    sizeof(failed) - 1, NULL, 0, forward);
		return;
	}
#if SIZE_MAX < INT_MAX
	if (result > SIZE_MAX) {
		static const uint8_t too_long[] = "Diagnostic message length overflow";

		log_emit(sess, level, DSSH_LOG_SOURCE_LIBRARY, error_code, 0, false, true, too_long,
		    sizeof(too_long) - 1, NULL, 0, forward);
		return;
	}
#endif
	size_t desired = (size_t)result;
	size_t used    = desired;

	if (used > DSSH_LOG_MESSAGE_MAX)
		used = DSSH_LOG_MESSAGE_MAX;
	log_emit(sess, level, DSSH_LOG_SOURCE_LIBRARY, error_code, 0, false,
	    desired > DSSH_LOG_MESSAGE_MAX, (const uint8_t *)message, used, NULL, 0, forward);
}

DSSH_PRIVATE void
dssh_log_emitf(struct dssh_session_s *sess, dssh_log_level level, int error_code, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	log_emitv(sess, level, error_code, true, format, ap);
	va_end(ap);
}

DSSH_PRIVATE void
dssh_log_emit_localf(struct dssh_session_s *sess, dssh_log_level level, int error_code, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	log_emitv(sess, level, error_code, false, format, ap);
	va_end(ap);
}

DSSH_PRIVATE void
dssh_log_cleanup(struct dssh_session_s *sess)
{
	if (sess->log_forward != NULL) {
		mtx_destroy(&sess->log_forward->mtx);
		free(sess->log_forward);
		sess->log_forward = NULL;
	}
}

DSSH_PRIVATE void
dssh_log_termination(struct dssh_session_s *sess, int cause)
{
	if (atomic_exchange_explicit(&sess->termination_logged, true, memory_order_relaxed))
		return;
	if (cause == DSSH_ERROR_NONE)
		DSSH_LOGF(sess, DSSH_LOG_DEBUG, DSSH_ERROR_NONE, "Session terminated by application");
	else
		DSSH_LOGF(sess, DSSH_LOG_ERROR, DSSH_ERROR_TERMINATED, "Session terminated after error: %s",
		    dssh_strerror(cause));
}

/* ================================================================
 * RFC 4251 wire format primitives (formerly ssh-arch.c)
 * ================================================================ */

DSSH_PUBLIC int64_t
dssh_parse_uint32(const uint8_t *buf, size_t bufsz, uint32_t *val)
{
	if (val == NULL || buf == NULL)
		return DSSH_ERROR_INVALID;
	if (bufsz < 4)
		return DSSH_ERROR_PARSE;
	*val = (((uint32_t)buf[0]) << 24) | (((uint32_t)buf[1]) << 16) | (((uint32_t)buf[2]) << 8) | buf[3];
	return 4;
}

DSSH_PUBLIC int
dssh_serialize_uint32(uint32_t val, uint8_t *buf, size_t bufsz, size_t *pos)
{
	if (pos == NULL || buf == NULL)
		return DSSH_ERROR_INVALID;
	size_t offset = *pos;
	if (offset > bufsz || bufsz - offset < 4)
		return DSSH_ERROR_TOOLONG;
	/* The encoded bytes and the updated position are distinct outputs.
	 * Compare object-representation addresses without ordering unrelated
	 * pointers or assuming an integer representation of an address. */
	const unsigned char *position_bytes = (const unsigned char *)pos;
	const unsigned char *destination = (const unsigned char *)(buf + offset);
	for (size_t i = 0; i < 4; i++) {
		for (size_t j = 0; j < sizeof(*pos); j++) {
			if (destination + i == position_bytes + j)
				return DSSH_ERROR_INVALID;
		}
	}
	buf[offset] = (uint8_t)((val >> 24) & 0xff);
	buf[offset + 1] = (uint8_t)((val >> 16) & 0xff);
	buf[offset + 2] = (uint8_t)((val >> 8) & 0xff);
	buf[offset + 3] = (uint8_t)(val & 0xff);
	*pos = offset + 4; /* The capacity check proves this addition fits. */
	return 0;
}

/*
 * Set the terminate flag and wake all library-owned condvar waiters.
 * This ensures that any thread blocked on rekey_cnd, accept_cnd,
 * or a channel poll_cnd sees termination promptly instead of
 * blocking until an external event (like a socket close) wakes it.
 */
static bool
session_mark_terminated(struct dssh_session_s *sess)
{
	return !atomic_exchange(&sess->terminate, true);
}

static void
session_notify_terminated(struct dssh_session_s *sess)
{
	if (atomic_exchange(&sess->terminate_notified, true))
		return;

	/* Notify the application so it can close sockets or signal
	 * its event loop, unblocking any I/O callbacks. */
	dssh_terminate_cb tcb = sess->terminate_cb;

	if (tcb) {
		struct dssh_session_scope scope;
		session_scope_enter(&scope, sess);
		scope.termination_callback = true;
		tcb(sess, sess->terminate_cbdata);
		session_scope_leave(&scope);
	}
}

static void
session_wake_terminated(struct dssh_session_s *sess)
{
	/* Completion is required once the one-shot wake flag is published. */
	dssh_thrd_require(mtx_lock(&sess->trans.tx_mtx));
	dssh_thrd_require(cnd_broadcast(&sess->trans.rekey_cnd));
	dssh_mtx_unlock(&sess->trans.tx_mtx);

	dssh_thrd_require(mtx_lock(&sess->trans.tx_queue_mtx));
	dssh_thrd_require(cnd_broadcast(&sess->trans.tx_slot_cnd));
	dssh_mtx_unlock(&sess->trans.tx_queue_mtx);

	if (sess->conn_initialized) {
		dssh_thrd_require(mtx_lock(&sess->accept_mtx));
		dssh_thrd_require(cnd_broadcast(&sess->accept_cnd));
		dssh_mtx_unlock(&sess->accept_mtx);

		/* Pin one channel at a time, releasing the registry mutex before
		 * acquiring the channel mutex. No nested library lock is needed. */
		for (;;) {
			struct dssh_channel_s *ch = NULL;
			dssh_thrd_require(mtx_lock(&sess->channel_mtx));
			for (size_t i = 0; i < sess->channel_count; i++) {
				struct dssh_channel_s *candidate = sess->channels[i];

				if (!atomic_exchange_explicit(&candidate->terminate_woken, true,
				    memory_order_acq_rel)) {
					atomic_fetch_add_explicit(&candidate->tx_refs, 1, memory_order_acq_rel);
					ch = candidate;
					break;
				}
			}
			dssh_mtx_unlock(&sess->channel_mtx);
			if (ch == NULL)
				break;
			dssh_thrd_require(mtx_lock(&ch->buf_mtx));
			dssh_thrd_require(cnd_broadcast(&ch->poll_cnd));
			dssh_mtx_unlock(&ch->buf_mtx);
			atomic_fetch_sub_explicit(&ch->tx_refs, 1, memory_order_acq_rel);
		}
	}
}

DSSH_PRIVATE void
session_set_terminate(struct dssh_session_s *sess)
{
	(void)session_mark_terminated(sess);
	session_notify_terminated(sess);
	if (!atomic_exchange(&sess->terminate_wake_started, true))
		session_wake_terminated(sess);
}

DSSH_PRIVATE void
session_set_terminate_no_wake(struct dssh_session_s *sess)
{
	atomic_store(&sess->terminate, true);
}

static bool
session_api_forbidden(struct dssh_session_s *sess)
{
	return sess != NULL && dssh_io_callback_active(sess);
}

DSSH_PUBLIC int
dssh_session_set_cbdata(struct dssh_session_s *sess, void *tx_cbdata, void *rx_cbdata, void *rx_line_cbdata,
    void *extra_line_cbdata)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	sess->tx_cbdata         = tx_cbdata;
	sess->rx_cbdata         = rx_cbdata;
	sess->rx_line_cbdata    = rx_line_cbdata;
	sess->extra_line_cbdata = extra_line_cbdata;
	return 0;
}

/* Callback setters below must be called before dssh_session_start().
 * The thrd_create in dssh_session_start() provides the C11
 * happens-before guarantee that makes these writes visible to the
 * demux thread.  Returns DSSH_ERROR_TOOLATE if called after start. */

DSSH_PUBLIC int
dssh_session_set_debug_cb(struct dssh_session_s *sess, dssh_debug_cb cb, void *cbdata)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	sess->debug_cb     = cb;
	sess->debug_cbdata = cbdata;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_log_cb(struct dssh_session_s *sess, dssh_log_cb cb, void *cbdata)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	if (cb != NULL && sess->log_forward == NULL) {
		struct dssh_log_forward_queue *queue = malloc(sizeof(*queue));

		if (queue == NULL)
			return DSSH_ERROR_ALLOC;
		*queue = (struct dssh_log_forward_queue){0};
		if (mtx_init(&queue->mtx, mtx_plain) != thrd_success) {
			free(queue);
			return DSSH_ERROR_INIT;
		}
		sess->log_forward = queue;
	}
	if (cb == NULL)
		dssh_log_cleanup(sess);
	sess->log_cb     = cb;
	sess->log_cbdata = cbdata;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_log_level(struct dssh_session_s *sess, dssh_log_level level)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (level < DSSH_LOG_ERROR || level > DSSH_LOG_DEBUG)
		return DSSH_ERROR_INVALID;
	atomic_store_explicit(&sess->log_level, level, memory_order_relaxed);
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_unimplemented_cb(struct dssh_session_s *sess, dssh_unimplemented_cb cb, void *cbdata)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	sess->unimplemented_cb     = cb;
	sess->unimplemented_cbdata = cbdata;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_banner_cb(struct dssh_session_s *sess, dssh_auth_banner_cb cb, void *cbdata)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	sess->banner_cb     = cb;
	sess->banner_cbdata = cbdata;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_global_request_cb(struct dssh_session_s *sess, dssh_global_request_cb cb, void *cbdata)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	sess->global_request_cb     = cb;
	sess->global_request_cbdata = cbdata;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_terminate_cb(struct dssh_session_s *sess, dssh_terminate_cb cb, void *cbdata)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	sess->terminate_cb     = cb;
	sess->terminate_cbdata = cbdata;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_hostkey_verify_cb(struct dssh_session_s *sess, dssh_hostkey_verify_cb cb, void *cbdata)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	sess->hostkey_verify_cb     = cb;
	sess->hostkey_verify_cbdata = cbdata;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_timeout(struct dssh_session_s *sess, int timeout_ms)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	sess->timeout_ms = timeout_ms;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_rekey_seconds(struct dssh_session_s *sess, uint32_t seconds)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	atomic_store_explicit(&sess->trans.rekey_seconds, seconds, memory_order_relaxed);
	return 0;
}

/*
 * Measure the storage for a comma-separated list including its NUL.
 * count == 0 or names == NULL produces *out = 0.
 * Rejects NULL elements, empty names, and names containing commas
 * (would corrupt CSV parsing).
 */
static int
filter_csv_size(const char * const *names, size_t count, size_t *out)
{
	*out = 0;
	if (names == NULL || count == 0)
		return 0;

	size_t total = 0;

	for (size_t i = 0; i < count; i++) {
		if (names[i] == NULL)
			return DSSH_ERROR_INVALID;
		if (strchr(names[i], ',') != NULL)
			return DSSH_ERROR_INVALID;
		size_t nlen = strlen(names[i]);

		if (nlen == 0)
			return DSSH_ERROR_INVALID;
		/* Reserve nlen + 1 per name for either the comma
		 * separator (i > 0) or the trailing NUL (last). */
		if (total == SIZE_MAX || nlen > SIZE_MAX - total - 1)
			return DSSH_ERROR_INVALID;
		total += nlen + 1;
	}

	*out = total;
	return 0;
}

static void
write_filter_csv(const char * const *names, size_t count, char *buf)
{
	size_t pos = 0;

	for (size_t i = 0; i < count; i++) {
		size_t nlen = strlen(names[i]);

		if (i > 0)
			buf[pos++] = ',';
		memcpy(&buf[pos], names[i], nlen);
		pos += nlen;
	}
	buf[pos] = 0;
}

static int
build_filter_csv(const char * const *names, size_t count, char **out)
{
	*out = NULL;
	size_t total;
	int ret = filter_csv_size(names, count, &total);
	if (ret < 0 || total == 0)
		return ret;
	char *buf = malloc(total);
	if (buf == NULL)
		return DSSH_ERROR_ALLOC;
	write_filter_csv(names, count, buf);
	*out = buf;
	return 0;
}

/* These parts use the RFC 3066 syntax referenced by SSH. Preserve case. */
static bool
language_tag_valid(const char *name)
{
	bool primary = true;
	unsigned length = 0;
	for (const char *at = name; *at != 0; at++) {
		unsigned char ch = (unsigned char)*at;
		if (ch == '-') {
			if (length == 0)
				return false;
			primary = false;
			length = 0;
			continue;
		}
		bool alpha = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
		if (!alpha && (primary || ch < '0' || ch > '9'))
			return false;
		length++;
		if (length > 8)
			return false;
	}
	return length != 0;
}

static int
set_languages_inner(struct dssh_session_s *sess, size_t direction,
    const char * const *names, size_t count)
{
	/* Even one-byte tags need a separator or terminating NUL each. */
	if (names != NULL && count > sess->trans.packet_buf_sz / 2)
		return DSSH_ERROR_TOOLONG;
	size_t needed;
	int ret = filter_csv_size(names, count, &needed);
	if (ret < 0)
		return ret;
	if (needed > sess->trans.packet_buf_sz)
		return DSSH_ERROR_TOOLONG;
	if (names != NULL) {
		for (size_t i = 0; i < count; i++) {
			if (!language_tag_valid(names[i]))
				return DSSH_ERROR_INVALID;
		}
	}
	struct dssh_language_field *field = &sess->local_languages[direction];
	char *candidate = NULL;
	char *old = NULL;
	int lr = mtx_lock(&sess->mtx);
	if (lr != thrd_success) {
		session_set_terminate(sess);
		return DSSH_ERROR_TERMINATED;
	}
	if (needed > field->capacity && !sess->terminate) {
		dssh_mtx_unlock(&sess->mtx);
		candidate = malloc(needed);
		if (candidate == NULL)
			return DSSH_ERROR_ALLOC;
		lr = mtx_lock(&sess->mtx);
		if (lr != thrd_success) {
			free(candidate);
			session_set_terminate(sess);
			return DSSH_ERROR_TERMINATED;
		}
	}
	if (sess->terminate) {
		dssh_mtx_unlock(&sess->mtx);
		free(candidate);
		return DSSH_ERROR_TERMINATED;
	}
	if (needed > field->capacity) {
		old = field->data;
		field->data = candidate;
		field->capacity = needed;
		candidate = NULL;
	}
	if (needed != 0)
		write_filter_csv(names, count, field->data);
	else if (field->data != NULL)
		field->data[0] = 0;
	field->len = needed != 0 ? needed - 1 : 0;
	dssh_mtx_unlock(&sess->mtx);
	free(old);
	free(candidate);
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_languages(struct dssh_session_s *sess,
    enum dssh_direction direction, const char * const *names, size_t count)
{
	if (sess == NULL || (direction != DSSH_CLIENT_TO_SERVER
	    && direction != DSSH_SERVER_TO_CLIENT) || session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	size_t index = (size_t)direction;
	struct dssh_session_scope scope;
	session_scope_enter(&scope, sess);
	int ret = set_languages_inner(sess, index, names, count);
	session_scope_leave(&scope);
	return ret;
}

static int64_t
get_languages_inner(struct dssh_session_s *sess, size_t direction,
    bool peer, char *buf, size_t bufsz)
{
	int lr = mtx_lock(&sess->mtx);
	if (lr != thrd_success) {
		session_set_terminate(sess);
		return DSSH_ERROR_TERMINATED;
	}
	if (peer && !sess->peer_languages_available) {
		dssh_mtx_unlock(&sess->mtx);
		return DSSH_ERROR_INIT;
	}
	size_t length = peer ? sess->peer_languages_len[direction]
	    : sess->local_languages[direction].len;
	const char *data = peer ? sess->peer_languages
	    : sess->local_languages[direction].data;
	if (peer && direction == 1 && data != NULL)
		data += sess->peer_languages_len[0] + 1;
	/* Both writers bound lengths by the immutable packet buffer, which is
	 * below SIZE_MAX - 4 and at most 64 MiB. Include the terminating NUL. */
	size_t needed = length + 1;
	int64_t result = (int64_t)needed;
	if (buf != NULL && bufsz != 0) {
		if (bufsz < needed)
			result = DSSH_ERROR_TOOLONG;
		else if (length == 0)
			buf[0] = 0;
		else
			memcpy(buf, data, needed);
	}
	dssh_mtx_unlock(&sess->mtx);
	return result;
}

static int64_t
get_languages(struct dssh_session_s *sess, enum dssh_direction direction,
    bool peer, char *buf, size_t bufsz)
{
	if (sess == NULL || (direction != DSSH_CLIENT_TO_SERVER
	    && direction != DSSH_SERVER_TO_CLIENT) || session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	size_t index = (size_t)direction;
	struct dssh_session_scope scope;
	session_scope_enter(&scope, sess);
	int64_t result = get_languages_inner(sess, index, peer, buf, bufsz);

	session_scope_leave(&scope);
	return result;
}

DSSH_PUBLIC int64_t
dssh_session_get_languages(struct dssh_session_s *sess,
    enum dssh_direction direction, char *buf, size_t bufsz)
{
	return get_languages(sess, direction, false, buf, bufsz);
}

DSSH_PUBLIC int64_t
dssh_session_get_peer_languages(struct dssh_session_s *sess,
    enum dssh_direction direction, char *buf, size_t bufsz)
{
	return get_languages(sess, direction, true, buf, bufsz);
}

/*
 * Per-session algorithm whitelist setters.  Caller-owned input;
 * the library makes its own copy.  count == 0 or names == NULL
 * clears any previous filter (= use all registered).  Filter
 * order is negotiation preference order.  Must be called before
 * dssh_session_start; returns DSSH_ERROR_TOOLATE otherwise.
 */
DSSH_PUBLIC int
dssh_session_set_kex_filter(struct dssh_session_s *sess, const char * const *names, size_t count)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	char *new_csv = NULL;
	int   ret     = build_filter_csv(names, count, &new_csv);

	if (ret < 0)
		return ret;
	free(sess->kex_filter);
	sess->kex_filter = new_csv;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_key_algo_filter(struct dssh_session_s *sess, const char * const *names, size_t count)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	char *new_csv = NULL;
	int   ret     = build_filter_csv(names, count, &new_csv);

	if (ret < 0)
		return ret;
	free(sess->key_algo_filter);
	sess->key_algo_filter = new_csv;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_enc_filter(struct dssh_session_s *sess, const char * const *names, size_t count)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	char *new_csv = NULL;
	int   ret     = build_filter_csv(names, count, &new_csv);

	if (ret < 0)
		return ret;
	free(sess->enc_filter);
	sess->enc_filter = new_csv;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_mac_filter(struct dssh_session_s *sess, const char * const *names, size_t count)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	char *new_csv = NULL;
	int   ret     = build_filter_csv(names, count, &new_csv);

	if (ret < 0)
		return ret;
	free(sess->mac_filter);
	sess->mac_filter = new_csv;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_comp_filter(struct dssh_session_s *sess, const char * const *names, size_t count)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (session_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	char *new_csv = NULL;
	int   ret     = build_filter_csv(names, count, &new_csv);

	if (ret < 0)
		return ret;
	free(sess->comp_filter);
	sess->comp_filter = new_csv;
	return 0;
}
