/*
 * ssh-internal.h -- Internal struct definitions.
 * Not a public header.  Included by library .c files only.
 */
#ifndef DSSH_INTERNAL_H
#define DSSH_INTERNAL_H

#include <stdatomic.h>
#include <threads.h>

#include "ssh-trans.h"
#include "deucessh-conn.h"

/* DSSH_TESTABLE is now defined in deucessh-portable.h */

/* ================================================================
 * Channel buffer structures (formerly ssh-chan.h)
 * ================================================================ */

/* Circular byte buffer (session channel stdout/stderr) */
struct dssh_bytebuf {
	uint8_t *data;
	size_t   capacity;
	size_t   head;   /* next read position */
	size_t   tail;   /* next write position */
	size_t   used;   /* bytes available to read */
	size_t   total;  /* total bytes ever written (for signal marks) */
};

/* Message queue entry (raw channel data) */
struct dssh_msgqueue_entry {
	struct dssh_msgqueue_entry *next;
	size_t                      len;
	uint8_t                     data[];
};

struct dssh_msgqueue {
	struct dssh_msgqueue_entry *head;
	struct dssh_msgqueue_entry *tail;
	size_t                      total_bytes;
	size_t                      count;
};

/* Signal queue with stream position marks */
struct dssh_signal_mark {
	struct dssh_signal_mark *next;
	size_t                   stdout_pos;
	size_t                   stderr_pos;
	char                     name[32];
};

struct dssh_signal_queue {
	struct dssh_signal_mark *head;
	struct dssh_signal_mark *tail;
};

/* Event queue for the new dssh_chan_* API (circular buffer).
 * Events are pushed by the demux thread, pulled by the app via
 * dssh_chan_poll(DSSH_POLL_EVENT) + dssh_chan_read_event(). */
struct dssh_event_entry {
	/* Pointer-sized */
	size_t   stdout_pos;
	size_t   stderr_pos;
	/* 4-byte */
	int      type;
	uint32_t u32_a;  /* exit_code, cols, break length */
	uint32_t u32_b;  /* rows */
	uint32_t u32_c;  /* wpx */
	uint32_t u32_d;  /* hpx */
	/* 1-byte */
	bool     flag_a; /* core_dumped */
	/* Char arrays */
	char     str_a[64];  /* signal name */
	char     str_b[256]; /* error message */
};

struct dssh_event_queue {
	/* Pointers */
	struct dssh_event_entry *entries;
	/* Pointer-sized */
	size_t                   capacity;
	size_t                   head;
	size_t                   tail;
	size_t                   count;
	/* Embedded struct (large) */
	struct dssh_event_entry  frozen; /* frozen event for poll/read_event */
	/* 1-byte */
	bool                     has_frozen;
};

/* Incoming channel open queue (for session_accept) */
struct dssh_incoming_open {
	struct dssh_incoming_open *next;
	size_t                     channel_type_len;
	uint32_t                   peer_channel;
	uint32_t                   peer_window;
	uint32_t                   peer_max_packet;
	char                       channel_type[64];
};

struct dssh_accept_queue {
	struct dssh_incoming_open *head;
	struct dssh_incoming_open *tail;
};

/* Channel types */
#define DSSH_CHAN_SESSION 1
#define DSSH_CHAN_RAW 2

/* Wire-length of a string literal (excludes NUL terminator) */
#define DSSH_STRLEN(lit) ((uint32_t)(sizeof(lit) - 1))

/* Unchecked big-endian uint32 read — caller ensures buf has >= 4 bytes */
#define DSSH_GET_U32(buf) \
	(((uint32_t)(buf)[0] << 24) | ((uint32_t)(buf)[1] << 16) | \
	 ((uint32_t)(buf)[2] << 8) | (uint32_t)(buf)[3])

/* Unchecked big-endian uint32 write — caller ensures buf has >= 4 bytes past *pos */
#define DSSH_PUT_U32(val, buf, pos) do { \
	uint32_t dssh_put_v_ = (val); \
	(buf)[(*(pos))++] = (uint8_t)(dssh_put_v_ >> 24); \
	(buf)[(*(pos))++] = (uint8_t)(dssh_put_v_ >> 16); \
	(buf)[(*(pos))++] = (uint8_t)(dssh_put_v_ >> 8); \
	(buf)[(*(pos))++] = (uint8_t)(dssh_put_v_); \
} while (0)

