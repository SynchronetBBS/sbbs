/*
 * ssh-internal.h -- Internal struct definitions.
 * Not a public header.  Included by library .c files only.
 */
#ifndef DSSH_INTERNAL_H
#define DSSH_INTERNAL_H

#include <stdatomic.h>
#include <stdlib.h>
#include <threads.h>
#include "ssh-time.h"
#include "ssh-timespec.h" /* timespec_get / TIME_UTC — dssh_deadline_from_ms */

#include "deucessh-conn.h"
#include "ssh-trans.h"

/* DSSH_TESTABLE is now defined in deucessh-portable.h */

/* Optional packet metadata uses the application's local diagnostic callback.
 * No direct I/O or peer forwarding. Default builds do not evaluate arguments. */
#ifdef DSSH_TRACE_WIRE
 #define DSSH_TRACE_W(sess, ...)                                      \
	do {                                                          \
		if (dssh_log_enabled((sess), DSSH_LOG_DEBUG))            \
			dssh_log_emit_localf((sess), DSSH_LOG_DEBUG,     \
			    DSSH_ERROR_NONE, "[dssh-wire] " __VA_ARGS__); \
	} while (0)
#else
 #define DSSH_TRACE_W(sess, ...)                                      \
	do {                                                          \
		if (0)                                                 \
			dssh_log_emit_localf((sess), DSSH_LOG_DEBUG,     \
			    DSSH_ERROR_NONE, "[dssh-wire] " __VA_ARGS__); \
	} while (0)
#endif

/* ================================================================
 * Channel buffer structures (formerly ssh-chan.h)
 * ================================================================ */

/* Circular byte buffer (session channel stdout/stderr) */
struct dssh_bytebuf {
	uint8_t *data;
	size_t   capacity;
	size_t   head;  /* next read position */
	size_t   tail;  /* next write position */
	size_t   used;  /* bytes available to read */
	size_t   total; /* total bytes ever written */
};

#define DSSH_DEFAULT_MAX_EVENTS 64

/* Event queue for the dssh_chan_* API (circular buffer).
 * Events are pushed by the demux thread, pulled by the app via
 * dssh_chan_poll(DSSH_POLL_EVENT) + dssh_chan_read_event(). */
struct dssh_event_entry {
	/* Pointer-sized */
	size_t stdout_pos;
	size_t stderr_pos;
	/* 4-byte */
	int      type;
	uint32_t u32_a; /* exit_code, cols, break length */
	uint32_t u32_b; /* rows */
	uint32_t u32_c; /* wpx */
	uint32_t u32_d; /* hpx */
	/* 1-byte */
	bool flag_a; /* core_dumped */
	/* Borrowed packet views for callbacks; retained slot views for polling. */
	struct dssh_bytes text_a;
	struct dssh_bytes text_b;
	struct dssh_bytes language;
	uint8_t *storage;
	size_t storage_capacity;
};

struct dssh_event_queue {
	/* Pointers */
	struct dssh_event_entry *entries;
	/* Pointer-sized */
	size_t capacity;
	size_t max_events; /* 0 = no event-count cap; memory remains bounded */
	size_t packet_limit;
	size_t text_capacity; /* all slot/frozen retained allocations */
	size_t head;
	size_t tail;
	size_t count;
	/* Embedded struct (large) */
	struct dssh_event_entry frozen; /* frozen event for poll/read_event */
	/* 1-byte */
	bool has_frozen;
};

/* Incoming channel open queue (for dssh_chan_accept).
 * Fixed-capacity ring buffer. Each admitted open owns a protocol record.
 * The demux rejects new opens at capacity and continues dispatching channels. */
#define DSSH_ACCEPT_QUEUE_CAP 8
/* Stop admitting channels while this many detached IDs await retirement. */
#define DSSH_RETIRED_CHANNEL_ID_CAP 1024

struct dssh_incoming_open {
	struct dssh_channel_id *protocol_id;
	size_t   channel_type_len;
	uint32_t peer_channel;
	uint32_t peer_window;
	uint32_t peer_max_packet;
	char     channel_type[64];
};

