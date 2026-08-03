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
 * OpenSSL uses a custom non-owning socket BIO. Its callbacks perform only
 * immediately-ready socket operations, allowing each high-level TLS call to
 * drive WANT_READ/WANT_WRITE with one monotonic deadline without changing
 * timeout or blocking state on the caller-owned socket.
 *
 * Legacy callers retain the Cryptlib-era permissive posture. Configured
 * callers can require hostname-checked certificate verification using either
 * the default Web-PKI roots or an explicit trust anchor, and can present
 * a client certificate.
 */

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* xp_tls.h pulls in sockwrap.h, which lands the OS socket + timeval
 * headers in the right order (winsock2 + ws2tcpip + wspiapi on Win32,
 * sys/socket + sys/time + friends on Unix).  Don't reach past it. */
#include "xp_tls.h"
#include "genwrap.h"
#include "xp_key_internal.h"
#include "xp_tls_internal.h"
#include "threadwrap.h"

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
	size_t         psk_identity_len;
	unsigned char *psk;
	size_t         psk_len;
	xp_tls_peer_chain_cb peer_chain_cb;
	void          *peer_chain_cb_arg;
	bool           psk_used;
	xp_tls_psk_lookup_cb server_psk_lookup;
	void          *server_psk_lookup_arg;
	xp_key_t       server_key;
	pthread_mutex_t read_lock;
	pthread_mutex_t write_lock;
	pthread_mutex_t provider_lock;
	bool locks_initialized;
	bool terminating;
};

static bool
initialize_context_locks(struct xp_tls_ctx *ctx)
{
	if (pthread_mutex_init(&ctx->read_lock, NULL) != 0)
		return false;
	if (pthread_mutex_init(&ctx->write_lock, NULL) != 0) {
		pthread_mutex_destroy(&ctx->read_lock);
		return false;
	}
	if (pthread_mutex_init(&ctx->provider_lock, NULL) != 0) {
		pthread_mutex_destroy(&ctx->write_lock);
		pthread_mutex_destroy(&ctx->read_lock);
		return false;
	}
	ctx->locks_initialized = true;
	return true;
}

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

/* --------------------------------------------------------- socket BIO */

struct operation_deadline {
	bool infinite;
	uint64_t end;
};

static struct operation_deadline
deadline_start(int timeout_ms)
{
	struct operation_deadline deadline = { timeout_ms < 0, 0 };
	if (!deadline.infinite)
		deadline.end = xp_timer64() + (uint64_t)timeout_ms;
	return deadline;
}

static int
deadline_remaining(const struct operation_deadline *deadline)
{
	if (deadline->infinite)
		return -1;
	uint64_t now = xp_timer64();
	if (now >= deadline->end)
		return 0;
	uint64_t remaining = deadline->end - now;
	return remaining > INT_MAX ? INT_MAX : (int)remaining;
}

static SOCKET
bio_socket(BIO *bio)
{
	return (SOCKET)(uintptr_t)BIO_get_data(bio);
}

static int
socket_bio_create(BIO *bio)
{
	BIO_set_init(bio, 1);
	BIO_set_shutdown(bio, 0);
	BIO_set_data(bio, (void *)(uintptr_t)INVALID_SOCKET);
	return 1;
}

static int
socket_bio_destroy(BIO *bio)
{
	if (bio == NULL)
		return 0;
	BIO_set_init(bio, 0);
	BIO_set_data(bio, NULL);
	return 1;
}

static int
socket_bio_read(BIO *bio, char *out, int length)
{
	SOCKET socket = bio_socket(bio);
	BIO_clear_retry_flags(bio);
	if (out == NULL || length <= 0)
		return 0;
	if (!socket_readable(socket, 0)) {
		BIO_set_retry_read(bio);
		return -1;
	}
	int result = recv(socket, out, (size_t)length, 0);
	if (result < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
		BIO_set_retry_read(bio);
		return -1;
	}
	return result;
}

