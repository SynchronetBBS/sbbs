/*
 * dssh_test_ossl.c -- OpenSSL + C11 threads failure injection.
 *
 * Countdown-based wrappers: each failable call decrements a shared
 * atomic counter.  When it reaches zero, the wrapper returns the
 * appropriate failure value without calling the real function.
 *
 * Under DSSH_TESTING, macros in ssh-internal.h redirect library
 * calls to these wrappers.  OpenSSL's own internal calls are
 * unaffected because OpenSSL doesn't include ssh-internal.h.
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <threads.h>

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/params.h>
#include <openssl/rand.h>

#include "dssh_test_ossl.h"

static atomic_int ossl_fail_at = -1;
static atomic_int ossl_call_num = 0;

/*
 * Thread-local participation flag.  Defaults to true so all threads
 * participate in the countdown (backward compatible).  A thread can
 * opt out by calling dssh_test_ossl_exclude_thread(), which sets its
 * local flag to false -- its ossl calls then pass straight through
 * without incrementing the counter.
 */
static _Thread_local bool ossl_this_thread = true;

/*
 * Separate countdown for lock/unlock/condvar wrappers (items 85-86).
 * Kept independent of the OpenSSL + init countdown so existing ossl
 * tests are not affected by the additional failable call sites.
 */
static atomic_int thrd_fail_at = -1;
static atomic_int thrd_call_num = 0;

void
dssh_test_ossl_reset(void)
{
	atomic_store(&ossl_fail_at, -1);
	atomic_store(&ossl_call_num, 0);
	atomic_store(&thrd_fail_at, -1);
	atomic_store(&thrd_call_num, 0);
	ossl_this_thread = true;
}

void
dssh_test_ossl_fail_after(int n)
{
	atomic_store(&ossl_call_num, 0);
	atomic_store(&ossl_fail_at, n);
}

void
dssh_test_ossl_exclude_thread(void)
{
	ossl_this_thread = false;
}

int
dssh_test_ossl_count(void)
{
	return atomic_load(&ossl_call_num);
}

void
dssh_test_thrd_fail_after(int n)
{
	atomic_store(&thrd_call_num, 0);
	atomic_store(&thrd_fail_at, n);
}

void
dssh_test_thrd_reset(void)
{
	atomic_store(&thrd_fail_at, -1);
	atomic_store(&thrd_call_num, 0);
}

int
dssh_test_thrd_count(void)
{
	return atomic_load(&thrd_call_num);
}

/*
 * Check if a lock/unlock/condvar call should fail.  Uses the
 * separate thrd_fail_at / thrd_call_num countdown.
 */
static bool
should_thrd_fail(void)
{
	if (!ossl_this_thread)
		return false;
	int fa = atomic_load(&thrd_fail_at);
	if (fa < 0)
		return false;
	int n = atomic_fetch_add(&thrd_call_num, 1);
	return (n == fa);
}

/*
 * Check if this call should fail.  Returns true if the countdown
 * has fired (call_num == fail_at).  Threads that called
 * dssh_test_ossl_exclude_thread() are ignored.
 */
static bool
should_fail(void)
{
	if (!ossl_this_thread)
		return false;
	int fa = atomic_load(&ossl_fail_at);
	if (fa < 0)
		return false;
	int n = atomic_fetch_add(&ossl_call_num, 1);
	return (n == fa);
}

/* ================================================================
 * OpenSSL creation wrappers (return NULL on failure)
 * ================================================================ */

EVP_MD_CTX *
dssh_test_EVP_MD_CTX_new(void)
{
	if (should_fail())
		return NULL;
	return EVP_MD_CTX_new();
}

EVP_CIPHER_CTX *
dssh_test_EVP_CIPHER_CTX_new(void)
{
	if (should_fail())
		return NULL;
	return EVP_CIPHER_CTX_new();
}

EVP_MAC_CTX *
dssh_test_EVP_MAC_CTX_new(EVP_MAC *mac)
{
	if (should_fail())
		return NULL;
	return EVP_MAC_CTX_new(mac);
}

