// RFC 4254: SSH Connection Protocol

#include <stdlib.h>
#include <string.h>

#include "deucessh-conn.h"
#include "ssh-internal.h"

#ifdef DSSH_TESTING
 #include "dssh_test_alloc.h"
 #include "dssh_test_ossl.h"
#endif

/* Thread-local guard: true when inside a ZC RX callback on the demux
 * thread.  TX functions check this and return DSSH_ERROR_INVALID to
 * prevent deadlock (demux thread calling TX while holding no mutex,
 * but TX needs tx_mtx which may wait on rekey which needs demux). */
static _Thread_local bool in_zc_rx;

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
	b->head     = 0;
	b->tail     = 0;
	b->used     = 0;
	b->total    = 0;
	return 0;
}

DSSH_TESTABLE void
bytebuf_free(struct dssh_bytebuf *b)
{
	free(b->data);
	b->data     = NULL;
	b->capacity = 0;
	b->head     = 0;
	b->tail     = 0;
	b->used     = 0;
}

DSSH_TESTABLE size_t
bytebuf_write(struct dssh_bytebuf *b, const uint8_t *data, size_t len)
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
bytebuf_read(struct dssh_bytebuf *b, uint8_t *buf, size_t bufsz, size_t limit)
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

/* ---- Accept queue ---- */

DSSH_TESTABLE void
acceptqueue_init(struct dssh_accept_queue *q)
{
	q->head  = 0;
	q->count = 0;
}

DSSH_TESTABLE void
acceptqueue_free(struct dssh_accept_queue *q)
{
	q->head  = 0;
	q->count = 0;
}

/*
 * Push to the accept queue ring buffer.  Caller MUST hold accept_mtx.
 * If the queue is full, the caller should stall on accept_cnd before
 * calling this function (see demux_channel_open).
 */
DSSH_TESTABLE int
acceptqueue_push(struct dssh_accept_queue *q, uint32_t peer_channel, uint32_t peer_window,
    uint32_t peer_max_packet, const uint8_t *type, size_t type_len)
{
	if (q->count >= DSSH_ACCEPT_QUEUE_CAP)
		return DSSH_ERROR_TOOMANY;

	size_t                     idx = (q->head + q->count) % DSSH_ACCEPT_QUEUE_CAP;
	struct dssh_incoming_open *e   = &q->entries[idx];

	e->peer_channel    = peer_channel;
	e->peer_window     = peer_window;
	e->peer_max_packet = peer_max_packet;
	if (type_len >= sizeof(e->channel_type))
		type_len = sizeof(e->channel_type) - 1;
	memcpy(e->channel_type, type, type_len);
	e->channel_type[type_len] = 0;
	e->channel_type_len       = type_len;
	q->count++;
	return 0;
}

/*
 * Pop from the accept queue ring buffer.  Caller MUST hold accept_mtx.
 * Copies the entry into *out and returns 0, or returns -1 if empty.
 */
DSSH_TESTABLE int
acceptqueue_pop(struct dssh_accept_queue *q, struct dssh_incoming_open *out)
{
	if (q->count == 0)
		return -1;
	*out    = q->entries[q->head];
	q->head = (q->head + 1) % DSSH_ACCEPT_QUEUE_CAP;
	q->count--;
	return 0;
}

/* ---- Event queue (dssh_chan_* API) ---- */

#define EVENT_QUEUE_INITIAL_CAPACITY 8

DSSH_TESTABLE int
event_queue_init(struct dssh_event_queue *q, size_t max_events)
{
	q->entries = calloc(EVENT_QUEUE_INITIAL_CAPACITY, sizeof(struct dssh_event_entry));
	if (q->entries == NULL)
		return DSSH_ERROR_ALLOC;
	q->capacity   = EVENT_QUEUE_INITIAL_CAPACITY;
	q->max_events = max_events;
	q->head       = 0;
	q->tail       = 0;
	q->count      = 0;
	q->has_frozen = false;
	return 0;
}

DSSH_TESTABLE void
event_queue_free(struct dssh_event_queue *q)
{
	free(q->entries);
	q->entries    = NULL;
	q->capacity   = 0;
	q->count      = 0;
	q->has_frozen = false;
}

DSSH_TESTABLE int
event_queue_push(struct dssh_event_queue *q, const struct dssh_event_entry *e)
{
	if (q->max_events != 0 && q->count >= q->max_events)
		return DSSH_ERROR_TOOMANY;
	if (q->count >= q->capacity) {
		size_t                   new_cap = q->capacity * 2;
		struct dssh_event_entry *newbuf  = calloc(new_cap, sizeof(struct dssh_event_entry));

		if (newbuf == NULL)
			return DSSH_ERROR_ALLOC;
		for (size_t i = 0; i < q->count; i++) {
			size_t idx = (q->head + i) % q->capacity;

			newbuf[i] = q->entries[idx];
		}
		free(q->entries);
		q->entries  = newbuf;
		q->head     = 0;
		q->tail     = q->count;
		q->capacity = new_cap;
	}
	q->entries[q->tail] = *e;
	q->tail             = (q->tail + 1) % q->capacity;
	q->count++;
	return 0;
}

static bool
event_queue_freeze(struct dssh_event_queue *q)
{
	q->has_frozen = false;
	if (q->count == 0)
		return false;
	q->frozen = q->entries[q->head];
	q->head   = (q->head + 1) % q->capacity;
	q->count--;
	q->has_frozen = true;
	return true;
}

/* ---- Channel params builder ---- */

DSSH_PUBLIC int
dssh_chan_params_init(struct dssh_chan_params *p, enum dssh_chan_type type)
{
	if (p == NULL)
		return DSSH_ERROR_INVALID;
	memset(p, 0, sizeof(*p));
	p->type = type;
	p->term = strdup("dumb");
	if (p->term == NULL)
		return DSSH_ERROR_ALLOC;
	if (type == DSSH_CHAN_SHELL)
		p->flags |= DSSH_PARAM_HAS_PTY;
	return 0;
}

DSSH_PUBLIC void
dssh_chan_params_free(struct dssh_chan_params *p)
{
	if (p == NULL)
		return;
	free(p->term);
	p->term = NULL;
	free(p->command);
	p->command = NULL;
	free(p->subsystem);
	p->subsystem = NULL;
	for (size_t i = 0; i < p->env_count; i++) {
		free(p->env[i].name);
		free(p->env[i].value);
	}
	free(p->env);
	p->env          = NULL;
	p->env_count    = 0;
	p->env_capacity = 0;
	free(p->modes);
	p->modes         = NULL;
	p->mode_count    = 0;
	p->mode_capacity = 0;
}