static int
socket_bio_write(BIO *bio, const char *in, int length)
{
	SOCKET socket = bio_socket(bio);
	BIO_clear_retry_flags(bio);
	if (in == NULL || length <= 0)
		return 0;
	if (!socket_writable(socket, 0)) {
		BIO_set_retry_write(bio);
		return -1;
	}
	int flags = 0;
#ifdef MSG_NOSIGNAL
	flags = MSG_NOSIGNAL;
#endif
	int result = send(socket, in, (size_t)length, flags);
	if (result < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
		BIO_set_retry_write(bio);
		return -1;
	}
	return result;
}

static long
socket_bio_ctrl(BIO *bio, int command, long number, void *pointer)
{
	(void)number;
	(void)pointer;
	switch (command) {
		case BIO_CTRL_FLUSH: return 1;
		case BIO_CTRL_EOF: return 0;
		case BIO_C_GET_FD: return (long)bio_socket(bio);
		default: return 0;
	}
}

static BIO_METHOD *socket_bio_method;

static int
attach_socket_bio(SSL *ssl, SOCKET socket)
{
	BIO *bio = BIO_new(socket_bio_method);
	if (bio == NULL)
		return 0;
	BIO_set_data(bio, (void *)(uintptr_t)socket);
	SSL_set_bio(ssl, bio, bio);
	return 1;
}

/* --------------------------------------------------------- one-shot init */

/*
 * OpenSSL 1.1+ auto-inits; explicit init is a no-op on modern libs
 * but safe. Kept for clarity.
 */
static void
initialize_ssl(void)
{
	(void)OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS |
	                      OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
	socket_bio_method = BIO_meth_new(BIO_TYPE_SOURCE_SINK, "xptls socket");
	if (socket_bio_method != NULL) {
		BIO_meth_set_create(socket_bio_method, socket_bio_create);
		BIO_meth_set_destroy(socket_bio_method, socket_bio_destroy);
		BIO_meth_set_read(socket_bio_method, socket_bio_read);
		BIO_meth_set_write(socket_bio_method, socket_bio_write);
		BIO_meth_set_ctrl(socket_bio_method, socket_bio_ctrl);
	}
}

