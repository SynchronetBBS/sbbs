/* Private X25519 operations shared by the standalone and hybrid methods. */
#ifndef DSSH_X25519_OPS_H
#define DSSH_X25519_OPS_H

#include <stddef.h>
#include <stdint.h>
#include "deucessh-portable.h"

#define DSSH_X25519_LEN 32

#ifdef __cplusplus
extern "C" {
#endif

/* init creates reusable generation resources; NULL success is valid. Error
 * leaves NULL or cleanup-valid state. cleanup/free_priv both accept NULL.
 * keygen ALWAYS generates fresh material, writing the public key and publishing
 * a cleanup-valid private handle even on failure. derive writes exactly 32 bytes
 * into caller-owned storage and does not release the private key. The caller
 * serializes generator use and releases each private key promptly after derive.
 * Failed generation resources must be reinitialized before another exchange. */
struct dssh_x25519_ops {
	int (*init)(void **backend);
	void (*cleanup)(void *backend);
	int (*keygen)(void *backend, uint8_t public_key[DSSH_X25519_LEN], void **private_key);
	int (*derive)(void *private_key, const uint8_t *peer, size_t peer_length,
	    uint8_t secret[DSSH_X25519_LEN]);
	void (*free_priv)(void *private_key);
};

/* One immutable selected provider table; its code has process lifetime. */
extern DSSH_PRIVATE const struct dssh_x25519_ops dssh_x25519_backend;

#ifdef __cplusplus
}
#endif
#endif
