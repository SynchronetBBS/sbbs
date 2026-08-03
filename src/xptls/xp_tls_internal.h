#ifndef _XP_TLS_INTERNAL_H
#define _XP_TLS_INTERNAL_H

#include "xp_tls.h"
#include "xp_ca.h"

#if defined(__cplusplus)
extern "C" {
#endif

const void *xp_tls_credentials_chain_pem(
	xp_tls_server_credentials_t credentials, size_t *length);
xp_key_t xp_tls_credentials_key(xp_tls_server_credentials_t credentials);
xp_tls_t xp_tls_provider_server_open(
	SOCKET socket, const void *chain_pem, size_t chain_pem_length,
	xp_key_t private_key, const struct xp_tls_server_config *config);
int xp_ca_cert_tls_server_usable(xp_ca_cert_t certificate);

#if defined(__cplusplus)
}
#endif

#endif
