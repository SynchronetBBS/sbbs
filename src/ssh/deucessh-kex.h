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

#include "deucessh.h"
#include "deucessh-key-algo.h"
#include "deucessh-result.h"
#include "deucessh-portable.h"

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
typedef int (*dssh_kex_send_fn)(const uint8_t *payload, size_t len, void *io_ctx);
typedef int (*dssh_kex_recv_fn)(uint8_t *msg_type, uint8_t **payload, size_t *payload_len, void *io_ctx);

/* Server DH-GEX group selection. p and g are nonempty unsigned big-endian
 * byte spans, not SSH mpint encodings. One producer lease covers both spans.
 * The consumer initializes out to zero, validates the selected group and
 * releases every present lease on success, failure or rejected output. Storage
 * remains immutable through backend import and is released before packet I/O.
 * The producer owns its storage; static groups use an explicit no-op release.
 * Return zero or a negative error; positive status is invalid. On failure,
 * publish partial resources through the lease or release them before returning.
 * The common result contract governs lifetime and release restrictions. */
struct dssh_dh_gex_group_result {
	struct dssh_bytes p;
	struct dssh_bytes g;
	struct dssh_result_lease lease;
};

/* Borrowed process-lifetime configuration for diffie-hellman-group-exchange-
 * sha256. Pass to dssh_kex_set_ctx() before the first session is created.
 * select_group may run concurrently for different sessions. Each live result
 * needs independently valid storage, or immutable shared static groups. */
struct dssh_dh_gex_provider {
	int (*select_group)(uint32_t min, uint32_t preferred, uint32_t max,
	    struct dssh_dh_gex_group_result *out, void *cbdata);
	void *cbdata;
};

/*
 * KEX context -- passed to the handler by the library.
 *
 * All fields are borrowed inputs populated by the library before the call.
 */
struct dssh_kex_context {
	/* Version strings (not NUL-terminated, not owned) */
	const char *v_c;
	size_t      v_c_len;
	const char *v_s;
	size_t      v_s_len;

	/* KEXINIT payloads (not owned) */
	const uint8_t *i_c;
	size_t         i_c_len;
	const uint8_t *i_s;
	size_t         i_s_len;

	/* Negotiated host key algorithm (not owned).
         * Provides sign/verify/pubkey function pointers. */
	dssh_key_algo key_algo;

	/* Frozen process-lifetime configuration (e.g. DH-GEX group provider),
	 * separate from this session's initialized state. Neither is owned by
	 * the handler invocation. State belongs to its module's cleanup callback. */
	void *config;
	void *state;

	/* I/O */
	dssh_kex_send_fn send;
	dssh_kex_recv_fn recv;
	void            *io_ctx;

	/* Role: true if this side is the SSH client */
	bool client;

	/* Host key verification callback (client-side only).
	 * Set by the transport layer from the session's callback. */
	dssh_hostkey_verify_cb hostkey_verify;
	void                  *hostkey_verify_cbdata;
};

/* One producer lease covers both immutable, nonempty outputs. shared_secret
 * is unsigned big-endian K, without an SSH length prefix/sign padding.
 * exchange_hash is the complete digest named by the registration. The consumer
 * initializes out to zero, validates success metadata and releases every present
 * lease on all outcomes. Release follows session-ID copy/all key derivations,
 * before state reset/cleanup. The producer clears its actual secret capacity.
 * Publish partial resources through the lease on error or release them yourself.
 * The common result contract governs lifetime and release restrictions. */
struct dssh_kex_result {
	struct dssh_bytes shared_secret;
	struct dssh_bytes exchange_hash;
	struct dssh_result_lease lease;
};

/* Init/reset receives NULL initially, or this creator's previous session state
 * on rekey. Reset in place and retain allocations when possible; always generate
 * fresh ephemeral material. A replaced state is released by its producer.
 * Error leaves NULL or a cleanup-valid owned state. Stateless NULL success is
 * valid. Return zero or a negative error; positive status is invalid. */
typedef int (*dssh_kex_init)(bool client, void *config, void **state);
typedef int (*dssh_kex_handler)(const struct dssh_kex_context *kctx, struct dssh_kex_result *out);

/* All three callbacks are required. Cleanup accepts NULL, releases only this
 * session's state and never destroys the borrowed global configuration. The
 * initialized creator receives cleanup once on replacement/destruction/error;
 * a candidate that never initialized and published no state is not cleaned.
 * Init/cleanup must not enter session/lifecycle/configuration APIs. No physical
 * library mutex spans init, handler, result release or cleanup. */
typedef void (*dssh_kex_cleanup)(void *state);

#define DSSH_KEX_FLAG_NEEDS_ENCRYPTION_CAPABLE UINT32_C(1 << 0)
#define DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE  UINT32_C(1 << 1)
#define DSSH_KEX_FLAG_K_ENCODING_STRING        UINT32_C(1 << 2)

typedef struct dssh_kex_s {
	struct dssh_kex_s *next; /* Registry link; must be NULL at registration. */
	dssh_kex_init      init;
	dssh_kex_handler   handler;
	dssh_kex_cleanup   cleanup;
	const char        *hash_name; /* digest name, e.g. "SHA-256" */
	uint32_t           flags;
	void              *config;
	char               name[];
} *dssh_kex;

/* Application-serialized global setup; finish before the first session-init
 * attempt. Descriptor/name/code stay borrowed for process lifetime. */
DSSH_PUBLIC int dssh_transport_register_kex(dssh_kex kex) DSSH_ABI(dssh_transport_register_kex);

#ifdef __cplusplus
}
#endif

#endif /* DSSH_KEX_H */