struct dssh_accept_queue {
	struct dssh_incoming_open entries[DSSH_ACCEPT_QUEUE_CAP];
	size_t                    head;  /* next pop index  */
	size_t                    count; /* current depth   */
};

/* Channel types */
#define DSSH_CHAN_SESSION 1

/* Wire-length of a string literal (excludes NUL terminator) */
#define DSSH_STRLEN(lit) ((uint32_t)(sizeof(lit) - 1))

/* Unchecked big-endian uint32 read — caller ensures buf has >= 4 bytes */
#define DSSH_GET_U32(buf) \
	(((uint32_t)(buf)[0] << 24) | ((uint32_t)(buf)[1] << 16) | ((uint32_t)(buf)[2] << 8) | (uint32_t)(buf)[3])

/* Unchecked big-endian uint32 write — caller ensures buf has >= 4 bytes past *pos */
#define DSSH_PUT_U32(val, buf, pos)                                  \
	do {                                                         \
		uint32_t dssh_put_v_ = (val);                        \
		(buf)[(*(pos))++]    = (uint8_t)(dssh_put_v_ >> 24); \
		(buf)[(*(pos))++]    = (uint8_t)(dssh_put_v_ >> 16); \
		(buf)[(*(pos))++]    = (uint8_t)(dssh_put_v_ >> 8);  \
		(buf)[(*(pos))++]    = (uint8_t)(dssh_put_v_);       \
	} while (0)

/* Set terminate flag and wake all library-owned condvar waiters. */
DSSH_PRIVATE void session_set_terminate(struct dssh_session_s *sess);
/* Mark terminated while logical packet ownership or a held mutex requires
 * notification to be deferred until the next lock-free boundary. */
DSSH_PRIVATE void session_set_terminate_no_wake(struct dssh_session_s *sess);
DSSH_PRIVATE bool dssh_io_callback_active(struct dssh_session_s *sess);
DSSH_PRIVATE bool dssh_channel_rx_active(void);
DSSH_PRIVATE bool dssh_setup_callback_active(struct dssh_session_s *sess);

DSSH_PRIVATE bool dssh_log_enabled(struct dssh_session_s *sess, dssh_log_level level);
DSSH_PRIVATE void dssh_log_emit(struct dssh_session_s *sess, dssh_log_level level, dssh_log_source source,
    int error_code, uint32_t ssh_reason_code, bool always_display, bool truncated, const uint8_t *message,
    size_t message_len, const uint8_t *language, size_t language_len);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 4, 5)))
#endif
DSSH_PRIVATE void dssh_log_emitf(struct dssh_session_s *sess, dssh_log_level level, int error_code,
    const char *format, ...);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 4, 5)))
#endif
DSSH_PRIVATE void dssh_log_emit_localf(struct dssh_session_s *sess, dssh_log_level level, int error_code,
    const char *format, ...);
DSSH_PRIVATE void dssh_log_cleanup(struct dssh_session_s *sess);
DSSH_PRIVATE void dssh_log_termination(struct dssh_session_s *sess, int cause);

/* The outer guard is intentional: suppressed format arguments are not
 * evaluated, formatted, or copied. */
#define DSSH_LOGF(sess, level, error_code, ...)                    \
	do {                                                        \
		if (dssh_log_enabled((sess), (level)))                \
			dssh_log_emitf((sess), (level), (error_code), __VA_ARGS__); \
	} while (0)

/* Stack-owned operation scopes prevent destruction from a nested callback. */
struct dssh_session_scope {
	struct dssh_session_s *sess;
	struct dssh_session_scope *previous;
	bool termination_callback;
};
DSSH_PRIVATE void session_scope_enter(struct dssh_session_scope *scope, struct dssh_session_s *sess);
DSSH_PRIVATE void session_scope_leave(struct dssh_session_scope *scope);
DSSH_PRIVATE bool session_scope_active(struct dssh_session_s *sess);
DSSH_PRIVATE bool session_termination_callback_active(struct dssh_session_s *sess);
DSSH_PRIVATE bool session_receive_api_conflict(struct dssh_session_s *sess);
DSSH_PRIVATE bool session_lifecycle_begin(struct dssh_session_s *sess);
DSSH_PRIVATE void session_stop_inner(struct dssh_session_s *sess);
DSSH_PRIVATE bool session_on_demux_thread(struct dssh_session_s *sess);

