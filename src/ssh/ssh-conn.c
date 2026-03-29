// RFC 4254: SSH Connection Protocol

#include <stdlib.h>
#include <string.h>

#include "deucessh-conn.h"
#include "ssh-internal.h"

#ifdef DSSH_TESTING
#include "dssh_test_ossl.h"
#include "dssh_test_alloc.h"
#endif

/* ================================================================
 * Channel buffer primitives (formerly ssh-chan.c).
 *
 * All DSSH_TESTABLE so unit tests can exercise them directly;
 * in production builds DSSH_TESTABLE compiles to static.
 * ================================================================ */

/* ---- Circular byte buffer ---- */

DSSH_TESTABLE int
bytebuf_init(struct dssh_bytebuf *b, size_t capacity)
{
	b->data = malloc(capacity);
	if (b->data == NULL)
		return DSSH_ERROR_ALLOC;
	b->capacity = capacity;
	b->head = 0;
	b->tail = 0;
	b->used = 0;
	b->total = 0;
	return 0;
}

DSSH_TESTABLE void
bytebuf_free(struct dssh_bytebuf *b)
{
	free(b->data);
	b->data = NULL;
	b->capacity = 0;
	b->head = 0;
	b->tail = 0;
	b->used = 0;
}

DSSH_TESTABLE size_t
bytebuf_write(struct dssh_bytebuf *b,
    const uint8_t *data, size_t len)
{
	if (b->capacity == 0)
		return 0;

	size_t space = b->capacity - b->used;

	if (len > space)
		len = space;
	if (len == 0)
		return 0;

	/* Write may wrap around the end of the buffer */
	size_t first = b->capacity - b->tail;

	if (first > len)
		first = len;
	memcpy(&b->data[b->tail], data, first);
	if (len > first)
		memcpy(b->data, &data[first], len - first);

	b->tail = (b->tail + len) % b->capacity;
	b->used += len;
	if (len > SIZE_MAX - b->total)
		b->total = SIZE_MAX;
	else
		b->total += len;
	return len;
}

DSSH_TESTABLE size_t
bytebuf_read(struct dssh_bytebuf *b,
    uint8_t *buf, size_t bufsz, size_t limit)
{
	if (b->capacity == 0)
		return 0;

	size_t avail = b->used;

	if ((limit > 0) && (avail > limit))
		avail = limit;
	if (bufsz < avail)
		avail = bufsz;
	if (avail == 0)
		return 0;

	/* Read may wrap around the end of the buffer */
	size_t first = b->capacity - b->head;

	if (first > avail)
		first = avail;
	memcpy(buf, &b->data[b->head], first);
	if (avail > first)
		memcpy(&buf[first], b->data, avail - first);

	b->head = (b->head + avail) % b->capacity;
	b->used -= avail;
	return avail;
}

DSSH_TESTABLE size_t
bytebuf_available(const struct dssh_bytebuf *b)
{
	return b->used;
}

DSSH_TESTABLE size_t
bytebuf_free_space(const struct dssh_bytebuf *b)
{
	return b->capacity - b->used;
}

/* ---- Message queue ---- */

DSSH_TESTABLE void
msgqueue_init(struct dssh_msgqueue *q)
{
	q->head = NULL;
	q->tail = NULL;
	q->total_bytes = 0;
	q->count = 0;
}

DSSH_TESTABLE void
msgqueue_free(struct dssh_msgqueue *q)
{
	struct dssh_msgqueue_entry *e = q->head;

	while (e != NULL) {
		struct dssh_msgqueue_entry *next = e->next;

		free(e);
		e = next;
	}
	q->head = NULL;
	q->tail = NULL;
	q->total_bytes = 0;
	q->count = 0;
}

DSSH_TESTABLE int
msgqueue_push(struct dssh_msgqueue *q,
    const uint8_t *data, size_t len)
{
	if (len > SIZE_MAX - sizeof(struct dssh_msgqueue_entry))
		return DSSH_ERROR_INVALID;

	struct dssh_msgqueue_entry *e = malloc(sizeof(*e) + len);

	if (e == NULL)
		return DSSH_ERROR_ALLOC;
	e->next = NULL;
	e->len = len;
	memcpy(e->data, data, len);

	if (q->tail != NULL)
		q->tail->next = e;
	else
		q->head = e;
	q->tail = e;
	if (len > SIZE_MAX - q->total_bytes)
		q->total_bytes = SIZE_MAX;
	else
		q->total_bytes += len;
	if (q->count < SIZE_MAX)
		q->count++;
	return 0;
}

DSSH_TESTABLE int64_t
msgqueue_pop(struct dssh_msgqueue *q,
    uint8_t *buf, size_t bufsz)
{
	if (q->head == NULL)
		return 0;
	if (buf == NULL)
		return (int64_t)q->head->len;
	if (bufsz < q->head->len)
		return DSSH_ERROR_TOOLONG;

	struct dssh_msgqueue_entry *e = q->head;
	size_t                      len = e->len;

	memcpy(buf, e->data, len);

	q->head = e->next;
	if (q->head == NULL)
		q->tail = NULL;
	q->total_bytes -= len;
	q->count--;
	free(e);
	return (int64_t)len;
}

/* ---- Signal queue ---- */

DSSH_TESTABLE void
sigqueue_init(struct dssh_signal_queue *q)
{
	q->head = NULL;
	q->tail = NULL;
}

DSSH_TESTABLE void
sigqueue_free(struct dssh_signal_queue *q)
{
	struct dssh_signal_mark *m = q->head;

	while (m != NULL) {
		struct dssh_signal_mark *next = m->next;

		free(m);
		m = next;
	}
	q->head = NULL;
	q->tail = NULL;
}

DSSH_TESTABLE int
sigqueue_push(struct dssh_signal_queue *q,
    const char *name, size_t stdout_pos, size_t stderr_pos)
{
	struct dssh_signal_mark *m = malloc(sizeof(*m));

	if (m == NULL)
		return DSSH_ERROR_ALLOC;
	m->next = NULL;
	m->stdout_pos = stdout_pos;
	m->stderr_pos = stderr_pos;

	size_t nlen = strlen(name);

	if (nlen >= sizeof(m->name))
		nlen = sizeof(m->name) - 1;
	memcpy(m->name, name, nlen);
	m->name[nlen] = 0;

	if (q->tail != NULL)
		q->tail->next = m;
	else
		q->head = m;
	q->tail = m;
	return 0;
}

DSSH_TESTABLE bool
sigqueue_ready(const struct dssh_signal_queue *q,
    size_t stdout_consumed, size_t stderr_consumed)
{
	if (q->head == NULL)
		return false;
	return stdout_consumed >= q->head->stdout_pos
	       && stderr_consumed >= q->head->stderr_pos;
}

DSSH_TESTABLE const char *
sigqueue_pop(struct dssh_signal_queue *q,
    size_t stdout_consumed, size_t stderr_consumed,
    char *buf, size_t bufsz)
{
	if (q->head == NULL)
		return NULL;
	if ((stdout_consumed < q->head->stdout_pos)
	    || (stderr_consumed < q->head->stderr_pos))
		return NULL;

	struct dssh_signal_mark *m = q->head;

	q->head = m->next;
	if (q->head == NULL)
		q->tail = NULL;

	size_t nlen = strlen(m->name);

	if (nlen >= bufsz)
		nlen = bufsz - 1;
	memcpy(buf, m->name, nlen);
	buf[nlen] = 0;
	free(m);
	return buf;
}

/* ---- Accept queue ---- */

DSSH_TESTABLE void
acceptqueue_init(struct dssh_accept_queue *q)
{
	q->head = NULL;
	q->tail = NULL;
}

DSSH_TESTABLE void
acceptqueue_free(struct dssh_accept_queue *q)
{
	struct dssh_incoming_open *e = q->head;

	while (e != NULL) {
		struct dssh_incoming_open *next = e->next;

		free(e);
		e = next;
	}
	q->head = NULL;
	q->tail = NULL;
}

DSSH_TESTABLE int
acceptqueue_push(struct dssh_accept_queue *q,
    uint32_t peer_channel, uint32_t peer_window,
    uint32_t peer_max_packet,
    const uint8_t *type, size_t type_len)
{
	struct dssh_incoming_open *e = malloc(sizeof(*e));

	if (e == NULL)
		return DSSH_ERROR_ALLOC;
	e->next = NULL;
	e->peer_channel = peer_channel;
	e->peer_window = peer_window;
	e->peer_max_packet = peer_max_packet;
	if (type_len >= sizeof(e->channel_type))
		type_len = sizeof(e->channel_type) - 1;
	memcpy(e->channel_type, type, type_len);
	e->channel_type[type_len] = 0;
	e->channel_type_len = type_len;

	if (q->tail != NULL)
		q->tail->next = e;
	else
		q->head = e;
	q->tail = e;
	return 0;
}

DSSH_TESTABLE struct dssh_incoming_open *

acceptqueue_pop(struct dssh_accept_queue *q)
{
	struct dssh_incoming_open *e = q->head;

	if (e == NULL)
		return NULL;
	q->head = e->next;
	if (q->head == NULL)
		q->tail = NULL;
	e->next = NULL;
	return e;
}

/* ================================================================ */

#define INITIAL_WINDOW_SIZE 0x200000
#define MAX_PACKET_SIZE 0x8000

/* Fixed overhead for channel messages (excludes variable-length fields). */

/* CHANNEL_DATA: msg_type + channel + data_len */
#define CHAN_DATA_FIXED  (1 + 4 + 4)

/* CHANNEL_EXTENDED_DATA: msg_type + channel + data_type + data_len */
#define CHAN_EXT_DATA_FIXED  (1 + 4 + 4 + 4)

/* CHANNEL_REQUEST: msg_type + channel + rtype_len + want_reply */
#define CHAN_REQUEST_FIXED  (1 + 4 + 4 + 1)

/* pty-req extra data: term_len + cols + rows + wpx + hpx + modes_len */
#define PTY_REQ_FIXED  (4 + 4 + 4 + 4 + 4 + 4)

/* window-change extra data: cols + rows + wpx + hpx */
#define WINDOW_CHANGE_FIXED  (4 + 4 + 4 + 4)

