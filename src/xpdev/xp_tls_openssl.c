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
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 ****************************************************************************/

/*
 * xp_tls — OpenSSL (libssl) backend.
 *
 * Blocking SSL I/O with SO_RCVTIMEO for read-side timeouts: a read that
 * times out surfaces as SSL_ERROR_WANT_READ, which the wrapper reports
 * as zero bytes copied with OK status. This mirrors Cryptlib's
 * CRYPT_OPTION_NET_READTIMEOUT + CRYPT_ERROR_TIMEOUT pattern that the
 * telnets.c / webget.c consumers already loop on.
 *
 * Peer-certificate verification is disabled to match the Cryptlib-era
 * posture (SyncTERM connects to BBSes with self-signed certs, Synchronet
 * server doesn't verify client certs by default either). This is a
 * temporary carry-over; the caller should opt in to verification once
 * the consumer code is ready to prompt users on mismatch.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <winsock2.h>
#else
  #include <sys/socket.h>
  #include <sys/time.h>
#endif

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "xp_tls.h"

/* ------------------------------------------------------------ last-err */

/*
 * A thread-local string buffer that receives formatted errors from
 * xp_tls_client_open() when it fails before returning a ctx.
 */
static _Thread_local char last_err_buf[256];

const char *
xp_tls_last_err(void)
{
	if (last_err_buf[0] == 0)
		return "no error";
	return last_err_buf;
}

static void
set_last_err(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(last_err_buf, sizeof(last_err_buf), fmt, ap);
	va_end(ap);
}

/* Pull the top-of-stack OpenSSL error into buf. */
static void
format_ssl_err(char *buf, size_t bufsz, const char *op)
{
	unsigned long e = ERR_get_error();
	char oerr[160];
	if (e == 0) {
		snprintf(buf, bufsz, "%s: unknown error", op);
		return;
	}
	ERR_error_string_n(e, oerr, sizeof(oerr));
	snprintf(buf, bufsz, "%s: %s", op, oerr);
}

/* --------------------------------------------------------- context */

struct xp_tls_ctx {
	SOCKET   sock;
	SSL_CTX *sslctx;
	SSL     *ssl;
	int      read_timeout_sec;
	char     err[256];
};

/* --------------------------------------------------------- socket timeout */

static int
set_recv_timeout(SOCKET sock, int seconds)
{
	if (seconds <= 0)
		return 0;
#ifdef _WIN32
	DWORD tv = (DWORD)seconds * 1000;
	return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#else
	struct timeval tv;
	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#endif
}

/* --------------------------------------------------------- one-shot init */

/*
 * OpenSSL 1.1+ auto-inits; explicit init is a no-op on modern libs
 * but safe. Kept for clarity.
 */
static void
ensure_ssl_init(void)
{
	static bool done = false;
	if (done)
		return;
	done = true;
	(void)OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS |
	                      OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
}

/* ------------------------------------------------------------- open */

xp_tls_t
xp_tls_client_open(SOCKET sock, const char *sni, int read_timeout)
{
	struct xp_tls_ctx *ctx;
	int rc;

	last_err_buf[0] = 0;
	ensure_ssl_init();

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		set_last_err("xp_tls_client_open: out of memory");
		return NULL;
	}
	ctx->sock = sock;
	ctx->read_timeout_sec = read_timeout;

	ctx->sslctx = SSL_CTX_new(TLS_client_method());
	if (ctx->sslctx == NULL) {
		format_ssl_err(last_err_buf, sizeof(last_err_buf), "SSL_CTX_new");
		goto fail;
	}
	/* TLS 1.2 floor; stay at the OpenSSL default ceiling. */
	SSL_CTX_set_min_proto_version(ctx->sslctx, TLS1_2_VERSION);
	SSL_CTX_set_verify(ctx->sslctx, SSL_VERIFY_NONE, NULL);

	ctx->ssl = SSL_new(ctx->sslctx);
	if (ctx->ssl == NULL) {
		format_ssl_err(last_err_buf, sizeof(last_err_buf), "SSL_new");
		goto fail;
	}
	if (sni != NULL && sni[0] != 0) {
		/* SSL_set_tlsext_host_name returns 1 on success. */
		if (SSL_set_tlsext_host_name(ctx->ssl, sni) != 1) {
			format_ssl_err(last_err_buf, sizeof(last_err_buf),
			              "SSL_set_tlsext_host_name");
			goto fail;
		}
		/* SNI is informational; hostname binding is separate and would
		   matter if verification were on. Wire it for future-proofing. */
		(void)SSL_set1_host(ctx->ssl, sni);
	}

	if (set_recv_timeout(sock, read_timeout) != 0) {
		set_last_err("xp_tls_client_open: setsockopt SO_RCVTIMEO failed (%d)", errno);
		goto fail;
	}
	if (SSL_set_fd(ctx->ssl, (int)sock) != 1) {
		format_ssl_err(last_err_buf, sizeof(last_err_buf), "SSL_set_fd");
		goto fail;
	}

	rc = SSL_connect(ctx->ssl);
	if (rc != 1) {
		int e = SSL_get_error(ctx->ssl, rc);
		if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE)
			set_last_err("SSL_connect: handshake would block");
		else
			format_ssl_err(last_err_buf, sizeof(last_err_buf), "SSL_connect");
		goto fail;
	}

	return ctx;

