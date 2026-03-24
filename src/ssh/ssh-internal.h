/*
 * ssh-internal.h — Internal struct definitions.
 * Not a public header.  Included by library .c files only.
 */

#ifndef DSSH_INTERNAL_H
#define DSSH_INTERNAL_H

#include <stdatomic.h>
#include <threads.h>

#include "deucessh.h"
#include "ssh-trans.h"
#include "ssh-chan.h"

/*
 * DSSH_TESTABLE: marks internal functions that need to be linkable
 * from test code.  In normal builds these are static; when compiled
 * with -DDSSH_TESTING they become externally visible.
 */
#ifdef DSSH_TESTING
#define DSSH_TESTABLE
#else
#define DSSH_TESTABLE static
#endif

/* Set terminate flag and wake all library-owned condvar waiters. */
DSSH_PRIVATE void dssh_session_set_terminate(dssh_session sess);

/*
 * Test allocation redirection.  Under DSSH_TESTING, library code
 * calls dssh_test_malloc/calloc/realloc instead of the real
 * allocators.  These go through a countdown that can inject NULL
 * returns without affecting OpenSSL's internal allocations (which
 * don't include this header).
 *
 * free() is NOT redirected — real free() handles all pointers
 * since dssh_test_malloc returns real malloc'd memory.
 */
#ifdef DSSH_TESTING
void *dssh_test_malloc(size_t sz);
void *dssh_test_calloc(size_t nmemb, size_t sz);
void *dssh_test_realloc(void *ptr, size_t sz);

#define malloc(x)       dssh_test_malloc(x)
#define calloc(n, s)    dssh_test_calloc(n, s)
#define realloc(p, s)   dssh_test_realloc(p, s)

/*
 * OpenSSL + C11 threads failure injection.  Same principle as above:
 * library calls are redirected to countdown wrappers; OpenSSL's own
 * internal calls are unaffected.  Free/destroy/cleanse calls are
 * NOT redirected — they always use the real implementation.
 */

/* OpenSSL creation functions (return NULL on failure) */
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/params.h>

EVP_MD_CTX *dssh_test_EVP_MD_CTX_new(void);
EVP_CIPHER_CTX *dssh_test_EVP_CIPHER_CTX_new(void);
EVP_MAC_CTX *dssh_test_EVP_MAC_CTX_new(EVP_MAC *mac);
EVP_MAC *dssh_test_EVP_MAC_fetch(OSSL_LIB_CTX *ctx, const char *algorithm, const char *properties);
EVP_MD *dssh_test_EVP_MD_fetch(OSSL_LIB_CTX *ctx, const char *algorithm, const char *properties);
EVP_PKEY_CTX *dssh_test_EVP_PKEY_CTX_new(EVP_PKEY *pkey, ENGINE *e);
EVP_PKEY_CTX *dssh_test_EVP_PKEY_CTX_new_id(int id, ENGINE *e);
EVP_PKEY_CTX *dssh_test_EVP_PKEY_CTX_new_from_name(OSSL_LIB_CTX *ctx, const char *name, const char *propquery);
EVP_PKEY *dssh_test_EVP_PKEY_new_raw_public_key(int type, ENGINE *e, const unsigned char *pub, size_t len);
BN_CTX *dssh_test_BN_CTX_new(void);
BIGNUM *dssh_test_BN_new(void);
OSSL_PARAM_BLD *dssh_test_OSSL_PARAM_BLD_new(void);
OSSL_PARAM *dssh_test_OSSL_PARAM_BLD_to_param(OSSL_PARAM_BLD *bld);

