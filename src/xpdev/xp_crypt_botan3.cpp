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
 ****************************************************************************/

/*
 * xp_crypt — Botan 3 backend (placeholder).
 *
 * Selecting XP_CRYPTO_BACKEND=Botan builds this file so the Botan
 * xp_tls shim can ship without pulling in OpenSSL.  The real Botan
 * implementation of PBKDF2-HMAC-SHA256 + symmetric ciphers is queued
 * as task #15 — for now, every entry point returns NULL / error so
 * callers of the encrypted-INI path get a clean failure rather than
 * a link error.
 */

#include <stdbool.h>
#include <stddef.h>

#include "xp_crypt.h"

extern "C" {

void
xp_crypt_register_legacy_decrypt(int, xp_crypt_legacy_decrypt_fn_t)
{
}

xp_crypt_t
xp_crypt_open(int, int, const void *, size_t, int, const void *, size_t, bool)
{
	return nullptr;
}

int  xp_crypt_blocksize(xp_crypt_t) { return 0; }
int  xp_crypt_ivsize(xp_crypt_t)    { return 0; }
int  xp_crypt_set_iv(xp_crypt_t, const void *, size_t) { return -1; }
int  xp_crypt_gen_iv(xp_crypt_t, void *, size_t)       { return -1; }
int  xp_crypt_process(xp_crypt_t, void *, size_t)      { return -1; }
void xp_crypt_close(xp_crypt_t) {}

}	/* extern "C" */
