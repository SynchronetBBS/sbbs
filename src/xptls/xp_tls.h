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
 * xp_tls — provider-neutral TLS sessions for xpdev.
 *
 * Backend-agnostic layer over TLS client and server sessions. Used by
 * SyncTERM's TelnetS (telnets.c) and HTTPS (webget.c); designed to also
 * serve Synchronet server code once that port follows. Backend is
 * OpenSSL libssl or Botan3, selected at build time.
 *
 * The API mirrors what the consumers actually need — client/server
 * handshakes, credentials, authentication, inspection, per-operation
 * deadlines, push/pop/flush, SNI, and explicit socket ownership.
 * Certificate verification is selected per client session so legacy callers
 * can remain permissive while callers with an authentication policy can opt
 * into either system trust or a connection-specific trust anchor.
 */

#ifndef _XP_TLS_H
#define _XP_TLS_H

#include <stdbool.h>
#include <stddef.h>

#include "sockwrap.h"	/* SOCKET */
#include "wrapdll.h"
#include "xp_key.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct xp_tls_ctx *xp_tls_t;
typedef struct xp_tls_server_credentials *xp_tls_server_credentials_t;

enum xp_tls_log_level {
	XP_TLS_LOG_ERROR,
	XP_TLS_LOG_WARNING,
	XP_TLS_LOG_DEBUG,
};

enum xp_tls_log_source {
	XP_TLS_LOG_SOURCE_LIBRARY,
	XP_TLS_LOG_SOURCE_BACKEND,
	XP_TLS_LOG_SOURCE_PEER_ALERT,
	XP_TLS_LOG_SOURCE_LOCAL_ALERT,
};

struct xp_tls_log_record {
	enum xp_tls_log_level  level;
	enum xp_tls_log_source source;
	const char            *backend;
	int                    error_code;
	unsigned long          native_code;
	bool                   fatal;
	const void            *message;
	size_t                 message_len;
};

/*
 * Optional per-session diagnostics. Records and pointed-to data are valid
 * only during the callback. Callbacks may overlap when separate threads use
 * one session and must not re-enter that session through xp_tls_*().
 */
typedef void (*xp_tls_log_cb)(const struct xp_tls_log_record *record,
	void *arg);

enum xp_tls_server_auth {
	XP_TLS_SERVER_AUTH_NONE,
	XP_TLS_SERVER_AUTH_WEB_PKI,
	XP_TLS_SERVER_AUTH_CERTIFICATE,
	XP_TLS_SERVER_AUTH_UNTRUSTED,
};

enum xp_tls_version {
	XP_TLS_VERSION_UNKNOWN,
	XP_TLS_VERSION_1_0,
	XP_TLS_VERSION_1_1,
	XP_TLS_VERSION_1_2,
	XP_TLS_VERSION_1_3,
};

enum xp_tls_client_auth {
	XP_TLS_CLIENT_AUTH_NONE,
	XP_TLS_CLIENT_AUTH_REQUEST_UNVERIFIED,
	XP_TLS_CLIENT_AUTH_REQUIRE_VALID,
};

enum xp_tls_auth_method {
	XP_TLS_AUTH_NONE,
	XP_TLS_AUTH_CERTIFICATE,
	XP_TLS_AUTH_PSK,
};

/* Controls TLS 1.2 PSK interoperability. MODERN permits only ephemeral
 * key exchange with AEAD ciphers. TLS12_COMPATIBILITY additionally permits
 * plain PSK and AES-CBC/SHA suites used by older brokers. TLS 1.3 always
 * retains forward-secret PSK key exchange regardless of this setting. */
enum xp_tls_psk_policy {
	XP_TLS_PSK_POLICY_MODERN,
	XP_TLS_PSK_POLICY_TLS12_COMPATIBILITY,
};

struct xp_tls_server_credentials_config {
	const char *certificate_chain_file;
	xp_key_t private_key;
};

/* Counted standard-PEM certificate chain plus an opaque private-key handle.
 * The chain is borrowed only for xp_tls_client_open_config(); the resulting
 * session retains private_key for its lifetime. */
struct xp_tls_client_identity {
	const void *certificate_chain_pem;
	size_t certificate_chain_pem_len;
	xp_key_t private_key;
};

