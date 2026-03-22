#ifndef SSH_ED25519_H
#define SSH_ED25519_H

#include <openssl/pem.h>

#include "deucessh.h"

DEUCE_SSH_PUBLIC int register_ssh_ed25519(void);
DEUCE_SSH_PUBLIC int ssh_ed25519_load_key_file(const char *path,
    pem_password_cb *pw_cb, void *pw_cbdata);
DEUCE_SSH_PUBLIC int ssh_ed25519_save_key_file(const char *path,
    pem_password_cb *pw_cb, void *pw_cbdata);
DEUCE_SSH_PUBLIC ssize_t ssh_ed25519_get_pub_str(char *buf, size_t bufsz);
DEUCE_SSH_PUBLIC int ssh_ed25519_save_pub_file(const char *path);
DEUCE_SSH_PUBLIC int ssh_ed25519_generate_key(void);

#endif
