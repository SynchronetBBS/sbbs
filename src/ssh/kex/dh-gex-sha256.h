#ifndef DH_GEX_SHA256_H
#define DH_GEX_SHA256_H

#include "deucessh.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DH group provider: optionally implemented by the server application
 * to override the built-in RFC 3526 group selection.
 * select_group returns 0 on success.  p and g are big-endian byte
 * arrays; the caller frees them.
 *
 * Pass to dssh_kex_set_ctx("diffie-hellman-group-exchange-sha256", &prov)
 * before dssh_session_init().
 */
struct dssh_dh_gex_provider {
	int   (*select_group)(uint32_t min, uint32_t preferred, uint32_t max, uint8_t **p, size_t *p_len,
            uint8_t **g, size_t *g_len, void *cbdata);
	void *cbdata;
};

DSSH_PUBLIC int dssh_register_dh_gex_sha256(void);

#ifdef __cplusplus
}
#endif

#endif   // ifndef DH_GEX_SHA256_H