EVP_MAC *
dssh_test_EVP_MAC_fetch(OSSL_LIB_CTX *ctx, const char *algorithm,
    const char *properties)
{
	if (should_fail())
		return NULL;
	return EVP_MAC_fetch(ctx, algorithm, properties);
}

EVP_MD *
dssh_test_EVP_MD_fetch(OSSL_LIB_CTX *ctx, const char *algorithm,
    const char *properties)
{
	if (should_fail())
		return NULL;
	return EVP_MD_fetch(ctx, algorithm, properties);
}

EVP_PKEY_CTX *
dssh_test_EVP_PKEY_CTX_new(EVP_PKEY *pkey, ENGINE *e)
{
	if (should_fail())
		return NULL;
	return EVP_PKEY_CTX_new(pkey, e);
}

EVP_PKEY_CTX *
dssh_test_EVP_PKEY_CTX_new_id(int id, ENGINE *e)
{
	if (should_fail())
		return NULL;
	return EVP_PKEY_CTX_new_id(id, e);
}

EVP_PKEY_CTX *
dssh_test_EVP_PKEY_CTX_new_from_name(OSSL_LIB_CTX *ctx,
    const char *name, const char *propquery)
{
	if (should_fail())
		return NULL;
	return EVP_PKEY_CTX_new_from_name(ctx, name, propquery);
}

EVP_PKEY *
dssh_test_EVP_PKEY_new_raw_public_key(int type, ENGINE *e,
    const unsigned char *pub, size_t len)
{
	if (should_fail())
		return NULL;
	return EVP_PKEY_new_raw_public_key(type, e, pub, len);
}

BN_CTX *
dssh_test_BN_CTX_new(void)
{
	if (should_fail())
		return NULL;
	return BN_CTX_new();
}

BIGNUM *
dssh_test_BN_new(void)
{
	if (should_fail())
		return NULL;
	return BN_new();
}

OSSL_PARAM_BLD *
dssh_test_OSSL_PARAM_BLD_new(void)
{
	if (should_fail())
		return NULL;
	return OSSL_PARAM_BLD_new();
}

OSSL_PARAM *
dssh_test_OSSL_PARAM_BLD_to_param(OSSL_PARAM_BLD *bld)
{
	if (should_fail())
		return NULL;
	return OSSL_PARAM_BLD_to_param(bld);
}

/* ================================================================
 * OpenSSL operation wrappers (return 0 or <=0 on failure)
 * ================================================================ */

int
dssh_test_EVP_DigestInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type,
    ENGINE *impl)
{
	if (should_fail())
		return 0;
	return EVP_DigestInit_ex(ctx, type, impl);
}

int
dssh_test_EVP_DigestUpdate(EVP_MD_CTX *ctx, const void *d,
    size_t cnt)
{
	if (should_fail())
		return 0;
	return EVP_DigestUpdate(ctx, d, cnt);
}

int
dssh_test_EVP_DigestFinal_ex(EVP_MD_CTX *ctx, unsigned char *md,
    unsigned int *s)
{
	if (should_fail())
		return 0;
	return EVP_DigestFinal_ex(ctx, md, s);
}

int
dssh_test_EVP_DigestSignInit(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx,
    const EVP_MD *type, ENGINE *e, EVP_PKEY *pkey)
{
	if (should_fail())
		return 0;
	return EVP_DigestSignInit(ctx, pctx, type, e, pkey);
}

int
dssh_test_EVP_DigestSign(EVP_MD_CTX *ctx, unsigned char *sigret,
    size_t *siglen, const unsigned char *tbs, size_t tbslen)
{
	if (should_fail())
		return 0;
	return EVP_DigestSign(ctx, sigret, siglen, tbs, tbslen);
}

int
dssh_test_EVP_DigestVerifyInit(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx,
    const EVP_MD *type, ENGINE *e, EVP_PKEY *pkey)
{
	if (should_fail())
		return 0;
	return EVP_DigestVerifyInit(ctx, pctx, type, e, pkey);
}