fail:
	xp_tls_close(ctx, false);
	return NULL;
}

/* ------------------------------------------------------------- push */

int
xp_tls_push(xp_tls_t ctx, const void *buf, size_t n, size_t *copied)
{
	int rc;
	size_t written = 0;

	if (copied != NULL)
		*copied = 0;
	if (ctx == NULL || buf == NULL)
		return XP_TLS_ERR;
	if (n == 0)
		return XP_TLS_OK;

	rc = SSL_write_ex(ctx->ssl, buf, n, &written);
	if (copied != NULL)
		*copied = written;
	if (rc == 1)
		return XP_TLS_OK;

	int e = SSL_get_error(ctx->ssl, rc);
	switch (e) {
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			return XP_TLS_OK;	/* caller loops */
		case SSL_ERROR_ZERO_RETURN:
			return XP_TLS_ERR_CLOSED;
		case SSL_ERROR_SYSCALL:
			if (errno == EPIPE || errno == ECONNRESET)
				return XP_TLS_ERR_CLOSED;
			snprintf(ctx->err, sizeof(ctx->err),
			        "SSL_write syscall error (errno=%d)", errno);
			return XP_TLS_ERR;
		default:
			format_ssl_err(ctx->err, sizeof(ctx->err), "SSL_write");
			return XP_TLS_ERR;
	}
}

/* ------------------------------------------------------------- pop */

int
xp_tls_pop(xp_tls_t ctx, void *buf, size_t n, size_t *copied)
{
	int rc;
	size_t read = 0;

	if (copied != NULL)
		*copied = 0;
	if (ctx == NULL || buf == NULL)
		return XP_TLS_ERR;
	if (n == 0)
		return XP_TLS_OK;

	rc = SSL_read_ex(ctx->ssl, buf, n, &read);
	if (copied != NULL)
		*copied = read;
	if (rc == 1)
		return XP_TLS_OK;

	int e = SSL_get_error(ctx->ssl, rc);
	switch (e) {
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			/* SO_RCVTIMEO elapsed, or TLS record incomplete. */
			return XP_TLS_TIMEOUT;
		case SSL_ERROR_ZERO_RETURN:
			return XP_TLS_ERR_CLOSED;
		case SSL_ERROR_SYSCALL:
#ifdef EWOULDBLOCK
			if (errno == EWOULDBLOCK)
				return XP_TLS_TIMEOUT;
#endif
			if (errno == EAGAIN || errno == EINTR)
				return XP_TLS_TIMEOUT;
			if (errno == 0)	/* clean EOF without close_notify */
				return XP_TLS_ERR_CLOSED;
			if (errno == ECONNRESET || errno == EPIPE)
				return XP_TLS_ERR_CLOSED;
			snprintf(ctx->err, sizeof(ctx->err),
			        "SSL_read syscall error (errno=%d)", errno);
			return XP_TLS_ERR;
		default:
			format_ssl_err(ctx->err, sizeof(ctx->err), "SSL_read");
			return XP_TLS_ERR;
	}
}

/* ------------------------------------------------------------- flush */

int
xp_tls_flush(xp_tls_t ctx)
{
	/* OpenSSL doesn't buffer plaintext across SSL_write calls; the TLS
	   record layer emits each call's bytes on the wire. No-op. */
	(void)ctx;
	return XP_TLS_OK;
}

/* ------------------------------------------------------------- close */

void
xp_tls_close(xp_tls_t ctx, bool close_socket)
{
	if (ctx == NULL)
		return;
	if (ctx->ssl != NULL) {
		/* Best-effort graceful shutdown. A zero or negative return is
		   acceptable — the peer may have already closed. */
		(void)SSL_shutdown(ctx->ssl);
		SSL_free(ctx->ssl);
	}
	if (ctx->sslctx != NULL)
		SSL_CTX_free(ctx->sslctx);
	if (close_socket && ctx->sock != INVALID_SOCKET)
		closesocket(ctx->sock);
	free(ctx);
}

/* ------------------------------------------------------------- errstr */

const char *
xp_tls_errstr(xp_tls_t ctx)
{
	if (ctx == NULL)
		return "(null xp_tls_t)";
	if (ctx->err[0] == 0)
		return "no error";
	return ctx->err;
}