/* Set terminate flag and wake all library-owned condvar waiters. */
DSSH_PRIVATE void session_set_terminate(struct dssh_session_s *sess);

/* Forward declaration of public function called from internal code
 * (ssh-trans.c cleanup path).  Avoids needing deucessh-conn.h. */
DSSH_PUBLIC void dssh_session_stop(struct dssh_session_s *sess);

/*
 * Test allocation redirection.  Under DSSH_TESTING, library code
 * calls dssh_test_malloc/calloc/realloc instead of the real
 * allocators.  Algorithm module files must include ssh-internal.h
 * to get these redirects.
 *
 * free() is NOT redirected -- real free() handles all pointers
 * since dssh_test_malloc returns real malloc'd memory.
 */
#ifdef DSSH_TESTING
void *dssh_test_malloc(size_t sz);
void *dssh_test_calloc(size_t nmemb, size_t sz);
void *dssh_test_realloc(void *ptr, size_t sz);

 #define malloc(x) dssh_test_malloc(x)
 #define calloc(n, s) dssh_test_calloc(n, s)
 #define realloc(p, s) dssh_test_realloc(p, s)

/* C11 threads */
int dssh_test_mtx_init(mtx_t *mtx, int type);
int dssh_test_cnd_init(cnd_t *cond);
int dssh_test_thrd_create(thrd_t *thr, thrd_start_t func, void *arg);

 #define mtx_init(m, t) dssh_test_mtx_init(m, t)
 #define cnd_init(c) dssh_test_cnd_init(c)
 #define thrd_create(t, f, a) dssh_test_thrd_create(t, f, a)

int dssh_test_mtx_lock(mtx_t *mtx);
int dssh_test_mtx_unlock(mtx_t *mtx);
int dssh_test_cnd_wait(cnd_t *cond, mtx_t *mtx);
int dssh_test_cnd_timedwait(cnd_t *cond, mtx_t *mtx,
    const struct timespec *ts);
int dssh_test_cnd_broadcast(cnd_t *cond);
int dssh_test_cnd_signal(cnd_t *cond);

 #define mtx_lock(m) dssh_test_mtx_lock(m)
 #define mtx_unlock(m) dssh_test_mtx_unlock(m)
 #define cnd_wait(c, m) dssh_test_cnd_wait(c, m)
 #define cnd_timedwait(c, m, t) dssh_test_cnd_timedwait(c, m, t)
 #define cnd_broadcast(c) dssh_test_cnd_broadcast(c)
 #define cnd_signal(c) dssh_test_cnd_signal(c)

/* OpenSSL */
int dssh_test_BN_rand(BIGNUM *rnd, int bits, int top, int bottom);
int dssh_test_EVP_PKEY_CTX_set_rsa_padding(EVP_PKEY_CTX *ctx, int pad_mode);

 #define BN_rand(r, b, t, bo) dssh_test_BN_rand(r, b, t, bo)
 #define EVP_PKEY_CTX_set_rsa_padding(c, p) dssh_test_EVP_PKEY_CTX_set_rsa_padding(c, p)
int dssh_test_EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *ctx, int pad);
 #define EVP_CIPHER_CTX_set_padding(c, p) dssh_test_EVP_CIPHER_CTX_set_padding(c, p)

#endif /* DSSH_TESTING */

/* ================================================================
 * Protocol constants
 * ================================================================ */

/* RFC 4253 s4.2: identification string max length (including SSH-2.0-
 * prefix and CR LF terminator) */
#define DSSH_VERSION_STRING_MAX 255

/* RFC 4253 s7.1: KEXINIT cookie is 16 random bytes */
#define DSSH_KEXINIT_COOKIE_SIZE 16

/* RFC 4253 s7.1: KEXINIT contains 10 algorithm name-lists */
#define DSSH_KEXINIT_NAMELIST_COUNT 10