/*
 * Test allocation redirection.  Under DSSH_TESTING, library code
 * calls dssh_test_malloc/calloc/realloc instead of the real
 * allocators.  Algorithm module files must include ssh-internal.h
 * to get these redirects.
 *
 * free() is observed for selected cleanup tests, then forwarded to real
 * free(). The allocation countdown and external allocators are unchanged.
 */
#ifdef DSSH_TESTING
void *dssh_test_malloc(size_t sz);
void *dssh_test_calloc(size_t nmemb, size_t sz);
void *dssh_test_realloc(void *ptr, size_t sz);
void dssh_test_free(void *ptr);

 #define malloc(x)     dssh_test_malloc(x)
 #define calloc(n, s)  dssh_test_calloc(n, s)
 #define realloc(p, s) dssh_test_realloc(p, s)
 #define free(p)       dssh_test_free(p)

/* Clock controls use their own thread-local state. */
int dssh_test_timespec_get(struct timespec *ts, int base);
 #undef timespec_get
 #define timespec_get(t, b) dssh_test_timespec_get(t, b)

/* C11 threads */
int dssh_test_mtx_init(mtx_t *mtx, int type);
int dssh_test_cnd_init(cnd_t *cond);
int dssh_test_thrd_create(thrd_t *thr, thrd_start_t func, void *arg);

 #define mtx_init(m, t)       dssh_test_mtx_init(m, t)
 #define cnd_init(c)          dssh_test_cnd_init(c)
 #define thrd_create(t, f, a) dssh_test_thrd_create(t, f, a)

int dssh_test_mtx_lock(mtx_t *mtx);
int dssh_test_mtx_trylock(mtx_t *mtx);
int dssh_test_mtx_unlock(mtx_t *mtx);
int dssh_test_cnd_wait(cnd_t *cond, mtx_t *mtx);
int dssh_test_cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *ts);
int dssh_test_cnd_broadcast(cnd_t *cond);
int dssh_test_cnd_signal(cnd_t *cond);
int dssh_test_thrd_join(thrd_t thr, int *res);

 #define mtx_lock(m)            dssh_test_mtx_lock(m)
 #define mtx_trylock(m)         dssh_test_mtx_trylock(m)
 #define mtx_unlock(m)          dssh_test_mtx_unlock(m)
 #define cnd_wait(c, m)         dssh_test_cnd_wait(c, m)
 #define cnd_timedwait(c, m, t) dssh_test_cnd_timedwait(c, m, t)
 #define cnd_broadcast(c)       dssh_test_cnd_broadcast(c)
 #define cnd_signal(c)          dssh_test_cnd_signal(c)
 #define thrd_join(t, r)        dssh_test_thrd_join(t, r)

/* OpenSSL */
int dssh_test_BN_rand(BIGNUM *rnd, int bits, int top, int bottom);
int dssh_test_BN_rand_range(BIGNUM *rnd, const BIGNUM *range);
int dssh_test_BN_priv_rand_range_ex(BIGNUM *rnd, const BIGNUM *range,
    unsigned int strength, BN_CTX *ctx);
