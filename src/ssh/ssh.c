#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "ssh-internal.h"

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
		case DSSH_ERROR_AUTH_REJECTED:
			return "Authentication rejected";
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
	mtx_unlock(&queue->mtx);
}

DSSH_PRIVATE void
dssh_log_emit(struct dssh_session_s *sess, dssh_log_level level, dssh_log_source source, int error_code,
    uint32_t ssh_reason_code, bool always_display, bool truncated, const uint8_t *message, size_t message_len,
    const uint8_t *language, size_t language_len)
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
	dssh_log_action action = sess->log_cb(&record, sess->log_cbdata);

	if (source != DSSH_LOG_SOURCE_LIBRARY)
		return;
	if (action == DSSH_LOG_SEND_DEBUG)
		queue_log_forward(sess, false, msg_copy, mlen);
	else if (action == DSSH_LOG_SEND_DEBUG_ALWAYS_DISPLAY)
		queue_log_forward(sess, true, msg_copy, mlen);
}

DSSH_PRIVATE void
dssh_log_emitf(struct dssh_session_s *sess, dssh_log_level level, int error_code, const char *format, ...)
{
	char    message[DSSH_LOG_MESSAGE_MAX + 1];
	va_list ap;

	va_start(ap, format);
	int result = vsnprintf(message, sizeof(message), format, ap);
	va_end(ap);
	if (result < 0) {
		static const uint8_t failed[] = "Diagnostic formatting failed";

		dssh_log_emit(sess, level, DSSH_LOG_SOURCE_LIBRARY, error_code, 0, false, false, failed,
		    sizeof(failed) - 1, NULL, 0);
		return;
	}
#if SIZE_MAX < INT_MAX
	if (result > SIZE_MAX) {
		static const uint8_t too_long[] = "Diagnostic message length overflow";

		dssh_log_emit(sess, level, DSSH_LOG_SOURCE_LIBRARY, error_code, 0, false, true, too_long,
		    sizeof(too_long) - 1, NULL, 0);
		return;
	}
#endif
	size_t desired = (size_t)result;
	size_t used    = desired;

	if (used > DSSH_LOG_MESSAGE_MAX)
		used = DSSH_LOG_MESSAGE_MAX;
	dssh_log_emit(sess, level, DSSH_LOG_SOURCE_LIBRARY, error_code, 0, false,
	    desired > DSSH_LOG_MESSAGE_MAX, (const uint8_t *)message, used, NULL, 0);
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
	if (*pos > bufsz || bufsz - *pos < 4)
		return DSSH_ERROR_TOOLONG;
	buf[(*pos)++] = (uint8_t)((val >> 24) & 0xff);
	buf[(*pos)++] = (uint8_t)((val >> 16) & 0xff);
	buf[(*pos)++] = (uint8_t)((val >> 8) & 0xff);
	buf[(*pos)++] = (uint8_t)(val & 0xff);
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
	if (atomic_exchange(&sess->terminate, true))
		return false;

	/* Notify the application so it can close sockets or signal
	 * its event loop, unblocking any I/O callbacks. */
	dssh_terminate_cb tcb = sess->terminate_cb;

	if (tcb)
		tcb(sess, sess->terminate_cbdata);
	return true;
}

static void
session_wake_terminated(struct dssh_session_s *sess, bool tx_locked)
{
	/* Wake senders blocked during rekey.  The predicate is checked with
	 * tx_mtx held, so an ordinary caller must acquire that mutex before
	 * broadcasting.  A busy trylock does not establish ownership: another
	 * thread may own the mutex and be about to enter cnd_wait. */
	if (tx_locked) {
		dssh_thrd_check(sess, cnd_broadcast(&sess->trans.rekey_cnd));
	}
	else if (dssh_thrd_check(sess, mtx_lock(&sess->trans.tx_mtx)) == thrd_success) {
		dssh_thrd_check(sess, cnd_broadcast(&sess->trans.rekey_cnd));
		dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_mtx));
	}

	/* Wake demux thread stalled on an occupied TX slot */
	if (dssh_thrd_check(sess, mtx_lock(&sess->trans.tx_queue_mtx)) == thrd_success) {
		dssh_thrd_check(sess, cnd_broadcast(&sess->trans.tx_slot_cnd));
		dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_queue_mtx));
	}

	/* Wake conn-layer waiters if initialized */
	if (sess->conn_initialized) {
		if (dssh_thrd_check(sess, mtx_lock(&sess->accept_mtx)) == thrd_success) {
			dssh_thrd_check(sess, cnd_broadcast(&sess->accept_cnd));
			dssh_thrd_check(sess, mtx_unlock(&sess->accept_mtx));
		}

		/* Lock order: channel_mtx then buf_mtx. */
		if (dssh_thrd_check(sess, mtx_lock(&sess->channel_mtx)) == thrd_success) {
			for (size_t i = 0; i < sess->channel_count; i++) {
				struct dssh_channel_s *ch = sess->channels[i];

				if (dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx)) == thrd_success) {
					dssh_thrd_check(sess, cnd_broadcast(&ch->poll_cnd));
					dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
				}
			}
			dssh_thrd_check(sess, mtx_unlock(&sess->channel_mtx));
		}
	}
}

DSSH_PRIVATE void
session_set_terminate(struct dssh_session_s *sess)
{
	if (session_mark_terminated(sess))
		session_wake_terminated(sess, false);
}

DSSH_PRIVATE void
session_set_terminate_tx_locked(struct dssh_session_s *sess)
{
	if (session_mark_terminated(sess))
		session_wake_terminated(sess, true);
}

DSSH_PRIVATE void
session_set_terminate_no_wake(struct dssh_session_s *sess)
{
	(void)session_mark_terminated(sess);
}

DSSH_PUBLIC int
dssh_session_set_cbdata(struct dssh_session_s *sess, void *tx_cbdata, void *rx_cbdata, void *rx_line_cbdata,
    void *extra_line_cbdata)
{
	if (sess == NULL)
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
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	if (cb != NULL && sess->log_forward == NULL) {
		struct dssh_log_forward_queue *queue = calloc(1, sizeof(*queue));

		if (queue == NULL)
			return DSSH_ERROR_ALLOC;
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
	sess->trans.rekey_seconds = seconds;
	return 0;
}

/*
 * Build a comma-separated CSV from names[count] into a fresh
 * malloc'd string.  count == 0 or names == NULL produces *out = NULL.
 * Rejects NULL elements, empty names, and names containing commas
 * (would corrupt CSV parsing).
 */
static int
build_filter_csv(const char * const *names, size_t count, char **out)
{
	*out = NULL;
	if (names == NULL || count == 0)
		return 0;

	/* Two passes: validate + measure, then allocate + copy. */
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
		if (nlen > SIZE_MAX - 1 - total)
			return DSSH_ERROR_INVALID;
		total += nlen + 1;
	}

	char *buf = malloc(total);

	if (buf == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	for (size_t i = 0; i < count; i++) {
		size_t nlen = strlen(names[i]);

		if (i > 0)
			buf[pos++] = ',';
		memcpy(&buf[pos], names[i], nlen);
		pos += nlen;
	}
	buf[pos] = 0;

	*out = buf;
	return 0;
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
