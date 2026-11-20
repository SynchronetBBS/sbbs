/*
 * deucessh-lang.h -- Per-session KEXINIT language preference fields.
 */

#ifndef DSSH_LANG_H
#define DSSH_LANG_H

#include "deucessh.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Set one outgoing field to these language tags in this order. The library
 * copies the strings. names == NULL or count == 0 clears the field. Tags use
 * RFC 3066 syntax. An empty field is the default. The two directions are
 * independent. A replacement applies to the next KEXINIT whose fields have
 * not yet been captured, including subsequent rekey offers.
 * Return zero or a negative error. Failed replacement preserves the field.
 * Calls after termination return DSSH_ERROR_TERMINATED. */
DSSH_PUBLIC int dssh_session_set_languages(dssh_session sess,
    enum dssh_direction direction, const char * const *names, size_t count) DSSH_ABI(dssh_session_set_languages);

/* Copy a complete comma-separated field as a NUL-terminated string. The
 * successful return is its required buffer size INCLUDING the NUL, matching
 * the public key-string getters. buf == NULL or bufsz == 0 queries that size.
 * Insufficient capacity returns DSSH_ERROR_TOOLONG without writing the buffer.
 * A size query and a later copy are separate snapshots.
 *
 * Local reads return the configured field for the next offer. Peer reads
 * return the exact field from the latest complete, validated peer KEXINIT,
 * including rekey offers. Before the first such packet, peer reads return
 * DSSH_ERROR_INIT. Peer values are preserved for application interpretation.
 * These reads remain available after termination. Applications choose how
 * to use the preferences.
 * NULL sessions and unknown directions return DSSH_ERROR_INVALID.
 * The caller coordinates session cleanup with calls using the handle. */
DSSH_PUBLIC int64_t dssh_session_get_languages(dssh_session sess,
    enum dssh_direction direction, char *buf, size_t bufsz) DSSH_ABI(dssh_session_get_languages);
DSSH_PUBLIC int64_t dssh_session_get_peer_languages(dssh_session sess,
    enum dssh_direction direction, char *buf, size_t bufsz) DSSH_ABI(dssh_session_get_peer_languages);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_LANG_H */
