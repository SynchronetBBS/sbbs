/*
 * ssh-chan.h — Internal channel buffer structures for the demux layer.
 *
 * Not a public header.  Included by ssh-conn.h for the channel struct
 * and by the demux implementation.
 */

#ifndef DSSH_CHAN_H
#define DSSH_CHAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <threads.h>

/* ================================================================
 * Circular byte buffer (session channel stdout/stderr)
 * ================================================================ */

struct dssh_bytebuf {
	uint8_t *data;
	size_t capacity;
	size_t head;     /* next read position */
	size_t tail;     /* next write position */
	size_t used;     /* bytes available to read */
	size_t total;    /* total bytes ever written (for signal marks) */
};

DSSH_PRIVATE int  dssh_bytebuf_init(struct dssh_bytebuf *b, size_t capacity);
DSSH_PRIVATE void dssh_bytebuf_free(struct dssh_bytebuf *b);
/* Returns bytes actually written (may be less than len if full) */
DSSH_PRIVATE size_t dssh_bytebuf_write(struct dssh_bytebuf *b,
    const uint8_t *data, size_t len);
/* Returns bytes actually read (may be less than bufsz if empty).
 * limit: max bytes to read (0 = no limit beyond buffer contents) */
DSSH_PRIVATE size_t dssh_bytebuf_read(struct dssh_bytebuf *b,
    uint8_t *buf, size_t bufsz, size_t limit);
/* Returns number of bytes available to read */
DSSH_PRIVATE size_t dssh_bytebuf_available(const struct dssh_bytebuf *b);
/* Returns free space */
DSSH_PRIVATE size_t dssh_bytebuf_free_space(const struct dssh_bytebuf *b);

/* ================================================================
 * Message queue (raw channel data)
 * ================================================================ */

struct dssh_msgqueue_entry {
	struct dssh_msgqueue_entry *next;
	size_t len;
	uint8_t data[];
};

struct dssh_msgqueue {
	struct dssh_msgqueue_entry *head;
	struct dssh_msgqueue_entry *tail;
	size_t total_bytes;   /* sum of all queued message lengths */
	size_t count;         /* number of messages */
};

DSSH_PRIVATE void dssh_msgqueue_init(struct dssh_msgqueue *q);
DSSH_PRIVATE void dssh_msgqueue_free(struct dssh_msgqueue *q);
/* Enqueue a message (copies data). Returns 0 on success. */
DSSH_PRIVATE int  dssh_msgqueue_push(struct dssh_msgqueue *q,
    const uint8_t *data, size_t len);
/* Peek at the next message size. Returns 0 if empty. */
DSSH_PRIVATE size_t dssh_msgqueue_peek_size(const struct dssh_msgqueue *q);
/* Dequeue a message into buf. Returns message length, or
 * DSSH_ERROR_TOOLONG if bufsz too small (message stays queued),
 * or 0 if empty. */
DSSH_PRIVATE int64_t dssh_msgqueue_pop(struct dssh_msgqueue *q,
    uint8_t *buf, size_t bufsz);

/* ================================================================
 * Signal queue with stream position marks
 * ================================================================ */

struct dssh_signal_mark {
	struct dssh_signal_mark *next;
	size_t stdout_pos;    /* bytebuf total at time of signal */
	size_t stderr_pos;    /* bytebuf total at time of signal */
	char name[32];        /* signal name without "SIG" prefix */
};

struct dssh_signal_queue {
	struct dssh_signal_mark *head;
	struct dssh_signal_mark *tail;
};

DSSH_PRIVATE void dssh_sigqueue_init(struct dssh_signal_queue *q);
DSSH_PRIVATE void dssh_sigqueue_free(struct dssh_signal_queue *q);
/* Record a signal at the current stream positions */
DSSH_PRIVATE int  dssh_sigqueue_push(struct dssh_signal_queue *q,
    const char *name, size_t stdout_pos, size_t stderr_pos);
/* Check if the front signal is ready (both streams drained past mark) */
DSSH_PRIVATE bool dssh_sigqueue_ready(const struct dssh_signal_queue *q,
    size_t stdout_consumed, size_t stderr_consumed);
/* Consume the front signal. Copies name into buf (bufsz >= 32).
 * Returns buf on success, or NULL if not ready. */
DSSH_PRIVATE const char *dssh_sigqueue_pop(struct dssh_signal_queue *q,
    size_t stdout_consumed, size_t stderr_consumed,
    char *buf, size_t bufsz);

/* ================================================================
 * Incoming channel open queue (for session_accept)
 * ================================================================ */

struct dssh_incoming_open {
	struct dssh_incoming_open *next;
	uint32_t peer_channel;
	uint32_t peer_window;
	uint32_t peer_max_packet;
	size_t channel_type_len;
	char channel_type[64];
};

struct dssh_accept_queue {
	struct dssh_incoming_open *head;
	struct dssh_incoming_open *tail;
};

DSSH_PRIVATE void dssh_acceptqueue_init(struct dssh_accept_queue *q);
DSSH_PRIVATE void dssh_acceptqueue_free(struct dssh_accept_queue *q);
DSSH_PRIVATE int  dssh_acceptqueue_push(struct dssh_accept_queue *q,
    uint32_t peer_channel, uint32_t peer_window,
    uint32_t peer_max_packet,
    const uint8_t *type, size_t type_len);
DSSH_PRIVATE struct dssh_incoming_open *dssh_acceptqueue_pop(
    struct dssh_accept_queue *q);

/* ================================================================
 * Channel types
 * ================================================================ */

#define DSSH_CHAN_SESSION  1
#define DSSH_CHAN_RAW     2

#endif