int
dssh_test_EVP_DigestVerify(EVP_MD_CTX *ctx, const unsigned char *sigret,
    size_t siglen, const unsigned char *tbs, size_t tbslen)
{
	if (should_fail())
		return 0;
	return EVP_DigestVerify(ctx, sigret, siglen, tbs, tbslen);
}

int
dssh_test_EVP_EncryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
    ENGINE *impl, const unsigned char *key, const unsigned char *iv)
{
	if (should_fail())
		return 0;
	return EVP_EncryptInit_ex(ctx, cipher, impl, key, iv);
}

int
dssh_test_EVP_EncryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out,
    int *outl, const unsigned char *in, int inl)
{
	if (should_fail())
		return 0;
	return EVP_EncryptUpdate(ctx, out, outl, in, inl);
}

int
dssh_test_EVP_MAC_init(EVP_MAC_CTX *ctx, const unsigned char *key,
    size_t keylen, const OSSL_PARAM params[])
{
	if (should_fail())
		return 0;
	return EVP_MAC_init(ctx, key, keylen, params);
}

int
dssh_test_EVP_MAC_update(EVP_MAC_CTX *ctx, const unsigned char *data,
    size_t datalen)
{
	if (should_fail())
		return 0;
	return EVP_MAC_update(ctx, data, datalen);
}

int
dssh_test_EVP_MAC_final(EVP_MAC_CTX *ctx, unsigned char *out,
    size_t *outl, size_t outsize)
{
	if (should_fail())
		return 0;
	return EVP_MAC_final(ctx, out, outl, outsize);
}

int
dssh_test_EVP_PKEY_keygen_init(EVP_PKEY_CTX *ctx)
{
	if (should_fail())
		return 0;
	return EVP_PKEY_keygen_init(ctx);
}

int
dssh_test_EVP_PKEY_keygen(EVP_PKEY_CTX *ctx, EVP_PKEY **ppkey)
{
	if (should_fail())
		return 0;
	return EVP_PKEY_keygen(ctx, ppkey);
}

int
dssh_test_EVP_PKEY_derive_init(EVP_PKEY_CTX *ctx)
{
	if (should_fail())
		return 0;
	return EVP_PKEY_derive_init(ctx);
}

int
dssh_test_EVP_PKEY_derive_set_peer(EVP_PKEY_CTX *ctx, EVP_PKEY *peer)
{
	if (should_fail())
		return 0;
	return EVP_PKEY_derive_set_peer(ctx, peer);
}

int
dssh_test_EVP_PKEY_derive(EVP_PKEY_CTX *ctx, unsigned char *key,
    size_t *keylen)
{
	if (should_fail())
		return 0;
	return EVP_PKEY_derive(ctx, key, keylen);
}

int
dssh_test_EVP_PKEY_fromdata_init(EVP_PKEY_CTX *ctx)
{
	if (should_fail())
		return 0;
	return EVP_PKEY_fromdata_init(ctx);
}

int
dssh_test_EVP_PKEY_fromdata(EVP_PKEY_CTX *ctx, EVP_PKEY **ppkey,
    int selection, OSSL_PARAM params[])
{
	if (should_fail())
		return 0;
	return EVP_PKEY_fromdata(ctx, ppkey, selection, params);
}

int
dssh_test_EVP_PKEY_get_raw_public_key(const EVP_PKEY *pkey,
    unsigned char *pub, size_t *len)
{
	if (should_fail())
		return 0;
	return EVP_PKEY_get_raw_public_key(pkey, pub, len);
}

int
dssh_test_EVP_PKEY_get_bn_param(const EVP_PKEY *pkey,
    const char *key_name, BIGNUM **bn)
{
	if (should_fail())
		return 0;
	return EVP_PKEY_get_bn_param(pkey, key_name, bn);
}

