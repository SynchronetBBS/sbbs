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

typedef int (*dssh_enc_init)(const uint8_t *key, const uint8_t *iv,
    bool encrypt, dssh_enc_ctx **ctx);
typedef int (*dssh_enc_crypt)(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx);
typedef void (*dssh_enc_cleanup)(dssh_enc_ctx *ctx);

typedef struct dssh_enc_s {
	struct dssh_enc_s *next;
	dssh_enc_init      init;
	dssh_enc_crypt     encrypt;
	dssh_enc_crypt     decrypt;
	dssh_enc_cleanup   cleanup;
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
