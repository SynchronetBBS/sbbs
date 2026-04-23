/* Copyright (C), 2026 by Stephen Hurd */
/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Encrypted INI file API — a layer on top of xpdev's plain iniReadFile /
 * iniWriteFile, backed by xp_crypt (OpenSSL or Botan 3) and a KDF over a
 * user-supplied passphrase.  Used by SyncTERM for the bbslist.ini
 * optional encryption feature.
 *
 * The implementation used to live in xpdev (ini_crypt.c) but was moved
 * to syncterm because SyncTERM is the only consumer — keeping it here
 * means libxpdev stays crypto-free and Synchronet server binaries don't
 * drag in OpenSSL / Botan 3 through the xpdev link.
 */

#ifndef _INI_CRYPT_H_
#define _INI_CRYPT_H_

#include <stdbool.h>
#include <stdio.h>

#include "str_list.h"  /* str_list_t */
#include "wrapdll.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Symmetric ciphers supported for encrypted INI files.
 *
 * Values are stable and match enum xp_crypt_algo in xp_crypt.h — the
 * numeric value is used only at runtime; the on-disk format encodes the
 * algorithm as its ASCII name (iniCryptGetAlgoName), so these integers
 * are not part of the file format.
 *
 * Read-only legacy ciphers (IDEA, RC2 when the backend lacks them) are
 * served by xp_crypt's legacy-decrypt registry; consumers register
 * handlers at init time. See syncterm/legacy_ciphers/ for an example.
 */
enum iniCryptAlgo {
	INI_CRYPT_ALGO_NONE     = 0,
	INI_CRYPT_ALGO_3DES     = 1,
	INI_CRYPT_ALGO_IDEA     = 2,
	INI_CRYPT_ALGO_CAST     = 3,
	INI_CRYPT_ALGO_RC2      = 4,
	INI_CRYPT_ALGO_RC4      = 5,
	INI_CRYPT_ALGO_AES      = 6,
	INI_CRYPT_ALGO_CHACHA20 = 7
};

DLLEXPORT str_list_t iniReadEncryptedFile(FILE* fp, bool(*get_key)(void *cb_data, char *keybuf, size_t *sz), int KDFiterations, enum iniCryptAlgo *algoPtr, int *ks, char *saltBuf, size_t *saltsz, void *cbdata);
DLLEXPORT bool iniWriteEncryptedFile(FILE* fp, const str_list_t list, enum iniCryptAlgo algo, int keySize, int KDFiterations, const char *key, char *salt);
DLLEXPORT const char *iniCryptGetAlgoName(enum iniCryptAlgo a);
DLLEXPORT enum iniCryptAlgo iniCryptGetAlgoFromName(const char *n);

#if defined(__cplusplus)
}
#endif

#endif