BIGNUM *dssh_test_BN_dup(const BIGNUM *a);
int dssh_test_BN_rshift1(BIGNUM *r, const BIGNUM *a);
int dssh_test_BN_sub_word(BIGNUM *a, BN_ULONG word);
int dssh_test_BN_add_word(BIGNUM *a, BN_ULONG word);
int dssh_test_BN_check_prime(const BIGNUM *p, BN_CTX *ctx, BN_GENCB *cb);
int dssh_test_EVP_PKEY_CTX_set_rsa_padding(EVP_PKEY_CTX *ctx, int pad_mode);

 #define BN_rand(r, b, t, bo)               dssh_test_BN_rand(r, b, t, bo)
 #define BN_rand_range(r, range)           dssh_test_BN_rand_range(r, range)
 #define BN_priv_rand_range_ex(r, range, s, c) dssh_test_BN_priv_rand_range_ex(r, range, s, c)
 #define BN_dup(a)                        dssh_test_BN_dup(a)
 #define BN_rshift1(r, a)                  dssh_test_BN_rshift1(r, a)
 #define BN_sub_word(a, w)                dssh_test_BN_sub_word(a, w)
 #define BN_add_word(a, w)                dssh_test_BN_add_word(a, w)
 #define BN_check_prime(p, c, cb)         dssh_test_BN_check_prime(p, c, cb)
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

#define DSSH_LOG_FORWARD_MAX 8

struct dssh_log_forward_entry {
	size_t  message_len;
	bool    always_display;
	uint8_t message[DSSH_LOG_MESSAGE_MAX];
};

struct dssh_log_forward_queue {
	mtx_t                         mtx;
	size_t                        head;
	size_t                        count;
	struct dssh_log_forward_entry entries[DSSH_LOG_FORWARD_MAX];
};

/* RFC 4251 s6: names must not contain DEL */
#define DSSH_ASCII_DEL 127

/* Practical buffer sizes for algorithm name-lists and per-channel
 * request data.  See README.md "Limits". */
#define DSSH_NAMELIST_BUF_SIZE 1024
#define DSSH_REQ_DATA_BUF_SIZE 1024

/* Channel types: defined in ssh-chan.h (included above) */

/* SSH connection protocol message types (RFC 4254) */
#define SSH_MSG_GLOBAL_REQUEST            UINT8_C(80)
#define SSH_MSG_REQUEST_SUCCESS           UINT8_C(81)
#define SSH_MSG_REQUEST_FAILURE           UINT8_C(82)
#define SSH_MSG_CHANNEL_OPEN              UINT8_C(90)
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION UINT8_C(91)
#define SSH_MSG_CHANNEL_OPEN_FAILURE      UINT8_C(92)
#define SSH_MSG_CHANNEL_WINDOW_ADJUST     UINT8_C(93)
#define SSH_MSG_CHANNEL_DATA              UINT8_C(94)
#define SSH_MSG_CHANNEL_EXTENDED_DATA     UINT8_C(95)
#define SSH_MSG_CHANNEL_EOF               UINT8_C(96)
#define SSH_MSG_CHANNEL_CLOSE             UINT8_C(97)
#define SSH_MSG_CHANNEL_REQUEST           UINT8_C(98)
#define SSH_MSG_CHANNEL_SUCCESS           UINT8_C(99)
#define SSH_MSG_CHANNEL_FAILURE           UINT8_C(100)

/* SSH_OPEN_* reason codes: defined in ssh-conn.c (only consumer) */

/* I/O model for channel dispatch */
#define DSSH_IO_STREAM 1 /* dssh_chan_ stream API */
#define DSSH_IO_ZC     2 /* dssh_chan_zc_ zero-copy API */

enum dssh_channel_dispatch_state {
	DSSH_CHANNEL_DISPATCH_IDLE = 0,
	DSSH_CHANNEL_DISPATCH_ACTIVE,
};

struct dssh_channel_s {
	/* Pointers and size_t (pointer-sized) */
	struct dssh_session_s *sess; /* back-pointer (new API takes ch only) */
	struct dssh_channel_id *protocol_id;
	uint8_t               *setup_payload;
	size_t                 setup_payload_len;
	size_t                 setup_payload_capacity;
	dssh_chan_zc_cb        zc_cb;    /* ZC data callback (or internal for stream) */
	void                  *zc_cbdata;
	dssh_chan_event_cb     event_cb; /* optional event push callback */
	void                  *event_cbdata;
	dssh_chan_break_cb     break_cb;
	void                  *break_cbdata;