DSSH_PUBLIC int
dssh_chan_params_set_max_window(struct dssh_chan_params *p, uint32_t max_window)
{
	if (p == NULL)
		return DSSH_ERROR_INVALID;
	p->max_window = max_window;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_params_set_pty(struct dssh_chan_params *p, bool enable)
{
	if (p == NULL)
		return DSSH_ERROR_INVALID;
	if (enable)
		p->flags |= DSSH_PARAM_HAS_PTY;
	else
		p->flags &= ~(uint32_t)DSSH_PARAM_HAS_PTY;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_params_set_term(struct dssh_chan_params *p, const char *term)
{
	if (p == NULL || term == NULL)
		return DSSH_ERROR_INVALID;
	free(p->term);
	p->term = strdup(term);
	if (p->term == NULL)
		return DSSH_ERROR_ALLOC;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_params_set_size(struct dssh_chan_params *p, uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx)
{
	if (p == NULL)
		return DSSH_ERROR_INVALID;
	p->cols = cols;
	p->rows = rows;
	p->wpx  = wpx;
	p->hpx  = hpx;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_params_set_mode(struct dssh_chan_params *p, uint8_t opcode, uint32_t value)
{
	if (p == NULL)
		return DSSH_ERROR_INVALID;
	for (size_t i = 0; i < p->mode_count; i++) {
		if (p->modes[i].opcode == opcode) {
			p->modes[i].value = value;
			return 0;
		}
	}
	if (p->mode_count >= p->mode_capacity) {
		size_t                       new_cap = p->mode_capacity == 0 ? 8 : p->mode_capacity * 2;
		struct dssh_chan_mode_entry *nm      = realloc(p->modes, new_cap * sizeof(*nm));

		if (nm == NULL)
			return DSSH_ERROR_ALLOC;
		p->modes         = nm;
		p->mode_capacity = new_cap;
	}
	p->modes[p->mode_count].opcode = opcode;
	p->modes[p->mode_count].value  = value;
	p->mode_count++;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_params_set_command(struct dssh_chan_params *p, const char *command)
{
	if (p == NULL || command == NULL)
		return DSSH_ERROR_INVALID;
	free(p->command);
	p->command = strdup(command);
	if (p->command == NULL)
		return DSSH_ERROR_ALLOC;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_params_set_subsystem(struct dssh_chan_params *p, const char *name)
{
	if (p == NULL || name == NULL)
		return DSSH_ERROR_INVALID;
	free(p->subsystem);
	p->subsystem = strdup(name);
	if (p->subsystem == NULL)
		return DSSH_ERROR_ALLOC;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_params_add_env(struct dssh_chan_params *p, const char *name, const char *value)
{
	if (p == NULL || name == NULL || value == NULL)
		return DSSH_ERROR_INVALID;
	if (p->env_count >= p->env_capacity) {
		size_t                new_cap = p->env_capacity == 0 ? 4 : p->env_capacity * 2;
		struct dssh_chan_env *ne      = realloc(p->env, new_cap * sizeof(*ne));

		if (ne == NULL)
			return DSSH_ERROR_ALLOC;
		p->env          = ne;
		p->env_capacity = new_cap;
	}
	p->env[p->env_count].name = strdup(name);
	if (p->env[p->env_count].name == NULL)
		return DSSH_ERROR_ALLOC;
	p->env[p->env_count].value = strdup(value);
	if (p->env[p->env_count].value == NULL) {
		free(p->env[p->env_count].name);
		return DSSH_ERROR_ALLOC;
	}
	p->env_count++;
	return 0;
}

/* ================================================================ */

#define INITIAL_WINDOW_SIZE 0x200000
#define MAX_PACKET_SIZE     0x8000

/* Fixed overhead for channel messages (excludes variable-length fields). */

/* CHANNEL_DATA: msg_type + channel + data_len */
#define CHAN_DATA_FIXED (1 + 4 + 4)

/* CHANNEL_EXTENDED_DATA: msg_type + channel + data_type + data_len */
#define CHAN_EXT_DATA_FIXED (1 + 4 + 4 + 4)

/* CHANNEL_REQUEST: msg_type + channel + rtype_len + want_reply */
#define CHAN_REQUEST_FIXED (1 + 4 + 4 + 1)

/* pty-req extra data: term_len + cols + rows + wpx + hpx + modes_len */
#define PTY_REQ_FIXED (4 + 4 + 4 + 4 + 4 + 4)

/* window-change extra data: cols + rows + wpx + hpx */
#define WINDOW_CHANGE_FIXED (4 + 4 + 4 + 4)

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
static const char str_pty_req[]         = "pty-req";
static const char str_env[]             = "env";
static const char str_exit_signal[]     = "exit-signal";
static const char str_break[]           = "break";

/* ================================================================
 * Timeout helper -- thin wrapper around dssh_deadline_from_ms()
 * ================================================================ */

static void
deadline_from_ms(struct timespec *ts, int timeout_ms)
{
	dssh_deadline_from_ms(ts, timeout_ms);
}

/* ================================================================
 * Parse helpers for channel request payloads (internal)
 * ================================================================ */

/* Internal PTY request struct -- pointers into wire data */
struct pty_req {
	const char    *term;
	const uint8_t *modes;
	size_t         modes_len;
	uint32_t       cols, rows;
	uint32_t       wpx, hpx;
};

static int
parse_pty_req_data(const uint8_t *data, size_t data_len, struct pty_req *pty)
{
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
	pty->wpx  = DSSH_GET_U32(&data[pos + 8]);
	pty->hpx  = DSSH_GET_U32(&data[pos + 12]);
	pos += 16;
	if (pos + 4 <= data_len) {
		uint32_t mlen = DSSH_GET_U32(&data[pos]);

		pos += 4;
		if (pos + mlen <= data_len) {
			pty->modes     = &data[pos];
			pty->modes_len = mlen;
		}
		else {
			pty->modes     = NULL;
			pty->modes_len = 0;
		}
	}
	else {
		pty->modes     = NULL;
		pty->modes_len = 0;
	}
	return 0;
}

static int
parse_env_data(const uint8_t *data, size_t data_len, const uint8_t **name, size_t *name_len, const uint8_t **value,
    size_t *value_len)
{
	size_t   pos = 0;
	uint32_t nlen;

	if (pos + 4 > data_len)
		return DSSH_ERROR_PARSE;
	nlen = DSSH_GET_U32(&data[pos]);
	pos += 4;
	if (pos + nlen > data_len)
		return DSSH_ERROR_PARSE;
	*name     = &data[pos];
	*name_len = nlen;
	pos += nlen;

	uint32_t vlen;

	if (pos + 4 > data_len)
		return DSSH_ERROR_PARSE;
	vlen = DSSH_GET_U32(&data[pos]);
	pos += 4;
	if (pos + vlen > data_len)
		return DSSH_ERROR_PARSE;
	*value     = &data[pos];
	*value_len = vlen;
	return 0;
}

/* Channel open failure reason codes (RFC 4254 s5.1) */
#define SSH_OPEN_ADMINISTRATIVELY_PROHIBITED UINT32_C(1)
#define SSH_OPEN_CONNECT_FAILED              UINT32_C(2)
#define SSH_OPEN_UNKNOWN_CHANNEL_TYPE        UINT32_C(3)
#define SSH_OPEN_RESOURCE_SHORTAGE           UINT32_C(4)

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

/*
 * Atomic saturating add for remote_window.
 * Only the demux thread increments (WINDOW_ADJUST); only the app
 * thread decrements (zc_send).  CAS loop handles the rare concurrent
 * case where both happen simultaneously.
 */
static inline void
window_atomic_add(atomic_uint_least32_t *win, uint32_t bytes)
{
	uint32_t old = atomic_load_explicit(win, memory_order_relaxed);

	for (;;) {
		uint32_t nv = window_add(old, bytes);

		if (atomic_compare_exchange_weak_explicit(win, &old, nv, memory_order_release,
			memory_order_relaxed))
			break;
	}
}

/*
 * Atomic saturating subtract for remote_window.
 * Deducts up to `bytes` from the window, clamping at 0.
 */
static inline void
window_atomic_sub(atomic_uint_least32_t *win, uint32_t bytes)
{
	uint32_t old = atomic_load_explicit(win, memory_order_relaxed);

	for (;;) {
		uint32_t nv = (bytes <= old) ? old - bytes : 0;

		if (atomic_compare_exchange_weak_explicit(win, &old, nv, memory_order_release,
			memory_order_relaxed))
			break;
	}
}

/* ================================================================
 * Low-level wire functions -- internal only.
 * Used by the high-level API; not part of the public interface.
 * ================================================================ */

#define SSH_EXTENDED_DATA_STDERR UINT32_C(1)

DSSH_TESTABLE int
send_window_adjust(struct dssh_session_s *sess, struct dssh_channel_s *ch, uint32_t bytes)
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
dssh_conn_send_exit_status(struct dssh_session_s *sess, struct dssh_channel_s *ch, uint32_t exit_code)
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
send_eof(struct dssh_session_s *sess, struct dssh_channel_s *ch)
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
conn_close(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	if (ch->close_sent) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return 0;
	}
	ch->close_sent = true;
	ch->open       = false;
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

		if (sess->channel_capacity > SIZE_MAX / 2 || newcap > SIZE_MAX / sizeof(struct dssh_channel_s *)) {
			dssh_thrd_check(sess, mtx_unlock(&sess->channel_mtx));
			return DSSH_ERROR_ALLOC;
		}
		struct dssh_channel_s **newt = realloc(sess->channels, newcap * sizeof(*newt));

		if (newt == NULL) {
			dssh_thrd_check(sess, mtx_unlock(&sess->channel_mtx));
			return DSSH_ERROR_ALLOC;
		}
		sess->channels         = newt;
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
		bool     in_use    = false;

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
/*
 * Phase 1: init sync primitives only.  Called before CHANNEL_OPEN
 * so the demux thread can signal the channel.  No buffer allocation.
 */
init_channel_sync(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	ch->chan_type          = DSSH_CHAN_SESSION;
	ch->exit_code          = 0;
	ch->exit_code_received = false;
	atomic_store(&ch->closing, false);

	/* TX slots start empty; allocated below */
	memset(&ch->wa_slot, 0, sizeof(ch->wa_slot));
	memset(&ch->chan_fail_slot, 0, sizeof(ch->chan_fail_slot));

	if (mtx_init(&ch->buf_mtx, mtx_plain) != thrd_success)
		return DSSH_ERROR_INIT;
	if (mtx_init(&ch->cb_mtx, mtx_plain) != thrd_success) {
		mtx_destroy(&ch->buf_mtx);
		return DSSH_ERROR_INIT;
	}
	if (cnd_init(&ch->poll_cnd) != thrd_success) {
		mtx_destroy(&ch->cb_mtx);
		mtx_destroy(&ch->buf_mtx);
		return DSSH_ERROR_INIT;
	}

	int ret = alloc_channel_slots(sess, ch);

	if (ret < 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->cb_mtx);
		mtx_destroy(&ch->buf_mtx);
		return ret;
	}
	return 0;
}

/*
 * Phase 2: allocate ring buffers after the terminal request succeeds.
 * Called with sync primitives already initialized.
 */
static int
init_channel_buffers(struct dssh_channel_s *ch, uint32_t window_max)
{
	ch->window_max = window_max;

	int res = bytebuf_init(&ch->buf.stdout_buf, window_max);

	if (res < 0)
		return res;
	res = bytebuf_init(&ch->buf.stderr_buf, window_max);
	if (res < 0) {
		bytebuf_free(&ch->buf.stdout_buf);
		return res;
	}
	return 0;
}

static void
cleanup_channel_buffers(struct dssh_channel_s *ch)
{
	if (ch->chan_type == DSSH_CHAN_SESSION) {
		bytebuf_free(&ch->buf.stdout_buf);
		bytebuf_free(&ch->buf.stderr_buf);
	}
	free(ch->setup_payload);
	ch->setup_payload = NULL;
	free_tx_slot(&ch->wa_slot);
	free_tx_slot(&ch->chan_fail_slot);

	if (ch->chan_type != 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->cb_mtx);
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
			size_t buf_free = bytebuf_free_space(&ch->buf.stdout_buf);
			size_t err_free = bytebuf_free_space(&ch->buf.stderr_buf);
			if (err_free < buf_free)
				buf_free = err_free;
			uint32_t buf_cap = buf_free > UINT32_MAX ? UINT32_MAX : (uint32_t)buf_free;
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
parse_channel_request(const uint8_t *payload, size_t payload_len, const uint8_t **rtype, uint32_t *rtype_len,
    bool *want_reply, const uint8_t **req_data, size_t *req_data_len)
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

	*req_data     = &payload[rpos];
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
handle_channel_request(struct dssh_session_s *sess, struct dssh_channel_s *ch, const uint8_t *payload,
    size_t payload_len)
{
	const uint8_t *rtype;
	uint32_t       rtype_len;
	bool           want_reply;
	const uint8_t *rdata;
	size_t         rdata_len;

	if (parse_channel_request(payload, payload_len, &rtype, &rtype_len, &want_reply, &rdata, &rdata_len) < 0) {
		uint8_t fail[8];
		size_t  fp = 0;

		fail[fp++] = SSH_MSG_CHANNEL_FAILURE;
		DSSH_PUT_U32(ch->remote_id, fail, &fp);
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		send_to_slot(sess, &ch->chan_fail_slot, fail, fp);
		dssh_transport_disconnect(sess, SSH_DISCONNECT_PROTOCOL_ERROR, "malformed CHANNEL_REQUEST");
		return 1;
	}

	int push_ret = 0;

	if (ch->chan_type == DSSH_CHAN_SESSION) {
		if ((rtype_len == DSSH_STRLEN(str_signal))
		    && (memcmp(rtype, str_signal, DSSH_STRLEN(str_signal)) == 0)) {
			if (rdata_len >= 4) {
				uint32_t sname_len = DSSH_GET_U32(rdata);

				if (4 + sname_len <= rdata_len) {
					struct dssh_event_entry ev = {.type = DSSH_EVENT_SIGNAL,
					    .stdout_pos                     = ch->buf.stdout_buf.total,
					    .stderr_pos                     = ch->buf.stderr_buf.total};
					size_t                  sn =
                                            sname_len < sizeof(ev.str_a) - 1 ? sname_len : sizeof(ev.str_a) - 1;

					memcpy(ev.str_a, &rdata[4], sn);
					ev.str_a[sn] = '\0';
					push_ret     = event_queue_push(&ch->events, &ev);
				}
			}
		}
		else if ((rtype_len == DSSH_STRLEN(str_exit_status))
			 && (memcmp(rtype, str_exit_status, DSSH_STRLEN(str_exit_status)) == 0)) {
			if (rdata_len >= 4) {
				ch->exit_code              = DSSH_GET_U32(rdata);
				ch->exit_code_received     = true;
				struct dssh_event_entry ev = {.type = DSSH_EVENT_EXIT_STATUS,
				    .stdout_pos                     = ch->buf.stdout_buf.total,
				    .stderr_pos                     = ch->buf.stderr_buf.total,
				    .u32_a                          = ch->exit_code};

				push_ret = event_queue_push(&ch->events, &ev);
			}
		}
		else if ((rtype_len == DSSH_STRLEN(str_window_change))
			 && (memcmp(rtype, str_window_change, DSSH_STRLEN(str_window_change)) == 0)) {
			if (rdata_len >= 16) {
				uint32_t wc_cols = DSSH_GET_U32(rdata);
				uint32_t wc_rows = DSSH_GET_U32(&rdata[4]);
				uint32_t wc_wpx  = DSSH_GET_U32(&rdata[8]);
				uint32_t wc_hpx  = DSSH_GET_U32(&rdata[12]);

				struct dssh_event_entry ev = {.type = DSSH_EVENT_WINDOW_CHANGE,
				    .stdout_pos                     = ch->buf.stdout_buf.total,
				    .stderr_pos                     = ch->buf.stderr_buf.total,
				    .u32_a                          = wc_cols,
				    .u32_b                          = wc_rows,
				    .u32_c                          = wc_wpx,
				    .u32_d                          = wc_hpx};

				push_ret = event_queue_push(&ch->events, &ev);
			}
		}
		else if ((rtype_len == DSSH_STRLEN(str_exit_signal))
			 && (memcmp(rtype, str_exit_signal, DSSH_STRLEN(str_exit_signal)) == 0)) {
			struct dssh_event_entry ev = {.type = DSSH_EVENT_EXIT_SIGNAL,
			    .stdout_pos                     = ch->buf.stdout_buf.total,
			    .stderr_pos                     = ch->buf.stderr_buf.total};

			if (rdata_len >= 4) {
				uint32_t snlen = DSSH_GET_U32(rdata);
				size_t   off   = 4;

				if (off + snlen <= rdata_len) {
					size_t cl = snlen < sizeof(ev.str_a) - 1 ? snlen : sizeof(ev.str_a) - 1;

					memcpy(ev.str_a, &rdata[off], cl);
					ev.str_a[cl] = '\0';
					off += snlen;
				}
				if (off + 1 <= rdata_len) {
					ev.flag_a = rdata[off] != 0;
					off++;
				}
				if (off + 4 <= rdata_len) {
					uint32_t emlen = DSSH_GET_U32(&rdata[off]);

					off += 4;
					if (off + emlen <= rdata_len) {
						size_t cl =
						    emlen < sizeof(ev.str_b) - 1 ? emlen : sizeof(ev.str_b) - 1;

						memcpy(ev.str_b, &rdata[off], cl);
						ev.str_b[cl] = '\0';
					}
				}
			}
			push_ret = event_queue_push(&ch->events, &ev);
		}
		else if ((rtype_len == DSSH_STRLEN(str_break))
			 && (memcmp(rtype, str_break, DSSH_STRLEN(str_break)) == 0)) {
			struct dssh_event_entry ev = {.type = DSSH_EVENT_BREAK,
			    .stdout_pos                     = ch->buf.stdout_buf.total,
			    .stderr_pos                     = ch->buf.stderr_buf.total};

			if (rdata_len >= 4)
				ev.u32_a = DSSH_GET_U32(rdata);
			push_ret = event_queue_push(&ch->events, &ev);
		}
	}

	/* Event queue overflow — close the channel */
	if (push_ret == DSSH_ERROR_TOOMANY) {
		ch->open = false;
		dssh_thrd_check(sess, cnd_broadcast(&ch->poll_cnd));
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		conn_close(sess, ch);
		return 1;
	}

	/* Send FAILURE for unhandled requests with want_reply */
	if (want_reply) {
		uint8_t fail[8];
		size_t  fp = 0;

		fail[fp++] = SSH_MSG_CHANNEL_FAILURE;
		DSSH_PUT_U32(ch->remote_id, fail, &fp);
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		send_to_slot(sess, &ch->chan_fail_slot, fail, fp);
		dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
		if (atomic_load(&ch->closing)) {
			dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
			return 1;
		}
	}
	return 0;
}

/*
 * Handle CHANNEL_DATA -- releases buf_mtx, calls the channel's zc_cb
 * with a direct pointer into rx_packet, then re-acquires buf_mtx and
 * adjusts local_window by the consumed amount returned by the callback.
 *
 * Called under buf_mtx.  Returns 1 if caller should return immediately
 * (channel closing), 0 otherwise.
 */
static int
handle_channel_data(struct dssh_session_s *sess, struct dssh_channel_s *ch, const uint8_t *payload,
    size_t payload_len)
{
	if (payload_len < 9)
		return 0;
	if (ch->eof_received || ch->close_received)
		return 0;

	uint32_t dlen = DSSH_GET_U32(&payload[5]);

	if (9 + dlen > payload_len)
		return 0;

	const uint8_t *data = &payload[9];

	dssh_chan_zc_cb cb  = ch->zc_cb;
	void           *cbd = ch->zc_cbdata;

	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	in_zc_rx          = true;
	uint32_t consumed = cb(ch, 0, data, dlen, cbd);
	in_zc_rx          = false;
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	if (atomic_load(&ch->closing)) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return 1;
	}

	if (consumed <= ch->local_window)
		ch->local_window -= consumed;
	else
		ch->local_window = 0;

	/* ZC mode only: send WINDOW_ADJUST for consumed bytes.
	 * Stream mode uses maybe_replenish_window() after app reads. */
	if (ch->io_model == DSSH_IO_ZC && consumed > 0) {
		ch->local_window += consumed;

		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		send_to_wa_slot(sess, ch, consumed);
		dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
		if (atomic_load(&ch->closing)) {
			dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
			return 1;
		}
	}
	return 0;
}

/*
 * Handle CHANNEL_EXTENDED_DATA -- same pattern as handle_channel_data
 * but for stderr (data_type==1).
 * Called under buf_mtx.  Returns 1 if closing, 0 otherwise.
 */
static int
handle_channel_extended_data(struct dssh_session_s *sess, struct dssh_channel_s *ch, const uint8_t *payload,
    size_t payload_len)
{
	if (payload_len < 13)
		return 0;
	if (ch->eof_received || ch->close_received)
		return 0;

	uint32_t data_type = DSSH_GET_U32(&payload[5]);
	uint32_t dlen      = DSSH_GET_U32(&payload[9]);

	if (13 + dlen > payload_len)
		return 0;

	const uint8_t *data = &payload[13];

	dssh_chan_zc_cb cb  = ch->zc_cb;
	void           *cbd = ch->zc_cbdata;

	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	in_zc_rx          = true;
	uint32_t consumed = cb(ch, (int)data_type, data, dlen, cbd);
	in_zc_rx          = false;
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	if (atomic_load(&ch->closing)) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return 1;
	}

	if (consumed <= ch->local_window)
		ch->local_window -= consumed;
	else
		ch->local_window = 0;

	/* ZC mode only: send WINDOW_ADJUST for consumed bytes.
	 * Stream mode uses maybe_replenish_window() after app reads. */
	if (ch->io_model == DSSH_IO_ZC && consumed > 0) {
		ch->local_window += consumed;

		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		send_to_wa_slot(sess, ch, consumed);
		dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
		if (atomic_load(&ch->closing)) {
			dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
			return 1;
		}
	}
	return 0;
}

/*
 * Dispatch a received packet to the appropriate channel buffer.
 * Called from the demux thread with no locks held.
 */
DSSH_TESTABLE int
demux_dispatch(struct dssh_session_s *sess, uint8_t msg_type, uint8_t *payload, size_t payload_len)
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
			ch->setup_msg_type    = msg_type;
			ch->setup_ready       = true;
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
			if (handle_channel_data(sess, ch, payload, payload_len))
				return 0;
			break;

		case SSH_MSG_CHANNEL_EXTENDED_DATA:
			if (handle_channel_extended_data(sess, ch, payload, payload_len))
				return 0;
			break;

		case SSH_MSG_CHANNEL_WINDOW_ADJUST:
			if (payload_len >= 9) {
				uint32_t bytes = DSSH_GET_U32(&payload[5]);

				window_atomic_add(&ch->remote_window, bytes);
			}
			break;

		case SSH_MSG_CHANNEL_EOF:
			ch->eof_received = true;
			{
				struct dssh_event_entry ev  = {.type = DSSH_EVENT_EOF,
				     .stdout_pos                     = ch->buf.stdout_buf.total,
				     .stderr_pos                     = ch->buf.stderr_buf.total};
				int                     ret = event_queue_push(&ch->events, &ev);

				if (ret == DSSH_ERROR_TOOMANY) {
					ch->open = false;
					dssh_thrd_check(sess, cnd_broadcast(&ch->poll_cnd));
					dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
					conn_close(sess, ch);
					return 0;
				}
			}
			break;

		case SSH_MSG_CHANNEL_CLOSE:
			ch->close_received = true;
			{
				struct dssh_event_entry ev  = {.type = DSSH_EVENT_CLOSE,
				     .stdout_pos                     = ch->buf.stdout_buf.total,
				     .stderr_pos                     = ch->buf.stderr_buf.total};
				int                     ret = event_queue_push(&ch->events, &ev);

				if (ret == DSSH_ERROR_TOOMANY) {
					ch->open = false;
					dssh_thrd_check(sess, cnd_broadcast(&ch->poll_cnd));
					dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
					conn_close(sess, ch);
					return 0;
				}
			}

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
				ch->request_success   = (msg_type == SSH_MSG_CHANNEL_SUCCESS);
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
demux_open_confirmation(struct dssh_session_s *sess, uint8_t *payload, size_t payload_len)
{
	if (payload_len < 1 + 16)
		return DSSH_ERROR_PARSE;

	uint32_t local_id = DSSH_GET_U32(&payload[1]);

	struct dssh_channel_s *ch = find_channel(sess, local_id);

	if (ch == NULL)
		return 0; /* late message for closed channel */

	/* find_channel() returns with buf_mtx held */
	ch->remote_id = DSSH_GET_U32(&payload[5]);
	atomic_store_explicit(&ch->remote_window, DSSH_GET_U32(&payload[9]), memory_order_release);
	ch->remote_max_packet = DSSH_GET_U32(&payload[13]);
	ch->open              = true;
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

	if (rpos + 4 > payload_len) {
		dssh_transport_disconnect(sess, SSH_DISCONNECT_PROTOCOL_ERROR, "malformed CHANNEL_OPEN");
		return DSSH_ERROR_TERMINATED;
	}
	type_len = DSSH_GET_U32(&payload[rpos]);
	rpos += 4;
	if (rpos + type_len > payload_len) {
		dssh_transport_disconnect(sess, SSH_DISCONNECT_PROTOCOL_ERROR, "malformed CHANNEL_OPEN");
		return DSSH_ERROR_TERMINATED;
	}

	const uint8_t *ctype = &payload[rpos];

	rpos += type_len;

	if (rpos + 12 > payload_len) {
		if (rpos + 4 <= payload_len) {
			uint32_t pc = DSSH_GET_U32(&payload[rpos]);
			uint8_t  fail[64];
			size_t   fp = 0;

			fail[fp++] = SSH_MSG_CHANNEL_OPEN_FAILURE;
			DSSH_PUT_U32(pc, fail, &fp);
			DSSH_PUT_U32(SSH_OPEN_ADMINISTRATIVELY_PROHIBITED, fail, &fp);
			DSSH_PUT_U32(0, fail, &fp);
			DSSH_PUT_U32(0, fail, &fp);
			send_to_slot(sess, &sess->trans.open_fail_slot, fail, fp);
		}
		dssh_transport_disconnect(sess, SSH_DISCONNECT_PROTOCOL_ERROR, "malformed CHANNEL_OPEN");
		return DSSH_ERROR_TERMINATED;
	}

	uint32_t peer_channel = DSSH_GET_U32(&payload[rpos]);

	rpos += 4;
	uint32_t peer_window = DSSH_GET_U32(&payload[rpos]);

	rpos += 4;
	uint32_t peer_max_packet = DSSH_GET_U32(&payload[rpos]);

	/* Auto-reject forbidden channel types */
	bool reject = false;

	if ((type_len == DSSH_STRLEN(str_x11)) && (memcmp(ctype, str_x11, DSSH_STRLEN(str_x11)) == 0))
		reject = true;
	if ((type_len == DSSH_STRLEN(str_forwarded_tcpip))
	    && (memcmp(ctype, str_forwarded_tcpip, DSSH_STRLEN(str_forwarded_tcpip)) == 0))
		reject = true;
	if ((type_len == DSSH_STRLEN(str_direct_tcpip))
	    && (memcmp(ctype, str_direct_tcpip, DSSH_STRLEN(str_direct_tcpip)) == 0))
		reject = true;

	/* Client rejects "session" opens from server (RFC 4254 s6.1) */
	if (sess->trans.client && (type_len == DSSH_STRLEN(str_session))
	    && (memcmp(ctype, str_session, DSSH_STRLEN(str_session)) == 0))
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
		send_to_slot(sess, &sess->trans.open_fail_slot, fail, fp);
		return 0;
	}

	/* Queue for application's accept() call.
         * Stall if the queue is full — this applies RX backpressure
         * against a peer flooding CHANNEL_OPEN messages. */
	dssh_thrd_check(sess, mtx_lock(&sess->accept_mtx));
	while (sess->accept_queue.count >= DSSH_ACCEPT_QUEUE_CAP && !sess->terminate) {
		dssh_thrd_check(sess, cnd_wait(&sess->accept_cnd, &sess->accept_mtx));
	}
	if (sess->terminate) {
		dssh_thrd_check(sess, mtx_unlock(&sess->accept_mtx));
		return DSSH_ERROR_TERMINATED;
	}
	acceptqueue_push(&sess->accept_queue, peer_channel, peer_window, peer_max_packet, ctype, type_len);
	dssh_thrd_check(sess, cnd_signal(&sess->accept_cnd));
	dssh_thrd_check(sess, mtx_unlock(&sess->accept_mtx));
	return 0;
}

static int
demux_thread_func(void *arg)
{
	struct dssh_session_s *sess = arg;
	uint8_t                msg_type;
	uint8_t               *payload;
	size_t                 payload_len;

#ifdef DSSH_TESTING
	dssh_test_ossl_exclude_thread();
	if (dssh_test_alloc_new_threads_excluded())
		dssh_test_alloc_exclude_thread();
#endif

	while (!sess->terminate) {
		int res = recv_packet(sess, &msg_type, &payload, &payload_len);

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
		else if ((msg_type >= SSH_MSG_CHANNEL_OPEN_FAILURE) && (msg_type <= SSH_MSG_CHANNEL_FAILURE))
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

	sess->channels         = NULL;
	sess->channel_count    = 0;
	sess->channel_capacity = 0;
	sess->next_channel_id  = 0;
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
		sess->channels         = NULL;
		sess->channel_count    = 0;
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

static int
accept_incoming(struct dssh_session_s *sess, struct dssh_incoming_open *inc, int timeout_ms)
{
	struct timespec ts;

	if (inc == NULL || sess == NULL)
		return DSSH_ERROR_INVALID;
	if (timeout_ms > 0)
		deadline_from_ms(&ts, timeout_ms);
	dssh_thrd_check(sess, mtx_lock(&sess->accept_mtx));

	while (sess->accept_queue.count == 0) {
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
			if (dssh_thrd_check(sess, cnd_timedwait(&sess->accept_cnd, &sess->accept_mtx, &ts))
			    == thrd_timedout) {
				dssh_thrd_check(sess, mtx_unlock(&sess->accept_mtx));
				return DSSH_ERROR_NOMORE;
			}
		}
	}

	acceptqueue_pop(&sess->accept_queue, inc);
	/* Signal demux thread in case it's stalled on a full queue */
	dssh_thrd_check(sess, cnd_signal(&sess->accept_cnd));
	dssh_thrd_check(sess, mtx_unlock(&sess->accept_mtx));
	return 0;
}

/*
 * Send CHANNEL_OPEN_CONFIRMATION for an incoming channel.
 */
static int
send_open_confirmation(struct dssh_session_s *sess, struct dssh_channel_s *ch, uint32_t peer_channel)
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
	/* Caller has initialized sync primitives (init_channel_sync).
         * Send initial_window=0 per design -- no data flows until the
         * terminal request succeeds and we send WINDOW_ADJUST. */
	int64_t cid = alloc_channel_id(sess);
	if (cid < 0)
		return (int)cid;
	ch->local_id       = (uint32_t)cid;
	ch->local_window   = 0;
	ch->open           = false;
	ch->close_sent     = false;
	ch->close_received = false;
	ch->eof_sent       = false;
	ch->eof_received   = false;

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
			dssh_thrd_check(sess, cnd_wait(&ch->poll_cnd, &ch->buf_mtx));
		}
		else {
			if (dssh_thrd_check(sess, cnd_timedwait(&ch->poll_cnd, &ch->buf_mtx, &ts))
			    == thrd_timedout) {
				dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
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
send_channel_request_wait(struct dssh_session_s *sess, struct dssh_channel_s *ch, const char *req_type,
    const uint8_t *extra, size_t extra_len)
{
	size_t rtlen = strlen(req_type);

	if (rtlen > UINT32_MAX)
		return DSSH_ERROR_INVALID;
	if (extra_len > SIZE_MAX - CHAN_REQUEST_FIXED - rtlen)
		return DSSH_ERROR_INVALID;

	size_t   max;
	int      err;
	uint8_t *msg = send_begin(sess, SSH_MSG_CHANNEL_REQUEST, &max, &err);

	if (msg == NULL)
		return err;

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
	ch->request_pending   = true;
	ch->request_responded = false;
	ch->request_success   = false;
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	int res = send_commit(sess, pos, NULL);
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
	while (!ch->request_responded && !sess->terminate && !ch->close_received) {
		if (sess->timeout_ms <= 0) {
			dssh_thrd_check(sess, cnd_wait(&ch->poll_cnd, &ch->buf_mtx));
		}
		else {
			if (dssh_thrd_check(sess, cnd_timedwait(&ch->poll_cnd, &ch->buf_mtx, &rts))
			    == thrd_timedout) {
				ch->request_pending   = false;
				ch->request_responded = false;
				dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
				return DSSH_ERROR_TIMEOUT;
			}
		}
	}

	bool responded = ch->request_responded;
	bool success   = ch->request_success;

	ch->request_pending   = false;
	ch->request_responded = false;
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	if (!responded)
		return DSSH_ERROR_TERMINATED;
	return success ? 0 : DSSH_ERROR_REJECTED;
}

/*
 * Wait for the next message in the setup mailbox.
 * Returns 0 on success, negative on error/termination.
 */
static int
setup_recv(struct dssh_session_s *sess, struct dssh_channel_s *ch, uint8_t *msg_type, uint8_t **payload,
    size_t *payload_len)
{
	struct timespec sts;
	if (sess->timeout_ms > 0)
		deadline_from_ms(&sts, sess->timeout_ms);
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	while (!ch->setup_ready && !ch->setup_error && !sess->terminate && !ch->close_received) {
		if (sess->timeout_ms <= 0) {
			dssh_thrd_check(sess, cnd_wait(&ch->poll_cnd, &ch->buf_mtx));
		}
		else {
			if (dssh_thrd_check(sess, cnd_timedwait(&ch->poll_cnd, &ch->buf_mtx, &sts))
			    == thrd_timedout) {
				dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
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

	*msg_type       = ch->setup_msg_type;
	*payload        = ch->setup_payload;
	*payload_len    = ch->setup_payload_len;
	ch->setup_ready = false;

	/* Caller takes ownership of setup_payload and must free it */
	ch->setup_payload     = NULL;
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
setup_reply(struct dssh_session_s *sess, struct dssh_channel_s *ch, bool success)
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

	if (ch == NULL)
		return NULL;
	int64_t cid = alloc_channel_id(sess);
	if (cid < 0) {
		free(ch);
		return NULL;
	}
	ch->local_id     = (uint32_t)cid;
	ch->local_window = 0; /* initial_window=0; WINDOW_ADJUST after setup */
	ch->remote_id    = inc->peer_channel;
	atomic_init(&ch->remote_window, inc->peer_window);
	ch->remote_max_packet = inc->peer_max_packet;

	memset(&ch->wa_slot, 0, sizeof(ch->wa_slot));
	memset(&ch->chan_fail_slot, 0, sizeof(ch->chan_fail_slot));

	if (mtx_init(&ch->buf_mtx, mtx_plain) != thrd_success) {
		free(ch);
		return NULL;
	}
	if (mtx_init(&ch->cb_mtx, mtx_plain) != thrd_success) {
		mtx_destroy(&ch->buf_mtx);
		free(ch);
		return NULL;
	}
	if (cnd_init(&ch->poll_cnd) != thrd_success) {
		mtx_destroy(&ch->cb_mtx);
		mtx_destroy(&ch->buf_mtx);
		free(ch);
		return NULL;
	}
	if (alloc_channel_slots(sess, ch) < 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->cb_mtx);
		mtx_destroy(&ch->buf_mtx);
		free(ch);
		return NULL;
	}
	ch->setup_mode = true;

	int res = register_channel(sess, ch);

	if (res < 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->cb_mtx);
		mtx_destroy(&ch->buf_mtx);
		free(ch);
		return NULL;
	}

	res = send_open_confirmation(sess, ch, inc->peer_channel);
	if (res < 0) {
		unregister_channel(sess, ch);
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->cb_mtx);
		mtx_destroy(&ch->buf_mtx);
		free(ch);
		return NULL;
	}
	ch->open = true;
	return ch;
}

/* ================================================================
 * dssh_chan_* API
 * ================================================================ */

/* ---- ZC core (internal) ---- */

/*
 * Acquire tx_mtx, wait for rekey, drain tx_queue, check window,
 * return pointer into tx_packet data area.  Caller MUST call
 * zc_send_inner or zc_cancel_inner to release tx_mtx.
 *
 * On success, *buf points to the data area within tx_packet
 * (past seq + pkt_length + pad_len + channel header), and
 * *max_len is the maximum bytes the caller may write.
 * The channel header area (tx_packet[9..9+chan_hdr_len)) is
 * reserved for zc_send_inner to fill in.
 */
static int
zc_getbuf_inner(struct dssh_channel_s *ch, int stream, uint8_t **buf, size_t *max_len)
{
	struct dssh_session_s *sess = ch->sess;

	dssh_thrd_check(sess, mtx_lock(&sess->trans.tx_mtx));

	/* Wait for rekey to complete (same as send_packet) */
	if (sess->trans.rekey_in_progress) {
		struct timespec rk_ts;

		if (sess->timeout_ms > 0)
			dssh_deadline_from_ms(&rk_ts, sess->timeout_ms);
		while (sess->trans.rekey_in_progress && !sess->terminate) {
			if (sess->timeout_ms <= 0) {
				dssh_thrd_check(sess, cnd_wait(&sess->trans.rekey_cnd, &sess->trans.tx_mtx));
			}
			else {
				if (dssh_thrd_check(sess,
					cnd_timedwait(&sess->trans.rekey_cnd, &sess->trans.tx_mtx, &rk_ts))
				    == thrd_timedout) {
					session_set_terminate(sess);
					dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_mtx));
					return DSSH_ERROR_TERMINATED;
				}
			}
		}
		if (sess->terminate) {
			dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_mtx));
			return DSSH_ERROR_TERMINATED;
		}
	}

	/* Drain slots now (per-packet path) or defer to zc_send_inner
	 * (gather path — slots will be batched with the data packet). */
	if (!tx_gather_enabled())
		drain_tx_slots(sess);

	/* Check remote window under buf_mtx */
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	if (!ch->open || ch->eof_sent || ch->close_received) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_mtx));
		return DSSH_ERROR_TERMINATED;
	}

	uint32_t window  = atomic_load_explicit(&ch->remote_window, memory_order_acquire);
	uint32_t max_pkt = ch->remote_max_packet;

	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	size_t avail = window < max_pkt ? window : max_pkt;

	if (avail == 0) {
		dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_mtx));
		return DSSH_ERROR_NOMORE;
	}

	/* Channel header size: CHANNEL_DATA = 9, CHANNEL_EXTENDED_DATA = 13 */
	size_t chan_hdr = (stream == 0) ? CHAN_DATA_FIXED : CHAN_EXT_DATA_FIXED;

	/* Cap by transport buffer capacity:
	 * tx_packet layout: [seq(4)][pkt_len(4)][pad_len(1)][payload][pad][mac]
	 * payload = chan_hdr + data.  Need room for at least 4 bytes padding. */
	size_t max_payload = sess->trans.packet_buf_sz - 4 - 32;

	if (chan_hdr >= max_payload) {
		dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_mtx));
		return DSSH_ERROR_TOOLONG;
	}
	size_t max_data = max_payload - chan_hdr;

	if (avail > max_data)
		avail = max_data;

	/* Data area starts after: seq(4) + pkt_len(4) + pad_len(1) + chan_hdr */
	*buf          = &sess->trans.tx_packet[9 + chan_hdr];
	*max_len      = avail;
	ch->zc_stream = stream;
	return 0;
}