typedef int (*xp_tls_psk_lookup_cb)(
	void *arg, const void *identity, size_t identity_len,
	void *key, size_t *key_len);

/* TLS 1.3 PSKs in this API are provisioned with HKDF-SHA-256. Providers
 * restrict TLS 1.3 PSK negotiation to compatible cipher suites. This is a
 * deliberate common contract: callers never inherit a provider's implicit
 * PSK hash choice. */

struct xp_tls_server_config {
	enum xp_tls_version min_version;
	enum xp_tls_version max_version;
	int handshake_timeout_ms;
	enum xp_tls_client_auth client_auth;
	const char *client_ca_file;
	xp_tls_psk_lookup_cb psk_lookup;
	void *psk_lookup_arg;
	enum xp_tls_psk_policy psk_policy;
	xp_tls_log_cb log_cb;
	void *log_cb_arg;
	enum xp_tls_log_level log_level;
};

/*
 * A certificate from the peer's presented chain. The strings are owned by
 * the TLS backend and remain valid only for the duration of the callback.
 * pem contains one complete PEM-encoded certificate; fingerprint_sha256 is
 * printable hexadecimal with separators chosen by the backend.
 */
struct xp_tls_peer_certificate {
	const char *subject;
	const char *issuer;
	const char *not_before;
	const char *not_after;
	const char *fingerprint_sha256;
	const char *pem;
};

typedef void (*xp_tls_peer_chain_cb)(void *arg,
    const struct xp_tls_peer_certificate *certificates, size_t count);

/*
 * Optional client-session policy. All pointed-to data remains owned by the
 * caller and only needs to remain valid until xp_tls_client_open_config()
 * returns.
 *
 * server_auth       — select Web PKI, a connection-specific certificate,
 *                     or validation with no trust anchors. UNTRUSTED
 *                     intentionally rejects certificate sessions while
 *                     still reporting the presented chain. NONE permits an
 *                     unauthenticated certificate session for opportunistic
 *                     protocols and compatibility callers.
 * trusted_cert_file — PEM trust anchor for CERTIFICATE mode. A leaf
 *                     certificate is accepted as the explicit trust anchor.
 * client_cert_file  — PEM certificate or chain presented to the server.
 * client_key_file   — matching unencrypted PEM private key.
 * client_identity   — storage-backed identity. Mutually exclusive with the
 *                     two filename fields; supports non-exportable keys.
 * psk_identity/psk  — when supplied, use a PSK-only handshake at the exact
 *                     version selected by psk_version; certificate trust and
 *                     client identity are ignored. TLS 1.3 uses the common
 *                     HKDF-SHA-256 provisioning contract described above.
 * peer_chain_cb     — optional diagnostic callback invoked with the
 *                     certificate chain presented by the peer when
 *                     certificate verification rejects the handshake.
 */
struct xp_tls_client_config {
	const char *server_name;
	enum xp_tls_server_auth server_auth;
	const char *trusted_cert_file;
	const char *client_cert_file;
	const char *client_key_file;
	const char *psk_identity;
	const void *psk;
	size_t      psk_len;
	enum xp_tls_version psk_version;
	enum xp_tls_psk_policy psk_policy;
	xp_tls_peer_chain_cb peer_chain_cb;
	void       *peer_chain_cb_arg;
	enum xp_tls_version min_version;
	enum xp_tls_version max_version;
	xp_crypto_secret_callback_t private_key_password;
	void       *private_key_password_arg;
	const struct xp_tls_client_identity *client_identity;
	xp_tls_log_cb log_cb;
	void *log_cb_arg;
	enum xp_tls_log_level log_level;
};

/*
 * Return codes from timed I/O. XP_TLS_OK is success, XP_TLS_TIMEOUT is a
 * no-progress deadline result, and negative values are errors.
 *
 * XP_TLS_TIMEOUT is distinct from XP_TLS_OK so callers can batch — e.g.
 * keep polling instead of forwarding a buffer to the next layer when the
 * underlying read timed out. Matches Cryptlib's CRYPT_ERROR_TIMEOUT
 * semantics that the existing telnets.c/webget.c loops already handle.
 */
