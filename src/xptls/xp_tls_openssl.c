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
 * Legacy callers retain the Cryptlib-era permissive posture. Configured
 * callers can require hostname-checked certificate verification using either
 * the default Web-PKI roots or an explicit trust anchor, and can present
 * a client certificate.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* xp_tls.h pulls in sockwrap.h, which lands the OS socket + timeval
 * headers in the right order (winsock2 + ws2tcpip + wspiapi on Win32,
 * sys/socket + sys/time + friends on Unix).  Don't reach past it. */
#include "xp_tls.h"

#include <openssl/err.h>
#include <openssl/pem.h>
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
	xp_tls_peer_chain_cb peer_chain_cb;
	void          *peer_chain_cb_arg;
};

struct certificate_strings {
	char *subject;
	char *issuer;
	char *not_before;
	char *not_after;
	char *fingerprint;
	char *pem;
};

static char *
bio_text(BIO *bio)
{
	BUF_MEM *mem = NULL;
	BIO_get_mem_ptr(bio, &mem);
	if (mem == NULL)
		return NULL;
	char *ret = malloc(mem->length + 1);
	if (ret != NULL) {
		memcpy(ret, mem->data, mem->length);
		ret[mem->length] = 0;
	}
	return ret;
}

static char *
name_text(X509_NAME *name)
{
	BIO *bio = BIO_new(BIO_s_mem());
	if (bio == NULL)
		return NULL;
	if (name != NULL)
		X509_NAME_print_ex(bio, name, 0, XN_FLAG_RFC2253);
	char *ret = bio_text(bio);
	BIO_free(bio);
	return ret;
}

static char *
time_text(const ASN1_TIME *time)
{
	struct tm tm;
	if (time != NULL && ASN1_TIME_to_tm(time, &tm) == 1) {
		char *ret = malloc(24);
		if (ret != NULL)
			snprintf(ret, 24, "%04d-%02d-%02d %02d:%02d:%02d UTC",
			    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			    tm.tm_hour, tm.tm_min, tm.tm_sec);
		return ret;
	}
	BIO *bio = BIO_new(BIO_s_mem());
	if (bio == NULL)
		return NULL;
	if (time != NULL)
		ASN1_TIME_print(bio, time);
	char *ret = bio_text(bio);
	BIO_free(bio);
	return ret;
}

static char *
fingerprint_text(X509 *cert)
{
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int length = 0;
	if (X509_digest(cert, EVP_sha256(), digest, &length) != 1)
		return NULL;
	char *ret = malloc(length * 3);
	if (ret == NULL)
		return NULL;
	for (unsigned int i = 0; i < length; i++)
		snprintf(ret + i * 3, 4, i + 1 == length ? "%02X" : "%02X:",
		    digest[i]);
	return ret;
}

static char *
pem_text(X509 *cert)
{
	BIO *bio = BIO_new(BIO_s_mem());
	if (bio == NULL)
		return NULL;
	char *ret = NULL;
	if (PEM_write_bio_X509(bio, cert) == 1)
		ret = bio_text(bio);
	BIO_free(bio);
	return ret;
}

static void
free_certificate_strings(struct certificate_strings *item)
{
	free(item->subject);
	free(item->issuer);
	free(item->not_before);
	free(item->not_after);
	free(item->fingerprint);
	free(item->pem);
}

static void
report_peer_chain(struct xp_tls_ctx *ctx)
{
	if (ctx->peer_chain_cb == NULL || ctx->ssl == NULL)
		return;
	STACK_OF(X509) *chain = SSL_get_peer_cert_chain(ctx->ssl);
	int count = chain == NULL ? 0 : sk_X509_num(chain);
	if (count <= 0)
		return;
	struct certificate_strings *strings = calloc((size_t)count,
	    sizeof(*strings));
	struct xp_tls_peer_certificate *certificates = calloc((size_t)count,
	    sizeof(*certificates));
	if (strings == NULL || certificates == NULL)
		goto cleanup;
	for (int i = 0; i < count; i++) {
		X509 *cert = sk_X509_value(chain, i);
		strings[i].subject = name_text(X509_get_subject_name(cert));
		strings[i].issuer = name_text(X509_get_issuer_name(cert));
		strings[i].not_before = time_text(X509_get0_notBefore(cert));
		strings[i].not_after = time_text(X509_get0_notAfter(cert));
		strings[i].fingerprint = fingerprint_text(cert);
		strings[i].pem = pem_text(cert);
		certificates[i].subject = strings[i].subject;
		certificates[i].issuer = strings[i].issuer;
		certificates[i].not_before = strings[i].not_before;
		certificates[i].not_after = strings[i].not_after;
		certificates[i].fingerprint_sha256 = strings[i].fingerprint;
		certificates[i].pem = strings[i].pem;
	}
	ctx->peer_chain_cb(ctx->peer_chain_cb_arg, certificates, (size_t)count);

cleanup:
	if (strings != NULL) {
		for (int i = 0; i < count; i++)
			free_certificate_strings(&strings[i]);
	}
	free(strings);
	free(certificates);
}

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

