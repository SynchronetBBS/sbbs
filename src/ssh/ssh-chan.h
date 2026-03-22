/*
 * ssh-chan.h — Internal channel buffer structures for the demux layer.
 *
 * Not a public header.  Included by ssh-conn.h for the channel struct
 * and by the demux implementation.
 */

#ifndef DEUCE_SSH_CHAN_H
#define DEUCE_SSH_CHAN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <threads.h>

/* ================================================================
 * Circular byte buffer (session channel stdout/stderr)
 * ================================================================ */

struct deuce_ssh_bytebuf {
	uint8_t *data;
	size_t capacity;
	size_t head;     /* next read position */
	size_t tail;     /* next write position */
	size_t used;     /* bytes available to read */
	size_t total;    /* total bytes ever written (for signal marks) */
};

DEUCE_SSH_PRIVATE int  deuce_ssh_bytebuf_init(struct deuce_ssh_bytebuf *b, size_t capacity);
DEUCE_SSH_PRIVATE void deuce_ssh_bytebuf_free(struct deuce_ssh_bytebuf *b);
/* Returns bytes actually written (may be less than len if full) */
DEUCE_SSH_PRIVATE size_t deuce_ssh_bytebuf_write(struct deuce_ssh_bytebuf *b,
    const uint8_t *data, size_t len);
/* Returns bytes actually read (may be less than bufsz if empty).
 * limit: max bytes to read (0 = no limit beyond buffer contents) */
DEUCE_SSH_PRIVATE size_t deuce_ssh_bytebuf_read(struct deuce_ssh_bytebuf *b,
    uint8_t *buf, size_t bufsz, size_t limit);
/* Returns number of bytes available to read */
DEUCE_SSH_PRIVATE size_t deuce_ssh_bytebuf_available(const struct deuce_ssh_bytebuf *b);
/* Returns free space */
DEUCE_SSH_PRIVATE size_t deuce_ssh_bytebuf_free_space(const struct deuce_ssh_bytebuf *b);

/* ================================================================
 * Message queue (raw channel data)
 * ================================================================ */

struct deuce_ssh_msgqueue_entry {
	struct deuce_ssh_msgqueue_entry *next;
	size_t len;
	uint8_t data[];
};

struct deuce_ssh_msgqueue {
	struct deuce_ssh_msgqueue_entry *head;
	struct deuce_ssh_msgqueue_entry *tail;
	size_t total_bytes;   /* sum of all queued message lengths */
	size_t count;         /* number of messages */
};

DEUCE_SSH_PRIVATE void deuce_ssh_msgqueue_init(struct deuce_ssh_msgqueue *q);
DEUCE_SSH_PRIVATE void deuce_ssh_msgqueue_free(struct deuce_ssh_msgqueue *q);
/* Enqueue a message (copies data). Returns 0 on success. */
DEUCE_SSH_PRIVATE int  deuce_ssh_msgqueue_push(struct deuce_ssh_msgqueue *q,
    const uint8_t *data, size_t len);
/* Peek at the next message size. Returns 0 if empty. */
DEUCE_SSH_PRIVATE size_t deuce_ssh_msgqueue_peek_size(const struct deuce_ssh_msgqueue *q);
/* Dequeue a message into buf. Returns message length, or
 * DEUCE_SSH_ERROR_TOOLONG if bufsz too small (message stays queued),
 * or 0 if empty. */
DEUCE_SSH_PRIVATE ssize_t deuce_ssh_msgqueue_pop(struct deuce_ssh_msgqueue *q,
    uint8_t *buf, size_t bufsz);

/* ================================================================
 * Signal queue with stream position marks
 * ================================================================ */

struct deuce_ssh_signal_mark {
	struct deuce_ssh_signal_mark *next;
	size_t stdout_pos;    /* bytebuf total at time of signal */
	size_t stderr_pos;    /* bytebuf total at time of signal */
	char name[32];        /* signal name without "SIG" prefix */
};

struct deuce_ssh_signal_queue {
	struct deuce_ssh_signal_mark *head;
	struct deuce_ssh_signal_mark *tail;
};

DEUCE_SSH_PRIVATE void deuce_ssh_sigqueue_init(struct deuce_ssh_signal_queue *q);
DEUCE_SSH_PRIVATE void deuce_ssh_sigqueue_free(struct deuce_ssh_signal_queue *q);
/* Record a signal at the current stream positions */
DEUCE_SSH_PRIVATE int  deuce_ssh_sigqueue_push(struct deuce_ssh_signal_queue *q,
    const char *name, size_t stdout_pos, size_t stderr_pos);
/* Check if the front signal is ready (both streams drained past mark) */
DEUCE_SSH_PRIVATE bool deuce_ssh_sigqueue_ready(const struct deuce_ssh_signal_queue *q,
    size_t stdout_consumed, size_t stderr_consumed);
/* Consume the front signal. Copies name into buf (bufsz >= 32).
 * Returns buf on success, or NULL if not ready. */
DEUCE_SSH_PRIVATE const char *deuce_ssh_sigqueue_pop(struct deuce_ssh_signal_queue *q,
    size_t stdout_consumed, size_t stderr_consumed,
    char *buf, size_t bufsz);

/* ================================================================
 * Incoming channel open queue (for session_accept)
 * ================================================================ */

struct deuce_ssh_incoming_open {
	struct deuce_ssh_incoming_open *next;
	uint32_t peer_channel;
	uint32_t peer_window;
	uint32_t peer_max_packet;
	size_t channel_type_len;
	char channel_type[64];
};

struct deuce_ssh_accept_queue {
	struct deuce_ssh_incoming_open *head;
	struct deuce_ssh_incoming_open *tail;
};

DEUCE_SSH_PRIVATE void deuce_ssh_acceptqueue_init(struct deuce_ssh_accept_queue *q);
DEUCE_SSH_PRIVATE void deuce_ssh_acceptqueue_free(struct deuce_ssh_accept_queue *q);
DEUCE_SSH_PRIVATE int  deuce_ssh_acceptqueue_push(struct deuce_ssh_accept_queue *q,
    uint32_t peer_channel, uint32_t peer_window,
    uint32_t peer_max_packet,
    const uint8_t *type, size_t type_len);
DEUCE_SSH_PRIVATE struct deuce_ssh_incoming_open *deuce_ssh_acceptqueue_pop(
    struct deuce_ssh_accept_queue *q);

/* ================================================================
 * Channel types
 * ================================================================ */

#define DEUCE_SSH_CHAN_SESSION  1
#define DEUCE_SSH_CHAN_RAW     2

#endif