#define XP_TLS_OK          0
#define XP_TLS_TIMEOUT     1   /* operation deadline elapsed; *copied is 0 */
#define XP_TLS_ERR        -1   /* generic error — see xp_tls_errstr */
#define XP_TLS_ERR_CLOSED -2   /* peer closed cleanly or connection reset */

/* Open a TLS client session using the supplied authentication policy. */
DLLEXPORT xp_tls_t xp_tls_client_open_config(SOCKET sock,
                                              const struct xp_tls_client_config *config);

DLLEXPORT int xp_tls_server_credentials_load(
	xp_tls_server_credentials_t *out,
	const struct xp_tls_server_credentials_config *config);
DLLEXPORT void xp_tls_server_credentials_retain(
	xp_tls_server_credentials_t credentials);
DLLEXPORT void xp_tls_server_credentials_release(
	xp_tls_server_credentials_t credentials);
DLLEXPORT xp_tls_t xp_tls_server_open(
	SOCKET socket, xp_tls_server_credentials_t credentials,
	const struct xp_tls_server_config *config);

/* Return the protocol version negotiated by an open TLS session. */
DLLEXPORT enum xp_tls_version xp_tls_protocol_version(xp_tls_t ctx);

/*
 * Returns true if the next xp_tls_pop_timeout() can return application data
 * without performing a socket read.  TLS records can decode into more
 * plaintext than the caller asked for, so a previous pop may have
 * left bytes buffered inside the TLS layer; a caller that gates pops
 * on socket-readability needs this OR'd in or it will sit on data
 * that's already in hand.  Cheap (no syscall, no locking required
 * when the caller is the only thread doing pops).
 */
DLLEXPORT bool xp_tls_has_pending(xp_tls_t ctx);

/*
 * Returns true if the active session authenticated with a configured PSK,
 * including a TLS 1.3 external PSK or a TLS 1.2 PSK cipher suite.
 */
DLLEXPORT bool xp_tls_used_psk(xp_tls_t ctx);

/* timeout_ms: zero polls, positive bounds the whole call, negative waits
 * indefinitely. These calls never change socket timeout options. */
DLLEXPORT int xp_tls_pop_timeout(
	xp_tls_t ctx, void *buf, size_t n, size_t *copied, int timeout_ms);
DLLEXPORT int xp_tls_push_timeout(
	xp_tls_t ctx, const void *buf, size_t n, size_t *copied, int timeout_ms);
DLLEXPORT int xp_tls_flush_timeout(xp_tls_t ctx, int timeout_ms);
DLLEXPORT const char *xp_tls_cipher_name(xp_tls_t ctx);
DLLEXPORT enum xp_tls_auth_method xp_tls_authentication_method(xp_tls_t ctx);
/* Counted selected PSK identity. out == NULL queries the required size. */
DLLEXPORT int xp_tls_psk_identity(
	xp_tls_t ctx, void *out, size_t *len);
DLLEXPORT size_t xp_tls_peer_certificate_count(xp_tls_t ctx);
DLLEXPORT int xp_tls_peer_certificate_der(
	xp_tls_t ctx, size_t index, void *out, size_t *len);

/* Mark the protocol session terminating and make a nonblocking best-effort
 * attempt to send close_notify. This never shuts down or closes the socket.
 * The socket owner must then shutdown() the socket, join its I/O workers,
 * and finally call xp_tls_close(). */
DLLEXPORT int xp_tls_terminate(xp_tls_t ctx);

/*
 * Shut down the TLS layer and free the session. Closes the socket only
 * if close_socket is true; otherwise the caller retains the fd (SyncTERM
 * shuts the socket separately from the session to coordinate with its
 * I/O threads).
 */
DLLEXPORT void xp_tls_close(xp_tls_t ctx, bool close_socket);

/*
 * Human-readable description of the last error recorded on ctx. Never
 * returns NULL; returns a static default string if no error is staged.
 */
DLLEXPORT const char *xp_tls_errstr(xp_tls_t ctx);

/*
 * Human-readable description of the last error from a failed
 * xp_tls_client_open_config() (the ctx couldn't be constructed, so no errstr
 * slot exists). Backed by thread-local storage.
 */
DLLEXPORT const char *xp_tls_last_err(void);

#if defined(__cplusplus)
}
#endif

#endif