static int
set_expected_peer_name(SSL *ssl, const char *name)
{
	unsigned char address[16];
	X509_VERIFY_PARAM *param = SSL_get0_param(ssl);
	if (inet_pton(AF_INET, name, address) == 1 ||
	    inet_pton(AF_INET6, name, address) == 1)
		return X509_VERIFY_PARAM_set1_ip_asc(param, name);
	return SSL_set1_host(ssl, name);
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
 * Common open path for certificate and PSK clients. Certificate sessions
 * negotiate TLS 1.2 or newer. PSK sessions use one exact configured version;
 * OpenSSL's legacy client callback supplies both TLS 1.2 PSKs and TLS 1.3
 * external PSKs (with SHA-256 as the TLS 1.3 callback's default digest).
 */
static xp_tls_t
client_open_inner(SOCKET sock, const struct xp_tls_client_config *config,
                  bool legacy_psk)
{
	struct xp_tls_ctx *ctx;
	int rc;
	const char *sni = config->server_name;
	const char *psk_identity = config->psk_identity;
	const void *psk = config->psk;
	size_t psk_len = config->psk_len;

	last_err_buf[0] = 0;
	ensure_ssl_init();

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		set_last_err("xp_tls_client_open: out of memory");
		return NULL;
	}
	ctx->sock = sock;
	ctx->read_timeout_sec = config->read_timeout;
	ctx->peer_chain_cb = config->peer_chain_cb;
	ctx->peer_chain_cb_arg = config->peer_chain_cb_arg;

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
	/* TLS 1.2 floor; stay at the OpenSSL default ceiling for certificate
	   sessions. PSK sessions select one exact protocol version so the same
	   external key is never reused by an automatic cross-version fallback. */
	SSL_CTX_set_min_proto_version(ctx->sslctx, TLS1_2_VERSION);
	if (ctx->psk != NULL) {
		int version = config->psk_version == XP_TLS_VERSION_1_3
		    ? TLS1_3_VERSION : TLS1_2_VERSION;
		if (SSL_CTX_set_min_proto_version(ctx->sslctx, version) != 1 ||
		    SSL_CTX_set_max_proto_version(ctx->sslctx, version) != 1) {
			format_ssl_err(last_err_buf, sizeof(last_err_buf),
			              "setting TLS-PSK protocol version");
			goto fail;
		}
		SSL_CTX_set_psk_client_callback(ctx->sslctx, psk_client_cb);
		/* Installing the legacy PSK callback can enable TLS 1.3 psk_ke.
		   Require psk_dhe_ke so compromise of the shared secret does not
		   retrospectively expose captured sessions. */
#ifdef SSL_OP_ALLOW_NO_DHE_KEX
		SSL_CTX_clear_options(ctx->sslctx, SSL_OP_ALLOW_NO_DHE_KEX);
#endif
		/* TLS 1.2 encodes authentication in the cipher suite. Require an
		   ephemeral PSK exchange rather than permitting plain PSK. TLS 1.3
		   cipher suites are configured separately and are unaffected. */
		if (!legacy_psk && config->psk_version == XP_TLS_VERSION_1_2 &&
		    SSL_CTX_set_cipher_list(ctx->sslctx,
		    "kECDHEPSK:kDHEPSK:!CBC:!eNULL") != 1) {
			format_ssl_err(last_err_buf, sizeof(last_err_buf),
			              "setting ephemeral TLS 1.2 PSK cipher list");
			goto fail;
		}
	}
	if (ctx->psk == NULL &&
	    config->server_auth != XP_TLS_SERVER_AUTH_NONE) {
		if (config->server_auth == XP_TLS_SERVER_AUTH_WEB_PKI &&
		    SSL_CTX_set_default_verify_paths(ctx->sslctx) != 1) {
			format_ssl_err(last_err_buf, sizeof(last_err_buf),
			              "SSL_CTX_set_default_verify_paths");
			goto fail;
		}
		else if (config->server_auth == XP_TLS_SERVER_AUTH_CERTIFICATE &&
		    config->trusted_cert_file != NULL &&
		    config->trusted_cert_file[0] != 0) {
			if (SSL_CTX_load_verify_locations(ctx->sslctx,
			    config->trusted_cert_file, NULL) != 1) {
				format_ssl_err(last_err_buf, sizeof(last_err_buf),
				              "SSL_CTX_load_verify_locations");
				goto fail;
			}
			/* An explicitly trusted leaf is a valid trust anchor too; it
			   need not be a self-signed CA certificate. */
			if (X509_VERIFY_PARAM_set_flags(
			    SSL_CTX_get0_param(ctx->sslctx),
			    X509_V_FLAG_PARTIAL_CHAIN) != 1) {
				format_ssl_err(last_err_buf, sizeof(last_err_buf),
				              "X509_VERIFY_PARAM_set_flags");
				goto fail;
			}
		}
		SSL_CTX_set_verify(ctx->sslctx, SSL_VERIFY_PEER, NULL);
	}
	else
		SSL_CTX_set_verify(ctx->sslctx, SSL_VERIFY_NONE, NULL);

	if (ctx->psk == NULL && config->client_cert_file != NULL &&
	    config->client_cert_file[0] != 0) {
		if (SSL_CTX_use_certificate_chain_file(ctx->sslctx,
		    config->client_cert_file) != 1) {
			format_ssl_err(last_err_buf, sizeof(last_err_buf),
			              "SSL_CTX_use_certificate_chain_file");
			goto fail;
		}
		if (SSL_CTX_use_PrivateKey_file(ctx->sslctx,
		    config->client_key_file, SSL_FILETYPE_PEM) != 1) {
			format_ssl_err(last_err_buf, sizeof(last_err_buf),
			              "SSL_CTX_use_PrivateKey_file");
			goto fail;
		}
		if (SSL_CTX_check_private_key(ctx->sslctx) != 1) {
			format_ssl_err(last_err_buf, sizeof(last_err_buf),
			              "SSL_CTX_check_private_key");
			goto fail;
		}
	}

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
		if (config->server_auth != XP_TLS_SERVER_AUTH_NONE &&
		    set_expected_peer_name(ctx->ssl, sni) != 1) {
			format_ssl_err(last_err_buf, sizeof(last_err_buf),
			              "setting expected TLS peer name");
			goto fail;
		}
	}

	if (set_recv_timeout(sock, config->read_timeout) != 0) {
		set_last_err("xp_tls_client_open: setsockopt SO_RCVTIMEO failed (%d)", errno);
		goto fail;
	}
	if (SSL_set_fd(ctx->ssl, (int)sock) != 1) {
		format_ssl_err(last_err_buf, sizeof(last_err_buf), "SSL_set_fd");
		goto fail;
	}

	rc = SSL_connect(ctx->ssl);
	if (rc != 1) {
		if (ctx->psk == NULL &&
		    SSL_get_verify_result(ctx->ssl) != X509_V_OK)
			report_peer_chain(ctx);
		int e = SSL_get_error(ctx->ssl, rc);
		if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE)
			set_last_err("SSL_connect: handshake would block");
		else
			format_ssl_err(last_err_buf, sizeof(last_err_buf), "SSL_connect");
		goto fail;
	}
	if (ctx->psk != NULL) {
		bool psk_selected = SSL_version(ctx->ssl) >= TLS1_3_VERSION
		    ? SSL_session_reused(ctx->ssl) == 1
		    : SSL_get_psk_identity(ctx->ssl) != NULL;
		if (!psk_selected) {
			set_last_err("TLS handshake did not select the configured PSK");
			goto fail;
		}
	}

	return ctx;