int
dssh_test_EVP_Digest(const void *data, size_t count, unsigned char *md,
    unsigned int *size, const EVP_MD *type, ENGINE *impl)
{
	if (should_fail())
		return 0;
	return EVP_Digest(data, count, md, size, type, impl);
}

int
dssh_test_RAND_bytes(unsigned char *buf, int num)
{
	if (should_fail())
		return 0;
	return RAND_bytes(buf, num);
}

int
dssh_test_BN_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx)
{
	if (should_fail())
		return 0;
	return BN_mod_exp(r, a, p, m, ctx);
}

int
dssh_test_BN_rand(BIGNUM *rnd, int bits, int top, int bottom)
{
	if (should_fail())
		return 0;
	return BN_rand(rnd, bits, top, bottom);
}

BIGNUM *
dssh_test_BN_bin2bn(const unsigned char *s, int len, BIGNUM *ret)
{
	if (should_fail())
		return NULL;
	return BN_bin2bn(s, len, ret);
}

int
dssh_test_OSSL_PARAM_BLD_push_BN(OSSL_PARAM_BLD *bld,
    const char *key, const BIGNUM *bn)
{
	if (should_fail())
		return 0;
	return OSSL_PARAM_BLD_push_BN(bld, key, bn);
}

int
dssh_test_EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *ctx, int pad)
{
	if (should_fail())
		return 0;
	return EVP_CIPHER_CTX_set_padding(ctx, pad);
}

int
dssh_test_EVP_PKEY_CTX_set_params(EVP_PKEY_CTX *ctx,
    const OSSL_PARAM *params)
{
	if (should_fail())
		return 0;
	return EVP_PKEY_CTX_set_params(ctx, params);
}

int
dssh_test_EVP_PKEY_CTX_set_rsa_padding(EVP_PKEY_CTX *ctx, int pad_mode)
{
	if (should_fail())
		return 0;
	return EVP_PKEY_CTX_set_rsa_padding(ctx, pad_mode);
}

/* ================================================================
 * C11 threads wrappers
 * ================================================================ */

int
dssh_test_mtx_init(mtx_t *mtx, int type)
{
	if (should_fail())
		return thrd_error;
	return mtx_init(mtx, type);
}

int
dssh_test_cnd_init(cnd_t *cond)
{
	if (should_fail())
		return thrd_error;
	return cnd_init(cond);
}

int
dssh_test_thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
	if (should_fail())
		return thrd_error;
	return thrd_create(thr, func, arg);
}

/*
 * Lock/unlock/condvar wrappers.
 *
 * mtx_lock, cnd_wait, cnd_timedwait: skip the real call on injected
 * failure.  Safe because the caller does not hold the lock on error.
 *
 * mtx_unlock, cnd_broadcast, cnd_signal: always call the real function,
 * return thrd_error on injected failure.  Prevents test deadlocks and
 * missed wakeups while still exercising the library error path.
 */

int
dssh_test_mtx_lock(mtx_t *mtx)
{
	if (should_thrd_fail())
		return thrd_error;
	return mtx_lock(mtx);
}

int
dssh_test_mtx_unlock(mtx_t *mtx)
{
	int ret = mtx_unlock(mtx);
	if (should_thrd_fail())
		return thrd_error;
	return ret;
}

int
dssh_test_cnd_wait(cnd_t *cond, mtx_t *mtx)
{
	if (should_thrd_fail())
		return thrd_error;
	return cnd_wait(cond, mtx);
}

int
dssh_test_cnd_timedwait(cnd_t *cond, mtx_t *mtx,
    const struct timespec *ts)
{
	if (should_thrd_fail())
		return thrd_error;
	return cnd_timedwait(cond, mtx, ts);
}

int
dssh_test_cnd_broadcast(cnd_t *cond)
{
	int ret = cnd_broadcast(cond);
	if (should_thrd_fail())
		return thrd_error;
	return ret;
}

int
dssh_test_cnd_signal(cnd_t *cond)
{
	int ret = cnd_signal(cond);
	if (should_thrd_fail())
		return thrd_error;
	return ret;
}
