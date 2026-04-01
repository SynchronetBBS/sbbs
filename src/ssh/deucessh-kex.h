/*
 * deucessh-kex.h -- KEX module interface for DeuceSSH.
 *
 * Defines the context struct passed to KEX handlers, providing all
 * inputs, outputs, and I/O function pointers needed to implement a
 * key exchange algorithm without access to session internals.
 */

#ifndef DSSH_KEX_H
#define DSSH_KEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "deucessh-portable.h"
#include "deucessh-key-algo.h"

/* KEX method-specific message types (ECDH, DH) */
#define SSH_MSG_KEX_ECDH_INIT  UINT8_C(30)
#define SSH_MSG_KEX_ECDH_REPLY UINT8_C(31)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * I/O function pointers for KEX handlers.
 *
 * send: sends an SSH transport packet with the given payload.
 *       The payload includes the message type byte.
 *       Returns 0 on success, negative error code on failure.
 *
 * recv: receives an SSH transport packet.  On success, msg_type is
 *       set to the message type, payload points to the raw payload
 *       (including msg_type byte), and payload_len is the total
 *       payload length.  Returns 0 on success.
 *
 * io_ctx is an opaque pointer passed through to each call.
 */
typedef int (*dssh_kex_send_fn)(const uint8_t *payload, size_t len,
    void *io_ctx);
typedef int (*dssh_kex_recv_fn)(uint8_t *msg_type, uint8_t **payload,
    size_t *payload_len, void *io_ctx);

/*
 * KEX context -- passed to the handler by the library.
 *
 * Input fields are populated by the library before the call.
 * Output fields must be set by the handler on success.
 */
struct dssh_kex_context {
	/* Version strings (not NUL-terminated, not owned) */
	const char    *v_c;
	size_t         v_c_len;
	const char    *v_s;
	size_t         v_s_len;

	/* KEXINIT payloads (not owned) */
	const uint8_t *i_c;
	size_t         i_c_len;
	const uint8_t *i_s;
	size_t         i_s_len;

	/* Negotiated host key algorithm (not owned).
	 * Provides sign/verify/pubkey function pointers. */
	dssh_key_algo  key_algo;

	/* KEX-specific data set by the application (e.g. DH-GEX
	 * group provider).  NULL if not applicable. */
	void          *kex_data;

	/* I/O */
	dssh_kex_send_fn  send;
	dssh_kex_recv_fn  recv;
	void             *io_ctx;

	/* Outputs -- handler must malloc these on success.
	 * Caller takes ownership after the handler returns. */
	uint8_t       *shared_secret;
	size_t         shared_secret_sz;
	uint8_t       *exchange_hash;
	size_t         exchange_hash_sz;

	/* Role: true if this side is the SSH client */
	bool           client;
};

/*
 * KEX handler function type.
 * Returns 0 on success, negative error code on failure.
 */
typedef int (*dssh_kex_handler)(struct dssh_kex_context *kctx);

/*
 * KEX cleanup function type.  Called when the session is destroyed.
 * May be NULL if the KEX module has no per-session state to free.
 */
typedef void (*dssh_kex_cleanup)(void *kex_data);

#define DSSH_KEX_FLAG_NEEDS_ENCRYPTION_CAPABLE UINT32_C(1 << 0)
#define DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE  UINT32_C(1 << 1)
#define DSSH_KEX_FLAG_K_ENCODING_STRING        UINT32_C(1 << 2)

typedef struct dssh_kex_s {
	struct dssh_kex_s *next;     /* must be first -- generic traversal assumes offsetof(next) == 0 */
	dssh_kex_handler   handler;
	dssh_kex_cleanup   cleanup;
	const char        *hash_name; /* digest name, e.g. "SHA-256" */
	uint32_t           flags;
	void              *ctx;
	char               name[];
} *dssh_kex;
static_assert(!offsetof(struct dssh_kex_s, next),
    "next must be at offset 0 for generic list traversal");

DSSH_PUBLIC int dssh_transport_register_kex(dssh_kex kex);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_KEX_H */