static const char str_signal[]          = "signal";
static const char str_exit_status[]     = "exit-status";
static const char str_window_change[]   = "window-change";
static const char str_x11[]             = "x11";
static const char str_forwarded_tcpip[] = "forwarded-tcpip";
static const char str_direct_tcpip[]    = "direct-tcpip";
static const char str_session[]         = "session";
static const char str_shell[]           = "shell";
static const char str_exec[]            = "exec";
static const char str_subsystem[]       = "subsystem";

/* ================================================================
 * Timeout helper -- thin wrapper around dssh_deadline_from_ms()
 * ================================================================ */

static void
deadline_from_ms(struct timespec *ts, int timeout_ms)
{
	dssh_deadline_from_ms(ts, timeout_ms);
}

/* ================================================================
 * Parse helpers for channel request payloads
 * ================================================================ */

DSSH_PUBLIC int
dssh_parse_pty_req_data(const uint8_t *data, size_t data_len,
    struct dssh_pty_req *pty)
{
	if (pty == NULL)
		return DSSH_ERROR_INVALID;
	if (data == NULL && data_len > 0)
		return DSSH_ERROR_INVALID;
	size_t   pos = 0;
	uint32_t tlen;

	if (pos + 4 > data_len)
		return DSSH_ERROR_PARSE;
	tlen = DSSH_GET_U32(&data[pos]);
	pos += 4;
	if (pos + tlen + 16 > data_len)
		return DSSH_ERROR_PARSE;
	pty->term = (const char *)&data[pos];
	pos += tlen;
	pty->cols = DSSH_GET_U32(&data[pos]);
	pty->rows = DSSH_GET_U32(&data[pos + 4]);
	pty->wpx = DSSH_GET_U32(&data[pos + 8]);
	pty->hpx = DSSH_GET_U32(&data[pos + 12]);
	pos += 16;
	if (pos + 4 <= data_len) {
		uint32_t mlen = DSSH_GET_U32(&data[pos]);

		pos += 4;
		if (pos + mlen <= data_len) {
			pty->modes = &data[pos];
			pty->modes_len = mlen;
		}
		else {
			pty->modes = NULL;
			pty->modes_len = 0;
		}
	}
	else {
		pty->modes = NULL;
		pty->modes_len = 0;
	}
	return 0;
}

DSSH_PUBLIC int
dssh_parse_env_data(const uint8_t *data, size_t data_len,
    const uint8_t **name, size_t *name_len,
    const uint8_t **value, size_t *value_len)
{
	if (name == NULL)
		return DSSH_ERROR_INVALID;
	if (value == NULL)
		return DSSH_ERROR_INVALID;
	if (name_len == NULL)
		return DSSH_ERROR_INVALID;
	if (value_len == NULL)
		return DSSH_ERROR_INVALID;
	if (data == NULL && data_len > 0)
		return DSSH_ERROR_INVALID;
	size_t   pos = 0;
	uint32_t nlen;

	if (pos + 4 > data_len)
		return DSSH_ERROR_PARSE;
	nlen = DSSH_GET_U32(&data[pos]);
	pos += 4;
	if (pos + nlen > data_len)
		return DSSH_ERROR_PARSE;
	*name = &data[pos];
	*name_len = nlen;
	pos += nlen;

	uint32_t vlen;

	if (pos + 4 > data_len)
		return DSSH_ERROR_PARSE;
	vlen = DSSH_GET_U32(&data[pos]);
	pos += 4;
	if (pos + vlen > data_len)
		return DSSH_ERROR_PARSE;
	*value = &data[pos];
	*value_len = vlen;
	return 0;
}

DSSH_PUBLIC int
dssh_parse_exec_data(const uint8_t *data, size_t data_len,
    const uint8_t **command, size_t *command_len)
{
	if (command == NULL)
		return DSSH_ERROR_INVALID;
	if (command_len == NULL)
		return DSSH_ERROR_INVALID;
	if (data == NULL && data_len > 0)
		return DSSH_ERROR_INVALID;
	if (data_len < 4)
		return DSSH_ERROR_PARSE;

	uint32_t clen = DSSH_GET_U32(data);

	if (4 + clen > data_len)
		return DSSH_ERROR_PARSE;
	*command = &data[4];
	*command_len = clen;
	return 0;
}

DSSH_PUBLIC int
dssh_parse_subsystem_data(const uint8_t *data, size_t data_len,
    const uint8_t **name, size_t *name_len)
{
	if (name == NULL)
		return DSSH_ERROR_INVALID;
	if (name_len == NULL)
		return DSSH_ERROR_INVALID;
	if (data == NULL && data_len > 0)
		return DSSH_ERROR_INVALID;
	if (data_len < 4)
		return DSSH_ERROR_PARSE;

	uint32_t slen = DSSH_GET_U32(data);

	if (4 + slen > data_len)
		return DSSH_ERROR_PARSE;
	*name = &data[4];
	*name_len = slen;
	return 0;
}

/* Channel open failure reason codes (RFC 4254 s5.1) */
#define SSH_OPEN_ADMINISTRATIVELY_PROHIBITED UINT32_C(1)
#define SSH_OPEN_CONNECT_FAILED UINT32_C(2)
#define SSH_OPEN_UNKNOWN_CHANNEL_TYPE UINT32_C(3)
#define SSH_OPEN_RESOURCE_SHORTAGE UINT32_C(4)

/*
 * Saturating add for window sizes -- clamp at UINT32_MAX per RFC 4254 s5.2.
 */
static inline uint32_t
window_add(uint32_t current, uint32_t bytes)
{
	uint32_t result = current + bytes;

	if (result < current)
		return UINT32_MAX;
	return result;
}

/* ================================================================
 * Low-level wire functions -- internal only.
 * Used by the high-level API; not part of the public interface.
 * ================================================================ */

DSSH_TESTABLE int
send_data(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, const uint8_t *data, size_t len,
    size_t *sentp)
{
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	if (!ch->open || ch->eof_sent || ch->close_received) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return DSSH_ERROR_TERMINATED;
	}
	if (sentp != NULL) {
		if (len > ch->remote_window)
			len = ch->remote_window;
		if (len > ch->remote_max_packet)
			len = ch->remote_max_packet;
		if (len == 0) {
			dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
			*sentp = 0;
			return 0;
		}
	} else if ((len > ch->remote_window)
	    || (len > ch->remote_max_packet)) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return DSSH_ERROR_TOOLONG;
	}

	/* len <= remote_window (uint32_t), so fits in uint32_t */
	uint32_t len_u32 = (uint32_t)len;

	ch->remote_window -= len_u32;
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	if (len > SIZE_MAX - CHAN_DATA_FIXED)
		return DSSH_ERROR_INVALID;
	size_t   msg_len = CHAN_DATA_FIXED + len;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_DATA;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);
	DSSH_PUT_U32(len_u32, msg, &pos);
	memcpy(&msg[pos], data, len);
	pos += len;

	int ret = send_packet(sess, msg, pos, NULL);

	free(msg);
	if (ret == 0 && sentp != NULL)
		*sentp = len;
	return ret;
}

DSSH_TESTABLE int
send_extended_data(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, uint32_t data_type_code,
    const uint8_t *data, size_t len,
    size_t *sentp)
{
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	if (!ch->open || ch->eof_sent || ch->close_received) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return DSSH_ERROR_TERMINATED;
	}
	if (sentp != NULL) {
		if (len > ch->remote_window)
			len = ch->remote_window;
		if (len > ch->remote_max_packet)
			len = ch->remote_max_packet;
		if (len == 0) {
			dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
			*sentp = 0;
			return 0;
		}
	} else if ((len > ch->remote_window)
	    || (len > ch->remote_max_packet)) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return DSSH_ERROR_TOOLONG;
	}

	/* len <= remote_window (uint32_t), so fits in uint32_t */
	uint32_t len_u32 = (uint32_t)len;

	ch->remote_window -= len_u32;
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	if (len > SIZE_MAX - CHAN_EXT_DATA_FIXED)
		return DSSH_ERROR_INVALID;
	size_t   msg_len = CHAN_EXT_DATA_FIXED + len;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);
	DSSH_PUT_U32(data_type_code, msg, &pos);
	DSSH_PUT_U32(len_u32, msg, &pos);
	memcpy(&msg[pos], data, len);
	pos += len;

	int ret = send_packet(sess, msg, pos, NULL);

	free(msg);
	if (ret == 0 && sentp != NULL)
		*sentp = len;
	return ret;
}

#define SSH_EXTENDED_DATA_STDERR UINT32_C(1)

DSSH_TESTABLE int
send_window_adjust(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, uint32_t bytes)
{
	uint8_t msg[32];
	size_t  pos = 0;

	int ret;

	msg[pos++] = SSH_MSG_CHANNEL_WINDOW_ADJUST;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);
	DSSH_PUT_U32(bytes, msg, &pos);

	ret = send_packet(sess, msg, pos, NULL);
	if (ret == 0) {
		dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
		ch->local_window = window_add(ch->local_window, bytes);
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	}
	return ret;
}

static int
dssh_conn_send_exit_status(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, uint32_t exit_code)
{
	uint8_t msg[32];
	size_t  pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);
	DSSH_PUT_U32(DSSH_STRLEN(str_exit_status), msg, &pos);
	memcpy(&msg[pos], str_exit_status, DSSH_STRLEN(str_exit_status));
	pos += DSSH_STRLEN(str_exit_status);
	msg[pos++] = 0; /* want_reply = false */
	DSSH_PUT_U32(exit_code, msg, &pos);
	return send_packet(sess, msg, pos, NULL);
}

DSSH_TESTABLE int
send_eof(struct dssh_session_s *sess,
    struct dssh_channel_s *ch)
{
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	if (ch->eof_sent) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return 0;
	}
	ch->eof_sent = true;
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	uint8_t msg[8];
	size_t  pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_EOF;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);

	return send_packet(sess, msg, pos, NULL);
}

DSSH_TESTABLE int
conn_close(struct dssh_session_s *sess,
    struct dssh_channel_s *ch)
{
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	if (ch->close_sent) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return 0;
	}
	ch->close_sent = true;
	ch->open = false;
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	uint8_t msg[8];
	size_t  pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_CLOSE;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);

	return send_packet(sess, msg, pos, NULL);
}

/* ================================================================
 * Channel table helpers
 * ================================================================ */