fail:
	xp_tls_close(ctx, false);
	return NULL;
}

xp_tls_t
xp_tls_client_open(SOCKET sock, const char *sni, int read_timeout)
{
	const struct xp_tls_client_config config = {
		.server_name = sni,
		.read_timeout = read_timeout,
	};
	return client_open_inner(sock, &config, false);
}

xp_tls_t
xp_tls_client_open_config(SOCKET sock,
                          const struct xp_tls_client_config *config)
{
	if (config == NULL) {
		set_last_err("xp_tls_client_open_config: missing configuration");
		return NULL;
	}
	bool have_psk_identity = config->psk_identity != NULL &&
	    config->psk_identity[0] != 0;
	bool have_psk = config->psk != NULL && config->psk_len > 0;
	if (have_psk_identity != have_psk) {
		set_last_err("xp_tls_client_open_config: incomplete PSK configuration");
		return NULL;
	}
	if (have_psk && config->psk_version != XP_TLS_VERSION_1_2 &&
	    config->psk_version != XP_TLS_VERSION_1_3) {
		set_last_err("xp_tls_client_open_config: PSK protocol version must be TLS 1.2 or TLS 1.3");
		return NULL;
	}
	if (!have_psk && config->psk_version != XP_TLS_VERSION_UNKNOWN) {
		set_last_err("xp_tls_client_open_config: PSK protocol version set without a PSK");
		return NULL;
	}
	bool have_client_cert = config->client_cert_file != NULL &&
	    config->client_cert_file[0] != 0;
	bool have_client_key = config->client_key_file != NULL &&
	    config->client_key_file[0] != 0;
	if (!have_psk && have_client_cert != have_client_key) {
		set_last_err("xp_tls_client_open_config: client certificate and key must both be set");
		return NULL;
	}
	if (!have_psk && (config->server_auth <= XP_TLS_SERVER_AUTH_NONE ||
	    config->server_auth > XP_TLS_SERVER_AUTH_UNTRUSTED)) {
		set_last_err("xp_tls_client_open_config: invalid server authentication mode");
		return NULL;
	}
	if (!have_psk && config->server_auth == XP_TLS_SERVER_AUTH_CERTIFICATE &&
	    (config->trusted_cert_file == NULL ||
	     config->trusted_cert_file[0] == 0)) {
		set_last_err("xp_tls_client_open_config: certificate authentication has no trust anchor");
		return NULL;
	}
	if (!have_psk && config->server_auth != XP_TLS_SERVER_AUTH_CERTIFICATE &&
	    config->trusted_cert_file != NULL && config->trusted_cert_file[0] != 0) {
		set_last_err("xp_tls_client_open_config: trusted certificate set for the wrong authentication mode");
		return NULL;
	}
	if (!have_psk && config->server_auth != XP_TLS_SERVER_AUTH_NONE &&
	    (config->server_name == NULL || config->server_name[0] == 0)) {
		set_last_err("xp_tls_client_open_config: authenticated TLS requires a server name");
		return NULL;
	}
	return client_open_inner(sock, config, false);
}

