/*
 * deucessh-enc.h -- Encryption algorithm module interface.
 */

#ifndef DSSH_ENC_H
#define DSSH_ENC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dssh_enc_ctx dssh_enc_ctx;

typedef int  (*dssh_enc_init)(const uint8_t *key, const uint8_t *iv, bool encrypt, dssh_enc_ctx **ctx);
typedef int  (*dssh_enc_crypt)(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx);
typedef void (*dssh_enc_cleanup)(dssh_enc_ctx *ctx);

typedef struct dssh_enc_s {
	struct dssh_enc_s *next; /* must be first -- generic traversal assumes offsetof(next) == 0 */
	dssh_enc_init      init;
	dssh_enc_crypt     encrypt;
	dssh_enc_crypt     decrypt;
	dssh_enc_cleanup   cleanup;
	/* Per-direction byte limit before auto-rekey is required.  Set
	 * by the module per RFC 4344 s3.2: 2^(L/4) blocks for L-bit
	 * block ciphers (64 GiB for 128-bit blocks like AES, 1 GiB for
	 * smaller blocks).  Set to UINT64_MAX to opt out (e.g. the
	 * none-cipher).  Compared independently against tx and rx
	 * counters, so c2s and s2c may use different limits. */
	uint64_t           bytes_per_key;
	uint32_t           flags;
	uint16_t           blocksize;
	uint16_t           key_size;
	char               name[];
} *dssh_enc;

DSSH_PUBLIC int dssh_transport_register_enc(dssh_enc enc);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_ENC_H */