/* RFC 4251 s6: individual algorithm names MUST NOT exceed 64 chars */
#define DSSH_ALGO_NAME_MAX 64

/* Practical limit for disconnect description length (clamped, not
 * rejected).  Ensures the DISCONNECT message fits a 256-byte buffer
 * with room for msg_type, reason, and empty language tag. */
#define DSSH_DISCONNECT_DESC_MAX 230

/* RFC 4251 s6: names must not contain DEL */
#define DSSH_ASCII_DEL 127

/* Practical buffer sizes for algorithm name-lists and per-channel
 * request data.  See README.md "Limits". */
#define DSSH_NAMELIST_BUF_SIZE  1024
#define DSSH_REQ_DATA_BUF_SIZE  1024

/* Channel types: defined in ssh-chan.h (included above) */

/* SSH connection protocol message types (RFC 4254) */
#define SSH_MSG_GLOBAL_REQUEST UINT8_C(80)
#define SSH_MSG_REQUEST_SUCCESS UINT8_C(81)
#define SSH_MSG_REQUEST_FAILURE UINT8_C(82)
#define SSH_MSG_CHANNEL_OPEN UINT8_C(90)
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION UINT8_C(91)
#define SSH_MSG_CHANNEL_OPEN_FAILURE UINT8_C(92)
#define SSH_MSG_CHANNEL_WINDOW_ADJUST UINT8_C(93)
#define SSH_MSG_CHANNEL_DATA UINT8_C(94)
#define SSH_MSG_CHANNEL_EXTENDED_DATA UINT8_C(95)
#define SSH_MSG_CHANNEL_EOF UINT8_C(96)
#define SSH_MSG_CHANNEL_CLOSE UINT8_C(97)
#define SSH_MSG_CHANNEL_REQUEST UINT8_C(98)
#define SSH_MSG_CHANNEL_SUCCESS UINT8_C(99)
#define SSH_MSG_CHANNEL_FAILURE UINT8_C(100)

/* SSH_OPEN_* reason codes: defined in ssh-conn.c (only consumer) */

/* I/O model for channel dispatch (old API vs new dssh_chan_* API) */
#define DSSH_IO_OLD    0  /* old dssh_session_ / dssh_channel_ API */
#define DSSH_IO_STREAM 1  /* new dssh_chan_ stream API */
#define DSSH_IO_ZC     2  /* new dssh_chan_zc_ zero-copy API */

struct dssh_channel_s {
        /* Pointers and size_t (pointer-sized) */
	struct dssh_session_s *sess; /* back-pointer (new API takes ch only) */
	uint8_t *setup_payload;
	size_t   setup_payload_len;
	size_t   stdout_consumed;
	size_t   stderr_consumed;
	void     (*window_change_cb)(uint32_t cols, uint32_t rows,
	    uint32_t wpx, uint32_t hpx, void *cbdata);
	void    *window_change_cbdata;
	dssh_chan_zc_cb    zc_cb;      /* ZC data callback (or internal for stream) */
	void             *zc_cbdata;
	dssh_chan_event_cb event_cb;   /* optional event push callback */
	void             *event_cbdata;

        /* C11 synchronization (platform-dependent size).
         * Lock order: channel_mtx -> buf_mtx -> cb_mtx -> tx_mtx. */
	mtx_t    buf_mtx;
	mtx_t    cb_mtx;  /* protects callback function pointer + cbdata pairs */
	cnd_t    poll_cnd;
	atomic_bool closing;  /* set by close functions before cleanup */

        /* 4-byte */
	uint32_t local_id;
	uint32_t remote_id;
	uint32_t local_window;
	atomic_uint_least32_t remote_window;
	uint32_t remote_max_packet;
	uint32_t window_max;
	uint32_t exit_code;
	int      chan_type;
	int      io_model;  /* DSSH_IO_OLD, DSSH_IO_STREAM, DSSH_IO_ZC */
	int      zc_stream; /* stashed stream from zc_getbuf */

        /* 1-byte */
	bool     open;
	bool     close_sent;
	bool     close_received;
	bool     eof_sent;
	bool     eof_received;
	bool     exit_code_received;
	bool     request_pending;
	bool     request_responded;
	bool     request_success;
	bool     setup_mode;
	bool     setup_ready;
	bool     setup_error;
	uint8_t  setup_msg_type;