/*
 * Fill channel header, call tx_finalize, deduct window, release tx_mtx.
 * Caller MUST have called zc_getbuf_inner first.
 */
static int
zc_send_inner(struct dssh_channel_s *ch, size_t len)
{
	struct dssh_session_s *sess     = ch->sess;
	int                    stream   = ch->zc_stream;
	size_t                 chan_hdr = (stream == 0) ? CHAN_DATA_FIXED : CHAN_EXT_DATA_FIXED;

	/* Build channel header at tx_packet[9] */
	size_t   hp  = 0;
	uint8_t *hdr = &sess->trans.tx_packet[9];

	if (stream == 0) {
		hdr[hp++] = SSH_MSG_CHANNEL_DATA;
		DSSH_PUT_U32(ch->remote_id, hdr, &hp);
		uint32_t len_u32 = (uint32_t)len;
		DSSH_PUT_U32(len_u32, hdr, &hp);
	}
	else {
		hdr[hp++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
		DSSH_PUT_U32(ch->remote_id, hdr, &hp);
		DSSH_PUT_U32(SSH_EXTENDED_DATA_STDERR, hdr, &hp);
		uint32_t len_u32 = (uint32_t)len;
		DSSH_PUT_U32(len_u32, hdr, &hp);
	}

	size_t payload_len = chan_hdr + len;
	int    ret;

	if (tx_gather_enabled())
		ret = tx_gather_with_packet(sess, sess->trans.tx_packet, payload_len);
	else
		ret = tx_finalize(sess, sess->trans.tx_packet, payload_len);

	if (ret == 0) {
		uint32_t len_u32 = (uint32_t)len;
		window_atomic_sub(&ch->remote_window, len_u32);
	}

	if ((ret < 0) && (ret != DSSH_ERROR_TOOLONG) && (ret != DSSH_ERROR_REKEY_NEEDED))
		session_set_terminate(sess);

	dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_mtx));
	return ret;
}

