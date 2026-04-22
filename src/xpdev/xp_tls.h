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
 * xp_tls — minimal TLS client wrapper for xpdev.
 *
 * Thin, backend-agnostic layer over a TLS client session. Used by
 * SyncTERM's TelnetS (telnets.c) and HTTPS (webget.c); designed to also
 * serve Synchronet server code once that port follows. Backend is
 * OpenSSL libssl or Botan3, selected at build time.
 *
 * The API mirrors what the consumers actually need — push/pop/flush +
 * SNI + read timeout + ownership clearing. Certificate verification
 * follows the backend's default trust store with peer-verify disabled,
 * matching the Cryptlib era's permissive posture; a future iteration
 * should gate on a per-session opt-in (SyncTERM wants lax verify for
 * self-signed BBS certs; Synchronet server wants strict).
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
