/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * Licensed under the BSD 2-Clause License. See LICENSE in this directory.	*
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

/* xp_tls.h pulls in sockwrap.h, which lands the OS socket + timeval
 * headers in the right order (winsock2 + ws2tcpip + wspiapi on Win32,
 * sys/socket + sys/time + friends on Unix).  Don't reach past it. */
#include "xp_tls.h"

#include <openssl/err.h>
#include <openssl/ssl.h>

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
	SOCKET         sock;
	SSL_CTX       *sslctx;
	SSL           *ssl;
	int            read_timeout_sec;
	char           err[256];
	/* PSK config: copies owned by the ctx so the OpenSSL callback can
	   read them at handshake time without holding pointers into the
	   caller's stack.  identity is NUL-terminated; psk is raw bytes. */
	char          *psk_identity;
	unsigned char *psk;
	size_t         psk_len;
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

/* ----------------------------------------------------------- PSK cb */

/*
 * OpenSSL invokes this during the ClientKeyExchange to fetch the PSK
 * identity and shared key.  We pull both from the ctx that was attached
 * via SSL_set_app_data() in xp_tls_client_open_psk().
 */
static unsigned int
psk_client_cb(SSL *ssl, const char *hint, char *id, unsigned int max_id_len,
              unsigned char *psk, unsigned int max_psk_len)
{
	struct xp_tls_ctx *ctx = SSL_get_app_data(ssl);
	(void)hint;
	if (ctx == NULL || ctx->psk_identity == NULL || ctx->psk == NULL)
		return 0;
	size_t id_len = strlen(ctx->psk_identity);
	if (id_len + 1 > max_id_len)
		return 0;
	if (ctx->psk_len > max_psk_len)
		return 0;
	memcpy(id, ctx->psk_identity, id_len + 1);
	memcpy(psk, ctx->psk, ctx->psk_len);
	return (unsigned int)ctx->psk_len;
}

/* ------------------------------------------------------------- open */

/*
 * Common open path for cert and PSK clients.  When identity/psk are both
 * NULL, runs as a normal cert client (peer-verify disabled).  When PSK is
 * supplied, configures the PSK client callback and pins the protocol
 * version to TLS 1.2 — TLS 1.3 PSK uses a different exchange that the
 * broker doesn't speak.
 */
static xp_tls_t
client_open_inner(SOCKET sock, const char *sni, int read_timeout,
                  const char *psk_identity, const void *psk, size_t psk_len)
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

	if (psk_identity != NULL && psk != NULL && psk_len > 0) {
		ctx->psk_identity = strdup(psk_identity);
		ctx->psk = malloc(psk_len);
		if (ctx->psk_identity == NULL || ctx->psk == NULL) {
			set_last_err("xp_tls_client_open: out of memory (PSK)");
			goto fail;
		}
		memcpy(ctx->psk, psk, psk_len);
		ctx->psk_len = psk_len;
	}

	ctx->sslctx = SSL_CTX_new(TLS_client_method());
	if (ctx->sslctx == NULL) {
		format_ssl_err(last_err_buf, sizeof(last_err_buf), "SSL_CTX_new");
		goto fail;
	}
	/* TLS 1.2 floor; stay at the OpenSSL default ceiling.  PSK pins
	   both ends to 1.2 — TLS 1.3 PSK is a different exchange that the
	   server side doesn't implement. */
	SSL_CTX_set_min_proto_version(ctx->sslctx, TLS1_2_VERSION);
	if (ctx->psk != NULL) {
		SSL_CTX_set_max_proto_version(ctx->sslctx, TLS1_2_VERSION);
		SSL_CTX_set_psk_client_callback(ctx->sslctx, psk_client_cb);
		/* PSK-only.  cryptlib's server-side prefers cert suites
		   when both PSK and cert are offered (TLS 1.2 cipher
		   selection is server-driven, so client ordering can't
		   force PSK), and a non-PSK negotiation puts the broker's
		   MQTT layer into a username/password path that expects a
		   different password format.  conn_mqtt.c handles fallback
		   to a cert-only second handshake when this PSK-only attempt
		   doesn't go through. */
		if (SSL_CTX_set_cipher_list(ctx->sslctx, "PSK") != 1) {
			format_ssl_err(last_err_buf, sizeof(last_err_buf),
			              "SSL_CTX_set_cipher_list(PSK)");
			goto fail;
		}
	}
	/* TODO: when ctx->psk and a hybrid PSK kex (ECDHE_PSK, DHE_PSK,
	   RSA_PSK) is negotiated the server still presents a certificate
	   that we currently accept blindly.  PSK alone authenticates the
	   peer for MITM purposes (an attacker without the PSK can't derive
	   session keys regardless of cert) so the connection is still
	   private, but pinning a server cert fingerprint per BBS entry
	   would catch unintended-server-swap.  Wire this once bbslist has
	   a fingerprint slot, similar to ssh_fingerprint.

	   Validation must surface a prompt similar to SSH host-key changes:
	   on first connect, store the fingerprint; on subsequent connects,
	   if the cert fails to validate (mismatched fingerprint or bad
	   signature) the user gets a yes/no/cancel modal — accept once,
	   accept and update stored fingerprint, or abort.  Don't ever
	   silently fail validation; the user owns the trust decision. */
	SSL_CTX_set_verify(ctx->sslctx, SSL_VERIFY_NONE, NULL);

	ctx->ssl = SSL_new(ctx->sslctx);
	if (ctx->ssl == NULL) {
		format_ssl_err(last_err_buf, sizeof(last_err_buf), "SSL_new");
		goto fail;
	}
	SSL_set_app_data(ctx->ssl, ctx);
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