/*
 * Release tx_mtx without sending.
 */
static void
zc_cancel_inner(struct dssh_channel_s *ch)
{
	struct dssh_session_s *sess = ch->sess;

	dssh_thrd_check(sess, mtx_unlock(&sess->trans.tx_mtx));
}

/* ---- Internal stream ZC callback (RX path) ---- */

/*
 * Registered as the zc_cb on stream-mode channels.  Called from the
 * demux thread with no library mutex held.  Copies data from rx_packet
 * into the per-channel ring buffer and broadcasts poll_cnd.
 * Returns bytes consumed for WINDOW_ADJUST.
 */
static uint32_t
stream_zc_cb(dssh_channel ch, int stream, const uint8_t *data, size_t len, void *cbdata)
{
	(void)cbdata;

	struct dssh_session_s *sess = ch->sess;

	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));

	struct dssh_bytebuf *bb      = (stream == 0) ? &ch->buf.stdout_buf : &ch->buf.stderr_buf;
	size_t               written = bytebuf_write(bb, data, len);

	dssh_thrd_check(sess, cnd_broadcast(&ch->poll_cnd));
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

	return (uint32_t)written;
}

static int
encode_modes(const struct dssh_chan_params *p, uint8_t **out, size_t *out_len)
{
	size_t   len = p->mode_count * 5 + 1;
	uint8_t *buf = malloc(len);

	if (buf == NULL)
		return DSSH_ERROR_ALLOC;

	size_t pos = 0;

	for (size_t i = 0; i < p->mode_count; i++) {
		if (p->modes[i].opcode >= 160)
			continue;
		buf[pos++] = p->modes[i].opcode;
		DSSH_PUT_U32(p->modes[i].value, buf, &pos);
	}
	for (size_t i = 0; i < p->mode_count; i++) {
		if (p->modes[i].opcode < 160)
			continue;
		buf[pos++] = p->modes[i].opcode;
		DSSH_PUT_U32(p->modes[i].value, buf, &pos);
	}
	buf[pos++] = 0;
	*out       = buf;
	*out_len   = pos;
	return 0;
}

