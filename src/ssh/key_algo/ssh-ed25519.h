#ifndef SSH_ED25519_H
#define SSH_ED25519_H

#include "deucessh-algorithms.h"

#ifdef __cplusplus
extern "C" {
#endif

DSSH_PUBLIC int     dssh_register_ssh_ed25519(void) DSSH_ABI(dssh_register_ssh_ed25519);
DSSH_PUBLIC int     dssh_ed25519_load_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata) DSSH_ABI(dssh_ed25519_load_key_file);
DSSH_PUBLIC int     dssh_ed25519_save_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata) DSSH_ABI(dssh_ed25519_save_key_file);
DSSH_PUBLIC int64_t dssh_ed25519_get_pub_str(char *buf, size_t bufsz) DSSH_ABI(dssh_ed25519_get_pub_str);
DSSH_PUBLIC int     dssh_ed25519_save_pub_file(const char *path) DSSH_ABI(dssh_ed25519_save_pub_file);
DSSH_PUBLIC int     dssh_ed25519_generate_key(void) DSSH_ABI(dssh_ed25519_generate_key);

#ifdef __cplusplus
}
#endif

#endif