static int
register_channel(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	dssh_thrd_check(sess, mtx_lock(&sess->channel_mtx));
	if (sess->channel_count >= sess->channel_capacity) {
		size_t newcap = sess->channel_capacity ? sess->channel_capacity * 2 : 8;

		if (sess->channel_capacity > SIZE_MAX / 2
		    || newcap > SIZE_MAX / sizeof(struct dssh_channel_s *)) {
			dssh_thrd_check(sess, mtx_unlock(&sess->channel_mtx));
			return DSSH_ERROR_ALLOC;
		}
		struct dssh_channel_s **newt = realloc(sess->channels,
		        newcap * sizeof(*newt));

		if (newt == NULL) {
			dssh_thrd_check(sess, mtx_unlock(&sess->channel_mtx));
			return DSSH_ERROR_ALLOC;
		}
		sess->channels = newt;
		sess->channel_capacity = newcap;
	}
	sess->channels[sess->channel_count++] = ch;
	dssh_thrd_check(sess, mtx_unlock(&sess->channel_mtx));
	return 0;
}

static struct dssh_channel_s *

find_channel(struct dssh_session_s *sess, uint32_t local_id)
{
	/* Lock order: channel_mtx then buf_mtx (hand-over-hand).
	 * Acquire buf_mtx before releasing channel_mtx so the
	 * channel cannot be freed between lookup and use. */
	dssh_thrd_check(sess, mtx_lock(&sess->channel_mtx));
	for (size_t i = 0; i < sess->channel_count; i++) {
		if (sess->channels[i]->local_id == local_id) {
			struct dssh_channel_s *ch = sess->channels[i];

			dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
			dssh_thrd_check(sess, mtx_unlock(&sess->channel_mtx));
			return ch; /* returned with buf_mtx held */
		}
	}
	dssh_thrd_check(sess, mtx_unlock(&sess->channel_mtx));
	return NULL;
}

static void
unregister_channel(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	dssh_thrd_check(sess, mtx_lock(&sess->channel_mtx));
	for (size_t i = 0; i < sess->channel_count; i++) {
		if (sess->channels[i] == ch) {
			sess->channels[i] = sess->channels[--sess->channel_count];
			break;
		}
	}
	dssh_thrd_check(sess, mtx_unlock(&sess->channel_mtx));
}

static int64_t
alloc_channel_id(struct dssh_session_s *sess)
{
	dssh_thrd_check(sess, mtx_lock(&sess->channel_mtx));

	uint32_t start = sess->next_channel_id;

	for (;;) {
		uint32_t candidate = sess->next_channel_id++;
		bool     in_use = false;

		for (size_t i = 0; i < sess->channel_count; i++) {
			if (sess->channels[i]->local_id == candidate) {
				in_use = true;
				break;
			}
		}
		if (!in_use) {
			dssh_thrd_check(sess, mtx_unlock(&sess->channel_mtx));
			return candidate;
		}
		if (sess->next_channel_id == start) {
			dssh_thrd_check(sess, mtx_unlock(&sess->channel_mtx));
			return DSSH_ERROR_TOOMANY;
		}
	}
}

/* ================================================================
 * Channel buffer initialization
 * ================================================================ */

static int
init_session_channel(struct dssh_channel_s *ch, uint32_t window_max)
{
	ch->chan_type = DSSH_CHAN_SESSION;
	ch->window_max = window_max;
	ch->stdout_consumed = 0;
	ch->stderr_consumed = 0;
	ch->exit_code = 0;
	ch->exit_code_received = false;
	atomic_store(&ch->closing, false);

	if (mtx_init(&ch->buf_mtx, mtx_plain) != thrd_success)
		return DSSH_ERROR_INIT;
	if (cnd_init(&ch->poll_cnd) != thrd_success) {
		mtx_destroy(&ch->buf_mtx);
		return DSSH_ERROR_INIT;
	}

	int res = bytebuf_init(&ch->buf.session.stdout_buf, window_max);

	if (res < 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		return res;
	}
	res = bytebuf_init(&ch->buf.session.stderr_buf, window_max);
	if (res < 0) {
		bytebuf_free(&ch->buf.session.stdout_buf);
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		return res;
	}
	sigqueue_init(&ch->buf.session.signals);
	return 0;
}

static int
init_raw_channel(struct dssh_channel_s *ch, uint32_t window_max)
{
	ch->chan_type = DSSH_CHAN_RAW;
	ch->window_max = window_max;
	atomic_store(&ch->closing, false);

	if (mtx_init(&ch->buf_mtx, mtx_plain) != thrd_success)
		return DSSH_ERROR_INIT;
	if (cnd_init(&ch->poll_cnd) != thrd_success) {
		mtx_destroy(&ch->buf_mtx);
		return DSSH_ERROR_INIT;
	}

	msgqueue_init(&ch->buf.raw.queue);
	return 0;
}

static void
cleanup_channel_buffers(struct dssh_channel_s *ch)
{
	if (ch->chan_type == DSSH_CHAN_SESSION) {
		bytebuf_free(&ch->buf.session.stdout_buf);
		bytebuf_free(&ch->buf.session.stderr_buf);
		sigqueue_free(&ch->buf.session.signals);
	}
	else if (ch->chan_type == DSSH_CHAN_RAW) {
		msgqueue_free(&ch->buf.raw.queue);
	}
	free(ch->setup_payload);
	ch->setup_payload = NULL;

	if (ch->chan_type != 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		ch->chan_type = 0;
	}
}

/* ================================================================
 * Window replenishment -- called after reads drain buffer
 * ================================================================ */

DSSH_TESTABLE int
maybe_replenish_window(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));

        /* No point replenishing if peer is done sending */
	if (ch->eof_received || ch->close_received) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return 0;
	}

        /* Replenish when window drops below half the target.
         * Cap by free buffer space so we never grant more window
         * than the buffers can absorb. */
	uint32_t add = 0;

	if (ch->local_window < ch->window_max / 2) {
		add = ch->window_max - ch->local_window;
		if (ch->chan_type == DSSH_CHAN_SESSION) {
			size_t buf_free = bytebuf_free_space(
			    &ch->buf.session.stdout_buf);
			size_t err_free = bytebuf_free_space(
			    &ch->buf.session.stderr_buf);
			if (err_free < buf_free)
				buf_free = err_free;
			uint32_t buf_cap = buf_free > UINT32_MAX
			    ? UINT32_MAX : (uint32_t)buf_free;
			if (add > buf_cap)
				add = buf_cap;
		}
	}

	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	if (add > 0)
		return send_window_adjust(sess, ch, add);
	return 0;
}

/* ================================================================
 * Demux thread
 * ================================================================ */

/*
 * Parse the common CHANNEL_REQUEST framing starting at offset 5
 * (after msg_type + recipient_channel):
 *   string  request_type
 *   boolean want_reply
 *   ...     type-specific data
 *
 * Returns 0 on success, DSSH_ERROR_PARSE if the payload is truncated.
 * On success, *rtype points into payload (not a copy).
 */
DSSH_TESTABLE int
parse_channel_request(const uint8_t *payload, size_t payload_len,
    const uint8_t **rtype, uint32_t *rtype_len,
    bool *want_reply,
    const uint8_t **req_data, size_t *req_data_len)
{
	size_t rpos = 5;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	*rtype_len = DSSH_GET_U32(&payload[rpos]);
	rpos += 4;
	if (rpos + *rtype_len + 1 > payload_len)
		return DSSH_ERROR_PARSE;

	*rtype = &payload[rpos];
	rpos += *rtype_len;

	*want_reply = (payload[rpos] != 0);
	rpos++;

	*req_data = &payload[rpos];
	*req_data_len = payload_len - rpos;
	return 0;
}

/*
 * Handle CHANNEL_REQUEST -- dispatch signal/exit-status/window-change
 * for session channels; send CHANNEL_FAILURE for unhandled want_reply.
 * Called under buf_mtx.  Returns 1 if the caller should return
 * immediately (channel closing detected during unlock-relock), 0 otherwise.
 */
static int
handle_channel_request(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    const uint8_t *payload, size_t payload_len)
{
	const uint8_t *rtype;
	uint32_t rtype_len;
	bool want_reply;
	const uint8_t *rdata;
	size_t rdata_len;

	if (parse_channel_request(payload, payload_len,
	    &rtype, &rtype_len, &want_reply, &rdata, &rdata_len) < 0)
		return 0;

	if (ch->chan_type == DSSH_CHAN_SESSION) {
		if ((rtype_len == DSSH_STRLEN(str_signal)) && (memcmp(rtype, str_signal, DSSH_STRLEN(str_signal)) == 0)) {
			/* Queue signal with current stream positions */
			if (rdata_len >= 4) {
				uint32_t sname_len = DSSH_GET_U32(rdata);

				if (4 + sname_len <= rdata_len) {
					char   sname[32];
					size_t sn = sname_len
					    < sizeof(sname) - 1 ? sname_len : sizeof(sname) - 1;

					memcpy(sname, &rdata[4], sn);
					sname[sn] = 0;
					sigqueue_push(&ch->buf.session.signals,
					    sname,
					    ch->buf.session.stdout_buf.total,
					    ch->buf.session.stderr_buf.total);
				}
			}
		}
		else if ((rtype_len == DSSH_STRLEN(str_exit_status)) && (memcmp(rtype, str_exit_status, DSSH_STRLEN(str_exit_status)) == 0)) {
			if (rdata_len >= 4) {
				ch->exit_code = DSSH_GET_U32(rdata);
				ch->exit_code_received = true;
			}
		}
		else if ((rtype_len == DSSH_STRLEN(str_window_change)) && (memcmp(rtype, str_window_change, DSSH_STRLEN(str_window_change)) == 0)) {
			void (*wc_cb)(uint32_t, uint32_t, uint32_t, uint32_t,
				    void *) = ch->window_change_cb;

			if ((wc_cb != NULL) && (rdata_len >= 16)) {
				uint32_t wc_cols = DSSH_GET_U32(rdata);
				uint32_t wc_rows = DSSH_GET_U32(&rdata[4]);
				uint32_t wc_wpx = DSSH_GET_U32(&rdata[8]);
				uint32_t wc_hpx = DSSH_GET_U32(&rdata[12]);

				dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
				wc_cb(wc_cols, wc_rows, wc_wpx, wc_hpx,
				    ch->window_change_cbdata);
				dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
				if (atomic_load(&ch->closing)) {
					dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
					return 1;
				}
			}
		}

		/* Ignore: xon-xoff, exit-signal, pty-req from server, etc. */
	}

	/* Send FAILURE for unhandled requests with want_reply */
	if (want_reply) {
		uint8_t fail[8];
		size_t  fp = 0;

		fail[fp++] = SSH_MSG_CHANNEL_FAILURE;
		DSSH_PUT_U32(ch->remote_id, fail, &fp);
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		send_or_queue(sess, fail, fp);
		dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
		if (atomic_load(&ch->closing)) {
			dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
			return 1;
		}
	}
	return 0;
}

