/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 ****************************************************************************/

/*
 * xp_crypt — symmetric cipher wrapper for xpdev.
 *
 * Thin, backend-agnostic API over PBKDF2-HMAC-SHA256 key derivation and a
 * small set of symmetric ciphers, used by ini_file.c for encrypted INI
 * files. Backend is OpenSSL libcrypto or Botan3, selected at build time.
 *
 * Read-only support for legacy ciphers (IDEA, RC2, ...) can be layered
 * in by consumers via xp_crypt_register_legacy_decrypt(); see syncterm's
 * legacy_ciphers/ for the usage pattern.
 */

#ifndef _XP_CRYPT_H
#define _XP_CRYPT_H

#include <stdbool.h>
#include <stddef.h>

#include "wrapdll.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Algorithm identifiers.
 *
 * Values match the encrypted-INI file format's enum iniCryptAlgo for
 * zero-cost interop — ini_file.c stores the same integer.
 */
enum xp_crypt_algo {
	XP_CRYPT_ALGO_NONE     = 0,
	XP_CRYPT_ALGO_3DES     = 1,
	XP_CRYPT_ALGO_IDEA     = 2,
	XP_CRYPT_ALGO_CAST     = 3,
	XP_CRYPT_ALGO_RC2      = 4,
	XP_CRYPT_ALGO_RC4      = 5,
	XP_CRYPT_ALGO_AES      = 6,
	XP_CRYPT_ALGO_CHACHA20 = 7,
	XP_CRYPT_ALGO_MAX
};

#define XP_CRYPT_MAX_IVSIZE   16    /* AES block; covers 3DES/IDEA/CAST/RC2 too */
#define XP_CRYPT_MAX_KEYSIZE  32    /* AES-256 / ChaCha20 key */

typedef struct xp_crypt_ctx *xp_crypt_t;

/*
 * Decrypt-only callback for legacy ciphers unavailable in the chosen
 * backend. The PBKDF2-derived key and IV (if any) arrive pre-cooked.
 * Returns 0 on success, non-zero on error. Must decrypt in place.
 */
typedef int (*xp_crypt_legacy_decrypt_fn_t)(
    int key_bits,
    const void *key, size_t key_len,
    const void *iv,  size_t iv_len,
    void *buf, size_t n);

/*
 * Register a legacy decrypt handler. algo is an xp_crypt_algo value.
 * Only consulted when the active backend does not support algo.
 * Pass NULL to unregister. Not thread-safe w.r.t. xp_crypt_open().
 */
DLLEXPORT void xp_crypt_register_legacy_decrypt(int algo,
                                                xp_crypt_legacy_decrypt_fn_t fn);

/*
 * Open a cipher context.
 *
 * algo       — xp_crypt_algo value
 * key_bits   — cipher key size in bits (e.g. 256 for AES-256, 128 for AES-128,
 *              168 for 3DES, 256 for ChaCha20). Zero asks for the algorithm's
 *              default.
 * salt,slen  — salt bytes for PBKDF2 (raw, passed through as-is — no hashing
 *              or encoding)
 * iters      — PBKDF2-HMAC-SHA256 iteration count (must be >=1)
 * pass,plen  — password bytes (raw, used as PBKDF2 input key material)
 * encrypt    — true for encryption, false for decryption. Legacy-only ciphers
 *              (unsupported by backend) can only be opened with encrypt=false.
 *
 * Returns NULL on error.
 */
DLLEXPORT xp_crypt_t xp_crypt_open(int algo, int key_bits,
    const void *salt, size_t slen,
    int iters,
    const void *pass, size_t plen,
    bool encrypt);

/* Cipher block size in bytes. 0 or 1 means stream cipher. */
DLLEXPORT int  xp_crypt_blocksize(xp_crypt_t ctx);

/* IV size in bytes. 0 means the cipher carries no IV. */
DLLEXPORT int  xp_crypt_ivsize(xp_crypt_t ctx);

/* Load IV (for decryption; caller read it from the file). */
DLLEXPORT int  xp_crypt_set_iv(xp_crypt_t ctx, const void *iv, size_t n);

/*
 * Generate a random IV using the backend's CSPRNG and load it into the
 * context. out must have room for >= xp_crypt_ivsize(ctx) bytes; on
 * success the bytes are also written there so the caller can emit them
 * to the file header.
 */
DLLEXPORT int  xp_crypt_gen_iv(xp_crypt_t ctx, void *out, size_t out_size);

/*
 * Encrypt or decrypt n bytes in place. For block ciphers n must be a
 * multiple of xp_crypt_blocksize(ctx); ini_file.c enforces this by
 * reading/writing in block-sized chunks.
 */
DLLEXPORT int  xp_crypt_process(xp_crypt_t ctx, void *buf, size_t n);

DLLEXPORT void xp_crypt_close(xp_crypt_t ctx);

#if defined(__cplusplus)
}
#endif

#endif
