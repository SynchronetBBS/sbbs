#ifndef DH_GEX_SHA256_H
#define DH_GEX_SHA256_H

#include "deucessh.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DH group provider: the server application implements this to
 * supply a safe prime and generator for the requested size range.
 * Returns 0 on success.  p and g are big-endian byte arrays;
 * the caller frees them.
 */
struct dssh_dh_gex_provider {
	int (*select_group)(uint32_t min, uint32_t preferred, uint32_t max,
	    uint8_t **p, size_t *p_len,
	    uint8_t **g, size_t *g_len,
	    void *cbdata);
	void *cbdata;
};

/*
 * Set the DH group provider for a session.
 * Must be called before key exchange on the server side.
 * The provider struct must remain valid for the session's lifetime.
 */
DSSH_PUBLIC void dssh_dh_gex_set_provider(dssh_session sess,
    struct dssh_dh_gex_provider *provider);

DSSH_PUBLIC int register_dh_gex_sha256(void);

#ifdef __cplusplus
}
#endif

#endif