/*
 * Handle CHANNEL_DATA -- write to stdout (session) or raw queue.
 * Called under buf_mtx.
 */
static void
handle_channel_data(struct dssh_channel_s *ch,
    const uint8_t *payload, size_t payload_len)
{
	if (payload_len < 9)
		return;
	if (ch->eof_received || ch->close_received)
		return;

	uint32_t dlen = DSSH_GET_U32(&payload[5]);

	if (9 + dlen > payload_len)
		return;

	const uint8_t *data = &payload[9];
	uint32_t consumed = dlen;

	if (ch->chan_type == DSSH_CHAN_SESSION) {
		size_t written = bytebuf_write(
		    &ch->buf.session.stdout_buf, data, dlen);
		consumed = (uint32_t)written;
	}
	else if (ch->chan_type == DSSH_CHAN_RAW)
		msgqueue_push(&ch->buf.raw.queue, data, dlen);
	if (consumed <= ch->local_window)
		ch->local_window -= consumed;
	else
		ch->local_window = 0;
}

/*
 * Handle CHANNEL_EXTENDED_DATA -- write to stderr (session only).
 * Called under buf_mtx.
 */
static void
handle_channel_extended_data(struct dssh_channel_s *ch,
    const uint8_t *payload, size_t payload_len)
{
	if (payload_len < 13)
		return;
	if (ch->eof_received || ch->close_received)
		return;

	uint32_t data_type = DSSH_GET_U32(&payload[5]);
	uint32_t dlen = DSSH_GET_U32(&payload[9]);

	if (13 + dlen > payload_len)
		return;

	const uint8_t *data = &payload[13];
	uint32_t consumed = dlen;

	if ((ch->chan_type == DSSH_CHAN_SESSION) && (data_type == 1)) {
		size_t written = bytebuf_write(
		    &ch->buf.session.stderr_buf, data, dlen);
		consumed = (uint32_t)written;
	}
	if (consumed <= ch->local_window)
		ch->local_window -= consumed;
	else
		ch->local_window = 0;
}

/*
 * Dispatch a received packet to the appropriate channel buffer.
 * Called from the demux thread with no locks held.
 */
DSSH_TESTABLE int
demux_dispatch(struct dssh_session_s *sess, uint8_t msg_type,
    uint8_t *payload, size_t payload_len)
{
        /* All channel messages have the recipient channel at payload[1..4] */
	if (payload_len < 5)
		return DSSH_ERROR_PARSE;

	uint32_t local_id = DSSH_GET_U32(&payload[1]);

	struct dssh_channel_s *ch = find_channel(sess, local_id);

	if (ch == NULL)
		return 0; /* late message for closed channel */

        /* find_channel() returns with buf_mtx held */

        /*
         * Setup mode: during session_accept_channel, deliver messages
         * via the mailbox instead of processing them normally.
         */
	if (ch->setup_mode) {
		/* Wait for the accept loop to consume the previous message */
		while (ch->setup_ready && !sess->terminate)
			dssh_thrd_check(sess, cnd_wait(&ch->poll_cnd, &ch->buf_mtx));
		free(ch->setup_payload);
		ch->setup_payload = malloc(payload_len);
		if (ch->setup_payload != NULL) {
			memcpy(ch->setup_payload, payload, payload_len);
			ch->setup_payload_len = payload_len;
			ch->setup_msg_type = msg_type;
			ch->setup_ready = true;
		}
		else {
			ch->setup_error = true;
		}
		dssh_thrd_check(sess, cnd_broadcast(&ch->poll_cnd));
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return 0;
	}

	if (ch->chan_type == 0) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return 0;
	}

	switch (msg_type) {
		case SSH_MSG_CHANNEL_DATA:
			handle_channel_data(ch, payload, payload_len);
			break;

		case SSH_MSG_CHANNEL_EXTENDED_DATA:
			handle_channel_extended_data(ch, payload, payload_len);
			break;

		case SSH_MSG_CHANNEL_WINDOW_ADJUST:
			if (payload_len >= 9) {
				uint32_t bytes = DSSH_GET_U32(&payload[5]);

				ch->remote_window = window_add(ch->remote_window, bytes);
			}
			break;

		case SSH_MSG_CHANNEL_EOF:
			ch->eof_received = true;
			break;

		case SSH_MSG_CHANNEL_CLOSE:
			ch->close_received = true;

                        /*
                         * Don't auto-send the reciprocal CLOSE here.  The
                         * application needs a chance to drain buffered data,
                         * send exit-status, and clean up before the channel
                         * is fully closed.  poll() will wake with POLL_READ
                         * (read returns 0), and the application calls
                         * session_close() or channel_close() which sends
                         * the reciprocal CLOSE via conn_close() (idempotent).
                         */
			break;

		case SSH_MSG_CHANNEL_REQUEST:
			if (handle_channel_request(sess, ch, payload, payload_len))
				return 0;
			break;

		case SSH_MSG_CHANNEL_OPEN_FAILURE:
                        /* Server rejected our CHANNEL_OPEN.  Mark the channel
                         * so open_session_channel's wait loop exits. */
			ch->close_received = true;
			break;

		case SSH_MSG_CHANNEL_SUCCESS:
		case SSH_MSG_CHANNEL_FAILURE:
			if (ch->request_pending) {
				ch->request_success = (msg_type == SSH_MSG_CHANNEL_SUCCESS);
				ch->request_responded = true;
			}
			break;
	}

	dssh_thrd_check(sess, cnd_broadcast(&ch->poll_cnd));
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	return 0;
}

/*
 * Handle CHANNEL_OPEN_CONFIRMATION -- recipient_channel is at offset 1.
 */
static int
demux_open_confirmation(struct dssh_session_s *sess,
    uint8_t *payload, size_t payload_len)
{
	if (payload_len < 1 + 16)
		return DSSH_ERROR_PARSE;

	uint32_t local_id = DSSH_GET_U32(&payload[1]);

	struct dssh_channel_s *ch = find_channel(sess, local_id);

	if (ch == NULL)
		return 0; /* late message for closed channel */

	/* find_channel() returns with buf_mtx held */
	ch->remote_id = DSSH_GET_U32(&payload[5]);
	ch->remote_window = DSSH_GET_U32(&payload[9]);
	ch->remote_max_packet = DSSH_GET_U32(&payload[13]);
	ch->open = true;
	dssh_thrd_check(sess, cnd_broadcast(&ch->poll_cnd));
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	return 0;
}

/*
 * Handle incoming CHANNEL_OPEN -- either queue for accept or auto-reject.
 */
static int
demux_channel_open(struct dssh_session_s *sess, uint8_t *payload, size_t payload_len)
{
        /* Parse: string channel_type, uint32 sender_channel,
         * uint32 initial_window, uint32 max_packet */
	size_t   rpos = 1;
	uint32_t type_len;

	if (rpos + 4 > payload_len)
		return DSSH_ERROR_PARSE;
	type_len = DSSH_GET_U32(&payload[rpos]);
	rpos += 4;
	if (rpos + type_len > payload_len)
		return DSSH_ERROR_PARSE;

	const uint8_t *ctype = &payload[rpos];

	rpos += type_len;

	if (rpos + 12 > payload_len)
		return DSSH_ERROR_PARSE;

	uint32_t peer_channel = DSSH_GET_U32(&payload[rpos]);

	rpos += 4;
	uint32_t peer_window = DSSH_GET_U32(&payload[rpos]);

	rpos += 4;
	uint32_t peer_max_packet = DSSH_GET_U32(&payload[rpos]);

        /* Auto-reject forbidden channel types */
	bool reject = false;

	if ((type_len == DSSH_STRLEN(str_x11)) && (memcmp(ctype, str_x11, DSSH_STRLEN(str_x11)) == 0))
		reject = true;
	if ((type_len == DSSH_STRLEN(str_forwarded_tcpip)) && (memcmp(ctype, str_forwarded_tcpip, DSSH_STRLEN(str_forwarded_tcpip)) == 0))
		reject = true;
	if ((type_len == DSSH_STRLEN(str_direct_tcpip)) && (memcmp(ctype, str_direct_tcpip, DSSH_STRLEN(str_direct_tcpip)) == 0))
		reject = true;

        /* Client rejects "session" opens from server (RFC 4254 s6.1) */
	if (sess->trans.client && (type_len == DSSH_STRLEN(str_session)) && (memcmp(ctype, str_session, DSSH_STRLEN(str_session)) == 0))
		reject = true;

	if (reject) {
                /* Send CHANNEL_OPEN_FAILURE */
		uint8_t fail[64];
		size_t  fp = 0;

		fail[fp++] = SSH_MSG_CHANNEL_OPEN_FAILURE;
		DSSH_PUT_U32(peer_channel, fail, &fp);
		DSSH_PUT_U32(SSH_OPEN_ADMINISTRATIVELY_PROHIBITED, fail, &fp);
		DSSH_PUT_U32(0, fail, &fp);
		DSSH_PUT_U32(0, fail, &fp);
		send_or_queue(sess, fail, fp);
		return 0;
	}

        /* Queue for application's accept() call */
	dssh_thrd_check(sess, mtx_lock(&sess->accept_mtx));
	acceptqueue_push(&sess->accept_queue,
	    peer_channel, peer_window, peer_max_packet, ctype, type_len);
	dssh_thrd_check(sess, cnd_signal(&sess->accept_cnd));
	dssh_thrd_check(sess, mtx_unlock(&sess->accept_mtx));
	return 0;
}