	/* C11 synchronization (platform-dependent size).
	 * TX slot draining uses tx_mtx -> channel_mtx -> buf_mtx.
	 * Callers must release buf_mtx before acquiring tx_mtx. */
	mtx_t       buf_mtx;
	mtx_t       cb_mtx;  /* protects callback function pointer + cbdata pairs */
	cnd_t       poll_cnd;
	atomic_bool closing; /* set by close functions before cleanup */
	atomic_bool terminate_woken;
	/* Keeps embedded TX slots alive while a transport owner drains them
	 * without channel_mtx held. */
	atomic_uint tx_refs;
	/* A session has one demux thread, so at most one dispatch can own a
	 * channel. ACTIVE keeps the channel alive while that dispatch drops
	 * buf_mtx for callbacks or I/O. */
	atomic_int dispatch_state;
	/* First receive-credit failure, shared by both stream read directions.
	 * Atomic publication also works after an acquisition failure; ordinary
	 * publication pairs with buf_mtx/poll_cnd to avoid a lost wakeup. */
	atomic_int read_error;
	/* A terminal failure can propagate through several scheduler frames.
	 * Publish it only once, even if another reader acknowledges it before
	 * the original read finishes releasing packet-engine ownership. */
	atomic_bool read_error_terminal;

			     /* 4-byte */
	uint32_t              local_id;
	uint32_t              remote_id;
	uint32_t              local_window;
	uint32_t              window_adjust_pending;
	uint32_t              window_adjust_inflight;
	atomic_uint_least32_t remote_window;
	uint32_t              remote_max_packet;
	uint32_t              window_max;
	uint32_t              exit_code;
	int                   chan_type;
	int                   io_model;  /* DSSH_IO_STREAM or DSSH_IO_ZC */
	int                   zc_stream; /* stashed stream from zc_getbuf */

					 /* 1-byte */
	bool    open;
	bool    open_pending; /* outbound OPEN has not received its one response */
	bool    close_sent;
	bool    close_received;
	bool    eof_sent;
	bool    eof_received;
	bool    exit_code_received;
	bool    request_pending;
	bool    request_responded;
	bool    request_success;
	bool    setup_mode;
	bool    setup_ready;
	bool    setup_error;
	/* Latches true after send_window_adjust at the end of
	 * dssh_chan_open / dssh_chan_zc_open succeeds; gates the
	 * early-data bypass.  Atomic so the writer (app thread,
	 * outside any DSSH mutex) publishes via release-store and
	 * the reader (demux, in handle_channel_*data) sees the new
	 * value via acquire-load without needing buf_mtx contention. */
	atomic_bool setup_complete;
	/* DSSH_PARAM_ACCEPT_EARLY_DATA copied from params at open time;
	 * lets demux deliver CHANNEL_DATA over a closed local_window
	 * for the duration of !setup_complete.  Plain bool: written
	 * before register_channel, so the channel_mtx release in
	 * register_channel publishes it to any later demux read. */
	bool    accept_pre_window_data;
	uint8_t setup_msg_type;

	/* Per-channel string buffers (avoids static storage) */
	char last_signal[32];
	char req_type[32];
	char req_data[DSSH_REQ_DATA_BUF_SIZE];

	/* Pre-allocated TX slots for demux fire-and-forget responses */
	struct dssh_tx_slot wa_slot;        /* CHANNEL_WINDOW_ADJUST (9 B) */
	struct dssh_tx_slot chan_reply_slot; /* CHANNEL_SUCCESS/FAILURE (8 B) */

					    /* Embedded structs (contain pointers, large) */
	struct dssh_chan_params params; /* new API: getters after open/accept */
	struct dssh_event_queue events; /* new API: event queue */

	struct {
		struct dssh_bytebuf stdout_buf;
		struct dssh_bytebuf stderr_buf;
	} buf;
};

DSSH_PRIVATE void channel_defer_read_error(struct dssh_session_s *sess,
	struct dssh_channel_s *ch, int error);

/* Private protocol ownership outlives an application channel when a peer
 * response is still outstanding. The local number is immutable once
 * assigned. All other fields are protected by channel_mtx. */
