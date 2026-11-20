// RFC 4254: SSH Connection Protocol

#include <stdlib.h>
#include <string.h>

#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "ssh-internal.h"
#include "ssh-window.h"
#include "ssh-ring.h"

#ifdef DSSH_TESTING
 #include "dssh_test_alloc.h"
 #include "dssh_test_ossl.h"
#endif

/* Thread-local guard: true when inside a ZC RX callback on the demux
 * thread.  TX functions check this and return DSSH_ERROR_INVALID to
 * prevent deadlock (demux thread calling TX while holding no mutex,
 * but TX needs tx_mtx which may wait on rekey which needs demux). */
static _Thread_local bool in_zc_rx;
static _Thread_local struct dssh_channel_s *zc_reserved_channel;
static _Thread_local size_t zc_reserved_capacity;

#ifdef DSSH_TESTING
static _Thread_local atomic_bool *detach_wait_observer;

DSSH_TESTABLE void
dssh_test_detach_observe(atomic_bool *waiting)
{
	detach_wait_observer = waiting;
}
#endif

static uint32_t stream_zc_cb(struct dssh_channel_s *ch, int stream, const uint8_t *data, size_t len, void *cbdata);

DSSH_PRIVATE bool
dssh_channel_rx_active(void)
{
	return in_zc_rx;
}

/* Setup callbacks borrow a channel on the accept caller thread. A linked
 * stack scope supports independent nested sessions without allocation. */
struct setup_callback_scope {
	struct dssh_session_s *sess;
	struct dssh_channel_s *ch;
	const struct dssh_chan_params *params;
	struct setup_callback_scope *previous;
	bool cancelled;
};

static _Thread_local struct setup_callback_scope *active_setup_callback;

DSSH_PRIVATE bool
dssh_setup_callback_active(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return false;
	for (struct setup_callback_scope *scope = active_setup_callback; scope != NULL;
	    scope = scope->previous) {
		if (scope->sess == sess)
			return true;
	}
	return false;
}

static struct setup_callback_scope *
setup_scope_for_channel(struct dssh_channel_s *ch)
{
	for (struct setup_callback_scope *scope = active_setup_callback; scope != NULL;
	    scope = scope->previous) {
		if (scope->ch == ch)
			return scope;
	}
	return NULL;
}

static const struct dssh_chan_params *
channel_params_view(struct dssh_channel_s *ch)
{
	struct setup_callback_scope *scope = setup_scope_for_channel(ch);
	return scope != NULL ? scope->params : &ch->params;
}

typedef int (*setup_request_callback)(dssh_channel, const struct dssh_chan_params *, void *);
typedef int (*setup_terminal_callback)(dssh_channel, const struct dssh_chan_params *,
    struct dssh_chan_accept_result *, void *);

static int
call_setup_request(struct dssh_channel_s *ch, const struct dssh_chan_params *params,
    setup_request_callback callback, void *data, bool *cancelled)
{
	struct setup_callback_scope scope = {.sess = ch != NULL ? ch->sess : NULL,
	    .ch = ch, .params = params, .previous = active_setup_callback};
	active_setup_callback = &scope;
	int ret = callback(ch, params, data);
	active_setup_callback = scope.previous;
	*cancelled = scope.cancelled;
	return ret;
}

static int
call_setup_terminal(struct dssh_channel_s *ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, setup_terminal_callback callback,
    void *data, bool *cancelled)
{
	struct setup_callback_scope scope = {.sess = ch->sess, .ch = ch,
	    .params = params, .previous = active_setup_callback};
	active_setup_callback = &scope;
	int ret = callback(ch, params, result, data);
	active_setup_callback = scope.previous;
	*cancelled = scope.cancelled;
	return ret;
}

static bool
channel_api_forbidden(struct dssh_channel_s *ch)
{
	if (ch == NULL)
		return false;
	struct setup_callback_scope *scope = setup_scope_for_channel(ch);
	return (scope != NULL && scope->cancelled)
	    || (ch->sess != NULL && dssh_io_callback_active(ch->sess));
}

static bool
channel_io_forbidden(struct dssh_channel_s *ch)
{
	return channel_api_forbidden(ch) || dssh_setup_callback_active(ch->sess);
}

static bool
connection_api_forbidden(struct dssh_session_s *sess)
{
	return sess != NULL && (dssh_io_callback_active(sess) || dssh_setup_callback_active(sess));
}

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

	b->tail = dssh_ring_advance(b->tail, len, b->capacity);
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

	b->head = dssh_ring_advance(b->head, avail, b->capacity);
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
 * A full queue returns TOOMANY; the demux rejects that new open without
 * stalling dispatch to channels that already exist.
 */
