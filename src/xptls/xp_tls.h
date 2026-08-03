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
 * xp_tls — minimal TLS client wrapper for xpdev.
 *
 * Thin, backend-agnostic layer over a TLS client session. Used by
 * SyncTERM's TelnetS (telnets.c) and HTTPS (webget.c); designed to also
 * serve Synchronet server code once that port follows. Backend is
 * OpenSSL libssl or Botan3, selected at build time.
 *
 * The API mirrors what the consumers actually need — push/pop/flush +
 * SNI + read timeout + ownership clearing. Certificate verification is
 * selected per client session so legacy callers can remain permissive while
 * callers with an authentication policy can opt into either system trust or
 * a connection-specific trust anchor.
 */

#ifndef _XP_TLS_H
#define _XP_TLS_H

#include <stdbool.h>
#include <stddef.h>

#include "sockwrap.h"	/* SOCKET */
#include "wrapdll.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct xp_tls_ctx *xp_tls_t;

enum xp_tls_server_auth {
	XP_TLS_SERVER_AUTH_NONE,
	XP_TLS_SERVER_AUTH_WEB_PKI,
	XP_TLS_SERVER_AUTH_CERTIFICATE,
	XP_TLS_SERVER_AUTH_UNTRUSTED,
};

enum xp_tls_version {
	XP_TLS_VERSION_UNKNOWN,
	XP_TLS_VERSION_1_2,
	XP_TLS_VERSION_1_3,
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
 *                     still reporting the presented chain. NONE is reserved
 *                     for the legacy xp_tls_client_open() wrapper and is
 *                     rejected by this configured interface.
 * trusted_cert_file — PEM trust anchor for CERTIFICATE mode. A leaf
 *                     certificate is accepted as the explicit trust anchor.
 * client_cert_file  — PEM certificate or chain presented to the server.
 * client_key_file   — matching unencrypted PEM private key.
 * psk_identity/psk  — when supplied, use a PSK-only handshake at the exact
 *                     version selected by psk_version; certificate trust and
 *                     client identity are ignored.
 * peer_chain_cb     — optional diagnostic callback invoked with the
 *                     certificate chain presented by the peer when
 *                     certificate verification rejects the handshake.
 */
struct xp_tls_client_config {
	const char *server_name;
	int         read_timeout;
	enum xp_tls_server_auth server_auth;
	const char *trusted_cert_file;
	const char *client_cert_file;
	const char *client_key_file;
	const char *psk_identity;
	const void *psk;
	size_t      psk_len;
	enum xp_tls_version psk_version;
	xp_tls_peer_chain_cb peer_chain_cb;
	void       *peer_chain_cb_arg;
};

/*
 * Return codes from push/pop/flush. Non-negative values are success;
 * negative values are error.
 *
 * XP_TLS_TIMEOUT is distinct from XP_TLS_OK so callers can batch — e.g.
 * keep polling instead of flushing a buffer to the next layer when the
 * underlying read timed out. Matches Cryptlib's CRYPT_ERROR_TIMEOUT
 * semantics that the existing telnets.c/webget.c loops already handle.
 */
#define XP_TLS_OK          0
#define XP_TLS_TIMEOUT     1   /* read-timeout elapsed; *copied may be 0 or partial */
#define XP_TLS_ERR        -1   /* generic error — see xp_tls_errstr */
#define XP_TLS_ERR_CLOSED -2   /* peer closed cleanly or connection reset */

/*
 * Open a TLS 1.2/1.3 client session over an already-connected socket.
 *
 * sock          — connected SOCKET (caller retains ownership; it is
 *                 referenced by the TLS layer and must outlive the ctx).
 * sni           — server name for SNI + (optional) hostname binding.
 *                 NULL to skip (some BBS servers don't use SNI).
 * read_timeout  — seconds to block on a single pop() before returning
 *                 0 copied. 0 means blocking-forever; not recommended.
 *
 * Performs handshake before returning. On failure, returns NULL; call
 * xp_tls_last_err() for a human-readable reason if a fresh ctx wasn't
 * produced.
 */
DLLEXPORT xp_tls_t xp_tls_client_open(SOCKET sock, const char *sni, int read_timeout);

/* Open a TLS client session using the supplied authentication policy. */
DLLEXPORT xp_tls_t xp_tls_client_open_config(SOCKET sock,
                                              const struct xp_tls_client_config *config);

/* Return the protocol version negotiated by an open TLS session. */
DLLEXPORT enum xp_tls_version xp_tls_protocol_version(xp_tls_t ctx);

/*
 * Open a TLS-PSK 1.2 client session.  Identical to xp_tls_client_open()
 * except authentication uses a pre-shared key instead of certificates.
 *
 * identity      — PSK identity string (NUL-terminated).  Sent in the clear
 *                 in the ClientKeyExchange.
 * psk           — raw PSK bytes (not hex-encoded).  Caller retains ownership.
 * psk_len       — length of psk in bytes.
 *
 * TLS 1.3 is not negotiated for PSK sessions: the handshake floor is pinned
 * to TLS 1.2 so the wire format matches the broker's PSK expectations.
 */
DLLEXPORT xp_tls_t xp_tls_client_open_psk(SOCKET sock, const char *sni,
                                          int read_timeout,
                                          const char *identity,
                                          const void *psk, size_t psk_len);

/*
 * Push up to n bytes. On return, *copied holds bytes actually written
 * (may be less than n if the peer's TLS flow-control kicks in).
 * Returns XP_TLS_OK on progress (including partial), XP_TLS_ERR_CLOSED
 * if the peer closed mid-write, XP_TLS_ERR otherwise.
 */
DLLEXPORT int xp_tls_push(xp_tls_t ctx, const void *buf, size_t n, size_t *copied);

/*
 * Pop up to n bytes. On return, *copied holds bytes actually read.
 * If the read blocks until the context's read_timeout expires, returns
 * XP_TLS_OK with *copied = 0. Returns XP_TLS_ERR_CLOSED on clean close,
 * XP_TLS_ERR on protocol error.
 */
DLLEXPORT int xp_tls_pop(xp_tls_t ctx, void *buf, size_t n, size_t *copied);

/*
 * Returns true if the next xp_tls_pop() can return application data
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

/*
 * Ensure any buffered plaintext/ciphertext is sent on the wire.
 * Always safe to call; no-op if there's nothing queued.
 */
DLLEXPORT int xp_tls_flush(xp_tls_t ctx);

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
 * xp_tls_client_open() (the ctx couldn't be constructed, so no errstr
 * slot exists). Backed by thread-local storage.
 */
DLLEXPORT const char *xp_tls_last_err(void);

#if defined(__cplusplus)
}
#endif

#endif
