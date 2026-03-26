/*
 * ssh-internal.h — Internal struct definitions.
 * Not a public header.  Included by library .c files only.
 */
#ifndef DSSH_INTERNAL_H
#define DSSH_INTERNAL_H

#include <stdatomic.h>
#include <threads.h>

#include "deucessh.h"
#include "ssh-chan.h"
#include "ssh-trans.h"

/* DSSH_TESTABLE is now defined in deucessh-portable.h */

/* Set terminate flag and wake all library-owned condvar waiters. */
DSSH_PRIVATE void dssh_session_set_terminate(dssh_session sess);

/*
 * Test allocation redirection.  Under DSSH_TESTING, library code
 * calls dssh_test_malloc/calloc/realloc instead of the real
 * allocators.  Algorithm module files must include ssh-internal.h
 * to get these redirects.
 *
 * free() is NOT redirected — real free() handles all pointers
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

/* OpenSSL */
int dssh_test_BN_rand(BIGNUM *rnd, int bits, int top, int bottom);
int dssh_test_EVP_PKEY_CTX_set_rsa_padding(EVP_PKEY_CTX *ctx, int pad_mode);

 #define BN_rand(r, b, t, bo) dssh_test_BN_rand(r, b, t, bo)
 #define EVP_PKEY_CTX_set_rsa_padding(c, p) dssh_test_EVP_PKEY_CTX_set_rsa_padding(c, p)
int dssh_test_EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *ctx, int pad);
 #define EVP_CIPHER_CTX_set_padding(c, p) dssh_test_EVP_CIPHER_CTX_set_padding(c, p)

#endif /* DSSH_TESTING */

/* Channel types */
#define DSSH_CHAN_SESSION 1
#define DSSH_CHAN_RAW 2

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

#define SSH_OPEN_ADMINISTRATIVELY_PROHIBITED UINT32_C(1)

struct dssh_channel_s {
        /* Pointers and size_t (pointer-sized) */
	uint8_t *setup_payload;
	size_t   setup_payload_len;
	size_t   stdout_consumed;
	size_t   stderr_consumed;
	void     (*window_change_cb)(uint32_t cols, uint32_t rows,
	    uint32_t wpx, uint32_t hpx, void *cbdata);
	void    *window_change_cbdata;

        /* C11 synchronization (platform-dependent size) */
	mtx_t    buf_mtx;
	cnd_t    poll_cnd;

        /* 4-byte */
	uint32_t local_id;
	uint32_t remote_id;
	uint32_t local_window;
	uint32_t remote_window;
	uint32_t remote_max_packet;
	uint32_t window_max;
	uint32_t exit_code;
	int      chan_type;

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
	uint8_t  setup_msg_type;

        /* Per-channel string buffers (avoids static storage) */
	char     last_signal[32];
	char     req_type[32];
	char     req_data[1024];

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
        /* Pointers and size_t (pointer-sized) — callbacks and cbdata */
	void                         *tx_cbdata;
	void                         *rx_cbdata;
	void                         *rx_line_cbdata;
	void                         *extra_line_cbdata;
	dssh_debug_cb                 debug_cb;
	void                         *debug_cbdata;
	dssh_unimplemented_cb         unimplemented_cb;
	void                         *unimplemented_cbdata;
	dssh_auth_banner_cb           banner_cb;
	void                         *banner_cbdata;
	dssh_global_request_cb        global_request_cb;
	void                         *global_request_cbdata;
	struct dssh_channel_s       **channels;
	size_t                        channel_count;
	size_t                        channel_capacity;

        /* Embedded transport state (large, naturally aligned) */
	struct dssh_transport_state_s trans;

        /* Incoming channel open queue */
	struct dssh_accept_queue      accept_queue;

        /* C11 synchronization (platform-dependent size) */
	mtx_t                         mtx;
	mtx_t                         channel_mtx;
	mtx_t                         accept_mtx;
	cnd_t                         accept_cnd;
	thrd_t                        demux_thread;

        /* 4-byte */
	uint32_t                      next_channel_id;

        /* 1-byte */
	atomic_bool                   initialized;
	atomic_bool                   terminate;
	atomic_bool                   demux_running;
	bool                          auth_service_requested;
	bool                          conn_initialized;
};

#endif // ifndef DSSH_INTERNAL_H