static int
chan_send_pty_req(struct dssh_session_s *sess, struct dssh_channel_s *ch, const struct dssh_chan_params *p)
{
	uint8_t *modes = NULL;
	size_t   mlen  = 0;
	int      ret;

	ret = encode_modes(p, &modes, &mlen);
	if (ret < 0)
		return ret;

	size_t tlen = strlen(p->term);

	if (tlen > UINT32_MAX || mlen > UINT32_MAX) {
		free(modes);
		return DSSH_ERROR_INVALID;
	}

	size_t   extra_len = PTY_REQ_FIXED + tlen + mlen;
	uint8_t *extra     = malloc(extra_len);

	if (extra == NULL) {
		free(modes);
		return DSSH_ERROR_ALLOC;
	}

	size_t ep = 0;

	DSSH_PUT_U32((uint32_t)tlen, extra, &ep);
	memcpy(&extra[ep], p->term, tlen);
	ep += tlen;
	DSSH_PUT_U32(p->cols, extra, &ep);
	DSSH_PUT_U32(p->rows, extra, &ep);
	DSSH_PUT_U32(p->wpx, extra, &ep);
	DSSH_PUT_U32(p->hpx, extra, &ep);
	DSSH_PUT_U32((uint32_t)mlen, extra, &ep);
	if (mlen > 0)
		memcpy(&extra[ep], modes, mlen);
	ep += mlen;
	free(modes);

	ret = send_channel_request_wait(sess, ch, "pty-req", extra, ep);
	free(extra);
	return ret;
}

static int
chan_send_env(struct dssh_session_s *sess, struct dssh_channel_s *ch, const struct dssh_chan_params *p)
{
	for (size_t i = 0; i < p->env_count; i++) {
		size_t nlen = strlen(p->env[i].name);
		size_t vlen = strlen(p->env[i].value);

		if (nlen > UINT32_MAX || vlen > UINT32_MAX)
			return DSSH_ERROR_INVALID;

		size_t   extra_len = 4 + nlen + 4 + vlen;
		uint8_t *extra     = malloc(extra_len);

		if (extra == NULL)
			return DSSH_ERROR_ALLOC;

		size_t ep = 0;

		DSSH_PUT_U32((uint32_t)nlen, extra, &ep);
		memcpy(&extra[ep], p->env[i].name, nlen);
		ep += nlen;
		DSSH_PUT_U32((uint32_t)vlen, extra, &ep);
		memcpy(&extra[ep], p->env[i].value, vlen);
		ep += vlen;

		int ret = send_channel_request_wait(sess, ch, "env", extra, ep);

		free(extra);
		if (ret < 0 && ret != DSSH_ERROR_REJECTED)
			return ret;
	}
	return 0;
}

static int
chan_send_terminal_request(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    const struct dssh_chan_params *p)
{
	switch (p->type) {
		case DSSH_CHAN_SHELL:
			return send_channel_request_wait(sess, ch, "shell", NULL, 0);
		case DSSH_CHAN_EXEC:
		{
			size_t clen = strlen(p->command);
			if (clen > UINT32_MAX)
				return DSSH_ERROR_INVALID;
			size_t   extra_len = 4 + clen;
			uint8_t *extra     = malloc(extra_len);
			if (extra == NULL)
				return DSSH_ERROR_ALLOC;
			size_t ep = 0;
			DSSH_PUT_U32((uint32_t)clen, extra, &ep);
			memcpy(&extra[ep], p->command, clen);
			ep += clen;
			int ret = send_channel_request_wait(sess, ch, "exec", extra, ep);
			free(extra);
			return ret;
		}
		case DSSH_CHAN_SUBSYSTEM:
		{
			size_t slen = strlen(p->subsystem);
			if (slen > UINT32_MAX)
				return DSSH_ERROR_INVALID;
			size_t   extra_len = 4 + slen;
			uint8_t *extra     = malloc(extra_len);
			if (extra == NULL)
				return DSSH_ERROR_ALLOC;
			size_t ep = 0;
			DSSH_PUT_U32((uint32_t)slen, extra, &ep);
			memcpy(&extra[ep], p->subsystem, slen);
			ep += slen;
			int ret = send_channel_request_wait(sess, ch, "subsystem", extra, ep);
			free(extra);
			return ret;
		}
		default:
			return DSSH_ERROR_INVALID;
	}
}