#define EVP_MD_CTX_new()            dssh_test_EVP_MD_CTX_new()
#define EVP_CIPHER_CTX_new()        dssh_test_EVP_CIPHER_CTX_new()
#define EVP_MAC_CTX_new(m)          dssh_test_EVP_MAC_CTX_new(m)
#define EVP_MAC_fetch(c,a,p)        dssh_test_EVP_MAC_fetch(c,a,p)
#define EVP_MD_fetch(c,a,p)         dssh_test_EVP_MD_fetch(c,a,p)
#define EVP_PKEY_CTX_new(k,e)       dssh_test_EVP_PKEY_CTX_new(k,e)
#define EVP_PKEY_CTX_new_id(i,e)    dssh_test_EVP_PKEY_CTX_new_id(i,e)
#define EVP_PKEY_CTX_new_from_name(c,n,p) dssh_test_EVP_PKEY_CTX_new_from_name(c,n,p)
#define EVP_PKEY_new_raw_public_key(t,e,p,l) dssh_test_EVP_PKEY_new_raw_public_key(t,e,p,l)
#define BN_CTX_new()                dssh_test_BN_CTX_new()
#define BN_new()                    dssh_test_BN_new()
#define OSSL_PARAM_BLD_new()        dssh_test_OSSL_PARAM_BLD_new()
#define OSSL_PARAM_BLD_to_param(b)  dssh_test_OSSL_PARAM_BLD_to_param(b)

/* OpenSSL operation functions (return 0/<=0 on failure) */
int dssh_test_EVP_DigestInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type, ENGINE *impl);
int dssh_test_EVP_DigestUpdate(EVP_MD_CTX *ctx, const void *d, size_t cnt);
int dssh_test_EVP_DigestFinal_ex(EVP_MD_CTX *ctx, unsigned char *md, unsigned int *s);
int dssh_test_EVP_DigestSignInit(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx, const EVP_MD *type, ENGINE *e, EVP_PKEY *pkey);
int dssh_test_EVP_DigestSign(EVP_MD_CTX *ctx, unsigned char *sigret, size_t *siglen, const unsigned char *tbs, size_t tbslen);
int dssh_test_EVP_DigestVerifyInit(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx, const EVP_MD *type, ENGINE *e, EVP_PKEY *pkey);
int dssh_test_EVP_DigestVerify(EVP_MD_CTX *ctx, const unsigned char *sigret, size_t siglen, const unsigned char *tbs, size_t tbslen);
int dssh_test_EVP_EncryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl, const unsigned char *key, const unsigned char *iv);
int dssh_test_EVP_EncryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in, int inl);
int dssh_test_EVP_MAC_init(EVP_MAC_CTX *ctx, const unsigned char *key, size_t keylen, const OSSL_PARAM params[]);
int dssh_test_EVP_MAC_update(EVP_MAC_CTX *ctx, const unsigned char *data, size_t datalen);
int dssh_test_EVP_MAC_final(EVP_MAC_CTX *ctx, unsigned char *out, size_t *outl, size_t outsize);
int dssh_test_EVP_PKEY_keygen_init(EVP_PKEY_CTX *ctx);
int dssh_test_EVP_PKEY_keygen(EVP_PKEY_CTX *ctx, EVP_PKEY **ppkey);
int dssh_test_EVP_PKEY_derive_init(EVP_PKEY_CTX *ctx);
int dssh_test_EVP_PKEY_derive_set_peer(EVP_PKEY_CTX *ctx, EVP_PKEY *peer);
int dssh_test_EVP_PKEY_derive(EVP_PKEY_CTX *ctx, unsigned char *key, size_t *keylen);
int dssh_test_EVP_PKEY_fromdata_init(EVP_PKEY_CTX *ctx);
int dssh_test_EVP_PKEY_fromdata(EVP_PKEY_CTX *ctx, EVP_PKEY **ppkey, int selection, OSSL_PARAM params[]);
int dssh_test_EVP_PKEY_get_raw_public_key(const EVP_PKEY *pkey, unsigned char *pub, size_t *len);
int dssh_test_EVP_PKEY_get_bn_param(const EVP_PKEY *pkey, const char *key_name, BIGNUM **bn);
int dssh_test_RAND_bytes(unsigned char *buf, int num);
int dssh_test_BN_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx);
int dssh_test_BN_rand(BIGNUM *rnd, int bits, int top, int bottom);
BIGNUM *dssh_test_BN_bin2bn(const unsigned char *s, int len, BIGNUM *ret);
int dssh_test_OSSL_PARAM_BLD_push_BN(OSSL_PARAM_BLD *bld, const char *key, const BIGNUM *bn);
int dssh_test_EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *ctx, int pad);
int dssh_test_EVP_PKEY_CTX_set_params(EVP_PKEY_CTX *ctx, const OSSL_PARAM *params);