enum dssh_channel_id_phase {
	DSSH_CHANNEL_ID_RESERVED,
	DSSH_CHANNEL_ID_QUEUED,
	DSSH_CHANNEL_ID_ACCEPTING,
	DSSH_CHANNEL_ID_OPENING,
	DSSH_CHANNEL_ID_CONFIRMED,
	DSSH_CHANNEL_ID_FAILED
};

struct dssh_channel_id {
	struct dssh_channel_id *next;
	size_t refs;
	uint32_t local;
	uint32_t remote;
	enum dssh_channel_id_phase phase;
	bool local_assigned;
	bool retired;
	bool close_started;
	bool close_ordered;
	bool close_sent;
	bool close_received;
};

struct dssh_language_field {
	char *data;
	size_t len;
	size_t capacity;
};

struct dssh_client_key_slot {
	struct dssh_client_key_slot *next;
	dssh_key_algo module;
	dssh_key_algo_ctx *ctx;
	struct dssh_result_lease lease;
};

/* Caller serializes client-key operations with synchronous session operations.
 * A slot is retained after clearing for reuse; a present lease marks a key. */
DSSH_PRIVATE dssh_key_algo_ctx *dssh_client_key_context(struct dssh_session_s *sess, dssh_key_algo module);
DSSH_PRIVATE void dssh_client_keys_cleanup(struct dssh_session_s *sess);

struct dssh_session_s {
	struct dssh_client_key_slot *client_keys;
	/* Language fields are protected by mtx. Peer fields share one allocation. */
	struct dssh_language_field local_languages[2];
	char *peer_languages;
	size_t peer_languages_capacity;
	size_t peer_languages_len[2];
	bool peer_languages_available;
	/* Pointers and size_t (pointer-sized) -- callbacks and cbdata */
	void *tx_cbdata;
	void *rx_cbdata;
	void *rx_line_cbdata;
	void *extra_line_cbdata;
	void  (*debug_cb)(bool always_display, const uint8_t *message, size_t message_len, void *cbdata);
	void *debug_cbdata;
	dssh_log_cb log_cb;
	void       *log_cbdata;
	struct dssh_log_forward_queue *log_forward;
	void  (*unimplemented_cb)(uint32_t rejected_seq, void *cbdata);
	void *unimplemented_cbdata;
	void (*banner_cb)(const uint8_t *message, size_t message_len, const uint8_t *language, size_t language_len,
	    void *cbdata);
	void *banner_cbdata;
	char *pending_banner;
	char *pending_banner_lang;
	/* Per-session algorithm whitelist filters (NULL = use all
	 * registered).  CSV form, parsed with name_in_filter().
	 * Filter order is negotiation preference order. */
	char *kex_filter;
	char *key_algo_filter;
	char *enc_filter;
	char *mac_filter;
	char *comp_filter;
	int   (*global_request_cb)(const uint8_t *name, size_t name_len, bool want_reply, const uint8_t *data,
            size_t data_len, void *cbdata);
	void *global_request_cbdata;
	void  (*terminate_cb)(struct dssh_session_s *sess, void *cbdata);
	void *terminate_cbdata;
	dssh_hostkey_verify_cb hostkey_verify_cb;
	void                  *hostkey_verify_cbdata;
	dssh_chan_event_cb      default_event_cb;
	void                   *default_event_cbdata;
	dssh_chan_break_cb      default_break_cb;
	void                   *default_break_cbdata;
	size_t                  default_max_events;
	struct dssh_channel_s **channels;
	size_t                  channel_count;
	size_t                  channel_capacity;
	struct dssh_channel_id *channel_ids;
	struct dssh_channel_id *free_channel_ids;
	size_t retired_channel_ids;

	/* Embedded transport state (large, naturally aligned) */
	struct dssh_transport_state_s trans;

	/* Incoming channel open queue */
	struct dssh_accept_queue accept_queue;

	/* C11 synchronization (platform-dependent size).
         * Lock order: channel_mtx then buf_mtx (never reversed). */
	mtx_t  mtx;
	mtx_t  channel_mtx;
	mtx_t  accept_mtx;
	cnd_t  accept_cnd;
	thrd_t demux_thread;