static void
ensure_ssl_init(void)
{
	static CRYPTO_ONCE once = CRYPTO_ONCE_STATIC_INIT;
	(void)CRYPTO_THREAD_run_once(&once, initialize_ssl);
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

static int
native_version(enum xp_tls_version version)
{
	switch (version) {
		case XP_TLS_VERSION_1_0: return TLS1_VERSION;
		case XP_TLS_VERSION_1_1: return TLS1_1_VERSION;
		case XP_TLS_VERSION_1_2: return TLS1_2_VERSION;
		case XP_TLS_VERSION_1_3: return TLS1_3_VERSION;
		default: return 0;
	}
}

static int
apply_versions(SSL_CTX *context, enum xp_tls_version minimum,
	enum xp_tls_version maximum)
{
	int min = native_version(minimum), max = native_version(maximum);
	if (minimum != XP_TLS_VERSION_UNKNOWN && min == 0)
		return 0;
	if (maximum != XP_TLS_VERSION_UNKNOWN && max == 0)
		return 0;
	if (min != 0 && max != 0 && min > max)
		return 0;
	return (min == 0 || SSL_CTX_set_min_proto_version(context, min) == 1)
		&& (max == 0 || SSL_CTX_set_max_proto_version(context, max) == 1);
}

static bool
wait_for_ssl(struct xp_tls_ctx *ctx, int ssl_error,
	const struct operation_deadline *deadline)
{
	int remaining = deadline_remaining(deadline);
	if (ssl_error == SSL_ERROR_WANT_READ)
		return socket_readable(ctx->sock, remaining);
	if (ssl_error == SSL_ERROR_WANT_WRITE)
		return socket_writable(ctx->sock, remaining);
	return false;
}

/* Returns 1 on success, 0 on timeout, and -1 on protocol/socket failure. */
static int
drive_handshake(struct xp_tls_ctx *ctx, bool server, int timeout_ms)
{
	struct operation_deadline deadline = deadline_start(timeout_ms);
	if (server)
		SSL_set_accept_state(ctx->ssl);
	else
		SSL_set_connect_state(ctx->ssl);
	for (;;) {
		int result = SSL_do_handshake(ctx->ssl);
		if (result == 1)
			return 1;
		int ssl_error = SSL_get_error(ctx->ssl, result);
		if (ssl_error != SSL_ERROR_WANT_READ &&
		    ssl_error != SSL_ERROR_WANT_WRITE)
			return -1;
		if (!wait_for_ssl(ctx, ssl_error, &deadline))
			return 0;
	}
}

static int
accept_unverified_client_certificate(int ok, X509_STORE_CTX *store)
{
	(void)ok;
	(void)store;
	return 1;
}

static unsigned int
server_psk_callback(SSL *ssl, const char *identity,
	unsigned char *key, unsigned int capacity)
{
	xp_tls_t context = SSL_get_app_data(ssl);
	if (context == NULL || context->server_psk_lookup == NULL || identity == NULL)
		return 0;
	size_t key_len = capacity;
	if (context->server_psk_lookup(context->server_psk_lookup_arg,
	    identity, strlen(identity), key, &key_len) != 0 || key_len > capacity)
		return 0;
	free(context->psk_identity);
	context->psk_identity_len = strlen(identity);
	context->psk_identity = malloc(context->psk_identity_len + 1);
	if (context->psk_identity == NULL) {
		OPENSSL_cleanse(key, key_len);
		return 0;
	}
	memcpy(context->psk_identity, identity, context->psk_identity_len + 1);
	context->psk_used = true;
	return (unsigned int)key_len;
}

static int
server_tls13_psk_callback(SSL *ssl, const unsigned char *identity,
	size_t identity_len, SSL_SESSION **session_out)
{
	xp_tls_t context = SSL_get_app_data(ssl);
	if (context == NULL || context->server_psk_lookup == NULL ||
	    identity == NULL || session_out == NULL)
		return 0;
	unsigned char *key = OPENSSL_malloc(1024);
	if (key == NULL)
		return 0;
	size_t key_len = 1024;
	int found = context->server_psk_lookup(context->server_psk_lookup_arg,
	    identity, identity_len, key, &key_len) == 0 && key_len <= 1024;
	SSL_SESSION *session = NULL;
	if (found) {
		static const unsigned char cipher_id[] = {0x13, 0x01};
		const SSL_CIPHER *cipher = SSL_CIPHER_find(ssl, cipher_id);
		session = SSL_SESSION_new();
		found = cipher != NULL && session != NULL &&
		    SSL_SESSION_set_protocol_version(session, TLS1_3_VERSION) == 1 &&
		    SSL_SESSION_set_cipher(session, cipher) == 1 &&
		    SSL_SESSION_set1_master_key(session, key, key_len) == 1;
	}
	OPENSSL_cleanse(key, key_len <= 1024 ? key_len : 1024);
	OPENSSL_free(key);
	if (!found) {
		SSL_SESSION_free(session);
		return 0;
	}
	char *saved = malloc(identity_len + 1);
	if (saved == NULL) {
		SSL_SESSION_free(session);
		return 0;
	}
	memcpy(saved, identity, identity_len);
	saved[identity_len] = 0;
	free(context->psk_identity);
	context->psk_identity = saved;
	context->psk_identity_len = identity_len;
	context->psk_used = true;
	*session_out = session;
	return 1;
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

struct private_key_password {
	xp_crypto_secret_callback_t callback;
	void *context;
};

static int
private_key_password_cb(char *buffer, int capacity, int writing, void *arg)
{
	(void)writing;
	struct private_key_password *password = arg;
	if (password == NULL || password->callback == NULL || capacity <= 0)
		return 0;
	size_t length = 0;
	if (password->callback(password->context, NULL, 0, &length) != 0 ||
	    length > (size_t)capacity || length > INT_MAX)
		return 0;
	unsigned char *temporary = OPENSSL_malloc(length == 0 ? 1 : length);
	if (temporary == NULL)
		return 0;
	size_t actual = length;
	int result = 0;
	if (password->callback(password->context, temporary, length, &actual) == 0 &&
	    actual == length) {
		memcpy(buffer, temporary, length);
		result = (int)length;
	}
	OPENSSL_cleanse(temporary, length);
	OPENSSL_free(temporary);
	return result;
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
	struct private_key_password key_password = {
		config->private_key_password, config->private_key_password_arg
	};

	last_err_buf[0] = 0;
	ensure_ssl_init();

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		set_last_err("xp_tls_client_open: out of memory");
		return NULL;
	}
	if (!initialize_context_locks(ctx)) {
		free(ctx);
		set_last_err("xp_tls_client_open: lock initialization failed");
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
		ctx->psk_identity_len = strlen(psk_identity);
	}

	ctx->sslctx = SSL_CTX_new(TLS_client_method());
	if (ctx->sslctx == NULL) {
		format_ssl_err(last_err_buf, sizeof(last_err_buf), "SSL_CTX_new");
		goto fail;
	}
	/* TLS 1.2 floor; stay at the OpenSSL default ceiling for certificate
	   sessions. PSK sessions select one exact protocol version so the same
	   external key is never reused by an automatic cross-version fallback. */
	if (config->min_version == XP_TLS_VERSION_UNKNOWN
	    && SSL_CTX_set_min_proto_version(ctx->sslctx, TLS1_2_VERSION) != 1) {
		format_ssl_err(last_err_buf, sizeof(last_err_buf), "setting TLS minimum");
		goto fail;
	}
	if ((config->min_version != XP_TLS_VERSION_UNKNOWN
	     || config->max_version != XP_TLS_VERSION_UNKNOWN)
	    && !apply_versions(ctx->sslctx, config->min_version, config->max_version)) {
		set_last_err("invalid or unavailable TLS version range");
		goto fail;
	}
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
		if (key_password.callback != NULL) {
			SSL_CTX_set_default_passwd_cb(ctx->sslctx, private_key_password_cb);
			SSL_CTX_set_default_passwd_cb_userdata(ctx->sslctx, &key_password);
		}
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
		SSL_CTX_set_default_passwd_cb_userdata(ctx->sslctx, NULL);
	}

	ctx->ssl = SSL_new(ctx->sslctx);
	if (ctx->ssl == NULL) {
		format_ssl_err(last_err_buf, sizeof(last_err_buf), "SSL_new");
		goto fail;
	}
	SSL_set_mode(ctx->ssl,
	    SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
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

	if (socket_bio_method == NULL || !attach_socket_bio(ctx->ssl, sock)) {
		format_ssl_err(last_err_buf, sizeof(last_err_buf), "TLS socket BIO");
		goto fail;
	}

	rc = drive_handshake(ctx, false, 30000);
	if (rc != 1) {
		if (ctx->psk == NULL &&
		    SSL_get_verify_result(ctx->ssl) != X509_V_OK)
			report_peer_chain(ctx);
		if (rc == 0)
			set_last_err("TLS client handshake timed out");
		else
			format_ssl_err(last_err_buf, sizeof(last_err_buf), "TLS client handshake");
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
	if (!have_psk &&
	    (unsigned)config->server_auth > XP_TLS_SERVER_AUTH_UNTRUSTED) {
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

static int
use_server_certificate_chain(SSL_CTX *context,
	const void *pem, size_t pem_length)
{
	BIO *source = BIO_new_mem_buf(pem, (int)pem_length);
	if (source == NULL)
		return 0;
	X509 *leaf = PEM_read_bio_X509_AUX(source, NULL, NULL, NULL);
	int ok = leaf != NULL && SSL_CTX_use_certificate(context, leaf) == 1;
	X509_free(leaf);
	while (ok) {
		X509 *certificate = PEM_read_bio_X509(source, NULL, NULL, NULL);
		if (certificate == NULL) {
			ERR_clear_error();
			break;
		}
		if (SSL_CTX_add_extra_chain_cert(context, certificate) != 1) {
			X509_free(certificate);
			ok = 0;
		}
	}
	BIO_free(source);
	return ok;
}

xp_tls_t
xp_tls_provider_server_open(SOCKET socket, const void *chain_pem,
	size_t chain_pem_length, xp_key_t private_key,
	const struct xp_tls_server_config *config)
{
	int rc;
	if (config == NULL
	    || ((chain_pem == NULL) != (private_key == NULL))
	    || (private_key == NULL && config->psk_lookup == NULL)
	    || (unsigned)config->client_auth > XP_TLS_CLIENT_AUTH_REQUIRE_VALID
	    || (config->client_auth == XP_TLS_CLIENT_AUTH_REQUIRE_VALID
	        && (config->client_ca_file == NULL || config->client_ca_file[0] == 0))) {
		set_last_err("xp_tls_server_open: invalid configuration");
		return NULL;
	}
	ensure_ssl_init();
	xp_tls_t context = calloc(1, sizeof(*context));
	if (context == NULL) {
		set_last_err("xp_tls_server_open: out of memory");
		return NULL;
	}
	if (!initialize_context_locks(context)) {
		free(context);
		set_last_err("xp_tls_server_open: lock initialization failed");
		return NULL;
	}
	context->sock = socket;
	context->server_psk_lookup = config->psk_lookup;
	context->server_psk_lookup_arg = config->psk_lookup_arg;
	context->server_key = private_key;
	if (private_key != NULL)
		xp_key_retain(private_key);
	context->sslctx = SSL_CTX_new(TLS_server_method());
	if (context->sslctx == NULL
	    || !apply_versions(context->sslctx,
	        config->min_version == XP_TLS_VERSION_UNKNOWN
	        ? XP_TLS_VERSION_1_2 : config->min_version,
	        config->max_version)) {
		format_ssl_err(last_err_buf, sizeof(last_err_buf), "TLS server context");
		goto fail;
	}
	if (private_key != NULL) {
		EVP_PKEY *key = xp_key_native_private(private_key);
		if (key == NULL
		    || !use_server_certificate_chain(context->sslctx,
		        chain_pem, chain_pem_length)
		    || SSL_CTX_use_PrivateKey(context->sslctx, key) != 1
		    || SSL_CTX_check_private_key(context->sslctx) != 1) {
			format_ssl_err(last_err_buf, sizeof(last_err_buf), "TLS server credentials");
			goto fail;
		}
	}
	if (config->client_auth == XP_TLS_CLIENT_AUTH_REQUEST_UNVERIFIED)
		SSL_CTX_set_verify(context->sslctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE,
		                   accept_unverified_client_certificate);
	else if (config->client_auth == XP_TLS_CLIENT_AUTH_REQUIRE_VALID) {
		if (SSL_CTX_load_verify_locations(context->sslctx,
		    config->client_ca_file, NULL) != 1) {
			format_ssl_err(last_err_buf, sizeof(last_err_buf), "TLS client CA");
			goto fail;
		}
		SSL_CTX_set_verify(context->sslctx,
			SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	}
	if (config->psk_lookup != NULL) {
		SSL_CTX_set_psk_server_callback(context->sslctx, server_psk_callback);
		SSL_CTX_set_psk_find_session_callback(context->sslctx,
		    server_tls13_psk_callback);
		if (SSL_CTX_set_ciphersuites(context->sslctx,
		    "TLS_AES_128_GCM_SHA256:TLS_CHACHA20_POLY1305_SHA256") != 1) {
			format_ssl_err(last_err_buf, sizeof(last_err_buf), "TLS 1.3 PSK suites");
			goto fail;
		}
		if (SSL_CTX_set_cipher_list(context->sslctx,
		    private_key == NULL ? "PSK:DHE-PSK:ECDHE-PSK"
		                        : "DEFAULT:PSK:DHE-PSK:ECDHE-PSK") != 1) {
			format_ssl_err(last_err_buf, sizeof(last_err_buf), "TLS PSK suites");
			goto fail;
		}
	}
	context->ssl = SSL_new(context->sslctx);
	if (context->ssl == NULL || socket_bio_method == NULL
	    || !attach_socket_bio(context->ssl, socket)) {
		format_ssl_err(last_err_buf, sizeof(last_err_buf), "TLS server session");
		goto fail;
	}
	SSL_set_mode(context->ssl,
	    SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	SSL_set_app_data(context->ssl, context);
	rc = drive_handshake(context, true, config->handshake_timeout_ms);
	if (rc == 0) {
		set_last_err("TLS server handshake timed out");
		goto fail;
	}
	if (rc < 0) {
		format_ssl_err(last_err_buf, sizeof(last_err_buf), "TLS server handshake");
		goto fail;
	}
	return context;
fail:
	xp_tls_close(context, false);
	return NULL;
}

enum xp_tls_version
xp_tls_protocol_version(xp_tls_t ctx)
{
	if (ctx == NULL || ctx->ssl == NULL)
		return XP_TLS_VERSION_UNKNOWN;
	pthread_mutex_lock(&ctx->provider_lock);
	int version = SSL_version(ctx->ssl);
	pthread_mutex_unlock(&ctx->provider_lock);
	switch (version) {
		case TLS1_2_VERSION:
			return XP_TLS_VERSION_1_2;
		case TLS1_3_VERSION:
			return XP_TLS_VERSION_1_3;
	}
	return XP_TLS_VERSION_UNKNOWN;
}

/* ------------------------------------------------------------- push */

static int
tls_io_error_locked(struct xp_tls_ctx *ctx, int result, const char *operation,
	int *want)
{
	int ssl_error = SSL_get_error(ctx->ssl, result);
	if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
		*want = ssl_error;
		return 2;
	}
	if (ssl_error == SSL_ERROR_ZERO_RETURN)
		return XP_TLS_ERR_CLOSED;
	if (ssl_error == SSL_ERROR_SYSCALL) {
		if (errno == 0 || errno == EPIPE || errno == ECONNRESET)
			return XP_TLS_ERR_CLOSED;
		snprintf(ctx->err, sizeof(ctx->err), "%s syscall error (errno=%d)",
		    operation, errno);
		return XP_TLS_ERR;
	}
	format_ssl_err(ctx->err, sizeof(ctx->err), operation);
	return XP_TLS_ERR;
}

int
xp_tls_push(xp_tls_t ctx, const void *buf, size_t n, size_t *copied)
{
	return xp_tls_push_timeout(ctx, buf, n, copied, -1);
}

/* ------------------------------------------------------------- pop */

int
xp_tls_pop(xp_tls_t ctx, void *buf, size_t n, size_t *copied)
{
	return xp_tls_pop_timeout(ctx, buf, n, copied,
	    ctx == NULL || ctx->read_timeout_sec <= 0
	    ? -1 : ctx->read_timeout_sec * 1000);
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
	pthread_mutex_lock(&ctx->provider_lock);
	bool pending = SSL_pending(ctx->ssl) > 0;
	pthread_mutex_unlock(&ctx->provider_lock);
	return pending;
}

bool
xp_tls_used_psk(xp_tls_t ctx)
{
	/* TLS 1.3 cipher-suite names and key-exchange NIDs do not identify
	   external-PSK authentication. A PSK-configured context is returned only
	   after client_open_inner() has verified that the handshake selected it. */
	return ctx != NULL && ctx->ssl != NULL && (ctx->psk != NULL || ctx->psk_used);
}

int
xp_tls_flush(xp_tls_t ctx)
{
	return ctx == NULL ? XP_TLS_OK : xp_tls_flush_timeout(ctx, -1);
}

int
xp_tls_pop_timeout(xp_tls_t ctx, void *buf, size_t n, size_t *copied,
	int timeout_ms)
{
	if (copied != NULL)
		*copied = 0;
	if (ctx == NULL || (buf == NULL && n != 0))
		return XP_TLS_ERR;
	if (n == 0)
		return XP_TLS_OK;
	pthread_mutex_lock(&ctx->read_lock);
	struct operation_deadline deadline = deadline_start(timeout_ms);
	int status = XP_TLS_ERR;
	for (;;) {
		pthread_mutex_lock(&ctx->provider_lock);
		if (ctx->terminating) {
			pthread_mutex_unlock(&ctx->provider_lock);
			status = XP_TLS_ERR_CLOSED;
			break;
		}
		size_t read = 0;
		int result = SSL_read_ex(ctx->ssl, buf, n, &read);
		if (result == 1) {
			if (copied != NULL) *copied = read;
			status = read > 0 ? XP_TLS_OK : XP_TLS_ERR;
			pthread_mutex_unlock(&ctx->provider_lock);
			break;
		}
		int want = 0;
		status = tls_io_error_locked(ctx, result, "SSL_read", &want);
		pthread_mutex_unlock(&ctx->provider_lock);
		if (status != 2)
			break;
		if (!wait_for_ssl(ctx, want, &deadline)) {
			status = XP_TLS_TIMEOUT;
			break;
		}
	}
	pthread_mutex_unlock(&ctx->read_lock);
	return status;
}

int
xp_tls_push_timeout(xp_tls_t ctx, const void *buf, size_t n, size_t *copied,
	int timeout_ms)
{
	if (copied != NULL)
		*copied = 0;
	if (ctx == NULL || (buf == NULL && n != 0))
		return XP_TLS_ERR;
	if (n == 0)
		return XP_TLS_OK;
	pthread_mutex_lock(&ctx->write_lock);
	struct operation_deadline deadline = deadline_start(timeout_ms);
	int status = XP_TLS_ERR;
	for (;;) {
		pthread_mutex_lock(&ctx->provider_lock);
		if (ctx->terminating) {
			pthread_mutex_unlock(&ctx->provider_lock);
			status = XP_TLS_ERR_CLOSED;
			break;
		}
		size_t written = 0;
		int result = SSL_write_ex(ctx->ssl, buf, n, &written);
		if (written != 0) {
			if (copied != NULL) *copied = written;
			status = XP_TLS_OK;
			pthread_mutex_unlock(&ctx->provider_lock);
			break;
		}
		if (result == 1) {
			snprintf(ctx->err, sizeof(ctx->err),
			    "SSL_write produced no progress");
			pthread_mutex_unlock(&ctx->provider_lock);
			status = XP_TLS_ERR;
			break;
		}
		int want = 0;
		status = tls_io_error_locked(ctx, result, "SSL_write", &want);
		pthread_mutex_unlock(&ctx->provider_lock);
		if (status != 2)
			break;
		if (!wait_for_ssl(ctx, want, &deadline)) {
			status = XP_TLS_TIMEOUT;
			break;
		}
	}
	pthread_mutex_unlock(&ctx->write_lock);
	return status;
}

int
xp_tls_flush_timeout(xp_tls_t ctx, int timeout_ms)
{
	if (ctx == NULL)
		return XP_TLS_ERR;
	(void)timeout_ms;
	pthread_mutex_lock(&ctx->write_lock);
	pthread_mutex_lock(&ctx->provider_lock);
	int status = ctx->terminating ? XP_TLS_ERR_CLOSED : XP_TLS_OK;
	pthread_mutex_unlock(&ctx->provider_lock);
	pthread_mutex_unlock(&ctx->write_lock);
	return status;
}

const char *
xp_tls_cipher_name(xp_tls_t ctx)
{
	return ctx == NULL || ctx->ssl == NULL ? NULL : SSL_get_cipher_name(ctx->ssl);
}

enum xp_tls_auth_method
xp_tls_authentication_method(xp_tls_t ctx)
{
	if (ctx == NULL || ctx->ssl == NULL)
		return XP_TLS_AUTH_NONE;
	return xp_tls_used_psk(ctx) ? XP_TLS_AUTH_PSK : XP_TLS_AUTH_CERTIFICATE;
}

int
xp_tls_psk_identity(xp_tls_t ctx, void *out, size_t *len)
{
	if (ctx == NULL || len == NULL)
		return XP_CRYPTO_ERR_INVALID;
	const char *identity = SSL_get_psk_identity(ctx->ssl);
	size_t required;
	if (ctx->psk_identity != NULL) {
		identity = ctx->psk_identity;
		required = ctx->psk_identity_len;
	}
	else
		required = identity == NULL ? 0 : strlen(identity);
	if (identity == NULL) {
		*len = 0;
		return XP_CRYPTO_ERR_NOT_FOUND;
	}
	if (out == NULL) {
		*len = required;
		return XP_CRYPTO_OK;
	}
	if (*len < required) {
		*len = required;
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	memcpy(out, identity, required);
	*len = required;
	return XP_CRYPTO_OK;
}

size_t
xp_tls_peer_certificate_count(xp_tls_t ctx)
{
	if (ctx == NULL || ctx->ssl == NULL)
		return 0;
	STACK_OF(X509) *chain = SSL_get_peer_cert_chain(ctx->ssl);
	return chain == NULL ? 0 : (size_t)sk_X509_num(chain);
}

int
xp_tls_peer_certificate_der(xp_tls_t ctx, size_t index, void *out, size_t *len)
{
	if (ctx == NULL || ctx->ssl == NULL || len == NULL)
		return XP_CRYPTO_ERR_INVALID;
	STACK_OF(X509) *chain = SSL_get_peer_cert_chain(ctx->ssl);
	if (chain == NULL || index >= (size_t)sk_X509_num(chain))
		return XP_CRYPTO_ERR_NOT_FOUND;
	X509 *certificate = sk_X509_value(chain, (int)index);
	int required = i2d_X509(certificate, NULL);
	if (required <= 0)
		return XP_CRYPTO_ERR;
	if (out == NULL) {
		*len = (size_t)required;
		return XP_CRYPTO_OK;
	}
	if (*len < (size_t)required) {
		*len = (size_t)required;
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	unsigned char *cursor = out;
	if (i2d_X509(certificate, &cursor) != required)
		return XP_CRYPTO_ERR;
	*len = (size_t)required;
	return XP_CRYPTO_OK;
}

/* ------------------------------------------------------------- close */

int
xp_tls_terminate(xp_tls_t ctx)
{
	if (ctx == NULL)
		return XP_TLS_ERR;
	pthread_mutex_lock(&ctx->provider_lock);
	if (!ctx->terminating) {
		ctx->terminating = true;
		if (ctx->ssl != NULL && ctx->sock != INVALID_SOCKET
		    && socket_writable(ctx->sock, 0))
			(void)SSL_shutdown(ctx->ssl);
	}
	pthread_mutex_unlock(&ctx->provider_lock);
	return XP_TLS_OK;
}

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
		if (!ctx->terminating && ctx->sock != INVALID_SOCKET
		    && socket_writable(ctx->sock, 0))
			(void)SSL_shutdown(ctx->ssl);
		SSL_free(ctx->ssl);
	}
	if (ctx->sslctx != NULL)
		SSL_CTX_free(ctx->sslctx);
	if (ctx->server_key != NULL)
		xp_key_release(ctx->server_key);
	if (close_socket && ctx->sock != INVALID_SOCKET)
		closesocket(ctx->sock);
	if (ctx->psk != NULL) {
		/* OPENSSL_cleanse() is barred from being elided as dead store;
		   plain memset() to a buffer about to be freed is not. */
		OPENSSL_cleanse(ctx->psk, ctx->psk_len);
		free(ctx->psk);
	}
	if (ctx->psk_identity != NULL) {
		OPENSSL_cleanse(ctx->psk_identity, ctx->psk_identity_len);
		free(ctx->psk_identity);
	}
	if (ctx->locks_initialized) {
		pthread_mutex_destroy(&ctx->provider_lock);
		pthread_mutex_destroy(&ctx->write_lock);
		pthread_mutex_destroy(&ctx->read_lock);
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
