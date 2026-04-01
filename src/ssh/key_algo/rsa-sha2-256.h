#ifndef RSA_SHA2_256_H
#define RSA_SHA2_256_H

#include "deucessh-algorithms.h"

#ifdef __cplusplus
extern "C" {
#endif

DSSH_PUBLIC int     dssh_register_rsa_sha2_256(void);
DSSH_PUBLIC int     dssh_rsa_sha2_256_load_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata);
DSSH_PUBLIC int     dssh_rsa_sha2_256_save_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata);
DSSH_PUBLIC int64_t dssh_rsa_sha2_256_get_pub_str(char *buf, size_t bufsz);
DSSH_PUBLIC int     dssh_rsa_sha2_256_save_pub_file(const char *path);
DSSH_PUBLIC int     dssh_rsa_sha2_256_generate_key(unsigned int bits);

#ifdef __cplusplus
}
#endif

#endif