static int
demux_thread_func(void *arg)
{
	struct dssh_session_s *sess = arg;
	uint8_t      msg_type;
	uint8_t     *payload;
	size_t       payload_len;

#ifdef DSSH_TESTING
	dssh_test_ossl_exclude_thread();
	if (dssh_test_alloc_new_threads_excluded())
		dssh_test_alloc_exclude_thread();
#endif

	while (!sess->terminate) {
		int res = recv_packet(sess,
		        &msg_type, &payload, &payload_len);

		if (res < 0) {
			if (res == DSSH_ERROR_TERMINATED)
				break;

                        /* Other errors -- terminate */
			session_set_terminate(sess);
			break;
		}

		int dres = 0;

		if (msg_type == SSH_MSG_CHANNEL_OPEN)
			dres = demux_channel_open(sess, payload, payload_len);
		else if (msg_type == SSH_MSG_CHANNEL_OPEN_CONFIRMATION)
			dres = demux_open_confirmation(sess, payload, payload_len);
		else if ((msg_type >= SSH_MSG_CHANNEL_OPEN_FAILURE)
		    && (msg_type <= SSH_MSG_CHANNEL_FAILURE))
			dres = demux_dispatch(sess, msg_type, payload, payload_len);

		/* Parse errors from malformed peer messages are
		 * non-fatal -- skip the packet.  I/O and serialize
		 * errors mean the connection is broken. */
		if (dres < 0 && dres != DSSH_ERROR_PARSE) {
			session_set_terminate(sess);
			break;
		}
                /* All other message types (GLOBAL_REQUEST, KEXINIT, etc.)
                 * are already handled by recv_packet transparently */
	}

        /* Wake any waiters so they see termination */
	dssh_thrd_check(sess, mtx_lock(&sess->accept_mtx));
	dssh_thrd_check(sess, cnd_broadcast(&sess->accept_cnd));
	dssh_thrd_check(sess, mtx_unlock(&sess->accept_mtx));

        /* Wake all registered channels.  buf_mtx and poll_cnd are
         * initialized before register_channel(), so this is safe even
         * for channels still in setup mode (chan_type == 0).
         * Lock order: channel_mtx then buf_mtx. */
	dssh_thrd_check(sess, mtx_lock(&sess->channel_mtx));
	for (size_t i = 0; i < sess->channel_count; i++) {
		struct dssh_channel_s *ch = sess->channels[i];

		dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
		dssh_thrd_check(sess, cnd_broadcast(&ch->poll_cnd));
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	}
	dssh_thrd_check(sess, mtx_unlock(&sess->channel_mtx));

	sess->demux_running = false;
	return 0;
}

DSSH_PUBLIC int
dssh_session_start(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (sess->conn_initialized)
		return DSSH_ERROR_INIT;

	if (mtx_init(&sess->channel_mtx, mtx_plain) != thrd_success)
		return DSSH_ERROR_INIT;
	if (mtx_init(&sess->accept_mtx, mtx_plain) != thrd_success) {
		mtx_destroy(&sess->channel_mtx);
		return DSSH_ERROR_INIT;
	}
	if (cnd_init(&sess->accept_cnd) != thrd_success) {
		mtx_destroy(&sess->accept_mtx);
		mtx_destroy(&sess->channel_mtx);
		return DSSH_ERROR_INIT;
	}

	sess->channels = NULL;
	sess->channel_count = 0;
	sess->channel_capacity = 0;
	sess->next_channel_id = 0;
	acceptqueue_init(&sess->accept_queue);
	sess->conn_initialized = true;

	sess->demux_running = true;
	if (thrd_create(&sess->demux_thread, demux_thread_func, sess) != thrd_success) {
		sess->demux_running = false;
		cnd_destroy(&sess->accept_cnd);
		mtx_destroy(&sess->accept_mtx);
		mtx_destroy(&sess->channel_mtx);
		return DSSH_ERROR_INIT;
	}

	return 0;
}

DSSH_PUBLIC void
dssh_session_stop(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return;
	if (!sess->demux_running && (sess->channels == NULL))
		return;

        /* Signal termination and join the demux thread */
	if (sess->demux_running) {
		session_set_terminate(sess);

		int demux_res;

		thrd_join(sess->demux_thread, &demux_res);
	}

        /* Clean up registered channels */
	if (sess->channels != NULL) {
                /* Channels in the array are never NULL; registration
                 * always stores a valid pointer. */
		for (size_t i = 0; i < sess->channel_count; i++) {
			struct dssh_channel_s *ch = sess->channels[i];

			if (ch != NULL) {
				cleanup_channel_buffers(ch);
				free(ch);
			}
		}
		free(sess->channels);
		sess->channels = NULL;
		sess->channel_count = 0;
		sess->channel_capacity = 0;
	}

	acceptqueue_free(&sess->accept_queue);
	cnd_destroy(&sess->accept_cnd);
	mtx_destroy(&sess->accept_mtx);
	mtx_destroy(&sess->channel_mtx);
	sess->conn_initialized = false;
}

/* ================================================================
 * Session accept -- wait for incoming CHANNEL_OPEN
 * ================================================================ */

DSSH_PUBLIC int
dssh_session_accept(struct dssh_session_s *sess,
    struct dssh_incoming_open **inc, int timeout_ms)
{
	struct timespec ts;