	/* Counters and millisecond intervals */
	uint32_t next_channel_id;
	intmax_t timeout_ms;

	/* 1-byte */
	atomic_bool initialized;
	atomic_bool terminate;
	atomic_bool demux_running;
	atomic_bool conn_initialized;
	atomic_int  log_level;
	atomic_bool termination_logged;
	atomic_bool terminate_notified;
	atomic_bool terminate_wake_started;
	atomic_bool lifecycle_busy;
	atomic_bool log_mirror_suppressed;
	/* Synchronous pre-start operations are serialized by the application.
	 * Publish only after their complete public operation succeeds. Session
	 * identifiers and installed keys can also exist on a failed handshake. */
	bool        handshake_complete;
	bool        auth_complete;
	bool        auth_service_requested;
	/* Client reply snapshots are mutable only during application-serialized
	 * pre-start authentication, and immutable after successful startup. */
	char       *auth_methods;
	size_t      auth_methods_len;
	size_t      auth_methods_capacity;
	bool        auth_methods_available;
	/* Owned by the synchronous server authentication receive operation. */
	bool        server_auth_active;
	/* Retained across server-operation errors; pre-start calls are serialized. */
	uint8_t    *server_auth_user;
	size_t      server_auth_user_len;
	size_t      server_auth_user_capacity;
	bool        server_auth_user_bound;
	bool        server_auth_service_accepted;
};

/* A failed release of a valid owned mutex leaves no recoverable ownership or
 * cleanup contract. Stop before callbacks, lifetime mutation or cleanup. */
static inline int
dssh_mtx_unlock(mtx_t *mutex)
{
	int ret = mtx_unlock(mutex);
	if (ret != thrd_success)
		abort();
	return ret;
}

/*
 * Generic C11 thread-call checker.  Call the real function, check the
 * result.  thrd_success, thrd_busy, and thrd_timedout are non-errors;
 * anything else (thrd_error, thrd_nomem) triggers session termination.
 *
 * Callers must check the return before using state protected by the mutex
 * or interpreting a wait result.  This helper does not establish ownership
 * when the underlying operation failed. Call only after releasing all held
 * locks, so termination callbacks run without locks. A deferred termination
 * flag does not establish that notification and wakeups have completed.
 */
static inline int
dssh_thrd_check(struct dssh_session_s *sess, int ret)
{
	if (ret != thrd_success && ret != thrd_busy && ret != thrd_timedout)
		session_set_terminate(sess);
	return ret;
}

/* Failure during the required termination wake cannot safely return with
 * hidden waiters or unknown mutex ownership. These objects are initialized
 * and valid; ordinary resource/acquisition failures use normal error paths. */
static inline void
dssh_thrd_require(int ret)
{
	if (ret != thrd_success)
		abort();
}

static inline int
dssh_thrd_check_unlock(struct dssh_session_s *sess, int ret)
{
	(void)sess;
	if (ret != thrd_success)
		abort();
	return ret;
}

DSSH_PRIVATE int transport_disconnect_inner(struct dssh_session_s *sess, uint32_t reason,
    const char *description);
DSSH_PRIVATE int transport_disconnect_result(struct dssh_session_s *sess, uint32_t reason,
    const char *description, int result);

DSSH_PRIVATE int dssh_clock_read(struct timespec *ts);

/* Convert a positive timeout, preserving the caller's output on failure. */
DSSH_PRIVATE int dssh_deadline_from_ms(struct timespec *ts, intmax_t timeout_ms);

/* Observe exact private results only in test builds. Release preprocessing
 * leaves the original expression, with no added call or evaluation. */
#ifdef DSSH_TESTING
 #define DSSH_TEST_CHANNEL_RESULT(stage, value) dssh_test_channel_observe_result(stage, value)
#else
 #define DSSH_TEST_CHANNEL_RESULT(stage, value) value
#endif

#endif   // ifndef DSSH_INTERNAL_H