xp_tls_t
xp_tls_client_open_psk(SOCKET sock, const char *sni, int read_timeout,
                       const char *identity, const void *psk, size_t psk_len)
{
	if (identity == NULL || psk == NULL || psk_len == 0) {
		set_last_err("xp_tls_client_open_psk: missing identity or PSK");
		return NULL;
	}
	const struct xp_tls_client_config config = {
		.server_name = sni,
		.read_timeout = read_timeout,
		.psk_identity = identity,
		.psk = psk,
		.psk_len = psk_len,
		.psk_version = XP_TLS_VERSION_1_2,
	};
	return client_open_inner(sock, &config, true);
}

enum xp_tls_version
xp_tls_protocol_version(xp_tls_t ctx)
{
	if (ctx == NULL || ctx->ssl == NULL)
		return XP_TLS_VERSION_UNKNOWN;
	switch (SSL_version(ctx->ssl)) {
		case TLS1_2_VERSION:
			return XP_TLS_VERSION_1_2;
		case TLS1_3_VERSION:
			return XP_TLS_VERSION_1_3;
	}
	return XP_TLS_VERSION_UNKNOWN;
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
	/* TLS 1.3 cipher-suite names and key-exchange NIDs do not identify
	   external-PSK authentication. A PSK-configured context is returned only
	   after client_open_inner() has verified that the handshake selected it. */
	return ctx != NULL && ctx->ssl != NULL && ctx->psk != NULL;
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
