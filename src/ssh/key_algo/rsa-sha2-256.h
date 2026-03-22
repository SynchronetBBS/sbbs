#ifndef RSA_SHA2_256_H
#define RSA_SHA2_256_H

#include <openssl/pem.h>

#include "deucessh.h"

DEUCE_SSH_PUBLIC int register_rsa_sha2_256(void);
DEUCE_SSH_PUBLIC int rsa_sha2_256_load_key_file(const char *path,
    pem_password_cb *pw_cb, void *pw_cbdata);
DEUCE_SSH_PUBLIC int rsa_sha2_256_save_key_file(const char *path,
    pem_password_cb *pw_cb, void *pw_cbdata);
DEUCE_SSH_PUBLIC ssize_t rsa_sha2_256_get_pub_str(char *buf, size_t bufsz);
DEUCE_SSH_PUBLIC int rsa_sha2_256_save_pub_file(const char *path);
DEUCE_SSH_PUBLIC int rsa_sha2_256_generate_key(unsigned int bits);

#endif