DSSH_PUBLIC struct dssh_channel_s *
dssh_chan_open(struct dssh_session_s *sess, const struct dssh_chan_params *params)
{
	if (sess == NULL || params == NULL)
		return NULL;

	struct dssh_channel_s *ch = calloc(1, sizeof(*ch));

	if (ch == NULL)
		return NULL;

	uint32_t winsz = params->max_window != 0 ? params->max_window : INITIAL_WINDOW_SIZE;

	/* Phase 1: sync primitives only (no buffers yet) */
	int res = init_channel_sync(sess, ch);

	if (res < 0) {
		free(ch);
		return NULL;
	}

	ch->sess         = sess;
	ch->io_model     = DSSH_IO_STREAM;
	ch->zc_cb        = stream_zc_cb;
	ch->event_cb     = sess->default_event_cb;
	ch->event_cbdata = sess->default_event_cbdata;

	/* Init event queue early — demux can dispatch EOF/CLOSE events
	 * to this channel as soon as it's registered. */
	res = event_queue_init(&ch->events, sess->default_max_events);
	if (res < 0)
		goto fail_sync;

	/* CHANNEL_OPEN with initial_window=0 */
	res = open_session_channel(sess, ch);
	if (res < 0)
		goto fail_events;

	/* Setup: env, pty-req, terminal request */
	if (params->env_count > 0) {
		res = chan_send_env(sess, ch, params);
		if (res < 0)
			goto fail_close;
	}
	if (params->flags & DSSH_PARAM_HAS_PTY) {
		res = chan_send_pty_req(sess, ch, params);
		if (res < 0)
			goto fail_close;
	}
	res = chan_send_terminal_request(sess, ch, params);
	if (res < 0)
		goto fail_close;

	/* Phase 2: allocate buffers now that I/O model is committed */
	res = init_channel_buffers(ch, winsz);
	if (res < 0)
		goto fail_close;

	/* Open the window -- data can flow now */
	res = send_window_adjust(sess, ch, winsz);
	if (res < 0) {
		cleanup_channel_buffers(ch);
		goto fail_close;
	}

	/* Copy params for getters */
	res = dssh_chan_params_init(&ch->params, params->type);
	if (res < 0)
		goto fail_close;
	ch->params.flags      = params->flags;
	ch->params.max_window = winsz;
	ch->params.cols       = params->cols;
	ch->params.rows       = params->rows;
	ch->params.wpx        = params->wpx;
	ch->params.hpx        = params->hpx;
	dssh_chan_params_set_term(&ch->params, params->term);
	if (params->command != NULL)
		dssh_chan_params_set_command(&ch->params, params->command);
	if (params->subsystem != NULL)
		dssh_chan_params_set_subsystem(&ch->params, params->subsystem);

	return ch;

fail_close:
	conn_close(sess, ch);
	unregister_channel(sess, ch);
fail_events:
	event_queue_free(&ch->events);
fail_sync:
	cnd_destroy(&ch->poll_cnd);
	mtx_destroy(&ch->cb_mtx);
	mtx_destroy(&ch->buf_mtx);
	free(ch);
	return NULL;
}

DSSH_PUBLIC int64_t
dssh_chan_read(struct dssh_channel_s *ch, int stream, uint8_t *buf, size_t bufsz)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	if (ch->io_model != DSSH_IO_STREAM)
		return DSSH_ERROR_INVALID;
	if (buf == NULL && bufsz != 0)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;
	bool                   ext  = (stream != 0);
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	struct dssh_bytebuf *bb = ext ? &ch->buf.stderr_buf : &ch->buf.stdout_buf;
	if (buf == NULL) {
		size_t avail = bytebuf_available(bb);
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return (int64_t)avail;
	}
	size_t n = bytebuf_read(bb, buf, bufsz, 0);
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	if (n > 0) {
		int wret = maybe_replenish_window(sess, ch);
		if (wret < 0)
			return wret;
	}
	return (int64_t)n;
}

DSSH_PUBLIC int64_t
dssh_chan_write(struct dssh_channel_s *ch, int stream, const uint8_t *buf, size_t bufsz)
{
	if (ch == NULL || buf == NULL)
		return DSSH_ERROR_INVALID;
	if (ch->io_model != DSSH_IO_STREAM)
		return DSSH_ERROR_INVALID;
	if (bufsz == 0)
		return 0;

	uint8_t *zbuf;
	size_t   max_len;
	int      ret;

	ret = zc_getbuf_inner(ch, stream, &zbuf, &max_len);
	if (ret == DSSH_ERROR_NOMORE)
		return 0; /* window full, 0 bytes sent */
	if (ret < 0)
		return ret;

	size_t len = bufsz < max_len ? bufsz : max_len;

	memcpy(zbuf, buf, len);
	ret = zc_send_inner(ch, len);
	if (ret < 0)
		return ret;
	return (int64_t)len;
}

DSSH_PUBLIC int
dssh_chan_poll(struct dssh_channel_s *ch, int events, int timeout_ms)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	if (ch->io_model != DSSH_IO_STREAM)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;
	struct timespec        ts;
	if (timeout_ms > 0)
		dssh_deadline_from_ms(&ts, timeout_ms);
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	for (;;) {
		int ready = 0;
		if ((events & DSSH_POLL_READ)
		    && ((bytebuf_available(&ch->buf.stdout_buf) > 0) || ch->eof_received || ch->close_received))
			ready |= DSSH_POLL_READ;
		if ((events & DSSH_POLL_READEXT)
		    && ((bytebuf_available(&ch->buf.stderr_buf) > 0) || ch->eof_received || ch->close_received))
			ready |= DSSH_POLL_READEXT;
		if ((events & DSSH_POLL_WRITE)
		    && (atomic_load_explicit(&ch->remote_window, memory_order_acquire) > 0) && ch->open
		    && !ch->close_received)
			ready |= DSSH_POLL_WRITE;
		if ((events & DSSH_POLL_EVENT) && (ch->events.count > 0 || ch->events.has_frozen)) {
			if (!ch->events.has_frozen) {
				event_queue_freeze(&ch->events);
				/* Overwrite queue-time positions with unread
				 * byte counts at poll time (design line 392). */
				if (ch->events.has_frozen && ch->chan_type == DSSH_CHAN_SESSION) {
					ch->events.frozen.stdout_pos = ch->buf.stdout_buf.used;
					ch->events.frozen.stderr_pos = ch->buf.stderr_buf.used;
				}
			}
			if (ch->events.has_frozen)
				ready |= DSSH_POLL_EVENT;
		}
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
			if (dssh_thrd_check(sess, cnd_timedwait(&ch->poll_cnd, &ch->buf_mtx, &ts))
			    == thrd_timedout) {
				dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
				return 0;
			}
		}
	}
}

DSSH_PUBLIC int
dssh_chan_read_event(struct dssh_channel_s *ch, struct dssh_chan_event *event)
{
	if (ch == NULL || event == NULL)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	if (!ch->events.has_frozen) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return DSSH_ERROR_NOMORE;
	}
	struct dssh_event_entry *e = &ch->events.frozen;
	memset(event, 0, sizeof(*event));
	event->type       = e->type;
	event->stdout_pos = e->stdout_pos;
	event->stderr_pos = e->stderr_pos;
	switch (e->type) {
		case DSSH_EVENT_SIGNAL:
			event->signal.name = e->str_a;
			break;
		case DSSH_EVENT_WINDOW_CHANGE:
			event->window_change.cols = e->u32_a;
			event->window_change.rows = e->u32_b;
			event->window_change.wpx  = e->u32_c;
			event->window_change.hpx  = e->u32_d;
			break;
		case DSSH_EVENT_BREAK:
			event->brk.length = e->u32_a;
			break;
		case DSSH_EVENT_EXIT_STATUS:
			event->exit_status.exit_code = e->u32_a;
			break;
		case DSSH_EVENT_EXIT_SIGNAL:
			event->exit_signal.signal_name   = e->str_a;
			event->exit_signal.core_dumped   = e->flag_a;
			event->exit_signal.error_message = e->str_b;
			break;
		default:
			break;
	}
	ch->events.has_frozen = false;
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	return 0;
}

DSSH_PUBLIC int
dssh_chan_shutwr(struct dssh_channel_s *ch)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	if (in_zc_rx)
		return DSSH_ERROR_INVALID;
	return send_eof(ch->sess, ch);
}

DSSH_PUBLIC int
dssh_chan_close(struct dssh_channel_s *ch, int64_t exit_code)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	if (in_zc_rx)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;
	if (exit_code >= 0 && exit_code <= UINT32_MAX) {
		uint32_t ec = (uint32_t)exit_code;
		dssh_conn_send_exit_status(sess, ch, ec);
	}
	send_eof(sess, ch);
	conn_close(sess, ch);
	atomic_store(&ch->closing, true);
	unregister_channel(sess, ch);
	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	dssh_chan_params_free(&ch->params);
	event_queue_free(&ch->events);
	cleanup_channel_buffers(ch);
	free(ch);
	return 0;
}

DSSH_PUBLIC int
dssh_chan_send_signal(struct dssh_channel_s *ch, const char *signal_name)
{
	if (ch == NULL || signal_name == NULL)
		return DSSH_ERROR_INVALID;
	if (in_zc_rx)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess  = ch->sess;
	size_t                 slen  = strlen(signal_name);
	size_t                 rtlen = DSSH_STRLEN(str_signal);
	size_t                 fixed = CHAN_REQUEST_FIXED + rtlen + 4;
	if (slen > UINT32_MAX || slen > SIZE_MAX - fixed)
		return DSSH_ERROR_INVALID;
	size_t   max;
	int      err;
	uint8_t *msg = send_begin(sess, SSH_MSG_CHANNEL_REQUEST, &max, &err);
	if (msg == NULL)
		return err;
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);
	DSSH_PUT_U32((uint32_t)rtlen, msg, &pos);
	memcpy(&msg[pos], str_signal, rtlen);
	pos += rtlen;
	msg[pos++] = 0;
	DSSH_PUT_U32((uint32_t)slen, msg, &pos);
	memcpy(&msg[pos], signal_name, slen);
	pos += slen;
	return send_commit(sess, pos, NULL);
}

