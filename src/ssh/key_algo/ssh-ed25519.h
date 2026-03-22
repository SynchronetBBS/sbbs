#ifndef SSH_ED25519_H
#define SSH_ED25519_H

#include "deucessh.h"

int register_ssh_ed25519(void);
int ssh_ed25519_load_key_file(deuce_ssh_session sess, const char *path);
int ssh_ed25519_generate_key(deuce_ssh_session sess);

#endif
