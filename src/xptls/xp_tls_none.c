/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * Licensed under the BSD 2-Clause License. See LICENSE in this directory.	*
 ****************************************************************************/

/*
 * xp_tls — stub backend.  Selected when xpdev is built WITHOUT_CRYPTO;
 * xp_tls_client_open_config() always fails, and the other entry points are
 * no-ops that return an error code / a fixed "TLS disabled" message.
 *
 * Consumers still compile + link against the xp_tls_* symbols; TLS
 * connections simply fail at open time.
 */

#include <stddef.h>

#include "xp_tls.h"
#include "xp_tls_internal.h"

xp_tls_t
xp_tls_client_open_config(SOCKET sock, const struct xp_tls_client_config *config)
{
	(void)sock;
	if (config != NULL) {
		const struct xp_tls_logger logger = {config->log_cb,
		    config->log_cb_arg, config->log_level, "none"};
		xp_tls_log_emit(&logger, XP_TLS_LOG_ERROR,
		    XP_TLS_LOG_SOURCE_LIBRARY, XP_TLS_ERR, 0, false,
		    "TLS disabled at build time (WITHOUT_CRYPTO)");
	}
	return NULL;
}

xp_tls_t
xp_tls_provider_server_open(SOCKET socket, const void *chain_pem,
	size_t chain_pem_length, xp_key_t private_key,
	const struct xp_tls_server_config *config)
{
	(void)socket; (void)chain_pem; (void)chain_pem_length;
	(void)private_key;
	if (config != NULL) {
		const struct xp_tls_logger logger = {config->log_cb,
		    config->log_cb_arg, config->log_level, "none"};
		xp_tls_log_emit(&logger, XP_TLS_LOG_ERROR,
		    XP_TLS_LOG_SOURCE_LIBRARY, XP_TLS_ERR, 0, false,
		    "TLS disabled at build time (WITHOUT_CRYPTO)");
	}
	return NULL;
}

enum xp_tls_version
xp_tls_protocol_version(xp_tls_t ctx)
{
	(void)ctx;
	return XP_TLS_VERSION_UNKNOWN;
}

int xp_tls_pop_timeout(xp_tls_t ctx, void *buf, size_t n, size_t *copied, int timeout_ms)
{
	(void)ctx; (void)buf; (void)n; (void)timeout_ms;
	if (copied != NULL) *copied = 0;
	return XP_TLS_ERR_CLOSED;
}
int xp_tls_push_timeout(xp_tls_t ctx, const void *buf, size_t n, size_t *copied, int timeout_ms)
{
	(void)ctx; (void)buf; (void)n; (void)timeout_ms;
	if (copied != NULL) *copied = 0;
	return XP_TLS_ERR;
}
int xp_tls_flush_timeout(xp_tls_t ctx, int timeout_ms)
{
	(void)ctx; (void)timeout_ms; return XP_TLS_ERR;
}
const char *xp_tls_cipher_name(xp_tls_t ctx) { (void)ctx; return NULL; }
enum xp_tls_auth_method xp_tls_authentication_method(xp_tls_t ctx)
{
	(void)ctx; return XP_TLS_AUTH_NONE;
}
int xp_tls_psk_identity(xp_tls_t ctx, void *out, size_t *len)
{
	(void)ctx; (void)out; if (len != NULL) *len = 0; return XP_CRYPTO_ERR_DISABLED;
}
size_t xp_tls_peer_certificate_count(xp_tls_t ctx) { (void)ctx; return 0; }
int xp_tls_peer_certificate_der(xp_tls_t ctx, size_t index, void *out, size_t *len)
{
	(void)ctx; (void)index; (void)out; (void)len; return XP_CRYPTO_ERR_DISABLED;
}
int xp_tls_terminate(xp_tls_t ctx)
{
	(void)ctx; return XP_TLS_ERR;
}

bool
xp_tls_has_pending(xp_tls_t ctx)
{
	(void)ctx;
	return false;
}

bool
xp_tls_used_psk(xp_tls_t ctx)
{
	(void)ctx;
	return false;
}

void
xp_tls_close(xp_tls_t ctx, bool close_socket)
{
	(void)ctx; (void)close_socket;
}

const char *
xp_tls_errstr(xp_tls_t ctx)
{
	(void)ctx;
	return "TLS disabled at build time (WITHOUT_CRYPTO)";
}

const char *
xp_tls_last_err(void)
{
	return "TLS disabled at build time (WITHOUT_CRYPTO)";
}