DSSH_PUBLIC int
dssh_chan_send_window_change(struct dssh_channel_s *ch, uint32_t cols, uint32_t rows, uint32_t wpx, uint32_t hpx)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	if (in_zc_rx)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;
	uint8_t                extra[WINDOW_CHANGE_FIXED];
	size_t                 ep = 0;
	DSSH_PUT_U32(cols, extra, &ep);
	DSSH_PUT_U32(rows, extra, &ep);
	DSSH_PUT_U32(wpx, extra, &ep);
	DSSH_PUT_U32(hpx, extra, &ep);
	size_t   rtlen = DSSH_STRLEN(str_window_change);
	size_t   max;
	int      err;
	uint8_t *msg = send_begin(sess, SSH_MSG_CHANNEL_REQUEST, &max, &err);
	if (msg == NULL)
		return err;
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);
	DSSH_PUT_U32((uint32_t)rtlen, msg, &pos);
	memcpy(&msg[pos], str_window_change, rtlen);
	pos += rtlen;
	msg[pos++] = 0;
	memcpy(&msg[pos], extra, ep);
	pos += ep;
	return send_commit(sess, pos, NULL);
}

DSSH_PUBLIC int
dssh_chan_send_break(struct dssh_channel_s *ch, uint32_t length)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	if (in_zc_rx)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;
	uint8_t                extra[4];
	size_t                 ep = 0;
	DSSH_PUT_U32(length, extra, &ep);
	size_t   rtlen = 5;
	size_t   max;
	int      err;
	uint8_t *msg = send_begin(sess, SSH_MSG_CHANNEL_REQUEST, &max, &err);
	if (msg == NULL)
		return err;
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);
	DSSH_PUT_U32((uint32_t)rtlen, msg, &pos);
	memcpy(&msg[pos], "break", rtlen);
	pos += rtlen;
	msg[pos++] = 0;
	memcpy(&msg[pos], extra, ep);
	pos += ep;
	return send_commit(sess, pos, NULL);
}

DSSH_PUBLIC enum dssh_chan_type
dssh_chan_get_type(struct dssh_channel_s *ch)
{
	return ch->params.type;
}

DSSH_PUBLIC const char *
dssh_chan_get_command(struct dssh_channel_s *ch)
{
	return ch->params.type != DSSH_CHAN_EXEC ? NULL : ch->params.command;
}

DSSH_PUBLIC const char *
dssh_chan_get_subsystem(struct dssh_channel_s *ch)
{
	return ch->params.type != DSSH_CHAN_SUBSYSTEM ? NULL : ch->params.subsystem;
}

DSSH_PUBLIC bool
dssh_chan_has_pty(struct dssh_channel_s *ch)
{
	return (ch->params.flags & DSSH_PARAM_HAS_PTY) != 0;
}

DSSH_PUBLIC const struct dssh_chan_params *
dssh_chan_get_pty(struct dssh_channel_s *ch)
{
	if (!(ch->params.flags & DSSH_PARAM_HAS_PTY))
		return NULL;
	return &ch->params;
}

/* ---- Public ZC API ---- */

DSSH_PUBLIC int
dssh_chan_zc_getbuf(struct dssh_channel_s *ch, int stream, uint8_t **buf, size_t *max_len)
{
	if (ch == NULL || buf == NULL || max_len == NULL)
		return DSSH_ERROR_INVALID;
	if (ch->io_model != DSSH_IO_ZC)
		return DSSH_ERROR_INVALID;
	if (in_zc_rx)
		return DSSH_ERROR_INVALID;
	return zc_getbuf_inner(ch, stream, buf, max_len);
}

DSSH_PUBLIC int
dssh_chan_zc_send(struct dssh_channel_s *ch, size_t len)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	if (in_zc_rx)
		return DSSH_ERROR_INVALID;
	return zc_send_inner(ch, len);
}

DSSH_PUBLIC int
dssh_chan_zc_cancel(struct dssh_channel_s *ch)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	zc_cancel_inner(ch);
	return 0;
}

DSSH_PUBLIC struct dssh_channel_s *
dssh_chan_zc_open(struct dssh_session_s *sess, const struct dssh_chan_params *params, dssh_chan_zc_cb cb,
    void *cbdata)
{
	if (sess == NULL || params == NULL || cb == NULL)
		return NULL;

	struct dssh_channel_s *ch = calloc(1, sizeof(*ch));

	if (ch == NULL)
		return NULL;

	uint32_t winsz = params->max_window != 0 ? params->max_window : INITIAL_WINDOW_SIZE;

	/* Phase 1: sync primitives only (ZC has no ring buffers) */
	int res = init_channel_sync(sess, ch);

	if (res < 0) {
		free(ch);
		return NULL;
	}

	ch->sess         = sess;
	ch->io_model     = DSSH_IO_ZC;
	ch->zc_cb        = cb;
	ch->zc_cbdata    = cbdata;
	ch->event_cb     = sess->default_event_cb;
	ch->event_cbdata = sess->default_event_cbdata;

	/* Init event queue early — demux can dispatch EOF/CLOSE events
	 * to this channel as soon as it's registered. */
	res = event_queue_init(&ch->events, sess->default_max_events);
	if (res < 0)
		goto fail_sync;

	/* CHANNEL_OPEN with initial_window=0 */
	res = open_session_channel(sess, ch);
	if (res < 0)
		goto fail_events;

	/* Setup: env, pty-req, terminal request */
	if (params->env_count > 0) {
		res = chan_send_env(sess, ch, params);
		if (res < 0)
			goto fail_close;
	}
	if (params->flags & DSSH_PARAM_HAS_PTY) {
		res = chan_send_pty_req(sess, ch, params);
		if (res < 0)
			goto fail_close;
	}
	res = chan_send_terminal_request(sess, ch, params);
	if (res < 0)
		goto fail_close;

	/* ZC mode: no ring buffers, just event queue + window */
	ch->window_max = winsz;

	/* Open the window */
	res = send_window_adjust(sess, ch, winsz);
	if (res < 0)
		goto fail_close;

	/* Copy params for getters */
	res = dssh_chan_params_init(&ch->params, params->type);
	if (res < 0)
		goto fail_close;
	ch->params.flags      = params->flags;
	ch->params.max_window = winsz;
	ch->params.cols       = params->cols;
	ch->params.rows       = params->rows;
	ch->params.wpx        = params->wpx;
	ch->params.hpx        = params->hpx;
	dssh_chan_params_set_term(&ch->params, params->term);
	if (params->command != NULL)
		dssh_chan_params_set_command(&ch->params, params->command);
	if (params->subsystem != NULL)
		dssh_chan_params_set_subsystem(&ch->params, params->subsystem);

	return ch;

fail_close:
	conn_close(sess, ch);
	unregister_channel(sess, ch);
fail_events:
	event_queue_free(&ch->events);
fail_sync:
	cnd_destroy(&ch->poll_cnd);
	mtx_destroy(&ch->cb_mtx);
	mtx_destroy(&ch->buf_mtx);
	free(ch);
	return NULL;
}

/* ---- Event callback setters ---- */

DSSH_PUBLIC int
dssh_chan_set_event_cb(struct dssh_channel_s *ch, dssh_chan_event_cb cb, void *cbdata)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;

	dssh_thrd_check(sess, mtx_lock(&ch->cb_mtx));
	ch->event_cb     = cb;
	ch->event_cbdata = cbdata;
	dssh_thrd_check(sess, mtx_unlock(&ch->cb_mtx));
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_event_cb(struct dssh_session_s *sess, dssh_chan_event_cb cb, void *cbdata)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	sess->default_event_cb     = cb;
	sess->default_event_cbdata = cbdata;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_max_events(struct dssh_session_s *sess, size_t max_events)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	sess->default_max_events = max_events;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_set_max_events(struct dssh_channel_s *ch, size_t max_events)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;

	dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
	if (max_events != 0 && max_events < ch->events.count) {
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
		return DSSH_ERROR_INVALID;
	}
	ch->events.max_events = max_events;
	dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
	return 0;
}

/* ================================================================
 * Server accept -- dssh_chan_accept
 * ================================================================ */

static int
accept_parse_pty(const uint8_t *data, size_t data_len, struct dssh_chan_params *p)
{
	struct pty_req pty;
	int            ret = parse_pty_req_data(data, data_len, &pty);
	if (ret < 0)
		return ret;
	p->flags |= DSSH_PARAM_HAS_PTY;
	dssh_chan_params_set_term(p, pty.term);
	p->cols = pty.cols;
	p->rows = pty.rows;
	p->wpx  = pty.wpx;
	p->hpx  = pty.hpx;
	if (pty.modes != NULL && pty.modes_len > 0) {
		size_t mpos = 0;
		while (mpos < pty.modes_len) {
			uint8_t opcode = pty.modes[mpos];
			if (opcode == 0 || opcode >= 160)
				break;
			mpos++;
			if (mpos + 4 > pty.modes_len)
				break;
			uint32_t val = DSSH_GET_U32(&pty.modes[mpos]);
			mpos += 4;
			dssh_chan_params_set_mode(p, opcode, val);
		}
	}
	return 0;
}

