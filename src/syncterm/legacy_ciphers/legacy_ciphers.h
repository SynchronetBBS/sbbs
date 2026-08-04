#ifndef SYNCTERM_LEGACY_CIPHERS_H
#define SYNCTERM_LEGACY_CIPHERS_H

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

int legacy_idea_decrypt_cbc(const void *key, size_t key_len,
	const void *iv, void *buf, size_t length);
int legacy_rc2_decrypt_cbc(const void *key, size_t key_len,
	const void *iv, void *buf, size_t length);

#if defined(__cplusplus)
}
#endif

#endif