	if (inc == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	if (timeout_ms > 0)
		deadline_from_ms(&ts, timeout_ms);
	dssh_thrd_check(sess, mtx_lock(&sess->accept_mtx));

	while (sess->accept_queue.head == NULL) {
		if (sess->terminate || !sess->demux_running) {
			dssh_thrd_check(sess, mtx_unlock(&sess->accept_mtx));
			return DSSH_ERROR_TERMINATED;
		}
		if (timeout_ms == 0) {
			dssh_thrd_check(sess, mtx_unlock(&sess->accept_mtx));
			return DSSH_ERROR_NOMORE;
		}
		if (timeout_ms < 0) {
			dssh_thrd_check(sess, cnd_wait(&sess->accept_cnd, &sess->accept_mtx));
		}
		else {
			if (dssh_thrd_check(sess, cnd_timedwait(&sess->accept_cnd, &sess->accept_mtx, &ts)) == thrd_timedout) {
				dssh_thrd_check(sess, mtx_unlock(&sess->accept_mtx));
				return DSSH_ERROR_NOMORE;
			}
		}
	}

	*inc = acceptqueue_pop(&sess->accept_queue);
	dssh_thrd_check(sess, mtx_unlock(&sess->accept_mtx));
	return 0;
}

DSSH_PUBLIC int
dssh_session_reject(struct dssh_session_s *sess,
    struct dssh_incoming_open *inc, uint32_t reason_code,
    const char *description)
{
	if (inc == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	size_t  dlen = description ? strlen(description) : 0;
	uint8_t msg[256];

	if (dlen > sizeof(msg) - 17) /* msg_type + 3xuint32 + language uint32 */
		dlen = sizeof(msg) - 17;

	size_t pos = 0;
	int    ret;

	msg[pos++] = SSH_MSG_CHANNEL_OPEN_FAILURE;
	DSSH_PUT_U32(inc->peer_channel, msg, &pos);
	DSSH_PUT_U32(reason_code, msg, &pos);
	DSSH_PUT_U32((uint32_t)dlen, msg, &pos);
	if (dlen > 0) {
		if (dlen > sizeof(msg) - pos - 4)
			dlen = sizeof(msg) - pos - 4;
		memcpy(&msg[pos], description, dlen);
		pos += dlen;
	}
	DSSH_PUT_U32(0, msg, &pos);
	ret = send_packet(sess, msg, pos, NULL);
	free(inc);
	return ret;
}

/* ================================================================
 * High-level channel open helpers
 * ================================================================ */

/*
 * Send CHANNEL_OPEN_CONFIRMATION for an incoming channel.
 */
static int
send_open_confirmation(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, uint32_t peer_channel)
{
	uint8_t msg[32];
	size_t  pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_OPEN_CONFIRMATION;
	DSSH_PUT_U32(peer_channel, msg, &pos);
	DSSH_PUT_U32(ch->local_id, msg, &pos);
	DSSH_PUT_U32(ch->local_window, msg, &pos);
	DSSH_PUT_U32(MAX_PACKET_SIZE, msg, &pos);
	return send_packet(sess, msg, pos, NULL);
}

/*
 * Common channel open: send CHANNEL_OPEN "session", wait for confirmation.
 * Does NOT start the demux -- the channel is registered for demux dispatch.
 */
static int
open_session_channel(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
        /* Caller has already initialized buffers (init_session_channel
         * or init_raw_channel).  Set wire-level fields only. */
	int64_t cid = alloc_channel_id(sess);
	if (cid < 0)
		return (int)cid;
	ch->local_id = (uint32_t)cid;
	ch->local_window = INITIAL_WINDOW_SIZE;
	ch->open = false;
	ch->close_sent = false;
	ch->close_received = false;
	ch->eof_sent = false;
	ch->eof_received = false;

        /* Register the channel first so the demux thread can dispatch
         * CHANNEL_OPEN_CONFIRMATION to it, then send CHANNEL_OPEN and
         * wait for the demux thread to set the open flag. */
	uint8_t msg[256];
	size_t  pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_OPEN;
	DSSH_PUT_U32(DSSH_STRLEN(str_session), msg, &pos);
	memcpy(&msg[pos], str_session, DSSH_STRLEN(str_session));
	pos += DSSH_STRLEN(str_session);
	DSSH_PUT_U32(ch->local_id, msg, &pos);
	DSSH_PUT_U32(ch->local_window, msg, &pos);
	DSSH_PUT_U32(MAX_PACKET_SIZE, msg, &pos);

	int res = register_channel(sess, ch);

	if (res < 0)
		return res;

	res = send_packet(sess, msg, pos, NULL);
	if (res < 0) {
		unregister_channel(sess, ch);
		return res;
	}

        /* Wait for CONFIRMATION -- the demux thread handles it */
	struct timespec ts;
	if (sess->timeout_ms > 0)
		deadline_from_ms(&ts, sess->timeout_ms);
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	while (!ch->open && !ch->close_received && !sess->terminate) {
		if (sess->timeout_ms <= 0) {
			dssh_thrd_check(sess, cnd_wait(&ch->poll_cnd,
			    &ch->buf_mtx));
		}
		else {
			if (dssh_thrd_check(sess, cnd_timedwait(&ch->poll_cnd,
			    &ch->buf_mtx, &ts)) == thrd_timedout) {
				dssh_thrd_check(sess,
				    mtx_unlock(&ch->buf_mtx));
				unregister_channel(sess, ch);
				return DSSH_ERROR_TIMEOUT;
			}
		}
	}
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	if (!ch->open) {
		unregister_channel(sess, ch);
		return DSSH_ERROR_REJECTED;
	}
	return 0;
}

/*
 * Send a CHANNEL_REQUEST with want_reply=true and wait for the demux
 * thread to deliver SUCCESS or FAILURE.  Returns 0 on SUCCESS,
 * DSSH_ERROR_INIT on FAILURE.
 */
static int
send_channel_request_wait(struct dssh_session_s *sess,
    struct dssh_channel_s *ch,
    const char *req_type, const uint8_t *extra, size_t extra_len)
{
	size_t   rtlen = strlen(req_type);

	if (rtlen > UINT32_MAX)
		return DSSH_ERROR_INVALID;
	if (extra_len > SIZE_MAX - CHAN_REQUEST_FIXED - rtlen)
		return DSSH_ERROR_INVALID;

	size_t   msg_len = CHAN_REQUEST_FIXED + rtlen + extra_len;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);
	DSSH_PUT_U32((uint32_t)rtlen, msg, &pos);
	memcpy(&msg[pos], req_type, rtlen);
	pos += rtlen;
	msg[pos++] = 1; /* want_reply = TRUE */
	if (extra_len > 0) {
		memcpy(&msg[pos], extra, extra_len);
		pos += extra_len;
	}

        /* Set up the pending response slot */
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	ch->request_pending = true;
	ch->request_responded = false;
	ch->request_success = false;
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	int res = send_packet(sess, msg, pos, NULL);

	free(msg);
	if (res < 0) {
		dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
		ch->request_pending = false;
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return res;
	}

        /* Wait for the demux thread to deliver the response */
	struct timespec rts;
	if (sess->timeout_ms > 0)
		deadline_from_ms(&rts, sess->timeout_ms);
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	while (!ch->request_responded && !sess->terminate
	    && !ch->close_received) {
		if (sess->timeout_ms <= 0) {
			dssh_thrd_check(sess, cnd_wait(&ch->poll_cnd,
			    &ch->buf_mtx));
		}
		else {
			if (dssh_thrd_check(sess, cnd_timedwait(&ch->poll_cnd,
			    &ch->buf_mtx, &rts)) == thrd_timedout) {
				ch->request_pending = false;
				ch->request_responded = false;
				dssh_thrd_check(sess,
				    mtx_unlock(&ch->buf_mtx));
				return DSSH_ERROR_TIMEOUT;
			}
		}
	}

	bool success = ch->request_success;

	ch->request_pending = false;
	ch->request_responded = false;
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	if (sess->terminate || ch->close_received)
		return DSSH_ERROR_TERMINATED;
	return success ? 0 : DSSH_ERROR_REJECTED;
}

/* ================================================================
 * High-level open functions
 * ================================================================ */

DSSH_PUBLIC struct dssh_channel_s *
dssh_session_open_shell(struct dssh_session_s *sess,
    const struct dssh_pty_req       *pty)
{
	if (pty == NULL || sess == NULL)
		return NULL;
	struct dssh_channel_s *ch = calloc(1, sizeof(*ch));

	if (ch == NULL)
		return NULL;

	int res = init_session_channel(ch, INITIAL_WINDOW_SIZE);

	if (res < 0) {
		free(ch);
		return NULL;
	}

	res = open_session_channel(sess, ch);
	if (res < 0) {
		cleanup_channel_buffers(ch);
		free(ch);
		return NULL;
	}

        /* Send pty-req */
	{
		size_t   tlen = strlen(pty->term);
		size_t   mlen = pty->modes_len;

		if (tlen > UINT32_MAX || mlen > UINT32_MAX) {
			conn_close(sess, ch);
			unregister_channel(sess, ch);
			cleanup_channel_buffers(ch);
			free(ch);
			return NULL;
		}

		if (tlen > SIZE_MAX - PTY_REQ_FIXED
		    || mlen > SIZE_MAX - PTY_REQ_FIXED - tlen) {
			conn_close(sess, ch);
			unregister_channel(sess, ch);
			cleanup_channel_buffers(ch);
			free(ch);
			return NULL;
		}
		size_t   extra_len = PTY_REQ_FIXED + tlen + mlen;
		uint8_t *extra = malloc(extra_len);

		if (extra == NULL) {
			conn_close(sess, ch);
			unregister_channel(sess, ch);
			cleanup_channel_buffers(ch);
			free(ch);
			return NULL;
		}

		size_t ep = 0;

		DSSH_PUT_U32((uint32_t)tlen, extra, &ep);
		memcpy(&extra[ep], pty->term, tlen);
		ep += tlen;
		DSSH_PUT_U32(pty->cols, extra, &ep);
		DSSH_PUT_U32(pty->rows, extra, &ep);
		DSSH_PUT_U32(pty->wpx, extra, &ep);
		DSSH_PUT_U32(pty->hpx, extra, &ep);
		DSSH_PUT_U32((uint32_t)mlen, extra, &ep);

		if (mlen > 0)
			memcpy(&extra[ep], pty->modes, mlen);
		ep += mlen;
		res = send_channel_request_wait(sess, ch, "pty-req", extra, ep);
		free(extra);
		if (res < 0)
			goto open_fail;
	}

        /* Send shell */
	res = send_channel_request_wait(sess, ch, "shell", NULL, 0);
	if (res < 0)
		goto open_fail;

	return ch;

open_fail:
	conn_close(sess, ch);
	unregister_channel(sess, ch);
	cleanup_channel_buffers(ch);
	free(ch);
	return NULL;
}

DSSH_PUBLIC struct dssh_channel_s *
dssh_session_open_exec(struct dssh_session_s *sess,
    const char                     *command)
{
	if (command == NULL || sess == NULL)
		return NULL;
	struct dssh_channel_s *ch = calloc(1, sizeof(*ch));

	if (ch == NULL)
		return NULL;

	int res = init_session_channel(ch, INITIAL_WINDOW_SIZE);

	if (res < 0) {
		free(ch);
		return NULL;
	}

	res = open_session_channel(sess, ch);
	if (res < 0) {
		cleanup_channel_buffers(ch);
		free(ch);
		return NULL;
	}

        /* Send exec */
	size_t   cmdlen = strlen(command);

	if (cmdlen > UINT32_MAX || cmdlen > SIZE_MAX - 4) {
		res = DSSH_ERROR_INVALID;
		goto open_fail;
	}

	size_t   extra_len = 4 + cmdlen;
	uint8_t *extra = malloc(extra_len);

	if (extra == NULL) {
		res = DSSH_ERROR_ALLOC;
		goto open_fail;
	}

	size_t ep = 0;

	DSSH_PUT_U32((uint32_t)cmdlen, extra, &ep);
	memcpy(&extra[ep], command, cmdlen);
	ep += cmdlen;
	res = send_channel_request_wait(sess, ch, "exec", extra, ep);
	free(extra);
	if (res < 0)
		goto open_fail;

	return ch;

open_fail:
	conn_close(sess, ch);
	unregister_channel(sess, ch);
	cleanup_channel_buffers(ch);
	free(ch);
	return NULL;
}

DSSH_PUBLIC struct dssh_channel_s *
dssh_channel_open_subsystem(struct dssh_session_s *sess,
    const char                          *subsystem)
{
	if (subsystem == NULL || sess == NULL)
		return NULL;
	struct dssh_channel_s *ch = calloc(1, sizeof(*ch));

	if (ch == NULL)
		return NULL;

	int res = init_raw_channel(ch, INITIAL_WINDOW_SIZE);

	if (res < 0) {
		free(ch);
		return NULL;
	}

	res = open_session_channel(sess, ch);
	if (res < 0) {
		cleanup_channel_buffers(ch);
		free(ch);
		return NULL;
	}

        /* Send subsystem */
	size_t   slen = strlen(subsystem);

	if (slen > UINT32_MAX || slen > SIZE_MAX - 4) {
		res = DSSH_ERROR_INVALID;
		goto open_fail;
	}

	size_t   extra_len = 4 + slen;
	uint8_t *extra = malloc(extra_len);

	if (extra == NULL) {
		res = DSSH_ERROR_ALLOC;
		goto open_fail;
	}

	size_t ep = 0;

	DSSH_PUT_U32((uint32_t)slen, extra, &ep);
	memcpy(&extra[ep], subsystem, slen);
	ep += slen;
	res = send_channel_request_wait(sess, ch, "subsystem", extra, ep);
	free(extra);
	if (res < 0)
		goto open_fail;

	return ch;

open_fail:
	conn_close(sess, ch);
	unregister_channel(sess, ch);
	cleanup_channel_buffers(ch);
	free(ch);
	return NULL;
}

/* ================================================================
 * Server-side accept helpers
 * ================================================================ */

DSSH_PUBLIC struct dssh_channel_s *
dssh_channel_accept_raw(struct dssh_session_s *sess,
    struct dssh_incoming_open       *inc)
{
	if (inc == NULL || sess == NULL)
		return NULL;
	struct dssh_channel_s *ch = calloc(1, sizeof(*ch));

	if (ch == NULL) {
		free(inc);
		return NULL;
	}

	int64_t cid = alloc_channel_id(sess);
	if (cid < 0) {
		free(inc);
		free(ch);
		return NULL;
	}
	ch->local_id = (uint32_t)cid;
	ch->local_window = INITIAL_WINDOW_SIZE;
	ch->remote_id = inc->peer_channel;
	ch->remote_window = inc->peer_window;
	ch->remote_max_packet = inc->peer_max_packet;

	int res = init_raw_channel(ch, INITIAL_WINDOW_SIZE);

	if (res < 0) {
		free(inc);
		free(ch);
		return NULL;
	}

	res = register_channel(sess, ch);
	if (res < 0) {
		cleanup_channel_buffers(ch);
		free(inc);
		free(ch);
		return NULL;
	}

	res = send_open_confirmation(sess, ch, inc->peer_channel);
	free(inc);
	if (res < 0) {
		unregister_channel(sess, ch);
		cleanup_channel_buffers(ch);
		free(ch);
		return NULL;
	}

	ch->open = true;
	return ch;
}

/* ================================================================
 * Server-side session accept (callback-driven setup)
 * ================================================================ */

/*
 * Wait for the next message in the setup mailbox.
 * Returns 0 on success, negative on error/termination.
 */
static int
setup_recv(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len)
{
	struct timespec sts;
	if (sess->timeout_ms > 0)
		deadline_from_ms(&sts, sess->timeout_ms);
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	while (!ch->setup_ready && !ch->setup_error
	    && !sess->terminate && !ch->close_received) {
		if (sess->timeout_ms <= 0) {
			dssh_thrd_check(sess, cnd_wait(&ch->poll_cnd,
			    &ch->buf_mtx));
		}
		else {
			if (dssh_thrd_check(sess, cnd_timedwait(&ch->poll_cnd,
			    &ch->buf_mtx, &sts)) == thrd_timedout) {
				dssh_thrd_check(sess,
				    mtx_unlock(&ch->buf_mtx));
				return DSSH_ERROR_TIMEOUT;
			}
		}
	}

	if (ch->setup_error) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return DSSH_ERROR_ALLOC;
	}

