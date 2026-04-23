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
 * xp_crypt — stub backend.  Selected when xpdev is built WITHOUT_CRYPTO;
 * every entry point returns NULL / an error code.  Consumers that still
 * call xp_crypt_open() (e.g. ini_file.c on an encrypted file) degrade
 * gracefully: the decrypt/encrypt path fails and the caller reports
 * the file as unreadable/unwritable.
 *
 * Exists so xpdev's library contains the xp_crypt_* symbols regardless
 * of whether a real crypto backend is linked — consumers don't have to
 * conditionally compile calls to it.
 */

#include <stddef.h>

#include "xp_crypt.h"

void
xp_crypt_register_legacy_decrypt(int algo, xp_crypt_legacy_decrypt_fn_t fn)
{
	(void)algo;
	(void)fn;
}

xp_crypt_t
xp_crypt_open(int algo, int key_bits,
              const void *salt, size_t slen,
              const struct xp_crypt_kdf_params *kdf,
              const void *pass, size_t plen,
              bool encrypt)
{
	(void)algo; (void)key_bits;
	(void)salt; (void)slen;
	(void)kdf;
	(void)pass; (void)plen;
	(void)encrypt;
	return NULL;
}

int
xp_crypt_blocksize(xp_crypt_t ctx)
{
	(void)ctx;
	return 0;
}

int
xp_crypt_ivsize(xp_crypt_t ctx)
{
	(void)ctx;
	return 0;
}

int
xp_crypt_set_iv(xp_crypt_t ctx, const void *iv, size_t n)
{
	(void)ctx; (void)iv; (void)n;
	return -1;
}

int
xp_crypt_gen_iv(xp_crypt_t ctx, void *out, size_t out_size)
{
	(void)ctx; (void)out; (void)out_size;
	return -1;
}

int
xp_crypt_process(xp_crypt_t ctx, void *buf, size_t n)
{
	(void)ctx; (void)buf; (void)n;
	return -1;
}

void
xp_crypt_close(xp_crypt_t ctx)
{
	(void)ctx;
}