xp_tls_t
xp_tls_client_open(SOCKET sock, const char *sni, int read_timeout)
{
	return client_open_inner(sock, sni, read_timeout, NULL, NULL, 0);
}

xp_tls_t
xp_tls_client_open_psk(SOCKET sock, const char *sni, int read_timeout,
                       const char *identity, const void *psk, size_t psk_len)
{
	if (identity == NULL || psk == NULL || psk_len == 0) {
		set_last_err("xp_tls_client_open_psk: missing identity or PSK");
		return NULL;
	}
	return client_open_inner(sock, sni, read_timeout, identity, psk, psk_len);
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

	/* Gate the underlying send() on socket_writable so a peer-closed
	   or locally-shut-down socket reports XP_TLS_ERR_CLOSED instead of
	   triggering SIGPIPE inside SSL_write. */
	if (ctx->sock != INVALID_SOCKET && !socket_writable(ctx->sock, 100))
		return XP_TLS_ERR_CLOSED;

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

bool
xp_tls_has_pending(xp_tls_t ctx)
{
	if (ctx == NULL || ctx->ssl == NULL)
		return false;
	/* SSL_pending() reports plaintext bytes already decrypted from a
	   previous record that the next SSL_read can return without doing
	   a socket read. */
	return SSL_pending(ctx->ssl) > 0;
}

bool
xp_tls_used_psk(xp_tls_t ctx)
{
	if (ctx == NULL || ctx->ssl == NULL)
		return false;
	const SSL_CIPHER *cipher = SSL_get_current_cipher(ctx->ssl);
	if (cipher == NULL)
		return false;
	int kx = SSL_CIPHER_get_kx_nid(cipher);
	return kx == NID_kx_psk
	    || kx == NID_kx_dhe_psk
	    || kx == NID_kx_ecdhe_psk
	    || kx == NID_kx_rsa_psk;
}

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
		/* Best-effort graceful shutdown — only attempted while the
		   socket is in a writable state, so SSL_shutdown's write of
		   close_notify doesn't trigger SIGPIPE on a peer-closed or
		   locally-shut-down socket. */
		if (ctx->sock != INVALID_SOCKET && socket_writable(ctx->sock, 0))
			(void)SSL_shutdown(ctx->ssl);
		SSL_free(ctx->ssl);
	}
	if (ctx->sslctx != NULL)
		SSL_CTX_free(ctx->sslctx);
	if (close_socket && ctx->sock != INVALID_SOCKET)
		closesocket(ctx->sock);
	if (ctx->psk != NULL) {
		/* OPENSSL_cleanse() is barred from being elided as dead store;
		   plain memset() to a buffer about to be freed is not.  These
		   bytes are the BBS user's password. */
		OPENSSL_cleanse(ctx->psk, ctx->psk_len);
		free(ctx->psk);
	}
	if (ctx->psk_identity != NULL) {
		OPENSSL_cleanse(ctx->psk_identity, strlen(ctx->psk_identity));
		free(ctx->psk_identity);
	}
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
