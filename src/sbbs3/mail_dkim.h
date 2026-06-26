/* mail_dkim.h: DKIM (RFC 6376) outbound signing for the Synchronet mail server.
 *
 * Produces a relaxed/relaxed rsa-sha256 DKIM-Signature header.  The signer is
 * only functional when built with DKIM_OPENSSL (OpenSSL/libcrypto available);
 * otherwise these are stubs (dkim_available() returns false, dkim_signer_open()
 * returns NULL) so callers degrade to sending unsigned mail.
 *
 * Two-pass friendly: the body hash (bh=) is computed incrementally via the
 * dkim_bodyhash_* streaming API so the message body never has to be buffered.
 */

#ifndef MAIL_DKIM_H_
#define MAIL_DKIM_H_

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Big enough for base64 of a SHA-256 digest (44 chars + NUL) */
#define DKIM_BODYHASH_STRLEN 64

/* true when DKIM signing is compiled in (OpenSSL available) */
bool dkim_available(void);

/* --- signer (holds the loaded private key + identity) --- */
typedef struct dkim_signer dkim_signer_t;

/* Load the RSA private key (PEM) and remember domain/selector.
 * Returns NULL on any failure (caller should log and send unsigned). */
dkim_signer_t* dkim_signer_open(const char* domain, const char* selector, const char* keyfile);
void           dkim_signer_close(dkim_signer_t*);

/* --- streaming relaxed body hash (bh=) --- */
typedef struct dkim_bodyhash dkim_bodyhash_t;

dkim_bodyhash_t* dkim_bodyhash_new(void);
/* Feed raw body bytes.  Line endings may be CRLF or LF; canonicalization
 * normalizes to CRLF and applies relaxed body rules internally. */
void             dkim_bodyhash_update(dkim_bodyhash_t*, const void* data, size_t len);
/* Finalize: writes base64(SHA-256(relaxed-canonicalized body)) to 'out'
 * (>= DKIM_BODYHASH_STRLEN).  Returns false on error. */
bool             dkim_bodyhash_final(dkim_bodyhash_t*, char* out, size_t outsz);
void             dkim_bodyhash_free(dkim_bodyhash_t*);

/* --- signed-header set --- */
typedef struct dkim_headers dkim_headers_t;

dkim_headers_t* dkim_headers_new(void);
/* Add a header field "Name: value" (no trailing CRLF; folded continuation
 * lines, if any, separated by CRLF).  Add order == signing order.  The header
 * name is recorded in the h= tag.  Returns false on allocation failure. */
bool            dkim_headers_add(dkim_headers_t*, const char* field);
void            dkim_headers_free(dkim_headers_t*);

/* Build and sign the DKIM-Signature header from the collected headers and the
 * body hash 'bh'.  'when' is the t= timestamp (0 => omit t=).  Writes the full
 * header including leading "DKIM-Signature: " and trailing CRLF to 'out'.
 * Returns false on error (buffer too small, no headers, signing failure). */
bool dkim_sign(dkim_signer_t*, const dkim_headers_t*, const char* bh, time_t when,
               char* out, size_t outsz);

/* --- capture observer: two-pass send-path integration ---------------------
 * The mail server renders a message to the wire one line at a time; a capture
 * observes each formatted output line (no trailing CRLF, as handed to the
 * socket).  Pass 1 (DKIM_CAP_SIGN) diverts every line to compute the body hash
 * and collect the signed headers; pass 2 (DKIM_CAP_VERIFY) re-hashes the body
 * while the line is also sent, and gates the SMTP terminator on a body-hash
 * match.  The first empty line marks the header/body boundary; the lone "."
 * line is the SMTP end-of-data terminator; leading-dot body lines are
 * un-stuffed.  Returns one of the DKIM_LINE_* actions for the caller to honor. */
enum {
	DKIM_LINE_DIVERT = 0,   /* capture only; do NOT transmit (pass 1) */
	DKIM_LINE_SEND,         /* transmit normally (pass 2) */
	DKIM_LINE_ABORT         /* verify mismatch; suppress line and fail the send */
};
#define DKIM_CAP_SIGN   0
#define DKIM_CAP_VERIFY 1

typedef struct dkim_capture dkim_capture_t;

dkim_capture_t* dkim_capture_new(int mode);
int             dkim_capture_line(dkim_capture_t*, const char* line, size_t len);
/* SIGN: build the DKIM-Signature header (no trailing CRLF) into 'out'; the
 * captured body hash is then available via dkim_capture_bodyhash(). */
bool            dkim_capture_sign(dkim_capture_t*, dkim_signer_t*, time_t when, char* out, size_t outsz);
const char*     dkim_capture_bodyhash(const dkim_capture_t*);
/* VERIFY: provide the expected body hash (from the SIGN pass) before pass 2. */
void            dkim_capture_expect(dkim_capture_t*, const char* bh);
bool            dkim_capture_mismatch(const dkim_capture_t*);
/* true if this capture diverts (never transmits) -- the SIGN/pass-1 mode */
bool            dkim_capture_diverts(const dkim_capture_t*);
void            dkim_capture_free(dkim_capture_t*);

#ifdef __cplusplus
}
#endif

#endif /* MAIL_DKIM_H_ */