#define EVP_DigestInit_ex(c,t,i)        dssh_test_EVP_DigestInit_ex(c,t,i)
#define EVP_DigestUpdate(c,d,n)         dssh_test_EVP_DigestUpdate(c,d,n)
#define EVP_DigestFinal_ex(c,m,s)       dssh_test_EVP_DigestFinal_ex(c,m,s)
#define EVP_DigestSignInit(c,p,t,e,k)   dssh_test_EVP_DigestSignInit(c,p,t,e,k)
#define EVP_DigestSign(c,s,sl,t,tl)     dssh_test_EVP_DigestSign(c,s,sl,t,tl)
#define EVP_DigestVerifyInit(c,p,t,e,k) dssh_test_EVP_DigestVerifyInit(c,p,t,e,k)
#define EVP_DigestVerify(c,s,sl,t,tl)   dssh_test_EVP_DigestVerify(c,s,sl,t,tl)
#define EVP_EncryptInit_ex(c,ci,i,k,iv) dssh_test_EVP_EncryptInit_ex(c,ci,i,k,iv)
#define EVP_EncryptUpdate(c,o,ol,i,il)  dssh_test_EVP_EncryptUpdate(c,o,ol,i,il)
#define EVP_MAC_init(c,k,kl,p)          dssh_test_EVP_MAC_init(c,k,kl,p)
#define EVP_MAC_update(c,d,dl)          dssh_test_EVP_MAC_update(c,d,dl)
#define EVP_MAC_final(c,o,ol,os)        dssh_test_EVP_MAC_final(c,o,ol,os)
#define EVP_PKEY_keygen_init(c)         dssh_test_EVP_PKEY_keygen_init(c)
#define EVP_PKEY_keygen(c,p)            dssh_test_EVP_PKEY_keygen(c,p)
#define EVP_PKEY_derive_init(c)         dssh_test_EVP_PKEY_derive_init(c)
#define EVP_PKEY_derive_set_peer(c,p)   dssh_test_EVP_PKEY_derive_set_peer(c,p)
#define EVP_PKEY_derive(c,k,kl)         dssh_test_EVP_PKEY_derive(c,k,kl)
#define EVP_PKEY_fromdata_init(c)       dssh_test_EVP_PKEY_fromdata_init(c)
#define EVP_PKEY_fromdata(c,p,s,pa)     dssh_test_EVP_PKEY_fromdata(c,p,s,pa)
#define EVP_PKEY_get_raw_public_key(p,b,l)  dssh_test_EVP_PKEY_get_raw_public_key(p,b,l)
#define EVP_PKEY_get_bn_param(p,k,b)    dssh_test_EVP_PKEY_get_bn_param(p,k,b)
#define RAND_bytes(b,n)                 dssh_test_RAND_bytes(b,n)
#define BN_mod_exp(r,a,p,m,c)           dssh_test_BN_mod_exp(r,a,p,m,c)
#define BN_rand(r,b,t,bo)              dssh_test_BN_rand(r,b,t,bo)
#define BN_bin2bn(s,l,r)               dssh_test_BN_bin2bn(s,l,r)
#define OSSL_PARAM_BLD_push_BN(b,k,bn) dssh_test_OSSL_PARAM_BLD_push_BN(b,k,bn)
#define EVP_CIPHER_CTX_set_padding(c,p) dssh_test_EVP_CIPHER_CTX_set_padding(c,p)
#define EVP_PKEY_CTX_set_params(c,p)    dssh_test_EVP_PKEY_CTX_set_params(c,p)

/* C11 threads */
int dssh_test_mtx_init(mtx_t *mtx, int type);
int dssh_test_cnd_init(cnd_t *cond);
int dssh_test_thrd_create(thrd_t *thr, thrd_start_t func, void *arg);