DSSH_TESTABLE int
acceptqueue_push(struct dssh_accept_queue *q, uint32_t peer_channel, uint32_t peer_window,
    uint32_t peer_max_packet, const uint8_t *type, size_t type_len)
{
	if (q->count >= DSSH_ACCEPT_QUEUE_CAP)
		return DSSH_ERROR_TOOMANY;

	size_t                     idx = (q->head + q->count) % DSSH_ACCEPT_QUEUE_CAP;
	struct dssh_incoming_open *e   = &q->entries[idx];

	e->protocol_id     = NULL;
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

/* Reservation owns only new allocations. Retired storage is returned separately
 * so the dispatch path can release every physical mutex before freeing it. */
struct event_queue_reservation {
	struct dssh_event_entry *entries;
	uint8_t *text;
	size_t capacity;
	size_t text_capacity;
};

static size_t
event_queue_memory_limit(const struct dssh_event_queue *q)
{
	size_t count = q->max_events != 0 ? q->max_events : DSSH_DEFAULT_MAX_EVENTS;
	size_t slots = EVENT_QUEUE_INITIAL_CAPACITY;
	while (slots < count) {
		if (slots > SIZE_MAX / 2)
			return SIZE_MAX;
		slots *= 2;
	}
	/* One frozen allocation and one in-flight replacement. Include metadata
	 * growth as well as text capacities, rather than just live text lengths. */
	if (slots > SIZE_MAX - 2 || sizeof(struct dssh_event_entry) > SIZE_MAX / 2)
		return SIZE_MAX;
	slots += 2;
	size_t overhead = 2 * sizeof(struct dssh_event_entry);
	if (q->packet_limit > SIZE_MAX - overhead)
		return SIZE_MAX;
	size_t unit = q->packet_limit + overhead;
	if (slots > SIZE_MAX / unit)
		return SIZE_MAX;
	return slots * unit;
}

static int
event_entry_text_size(const struct dssh_event_queue *q,
    const struct dssh_event_entry *e, size_t *size)
{
	if (e->text_a.len > q->packet_limit
	    || e->text_b.len > q->packet_limit - e->text_a.len)
		return DSSH_ERROR_TOOMANY;
	size_t total = e->text_a.len + e->text_b.len;
	if (e->language.len > q->packet_limit - total)
		return DSSH_ERROR_TOOMANY;
	*size = total + e->language.len;
	return 0;
}

/* Called with the channel buffer mutex, or on an unshared unit-test queue.
 * This stage only computes sizes; it never allocates or copies event text. */
static int
event_queue_plan(const struct dssh_event_queue *q, const struct dssh_event_entry *e,
    struct event_queue_reservation *r)
{
	if (q->max_events != 0 && q->count >= q->max_events)
		return DSSH_ERROR_TOOMANY;
	size_t text_size = 0;
	int ret = event_entry_text_size(q, e, &text_size);
	if (ret < 0)
		return ret;
	if (q->count == q->capacity) {
		if (q->capacity > SIZE_MAX / 2)
			return DSSH_ERROR_TOOMANY;
		r->capacity = q->capacity * 2;
		if (r->capacity > SIZE_MAX / sizeof(*q->entries))
			return DSSH_ERROR_TOOMANY;
	}
	size_t available = r->capacity != 0 ? 0 : q->entries[q->tail].storage_capacity;
	if (text_size > available) {
		r->text_capacity = available;
		if (r->text_capacity == 0)
			r->text_capacity = 64;
		while (r->text_capacity < text_size) {
			if (r->text_capacity > SIZE_MAX / 2) {
				r->text_capacity = text_size;
				break;
			}
			r->text_capacity *= 2;
		}
	}
	if (r->text_capacity > q->packet_limit)
		r->text_capacity = text_size;
	size_t limit = event_queue_memory_limit(q);
	size_t used = q->capacity * sizeof(*q->entries);
	if (q->text_capacity > SIZE_MAX - used)
		return DSSH_ERROR_TOOMANY;
	used += q->text_capacity;
	size_t metadata = r->capacity * sizeof(*q->entries);
	if (metadata > SIZE_MAX - used)
		return DSSH_ERROR_TOOMANY;
	used += metadata;
	if (r->capacity == 0 && r->text_capacity == 0)
		return 0; /* Reuse remains possible after a limit is lowered. */
	if (used > limit || text_size > limit - used)
		return DSSH_ERROR_TOOMANY;
	if (r->text_capacity > limit - used)
		r->text_capacity = text_size;
	return 0;
}

static int
event_queue_allocate(struct event_queue_reservation *r)
{
	if (r->capacity != 0) {
		r->entries = malloc(r->capacity * sizeof(*r->entries));
		if (r->entries == NULL)
			return DSSH_ERROR_ALLOC;
		for (size_t i = 0; i < r->capacity; i++)
			r->entries[i] = (struct dssh_event_entry){0};
	}
	if (r->text_capacity != 0) {
		r->text = malloc(r->text_capacity);
		if (r->text == NULL)
			return DSSH_ERROR_ALLOC;
	}
	return 0;
}

static void
event_queue_reservation_free(struct event_queue_reservation *r)
{
	free(r->entries);
	free(r->text);
	*r = (struct event_queue_reservation){0};
}

DSSH_TESTABLE int
event_queue_init(struct dssh_event_queue *q, size_t max_events, size_t packet_limit)
{
	*q = (struct dssh_event_queue){.max_events = max_events, .packet_limit = packet_limit};
	if (EVENT_QUEUE_INITIAL_CAPACITY > SIZE_MAX / sizeof(*q->entries))
		return DSSH_ERROR_ALLOC;
	q->entries = malloc(EVENT_QUEUE_INITIAL_CAPACITY * sizeof(*q->entries));
	if (q->entries == NULL)
		return DSSH_ERROR_ALLOC;
	q->capacity = EVENT_QUEUE_INITIAL_CAPACITY;
	for (size_t i = 0; i < q->capacity; i++)
		q->entries[i] = (struct dssh_event_entry){0};
	return 0;
}

DSSH_TESTABLE void
event_queue_free(struct dssh_event_queue *q)
{
	for (size_t i = 0; i < q->capacity; i++)
		free(q->entries[i].storage);
	free(q->frozen.storage);
	free(q->entries);
	*q = (struct dssh_event_queue){0};
}

/* No allocation/free here. Consumer activity may have changed head/count or
 * the frozen storage while a reservation was being allocated, so move every
 * old slot (including unused retained buffers) from the CURRENT head. */
static int
event_queue_commit(struct dssh_event_queue *q, const struct dssh_event_entry *e,
    struct event_queue_reservation *r, struct event_queue_reservation *retired)
{
	if (q->max_events != 0 && q->count >= q->max_events)
		return DSSH_ERROR_TOOMANY;
	size_t text_size = 0;
	int ret = event_entry_text_size(q, e, &text_size);
	if (ret < 0)
		return ret;
	/* A concurrent count-limit change cannot authorize a larger allocation
	 * retroactively. Check the current envelope before transferring storage. */
	size_t used = q->capacity * sizeof(*q->entries);
	if (q->text_capacity > SIZE_MAX - used)
		return DSSH_ERROR_TOOMANY;
	used += q->text_capacity;
	size_t extra = r->capacity * sizeof(*q->entries);
	if (r->text_capacity > SIZE_MAX - extra)
		return DSSH_ERROR_TOOMANY;
	extra += r->text_capacity;
	if (extra != 0) {
		size_t limit = event_queue_memory_limit(q);
		if (used > limit || extra > limit - used)
			return DSSH_ERROR_TOOMANY;
	}
	if (r->entries != NULL) {
		size_t idx = q->head;
		for (size_t i = 0; i < q->capacity; i++) {
			r->entries[i] = q->entries[idx];
			idx = idx == q->capacity - 1 ? 0 : idx + 1;
		}
		retired->entries = q->entries;
		q->entries = r->entries;
		q->capacity = r->capacity;
		q->head = 0;
		q->tail = q->count;
		r->entries = NULL;
	}
	struct dssh_event_entry *slot = &q->entries[q->tail];
	if (text_size > slot->storage_capacity) {
		if (r->text == NULL || r->text_capacity < text_size)
			return DSSH_ERROR_INVALID;
		retired->text = slot->storage;
		q->text_capacity -= slot->storage_capacity;
		q->text_capacity += r->text_capacity;
		slot->storage = r->text;
		slot->storage_capacity = r->text_capacity;
		r->text = NULL;
	}
	uint8_t *storage = slot->storage;
	size_t capacity = slot->storage_capacity;
	*slot = *e;
	slot->storage = storage;
	slot->storage_capacity = capacity;
	size_t off = 0;
	struct dssh_bytes *views[] = {&slot->text_a, &slot->text_b, &slot->language};
	for (size_t i = 0; i < sizeof(views) / sizeof(views[0]); i++) {
		if (views[i]->len == 0)
			views[i]->data = NULL;
		else {
			memcpy(storage + off, views[i]->data, views[i]->len);
			views[i]->data = storage + off;
			off += views[i]->len;
		}
	}
	q->tail = q->tail == q->capacity - 1 ? 0 : q->tail + 1;
	q->count++;
	return 0;
}

/* Unit-test convenience for an unshared queue. Production dispatch separates
 * planning/commit under buf_mtx from allocation/free with no mutex held. */
#ifdef DSSH_TESTING
DSSH_TESTABLE int
event_queue_push(struct dssh_event_queue *q, const struct dssh_event_entry *e)
{
	struct event_queue_reservation r = {0};
	struct event_queue_reservation retired = {0};
	int ret = event_queue_plan(q, e, &r);
	if (ret == 0)
		ret = event_queue_allocate(&r);
	if (ret == 0)
		ret = event_queue_commit(q, e, &r, &retired);
	event_queue_reservation_free(&retired);
	event_queue_reservation_free(&r);
	return ret;
}

#endif

DSSH_TESTABLE bool
event_queue_freeze(struct dssh_event_queue *q)
{
	q->has_frozen = false;
	if (q->count == 0)
		return false;
	struct dssh_event_entry old = q->frozen;
	q->frozen = q->entries[q->head];
	q->entries[q->head] = old;
	q->head = q->head == q->capacity - 1 ? 0 : q->head + 1;
	q->count--;
	q->has_frozen = true;
	return true;
}

static void
event_from_entry(const struct dssh_event_entry *e, struct dssh_chan_event *event)
{
	struct dssh_chan_event converted = {
		.type = e->type, .stdout_pos = e->stdout_pos, .stderr_pos = e->stderr_pos,
	};
	switch (e->type) {
		case DSSH_EVENT_SIGNAL:
			converted.signal.name = e->text_a;
			break;
		case DSSH_EVENT_WINDOW_CHANGE:
			converted.window_change.cols = e->u32_a;
			converted.window_change.rows = e->u32_b;
			converted.window_change.wpx  = e->u32_c;
			converted.window_change.hpx  = e->u32_d;
			break;
		case DSSH_EVENT_EXIT_STATUS:
			converted.exit_status.exit_code = e->u32_a;
			break;
		case DSSH_EVENT_EXIT_SIGNAL:
			converted.exit_signal.signal_name   = e->text_a;
			converted.exit_signal.core_dumped   = e->flag_a;
			converted.exit_signal.error_message = e->text_b;
			converted.exit_signal.language = e->language;
			break;
		default:
			break;
	}
	*event = converted;
}

/* ---- Channel params builder ---- */

/* Parameter strings use only C17 allocation/copy operations. The checked
 * size is shared by allocation and copy, and participates in library fault
 * injection rather than escaping through a host strdup implementation. */
static char *
copy_param_string(const char *text)
{
	size_t len = strlen(text);
	if (len == SIZE_MAX)
		return NULL;
	size_t size = len + 1;
	char *copy = malloc(size);
	if (copy != NULL)
		memcpy(copy, text, size);
	return copy;
}


static int
params_init(struct dssh_chan_params *p, enum dssh_chan_type type)
{
	*p = (struct dssh_chan_params){0};
	p->type = type;
	p->term = copy_param_string("dumb");
	if (p->term == NULL)
		return DSSH_ERROR_ALLOC;
	if (type == DSSH_CHAN_SHELL)
		p->flags |= DSSH_PARAM_HAS_PTY;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_params_init(struct dssh_chan_params *p, enum dssh_chan_type type)
{
	if (p == NULL || (type != DSSH_CHAN_SHELL && type != DSSH_CHAN_EXEC
	    && type != DSSH_CHAN_SUBSYSTEM))
		return DSSH_ERROR_INVALID;
	return params_init(p, type);
}

static void
params_free(struct dssh_chan_params *p)
{
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

DSSH_PUBLIC void
dssh_chan_params_free(struct dssh_chan_params *p)
{
	if (p != NULL)
		params_free(p);
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
dssh_chan_params_set_accept_early_data(struct dssh_chan_params *p, bool enable)
{
	if (p == NULL)
		return DSSH_ERROR_INVALID;
	if (enable)
		p->flags |= DSSH_PARAM_ACCEPT_EARLY_DATA;
	else
		p->flags &= ~(uint32_t)DSSH_PARAM_ACCEPT_EARLY_DATA;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_params_set_term(struct dssh_chan_params *p, const char *term)
{
	if (p == NULL || term == NULL)
		return DSSH_ERROR_INVALID;
	char *replacement = copy_param_string(term);
	if (replacement == NULL)
		return DSSH_ERROR_ALLOC;
	free(p->term);
	p->term = replacement;
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

static int
params_set_mode(struct dssh_chan_params *p, uint8_t opcode, uint32_t value)
{
	if (opcode == 0 || p->mode_count > p->mode_capacity
	    || (p->mode_capacity != 0 && p->modes == NULL))
		return DSSH_ERROR_INVALID;
	size_t match = p->mode_count;
	for (size_t i = 0; i < p->mode_count; i++) {
		if (p->modes[i].opcode == 0)
			return DSSH_ERROR_INVALID;
		if (p->modes[i].opcode == opcode && match == p->mode_count)
			match = i;
	}
	if (match < p->mode_count) {
		p->modes[match].value = value;
		return 0;
	}
	if (p->mode_count >= p->mode_capacity) {
		if (p->mode_capacity > SIZE_MAX / 2)
			return DSSH_ERROR_ALLOC;
		size_t                       new_cap = p->mode_capacity == 0 ? 8 : p->mode_capacity * 2;
		if (new_cap > SIZE_MAX / sizeof(*p->modes))
			return DSSH_ERROR_ALLOC;
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
dssh_chan_params_set_mode(struct dssh_chan_params *p, uint8_t opcode, uint32_t value)
{
	if (p == NULL)
		return DSSH_ERROR_INVALID;
	return params_set_mode(p, opcode, value);
}

DSSH_PUBLIC int
dssh_chan_params_set_command(struct dssh_chan_params *p, const char *command)
{
	if (p == NULL || command == NULL)
		return DSSH_ERROR_INVALID;
	char *replacement = copy_param_string(command);
	if (replacement == NULL)
		return DSSH_ERROR_ALLOC;
	free(p->command);
	p->command = replacement;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_params_set_subsystem(struct dssh_chan_params *p, const char *name)
{
	if (p == NULL || name == NULL)
		return DSSH_ERROR_INVALID;
	char *replacement = copy_param_string(name);
	if (replacement == NULL)
		return DSSH_ERROR_ALLOC;
	free(p->subsystem);
	p->subsystem = replacement;
	return 0;
}

static int
params_add_env(struct dssh_chan_params *p, const uint8_t *name, size_t name_len,
    const uint8_t *value, size_t value_len)
{
	if (p->env_count > p->env_capacity || (p->env_capacity != 0 && p->env == NULL))
		return DSSH_ERROR_INVALID;
	if (name_len == SIZE_MAX || value_len == SIZE_MAX)
		return DSSH_ERROR_TOOLONG;
	char *new_name = malloc(name_len + 1);
	if (new_name == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(new_name, name, name_len);
	new_name[name_len] = 0;
	char *new_value = malloc(value_len + 1);
	if (new_value == NULL) {
		free(new_name);
		return DSSH_ERROR_ALLOC;
	}
	memcpy(new_value, value, value_len);
	new_value[value_len] = 0;
	if (p->env_count == p->env_capacity) {
		if (p->env_capacity > SIZE_MAX / 2) {
			free(new_name);
			free(new_value);
			return DSSH_ERROR_ALLOC;
		}
		size_t                new_cap = p->env_capacity == 0 ? 4 : p->env_capacity * 2;
		if (new_cap > SIZE_MAX / sizeof(*p->env)) {
			free(new_name);
			free(new_value);
			return DSSH_ERROR_ALLOC;
		}
		struct dssh_chan_env *ne      = realloc(p->env, new_cap * sizeof(*ne));

		if (ne == NULL) {
			free(new_name);
			free(new_value);
			return DSSH_ERROR_ALLOC;
		}
		p->env          = ne;
		p->env_capacity = new_cap;
	}
	p->env[p->env_count].name = new_name;
	p->env[p->env_count].value = new_value;
	p->env_count++;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_params_add_env(struct dssh_chan_params *p, const char *name, const char *value)
{
	if (p == NULL || name == NULL || value == NULL)
		return DSSH_ERROR_INVALID;
	const uint8_t *name_bytes = (const uint8_t *)name;
	const uint8_t *value_bytes = (const uint8_t *)value;
	return params_add_env(p, name_bytes, strlen(name), value_bytes, strlen(value));
}

/* Validate exposed builder metadata before any array/string access or
 * channel publication. Object/string extents remain the caller's C contract. */
static int
params_validate(const struct dssh_chan_params *p)
{
	if (p->type != DSSH_CHAN_SHELL && p->type != DSSH_CHAN_EXEC
	    && p->type != DSSH_CHAN_SUBSYSTEM)
		return DSSH_ERROR_INVALID;
	if ((p->flags & ~(uint32_t)(DSSH_PARAM_HAS_PTY | DSSH_PARAM_ACCEPT_EARLY_DATA)) != 0)
		return DSSH_ERROR_INVALID;
	if ((p->flags & DSSH_PARAM_ACCEPT_EARLY_DATA) && p->type == DSSH_CHAN_SUBSYSTEM)
		return DSSH_ERROR_INVALID;
	if ((p->flags & DSSH_PARAM_HAS_PTY) && p->term == NULL)
		return DSSH_ERROR_INVALID;
	if ((p->type == DSSH_CHAN_EXEC && p->command == NULL)
	    || (p->type == DSSH_CHAN_SUBSYSTEM && p->subsystem == NULL))
		return DSSH_ERROR_INVALID;
	if (p->env_count > p->env_capacity || p->mode_count > p->mode_capacity
	    || p->env_capacity > SIZE_MAX / sizeof(*p->env)
	    || p->mode_capacity > SIZE_MAX / sizeof(*p->modes)
	    || (p->env_capacity != 0 && p->env == NULL)
	    || (p->mode_capacity != 0 && p->modes == NULL))
		return DSSH_ERROR_INVALID;
	for (size_t i = 0; i < p->env_count; i++) {
		if (p->env[i].name == NULL || p->env[i].value == NULL)
			return DSSH_ERROR_INVALID;
	}
	for (size_t i = 0; i < p->mode_count; i++) {
		if (p->modes[i].opcode == 0)
			return DSSH_ERROR_INVALID;
	}
	return 0;
}

/* Build a complete library-owned immutable snapshot before registry publication
 * or wire I/O. The validated source stays borrowed and is never modified. */
static int
params_clone(struct dssh_chan_params *out, const struct dssh_chan_params *source, uint32_t window)
{
	struct dssh_chan_params copy = {
		.type = source->type,
		.flags = source->flags,
		.max_window = window,
		.cols = source->cols,
		.rows = source->rows,
		.wpx = source->wpx,
		.hpx = source->hpx,
	};
	if (source->term != NULL) {
		copy.term = copy_param_string(source->term);
		if (copy.term == NULL)
			goto fail;
	}
	if (source->command != NULL) {
		copy.command = copy_param_string(source->command);
		if (copy.command == NULL)
			goto fail;
	}
	if (source->subsystem != NULL) {
		copy.subsystem = copy_param_string(source->subsystem);
		if (copy.subsystem == NULL)
			goto fail;
	}
	if (source->mode_count != 0) {
		size_t bytes = source->mode_count * sizeof(*source->modes);
		copy.modes = malloc(bytes);
		if (copy.modes == NULL)
			goto fail;
		for (size_t i = 0; i < source->mode_count; i++)
			copy.modes[i] = source->modes[i];
		copy.mode_count = source->mode_count;
		copy.mode_capacity = source->mode_count;
	}
	if (source->env_count != 0) {
		size_t bytes = source->env_count * sizeof(*source->env);
		copy.env = malloc(bytes);
		if (copy.env == NULL)
			goto fail;
		copy.env_capacity = source->env_count;
		for (size_t i = 0; i < source->env_count; i++) {
			copy.env[i] = (struct dssh_chan_env){0};
			copy.env_count++;
			copy.env[i].name = copy_param_string(source->env[i].name);
			if (copy.env[i].name == NULL)
				goto fail;
			copy.env[i].value = copy_param_string(source->env[i].value);
			if (copy.env[i].value == NULL)
				goto fail;
		}
	}
	*out = copy;
	return 0;
fail:
	params_free(&copy);
	return DSSH_ERROR_ALLOC;
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
 * Parse helpers for channel request payloads (internal)
 * ================================================================ */

/* Internal PTY request struct -- pointers into wire data */
struct pty_req {
	const char    *term;
	size_t         term_len;
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

	if (!dssh_buffer_has(data_len, pos, 4))
		return DSSH_ERROR_PARSE;
	tlen = DSSH_GET_U32(&data[pos]);
	pos += 4;
#if SIZE_MAX < UINT32_MAX
	if (tlen > SIZE_MAX)
		return DSSH_ERROR_PARSE;
#endif
	size_t term_len = (size_t)tlen;
	if (!dssh_buffer_has(data_len, pos, term_len)
	    || !dssh_buffer_has(data_len, pos + term_len, 16))
		return DSSH_ERROR_PARSE;
	pty->term = (const char *)&data[pos];
	pty->term_len = term_len;
	pos += term_len;
	pty->cols = DSSH_GET_U32(&data[pos]);
	pty->rows = DSSH_GET_U32(&data[pos + 4]);
	pty->wpx  = DSSH_GET_U32(&data[pos + 8]);
	pty->hpx  = DSSH_GET_U32(&data[pos + 12]);
	pos += 16;
	if (!dssh_buffer_has(data_len, pos, 4))
		return DSSH_ERROR_PARSE;
	uint32_t mlen = DSSH_GET_U32(&data[pos]);
	pos += 4;
	if (mlen != data_len - pos)
		return DSSH_ERROR_PARSE;
	pty->modes     = &data[pos];
	pty->modes_len = data_len - pos;
	return 0;
}

static int
parse_env_data(const uint8_t *data, size_t data_len, const uint8_t **name, size_t *name_len, const uint8_t **value,
    size_t *value_len)
{
	size_t   pos = 0;
	uint32_t nlen;

	if (!dssh_buffer_has(data_len, pos, 4))
		return DSSH_ERROR_PARSE;
	nlen = DSSH_GET_U32(&data[pos]);
	pos += 4;
#if SIZE_MAX < UINT32_MAX
	if (nlen > SIZE_MAX)
		return DSSH_ERROR_PARSE;
#endif
	size_t name_size = (size_t)nlen;
	if (!dssh_buffer_has(data_len, pos, name_size))
		return DSSH_ERROR_PARSE;
	*name     = &data[pos];
	*name_len = name_size;
	pos += name_size;

	uint32_t vlen;

	if (!dssh_buffer_has(data_len, pos, 4))
		return DSSH_ERROR_PARSE;
	vlen = DSSH_GET_U32(&data[pos]);
	pos += 4;
#if SIZE_MAX < UINT32_MAX
	if (vlen > SIZE_MAX)
		return DSSH_ERROR_PARSE;
#endif
	size_t value_size = (size_t)vlen;
	if (!dssh_buffer_has(data_len, pos, value_size) || value_size != data_len - pos)
		return DSSH_ERROR_PARSE;
	*value     = &data[pos];
	*value_len = value_size;
	return 0;
}

/* Channel open failure reason codes (RFC 4254 s5.1) */
#define SSH_OPEN_ADMINISTRATIVELY_PROHIBITED UINT32_C(1)
#define SSH_OPEN_CONNECT_FAILED              UINT32_C(2)
#define SSH_OPEN_UNKNOWN_CHANNEL_TYPE        UINT32_C(3)
#define SSH_OPEN_RESOURCE_SHORTAGE           UINT32_C(4)

/* Queue receive-window credit without advertising it yet.  Caller holds
 * buf_mtx.  Only the transport drainer moves this credit to local_window. */
static uint32_t
queue_window_adjust_locked(struct dssh_channel_s *ch, uint32_t bytes)
{
	if (ch->close_sent)
		return 0;
	uint32_t outstanding = window_add(ch->local_window, ch->window_adjust_pending);

	outstanding = window_add(outstanding, ch->window_adjust_inflight);
	uint32_t capacity = UINT32_MAX - outstanding;
	uint32_t queued   = bytes < capacity ? bytes : capacity;

	if (queued > 0) {
		ch->window_adjust_pending += queued;
		atomic_store(&ch->wa_slot.ready, true);
	}
	return queued;
}

/* Protocol records use only channel_mtx. No physical lock spans allocation,
 * I/O or channel-dispatch quiescence. A channel owns one reference from its
 * construction through final retirement; dispatch takes a temporary reference
 * before looking up the application channel. */
static int64_t
channel_id_candidate_locked(struct dssh_session_s *sess)
{
	uint32_t start = sess->next_channel_id;
	for (;;) {
		uint32_t candidate = sess->next_channel_id;
		sess->next_channel_id = candidate == UINT32_MAX ? 0 : candidate + 1;
		bool in_use = false;
		for (struct dssh_channel_id *id = sess->channel_ids; id != NULL; id = id->next) {
			if (id->local_assigned && id->local == candidate) {
				in_use = true;
				break;
			}
		}
		if (!in_use)
			return candidate;
		if (sess->next_channel_id == start)
			return DSSH_ERROR_TOOMANY;
	}
}

static void
channel_id_put_locked(struct dssh_session_s *sess, struct dssh_channel_id *id)
{
	if (id->refs == 0)
		abort();
	id->refs--;
	if (id->refs != 0 || (id->phase != DSSH_CHANNEL_ID_RESERVED
	    && id->phase != DSSH_CHANNEL_ID_FAILED
	    && !(id->close_sent && id->close_received)))
		return;
	struct dssh_channel_id **link = &sess->channel_ids;
	while (*link != id) {
		if (*link == NULL)
			abort();
		link = &(*link)->next;
	}
	*link = id->next;
	if (id->retired) {
		if (sess->retired_channel_ids == 0)
			abort();
		sess->retired_channel_ids--;
	}
	id->next = sess->free_channel_ids;
	sess->free_channel_ids = id;
}

static int
channel_id_put(struct dssh_session_s *sess, struct dssh_channel_id *id)
{
	int ret = mtx_lock(&sess->channel_mtx);
	if (ret != thrd_success) {
		/* Session cleanup owns the still-linked record on acquisition
		 * failure. It contains no pointer to the retired channel. */
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	channel_id_put_locked(sess, id);
	dssh_mtx_unlock(&sess->channel_mtx);
	return 0;
}

static int
channel_id_admit_locked(struct dssh_session_s *sess)
{
	if (sess->terminate)
		return DSSH_ERROR_TERMINATED;
	if (sess->retired_channel_ids >= DSSH_RETIRED_CHANNEL_ID_CAP)
		return DSSH_ERROR_TOOMANY;
	return 0;
}

static int
channel_id_reserve(struct dssh_session_s *sess, enum dssh_channel_id_phase phase,
    uint32_t remote, struct dssh_channel_id **out)
{
	int ret = mtx_lock(&sess->channel_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	ret = channel_id_admit_locked(sess);
	if (ret < 0) {
		dssh_mtx_unlock(&sess->channel_mtx);
		return ret;
	}
	struct dssh_channel_id *id = sess->free_channel_ids;
	if (id != NULL) {
		sess->free_channel_ids = id->next;
	}
	else {
		dssh_mtx_unlock(&sess->channel_mtx);
		id = malloc(sizeof(*id));
		if (id == NULL)
			return DSSH_ERROR_ALLOC;
		ret = mtx_lock(&sess->channel_mtx);
		if (ret != thrd_success) {
			free(id);
			dssh_thrd_check(sess, ret);
			return DSSH_ERROR_TERMINATED;
		}
		/* Another channel can retire while the allocation is in flight. */
		ret = channel_id_admit_locked(sess);
		if (ret < 0) {
			dssh_mtx_unlock(&sess->channel_mtx);
			free(id);
			return ret;
		}
	}
	int64_t candidate = phase == DSSH_CHANNEL_ID_QUEUED ? 0 : channel_id_candidate_locked(sess);
	if (candidate < 0) {
		dssh_mtx_unlock(&sess->channel_mtx);
		free(id);
		/* The only negative result is DSSH_ERROR_TOOMANY. */
		return DSSH_ERROR_TOOMANY;
	}
	/* The candidate is nonnegative and was selected from uint32_t. */
	uint32_t local = (uint32_t)candidate;
	*id = (struct dssh_channel_id){.next = sess->channel_ids, .refs = 1,
	    .local = local, .remote = remote, .phase = phase,
	    .local_assigned = phase != DSSH_CHANNEL_ID_QUEUED};
	sess->channel_ids = id;
	*out = id;
	dssh_mtx_unlock(&sess->channel_mtx);
	return 0;
}

/* Caller holds channel_mtx. Reuse becomes possible after reciprocal CLOSE
 * is ordered, even while the old transport callback is returning. */
static bool
channel_id_peer_owned_locked(struct dssh_session_s *sess, uint32_t remote)
{
	for (struct dssh_channel_id *id = sess->channel_ids; id != NULL; id = id->next) {
		if (id->remote == remote && (id->phase == DSSH_CHANNEL_ID_QUEUED
		    || id->phase == DSSH_CHANNEL_ID_ACCEPTING
		    || (id->phase == DSSH_CHANNEL_ID_CONFIRMED
		        && !(id->close_ordered && id->close_received))))
			return true;
	}
	return false;
}

static int
check_peer_channel_id(struct dssh_session_s *sess, uint32_t remote)
{
	int ret = mtx_lock(&sess->channel_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	bool owned = channel_id_peer_owned_locked(sess, remote);
	dssh_mtx_unlock(&sess->channel_mtx);
	return owned ? DSSH_ERROR_PARSE : 0;
}

/* Order the terminal response while holding the packet engine. If it cannot
 * reach the wire, the required commit terminates before another sender can
 * confirm a reuse of the number. */
static int
send_peer_channel_terminal(struct dssh_session_s *sess, struct dssh_channel_id *id,
    const uint8_t *msg, size_t len)
{
	size_t capacity;
	int ret;
	uint8_t *packet = send_begin(sess, msg[0], &capacity, &ret);
	if (packet == NULL)
		return ret;
	if (len > capacity) {
		send_cancel(sess);
		return DSSH_ERROR_TOOLONG;
	}
	/* Gather normally drains in send_commit(). Preserve the ordering of
	 * already queued channel controls before publishing CLOSE. */
	if (msg[0] == SSH_MSG_CHANNEL_CLOSE && tx_gather_enabled()) {
		ret = drain_tx_slots(sess);
		if (ret < 0) {
			send_cancel(sess);
			return ret;
		}
	}
	ret = mtx_lock(&sess->channel_mtx);
	if (ret != thrd_success) {
		send_cancel(sess);
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	if (msg[0] == SSH_MSG_CHANNEL_CLOSE)
		id->close_ordered = true;
	else
		id->phase = DSSH_CHANNEL_ID_FAILED;
	dssh_mtx_unlock(&sess->channel_mtx);
	memcpy(packet, msg, len);
	return send_commit_required(sess, len);
}

/* The caller owns a channel or dispatch reference. The demux uses a
 * session-owned control slot for a detached channel. The packet engine
 * drains this slot before a later OPEN or confirmation can publish a reused
 * number. Application retirement can wait for the regular send path. */
static int
channel_id_close(struct dssh_session_s *sess, struct dssh_channel_id *id, bool retired_only)
{
	int ret = mtx_lock(&sess->channel_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	if (id->phase != DSSH_CHANNEL_ID_CONFIRMED || id->close_started
	    || (retired_only && !id->retired) || sess->terminate) {
		dssh_mtx_unlock(&sess->channel_mtx);
		return sess->terminate ? DSSH_ERROR_TERMINATED : 0;
	}
	id->close_started = true;
	uint32_t remote = id->remote;
	dssh_mtx_unlock(&sess->channel_mtx);
	uint8_t msg[5];
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_CLOSE;
	DSSH_PUT_U32(remote, msg, &pos);
	bool queue = retired_only && session_on_demux_thread(sess);
	ret = queue ? send_to_slot(sess, &sess->trans.open_fail_slot, msg, pos)
	    : send_peer_channel_terminal(sess, id, msg, pos);
	if (ret < 0) {
		/* CLOSE cannot be retried through the application handle. Keep the
		 * number reserved until session cleanup on mandatory failure. */
		session_set_terminate(sess);
		return ret;
	}
	ret = mtx_lock(&sess->channel_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	id->close_ordered = true;
	id->close_sent = true;
	dssh_mtx_unlock(&sess->channel_mtx);
	return 0;
}

static void
free_channel(struct dssh_channel_s *ch)
{
	if (ch->protocol_id != NULL) {
		int ret = channel_id_close(ch->sess, ch->protocol_id, true);
		if (ret < 0)
			session_set_terminate(ch->sess);
		/* On acquisition failure, session cleanup still owns the record. */
		(void)channel_id_put(ch->sess, ch->protocol_id);
	}
	free(ch);
}

/* Publish protocol intent only after the packet engine and payload capacity
 * have been acquired. A recoverable pre-wire refusal restores the unused
 * reservation if no dispatch observed the attempted publication. */
static int
send_channel_open(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    const uint8_t *msg, size_t len, enum dssh_channel_id_phase phase)
{
	size_t capacity;
	int ret;
	uint8_t *packet = send_begin(sess, msg[0], &capacity, &ret);
	if (packet == NULL)
		return ret;
	if (len > capacity) {
		send_cancel(sess);
		return DSSH_ERROR_TOOLONG;
	}
	ret = mtx_lock(&sess->channel_mtx);
	if (ret != thrd_success) {
		send_cancel(sess);
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	enum dssh_channel_id_phase previous = ch->protocol_id->phase;
	ch->protocol_id->phase = phase;
	ch->protocol_id->remote = ch->remote_id;
	dssh_mtx_unlock(&sess->channel_mtx);
	memcpy(packet, msg, len);
	ret = send_commit(sess, len, NULL);
	if (ret >= 0 || sess->terminate)
		return ret;
	/* A nonterminal commit error is a pre-wire size or key-limit refusal.
	 * A concurrent response that observed the publication prevents rollback. */
	int lock_ret = mtx_lock(&sess->channel_mtx);
	if (lock_ret != thrd_success) {
		dssh_thrd_check(sess, lock_ret);
		return DSSH_ERROR_TERMINATED;
	}
	struct dssh_channel_id *id = ch->protocol_id;
	bool unused = id->refs == 1 && id->phase == phase && !id->close_received;
	if (unused)
		id->phase = previous;
	dssh_mtx_unlock(&sess->channel_mtx);
	if (!unused)
		session_set_terminate(sess);
	return ret;
}

/* ================================================================
 * Low-level wire functions -- internal only.
 * Used by the high-level API; not part of the public interface.
 * ================================================================ */

#define SSH_EXTENDED_DATA_STDERR UINT32_C(1)

/* Success leaves BUF locked and the packet engine owned. A positive return
 * means local CLOSE already owns this channel's send side. */
static int
begin_channel_packet(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    uint8_t type, size_t len, uint8_t **out)
{
	size_t capacity;
	int ret;
	uint8_t *packet = send_begin(sess, type, &capacity, &ret);
	if (packet == NULL)
		return ret;
	if (len > capacity) {
		send_cancel(sess);
		return DSSH_ERROR_TOOLONG;
	}
	ret = mtx_lock(&ch->buf_mtx);
	if (ret != thrd_success) {
		session_set_terminate_no_wake(sess);
		send_cancel(sess);
		session_set_terminate(sess);
		return DSSH_ERROR_TERMINATED;
	}
	if (ch->close_sent) {
		dssh_mtx_unlock(&ch->buf_mtx);
		send_cancel(sess);
		return 1;
	}
	*out = packet;
	return 0;
}

static int
send_channel_packet(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    const uint8_t *msg, size_t len)
{
	uint8_t *packet;
	int ret = begin_channel_packet(sess, ch, msg[0], len, &packet);
	if (ret != 0)
		return ret < 0 ? ret : DSSH_ERROR_TERMINATED;
	dssh_mtx_unlock(&ch->buf_mtx);
	memcpy(packet, msg, len);
	return send_commit(sess, len, NULL);
}

DSSH_TESTABLE int
send_window_adjust(struct dssh_session_s *sess, struct dssh_channel_s *ch, uint32_t bytes)
{
	uint8_t *packet;
	int ret = begin_channel_packet(sess, ch, SSH_MSG_CHANNEL_WINDOW_ADJUST, 9, &packet);
	if (ret != 0)
		return ret < 0 ? ret : DSSH_ERROR_TERMINATED;
	ch->window_adjust_inflight = window_add(ch->window_adjust_inflight, bytes);
	uint32_t remote = ch->remote_id;
	dssh_mtx_unlock(&ch->buf_mtx);

	size_t pos = 0;
	packet[pos++] = SSH_MSG_CHANNEL_WINDOW_ADJUST;
	DSSH_PUT_U32(remote, packet, &pos);
	DSSH_PUT_U32(bytes, packet, &pos);
	ret = send_commit(sess, pos, NULL);

	if (dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;
	if (ret == 0)
		ch->local_window = window_add(ch->local_window, bytes);
	if (bytes <= ch->window_adjust_inflight)
		ch->window_adjust_inflight -= bytes;
	else
		ch->window_adjust_inflight = 0;
	int br = cnd_broadcast(&ch->poll_cnd);
	int ur = dssh_mtx_unlock(&ch->buf_mtx);

	if (ur != thrd_success) {
		dssh_thrd_check_unlock(sess, ur);
		return DSSH_ERROR_TERMINATED;
	}
	if (br != thrd_success) {
		dssh_thrd_check(sess, br);
		return DSSH_ERROR_TERMINATED;
	}
	return ret;
}

/*
 * Send a 0-byte SSH_MSG_CHANNEL_DATA.  Workaround for Cryptlib-based
 * servers (Mystic BBS) that gate further output on receiving any
 * client input.  An empty data message is RFC-conformant (the peer
 * just sees "0 bytes of stdin") but it satisfies the server's
 * "client has spoken at least once" predicate.  Best-effort: failure
 * is non-fatal, the user can still kick it manually.
 */
static int
send_empty_channel_data_kick(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	uint8_t msg[9];
	size_t  pos = 0;

	msg[pos++] = SSH_MSG_CHANNEL_DATA;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);
	DSSH_PUT_U32(0, msg, &pos);
	return send_channel_packet(sess, ch, msg, pos);
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
	return send_channel_packet(sess, ch, msg, pos);
}

DSSH_TESTABLE int
send_eof(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	if (dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;
	bool done = ch->eof_sent || ch->close_sent;
	dssh_mtx_unlock(&ch->buf_mtx);
	if (done)
		return 0;

	uint8_t *packet;
	int ret = begin_channel_packet(sess, ch, SSH_MSG_CHANNEL_EOF, 5, &packet);
	if (ret != 0)
		return ret < 0 ? ret : 0;
	if (ch->eof_sent) {
		dssh_mtx_unlock(&ch->buf_mtx);
		send_cancel(sess);
		return 0;
	}
	uint32_t remote = ch->remote_id;
	dssh_mtx_unlock(&ch->buf_mtx);

	size_t pos = 0;
	packet[pos++] = SSH_MSG_CHANNEL_EOF;
	DSSH_PUT_U32(remote, packet, &pos);
	ret = send_commit_held(sess, pos);
	if (ret == 0) {
		if (dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx)) != thrd_success)
			return tx_unlock_with_result(sess, DSSH_ERROR_TERMINATED);
		ch->eof_sent = true;
		dssh_mtx_unlock(&ch->buf_mtx);
	}
	return tx_unlock_with_result(sess, ret);
}

DSSH_TESTABLE int
conn_close(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	if (dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;
	if (ch->close_sent) {
		if (dssh_thrd_check_unlock(sess, dssh_mtx_unlock(&ch->buf_mtx)) != thrd_success)
			return DSSH_ERROR_TERMINATED;
		return 0;
	}
	ch->close_sent = true;
	ch->open       = false;
	if (dssh_thrd_check_unlock(sess, dssh_mtx_unlock(&ch->buf_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;

	return channel_id_close(sess, ch->protocol_id, false);
}

/* ================================================================
 * Channel table helpers
 * ================================================================ */

DSSH_TESTABLE int
register_channel(struct dssh_session_s *sess, struct dssh_channel_s *ch, bool *registered)
{
	*registered = false;
	bool acquired = false;
	int  ret;

	/* Keep channel creation on one side of a key transition: slot sizing
	 * and registry publication occur during the same TX ownership interval. */
	if (!tx_owner_is_current(sess)) {
		ret = tx_acquire(sess, SSH_MSG_CHANNEL_OPEN);
		if (ret < 0)
			return ret;
		acquired = true;
	}
	ret = alloc_channel_slots(sess, ch);
	if (ret < 0)
		goto register_done;

	struct dssh_channel_s **candidate = NULL;
	size_t                   candidate_capacity = 0;

	for (;;) {
		ret = mtx_lock(&sess->channel_mtx);
		if (ret != thrd_success) {
			dssh_thrd_check(sess, ret);
			ret = DSSH_ERROR_TERMINATED;
			break;
		}
		if (sess->terminate) {
			dssh_thrd_check_unlock(sess, dssh_mtx_unlock(&sess->channel_mtx));
			ret = DSSH_ERROR_TERMINATED;
			break;
		}
		if (sess->channel_count < sess->channel_capacity) {
			sess->channels[sess->channel_count++] = ch;
			if (ch->protocol_id->retired) {
				if (sess->retired_channel_ids == 0)
					abort();
				sess->retired_channel_ids--;
				ch->protocol_id->retired = false;
			}
			*registered = true;
			ret = dssh_mtx_unlock(&sess->channel_mtx);
			if (ret != thrd_success) {
				dssh_thrd_check_unlock(sess, ret);
				ret = DSSH_ERROR_TERMINATED;
			}
			else {
				ret = 0;
			}
			break;
		}

		size_t newcap = 0;
		if (sess->channel_capacity <= SIZE_MAX / 2)
			newcap = sess->channel_capacity ? sess->channel_capacity * 2 : 8;

		if (newcap == 0 || newcap > SIZE_MAX / sizeof(*candidate)) {
			int ur = dssh_mtx_unlock(&sess->channel_mtx);

			if (ur != thrd_success)
				dssh_thrd_check_unlock(sess, ur);
			ret = DSSH_ERROR_ALLOC;
			break;
		}
		int ur = dssh_mtx_unlock(&sess->channel_mtx);

		if (ur != thrd_success) {
			dssh_thrd_check_unlock(sess, ur);
			ret = DSSH_ERROR_TERMINATED;
			break;
		}
		if (newcap > candidate_capacity) {
			struct dssh_channel_s **larger = realloc(candidate, newcap * sizeof(*candidate));

			if (larger == NULL) {
				ret = DSSH_ERROR_ALLOC;
				break;
			}
			candidate = larger;
			candidate_capacity = newcap;
		}

		ret = mtx_lock(&sess->channel_mtx);
		if (ret != thrd_success) {
			dssh_thrd_check(sess, ret);
			ret = DSSH_ERROR_TERMINATED;
			break;
		}
		if (sess->channel_count >= sess->channel_capacity
		    && candidate_capacity > sess->channel_capacity) {
			struct dssh_channel_s **old = sess->channels;

			if (sess->channel_count > 0)
				memcpy(candidate, old, sess->channel_count * sizeof(*candidate));
			sess->channels = candidate;
			sess->channel_capacity = candidate_capacity;
			candidate = old;
			candidate_capacity = 0;
		}
		ur = dssh_mtx_unlock(&sess->channel_mtx);
		if (ur != thrd_success) {
			dssh_thrd_check_unlock(sess, ur);
			ret = DSSH_ERROR_TERMINATED;
			break;
		}
	}
	free(candidate);

register_done:
	if (acquired)
		return tx_unlock_with_result(sess, ret);
	return ret;
}

static int
find_channel(struct dssh_session_s *sess, uint32_t local_id, struct dssh_channel_s **ch_out)
{
	/* Pin under the registry lock, then acquire the channel lock after the
	 * registry lock is released.  dispatch_state keeps the channel alive
	 * after the temporary pin is dropped. */
	*ch_out = NULL;
	int ret = mtx_lock(&sess->channel_mtx);

	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	for (size_t i = 0; i < sess->channel_count; i++) {
		if (sess->channels[i]->local_id == local_id) {
			struct dssh_channel_s *ch = sess->channels[i];
			if (ch->protocol_id->phase == DSSH_CHANNEL_ID_ACCEPTING) {
				dssh_mtx_unlock(&sess->channel_mtx);
				return DSSH_ERROR_PARSE;
			}

			atomic_fetch_add_explicit(&ch->tx_refs, 1, memory_order_acq_rel);
			atomic_store_explicit(&ch->dispatch_state, DSSH_CHANNEL_DISPATCH_ACTIVE,
			    memory_order_release);
			ret = dssh_mtx_unlock(&sess->channel_mtx);
			if (ret != thrd_success) {
				atomic_store_explicit(&ch->dispatch_state, DSSH_CHANNEL_DISPATCH_IDLE,
				    memory_order_release);
				atomic_fetch_sub_explicit(&ch->tx_refs, 1, memory_order_acq_rel);
				dssh_thrd_check_unlock(sess, ret);
				return DSSH_ERROR_TERMINATED;
			}
			ret = mtx_lock(&ch->buf_mtx);
			if (ret != thrd_success) {
				atomic_store_explicit(&ch->dispatch_state, DSSH_CHANNEL_DISPATCH_IDLE,
				    memory_order_release);
				atomic_fetch_sub_explicit(&ch->tx_refs, 1, memory_order_acq_rel);
				dssh_thrd_check(sess, ret);
				return DSSH_ERROR_TERMINATED;
			}
			atomic_fetch_sub_explicit(&ch->tx_refs, 1, memory_order_acq_rel);
			*ch_out = ch;
			return 0; /* returned with buf_mtx held */
		}
	}
	ret = dssh_mtx_unlock(&sess->channel_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check_unlock(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	return DSSH_ERROR_NOMORE;
}

/* Publish IDLE only after successfully releasing the active dispatch lock. */
static int
release_channel_locked(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	(void)sess;
	dssh_mtx_unlock(&ch->buf_mtx);
	atomic_store_explicit(&ch->dispatch_state, DSSH_CHANNEL_DISPATCH_IDLE,
	    memory_order_release);
	return 0;
}

static int
publish_and_release_channel(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	int ret = cnd_broadcast(&ch->poll_cnd);
	int release_ret = release_channel_locked(sess, ch);

	if (release_ret < 0)
		return DSSH_ERROR_TERMINATED;
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	return 0;
}

static void
release_channel_unlocked(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	(void)sess;
	atomic_store_explicit(&ch->dispatch_state, DSSH_CHANNEL_DISPATCH_IDLE,
	    memory_order_release);
}

/* Dispatch remains active while buf_mtx is dropped around callbacks or I/O. */
static int
dispatch_unlock_channel(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	(void)sess;
	dssh_mtx_unlock(&ch->buf_mtx);
	return 0;
}

static int
dispatch_lock_channel(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	if (sess->terminate) {
		release_channel_unlocked(sess, ch);
		return DSSH_ERROR_TERMINATED;
	}
	int ret = mtx_lock(&ch->buf_mtx);

	if (ret == thrd_success)
		return 0;
	atomic_store_explicit(&ch->dispatch_state, DSSH_CHANNEL_DISPATCH_IDLE,
	    memory_order_release);
	dssh_thrd_check(sess, ret);
	return DSSH_ERROR_TERMINATED;
}

/* Called with buf_mtx and ACTIVE dispatch. Success returns 1 with the
 * dispatch still active and no mutex held. Errors release dispatch ownership
 * before reporting termination or propagating a failed mandatory CLOSE. */
static int
finish_event_overflow(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	ch->open = false;
	int broadcast_ret = cnd_broadcast(&ch->poll_cnd);
	if (dispatch_unlock_channel(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	if (broadcast_ret != thrd_success) {
		release_channel_unlocked(sess, ch);
		session_set_terminate(sess);
		return DSSH_ERROR_TERMINATED;
	}
	int ret = conn_close(sess, ch);
	if (ret < 0) {
		release_channel_unlocked(sess, ch);
		return ret;
	}
	return 1;
}

static void
apply_event_state(struct dssh_channel_s *ch, const struct dssh_event_entry *entry)
{
	switch (entry->type) {
		case DSSH_EVENT_EOF:
			ch->eof_received = true;
			break;
		case DSSH_EVENT_CLOSE:
			ch->close_received = true;
			break;
		case DSSH_EVENT_EXIT_STATUS:
			ch->exit_code = entry->u32_a;
			ch->exit_code_received = true;
			break;
		default:
			break;
	}
}

/* Caller owns buf_mtx and ACTIVE. Allocation/free is separated from queue and
 * state publication. Consumer freeze/read can run while allocation is in
 * progress; commit rechecks limits and uses the current ring position. */
static int
publish_polled_event(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    const struct dssh_event_entry *entry)
{
	struct event_queue_reservation r = {0};
	struct event_queue_reservation retired = {0};
	int ret = event_queue_plan(&ch->events, entry, &r);
	if (ret == DSSH_ERROR_TOOMANY) {
		apply_event_state(ch, entry);
		return ret;
	}
	if (ret < 0) {
		if (release_channel_locked(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return ret;
	}
	if (r.capacity != 0 || r.text_capacity != 0) {
		if (dispatch_unlock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		ret = event_queue_allocate(&r);
		if (ret < 0) {
			event_queue_reservation_free(&r);
			release_channel_unlocked(sess, ch);
			return ret;
		}
		if (dispatch_lock_channel(sess, ch) < 0) {
			event_queue_reservation_free(&r);
			return DSSH_ERROR_TERMINATED;
		}
		if (atomic_load(&ch->closing) || sess->terminate) {
			if (dispatch_unlock_channel(sess, ch) < 0) {
				event_queue_reservation_free(&r);
				return DSSH_ERROR_TERMINATED;
			}
			event_queue_reservation_free(&r);
			return 1;
		}
	}
	ret = event_queue_commit(&ch->events, entry, &r, &retired);
	if (ret == 0 || ret == DSSH_ERROR_TOOMANY)
		apply_event_state(ch, entry);
	bool dispose = r.entries != NULL || r.text != NULL
	    || retired.entries != NULL || retired.text != NULL;
	if (dispose) {
		if (dispatch_unlock_channel(sess, ch) < 0) {
			event_queue_reservation_free(&retired);
			event_queue_reservation_free(&r);
			return DSSH_ERROR_TERMINATED;
		}
		event_queue_reservation_free(&retired);
		event_queue_reservation_free(&r);
		if (ret < 0 && ret != DSSH_ERROR_TOOMANY) {
			release_channel_unlocked(sess, ch);
			return ret;
		}
		if (dispatch_lock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		if (atomic_load(&ch->closing)) {
			if (dispatch_unlock_channel(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			return 1;
		}
	}
	if (ret < 0 && ret != DSSH_ERROR_TOOMANY) {
		if (release_channel_locked(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
	}
	return ret;
}

/* Route exactly this event using one coherent callback snapshot. Caller owns
 * the active dispatch and buf_mtx. Return 0/TOOMANY with buf_mtx held, 1 with
 * the dispatch still active but unlocked (closing), or another negative error
 * after releasing the dispatch. Poll-visible state and queue insertion commit
 * under the same buffer mutex. No physical mutex spans another acquisition
 * or a callback. Existing queued/frozen poll events are not callback storage. */
static int
publish_channel_event(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    const struct dssh_event_entry *entry)
{
	if (dispatch_unlock_channel(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	int ret = mtx_lock(&ch->cb_mtx);
	if (ret != thrd_success) {
		release_channel_unlocked(sess, ch);
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	dssh_chan_event_cb cb = ch->event_cb;
	void *cbdata = ch->event_cbdata;
	dssh_mtx_unlock(&ch->cb_mtx);
	if (dispatch_lock_channel(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	if (atomic_load(&ch->closing)) {
		if (dispatch_unlock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return 1;
	}
	if (cb == NULL)
		return publish_polled_event(sess, ch, entry);
	apply_event_state(ch, entry);
	if (dispatch_unlock_channel(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	if (!sess->terminate && !atomic_load(&ch->closing)) {
		struct dssh_chan_event event;
		event_from_entry(entry, &event);
		struct dssh_session_scope scope;
		session_scope_enter(&scope, sess);
		bool previous = in_zc_rx;
		in_zc_rx = true;
		cb(ch, &event, cbdata);
		in_zc_rx = previous;
		session_scope_leave(&scope);
	}
	if (dispatch_lock_channel(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	if (atomic_load(&ch->closing)) {
		if (dispatch_unlock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return 1;
	}
	return 0;
}

static int
channel_unlock_lifecycle(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	(void)sess;
	dssh_mtx_unlock(&ch->buf_mtx);
	return 0;
}

/* Remove a channel from demux lookup and wait for every dispatch that
 * already found it to finish, including intervals where it drops buf_mtx. */
DSSH_TESTABLE int
detach_channel(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	atomic_store(&ch->closing, true);
	int ret = mtx_lock(&sess->channel_mtx);

	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	for (size_t i = 0; i < sess->channel_count; i++) {
		if (sess->channels[i] == ch) {
			sess->channels[i] = sess->channels[--sess->channel_count];
			ch->protocol_id->retired = true;
			if (sess->retired_channel_ids == SIZE_MAX)
				abort();
			sess->retired_channel_ids++;
			break;
		}
	}
	ret = dssh_mtx_unlock(&sess->channel_mtx);
	if (ret != thrd_success)
		dssh_thrd_check_unlock(sess, ret);

	int state;
	do {
		state = atomic_load_explicit(&ch->dispatch_state, memory_order_acquire);
#ifdef DSSH_TESTING
		if (state == DSSH_CHANNEL_DISPATCH_ACTIVE && detach_wait_observer != NULL) {
			atomic_store_explicit(detach_wait_observer, true, memory_order_release);
			detach_wait_observer = NULL;
		}
#endif
		if (state == DSSH_CHANNEL_DISPATCH_ACTIVE
		    || atomic_load_explicit(&ch->tx_refs, memory_order_acquire) != 0)
			thrd_yield();
	} while (state == DSSH_CHANNEL_DISPATCH_ACTIVE
	    || atomic_load_explicit(&ch->tx_refs, memory_order_acquire) != 0);

	return 0;
}

#ifdef DSSH_TESTING
DSSH_TESTABLE int64_t
alloc_channel_id(struct dssh_session_s *sess)
{
	int ret = mtx_lock(&sess->channel_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	int64_t result = channel_id_candidate_locked(sess);
	dssh_mtx_unlock(&sess->channel_mtx);
	return result;
}

#endif

/* ================================================================
 * Channel buffer initialization
 * ================================================================ */

static int
assign_incoming_channel_id(struct dssh_session_s *sess, struct dssh_channel_id *id)
{
	int ret = mtx_lock(&sess->channel_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	ret = channel_id_admit_locked(sess);
	if (ret < 0) {
		dssh_mtx_unlock(&sess->channel_mtx);
		return ret;
	}
	if (id->phase != DSSH_CHANNEL_ID_QUEUED)
		abort();
	int64_t candidate = channel_id_candidate_locked(sess);
	if (candidate < 0) {
		dssh_mtx_unlock(&sess->channel_mtx);
		return DSSH_ERROR_TOOMANY;
	}
	/* The candidate is nonnegative and came from uint32_t. */
	uint32_t local = (uint32_t)candidate;
	id->local = local;
	id->local_assigned = true;
	id->phase = DSSH_CHANNEL_ID_ACCEPTING;
	dssh_mtx_unlock(&sess->channel_mtx);
	return 0;
}

/* Only for a fresh, unpublished channel: native objects are constructed below
 * and atomic initialization must precede every operation/publication. */
static struct dssh_channel_s *
new_channel(struct dssh_session_s *sess, struct dssh_channel_id *id)
{
	bool reserve = id == NULL;
	int ret = reserve ? channel_id_reserve(sess, DSSH_CHANNEL_ID_RESERVED, 0, &id)
	    : assign_incoming_channel_id(sess, id);
	if (ret < 0)
		return NULL;
	struct dssh_channel_s *ch = malloc(sizeof(*ch));
	if (ch == NULL) {
		/* An incoming open still owns its record on constructor failure. */
		if (reserve)
			(void)channel_id_put(sess, id);
		return NULL;
	}
	*ch = (struct dssh_channel_s){0};
	atomic_init(&ch->closing, false);
	atomic_init(&ch->terminate_woken, false);
	atomic_init(&ch->tx_refs, 0);
	atomic_init(&ch->dispatch_state, DSSH_CHANNEL_DISPATCH_IDLE);
	atomic_init(&ch->read_error, 0);
	atomic_init(&ch->read_error_terminal, false);
	atomic_init(&ch->remote_window, 0);
	atomic_init(&ch->setup_complete, false);
	atomic_init(&ch->wa_slot.ready, false);
	atomic_init(&ch->chan_reply_slot.ready, false);
	ch->sess = sess;
	ch->protocol_id = id;
	ch->local_id = id->local;
	return ch;
}

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
		free_tx_slot(&ch->wa_slot);
		free_tx_slot(&ch->chan_reply_slot);
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
#if SIZE_MAX < UINT32_MAX
	if (window_max > SIZE_MAX)
		return DSSH_ERROR_INVALID;
#endif
	size_t buffer_size = (size_t)window_max;
	ch->window_max = window_max;

	int res = bytebuf_init(&ch->buf.stdout_buf, buffer_size);

	if (res < 0)
		return res;
	res = bytebuf_init(&ch->buf.stderr_buf, buffer_size);
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
	ch->setup_payload_len = 0;
	ch->setup_payload_capacity = 0;
	free_tx_slot(&ch->wa_slot);
	free_tx_slot(&ch->chan_reply_slot);

	/* Every caller has completed sync initialization, including registered
	 * setup channels whose terminal type has not been selected yet. */
	cnd_destroy(&ch->poll_cnd);
	mtx_destroy(&ch->cb_mtx);
	mtx_destroy(&ch->buf_mtx);
	ch->chan_type = 0;
}

/* ================================================================
 * Window replenishment -- called after reads drain buffer
 * ================================================================ */

DSSH_TESTABLE int
maybe_replenish_window(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	int ret = mtx_lock(&ch->buf_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}

	/* No point replenishing if peer is done sending */
	if (ch->eof_received || ch->close_received || sess->terminate) {
		return channel_unlock_lifecycle(sess, ch);
	}

	/* Replenish when advertised + queued credit drops below half the target.
         * Cap by free buffer space so we never grant more window
         * than the buffers can absorb. */
	uint32_t add = 0;
	uint32_t outstanding = window_add(ch->local_window, ch->window_adjust_pending);

	outstanding = window_add(outstanding, ch->window_adjust_inflight);

	if (outstanding < ch->window_max / 2) {
		add = ch->window_max - outstanding;
		if (ch->chan_type == DSSH_CHAN_SESSION) {
			size_t buf_free = bytebuf_free_space(&ch->buf.stdout_buf);
			size_t err_free = bytebuf_free_space(&ch->buf.stderr_buf);
			if (err_free < buf_free)
				buf_free = err_free;
			uint32_t buf_cap = buf_free > UINT32_MAX ? UINT32_MAX : (uint32_t)buf_free;
			/* Advertised and queued credit can still arrive.  Reserve
			 * its space before promising any additional bytes. */
			uint32_t available = buf_cap > outstanding ? buf_cap - outstanding : 0;
			if (add > available)
				add = available;
		}
		add = queue_window_adjust_locked(ch, add);
	}

	if (channel_unlock_lifecycle(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;

	if (add > 0)
		/* Schedule the pre-reserved wa_slot without waiting for the engine,
		 * so the WINDOW_ADJUST can follow a bulk-data sender — matters under heavy
		 * bidirectional channel load.  Keeps stream-mode parity
		 * with ZC mode, which has always used wa_slot. */
		return schedule_wa_slot(sess);
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
 * Returns 0 on success, DSSH_ERROR_PARSE for invalid framing/name bytes.
 * Name length/characters follow RFC 4250 section 4.6.1; unknown valid names
 * remain unsupported requests. On success, *rtype borrows payload bytes.
 */
DSSH_TESTABLE int
parse_channel_request(const uint8_t *payload, size_t payload_len, const uint8_t **rtype, uint32_t *rtype_len,
    bool *want_reply, const uint8_t **req_data, size_t *req_data_len)
{
	size_t rpos = 5;

	if (!dssh_buffer_has(payload_len, rpos, 4))
		return DSSH_ERROR_PARSE;
	uint32_t name_wire = DSSH_GET_U32(payload + rpos);
	rpos += 4;
	if (name_wire == 0 || name_wire > 64)
		return DSSH_ERROR_PARSE;
	/* The protocol name bound fits every conforming size_t. */
	size_t name_len = (size_t)name_wire;
	if (!dssh_buffer_has(payload_len, rpos, name_len)
	    || !dssh_buffer_has(payload_len, rpos + name_len, 1))
		return DSSH_ERROR_PARSE;
	for (size_t i = 0; i < name_len; i++) {
		uint8_t byte = payload[rpos + i];
		if (byte <= ' ' || byte >= DSSH_ASCII_DEL || byte == ',')
			return DSSH_ERROR_PARSE;
	}

	*rtype_len = name_wire;
	*rtype = payload + rpos;
	rpos += name_len;

	*want_reply = (payload[rpos] != 0);
	rpos++;

	*req_data     = &payload[rpos];
	*req_data_len = payload_len - rpos;
	return 0;
}

/* Validate mandatory fields and notification reply flags before publication.
 * Unsupported request types remain the generic request path's responsibility. */
static bool
event_request_valid(const uint8_t *type, uint32_t type_len,
    const uint8_t *data, size_t length, bool want_reply)
{
	if (type_len == DSSH_STRLEN(str_break)
	    && memcmp(type, str_break, DSSH_STRLEN(str_break)) == 0)
		return length == 4;
	if (type_len == DSSH_STRLEN(str_exit_status)
	    && memcmp(type, str_exit_status, DSSH_STRLEN(str_exit_status)) == 0)
		return !want_reply && length == 4;
	if (type_len == DSSH_STRLEN(str_window_change)
	    && memcmp(type, str_window_change, DSSH_STRLEN(str_window_change)) == 0)
		return !want_reply && length == 16;
	bool exit_signal = type_len == DSSH_STRLEN(str_exit_signal)
	    && memcmp(type, str_exit_signal, DSSH_STRLEN(str_exit_signal)) == 0;
	bool signal = type_len == DSSH_STRLEN(str_signal)
	    && memcmp(type, str_signal, DSSH_STRLEN(str_signal)) == 0;
	if (!exit_signal && !signal)
		return true;
	if (want_reply)
		return false;
	size_t pos = 0;
	unsigned fields = exit_signal ? 3U : 1U;
	for (unsigned field = 0; field < fields; field++) {
		if (!dssh_buffer_has(length, pos, 4))
			return false;
		uint32_t wire_len = DSSH_GET_U32(data + pos);
		pos += 4;
#if SIZE_MAX < UINT32_MAX
		if (wire_len > SIZE_MAX)
			return false;
#endif
		size_t bytes = (size_t)wire_len;
		if (!dssh_buffer_has(length, pos, bytes))
			return false;
		pos += bytes;
		if (exit_signal && field == 0) {
			if (!dssh_buffer_has(length, pos, 1))
				return false;
			pos++;
		}
	}
	return pos == length;
}

/* Entry: BUF plus ACTIVE. A successful reply returns BUF held; a closing
 * channel returns 1 unlocked. Errors release dispatch ownership as before. */
static int
channel_request_reply(struct dssh_session_s *sess, struct dssh_channel_s *ch, bool success)
{
	if (ch->close_sent)
		return 0;
	uint8_t reply[8];
	size_t  pos = 0;

	reply[pos++] = success ? SSH_MSG_CHANNEL_SUCCESS : SSH_MSG_CHANNEL_FAILURE;
	DSSH_PUT_U32(ch->remote_id, reply, &pos);
	if (dispatch_unlock_channel(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	int send_ret = send_channel_reply(sess, ch, reply, pos);

	if (send_ret < 0) {
		release_channel_unlocked(sess, ch);
		return send_ret;
	}
	if (dispatch_lock_channel(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	if (atomic_load(&ch->closing)) {
		if (dispatch_unlock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return 1;
	}
	return 0;
}

/* Entry/return ownership follows channel_request_reply. BREAK is an action,
 * never an event-queue publication. Reuse the existing channel reply slot. */
static int
handle_channel_break(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    uint32_t length, bool want_reply)
{
	struct dssh_chan_break_request request = {.stdout_pos = ch->buf.stdout_buf.total,
	    .stderr_pos = ch->buf.stderr_buf.total, .length = length, .want_reply = want_reply};
	if (dispatch_unlock_channel(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	int ret = mtx_lock(&ch->cb_mtx);
	if (ret != thrd_success) {
		release_channel_unlocked(sess, ch);
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	dssh_chan_break_cb cb = ch->break_cb;
	void *cbdata = ch->break_cbdata;
	dssh_mtx_unlock(&ch->cb_mtx);
	if (dispatch_lock_channel(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	if (atomic_load(&ch->closing)) {
		if (dispatch_unlock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return 1;
	}
	bool performed = false;
	if (cb != NULL) {
		if (dispatch_unlock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		if (!sess->terminate && !atomic_load(&ch->closing)) {
			struct dssh_session_scope scope;
			session_scope_enter(&scope, sess);
			bool previous = in_zc_rx;
			in_zc_rx = true;
			performed = cb(ch, &request, cbdata);
			in_zc_rx = previous;
			session_scope_leave(&scope);
		}
		if (dispatch_lock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		if (atomic_load(&ch->closing)) {
			if (dispatch_unlock_channel(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			return 1;
		}
	}
	return want_reply ? channel_request_reply(sess, ch, performed) : 0;
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
		if (dispatch_unlock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		int disconnect_ret = transport_disconnect_inner(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
		    "malformed CHANNEL_REQUEST");
		if (disconnect_ret < 0) {
			release_channel_unlocked(sess, ch);
			return disconnect_ret;
		}
		return 1;
	}

	if (ch->chan_type == DSSH_CHAN_SESSION
	    && !event_request_valid(rtype, rtype_len, rdata, rdata_len, want_reply)) {
		if (want_reply)
			return channel_request_reply(sess, ch, false);
		if (release_channel_locked(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return DSSH_ERROR_PARSE;
	}

	int push_ret = 0;

	if (ch->chan_type == DSSH_CHAN_SESSION) {
		if ((rtype_len == DSSH_STRLEN(str_signal))
		    && (memcmp(rtype, str_signal, DSSH_STRLEN(str_signal)) == 0)) {
			if (rdata_len >= 4) {
				uint32_t sname_len = DSSH_GET_U32(rdata);
				/* event_request_valid checked the wire length's size_t bound. */
				size_t sn = (size_t)sname_len;

				if (dssh_buffer_has(rdata_len, 4, sn)) {
					struct dssh_event_entry ev = {.type = DSSH_EVENT_SIGNAL,
					    .stdout_pos                     = ch->buf.stdout_buf.total,
					    .stderr_pos                     = ch->buf.stderr_buf.total};
					ev.text_a = (struct dssh_bytes){.data = sn != 0 ? rdata + 4 : NULL, .len = sn};
					push_ret     = publish_channel_event(sess, ch, &ev);
				}
			}
		}
		else if ((rtype_len == DSSH_STRLEN(str_exit_status))
			 && (memcmp(rtype, str_exit_status, DSSH_STRLEN(str_exit_status)) == 0)) {
			if (rdata_len >= 4) {
				struct dssh_event_entry ev = {.type = DSSH_EVENT_EXIT_STATUS,
				    .stdout_pos                     = ch->buf.stdout_buf.total,
				    .stderr_pos                     = ch->buf.stderr_buf.total,
				    .u32_a                          = DSSH_GET_U32(rdata)};

				push_ret = publish_channel_event(sess, ch, &ev);
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

				push_ret = publish_channel_event(sess, ch, &ev);
			}
		}
		else if ((rtype_len == DSSH_STRLEN(str_exit_signal))
		    && memcmp(rtype, str_exit_signal, DSSH_STRLEN(str_exit_signal)) == 0) {
			struct dssh_event_entry ev = {.type = DSSH_EVENT_EXIT_SIGNAL,
				.stdout_pos = ch->buf.stdout_buf.total,
				.stderr_pos = ch->buf.stderr_buf.total};
			/* The complete string/boolean/string/language layout and
			 * all narrowing bounds were checked before changing state. */
			uint32_t signal_wire = DSSH_GET_U32(rdata);
			size_t signal_len = (size_t)signal_wire;
			ev.text_a = (struct dssh_bytes){.data = signal_len != 0 ? rdata + 4 : NULL, .len = signal_len};
			size_t off = 4 + signal_len;
			ev.flag_a = rdata[off] != 0;
			off++;
			uint32_t error_wire = DSSH_GET_U32(rdata + off);
			size_t error_len = (size_t)error_wire;
			off += 4;
			ev.text_b = (struct dssh_bytes){.data = error_len != 0 ? rdata + off : NULL, .len = error_len};
			off += error_len;
			uint32_t language_wire = DSSH_GET_U32(rdata + off);
			size_t language_len = (size_t)language_wire;
			off += 4;
			ev.language = (struct dssh_bytes){.data = language_len != 0 ? rdata + off : NULL, .len = language_len};
			push_ret = publish_channel_event(sess, ch, &ev);
		}
		else if ((rtype_len == DSSH_STRLEN(str_break))
			 && (memcmp(rtype, str_break, DSSH_STRLEN(str_break)) == 0)) {
			return handle_channel_break(sess, ch, DSSH_GET_U32(rdata), want_reply);
		}
	}

	if (push_ret > 0 || (push_ret < 0 && push_ret != DSSH_ERROR_TOOMANY))
		return push_ret;

	/* Event queue overflow — close the channel only after notification
	 * succeeds; otherwise complete termination before attempting TX. */
	if (push_ret == DSSH_ERROR_TOOMANY)
		return finish_event_overflow(sess, ch);

	/* Unsupported and unperformed requests receive failure only if asked. */
	if (want_reply)
		return channel_request_reply(sess, ch, false);
	return 0;
}

/* Receive either data message using the same framing, window and lifetime
 * rules. Unsupported nonzero extended types are discarded and credited as
 * consumed; application callbacks receive only the two supported streams. */
static int
handle_channel_data(struct dssh_session_s *sess, struct dssh_channel_s *ch, const uint8_t *payload,
    size_t payload_len, bool extended)
{
	size_t fixed = extended ? 13 : 9;
	if (payload_len < fixed)
		goto malformed;
	uint32_t data_type = extended ? DSSH_GET_U32(&payload[5]) : 0;
	if (extended && data_type == 0)
		goto malformed;
	uint32_t wire_len = DSSH_GET_U32(&payload[fixed - 4]);
	if (wire_len > MAX_PACKET_SIZE)
		goto malformed;
#if SIZE_MAX < UINT32_MAX
	if (wire_len > SIZE_MAX)
		goto malformed;
#endif
	size_t length = (size_t)wire_len;
	if (length != payload_len - fixed)
		goto malformed;
	if (ch->eof_received || ch->close_received)
		return 0;

	const uint8_t *data = &payload[fixed];
	bool discard = extended && data_type != SSH_EXTENDED_DATA_STDERR;
	bool bypass = ch->accept_pre_window_data
	    && !atomic_load_explicit(&ch->setup_complete, memory_order_acquire);
	uint32_t delivered = wire_len;
	if (!bypass && delivered > ch->local_window)
		delivered = ch->local_window;
	/* Reserve receive credit before publishing bytes through the unlocked
	 * callback. A stream reader may consume and replenish those bytes before
	 * the callback returns; it must observe their debit at that point. */
	if (!bypass)
		ch->local_window -= delivered;
	uint32_t consumed = delivered;
	if (!discard) {
		dssh_chan_zc_cb cb = ch->zc_cb;
		void *cbdata = ch->zc_cbdata;
		if (dispatch_unlock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		in_zc_rx = true;
		consumed = cb(ch, extended ? 1 : 0, data, delivered, cbdata);
		in_zc_rx = false;
		if (dispatch_lock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		if (atomic_load(&ch->closing)) {
			if (dispatch_unlock_channel(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			return 1;
		}
	}
	if (ch->io_model == DSSH_IO_ZC && !discard) {
		struct dssh_bytebuf *counter = extended ? &ch->buf.stderr_buf : &ch->buf.stdout_buf;
		/* delivered is bounded by the already size-checked wire length. */
		size_t counted = (size_t)delivered;
		if (counted > SIZE_MAX - counter->total)
			counter->total = SIZE_MAX;
		else
			counter->total += counted;
	}
	if (bypass)
		return 0;
	uint32_t credit = consumed < delivered ? consumed : delivered;
	/* Buffered stream bytes are replenished after reads. Discarded data
	 * occupies no application buffer, so refund it for either I/O model. */
	if ((discard || ch->io_model == DSSH_IO_ZC) && credit > 0) {
		credit = queue_window_adjust_locked(ch, credit);
		if (dispatch_unlock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		int ret = credit > 0 ? schedule_wa_slot(sess) : 0;
		if (ret < 0)
			session_set_terminate(sess);
		if (dispatch_lock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		if (atomic_load(&ch->closing)) {
			if (dispatch_unlock_channel(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			return 1;
		}
	}
	return 0;

malformed:
	if (release_channel_locked(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	return DSSH_ERROR_PARSE;
}

/* buf_mtx and ACTIVE are owned on entry. Zero means dispatch is released;
 * one requests a fresh normal dispatch after setup ended during allocation.
 * Every allocation/free runs unlocked; ACTIVE pins storage across the gap. */
static int
publish_setup_packet(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    uint8_t msg_type, const uint8_t *payload, size_t payload_len)
{
	size_t limit = sess->trans.packet_buf_sz;
	if (payload_len > limit) {
		if (release_channel_locked(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return DSSH_ERROR_PARSE;
	}
	uint8_t *retired = NULL;
	if (payload_len > ch->setup_payload_capacity) {
		size_t capacity = ch->setup_payload_capacity;
		if (capacity < 128)
			capacity = 128;
		if (capacity > limit)
			capacity = limit;
		while (capacity < payload_len) {
			if (capacity > limit / 2)
				capacity = limit;
			else
				capacity *= 2;
		}
		if (dispatch_unlock_channel(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		uint8_t *candidate = malloc(capacity);
		if (dispatch_lock_channel(sess, ch) < 0) {
			free(candidate);
			return DSSH_ERROR_TERMINATED;
		}
		if (atomic_load(&ch->closing) || !ch->setup_mode || sess->terminate) {
			bool redispatch = !atomic_load(&ch->closing) && !sess->terminate;
			int release_ret = release_channel_locked(sess, ch);
			free(candidate);
			if (release_ret < 0)
				return DSSH_ERROR_TERMINATED;
			return redispatch ? 1 : 0;
		}
		if (candidate == NULL) {
			ch->setup_error = true;
			return publish_and_release_channel(sess, ch);
		}
		retired = ch->setup_payload;
		ch->setup_payload = candidate;
		ch->setup_payload_capacity = capacity;
	}
	memcpy(ch->setup_payload, payload, payload_len);
	ch->setup_payload_len = payload_len;
	ch->setup_msg_type = msg_type;
	ch->setup_ready = true;
	int ret = publish_and_release_channel(sess, ch);
	if (retired != NULL)
		free(retired);
	return ret;
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

	/* Reject fixed-layout controls before registry lookup, setup-mailbox
	 * publication, state changes or application event delivery. */
	switch (msg_type) {
		case SSH_MSG_CHANNEL_WINDOW_ADJUST:
			if (payload_len != 9)
				return DSSH_ERROR_PARSE;
			break;
		case SSH_MSG_CHANNEL_EOF:
		case SSH_MSG_CHANNEL_CLOSE:
		case SSH_MSG_CHANNEL_SUCCESS:
		case SSH_MSG_CHANNEL_FAILURE:
			if (payload_len != 5)
				return DSSH_ERROR_PARSE;
			break;
		default:
			break;
	}

	uint32_t local_id = DSSH_GET_U32(&payload[1]);

	struct dssh_channel_s *ch;
	int find_ret = find_channel(sess, local_id, &ch);

	if (find_ret == DSSH_ERROR_NOMORE)
		return 0; /* late message for closed channel */
	if (find_ret < 0)
		return find_ret;

			  /* find_channel() returns with buf_mtx held */

	/* Framing and protocol phase have already been checked. */
	if (msg_type == SSH_MSG_CHANNEL_OPEN_FAILURE) {
		if (!ch->open_pending || ch->open || ch->close_sent || ch->close_received) {
			if (release_channel_locked(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			return DSSH_ERROR_PARSE;
		}
		ch->open_pending = false;
		ch->close_received = true;
		return publish_and_release_channel(sess, ch);
	}
	if (ch->open_pending) {
		if (release_channel_locked(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return DSSH_ERROR_PARSE;
	}

	/*
         * Setup mode: during session_accept_channel, deliver messages
         * via the mailbox instead of processing them normally.
         */
	if (ch->setup_mode) {
		/* Wait for the accept loop to consume the previous message */
		while (ch->setup_mode && ch->setup_ready && !sess->terminate) {
			int wait_ret = cnd_wait(&ch->poll_cnd, &ch->buf_mtx);

			if (wait_ret != thrd_success) {
				int release_ret = release_channel_locked(sess, ch);

				if (release_ret >= 0)
					dssh_thrd_check(sess, wait_ret);
				return DSSH_ERROR_TERMINATED;
			}
		}
		if (sess->terminate) {
			if (release_channel_locked(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			return DSSH_ERROR_TERMINATED;
		}
		/* setup_ready remains set until the acceptor finishes processing
		 * the mailbox entry.  If that entry completed setup, dispatch this
		 * packet normally now that the channel is fully initialized. */
		if (!ch->setup_mode) {
			if (release_channel_locked(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			if (sess->terminate)
				return DSSH_ERROR_TERMINATED;
			return demux_dispatch(sess, msg_type, payload, payload_len);
		}
		int setup_ret = publish_setup_packet(sess, ch, msg_type, payload, payload_len);
		if (setup_ret == 1)
			return demux_dispatch(sess, msg_type, payload, payload_len);
		return setup_ret;
	}

	if (ch->chan_type == 0) {
		if (release_channel_locked(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return 0;
	}

	/* A WINDOW_ADJUST sender calls the consumer without buf_mtx held.
	 * The peer can receive that adjustment and race data back before
	 * the callback returns.  Defer that data until the sender publishes
	 * the corresponding local_window increase. */
	if (msg_type == SSH_MSG_CHANNEL_DATA || msg_type == SSH_MSG_CHANNEL_EXTENDED_DATA) {
		while (ch->window_adjust_inflight > 0 && !sess->terminate) {
			int wait_ret = cnd_wait(&ch->poll_cnd, &ch->buf_mtx);

			if (wait_ret != thrd_success) {
				int release_ret = release_channel_locked(sess, ch);

				if (release_ret >= 0)
					dssh_thrd_check(sess, wait_ret);
				return DSSH_ERROR_TERMINATED;
			}
		}
		if (sess->terminate) {
			if (release_channel_locked(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			return DSSH_ERROR_TERMINATED;
		}
	}

	int handle_ret;
	switch (msg_type) {
		case SSH_MSG_CHANNEL_DATA:
			handle_ret = handle_channel_data(sess, ch, payload, payload_len, false);
			if (handle_ret < 0)
				return handle_ret;
			if (handle_ret > 0) {
				release_channel_unlocked(sess, ch);
				return 0;
			}
			break;

		case SSH_MSG_CHANNEL_EXTENDED_DATA:
			handle_ret = handle_channel_data(sess, ch, payload, payload_len, true);
			if (handle_ret < 0)
				return handle_ret;
			if (handle_ret > 0) {
				release_channel_unlocked(sess, ch);
				return 0;
			}
			break;

		case SSH_MSG_CHANNEL_WINDOW_ADJUST:
			window_atomic_add(&ch->remote_window, DSSH_GET_U32(&payload[5]));
			break;

		case SSH_MSG_CHANNEL_EOF:
			{
				struct dssh_event_entry ev  = {.type = DSSH_EVENT_EOF,
				     .stdout_pos                     = ch->buf.stdout_buf.total,
				     .stderr_pos                     = ch->buf.stderr_buf.total};
				int                     ret = publish_channel_event(sess, ch, &ev);

				if (ret > 0) {
					release_channel_unlocked(sess, ch);
					return 0;
				}
				if (ret < 0 && ret != DSSH_ERROR_TOOMANY)
					return ret;
				if (ret == DSSH_ERROR_TOOMANY) {
					ret = finish_event_overflow(sess, ch);
					if (ret < 0)
						return ret;
					release_channel_unlocked(sess, ch);
					return 0;
				}
			}
			break;

		case SSH_MSG_CHANNEL_CLOSE:
			{
				struct dssh_event_entry ev  = {.type = DSSH_EVENT_CLOSE,
				     .stdout_pos                     = ch->buf.stdout_buf.total,
				     .stderr_pos                     = ch->buf.stderr_buf.total};
				int                     ret = publish_channel_event(sess, ch, &ev);

				if (ret > 0) {
					release_channel_unlocked(sess, ch);
					return 0;
				}
				if (ret < 0 && ret != DSSH_ERROR_TOOMANY)
					return ret;
				if (ret == DSSH_ERROR_TOOMANY) {
					ret = finish_event_overflow(sess, ch);
					if (ret < 0)
						return ret;
					release_channel_unlocked(sess, ch);
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
			handle_ret = handle_channel_request(sess, ch, payload, payload_len);
			if (handle_ret < 0)
				return handle_ret;
			if (handle_ret > 0) {
				release_channel_unlocked(sess, ch);
				return 0;
			}
			break;

		case SSH_MSG_CHANNEL_SUCCESS:
		case SSH_MSG_CHANNEL_FAILURE:
			if (ch->request_pending) {
				ch->request_success   = (msg_type == SSH_MSG_CHANNEL_SUCCESS);
				ch->request_responded = true;
			}
			break;
	}

	if (publish_and_release_channel(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	return 0;
}

#ifdef DSSH_TESTING
static atomic_flag test_dispatch_gate = ATOMIC_FLAG_INIT;

static void
test_dispatch_enter(void)
{
	while (atomic_flag_test_and_set_explicit(&test_dispatch_gate,
		memory_order_acquire))
		thrd_yield();
}

static void
test_dispatch_leave(void)
{
	atomic_flag_clear_explicit(&test_dispatch_gate, memory_order_release);
}

#endif

static bool
channel_expects_confirmation(struct dssh_channel_s *ch)
{
	return ch->open_pending && !ch->open && !ch->close_sent && !ch->close_received;
}

/* Publish protocol ownership before the application can send to the new
 * channel. The dispatch pin keeps the handle alive between the two locks. */
static int
demux_open_confirmation(struct dssh_session_s *sess, struct dssh_channel_id *id,
    const uint8_t *payload, bool attached)
{
	struct dssh_channel_s *ch = NULL;
	if (attached) {
		int ret = find_channel(sess, id->local, &ch);
		if (ret < 0 && ret != DSSH_ERROR_NOMORE)
			return ret;
		if (ch != NULL) {
			if (!channel_expects_confirmation(ch)) {
				release_channel_locked(sess, ch);
				return DSSH_ERROR_PARSE;
			}
			dssh_mtx_unlock(&ch->buf_mtx);
		}
	}

	uint32_t remote = DSSH_GET_U32(&payload[5]);
	int ret = mtx_lock(&sess->channel_mtx);
	if (ret != thrd_success) {
		if (ch != NULL)
			release_channel_unlocked(sess, ch);
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	id->remote = remote;
	id->phase = DSSH_CHANNEL_ID_CONFIRMED;
	dssh_mtx_unlock(&sess->channel_mtx);
	if (ch == NULL)
		return 0;

	if (dispatch_lock_channel(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	if (atomic_load(&ch->closing))
		return release_channel_locked(sess, ch);
	if (!channel_expects_confirmation(ch)) {
		release_channel_locked(sess, ch);
		session_set_terminate(sess);
		return DSSH_ERROR_TERMINATED;
	}
	ch->remote_id = remote;
	atomic_store_explicit(&ch->remote_window, DSSH_GET_U32(&payload[9]), memory_order_release);
	ch->remote_max_packet = DSSH_GET_U32(&payload[13]);
	ch->open_pending      = false;
	ch->open              = true;
	return publish_and_release_channel(sess, ch);
}

/* Unadmitted opens use the preallocated reply slot. For a claimed open the
 * application orders the rejection before releasing the claim. */
static int
send_open_failure(struct dssh_session_s *sess, uint32_t peer_channel, uint32_t reason,
    struct dssh_channel_id *id)
{
	uint8_t fail[17];
	size_t pos = 0;

	fail[pos++] = SSH_MSG_CHANNEL_OPEN_FAILURE;
	DSSH_PUT_U32(peer_channel, fail, &pos);
	DSSH_PUT_U32(reason, fail, &pos);
	DSSH_PUT_U32(0, fail, &pos); /* description */
	DSSH_PUT_U32(0, fail, &pos); /* language */
	if (id == NULL)
		return send_to_slot(sess, &sess->trans.open_fail_slot, fail, pos);
	int ret = send_peer_channel_terminal(sess, id, fail, pos);
	int put_ret = channel_id_put(sess, id);
	return ret < 0 ? ret : put_ret;
}

/* Handle incoming CHANNEL_OPEN -- only server-side session channels exist. */
static int
demux_channel_open(struct dssh_session_s *sess, uint8_t *payload, size_t payload_len)
{
	size_t pos = 1;
	if (!dssh_buffer_has(payload_len, pos, 4)) {
		return transport_disconnect_result(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
		    "malformed CHANNEL_OPEN", DSSH_ERROR_TERMINATED);
	}
	uint32_t wire_type_len = DSSH_GET_U32(&payload[pos]);
	pos += 4;
#if SIZE_MAX < UINT32_MAX
	if (wire_type_len > SIZE_MAX) {
		return transport_disconnect_result(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
		    "malformed CHANNEL_OPEN", DSSH_ERROR_TERMINATED);
	}
#endif
	size_t type_len = (size_t)wire_type_len;
	if (!dssh_buffer_has(payload_len, pos, type_len)) {
		return transport_disconnect_result(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
		    "malformed CHANNEL_OPEN", DSSH_ERROR_TERMINATED);
	}
	const uint8_t *type = &payload[pos];
	pos += type_len;
	if (!dssh_buffer_has(payload_len, pos, 12)) {
		int ret = 0;
		if (dssh_buffer_has(payload_len, pos, 4)) {
			uint32_t peer_channel = DSSH_GET_U32(&payload[pos]);
			ret = check_peer_channel_id(sess, peer_channel);
			if (ret == DSSH_ERROR_TERMINATED)
				return ret;
			if (ret == 0)
				ret = send_open_failure(sess, peer_channel,
				    SSH_OPEN_ADMINISTRATIVELY_PROHIBITED, NULL);
		}
		int disconnect_ret = transport_disconnect_result(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
		    "malformed CHANNEL_OPEN", DSSH_ERROR_TERMINATED);
		return ret < 0 ? ret : disconnect_ret;
	}
	uint32_t peer_channel = DSSH_GET_U32(&payload[pos]);
	pos += 4;
	uint32_t peer_window = DSSH_GET_U32(&payload[pos]);
	pos += 4;
	uint32_t peer_max_packet = DSSH_GET_U32(&payload[pos]);
	pos += 4;
	int ret = check_peer_channel_id(sess, peer_channel);
	if (ret < 0)
		return ret;

	bool session_type = type_len == DSSH_STRLEN(str_session)
	    && memcmp(type, str_session, DSSH_STRLEN(str_session)) == 0;
	bool forbidden_type = session_type
	    || (type_len == DSSH_STRLEN(str_x11) && memcmp(type, str_x11, type_len) == 0)
	    || (type_len == DSSH_STRLEN(str_forwarded_tcpip)
	        && memcmp(type, str_forwarded_tcpip, type_len) == 0)
	    || (type_len == DSSH_STRLEN(str_direct_tcpip)
	        && memcmp(type, str_direct_tcpip, type_len) == 0);
	if (!session_type || sess->trans.client) {
		uint32_t reason = forbidden_type ? SSH_OPEN_ADMINISTRATIVELY_PROHIBITED
		    : SSH_OPEN_UNKNOWN_CHANNEL_TYPE;
		return send_open_failure(sess, peer_channel, reason, NULL);
	}
	/* Session opens have no channel-specific fields. */
	if (pos != payload_len) {
		ret = send_open_failure(sess, peer_channel, SSH_OPEN_ADMINISTRATIVELY_PROHIBITED, NULL);
		int disconnect_ret = transport_disconnect_result(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
		    "malformed session CHANNEL_OPEN", DSSH_ERROR_TERMINATED);
		return ret < 0 ? ret : disconnect_ret;
	}

	/* The sole demux producer can check the FIFO before reserving an ID.
	 * Acceptors can only remove entries until this producer publishes. */
	ret = mtx_lock(&sess->accept_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	bool full = sess->accept_queue.count >= DSSH_ACCEPT_QUEUE_CAP;
	dssh_mtx_unlock(&sess->accept_mtx);
	if (full)
		return send_open_failure(sess, peer_channel, SSH_OPEN_RESOURCE_SHORTAGE, NULL);

	struct dssh_channel_id *id;
	ret = channel_id_reserve(sess, DSSH_CHANNEL_ID_QUEUED, peer_channel, &id);
	if (ret == DSSH_ERROR_TOOMANY || ret == DSSH_ERROR_ALLOC)
		return send_open_failure(sess, peer_channel, SSH_OPEN_RESOURCE_SHORTAGE, NULL);
	if (ret < 0)
		return ret;
	ret = mtx_lock(&sess->accept_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	if (sess->terminate) {
		dssh_mtx_unlock(&sess->accept_mtx);
		return DSSH_ERROR_TERMINATED;
	}
	size_t tail = (sess->accept_queue.head + sess->accept_queue.count) % DSSH_ACCEPT_QUEUE_CAP;
	ret = acceptqueue_push(&sess->accept_queue, peer_channel, peer_window,
	    peer_max_packet, type, type_len);
	if (ret != 0)
		abort();
	sess->accept_queue.entries[tail].protocol_id = id;
	int signal_ret = cnd_signal(&sess->accept_cnd);
	dssh_mtx_unlock(&sess->accept_mtx);
	if (signal_ret != thrd_success) {
		dssh_thrd_check(sess, signal_ret);
		return DSSH_ERROR_TERMINATED;
	}
	return 0;
}

/* Complete framing is checked before a late reply can release an ID. */
static bool
channel_open_failure_valid(const uint8_t *payload, size_t payload_len)
{
	size_t pos = 9;
	if (payload_len < pos || DSSH_GET_U32(&payload[5]) == 0)
		return false;
	for (int field = 0; field < 2; field++) {
		if (!dssh_buffer_has(payload_len, pos, 4))
			return false;
		uint32_t len = DSSH_GET_U32(&payload[pos]);
		pos += 4;
#if SIZE_MAX < UINT32_MAX
		if (len > SIZE_MAX)
			return false;
#endif
		size_t field_len = (size_t)len;
		if (!dssh_buffer_has(payload_len, pos, field_len))
			return false;
		pos += field_len;
	}
	return pos == payload_len;
}

/* Pin the protocol owner before looking up the application handle. Its ID
 * cannot be recycled in the interval between detachment and this dispatch.
 * There is one wire dispatcher; the test dispatcher has the same serialization. */
static int
channel_id_dispatch(struct dssh_session_s *sess, uint8_t msg_type,
    uint8_t *payload, size_t payload_len)
{
	if ((msg_type == SSH_MSG_CHANNEL_OPEN_CONFIRMATION && payload_len != 17)
	    || (msg_type == SSH_MSG_CHANNEL_CLOSE && payload_len != 5)
	    || (msg_type == SSH_MSG_CHANNEL_OPEN_FAILURE
	        && !channel_open_failure_valid(payload, payload_len)))
		return DSSH_ERROR_PARSE;
	uint32_t local = DSSH_GET_U32(&payload[1]);
	int ret = mtx_lock(&sess->channel_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	struct dssh_channel_id *id = sess->channel_ids;
	while (id != NULL && (!id->local_assigned || id->local != local))
		id = id->next;
	if (id == NULL) {
		dssh_mtx_unlock(&sess->channel_mtx);
		return 0;
	}
	bool valid = msg_type == SSH_MSG_CHANNEL_CLOSE
	    ? id->phase == DSSH_CHANNEL_ID_CONFIRMED
	    : id->phase == DSSH_CHANNEL_ID_OPENING;
	if (!valid || id->refs == SIZE_MAX) {
		dssh_mtx_unlock(&sess->channel_mtx);
		return valid ? DSSH_ERROR_TOOMANY : DSSH_ERROR_PARSE;
	}
	if (msg_type == SSH_MSG_CHANNEL_OPEN_CONFIRMATION
	    && channel_id_peer_owned_locked(sess, DSSH_GET_U32(&payload[5]))) {
		dssh_mtx_unlock(&sess->channel_mtx);
		return DSSH_ERROR_PARSE;
	}
	id->refs++;
	bool attached = !id->retired;
	dssh_mtx_unlock(&sess->channel_mtx);
	if (msg_type == SSH_MSG_CHANNEL_OPEN_CONFIRMATION) {
		ret = demux_open_confirmation(sess, id, payload, attached);
	}
	else {
		ret = attached ? demux_dispatch(sess, msg_type, payload, payload_len) : 0;
		if (ret == 0) {
			int lock_ret = mtx_lock(&sess->channel_mtx);
			if (lock_ret != thrd_success) {
				dssh_thrd_check(sess, lock_ret);
				ret = DSSH_ERROR_TERMINATED;
			}
			else {
				if (msg_type == SSH_MSG_CHANNEL_OPEN_FAILURE)
					id->phase = DSSH_CHANNEL_ID_FAILED;
				else
					id->close_received = true;
				dssh_mtx_unlock(&sess->channel_mtx);
			}
		}
	}
	if (ret == 0)
		ret = channel_id_close(sess, id, true);
	int put_ret = channel_id_put(sess, id);
	return ret < 0 ? ret : put_ret;
}

static int
dispatch_connection_packet(struct dssh_session_s *sess, uint8_t msg_type,
    uint8_t *payload, size_t payload_len)
{
	/* RFC 4252 s5.1: the completed server auth protocol no longer receives
	 * requests. Keep later service traffic on the ordinary dispatch path. */
	if (!sess->trans.client && sess->auth_complete
	    && msg_type == SSH_MSG_USERAUTH_REQUEST) {
		dssh_cleanse(payload, payload_len);
		return 0;
	}
	if (msg_type == SSH_MSG_CHANNEL_OPEN)
		return demux_channel_open(sess, payload, payload_len);
	if (msg_type == SSH_MSG_CHANNEL_OPEN_CONFIRMATION
	    || msg_type == SSH_MSG_CHANNEL_OPEN_FAILURE || msg_type == SSH_MSG_CHANNEL_CLOSE)
		return channel_id_dispatch(sess, msg_type, payload, payload_len);
	if (msg_type >= SSH_MSG_CHANNEL_WINDOW_ADJUST && msg_type <= SSH_MSG_CHANNEL_FAILURE)
		return demux_dispatch(sess, msg_type, payload, payload_len);
	return send_unimplemented(sess, sess->trans.last_rx_seq);
}

#ifdef DSSH_TESTING
DSSH_TESTABLE int
dssh_test_demux_dispatch(struct dssh_session_s *sess, uint8_t msg_type,
    uint8_t *payload, size_t payload_len)
{
	test_dispatch_enter();
	int ret = dispatch_connection_packet(sess, msg_type, payload, payload_len);
	test_dispatch_leave();
	return ret;
}
#endif

static _Thread_local struct dssh_session_s *demux_session;

DSSH_PRIVATE bool
session_on_demux_thread(struct dssh_session_s *sess)
{
	return demux_session == sess;
}

DSSH_TESTABLE int
demux_thread_func(void *arg)
{
	struct dssh_session_s *sess = arg;
	uint8_t                msg_type;
	uint8_t               *payload;
	size_t                 payload_len;

	demux_session = sess;
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
			DSSH_LOGF(sess, DSSH_LOG_ERROR, res, "Connection demultiplexer receive failed: %s",
			    dssh_strerror(res));
			session_set_terminate(sess);
			dssh_log_termination(sess, res);
			break;
		}

		int dres = 0;

#ifdef DSSH_TESTING
		test_dispatch_enter();
#endif
		dres = dispatch_connection_packet(sess, msg_type, payload, payload_len);
#ifdef DSSH_TESTING
		test_dispatch_leave();
#endif

		/* Parse errors from malformed peer messages are
		 * non-fatal -- skip the packet.  I/O and serialize
		 * errors mean the connection is broken. */
		if (dres == DSSH_ERROR_PARSE) {
			DSSH_LOGF(sess, DSSH_LOG_WARNING, dres, "Malformed connection-layer message ignored");
		}
		else if (dres < 0) {
			DSSH_LOGF(sess, DSSH_LOG_ERROR, dres, "Connection demultiplexer dispatch failed: %s",
			    dssh_strerror(dres));
			session_set_terminate(sess);
			dssh_log_termination(sess, dres);
			break;
		}
		/* All other message types (GLOBAL_REQUEST, KEXINIT, etc.)
                 * are already handled by recv_packet transparently */
	}

	/* Complete deferred notification and every transport/connection wake
	 * through the same one-shot termination path used by API callers. */
	session_set_terminate(sess);

	sess->demux_running = false;
	demux_session = NULL;
	return 0;
}

DSSH_PUBLIC int
dssh_session_start(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (connection_api_forbidden(sess) || session_scope_active(sess))
		return DSSH_ERROR_INVALID;
	if (sess->conn_initialized) {
		DSSH_LOGF(sess, DSSH_LOG_ERROR, DSSH_ERROR_INIT, "Session connection layer is already started");
		return DSSH_ERROR_INIT;
	}
	if (sess->terminate)
		return DSSH_ERROR_TERMINATED;
	if (!sess->handshake_complete || !sess->auth_complete)
		return DSSH_ERROR_INVALID;

	if (mtx_init(&sess->channel_mtx, mtx_plain) != thrd_success) {
		DSSH_LOGF(sess, DSSH_LOG_ERROR, DSSH_ERROR_INIT, "Failed to initialize channel mutex");
		return DSSH_ERROR_INIT;
	}
	if (mtx_init(&sess->accept_mtx, mtx_plain) != thrd_success) {
		mtx_destroy(&sess->channel_mtx);
		DSSH_LOGF(sess, DSSH_LOG_ERROR, DSSH_ERROR_INIT, "Failed to initialize accept mutex");
		return DSSH_ERROR_INIT;
	}
	if (cnd_init(&sess->accept_cnd) != thrd_success) {
		mtx_destroy(&sess->accept_mtx);
		mtx_destroy(&sess->channel_mtx);
		DSSH_LOGF(sess, DSSH_LOG_ERROR, DSSH_ERROR_INIT, "Failed to initialize accept condition variable");
		return DSSH_ERROR_INIT;
	}

	sess->channels         = NULL;
	sess->channel_count    = 0;
	sess->channel_capacity = 0;
	sess->next_channel_id  = 0;
	sess->channel_ids = NULL;
	sess->free_channel_ids = NULL;
	sess->retired_channel_ids = 0;
	acceptqueue_init(&sess->accept_queue);
	sess->conn_initialized = true;

	sess->demux_running = true;
	if (thrd_create(&sess->demux_thread, demux_thread_func, sess) != thrd_success) {
		sess->demux_running = false;
		sess->conn_initialized = false;
		cnd_destroy(&sess->accept_cnd);
		mtx_destroy(&sess->accept_mtx);
		mtx_destroy(&sess->channel_mtx);
		DSSH_LOGF(sess, DSSH_LOG_ERROR, DSSH_ERROR_INIT, "Failed to create connection demultiplexer thread");
		return DSSH_ERROR_INIT;
	}

	DSSH_LOGF(sess, DSSH_LOG_DEBUG, DSSH_ERROR_NONE, "Connection demultiplexer started");
	return 0;
}

DSSH_PRIVATE void
session_stop_inner(struct dssh_session_s *sess)
{
	if (!sess->conn_initialized)
		return;

	/* demux_running describes the worker's current state, not whether
	 * demux_thread is joinable.  Always join a successfully-created
	 * worker before releasing any session-owned state, including when
	 * the worker has already stopped itself after an I/O error. */
	session_set_terminate(sess);

	int ret = thrd_join(sess->demux_thread, NULL);

	/* Under the serialized lifecycle preconditions this is a valid joinable
	 * thread. Failure leaves its ownership/completion indeterminate; neither
	 * callback diagnostics, cleanup nor caller retry is a recovery contract. */
	if (ret != thrd_success)
		abort();

	/* Clean up registered channels */
	if (sess->channels != NULL) {
		/* Channels in the array are never NULL; registration
                 * always stores a valid pointer. */
		for (size_t i = 0; i < sess->channel_count; i++) {
			struct dssh_channel_s *ch = sess->channels[i];

			if (ch != NULL) {
				params_free(&ch->params);
				event_queue_free(&ch->events);
				cleanup_channel_buffers(ch);
				free(ch);
			}
		}
		free(sess->channels);
		sess->channels         = NULL;
		sess->channel_count    = 0;
		sess->channel_capacity = 0;
	}

	while (sess->channel_ids != NULL) {
		struct dssh_channel_id *id = sess->channel_ids;
		sess->channel_ids = id->next;
		free(id);
	}
	while (sess->free_channel_ids != NULL) {
		struct dssh_channel_id *id = sess->free_channel_ids;
		sess->free_channel_ids = id->next;
		free(id);
	}

	sess->retired_channel_ids = 0;

	acceptqueue_free(&sess->accept_queue);
	cnd_destroy(&sess->accept_cnd);
	mtx_destroy(&sess->accept_mtx);
	mtx_destroy(&sess->channel_mtx);
	sess->conn_initialized = false;
}

DSSH_PUBLIC void
dssh_session_stop(struct dssh_session_s *sess)
{
	if (sess == NULL)
		return;
	if (dssh_io_callback_active(sess))
		return;
	if (!session_lifecycle_begin(sess))
		return;
	session_stop_inner(sess);
	atomic_store_explicit(&sess->lifecycle_busy, false, memory_order_release);
}

/* ================================================================
 * Session accept -- wait for incoming CHANNEL_OPEN
 * ================================================================ */

DSSH_TESTABLE int
accept_incoming(struct dssh_session_s *sess, struct dssh_incoming_open *inc, int timeout_ms)
{
	struct timespec ts;
	int ret = 0;
	if (timeout_ms > 0) {
		ret = dssh_deadline_from_ms(&ts, timeout_ms);
		if (ret < 0) {
			return ret;
		}
	}
	ret = mtx_lock(&sess->accept_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	while (sess->accept_queue.count == 0 && !sess->terminate && sess->demux_running) {
		if (timeout_ms == 0) {
			if (dssh_thrd_check_unlock(sess, dssh_mtx_unlock(&sess->accept_mtx)) != thrd_success)
				return DSSH_ERROR_TERMINATED;
			return DSSH_ERROR_NOMORE;
		}
		ret = timeout_ms < 0 ? cnd_wait(&sess->accept_cnd, &sess->accept_mtx)
		    : cnd_timedwait(&sess->accept_cnd, &sess->accept_mtx, &ts);
		if (ret != thrd_success) {
			int unlock_ret = dssh_mtx_unlock(&sess->accept_mtx);
			if (unlock_ret != thrd_success) {
				dssh_thrd_check_unlock(sess, unlock_ret);
				return DSSH_ERROR_TERMINATED;
			}
			if (ret == thrd_timedout)
				return DSSH_ERROR_NOMORE;
			dssh_thrd_check(sess, ret);
			return DSSH_ERROR_TERMINATED;
		}
	}
	int result = DSSH_ERROR_TERMINATED;
	if (!sess->terminate && sess->demux_running)
		result = acceptqueue_pop(&sess->accept_queue, inc) == 0 ? 0 : DSSH_ERROR_NOMORE;
	if (dssh_thrd_check_unlock(sess, dssh_mtx_unlock(&sess->accept_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;
	return result;
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
	return send_channel_open(sess, ch, msg, pos, DSSH_CHANNEL_ID_CONFIRMED);
}

/* Wait with only buf_mtx held; capture the final state before unlocking. */
DSSH_TESTABLE int
wait_open_response(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	struct timespec ts;
	int ret = 0;
	if (sess->timeout_ms > 0) {
		ret = dssh_deadline_from_ms(&ts, sess->timeout_ms);
		if (ret < 0) {
			session_set_terminate(sess);
			return DSSH_ERROR_TERMINATED;
		}
	}
	ret = mtx_lock(&ch->buf_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	while (ch->open_pending && !ch->close_received && !ch->close_sent && !sess->terminate) {
		ret = sess->timeout_ms <= 0 ? cnd_wait(&ch->poll_cnd, &ch->buf_mtx)
		    : cnd_timedwait(&ch->poll_cnd, &ch->buf_mtx, &ts);
		if (ret != thrd_success) {
			if (channel_unlock_lifecycle(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			if (ret == thrd_timedout)
				return DSSH_ERROR_TIMEOUT;
			dssh_thrd_check(sess, ret);
			return DSSH_ERROR_TERMINATED;
		}
	}
	int result = sess->terminate ? DSSH_ERROR_TERMINATED
	    : ch->open ? 0 : DSSH_ERROR_REJECTED;
	if (channel_unlock_lifecycle(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	return result;
}

/*
 * Common channel open: send CHANNEL_OPEN "session", wait for confirmation.
 * Does NOT start the demux -- the channel is registered for demux dispatch.
 */
static int
open_session_channel(struct dssh_session_s *sess, struct dssh_channel_s *ch, bool *registered)
{
	/* Caller has initialized sync primitives (init_channel_sync).
         * Send initial_window=0 per design -- no data flows until the
         * terminal request succeeds and we send WINDOW_ADJUST. */
	*registered = false;
	ch->local_window   = 0;
	ch->open           = false;
	ch->open_pending   = true;
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

	int res = register_channel(sess, ch, registered);

	if (res < 0)
		return res;
	res = send_channel_open(sess, ch, msg, pos, DSSH_CHANNEL_ID_OPENING);
	if (res < 0)
		return res;

	return DSSH_TEST_CHANNEL_RESULT(DSSH_TEST_CHANNEL_OPEN_WAIT, wait_open_response(sess, ch));
}

/* Finish an outstanding request with only its associated mutex held. */
DSSH_TESTABLE int
wait_channel_request_response(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	struct timespec ts;
	int ret = 0;
	if (sess->timeout_ms > 0) {
		ret = dssh_deadline_from_ms(&ts, sess->timeout_ms);
		if (ret < 0) {
			session_set_terminate(sess);
			return DSSH_ERROR_TERMINATED;
		}
	}
	ret = mtx_lock(&ch->buf_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	while (!ch->request_responded && !sess->terminate && !ch->close_received && !ch->close_sent) {
		ret = sess->timeout_ms <= 0 ? cnd_wait(&ch->poll_cnd, &ch->buf_mtx)
		    : cnd_timedwait(&ch->poll_cnd, &ch->buf_mtx, &ts);
		if (ret != thrd_success) {
			ch->request_pending = false;
			ch->request_responded = false;
			if (channel_unlock_lifecycle(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			if (ret == thrd_timedout)
				return DSSH_ERROR_TIMEOUT;
			dssh_thrd_check(sess, ret);
			return DSSH_ERROR_TERMINATED;
		}
	}
	int result = sess->terminate || !ch->request_responded ? DSSH_ERROR_TERMINATED
	    : ch->request_success ? 0 : DSSH_ERROR_REJECTED;
	ch->request_pending = false;
	ch->request_responded = false;
	if (channel_unlock_lifecycle(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	return result;
}

/* Send one setup request, optionally waiting for SUCCESS/FAILURE. */
static int
send_channel_request(struct dssh_session_s *sess, struct dssh_channel_s *ch, const char *req_type,
    const uint8_t *extra, size_t extra_len, bool want_reply)
{
	size_t rtlen = strlen(req_type);
	if (rtlen > UINT32_MAX || rtlen > SIZE_MAX - CHAN_REQUEST_FIXED
	    || extra_len > SIZE_MAX - CHAN_REQUEST_FIXED - rtlen)
		return DSSH_ERROR_INVALID;
	size_t total = CHAN_REQUEST_FIXED + rtlen + extra_len;
	uint8_t *msg;
	int ret = begin_channel_packet(sess, ch, SSH_MSG_CHANNEL_REQUEST, total, &msg);
	if (ret != 0)
		return ret < 0 ? ret : DSSH_ERROR_TERMINATED;
	if (!ch->open || ch->close_received || ch->request_pending) {
		int result = ch->request_pending ? DSSH_ERROR_INVALID : DSSH_ERROR_TERMINATED;
		if (channel_unlock_lifecycle(sess, ch) < 0)
			result = DSSH_ERROR_TERMINATED;
		send_cancel(sess);
		return result;
	}
	if (want_reply) {
		ch->request_pending = true;
		ch->request_responded = false;
		ch->request_success = false;
	}
	if (channel_unlock_lifecycle(sess, ch) < 0) {
		send_cancel(sess);
		return DSSH_ERROR_TERMINATED;
	}

	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(ch->remote_id, msg, &pos);
	uint32_t wire_rtlen = (uint32_t)rtlen;
	DSSH_PUT_U32(wire_rtlen, msg, &pos);
	memcpy(&msg[pos], req_type, rtlen);
	pos += rtlen;
	msg[pos++] = want_reply ? 1 : 0;
	if (extra_len > 0)
		memcpy(&msg[pos], extra, extra_len);
	ret = send_commit(sess, total, NULL);
	if (!want_reply)
		return ret;
	if (ret < 0) {
		int lock_ret = mtx_lock(&ch->buf_mtx);
		if (lock_ret != thrd_success) {
			dssh_thrd_check(sess, lock_ret);
			return DSSH_ERROR_TERMINATED;
		}
		ch->request_pending = false;
		ch->request_responded = false;
		if (channel_unlock_lifecycle(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return ret;
	}
	return wait_channel_request_response(sess, ch);
}

/* Send one setup request and wait for its SUCCESS/FAILURE response. */
DSSH_TESTABLE int
send_channel_request_wait(struct dssh_session_s *sess, struct dssh_channel_s *ch, const char *req_type,
    const uint8_t *extra, size_t extra_len)
{
	return send_channel_request(sess, ch, req_type, extra, extra_len, true);
}

/*
 * Wait for the next message in the setup mailbox.  On success the mailbox
 * remains occupied until setup_release() is called.  This prevents the
 * demux from placing a later packet into setup storage while the acceptor
 * is deciding whether the current request completes setup.
 */
static int
setup_recv_until(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len,
    const struct timespec *deadline)
{
	int ret = mtx_lock(&ch->buf_mtx);

	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	while (!ch->setup_ready && !ch->setup_error && !sess->terminate && !ch->close_received) {
		if (deadline == NULL) {
			ret = cnd_wait(&ch->poll_cnd, &ch->buf_mtx);
			if (ret != thrd_success) {
				int unlock_ret = channel_unlock_lifecycle(sess, ch);

				if (unlock_ret >= 0)
					dssh_thrd_check(sess, ret);
				return DSSH_ERROR_TERMINATED;
			}
		}
		else {
			ret = cnd_timedwait(&ch->poll_cnd, &ch->buf_mtx, deadline);
			if (ret == thrd_timedout) {
				if (channel_unlock_lifecycle(sess, ch) < 0)
					return DSSH_ERROR_TERMINATED;
				return DSSH_ERROR_TIMEOUT;
			}
			if (ret != thrd_success) {
				int unlock_ret = channel_unlock_lifecycle(sess, ch);

				if (unlock_ret >= 0)
					dssh_thrd_check(sess, ret);
				return DSSH_ERROR_TERMINATED;
			}
		}
	}

	if (ch->setup_error) {
		if (channel_unlock_lifecycle(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return DSSH_ERROR_ALLOC;
	}

	if (!ch->setup_ready) {
		if (channel_unlock_lifecycle(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return DSSH_ERROR_TERMINATED;
	}

	*msg_type       = ch->setup_msg_type;
	*payload        = ch->setup_payload;
	*payload_len    = ch->setup_payload_len;

	/* Borrowed until setup_release or the final setup-mode transition.
	 * Keeping the occupied mailbox protects the retained allocation. */
	if (channel_unlock_lifecycle(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	return 0;
}

DSSH_TESTABLE int
setup_recv(struct dssh_session_s *sess, struct dssh_channel_s *ch, uint8_t *msg_type,
    uint8_t **payload, size_t *payload_len)
{
	struct timespec deadline;
	const struct timespec *until = NULL;
	if (sess->timeout_ms > 0) {
		int ret = dssh_deadline_from_ms(&deadline, sess->timeout_ms);
		if (ret < 0) {
			session_set_terminate(sess);
			return DSSH_ERROR_TERMINATED;
		}
		until = &deadline;
	}
	return setup_recv_until(sess, ch, msg_type, payload, payload_len, until);
}

DSSH_TESTABLE int
setup_release(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	int ret = mtx_lock(&ch->buf_mtx);

	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	if (ch->setup_ready) {
		if (ch->setup_msg_type == SSH_MSG_CHANNEL_CLOSE)
			ch->close_received = true;
		else if (ch->setup_msg_type == SSH_MSG_CHANNEL_EOF)
			ch->eof_received = true;
	}
	ch->setup_ready = false;
	ch->setup_error = false;
	ch->setup_payload_len = 0;
	ret = cnd_broadcast(&ch->poll_cnd);
	int unlock_ret = channel_unlock_lifecycle(sess, ch);

	if (unlock_ret < 0)
		return DSSH_ERROR_TERMINATED;
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	return 0;
}

static int
setup_publish_and_unlock(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	int ret = cnd_broadcast(&ch->poll_cnd);
	int unlock_ret = channel_unlock_lifecycle(sess, ch);

	if (unlock_ret < 0)
		return DSSH_ERROR_TERMINATED;
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	return 0;
}

static int
setup_disable(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	int ret = mtx_lock(&ch->buf_mtx);

	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	ch->setup_mode = false;
	return setup_publish_and_unlock(sess, ch);
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
	return send_channel_packet(sess, ch, msg, pos);
}

static int
setup_finish(struct dssh_session_s *sess, struct dssh_channel_s *ch,
    bool want_reply, bool success)
{
	int ret = 0;

	if (want_reply)
		ret = setup_reply(sess, ch, success);
	int release_ret = setup_release(sess, ch);

	if (release_ret < 0)
		return release_ret;
	return ret;
}

/*
 * Allocate a channel, init sync primitives, register with the session,
 * and send CHANNEL_OPEN_CONFIRMATION. The channel takes over the incoming
 * protocol record on success. On failure the accept operation keeps it.
 */
DSSH_TESTABLE struct dssh_channel_s *
accept_channel_init(struct dssh_session_s *sess, struct dssh_incoming_open *inc)
{
	struct dssh_channel_s *ch = new_channel(sess, inc->protocol_id);

	if (ch == NULL)
		return NULL;
	ch->local_window = 0; /* initial_window=0; WINDOW_ADJUST after setup */
	ch->remote_id    = inc->peer_channel;
	atomic_store(&ch->remote_window, inc->peer_window);
	ch->remote_max_packet = inc->peer_max_packet;
	ch->sess = sess;
	ch->io_model = DSSH_IO_STREAM;
	ch->zc_cb = stream_zc_cb;
	ch->event_cb = sess->default_event_cb;
	ch->event_cbdata = sess->default_event_cbdata;
	ch->break_cb = sess->default_break_cb;
	ch->break_cbdata = sess->default_break_cbdata;

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
	int queue_ret = event_queue_init(&ch->events, sess->default_max_events, sess->trans.packet_buf_sz);
	if (queue_ret < 0) {
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->cb_mtx);
		mtx_destroy(&ch->buf_mtx);
		free(ch);
		return NULL;
	}
	if (alloc_channel_slots(sess, ch) < 0) {
		event_queue_free(&ch->events);
		free_tx_slot(&ch->wa_slot);
		free_tx_slot(&ch->chan_reply_slot);
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->cb_mtx);
		mtx_destroy(&ch->buf_mtx);
		free(ch);
		return NULL;
	}
	ch->setup_mode = true;
	ch->open = true;

	bool registered;
	int res = register_channel(sess, ch, &registered);

	if (res < 0) {
		if (registered && detach_channel(sess, ch) < 0)
			return NULL;
		event_queue_free(&ch->events);
		free(ch->setup_payload);
		free_tx_slot(&ch->wa_slot);
		free_tx_slot(&ch->chan_reply_slot);
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->cb_mtx);
		mtx_destroy(&ch->buf_mtx);
		free(ch);
		return NULL;
	}

	res = send_open_confirmation(sess, ch, inc->peer_channel);
	if (res < 0) {
		if (detach_channel(sess, ch) < 0)
			return NULL;
		event_queue_free(&ch->events);
		free(ch->setup_payload);
		free_tx_slot(&ch->wa_slot);
		free_tx_slot(&ch->chan_reply_slot);
		cnd_destroy(&ch->poll_cnd);
		mtx_destroy(&ch->cb_mtx);
		mtx_destroy(&ch->buf_mtx);
		free(ch);
		return NULL;
	}
	return ch;
}

/* ================================================================
 * dssh_chan_* API
 * ================================================================ */

/* ---- ZC core (internal) ---- */

/*
 * Reserve the packet engine, wait for rekey, drain queued slots, check window,
 * return pointer into tx_packet data area.  Caller MUST call
 * zc_send_inner or zc_cancel_inner to release the reservation.
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
	if (in_zc_rx || stream < 0 || stream > 1)
		return DSSH_ERROR_INVALID;

	int acquire_ret = tx_acquire(sess, SSH_MSG_CHANNEL_DATA);

	if (acquire_ret < 0)
		return acquire_ret;

	/* Drain slots now (per-packet path) or defer to zc_send_inner
	 * (gather path — slots will be batched with the data packet). */
	if (!tx_gather_enabled()) {
		int drain_ret = drain_tx_slots(sess);
		if (drain_ret < 0)
			return tx_unlock_with_result(sess, drain_ret);
	}
	if (sess->terminate) {
		tx_unlock_with_slots(sess);
		return DSSH_ERROR_TERMINATED;
	}

	/* Check remote window under buf_mtx */
	int lock_ret = mtx_lock(&ch->buf_mtx);
	if (lock_ret != thrd_success) {
		session_set_terminate_no_wake(sess);
		tx_unlock_with_slots(sess);
		session_set_terminate(sess);
		return DSSH_ERROR_TERMINATED;
	}
	if (!ch->open || ch->eof_sent || ch->close_received) {
		int unlock_ret = channel_unlock_lifecycle(sess, ch);
		tx_unlock_with_slots(sess);
		return unlock_ret < 0 ? unlock_ret : DSSH_ERROR_TERMINATED;
	}

	uint_least32_t window = atomic_load_explicit(&ch->remote_window, memory_order_acquire);
	uint32_t max_pkt = ch->remote_max_packet;

	if (channel_unlock_lifecycle(sess, ch) < 0) {
		tx_unlock_with_slots(sess);
		return DSSH_ERROR_TERMINATED;
	}
	uint32_t credit = max_pkt;
	if (window < max_pkt) {
		uint32_t bounded = (uint32_t)window;
		credit = bounded;
	}

	if (credit == 0) {
		tx_unlock_with_slots(sess);
		return DSSH_ERROR_NOMORE;
	}

	/* Channel header size: CHANNEL_DATA = 9, CHANNEL_EXTENDED_DATA = 13 */
	size_t chan_hdr = (stream == 0) ? CHAN_DATA_FIXED : CHAN_EXT_DATA_FIXED;

	/* Cap by transport buffer capacity:
	 * tx_packet layout: [seq(4)][pkt_len(4)][pad_len(1)][payload][pad][mac]
	 * payload = chan_hdr + data.  Need room for at least 4 bytes padding. */
	size_t max_payload = tx_payload_capacity(sess);

	if (chan_hdr >= max_payload) {
		tx_unlock_with_slots(sess);
		return DSSH_ERROR_TOOLONG;
	}
	size_t max_data = max_payload - chan_hdr;
	size_t avail = max_data;
	if (credit < max_data) {
		size_t credit_size = (size_t)credit;
		avail = credit_size;
	}

	/* Data area starts after: seq(4) + pkt_len(4) + pad_len(1) + chan_hdr */
	*buf          = &sess->trans.tx_packet[9 + chan_hdr];
	*max_len      = avail;
	ch->zc_stream = stream;
	zc_reserved_channel = ch;
	zc_reserved_capacity = avail;
	return 0;
}

/*
 * Fill the header, debit credit before I/O, refund errors and release TX ownership.
 * Caller MUST have called zc_getbuf_inner first.
 */
static int
zc_send_inner(struct dssh_channel_s *ch, size_t len)
{
	struct dssh_session_s *sess     = ch->sess;
	if (!tx_owner_is_current(sess) || zc_reserved_channel != ch)
		return DSSH_ERROR_INVALID;
	if (len > zc_reserved_capacity) {
		zc_reserved_channel = NULL;
		zc_reserved_capacity = 0;
		tx_unlock_with_slots(sess);
		return DSSH_ERROR_TOOLONG;
	}
	zc_reserved_channel = NULL;
	zc_reserved_capacity = 0;
	int                    stream   = ch->zc_stream;
	size_t                 chan_hdr = (stream == 0) ? CHAN_DATA_FIXED : CHAN_EXT_DATA_FIXED;

	/* The reservation capacity was bounded by uint32 peer credit. */
	uint32_t len_u32 = (uint32_t)len;

	/* Build channel header at tx_packet[9] */
	size_t   hp  = 0;
	uint8_t *hdr = &sess->trans.tx_packet[9];

	if (stream == 0) {
		hdr[hp++] = SSH_MSG_CHANNEL_DATA;
		DSSH_PUT_U32(ch->remote_id, hdr, &hp);
		DSSH_PUT_U32(len_u32, hdr, &hp);
	}
	else {
		hdr[hp++] = SSH_MSG_CHANNEL_EXTENDED_DATA;
		DSSH_PUT_U32(ch->remote_id, hdr, &hp);
		DSSH_PUT_U32(SSH_EXTENDED_DATA_STDERR, hdr, &hp);
		DSSH_PUT_U32(len_u32, hdr, &hp);
	}

	size_t payload_len = chan_hdr + len;
	int    ret;

	/* Commit the debit before the peer can observe this packet. A peer
	 * may return credit while the I/O callback is still completing. */
	window_atomic_sub(&ch->remote_window, len_u32);
	if (tx_gather_enabled())
		ret = tx_gather_with_packet(sess, sess->trans.tx_packet, payload_len);
	else
		ret = tx_finalize(sess, sess->trans.tx_packet, payload_len);

	/* A refused/failed send retains the caller's previous credit policy.
	 * Fatal I/O/transform failures still terminate through the existing path. */
	if (ret < 0)
		window_atomic_add(&ch->remote_window, len_u32);

	if ((ret < 0) && (ret != DSSH_ERROR_TOOLONG) && (ret != DSSH_ERROR_REKEY_NEEDED))
		session_set_terminate(sess);

	return tx_unlock_with_result(sess, ret);
}

/*
 * Release the packet-engine reservation without sending.
 */
static int
zc_cancel_inner(struct dssh_channel_s *ch)
{
	struct dssh_session_s *sess = ch->sess;

	if (!tx_owner_is_current(sess) || zc_reserved_channel != ch)
		return DSSH_ERROR_INVALID;
	zc_reserved_channel = NULL;
	zc_reserved_capacity = 0;
	tx_unlock_with_slots(sess);
	return 0;
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

	int ret = mtx_lock(&ch->buf_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return 0;
	}

	struct dssh_bytebuf *bb      = (stream == 0) ? &ch->buf.stdout_buf : &ch->buf.stderr_buf;
	size_t               written = bytebuf_write(bb, data, len);

	ret = cnd_broadcast(&ch->poll_cnd);
	if (channel_unlock_lifecycle(sess, ch) < 0)
		return 0;
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return 0;
	}

	uint32_t consumed = (uint32_t)written;

	return consumed;
}

static int
encode_modes(const struct dssh_chan_params *p, uint8_t **out, size_t *out_len)
{
	if (p->mode_count > (SIZE_MAX - 1) / 5)
		return DSSH_ERROR_ALLOC;
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

	if (tlen > SIZE_MAX - PTY_REQ_FIXED || mlen > SIZE_MAX - PTY_REQ_FIXED - tlen) {
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

		if (nlen > SIZE_MAX - 8 || vlen > SIZE_MAX - 8 - nlen)
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

		int ret = send_channel_request(sess, ch, "env", extra, ep, false);

		free(extra);
		if (ret < 0)
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
#if SIZE_MAX >= UINT32_MAX
			if (clen > UINT32_MAX || clen > SIZE_MAX - 4)
#else
			if (clen > SIZE_MAX - 4)
#endif
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
#if SIZE_MAX >= UINT32_MAX
			if (slen > UINT32_MAX || slen > SIZE_MAX - 4)
#else
			if (slen > SIZE_MAX - 4)
#endif
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
	if (in_zc_rx || connection_api_forbidden(sess))
		return NULL;
	if (!sess->conn_initialized || !sess->demux_running || sess->terminate)
		return NULL;
	if (params_validate(params) < 0)
		return NULL;

	/* Early-data delivery is a shell/exec workaround. Subsystem channels
	 * keep ordinary zero-window setup in both I/O models. This is a
	 * channel-type policy; this stream open still uses ring buffers
	 * for all supported terminal request types. */
	if ((params->flags & DSSH_PARAM_ACCEPT_EARLY_DATA)
	    && params->type != DSSH_CHAN_SHELL && params->type != DSSH_CHAN_EXEC)
		return NULL;

	struct dssh_channel_s *ch = new_channel(sess, NULL);

	if (ch == NULL)
		return NULL;

	uint32_t winsz = params->max_window != 0 ? params->max_window : INITIAL_WINDOW_SIZE;

	int res = params_clone(&ch->params, params, winsz);
	if (res < 0) {
		free_channel(ch);
		return NULL;
	}
	const struct dssh_chan_params *snapshot = &ch->params;

	/* Phase 1: sync primitives only */
	res = init_channel_sync(sess, ch);
	bool registered;

	if (res < 0) {
		params_free(&ch->params);
		free_channel(ch);
		return NULL;
	}

	ch->sess                  = sess;
	ch->io_model              = DSSH_IO_STREAM;
	ch->zc_cb                 = stream_zc_cb;
	ch->event_cb              = sess->default_event_cb;
	ch->event_cbdata          = sess->default_event_cbdata;
	ch->break_cb              = sess->default_break_cb;
	ch->break_cbdata          = sess->default_break_cbdata;
	ch->accept_pre_window_data = (snapshot->flags & DSSH_PARAM_ACCEPT_EARLY_DATA) != 0;

	/* Init event queue early — demux can dispatch EOF/CLOSE events
	 * to this channel as soon as it's registered. */
	res = event_queue_init(&ch->events, sess->default_max_events, sess->trans.packet_buf_sz);
	if (res < 0)
		goto fail_sync;

	/* Phase 2: allocate buffers BEFORE OPEN.  Universal rx-window
	 * truncation (handle_channel_data) keeps non-flagged channels'
	 * early-data behaviour identical -- bytes are clipped to
	 * local_window=0 before stream_zc_cb runs, so allocated-but-
	 * unused bytebufs are harmless.  Flagged channels need them
	 * populated for the bypass path to deliver into. */
	res = init_channel_buffers(ch, winsz);
	if (res < 0)
		goto fail_events;

	/* CHANNEL_OPEN with initial_window=0 */
	res = open_session_channel(sess, ch, &registered);
	if (res < 0) {
		if (registered && detach_channel(sess, ch) < 0)
			return NULL;
		goto fail_buffers;
	}

	/* Setup: env, pty-req, terminal request */
	if (snapshot->env_count > 0) {
		res = chan_send_env(sess, ch, snapshot);
		if (res < 0)
			goto fail_close;
	}
	if (snapshot->flags & DSSH_PARAM_HAS_PTY) {
		res = chan_send_pty_req(sess, ch, snapshot);
		if (res < 0)
			goto fail_close;
	}
	res = chan_send_terminal_request(sess, ch, snapshot);
	if (res < 0)
		goto fail_close;

	/* Open the window -- data can flow now */
	res = send_window_adjust(sess, ch, winsz);
	if (res < 0)
		goto fail_close;

	/* Mystic-server workaround: a Cryptlib-based server may gate
	 * further output on receiving any client CHANNEL_DATA.  Send a
	 * 0-byte CHANNEL_DATA to satisfy that predicate without injecting
	 * spurious keystrokes.  Best-effort -- failure isn't fatal, the
	 * app's first real keystroke still kicks the server. */
	if (ch->accept_pre_window_data)
		(void)send_empty_channel_data_kick(sess, ch);

	/* Setup is done; rx-window enforcement applies for the rest of
	 * the channel's life.  Release-store so the demux's acquire-
	 * load in handle_channel_*data sees this without buf_mtx. */
	atomic_store_explicit(&ch->setup_complete, true, memory_order_release);


	return ch;

fail_close:
	conn_close(sess, ch);
	if (detach_channel(sess, ch) < 0)
		return NULL;
fail_buffers:
	bytebuf_free(&ch->buf.stdout_buf);
	bytebuf_free(&ch->buf.stderr_buf);
fail_events:
	event_queue_free(&ch->events);
fail_sync:
	params_free(&ch->params);
	free_tx_slot(&ch->wa_slot);
	free_tx_slot(&ch->chan_reply_slot);
	cnd_destroy(&ch->poll_cnd);
	mtx_destroy(&ch->cb_mtx);
	mtx_destroy(&ch->buf_mtx);
	free_channel(ch);
	return NULL;
}

/* Store before attempting notification: a failed acquisition must not replace
 * the first error or the count of bytes already delivered. No mutex is held
 * on entry. The TX caller retains its channel reference through this call. */
DSSH_PRIVATE void
channel_defer_read_error(struct dssh_session_s *sess, struct dssh_channel_s *ch, int error)
{
	if (ch->io_model != DSSH_IO_STREAM || error >= 0)
		return;
	if ((error == DSSH_ERROR_TERMINATED || sess->terminate)
	    && atomic_exchange(&ch->read_error_terminal, true))
		return;
	int expected = 0;
	(void)atomic_compare_exchange_strong(&ch->read_error, &expected, error);
	/* Termination already makes every requested read direction ready. */
	if (sess->terminate)
		return;
	int ret = mtx_lock(&ch->buf_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return;
	}
	ret = cnd_broadcast(&ch->poll_cnd);
	dssh_mtx_unlock(&ch->buf_mtx);
	if (ret != thrd_success)
		dssh_thrd_check(sess, ret);
}

DSSH_PUBLIC int64_t
dssh_chan_read(struct dssh_channel_s *ch, int stream, uint8_t *buf, size_t bufsz)
{
	if (ch == NULL || stream < 0 || stream > 1)
		return DSSH_ERROR_INVALID;
	if (in_zc_rx || channel_io_forbidden(ch))
		return DSSH_ERROR_INVALID;
	if (ch->io_model != DSSH_IO_STREAM)
		return DSSH_ERROR_INVALID;
	if (buf == NULL && bufsz != 0)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;
	if (buf != NULL && bufsz != 0 && !tx_slot_context_allowed(sess))
		return DSSH_ERROR_INVALID;
	bool                   ext  = (stream != 0);
	int lock_ret = mtx_lock(&ch->buf_mtx);
	if (lock_ret != thrd_success) {
		dssh_thrd_check(sess, lock_ret);
		return DSSH_ERROR_TERMINATED;
	}
	struct dssh_bytebuf *bb = ext ? &ch->buf.stderr_buf : &ch->buf.stdout_buf;
	if (buf == NULL) {
		size_t avail = bytebuf_available(bb);
		if (channel_unlock_lifecycle(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return (int64_t)avail;
	}
	if (bufsz != 0) {
		int error = atomic_exchange(&ch->read_error, 0);
		if (error < 0) {
			if (channel_unlock_lifecycle(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			return error;
		}
		/* Retry already queued credit even if this stream is empty. A
		 * resolved pre-wire refusal must not need another application send.
		 * Reacquire and recheck before consuming any bytes or new error. */
		bool retry = !sess->terminate && !ch->eof_received && !ch->close_received
		    && ch->window_adjust_pending > 0 && atomic_load(&ch->wa_slot.ready);
		if (retry) {
			if (channel_unlock_lifecycle(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			int ret = schedule_wa_slot(sess);
			if (ret < 0)
				channel_defer_read_error(sess, ch, ret);
			lock_ret = mtx_lock(&ch->buf_mtx);
			if (lock_ret != thrd_success) {
				dssh_thrd_check(sess, lock_ret);
				return DSSH_ERROR_TERMINATED;
			}
			error = atomic_exchange(&ch->read_error, 0);
			if (error < 0) {
				if (channel_unlock_lifecycle(sess, ch) < 0)
					return DSSH_ERROR_TERMINATED;
				return error;
			}
		}
	}
	size_t n              = bytebuf_read(bb, buf, bufsz, 0);
	/* Disambiguate empty-buffer from EOF: return 0 only when EOF
	 * has actually arrived (peer EOF/CLOSE) or the session has
	 * terminated; otherwise NOMORE means "nothing right now, not
	 * EOF".  Read under buf_mtx -- eof_received / close_received
	 * are written under buf_mtx in the demux. */
	bool nomore_not_eof = (n == 0) && !ch->eof_received && !ch->close_received && !sess->terminate;
	if (channel_unlock_lifecycle(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	if (nomore_not_eof)
		return DSSH_ERROR_NOMORE;
	if (n > 0) {
		int wret = maybe_replenish_window(sess, ch);
		if (wret < 0)
			channel_defer_read_error(sess, ch, wret);
	}
	return (int64_t)n;
}

DSSH_PUBLIC int64_t
dssh_chan_write(struct dssh_channel_s *ch, int stream, const uint8_t *buf, size_t bufsz)
{
	if (ch == NULL || buf == NULL || stream < 0 || stream > 1)
		return DSSH_ERROR_INVALID;
	if (channel_io_forbidden(ch))
		return DSSH_ERROR_INVALID;
	if (ch->io_model != DSSH_IO_STREAM)
		return DSSH_ERROR_INVALID;
	if (bufsz == 0)
		return 0;

	uint8_t *zbuf;
	size_t   max_len;
	int      ret;

	ret = zc_getbuf_inner(ch, stream, &zbuf, &max_len);
	if (ret < 0)
		return ret; /* NOMORE for window-full, other negatives for error */

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
	if (ch == NULL || (events & ~(DSSH_POLL_READ | DSSH_POLL_READEXT | DSSH_POLL_WRITE | DSSH_POLL_EVENT)))
		return DSSH_ERROR_INVALID;
	if (in_zc_rx || channel_io_forbidden(ch))
		return DSSH_ERROR_INVALID;
	if (ch->io_model != DSSH_IO_STREAM)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;
	struct timespec        ts;
	int ret = 0;
	if (timeout_ms > 0) {
		ret = dssh_deadline_from_ms(&ts, timeout_ms);
		if (ret < 0) {
			return ret;
		}
	}
	ret = mtx_lock(&ch->buf_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	for (;;) {
		/* Termination makes every requested data direction ready.
		 * Reads first report retained errors and drain buffered bytes,
		 * then return EOF. Writes return a negative error. This mirrors
		 * how close_received forces read readiness, and uses the same
		 * ordinary poll predicates below. */
		bool terminated = sess->terminate;
		bool read_failed = atomic_load(&ch->read_error) < 0;
		int ready = 0;
		if ((events & DSSH_POLL_READ)
		    && ((bytebuf_available(&ch->buf.stdout_buf) > 0) || ch->eof_received || ch->close_received
		        || terminated || read_failed))
			ready |= DSSH_POLL_READ;
		if ((events & DSSH_POLL_READEXT)
		    && ((bytebuf_available(&ch->buf.stderr_buf) > 0) || ch->eof_received || ch->close_received
		        || terminated || read_failed))
			ready |= DSSH_POLL_READEXT;
		if ((events & DSSH_POLL_WRITE)
		    && (terminated
		        || ((atomic_load_explicit(&ch->remote_window, memory_order_acquire) > 0) && ch->open
		            && !ch->close_received && !ch->eof_sent && ch->remote_max_packet > 0)))
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
		if (ready || terminated) {
			if (channel_unlock_lifecycle(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			return ready;
		}
		if (timeout_ms == 0) {
			if (channel_unlock_lifecycle(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			return 0;
		}
		ret = timeout_ms < 0 ? cnd_wait(&ch->poll_cnd, &ch->buf_mtx)
		    : cnd_timedwait(&ch->poll_cnd, &ch->buf_mtx, &ts);
		if (ret != thrd_success) {
			if (channel_unlock_lifecycle(sess, ch) < 0)
				return DSSH_ERROR_TERMINATED;
			if (ret == thrd_timedout)
				return 0;
			dssh_thrd_check(sess, ret);
			return DSSH_ERROR_TERMINATED;
		}
	}
}

DSSH_PUBLIC int
dssh_chan_read_event(struct dssh_channel_s *ch, struct dssh_chan_event *event)
{
	if (ch == NULL || event == NULL)
		return DSSH_ERROR_INVALID;
	if (in_zc_rx || channel_io_forbidden(ch))
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;
	int ret = mtx_lock(&ch->buf_mtx);
	if (ret != thrd_success) {
		dssh_thrd_check(sess, ret);
		return DSSH_ERROR_TERMINATED;
	}
	if (!ch->events.has_frozen) {
		if (channel_unlock_lifecycle(sess, ch) < 0)
			return DSSH_ERROR_TERMINATED;
		return DSSH_ERROR_NOMORE;
	}
	event_from_entry(&ch->events.frozen, event);
	ch->events.has_frozen = false;
	if (channel_unlock_lifecycle(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_shutwr(struct dssh_channel_s *ch)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	if (channel_io_forbidden(ch))
		return DSSH_ERROR_INVALID;
	if (in_zc_rx)
		return DSSH_ERROR_INVALID;
	return send_eof(ch->sess, ch);
}

/* Status/EOF are best effort during disposal. Their helpers already apply
 * transport failure policy. Retain recoverable pre-wire refusal and the
 * channel-local already-closed result without terminating a healthy session. */
static void
close_optional_packet_result(struct dssh_session_s *sess, int result)
{
	if (result < 0 && result != DSSH_ERROR_TOOLONG && result != DSSH_ERROR_REKEY_NEEDED
	    && result != DSSH_ERROR_TERMINATED)
		session_set_terminate(sess);
}

DSSH_PUBLIC int
dssh_chan_close(struct dssh_channel_s *ch, int64_t exit_code)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	if (channel_api_forbidden(ch))
		return DSSH_ERROR_INVALID;
	if (in_zc_rx)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;
	if (tx_owner_is_current(sess))
		return DSSH_ERROR_INVALID;
	struct setup_callback_scope *scope = setup_scope_for_channel(ch);
	if (scope != NULL) {
		scope->cancelled = true;
		return 0;
	}
	if (channel_io_forbidden(ch))
		return DSSH_ERROR_INVALID;
	int ret;
	if (!sess->trans.client && exit_code >= 0 && exit_code <= UINT32_MAX) {
		uint32_t ec = (uint32_t)exit_code;
		ret = dssh_conn_send_exit_status(sess, ch, ec);
		close_optional_packet_result(sess, ret);
	}
	ret = send_eof(sess, ch);
	close_optional_packet_result(sess, ret);
	ret = conn_close(sess, ch);
	if (ret < 0)
		session_set_terminate(sess);
	if (detach_channel(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	params_free(&ch->params);
	event_queue_free(&ch->events);
	cleanup_channel_buffers(ch);
	free_channel(ch);
	return 0;
}

DSSH_PUBLIC int
dssh_chan_send_signal(struct dssh_channel_s *ch, const char *signal_name)
{
	if (ch == NULL || signal_name == NULL)
		return DSSH_ERROR_INVALID;
	if (channel_io_forbidden(ch))
		return DSSH_ERROR_INVALID;
	if (in_zc_rx)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess  = ch->sess;
	size_t                 slen  = strlen(signal_name);
	size_t                 rtlen = DSSH_STRLEN(str_signal);
	size_t                 fixed = CHAN_REQUEST_FIXED + rtlen + 4;
	if (slen > UINT32_MAX || slen > SIZE_MAX - fixed)
		return DSSH_ERROR_INVALID;
	uint8_t *msg;
	int ret = begin_channel_packet(sess, ch, SSH_MSG_CHANNEL_REQUEST, fixed + slen, &msg);
	if (ret != 0)
		return ret < 0 ? ret : DSSH_ERROR_TERMINATED;
	dssh_mtx_unlock(&ch->buf_mtx);
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
	if (channel_io_forbidden(ch))
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
	size_t rtlen = sizeof(str_window_change) - 1;
	size_t total = CHAN_REQUEST_FIXED + rtlen + sizeof(extra);
	uint8_t *msg;
	int ret = begin_channel_packet(sess, ch, SSH_MSG_CHANNEL_REQUEST, total, &msg);
	if (ret != 0)
		return ret < 0 ? ret : DSSH_ERROR_TERMINATED;
	uint32_t remote = ch->remote_id;
	dssh_mtx_unlock(&ch->buf_mtx);
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(remote, msg, &pos);
	DSSH_PUT_U32(DSSH_STRLEN(str_window_change), msg, &pos);
	memcpy(&msg[pos], str_window_change, rtlen);
	pos += rtlen;
	msg[pos++] = 0;
	memcpy(&msg[pos], extra, ep);
	return send_commit(sess, total, NULL);
}

DSSH_PUBLIC int
dssh_chan_send_break(struct dssh_channel_s *ch, uint32_t length)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	if (channel_io_forbidden(ch))
		return DSSH_ERROR_INVALID;
	if (in_zc_rx)
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;
	uint8_t                extra[4];
	size_t                 ep = 0;
	DSSH_PUT_U32(length, extra, &ep);
	size_t rtlen = sizeof("break") - 1;
	size_t total = CHAN_REQUEST_FIXED + rtlen + sizeof(extra);
	uint8_t *msg;
	int ret = begin_channel_packet(sess, ch, SSH_MSG_CHANNEL_REQUEST, total, &msg);
	if (ret != 0)
		return ret < 0 ? ret : DSSH_ERROR_TERMINATED;
	uint32_t remote = ch->remote_id;
	dssh_mtx_unlock(&ch->buf_mtx);
	size_t pos = 0;
	msg[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(remote, msg, &pos);
	DSSH_PUT_U32(DSSH_STRLEN("break"), msg, &pos);
	memcpy(&msg[pos], "break", rtlen);
	pos += rtlen;
	msg[pos++] = 0;
	memcpy(&msg[pos], extra, ep);
	return send_commit(sess, total, NULL);
}

DSSH_PUBLIC enum dssh_chan_type
dssh_chan_get_type(struct dssh_channel_s *ch)
{
	if (ch == NULL || channel_api_forbidden(ch))
		return DSSH_CHAN_SHELL;
	return channel_params_view(ch)->type;
}

DSSH_PUBLIC const char *
dssh_chan_get_command(struct dssh_channel_s *ch)
{
	if (ch == NULL || channel_api_forbidden(ch))
		return NULL;
	const struct dssh_chan_params *params = channel_params_view(ch);
	return params->type != DSSH_CHAN_EXEC ? NULL : params->command;
}

DSSH_PUBLIC const char *
dssh_chan_get_subsystem(struct dssh_channel_s *ch)
{
	if (ch == NULL || channel_api_forbidden(ch))
		return NULL;
	const struct dssh_chan_params *params = channel_params_view(ch);
	return params->type != DSSH_CHAN_SUBSYSTEM ? NULL : params->subsystem;
}

DSSH_PUBLIC bool
dssh_chan_has_pty(struct dssh_channel_s *ch)
{
	if (ch == NULL || channel_api_forbidden(ch))
		return false;
	return (channel_params_view(ch)->flags & DSSH_PARAM_HAS_PTY) != 0;
}

DSSH_PUBLIC const struct dssh_chan_params *
dssh_chan_get_pty(struct dssh_channel_s *ch)
{
	if (ch == NULL || channel_api_forbidden(ch))
		return NULL;
	const struct dssh_chan_params *params = channel_params_view(ch);
	if (!(params->flags & DSSH_PARAM_HAS_PTY))
		return NULL;
	return params;
}

/* ---- Public ZC API ---- */

DSSH_PUBLIC int
dssh_chan_zc_getbuf(struct dssh_channel_s *ch, int stream, uint8_t **buf, size_t *max_len)
{
	if (ch == NULL || buf == NULL || max_len == NULL)
		return DSSH_ERROR_INVALID;
	if (channel_io_forbidden(ch))
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
	if (channel_io_forbidden(ch))
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
	if (in_zc_rx || channel_io_forbidden(ch))
		return DSSH_ERROR_INVALID;
	return zc_cancel_inner(ch);
}

DSSH_PUBLIC struct dssh_channel_s *
dssh_chan_zc_open(struct dssh_session_s *sess, const struct dssh_chan_params *params, dssh_chan_zc_cb cb,
    void *cbdata)
{
	if (sess == NULL || params == NULL || cb == NULL)
		return NULL;
	if (in_zc_rx || connection_api_forbidden(sess))
		return NULL;
	if (!sess->conn_initialized || !sess->demux_running || sess->terminate)
		return NULL;
	if (params_validate(params) < 0)
		return NULL;

	/* See dssh_chan_open: the shell/exec early-data workaround excludes
	 * subsystem channels in both stream and zero-copy mode. */
	if ((params->flags & DSSH_PARAM_ACCEPT_EARLY_DATA)
	    && params->type != DSSH_CHAN_SHELL && params->type != DSSH_CHAN_EXEC)
		return NULL;

	struct dssh_channel_s *ch = new_channel(sess, NULL);

	if (ch == NULL)
		return NULL;

	uint32_t winsz = params->max_window != 0 ? params->max_window : INITIAL_WINDOW_SIZE;

	int res = params_clone(&ch->params, params, winsz);
	if (res < 0) {
		free_channel(ch);
		return NULL;
	}
	const struct dssh_chan_params *snapshot = &ch->params;

	/* Phase 1: sync primitives only (ZC has no ring buffers) */
	res = init_channel_sync(sess, ch);
	bool registered;

	if (res < 0) {
		params_free(&ch->params);
		free_channel(ch);
		return NULL;
	}

	ch->sess                  = sess;
	ch->io_model              = DSSH_IO_ZC;
	ch->zc_cb                 = cb;
	ch->zc_cbdata             = cbdata;
	ch->event_cb              = sess->default_event_cb;
	ch->event_cbdata          = sess->default_event_cbdata;
	ch->break_cb              = sess->default_break_cb;
	ch->break_cbdata          = sess->default_break_cbdata;
	ch->accept_pre_window_data = (snapshot->flags & DSSH_PARAM_ACCEPT_EARLY_DATA) != 0;

	/* Init event queue early — demux can dispatch EOF/CLOSE events
	 * to this channel as soon as it's registered. */
	res = event_queue_init(&ch->events, sess->default_max_events, sess->trans.packet_buf_sz);
	if (res < 0)
		goto fail_sync;

	/* CHANNEL_OPEN with initial_window=0 */
	res = open_session_channel(sess, ch, &registered);
	if (res < 0) {
		if (registered && detach_channel(sess, ch) < 0)
			return NULL;
		goto fail_events;
	}

	/* Setup: env, pty-req, terminal request */
	if (snapshot->env_count > 0) {
		res = chan_send_env(sess, ch, snapshot);
		if (res < 0)
			goto fail_close;
	}
	if (snapshot->flags & DSSH_PARAM_HAS_PTY) {
		res = chan_send_pty_req(sess, ch, snapshot);
		if (res < 0)
			goto fail_close;
	}
	res = chan_send_terminal_request(sess, ch, snapshot);
	if (res < 0)
		goto fail_close;

	/* ZC mode: no ring buffers, just event queue + window */
	ch->window_max = winsz;

	/* Open the window */
	res = send_window_adjust(sess, ch, winsz);
	if (res < 0)
		goto fail_close;

	/* Mystic-server workaround: see dssh_chan_open. */
	if (ch->accept_pre_window_data)
		(void)send_empty_channel_data_kick(sess, ch);

	/* Setup is done; rx-window enforcement applies for the rest of
	 * the channel's life.  See dssh_chan_open for the atomic
	 * release-store rationale. */
	atomic_store_explicit(&ch->setup_complete, true, memory_order_release);


	return ch;

fail_close:
	conn_close(sess, ch);
	if (detach_channel(sess, ch) < 0)
		return NULL;
fail_events:
	event_queue_free(&ch->events);
fail_sync:
	params_free(&ch->params);
	free_tx_slot(&ch->wa_slot);
	free_tx_slot(&ch->chan_reply_slot);
	cnd_destroy(&ch->poll_cnd);
	mtx_destroy(&ch->cb_mtx);
	mtx_destroy(&ch->buf_mtx);
	free_channel(ch);
	return NULL;
}

/* ---- Event callback setters ---- */

DSSH_PUBLIC int
dssh_chan_set_event_cb(struct dssh_channel_s *ch, dssh_chan_event_cb cb, void *cbdata)
{
	if (ch == NULL)
		return DSSH_ERROR_INVALID;
	if (channel_api_forbidden(ch))
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;

	if (dssh_thrd_check(sess, mtx_lock(&ch->cb_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;
	ch->event_cb     = cb;
	ch->event_cbdata = cbdata;
	if (dssh_thrd_check_unlock(sess, dssh_mtx_unlock(&ch->cb_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_event_cb(struct dssh_session_s *sess, dssh_chan_event_cb cb, void *cbdata)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (connection_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	sess->default_event_cb     = cb;
	sess->default_event_cbdata = cbdata;
	return 0;
}

DSSH_PUBLIC int
dssh_chan_set_break_cb(struct dssh_channel_s *ch, dssh_chan_break_cb cb, void *cbdata)
{
	if (ch == NULL || channel_api_forbidden(ch))
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;
	if (dssh_thrd_check(sess, mtx_lock(&ch->cb_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;
	ch->break_cb = cb;
	ch->break_cbdata = cbdata;
	if (dssh_thrd_check_unlock(sess, dssh_mtx_unlock(&ch->cb_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_break_cb(struct dssh_session_s *sess, dssh_chan_break_cb cb, void *cbdata)
{
	if (sess == NULL || connection_api_forbidden(sess))
		return DSSH_ERROR_INVALID;
	if (sess->demux_running)
		return DSSH_ERROR_TOOLATE;
	sess->default_break_cb = cb;
	sess->default_break_cbdata = cbdata;
	return 0;
}

DSSH_PUBLIC int
dssh_session_set_max_events(struct dssh_session_s *sess, size_t max_events)
{
	if (sess == NULL)
		return DSSH_ERROR_INVALID;
	if (connection_api_forbidden(sess))
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
	if (channel_api_forbidden(ch))
		return DSSH_ERROR_INVALID;
	struct dssh_session_s *sess = ch->sess;

	if (dssh_thrd_check(sess, mtx_lock(&ch->buf_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;
	if (max_events != 0 && max_events < ch->events.count) {
		if (dssh_thrd_check_unlock(sess, dssh_mtx_unlock(&ch->buf_mtx)) != thrd_success)
			return DSSH_ERROR_TERMINATED;
		return DSSH_ERROR_INVALID;
	}
	ch->events.max_events = max_events;
	if (dssh_thrd_check_unlock(sess, dssh_mtx_unlock(&ch->buf_mtx)) != thrd_success)
		return DSSH_ERROR_TERMINATED;
	return 0;
}

/* ================================================================
 * Server accept -- dssh_chan_accept
 * ================================================================ */

DSSH_TESTABLE int
accept_parse_pty(const uint8_t *data, size_t data_len, struct dssh_chan_params *p)
{
	struct pty_req pty;
	int            ret = parse_pty_req_data(data, data_len, &pty);
	if (ret < 0)
		return ret;
	if (pty.term_len == SIZE_MAX || memchr(pty.term, 0, pty.term_len) != NULL)
		return DSSH_ERROR_PARSE;
	/* Validate all opcode arguments before allocating or publishing state.
	 * END and future opcodes stop parsing as specified by RFC 4254 s8. */
	size_t mpos = 0;
	while (mpos < pty.modes_len) {
		uint8_t opcode = pty.modes[mpos++];
		if (opcode == 0 || opcode >= 160)
			break;
		if (!dssh_buffer_has(pty.modes_len, mpos, 4))
			return DSSH_ERROR_PARSE;
		mpos += 4;
		if (mpos == pty.modes_len)
			return DSSH_ERROR_PARSE;
	}
	char *term = malloc(pty.term_len + 1);
	if (term == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(term, pty.term, pty.term_len);
	term[pty.term_len] = 0;
	struct dssh_chan_params parsed = { 0 };
	if (pty.modes_len > 0) {
		mpos = 0;
		while (mpos < pty.modes_len) {
			uint8_t opcode = pty.modes[mpos++];
			if (opcode == 0 || opcode >= 160)
				break;
			uint32_t val = DSSH_GET_U32(&pty.modes[mpos]);
			mpos += 4;
			ret = params_set_mode(&parsed, opcode, val);
			if (ret < 0) {
				free(term);
				free(parsed.modes);
				return ret;
			}
		}
	}
	free(p->term);
	free(p->modes);
	p->term = term;
	p->modes = parsed.modes;
	p->mode_count = parsed.mode_count;
	p->mode_capacity = parsed.mode_capacity;
	p->flags |= DSSH_PARAM_HAS_PTY;
	p->cols = pty.cols;
	p->rows = pty.rows;
	p->wpx  = pty.wpx;
	p->hpx  = pty.hpx;
	return 0;
}

/* Bound retained environment strings independently of the packet size limit. */
#define ACCEPT_ENV_COUNT 128
#define ACCEPT_ENV_BYTES UINT32_C(1048576)

static int
accept_parse_env(const uint8_t *data, size_t data_len, struct dssh_chan_params *p)
{
	const uint8_t *name, *value;
	size_t         nlen, vlen;
	int            ret = parse_env_data(data, data_len, &name, &nlen, &value, &vlen);
	if (ret < 0)
		return ret;
	if (memchr(name, 0, nlen) != NULL || memchr(value, 0, vlen) != NULL)
		return DSSH_ERROR_PARSE;
	if (p->env_count >= ACCEPT_ENV_COUNT)
		return DSSH_ERROR_TOOMANY;
#if SIZE_MAX < ACCEPT_ENV_BYTES
	size_t remaining = SIZE_MAX;
#else
	size_t remaining = ACCEPT_ENV_BYTES;
#endif
	for (size_t i = 0; i < p->env_count; i++) {
		size_t len = strlen(p->env[i].name);
		if (len >= remaining)
			return DSSH_ERROR_TOOLONG;
		remaining -= len + 1;
		len = strlen(p->env[i].value);
		if (len >= remaining)
			return DSSH_ERROR_TOOLONG;
		remaining -= len + 1;
	}
	if (nlen >= remaining)
		return DSSH_ERROR_TOOLONG;
	remaining -= nlen + 1;
	if (vlen >= remaining)
		return DSSH_ERROR_TOOLONG;
	return params_add_env(p, name, nlen, value, vlen);
}

DSSH_TESTABLE int
accept_env_request(struct dssh_channel_s *ch, const uint8_t *data, size_t data_len,
    struct dssh_chan_params *p, const struct dssh_chan_accept_cbs *cbs, bool *cancelled)
{
	*cancelled = false;
	if (cbs->env == NULL)
		return DSSH_ERROR_REJECTED;
	int ret = accept_parse_env(data, data_len, p);
	if (ret < 0)
		return ret;
	ret = call_setup_request(ch, p, cbs->env, cbs->cbdata, cancelled);
	if (ret < 0) {
		p->env_count--;
		free(p->env[p->env_count].name);
		free(p->env[p->env_count].value);
		p->env[p->env_count].name = NULL;
		p->env[p->env_count].value = NULL;
	}
	return ret;
}

static int
accept_parse_string(const uint8_t *data, size_t data_len, char **dest)
{
	if (data_len < 4)
		return DSSH_ERROR_PARSE;
	uint32_t wire_len = DSSH_GET_U32(data);
	if (wire_len != data_len - 4 || memchr(&data[4], 0, data_len - 4) != NULL)
		return DSSH_ERROR_PARSE;
	size_t len = data_len - 4;
	char *replacement = malloc(len + 1);
	if (replacement == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(replacement, &data[4], len);
	replacement[len] = 0;
	free(*dest);
	*dest = replacement;
	return 0;
}

DSSH_TESTABLE int
accept_parse_exec(const uint8_t *data, size_t data_len, struct dssh_chan_params *p)
{
	return accept_parse_string(data, data_len, &p->command);
}

DSSH_TESTABLE int
accept_parse_subsystem(const uint8_t *data, size_t data_len, struct dssh_chan_params *p)
{
	return accept_parse_string(data, data_len, &p->subsystem);
}

/*
 * After sending CHANNEL_CLOSE for a rejected channel, drain the setup
 * mailbox until the peer's reciprocal CHANNEL_CLOSE arrives.  Best-effort:
 * on timeout or session termination we give up and let the channel be freed
 * (the demux will silently discard the late CLOSE via find_channel→NULL).
 */
DSSH_TESTABLE int
wait_for_close(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	struct timespec deadline;
	const struct timespec *until = NULL;
	if (sess->timeout_ms > 0) {
		int ret = dssh_deadline_from_ms(&deadline, sess->timeout_ms);
		if (ret < 0)
			return ret;
		until = &deadline;
	}
	for (;;) {
		int ret = mtx_lock(&ch->buf_mtx);
		if (ret != thrd_success) {
			dssh_thrd_check(sess, ret);
			return DSSH_ERROR_TERMINATED;
		}
		bool closed = ch->close_received;
		dssh_mtx_unlock(&ch->buf_mtx);
		if (closed)
			return 0;
		if (until != NULL) {
			struct timespec now;
			ret = dssh_clock_read(&now);
			if (ret < 0)
				return ret;
			if (now.tv_sec > until->tv_sec
			    || (now.tv_sec == until->tv_sec && now.tv_nsec >= until->tv_nsec))
				return DSSH_ERROR_TIMEOUT;
		}
		uint8_t msg_type;
		uint8_t *payload;
		size_t payload_len;
		ret = setup_recv_until(sess, ch, &msg_type, &payload, &payload_len, until);
		/* Normal-mode CLOSE wakes the same condition but has no mailbox.
		 * Recheck its protected predicate rather than treating it as failure. */
		if (ret == DSSH_ERROR_TERMINATED && !sess->terminate)
			continue;
		if (ret < 0)
			return ret;
		ret = setup_finish(sess, ch, false, false);
		if (ret < 0)
			return ret;
		if (msg_type == SSH_MSG_CHANNEL_CLOSE)
			return 0;
	}
}

/* End the occupied request before waiting, so demux can publish reciprocal
 * CLOSE. Every mandatory send is checked; failure never enters a futile wait. */
static int
close_setup_channel(struct dssh_session_s *sess, struct dssh_channel_s *ch, bool want_reply)
{
	if (sess->terminate)
		return DSSH_ERROR_TERMINATED;
	int ret = setup_finish(sess, ch, want_reply, false);
	if (ret < 0) {
		session_set_terminate(sess);
		return ret;
	}
	ret = conn_close(sess, ch);
	if (ret < 0) {
		session_set_terminate(sess);
		return ret;
	}
	return wait_for_close(sess, ch);
}

static int
discard_setup_channel(struct dssh_session_s *sess, struct dssh_channel_s *ch)
{
	if (setup_disable(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	if (detach_channel(sess, ch) < 0)
		return DSSH_ERROR_TERMINATED;
	params_free(&ch->params);
	event_queue_free(&ch->events);
	cleanup_channel_buffers(ch);
	free_channel(ch);
	return 0;
}

static int
cancel_setup_channel(struct dssh_session_s *sess, struct dssh_channel_s *ch, bool want_reply)
{
	int ret = close_setup_channel(sess, ch, want_reply);
	return ret < 0 && ret != DSSH_ERROR_TIMEOUT ? ret : DSSH_ERROR_REJECTED;
}

static int
chan_accept_setup_loop(struct dssh_session_s *sess, struct dssh_channel_s *ch,
	const struct dssh_chan_accept_cbs *cbs, struct dssh_chan_params *p, struct dssh_chan_accept_result *result,
	bool *terminal_want_reply)
{
	bool had_pty = false;
	for (;;) {
		uint8_t  msg_type;
		uint8_t *payload;
		size_t   payload_len;
		int      res = DSSH_TEST_CHANNEL_RESULT(DSSH_TEST_CHANNEL_ACCEPT_WAIT, setup_recv(sess, ch, &msg_type, &payload, &payload_len));
		if (res < 0)
			return res;
		if (msg_type == SSH_MSG_CHANNEL_CLOSE) {
			res = close_setup_channel(sess, ch, false);
			return res < 0 ? res : DSSH_ERROR_REJECTED;
		}
		if (msg_type == SSH_MSG_CHANNEL_WINDOW_ADJUST) {
			/* Fixed layout was validated before publishing this mailbox. */
			window_atomic_add(&ch->remote_window, DSSH_GET_U32(&payload[5]));
			res = setup_finish(sess, ch, false, false);
			if (res < 0)
				return res;
			continue;
		}
		if (msg_type != SSH_MSG_CHANNEL_REQUEST) {
			res = setup_finish(sess, ch, false, false);
			if (res < 0)
				return res;
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
			res = setup_finish(sess, ch, false, false);
			int disconnect_ret = transport_disconnect_result(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
			    "malformed CHANNEL_REQUEST", DSSH_ERROR_TERMINATED);
			return res < 0 ? res : disconnect_ret;
		}

		if (rtype_len == DSSH_STRLEN(str_pty_req)
		    && memcmp(rtype, str_pty_req, DSSH_STRLEN(str_pty_req)) == 0) {
			if (had_pty) {
				res = setup_finish(sess, ch, false, false);
				int disconnect_ret = transport_disconnect_result(sess, SSH_DISCONNECT_PROTOCOL_ERROR,
				    "duplicate pty-req", DSSH_ERROR_TERMINATED);
				return res < 0 ? res : disconnect_ret;
			}
			had_pty = true;
			res     = accept_parse_pty(req_data, req_data_len, p);
			if (res < 0) {
				res = setup_finish(sess, ch, want_reply, false);
				if (res < 0)
					return res;
				continue;
			}
			int cb_res = 0;
			bool cancelled = false;
			if (cbs->pty_req != NULL)
				cb_res = call_setup_request(ch, p, cbs->pty_req, cbs->cbdata, &cancelled);
			if (cancelled)
				return cancel_setup_channel(sess, ch, want_reply);
			if (cb_res < 0)
				p->flags &= ~(uint32_t)DSSH_PARAM_HAS_PTY;
			res = setup_finish(sess, ch, want_reply, cb_res >= 0);
			if (res < 0)
				return res;
			continue;
		}
		if (rtype_len == DSSH_STRLEN(str_env) && memcmp(rtype, str_env, DSSH_STRLEN(str_env)) == 0) {
			bool cancelled = false;
			int cb_res = accept_env_request(ch, req_data, req_data_len, p, cbs, &cancelled);
			if (cancelled)
				return cancel_setup_channel(sess, ch, want_reply);
			res = setup_finish(sess, ch, want_reply, cb_res >= 0);
			if (res < 0)
				return res;
			continue;
		}
		if (rtype_len == DSSH_STRLEN(str_shell) && memcmp(rtype, str_shell, DSSH_STRLEN(str_shell)) == 0) {
			p->type      = DSSH_CHAN_SHELL;
			bool cancelled = false;
			int cb_res   = -1;
			if (req_data_len == 0 && cbs->shell != NULL)
				cb_res = call_setup_terminal(ch, p, result, cbs->shell, cbs->cbdata, &cancelled);
			if (cancelled)
				return cancel_setup_channel(sess, ch, want_reply);
			if (cb_res >= 0) {
				*terminal_want_reply = want_reply;
				return 0;
			}
			res = close_setup_channel(sess, ch, want_reply);
			if (res < 0 && res != DSSH_ERROR_TIMEOUT)
				return res;
			return DSSH_ERROR_REJECTED;
		}
		if (rtype_len == DSSH_STRLEN(str_exec) && memcmp(rtype, str_exec, DSSH_STRLEN(str_exec)) == 0) {
			p->type      = DSSH_CHAN_EXEC;
			res          = accept_parse_exec(req_data, req_data_len, p);
			bool cancelled = false;
			int cb_res   = -1;
			if (res >= 0 && cbs->exec != NULL)
				cb_res = call_setup_terminal(ch, p, result, cbs->exec, cbs->cbdata, &cancelled);
			if (cancelled)
				return cancel_setup_channel(sess, ch, want_reply);
			if (cb_res >= 0) {
				*terminal_want_reply = want_reply;
				return 0;
			}
			res = close_setup_channel(sess, ch, want_reply);
			if (res < 0 && res != DSSH_ERROR_TIMEOUT)
				return res;
			return DSSH_ERROR_REJECTED;
		}
		if (rtype_len == DSSH_STRLEN(str_subsystem)
		    && memcmp(rtype, str_subsystem, DSSH_STRLEN(str_subsystem)) == 0) {
			p->type      = DSSH_CHAN_SUBSYSTEM;
			res          = accept_parse_subsystem(req_data, req_data_len, p);
			bool cancelled = false;
			int cb_res   = -1;
			if (res >= 0 && cbs->subsystem != NULL)
				cb_res = call_setup_terminal(ch, p, result, cbs->subsystem, cbs->cbdata, &cancelled);
			if (cancelled)
				return cancel_setup_channel(sess, ch, want_reply);
			if (cb_res >= 0) {
				*terminal_want_reply = want_reply;
				return 0;
			}
			res = close_setup_channel(sess, ch, want_reply);
			if (res < 0 && res != DSSH_ERROR_TIMEOUT)
				return res;
			return DSSH_ERROR_REJECTED;
		}
		res = setup_finish(sess, ch, want_reply, false);
		if (res < 0)
			return res;
	}
}

static struct dssh_channel_s *
chan_accept_inner(struct dssh_session_s *sess, const struct dssh_chan_accept_cbs *cbs, int timeout_ms)
{
	if (!sess->conn_initialized || !sess->demux_running || sess->terminate)
		return NULL;

	for (;;) {
		struct dssh_incoming_open inc;
		int                       res = accept_incoming(sess, &inc, timeout_ms);

		if (res < 0)
			return NULL;

		struct dssh_channel_s *ch = accept_channel_init(sess, &inc);

		if (ch == NULL) {
			if (!sess->terminate) {
				res = send_open_failure(sess, inc.peer_channel, SSH_OPEN_RESOURCE_SHORTAGE,
				    inc.protocol_id);
				if (res < 0)
					session_set_terminate(sess);
			}
			return NULL;
		}

		struct dssh_chan_params p;

		res = params_init(&p, DSSH_CHAN_SHELL);
		p.flags &= ~(uint32_t)DSSH_PARAM_HAS_PTY;

		struct dssh_chan_accept_result result = {
		    .max_window = 0,
		    .zc_cb      = NULL,
		    .zc_cbdata  = NULL,
		};
		bool terminal_want_reply = false;

		if (res >= 0)
			res = chan_accept_setup_loop(sess, ch, cbs, &p, &result, &terminal_want_reply);
		if (res < 0) {
			if (res != DSSH_ERROR_REJECTED && !sess->terminate) {
				int close_ret = close_setup_channel(sess, ch, false);
				if (close_ret < 0 && close_ret != DSSH_ERROR_TIMEOUT)
					res = close_ret;
			}
			params_free(&p);
			if (discard_setup_channel(sess, ch) < 0)
				return NULL;
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

		int close_ret;
		struct dssh_bytebuf stdout_buffer = {0};
		struct dssh_bytebuf stderr_buffer = {0};
		if (ch->io_model == DSSH_IO_STREAM) {
#if SIZE_MAX < UINT32_MAX
			if (winsz > SIZE_MAX)
				goto accept_storage_failure;
#endif
			size_t buffer_size = (size_t)winsz;
			res = bytebuf_init(&stdout_buffer, buffer_size);
			if (res < 0)
				goto accept_storage_failure;
			res = bytebuf_init(&stderr_buffer, buffer_size);
			if (res < 0)
				goto accept_storage_failure;
		}
		res = mtx_lock(&ch->buf_mtx);
		if (res != thrd_success) {
			dssh_thrd_check(sess, res);
			goto accept_storage_failure;
		}
		if (sess->terminate || atomic_load(&ch->closing)) {
			dssh_mtx_unlock(&ch->buf_mtx);
			goto accept_storage_failure;
		}
		ch->params = p;
		p = (struct dssh_chan_params){0};
		ch->window_max = winsz;
		ch->chan_type = DSSH_CHAN_SESSION;
		ch->exit_code = 0;
		ch->exit_code_received = false;
		if (ch->io_model == DSSH_IO_STREAM) {
			ch->buf.stdout_buf = stdout_buffer;
			ch->buf.stderr_buf = stderr_buffer;
			stdout_buffer = (struct dssh_bytebuf){0};
			stderr_buffer = (struct dssh_bytebuf){0};
		}
		/* The accepted terminal request remains the occupied setup-mailbox
		 * entry until initialization is complete.  Clear it and switch modes
		 * atomically so a later packet either waits here or sees a fully
		 * initialized normal channel. */
		ch->setup_ready = false;
		ch->setup_payload_len = 0;
		ch->setup_mode = false;
		if (setup_publish_and_unlock(sess, ch) < 0)
			goto accept_committed_failure;

		if (terminal_want_reply)
			res = setup_reply(sess, ch, true);
		else
			res = 0;
		if (res < 0)
			goto accept_committed_failure;

		/* Open the window -- data can flow now */
		res = send_window_adjust(sess, ch, winsz);
		if (res < 0)
			goto accept_committed_failure;

		return ch;

accept_storage_failure:
		bytebuf_free(&stdout_buffer);
		bytebuf_free(&stderr_buffer);
		/* A validated terminal request is still occupied and has not yet
		 * received success. Reject it if requested, then exchange CLOSE. */
		close_ret = close_setup_channel(sess, ch, terminal_want_reply);
		goto accept_retire_failure;

accept_committed_failure:
		close_ret = DSSH_ERROR_TERMINATED;
		if (!sess->terminate) {
			close_ret = conn_close(sess, ch);
			if (close_ret < 0)
				session_set_terminate(sess);
			else
				close_ret = wait_for_close(sess, ch);
		}
accept_retire_failure:
		params_free(&p);
		if (close_ret < 0 && close_ret != DSSH_ERROR_TIMEOUT && !sess->terminate)
			session_set_terminate(sess);
		if (discard_setup_channel(sess, ch) < 0)
			return NULL;
		return NULL;
	} /* accept loop */
}

DSSH_PUBLIC struct dssh_channel_s *
dssh_chan_accept(struct dssh_session_s *sess, const struct dssh_chan_accept_cbs *cbs, int timeout_ms)
{
	if (sess == NULL || cbs == NULL)
		return NULL;
	if (in_zc_rx || connection_api_forbidden(sess))
		return NULL;
	struct dssh_session_scope scope;

	session_scope_enter(&scope, sess);
	struct dssh_channel_s *ch = chan_accept_inner(sess, cbs, timeout_ms);
	session_scope_leave(&scope);
	return ch;
}