	if (!ch->setup_ready) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return DSSH_ERROR_TERMINATED;
	}

	*msg_type = ch->setup_msg_type;
	*payload = ch->setup_payload;
	*payload_len = ch->setup_payload_len;
	ch->setup_ready = false;

        /* Caller takes ownership of setup_payload and must free it */
	ch->setup_payload = NULL;
	ch->setup_payload_len = 0;

        /* Wake demux if it's waiting to deliver the next message */
	dssh_thrd_check(sess, cnd_broadcast(&ch->poll_cnd));
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	return 0;
}

/*
 * Send CHANNEL_SUCCESS or CHANNEL_FAILURE for a setup request.
 */
static int
setup_reply(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    bool success)
{
	uint8_t msg[8];
	size_t  pos = 0;

	msg[pos++] = success ? SSH_MSG_CHANNEL_SUCCESS : SSH_MSG_CHANNEL_FAILURE;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);
	return send_packet(sess, msg, pos, NULL);
}

/*
 * Allocate a channel, init sync primitives, register with the session,
 * and send CHANNEL_OPEN_CONFIRMATION.  Frees `inc` on all paths.
 * Returns the channel or NULL on failure.
 */
static struct dssh_channel_s *
accept_channel_init(struct dssh_session_s *sess, struct dssh_incoming_open *inc)
{
	struct dssh_channel_s *ch = calloc(1, sizeof(*ch));

	if (ch == NULL) {
		free(inc);
		return NULL;
	}
	int64_t cid = alloc_channel_id(sess);
	if (cid < 0) {
		free(inc);
		free(ch);
		return NULL;
	}
	ch->local_id = (uint32_t)cid;
	ch->local_window = INITIAL_WINDOW_SIZE;
	ch->remote_id = inc->peer_channel;
	ch->remote_window = inc->peer_window;
	ch->remote_max_packet = inc->peer_max_packet;

	if (mtx_init(&ch->buf_mtx, mtx_plain) != thrd_success) {
		free(inc);
		free(ch);
		return NULL;
	}
	if (cnd_init(&ch->poll_cnd) != thrd_success) {
		mtx_destroy(&ch->buf_mtx);
		free(inc);
		free(ch);
		return NULL;
	}
	ch->setup_mode = true;

	int res = register_channel(sess, ch);

	if (res < 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		free(inc);
		free(ch);
		return NULL;
	}

	res = send_open_confirmation(sess, ch, inc->peer_channel);
	free(inc);
	if (res < 0) {
		unregister_channel(sess, ch);
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->buf_mtx);
		free(ch);
		return NULL;
	}
	ch->open = true;
	return ch;
}

/*
 * Process setup requests until a terminal request (shell/exec/subsystem)
 * is accepted or rejected by the application callback.
 * Returns 0 on success (terminal request accepted), negative on failure.
 * On success, sets ch->req_type, ch->req_data, and *is_subsystem.
 */
static int
accept_setup_loop(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    const struct dssh_server_session_cbs *cbs, bool *is_subsystem)
{
	ch->req_type[0] = 0;
	ch->req_data[0] = 0;

	for (;;) {
		uint8_t  msg_type;
		uint8_t *payload;
		size_t   payload_len;

		int res = setup_recv(sess, ch, &msg_type, &payload, &payload_len);

		if (res < 0)
			return res;

		if (msg_type != SSH_MSG_CHANNEL_REQUEST) {
			/* Handle WINDOW_ADJUST during setup */
			if ((msg_type == SSH_MSG_CHANNEL_WINDOW_ADJUST) && (payload_len >= 9)) {
				uint32_t bytes;

				bytes = DSSH_GET_U32(&payload[5]);
				ch->remote_window = window_add(ch->remote_window, bytes);
			}
			free(payload);
			continue;
		}

		const uint8_t *rtype;
		uint32_t rtype_len;
		bool want_reply;
		const uint8_t *req_data;
		size_t req_data_len;

		if (parse_channel_request(payload, payload_len,
		    &rtype, &rtype_len, &want_reply,
		    &req_data, &req_data_len) < 0) {
			free(payload);
			continue;
		}

		/* Call the application callback */
		int cb_res = 0;

		if (cbs->request_cb != NULL) {
			cb_res = cbs->request_cb((const char *)rtype, rtype_len,
			        want_reply, req_data, req_data_len,
			        cbs->cbdata);
		}

		/* Is this a terminal request (shell/exec/subsystem)? */
		bool is_terminal =
		    (rtype_len == DSSH_STRLEN(str_shell) && memcmp(rtype, str_shell, DSSH_STRLEN(str_shell)) == 0)
		    || (rtype_len == DSSH_STRLEN(str_exec) && memcmp(rtype, str_exec, DSSH_STRLEN(str_exec)) == 0)
		    || (rtype_len == DSSH_STRLEN(str_subsystem) && memcmp(rtype, str_subsystem, DSSH_STRLEN(str_subsystem)) == 0);

		if (want_reply)
			setup_reply(sess, ch, cb_res >= 0);

		if (is_terminal && (cb_res < 0)) {
			/* App rejected the terminal request -- close channel */
			free(payload);
			conn_close(sess, ch);
			return DSSH_ERROR_REJECTED;
		}

		if (is_terminal && (cb_res >= 0)) {
			/* Accepted -- save type and data, determine channel kind */
			size_t tn = rtype_len < sizeof(ch->req_type) - 1
			    ? rtype_len : sizeof(ch->req_type) - 1;

			memcpy(ch->req_type, rtype, tn);
			ch->req_type[tn] = 0;

			/* Extract command/subsystem name for convenience */
			if (req_data_len >= 4) {
				uint32_t dlen = DSSH_GET_U32(req_data);

				size_t dn = dlen < sizeof(ch->req_data) - 1
				    ? dlen : sizeof(ch->req_data) - 1;

				if (4 + dn <= req_data_len)
					memcpy(ch->req_data, &req_data[4], dn);
				ch->req_data[dn] = 0;
			}

			*is_subsystem = (rtype_len == DSSH_STRLEN(str_subsystem)
			    && memcmp(rtype, str_subsystem, DSSH_STRLEN(str_subsystem)) == 0);
			free(payload);
			return 0;
		}

		free(payload);
	}
}

DSSH_PUBLIC struct dssh_channel_s *
dssh_session_accept_channel(struct dssh_session_s *sess,
    struct dssh_incoming_open *inc,
    const struct dssh_server_session_cbs *cbs,
    const char **request_type, const char **request_data)
{
	if (cbs == NULL || inc == NULL || sess == NULL)
		return NULL;

	struct dssh_channel_s *ch = accept_channel_init(sess, inc);

	if (ch == NULL)
		return NULL;

	bool is_subsystem = false;
	int  res = accept_setup_loop(sess, ch, cbs, &is_subsystem);

	if (res < 0)
		goto fail;

	/* Transition from setup mode to normal buffered mode.
	 * buf_mtx and poll_cnd are already initialized from setup.
	 * Channel type depends on the terminal request:
	 *   shell/exec -> DSSH_CHAN_SESSION (stream-based bytebufs)
	 *   subsystem  -> DSSH_CHAN_RAW    (message-based msgqueue)
	 *
	 * Hold buf_mtx during the transition so the demux thread
	 * either sees setup_mode==true (mailbox path) or a fully
	 * initialized channel.  Set setup_mode=false last.
	 */
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	ch->window_max = INITIAL_WINDOW_SIZE;

	if (is_subsystem) {
		ch->chan_type = DSSH_CHAN_RAW;
		msgqueue_init(&ch->buf.raw.queue);
	}
	else {
		ch->chan_type = DSSH_CHAN_SESSION;
		ch->stdout_consumed = 0;
		ch->stderr_consumed = 0;
		ch->exit_code = 0;
		ch->exit_code_received = false;
		res = bytebuf_init(&ch->buf.session.stdout_buf,
		        INITIAL_WINDOW_SIZE);
		if (res < 0) {
			dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
			goto fail;
		}
		res = bytebuf_init(&ch->buf.session.stderr_buf,
		        INITIAL_WINDOW_SIZE);
		if (res < 0) {
			bytebuf_free(&ch->buf.session.stdout_buf);
			dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
			goto fail;
		}
		sigqueue_init(&ch->buf.session.signals);
	}

	/* Install window-change callback for post-setup delivery */
	if (cbs->window_change != NULL) {
		ch->window_change_cb = cbs->window_change;
		ch->window_change_cbdata = cbs->cbdata;
	}

	ch->setup_mode = false;
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	if (request_type != NULL)
		*request_type = ch->req_type;
	if (request_data != NULL)
		*request_data = ch->req_data;
	return ch;

fail:
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	ch->setup_mode = false;
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	unregister_channel(sess, ch);
	free(ch->setup_payload);
	cnd_destroy(&ch->poll_cnd);
	mtx_destroy(&ch->buf_mtx);
	free(ch);
	return NULL;
}

/* ================================================================
 * Poll / Read / Write -- Session channels (stream-based)
 * ================================================================ */

/*
 * Compute how many bytes are readable on stdout (ext=false) or
 * stderr (ext=true) before hitting a signal mark.
 */
static size_t
session_readable(struct dssh_channel_s *ch, bool ext)
{
	struct dssh_bytebuf *bb = ext
	    ? &ch->buf.session.stderr_buf
	    : &ch->buf.session.stdout_buf;
	size_t avail = bytebuf_available(bb);

	if (avail == 0)
		return 0;

	struct dssh_signal_queue *sq = &ch->buf.session.signals;

	if (sq->head != NULL) {
		size_t mark = ext
		    ? sq->head->stderr_pos
		    : sq->head->stdout_pos;
		size_t consumed = ext
		    ? ch->stderr_consumed
		    : ch->stdout_consumed;

		if (mark > consumed) {
			size_t to_mark = mark - consumed;

			if (to_mark < avail)
				avail = to_mark;
		}
	}
	return avail;
}