        /* Per-channel string buffers (avoids static storage) */
	char     last_signal[32];
	char     req_type[32];
	char     req_data[DSSH_REQ_DATA_BUF_SIZE];

        /* Embedded structs (contain pointers, large) */
	struct dssh_chan_params params;  /* new API: getters after open/accept */
	struct dssh_event_queue events; /* new API: event queue */

	union {
		struct {
			struct dssh_bytebuf      stdout_buf;
			struct dssh_bytebuf      stderr_buf;
			struct dssh_signal_queue signals;
		} session;
		struct {
			struct dssh_msgqueue queue;
		} raw;
	} buf;
};

struct dssh_session_s {
        /* Pointers and size_t (pointer-sized) -- callbacks and cbdata */
	void                         *tx_cbdata;
	void                         *rx_cbdata;
	void                         *rx_line_cbdata;
	void                         *extra_line_cbdata;
	void (*debug_cb)(bool always_display,
	    const uint8_t *message, size_t message_len, void *cbdata);
	void                         *debug_cbdata;
	void (*unimplemented_cb)(uint32_t rejected_seq, void *cbdata);
	void                         *unimplemented_cbdata;
	void (*banner_cb)(const uint8_t *message,
	    size_t message_len, const uint8_t *language,
	    size_t language_len, void *cbdata);
	void                         *banner_cbdata;
	char                         *pending_banner;
	char                         *pending_banner_lang;
	int (*global_request_cb)(const uint8_t *name,
	    size_t name_len, bool want_reply, const uint8_t *data,
	    size_t data_len, void *cbdata);
	void                         *global_request_cbdata;
	void (*terminate_cb)(struct dssh_session_s *sess, void *cbdata);
	void                         *terminate_cbdata;
	dssh_chan_event_cb            default_event_cb;
	void                         *default_event_cbdata;
	struct dssh_channel_s       **channels;
	size_t                        channel_count;
	size_t                        channel_capacity;

        /* Embedded transport state (large, naturally aligned) */
	struct dssh_transport_state_s trans;

        /* Incoming channel open queue */
	struct dssh_accept_queue      accept_queue;

        /* C11 synchronization (platform-dependent size).
         * Lock order: channel_mtx then buf_mtx (never reversed). */
	mtx_t                         mtx;
	mtx_t                         channel_mtx;
	mtx_t                         accept_mtx;
	cnd_t                         accept_cnd;
	thrd_t                        demux_thread;

        /* 4-byte */
	uint32_t                      next_channel_id;
	int                           timeout_ms;

        /* 1-byte */
	atomic_bool                   initialized;
	atomic_bool                   terminate;
	atomic_bool                   demux_running;
	atomic_bool                   conn_initialized;
	bool                          auth_service_requested;
};

/*
 * Generic C11 thread-call checker.  Call the real function, check the
 * result.  thrd_success, thrd_busy, and thrd_timedout are non-errors;
 * anything else (thrd_error, thrd_nomem) triggers session termination.
 *
 * Library code ignores the return — the wrapper handles set_terminate.
 * Exception: set_terminate() itself checks returns to skip blocks whose
 * lock was not acquired.
 */
static inline int
dssh_thrd_check(struct dssh_session_s *sess, int ret)
{
	if (ret != thrd_success && ret != thrd_busy
	    && ret != thrd_timedout && !sess->terminate)
		session_set_terminate(sess);
	return ret;
}

/* Convert a millisecond timeout to an absolute deadline timespec.
 * Caller must ensure timeout_ms > 0. */
static inline void
dssh_deadline_from_ms(struct timespec *ts, int timeout_ms)
{
	timespec_get(ts, TIME_UTC);
	ts->tv_sec += timeout_ms / 1000;
	ts->tv_nsec += (timeout_ms % 1000) * 1000000L;
	if (ts->tv_nsec >= 1000000000L) {
		ts->tv_sec++;
		ts->tv_nsec -= 1000000000L;
	}
}

#endif // ifndef DSSH_INTERNAL_H
