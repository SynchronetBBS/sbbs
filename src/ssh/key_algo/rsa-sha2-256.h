#ifndef RSA_SHA2_256_H
#define RSA_SHA2_256_H

#include "deucessh.h"

int register_rsa_sha2_256(void);
int rsa_sha2_256_load_key_file(deuce_ssh_session sess, const char *path);
int rsa_sha2_256_generate_key(deuce_ssh_session sess, unsigned int bits);

#endif