#define mtx_init(m,t)           dssh_test_mtx_init(m,t)
#define cnd_init(c)             dssh_test_cnd_init(c)
#define thrd_create(t,f,a)      dssh_test_thrd_create(t,f,a)

#endif /* DSSH_TESTING */

/* Channel types */
#define DSSH_CHAN_SESSION 1
#define DSSH_CHAN_RAW     2

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

#define SSH_OPEN_ADMINISTRATIVELY_PROHIBITED UINT32_C(1)

struct dssh_channel_s {
	/* Wire-level fields */
	uint32_t local_id;
	uint32_t remote_id;
	uint32_t local_window;
	uint32_t remote_window;
	uint32_t remote_max_packet;
	bool open;
	bool close_sent;
	bool close_received;
	bool eof_sent;
	bool eof_received;

	/* Buffered mode fields (chan_type > 0) */
	int chan_type;
	mtx_t buf_mtx;
	cnd_t poll_cnd;

	/* Consumed byte counters (for signal mark tracking) */
	size_t stdout_consumed;
	size_t stderr_consumed;

	/* Window replenishment threshold */
	uint32_t window_max;

	/* Exit status from peer (session channels) */
	uint32_t exit_code;
	bool exit_code_received;

	/* Channel request response (for send_channel_request_wait) */
	bool request_pending;
	bool request_responded;
	bool request_success;

	/* Setup mailbox (for session_accept_channel) */
	bool setup_mode;
	bool setup_ready;
	uint8_t setup_msg_type;
	uint8_t *setup_payload;
	size_t setup_payload_len;

	/* Window-change callback (server-side, post-setup) */
	void (*window_change_cb)(uint32_t cols, uint32_t rows,
	    uint32_t wpx, uint32_t hpx, void *cbdata);
	void *window_change_cbdata;

	/* Per-channel string buffers (avoids static storage) */
	char last_signal[32];
	char req_type[32];
	char req_data[1024];

	union {
		struct {
			struct dssh_bytebuf stdout_buf;
			struct dssh_bytebuf stderr_buf;
			struct dssh_signal_queue signals;
		} session;
		struct {
			struct dssh_msgqueue queue;
		} raw;
	} buf;
};

struct dssh_session_s {
	mtx_t mtx;
	atomic_bool initialized;
	atomic_bool terminate;

	/* Per-session I/O callback data (passed to the global I/O callbacks) */
	void *tx_cbdata;
	void *rx_cbdata;
	void *rx_line_cbdata;
	void *extra_line_cbdata;

	/* Optional notification callbacks */
	dssh_debug_cb debug_cb;
	void *debug_cbdata;
	dssh_unimplemented_cb unimplemented_cb;
	void *unimplemented_cbdata;

	/* Auth banner callback (set before authentication) */
	void *banner_cb;
	void *banner_cbdata;

	/*
	 * Optional global request callback (RFC 4254 s4).
	 * If set, called for every SSH_MSG_GLOBAL_REQUEST.
	 * Return 0 for REQUEST_SUCCESS, negative for REQUEST_FAILURE.
	 * If NULL, REQUEST_FAILURE is sent automatically.
	 */
	void *global_request_cb;
	void *global_request_cbdata;

	/* Auth state */
	bool auth_service_requested;

	/* Transport layer state */
	struct dssh_transport_state_s trans;

	/* Demux thread and channel table */
	thrd_t demux_thread;
	atomic_bool demux_running;
	bool conn_initialized;  /* channel_mtx/accept_mtx/accept_cnd valid */

	struct dssh_channel_s **channels;
	size_t channel_count;
	size_t channel_capacity;
	mtx_t channel_mtx;

	/* Incoming channel open queue */
	struct dssh_accept_queue accept_queue;
	mtx_t accept_mtx;
	cnd_t accept_cnd;

	/* Next auto-assigned local channel ID */
	uint32_t next_channel_id;
};

#endif
