/*
 * deucessh-lang.h -- Language tag module interface.
 *
 * Language tags are associated with human-readable strings in the SSH
 * protocol (RFC 4253 s7.1, RFC 4254 s5.4).  Applications register
 * language preferences before creating any session; the library
 * advertises them in KEXINIT and makes the peer's preferences
 * available after handshake.
 */

#ifndef DSSH_LANG_H
#define DSSH_LANG_H

#include <stddef.h>

#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dssh_language_s {
	struct dssh_language_s *next;  /* must be first -- generic traversal assumes offsetof(next) == 0 */
	char                    name[];
} *dssh_language;
static_assert(!offsetof(struct dssh_language_s, next),
    "next must be at offset 0 for generic list traversal");

DSSH_PUBLIC int dssh_transport_register_lang(dssh_language lang);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_LANG_H */
