/*
 * ssh-chan.c -- Internal channel buffer implementations.
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "ssh-chan.h"

/* ================================================================
 * Circular byte buffer
 * ================================================================ */
DSSH_PRIVATE int
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

DSSH_PRIVATE void
bytebuf_free(struct dssh_bytebuf *b)
{
	free(b->data);
	b->data = NULL;
	b->capacity = 0;
	b->head = 0;
	b->tail = 0;
	b->used = 0;
}

DSSH_PRIVATE size_t
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

DSSH_PRIVATE size_t
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

DSSH_PRIVATE size_t
bytebuf_available(const struct dssh_bytebuf *b)
{
	return b->used;
}

DSSH_PRIVATE size_t
bytebuf_free_space(const struct dssh_bytebuf *b)
{
	return b->capacity - b->used;
}

/* ================================================================
 * Message queue
 * ================================================================ */
DSSH_PRIVATE void
msgqueue_init(struct dssh_msgqueue *q)
{
	q->head = NULL;
	q->tail = NULL;
	q->total_bytes = 0;
	q->count = 0;
}

DSSH_PRIVATE void
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

DSSH_PRIVATE int
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

DSSH_PRIVATE int64_t
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

/* ================================================================
 * Signal queue
 * ================================================================ */
DSSH_PRIVATE void
sigqueue_init(struct dssh_signal_queue *q)
{
	q->head = NULL;
	q->tail = NULL;
}

DSSH_PRIVATE void
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

DSSH_PRIVATE int
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

DSSH_PRIVATE bool
sigqueue_ready(const struct dssh_signal_queue *q,
    size_t stdout_consumed, size_t stderr_consumed)
{
	if (q->head == NULL)
		return false;
	return stdout_consumed >= q->head->stdout_pos
	       && stderr_consumed >= q->head->stderr_pos;
}

DSSH_PRIVATE const char *
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

/* ================================================================
 * Accept queue
 * ================================================================ */
DSSH_PRIVATE void
acceptqueue_init(struct dssh_accept_queue *q)
{
	q->head = NULL;
	q->tail = NULL;
}

DSSH_PRIVATE void
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

DSSH_PRIVATE int
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

DSSH_PRIVATE struct dssh_incoming_open *

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