static int
accept_parse_env(const uint8_t *data, size_t data_len, struct dssh_chan_params *p)
{
	const uint8_t *name, *value;
	size_t         nlen, vlen;
	int            ret = parse_env_data(data, data_len, &name, &nlen, &value, &vlen);
	if (ret < 0)
		return ret;
	char *nstr = malloc(nlen + 1);
	if (nstr == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(nstr, name, nlen);
	nstr[nlen] = '\0';
	char *vstr = malloc(vlen + 1);
	if (vstr == NULL) {
		free(nstr);
		return DSSH_ERROR_ALLOC;
	}
	memcpy(vstr, value, vlen);
	vstr[vlen] = '\0';
	ret        = dssh_chan_params_add_env(p, nstr, vstr);
	free(nstr);
	free(vstr);
	return ret;
}

static int
accept_parse_exec(const uint8_t *data, size_t data_len, struct dssh_chan_params *p)
{
	if (data_len < 4)
		return DSSH_ERROR_PARSE;
	uint32_t cmdlen = DSSH_GET_U32(data);
	if (4 + cmdlen > data_len)
		return DSSH_ERROR_PARSE;
	char *cstr = malloc(cmdlen + 1);
	if (cstr == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(cstr, &data[4], cmdlen);
	cstr[cmdlen] = '\0';
	int ret      = dssh_chan_params_set_command(p, cstr);
	free(cstr);
	return ret;
}

static int
accept_parse_subsystem(const uint8_t *data, size_t data_len, struct dssh_chan_params *p)
{
	if (data_len < 4)
		return DSSH_ERROR_PARSE;
	uint32_t namelen = DSSH_GET_U32(data);
	if (4 + namelen > data_len)
		return DSSH_ERROR_PARSE;
	char *nstr = malloc(namelen + 1);
	if (nstr == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(nstr, &data[4], namelen);
	nstr[namelen] = '\0';
	int ret       = dssh_chan_params_set_subsystem(p, nstr);
	free(nstr);
	return ret;
}

/*
 * After sending CHANNEL_CLOSE for a rejected channel, drain the setup
 * mailbox until the peer's reciprocal CHANNEL_CLOSE arrives.  Best-effort:
 * on timeout or session termination we give up and let the channel be freed
 * (the demux will silently discard the late CLOSE via find_channel→NULL).
 */
static void
wait_for_close(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	for (;;) {
		uint8_t  msg_type;
		uint8_t *payload;
		size_t   payload_len;
		int      ret = setup_recv(sess, ch, &msg_type, &payload, &payload_len);

		if (ret < 0)
			break;
		free(payload);
		if (msg_type == SSH_MSG_CHANNEL_CLOSE)
			break;
	}
}

static int
chan_accept_setup_loop(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    const struct dssh_chan_accept_cbs *cbs, struct dssh_chan_params *p, struct dssh_chan_accept_result *result)
{
	bool had_pty = false, had_terminal = false;
	for (;;) {
		uint8_t  msg_type;
		uint8_t *payload;
		size_t   payload_len;
		int      res = setup_recv(sess, ch, &msg_type, &payload, &payload_len);
		if (res < 0)
			return res;
		if (msg_type == SSH_MSG_CHANNEL_WINDOW_ADJUST) {
			if (payload_len >= 9)
				window_atomic_add(&ch->remote_window, DSSH_GET_U32(&payload[5]));
			free(payload);
			continue;
		}
		if (msg_type != SSH_MSG_CHANNEL_REQUEST) {
			free(payload);
			continue;
		}
		const uint8_t *rtype;
		uint32_t       rtype_len;
		bool           want_reply;
		const uint8_t *req_data;
		size_t         req_data_len;
		if (parse_channel_request(payload, payload_len, &rtype, &rtype_len, &want_reply, &req_data,
			&req_data_len)
		    < 0) {
			setup_reply(sess, ch, false);
			free(payload);
			dssh_transport_disconnect(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
			    "malformed CHANNEL_REQUEST");
			return DSSH_ERROR_TERMINATED;
		}

		if (rtype_len == DSSH_STRLEN(str_pty_req)
		    && memcmp(rtype, str_pty_req, DSSH_STRLEN(str_pty_req)) == 0) {
			if (had_pty) {
				free(payload);
				dssh_transport_disconnect(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
				    "duplicate pty-req");
				return DSSH_ERROR_TERMINATED;
			}
			if (had_terminal) {
				if (want_reply)
					setup_reply(sess, ch, false);
				free(payload);
				continue;
			}
			had_pty = true;
			res     = accept_parse_pty(req_data, req_data_len, p);
			if (res < 0) {
				if (want_reply)
					setup_reply(sess, ch, false);
				free(payload);
				continue;
			}
			int cb_res = 0;
			if (cbs->pty_req != NULL)
				cb_res = cbs->pty_req(ch, p, cbs->cbdata);
			if (cb_res < 0)
				p->flags &= ~(uint32_t)DSSH_PARAM_HAS_PTY;
			if (want_reply)
				setup_reply(sess, ch, cb_res >= 0);
			free(payload);
			continue;
		}
		if (rtype_len == DSSH_STRLEN(str_env) && memcmp(rtype, str_env, DSSH_STRLEN(str_env)) == 0) {
			if (had_terminal) {
				if (want_reply)
					setup_reply(sess, ch, false);
				free(payload);
				continue;
			}
			int cb_res = -1;
			if (cbs->env != NULL) {
				res = accept_parse_env(req_data, req_data_len, p);
				if (res >= 0)
					cb_res = cbs->env(ch, p, cbs->cbdata);
			}
			if (want_reply)
				setup_reply(sess, ch, cb_res >= 0);
			free(payload);
			continue;
		}
		if (rtype_len == DSSH_STRLEN(str_shell) && memcmp(rtype, str_shell, DSSH_STRLEN(str_shell)) == 0) {
			if (had_terminal) {
				if (want_reply)
					setup_reply(sess, ch, false);
				free(payload);
				continue;
			}
			had_terminal = true;
			p->type      = DSSH_CHAN_SHELL;
			int cb_res   = -1;
			if (cbs->shell != NULL)
				cb_res = cbs->shell(ch, p, result, cbs->cbdata);
			if (want_reply)
				setup_reply(sess, ch, cb_res >= 0);
			free(payload);
			if (cb_res >= 0)
				return 0;
			conn_close(sess, ch);
			wait_for_close(sess, ch);
			return DSSH_ERROR_REJECTED;
		}
		if (rtype_len == DSSH_STRLEN(str_exec) && memcmp(rtype, str_exec, DSSH_STRLEN(str_exec)) == 0) {
			if (had_terminal) {
				if (want_reply)
					setup_reply(sess, ch, false);
				free(payload);
				continue;
			}
			had_terminal = true;
			p->type      = DSSH_CHAN_EXEC;
			res          = accept_parse_exec(req_data, req_data_len, p);
			int cb_res   = -1;
			if (res >= 0 && cbs->exec != NULL)
				cb_res = cbs->exec(ch, p, result, cbs->cbdata);
			if (want_reply)
				setup_reply(sess, ch, cb_res >= 0);
			free(payload);
			if (cb_res >= 0)
				return 0;
			conn_close(sess, ch);
			wait_for_close(sess, ch);
			return DSSH_ERROR_REJECTED;
		}
		if (rtype_len == DSSH_STRLEN(str_subsystem)
		    && memcmp(rtype, str_subsystem, DSSH_STRLEN(str_subsystem)) == 0) {
			if (had_terminal) {
				if (want_reply)
					setup_reply(sess, ch, false);
				free(payload);
				continue;
			}
			had_terminal = true;
			p->type      = DSSH_CHAN_SUBSYSTEM;
			res          = accept_parse_subsystem(req_data, req_data_len, p);
			int cb_res   = -1;
			if (res >= 0 && cbs->subsystem != NULL)
				cb_res = cbs->subsystem(ch, p, result, cbs->cbdata);
			if (want_reply)
				setup_reply(sess, ch, cb_res >= 0);
			free(payload);
			if (cb_res >= 0)
				return 0;
			conn_close(sess, ch);
			wait_for_close(sess, ch);
			return DSSH_ERROR_REJECTED;
		}
		if (want_reply)
			setup_reply(sess, ch, false);
		free(payload);
	}
}

DSSH_PUBLIC struct dssh_channel_s *
dssh_chan_accept(struct dssh_session_s *sess, const struct dssh_chan_accept_cbs *cbs, int timeout_ms)
{
	if (sess == NULL || cbs == NULL)
		return NULL;

	for (;;) {
		struct dssh_incoming_open inc;
		int                       res = accept_incoming(sess, &inc, timeout_ms);

		if (res < 0)
			return NULL;

		struct dssh_channel_s *ch = accept_channel_init(sess, &inc);

		if (ch == NULL)
			return NULL;

		ch->sess         = sess;
		ch->io_model     = DSSH_IO_STREAM;
		ch->zc_cb        = stream_zc_cb;
		ch->event_cb     = sess->default_event_cb;
		ch->event_cbdata = sess->default_event_cbdata;

		/* Init event queue before setup loop — demux can dispatch
	 * EOF/CLOSE events to this channel during reject cleanup. */
		res = event_queue_init(&ch->events, sess->default_max_events);
		if (res < 0) {
			unregister_channel(sess, ch);
			free(ch->setup_payload);
			cnd_destroy(&ch->poll_cnd);
			mtx_destroy(&ch->cb_mtx);
			mtx_destroy(&ch->buf_mtx);
			free(ch);
			return NULL;
		}

		struct dssh_chan_params p;

		dssh_chan_params_init(&p, DSSH_CHAN_SHELL);
		p.flags &= ~(uint32_t)DSSH_PARAM_HAS_PTY;

		struct dssh_chan_accept_result result = {
		    .max_window = 0,
		    .zc_cb      = NULL,
		    .zc_cbdata  = NULL,
		};

		res = chan_accept_setup_loop(sess, ch, cbs, &p, &result);
		if (res < 0) {
			dssh_chan_params_free(&p);

			/* On reject, clean up this channel and loop back to
		 * accept the next CHANNEL_OPEN (design lines 945-946).
		 * wait_for_close() already drained the peer's reciprocal
		 * CLOSE in chan_accept_setup_loop's reject path. */
			dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
			ch->setup_mode = false;
			dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
			event_queue_free(&ch->events);
			unregister_channel(sess, ch);
			free(ch->setup_payload);
			cnd_destroy(&ch->poll_cnd);
			mtx_destroy(&ch->cb_mtx);
			mtx_destroy(&ch->buf_mtx);
			free(ch);

			if (res == DSSH_ERROR_REJECTED)
				continue;
			return NULL;
		}
		uint32_t winsz = result.max_window != 0 ? result.max_window : INITIAL_WINDOW_SIZE;

		/* Commit I/O model based on app's terminal request callback.
	 * ZC mode: app callback, no ring buffers.
	 * Stream mode: internal adapter + ring buffers. */
		if (result.zc_cb != NULL) {
			ch->io_model  = DSSH_IO_ZC;
			ch->zc_cb     = result.zc_cb;
			ch->zc_cbdata = result.zc_cbdata;
		}
		/* else: stream mode already set before setup loop */

		dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx));
		ch->window_max         = winsz;
		ch->chan_type          = DSSH_CHAN_SESSION;
		ch->exit_code          = 0;
		ch->exit_code_received = false;
		if (ch->io_model == DSSH_IO_STREAM) {
			res = bytebuf_init(&ch->buf.stdout_buf, winsz);
			if (res < 0) {
				ch->setup_mode = false;
				dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
				dssh_chan_params_free(&p);
				event_queue_free(&ch->events);
				unregister_channel(sess, ch);
				free(ch->setup_payload);
				cnd_destroy(&ch->poll_cnd);
				mtx_destroy(&ch->cb_mtx);
				mtx_destroy(&ch->buf_mtx);
				free(ch);
				return NULL;
			}
			res = bytebuf_init(&ch->buf.stderr_buf, winsz);
			if (res < 0) {
				bytebuf_free(&ch->buf.stdout_buf);
				ch->setup_mode = false;
				dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));
				dssh_chan_params_free(&p);
				event_queue_free(&ch->events);
				unregister_channel(sess, ch);
				free(ch->setup_payload);
				cnd_destroy(&ch->poll_cnd);
				mtx_destroy(&ch->cb_mtx);
				mtx_destroy(&ch->buf_mtx);
				free(ch);
				return NULL;
			}
		}
		ch->setup_mode = false;
		dssh_thrd_check(sess, mtx_unlock(&ch->buf_mtx));

		/* Open the window -- data can flow now */
		res = send_window_adjust(sess, ch, winsz);
		if (res < 0) {
			event_queue_free(&ch->events);
			dssh_chan_params_free(&p);
			unregister_channel(sess, ch);
			cleanup_channel_buffers(ch);
			free(ch);
			return NULL;
		}

		ch->params = p;
		return ch;
	} /* accept loop */
}
