/* Private builtin constructors/exporters. Contexts stay with their producer. */
#ifndef DSSH_CLIENT_KEY_H
#define DSSH_CLIENT_KEY_H

#include "deucessh-algorithms.h"

struct dssh_key_algo_s;

struct dssh_client_key_ops {
	int (*load)(void **out, const char *path, dssh_pem_password_cb cb, void *cbdata);
	int (*generate)(void **out, unsigned int bits);
	int (*save)(void *ctx, const char *path, dssh_pem_password_cb cb, void *cbdata);
	void (*release)(void *ctx);
};

#ifdef __cplusplus
extern "C" {
#endif

/* Only return builtin operations when all context-using callbacks match the
 * actual builtin module. A custom registration may use the same wire name. */
DSSH_PRIVATE const struct dssh_client_key_ops *dssh_ed25519_client_ops(struct dssh_key_algo_s *module);
DSSH_PRIVATE const struct dssh_client_key_ops *dssh_rsa256_client_ops(struct dssh_key_algo_s *module);
DSSH_PRIVATE const struct dssh_client_key_ops *dssh_rsa512_client_ops(struct dssh_key_algo_s *module);

#ifdef __cplusplus
}
#endif

#endif