DSSH_PUBLIC int
dssh_session_poll(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, int events, int timeout_ms)
{
	struct timespec ts;

	if (ch == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	if (ch->chan_type != DSSH_CHAN_SESSION)
		return DSSH_ERROR_INVALID;
	if (timeout_ms > 0)
		deadline_from_ms(&ts, timeout_ms);
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));

	for (;;) {
		int ready = 0;

		if ((events & DSSH_POLL_READ)
		    && ((session_readable(ch, false) > 0) || ch->eof_received || ch->close_received))
			ready |= DSSH_POLL_READ;

		if ((events & DSSH_POLL_READEXT)
		    && ((session_readable(ch, true) > 0) || ch->eof_received || ch->close_received))
			ready |= DSSH_POLL_READEXT;

		if ((events & DSSH_POLL_WRITE) && (ch->remote_window > 0) && ch->open && !ch->close_received)
			ready |= DSSH_POLL_WRITE;

		if ((events & DSSH_POLL_SIGNAL)
		    && sigqueue_ready(&ch->buf.session.signals,
		    ch->stdout_consumed, ch->stderr_consumed))
			ready |= DSSH_POLL_SIGNAL;

		if (ready || sess->terminate) {
			dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
			return ready;
		}

		if (timeout_ms == 0) {
			dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
			return 0;
		}

		if (timeout_ms < 0) {
			dssh_thrd_check(sess, cnd_wait(&ch->poll_cnd, &ch->buf_mtx));
		}
		else {
			if (dssh_thrd_check(sess, cnd_timedwait(&ch->poll_cnd, &ch->buf_mtx, &ts)) == thrd_timedout) {
				dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
				return 0;
			}
		}
	}
}

static int64_t
session_read_impl(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, uint8_t *buf, size_t bufsz, bool ext)
{
	if (buf == NULL || ch == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	if (ch->chan_type != DSSH_CHAN_SESSION)
		return DSSH_ERROR_INVALID;
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));

	struct dssh_bytebuf *bb = ext
	    ? &ch->buf.session.stderr_buf
	    : &ch->buf.session.stdout_buf;
	size_t limit = session_readable(ch, ext);
	size_t n = bytebuf_read(bb, buf, bufsz, limit);

	if (ext)
		ch->stderr_consumed += n;
	else
		ch->stdout_consumed += n;
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	if (n > 0) {
		int wret = maybe_replenish_window(sess, ch);
		if (wret < 0)
			return wret;
	}
	return (int64_t)n;
}

DSSH_PUBLIC int64_t
dssh_session_read(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, uint8_t *buf, size_t bufsz)
{
	return session_read_impl(sess, ch, buf, bufsz, false);
}

DSSH_PUBLIC int64_t
dssh_session_read_ext(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, uint8_t *buf, size_t bufsz)
{
	return session_read_impl(sess, ch, buf, bufsz, true);
}

DSSH_PUBLIC int64_t
dssh_session_write(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, const uint8_t *buf, size_t bufsz)
{
	if (buf == NULL || ch == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	if (ch->chan_type != DSSH_CHAN_SESSION)
		return DSSH_ERROR_INVALID;
	if (bufsz == 0)
		return 0;

	size_t sent = 0;
	int res = send_data(sess, ch, buf, bufsz, &sent);

	if (res < 0)
		return res;
	int64_t sent_i64 = (int64_t)sent;
	return sent_i64;
}

DSSH_PUBLIC int64_t
dssh_session_write_ext(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, const uint8_t *buf, size_t bufsz)
{
	if (buf == NULL || ch == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	if (ch->chan_type != DSSH_CHAN_SESSION)
		return DSSH_ERROR_INVALID;
	if (bufsz == 0)
		return 0;

	size_t sent = 0;
	int res = send_extended_data(sess, ch,
	        SSH_EXTENDED_DATA_STDERR, buf, bufsz, &sent);

	if (res < 0)
		return res;
	int64_t sent_i64 = (int64_t)sent;
	return sent_i64;
}

DSSH_PUBLIC int
dssh_session_read_signal(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, const char **signal_name)
{
	if (signal_name == NULL || ch == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	if (ch->chan_type != DSSH_CHAN_SESSION)
		return DSSH_ERROR_INVALID;
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));

	const char *name = sigqueue_pop(&ch->buf.session.signals,
	        ch->stdout_consumed, ch->stderr_consumed,
	        ch->last_signal, sizeof(ch->last_signal));

	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	if (name == NULL)
		return DSSH_ERROR_NOMORE;
	*signal_name = ch->last_signal;
	return 0;
}

DSSH_PUBLIC int
dssh_session_close(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, uint32_t exit_code)
{
	if (ch == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	if (ch->chan_type != DSSH_CHAN_SESSION)
		return DSSH_ERROR_INVALID;
	dssh_conn_send_exit_status(sess, ch, exit_code);
	send_eof(sess, ch);
	conn_close(sess, ch);

	/* Synchronize with the demux thread before freeing.
	 * Set closing first so the demux bails out of any
	 * unlock-relock window (window-change cb, send_packet).
	 * Then unregister to prevent new find_channel() hits.
	 * Finally acquire buf_mtx to ensure the demux has
	 * exited its critical section. */
	atomic_store(&ch->closing, true);
	unregister_channel(sess, ch);
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	cleanup_channel_buffers(ch);
	free(ch);
	return 0;
}

/* ================================================================
 * Poll / Read / Write -- Raw channels (message-based)
 * ================================================================ */

DSSH_PUBLIC int
dssh_channel_poll(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, int events, int timeout_ms)
{
	struct timespec ts;

	if (ch == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	if (ch->chan_type != DSSH_CHAN_RAW)
		return DSSH_ERROR_INVALID;
	if (timeout_ms > 0)
		deadline_from_ms(&ts, timeout_ms);
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));

	for (;;) {
		int ready = 0;

		if ((events & DSSH_POLL_READ)
		    && ((ch->buf.raw.queue.count > 0) || ch->eof_received || ch->close_received))
			ready |= DSSH_POLL_READ;

		if ((events & DSSH_POLL_WRITE) && (ch->remote_window > 0) && ch->open && !ch->close_received)
			ready |= DSSH_POLL_WRITE;

		if (ready || sess->terminate) {
			dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
			return ready;
		}

		if (timeout_ms == 0) {
			dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
			return 0;
		}

		if (timeout_ms < 0) {
			dssh_thrd_check(sess, cnd_wait(&ch->poll_cnd, &ch->buf_mtx));
		}
		else {
			if (dssh_thrd_check(sess, cnd_timedwait(&ch->poll_cnd, &ch->buf_mtx, &ts)) == thrd_timedout) {
				dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
				return 0;
			}
		}
	}
}

DSSH_PUBLIC int64_t
dssh_channel_read(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, uint8_t *buf, size_t bufsz)
{
	if (ch == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	if (buf == NULL && bufsz != 0)
		return DSSH_ERROR_INVALID;
	if (ch->chan_type != DSSH_CHAN_RAW)
		return DSSH_ERROR_INVALID;
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));

	int64_t n = msgqueue_pop(&ch->buf.raw.queue, buf, bufsz);

	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	if (n > 0 && buf != NULL) {
		int wret = maybe_replenish_window(sess, ch);
		if (wret < 0)
			return wret;
	}
	return n;
}

DSSH_PUBLIC int
dssh_channel_write(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, const uint8_t *buf, size_t len)
{
	if (buf == NULL || ch == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	if (ch->chan_type != DSSH_CHAN_RAW)
		return DSSH_ERROR_INVALID;
	if (len == 0)
		return 0;

	return send_data(sess, ch, buf, len, NULL);
}

DSSH_PUBLIC int
dssh_channel_close(struct dssh_session_s *sess,
    struct dssh_channel_s *ch)
{
	if (ch == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	if (ch->chan_type != DSSH_CHAN_RAW)
		return DSSH_ERROR_INVALID;
	send_eof(sess, ch);
	conn_close(sess, ch);

	/* Same demux synchronization as dssh_session_close(). */
	atomic_store(&ch->closing, true);
	unregister_channel(sess, ch);
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	cleanup_channel_buffers(ch);
	free(ch);
	return 0;
}

/* ================================================================
 * Window change (client-side)
 * ================================================================ */

DSSH_PUBLIC int
dssh_session_send_signal(struct dssh_session_s *sess,
    struct dssh_channel_s *ch, const char *signal_name)
{
	if (signal_name == NULL || ch == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	size_t   slen = strlen(signal_name);

	size_t   rtlen = DSSH_STRLEN(str_signal);
	size_t   fixed = CHAN_REQUEST_FIXED + rtlen + 4;

	if (slen > UINT32_MAX || slen > SIZE_MAX - fixed)
		return DSSH_ERROR_INVALID;

	size_t   msg_len = fixed + slen;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;
	int    ret;

	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);
	DSSH_PUT_U32((uint32_t)rtlen, msg, &pos);
	memcpy(&msg[pos], "signal", rtlen);
	pos += rtlen;
	msg[pos++] = 0; /* want_reply = FALSE */
	DSSH_PUT_U32((uint32_t)slen, msg, &pos);
	memcpy(&msg[pos], signal_name, slen);
	pos += slen;

	ret = send_packet(sess, msg, pos, NULL);

	free(msg);
	return ret;
}

DSSH_PUBLIC int
dssh_session_send_window_change(struct dssh_session_s *sess,
    struct dssh_channel_s *ch,
    uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx)
{
	if (ch == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	uint8_t extra[WINDOW_CHANGE_FIXED];
	size_t  ep = 0;
	int     ret;

	DSSH_PUT_U32(cols, extra, &ep);
	DSSH_PUT_U32(rows, extra, &ep);
	DSSH_PUT_U32(wpx, extra, &ep);
	DSSH_PUT_U32(hpx, extra, &ep);

        /* window-change uses want_reply=false */
	size_t   rtlen = DSSH_STRLEN(str_window_change);
	size_t   msg_len = 1 + 4 + 4 + rtlen + 1 + ep;
	uint8_t *msg = malloc(msg_len);

	if (msg == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);
	DSSH_PUT_U32((uint32_t)rtlen, msg, &pos);
	memcpy(&msg[pos], "window-change", rtlen);
	pos += rtlen;
	msg[pos++] = 0; /* want_reply = FALSE */
	memcpy(&msg[pos], extra, ep);
	pos += ep;

	ret = send_packet(sess, msg, pos, NULL);

	free(msg);
	return ret;
}
