/*
 * key_algo/ssh-ed25519-botan.c -- Registration for Ed25519 (Botan3).
 */

#include <stdlib.h>
#include <string.h>

#include "deucessh.h"
#include "deucessh-key-algo.h"

#define ED25519_NAME     "ssh-ed25519"
#define ED25519_NAME_LEN 11

/* Implemented in ssh-ed25519-botan.cpp */
extern int dssh_botan_ed25519_sign(uint8_t **out, size_t *outlen,
    const uint8_t *data, size_t data_len, dssh_key_algo_ctx *ctx);
extern int dssh_botan_ed25519_verify(const uint8_t *key_blob, size_t key_blob_len,
    const uint8_t *sig_blob, size_t sig_blob_len,
    const uint8_t *data, size_t data_len);
extern int dssh_botan_ed25519_pubkey(const uint8_t **out, size_t *outlen,
    dssh_key_algo_ctx *ctx);
extern int dssh_botan_ed25519_haskey(dssh_key_algo_ctx *ctx);
extern void dssh_botan_ed25519_cleanup(dssh_key_algo_ctx *ctx);

DSSH_PUBLIC int
dssh_register_ssh_ed25519(void)
{
	struct dssh_key_algo_s *ka = malloc(sizeof(*ka) + ED25519_NAME_LEN + 1);
	if (ka == NULL)
		return DSSH_ERROR_ALLOC;
	ka->next = NULL;
	ka->sign = dssh_botan_ed25519_sign;
	ka->verify = dssh_botan_ed25519_verify;
	ka->pubkey = dssh_botan_ed25519_pubkey;
	ka->haskey = dssh_botan_ed25519_haskey;
	ka->cleanup = dssh_botan_ed25519_cleanup;
	ka->ctx = NULL;
	ka->flags = DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
	memcpy(ka->name, ED25519_NAME, ED25519_NAME_LEN + 1);
	return dssh_transport_register_key_algo(ka);
}
