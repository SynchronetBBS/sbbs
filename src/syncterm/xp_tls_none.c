/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 ****************************************************************************/

/*
 * xp_tls — stub backend.  Selected when xpdev is built WITHOUT_CRYPTO;
 * xp_tls_client_open() always fails, and the other entry points are
 * no-ops that return an error code / a fixed "TLS disabled" message.
 *
 * Consumers still compile + link against the xp_tls_* symbols; TLS
 * connections simply fail at open time.
 */

#include <stddef.h>

#include "xp_tls.h"

xp_tls_t
xp_tls_client_open(SOCKET sock, const char *sni, int read_timeout)
{
	(void)sock; (void)sni; (void)read_timeout;
	return NULL;
}

int
xp_tls_push(xp_tls_t ctx, const void *buf, size_t n, size_t *copied)
{
	(void)ctx; (void)buf; (void)n;
	if (copied)
		*copied = 0;
	return XP_TLS_ERR;
}

int
xp_tls_pop(xp_tls_t ctx, void *buf, size_t n, size_t *copied)
{
	(void)ctx; (void)buf; (void)n;
	if (copied)
		*copied = 0;
	return XP_TLS_ERR_CLOSED;
}

int
xp_tls_flush(xp_tls_t ctx)
{
	(void)ctx;
	return XP_TLS_ERR;
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
