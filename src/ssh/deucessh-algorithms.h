/*
 * deuce-ssh-algorithms.h -- Built-in algorithm registration and key management.
 *
 * Include this header to access register_*() functions for built-in
 * algorithms and key load/save/generate functions for key algorithms.
 */

#ifndef DSSH_ALGORITHMS_H
#define DSSH_ALGORITHMS_H

#include "deucessh.h"
#include "deucessh-result.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Password callback for PEM key files.  Same signature as OpenSSL's
 * pem_password_cb -- buf receives the passphrase, size is the buffer
 * length, rwflag is 0 for reading / 1 for writing, userdata is the
 * opaque pointer passed alongside. Return a byte count in [0, size], including
 * zero for an empty password; a negative result reports failure. A full buffer
 * is valid and needs no NUL terminator. Passphrases may contain NUL bytes.
 * Do not throw C++ exceptions through this callback. NULL callback permits
 * unencrypted input/output only and never invokes a provider prompt; cbdata
 * is ignored in that case. File imports preserve the installed key on error.
 */
typedef int (*dssh_pem_password_cb)(char *buf, int size, int rwflag, void *userdata);

/* Global registration and global key replacement use application-serialized setup;
 * complete them before any session-init attempt. Before freeze, serialize key
 * utilities with replacement, including work on other application threads.
 * Same-thread password-callback key replacement is allowed; key-file export
 * uses the native key selected on entry. After freeze, key reads/exports can
 * overlap with sessions and each other, with caller-coordinated destinations.
 * See the process-global setup contract at dssh_session_init(). Session-local
 * credentials below follow their separate per-session contract. */

/* Key exchange */
DSSH_PUBLIC int dssh_register_curve25519_sha256(void) DSSH_ABI(dssh_register_curve25519_sha256);
DSSH_PUBLIC int dssh_register_dh_gex_sha256(void) DSSH_ABI(dssh_register_dh_gex_sha256);
DSSH_PUBLIC int dssh_register_dh_group14_sha256(void) DSSH_ABI(dssh_register_dh_group14_sha256);
DSSH_PUBLIC int dssh_register_dh_group16_sha512(void) DSSH_ABI(dssh_register_dh_group16_sha512);
DSSH_PUBLIC int dssh_register_sntrup761x25519_sha512(void) DSSH_ABI(dssh_register_sntrup761x25519_sha512);
DSSH_PUBLIC int dssh_register_mlkem768x25519_sha256(void) DSSH_ABI(dssh_register_mlkem768x25519_sha256);

/* Private-key save contract (all three builtin key families):
 * Save PEM by opening and overwriting the selected file itself. Validate a
 * regular, singly linked, effective-caller-owned target before changing it;
 * reject a final-component symbolic link/reparse point. New/final files are
 * private to the effective caller (POSIX mode 0600 or protected Windows
 * caller-SID ACL). An error may leave the destination empty or partial.
 * Existing file identity is retained; the caller coordinates the destination
 * directory/namespace throughout the operation and manages recovery copies. */

/* Host key algorithms */
/* Load or generate keys before the first dssh_session_init(). Subsequent
 * key replacement returns DSSH_ERROR_TOOLATE. Failed replacement preserves
 * the existing key. Public-key blobs are cached before key publication. */
DSSH_PUBLIC int     dssh_register_ssh_ed25519(void) DSSH_ABI(dssh_register_ssh_ed25519);
DSSH_PUBLIC int     dssh_ed25519_load_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata) DSSH_ABI(dssh_ed25519_load_key_file);
DSSH_PUBLIC int     dssh_ed25519_save_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata) DSSH_ABI(dssh_ed25519_save_key_file);
DSSH_PUBLIC int64_t dssh_ed25519_get_pub_str(char *buf, size_t bufsz) DSSH_ABI(dssh_ed25519_get_pub_str);
DSSH_PUBLIC int     dssh_ed25519_save_pub_file(const char *path) DSSH_ABI(dssh_ed25519_save_pub_file);
DSSH_PUBLIC int     dssh_ed25519_generate_key(void) DSSH_ABI(dssh_ed25519_generate_key);
DSSH_PUBLIC int     dssh_register_rsa_sha2_256(void) DSSH_ABI(dssh_register_rsa_sha2_256);
DSSH_PUBLIC int     dssh_rsa_sha2_256_load_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata) DSSH_ABI(dssh_rsa_sha2_256_load_key_file);
DSSH_PUBLIC int     dssh_rsa_sha2_256_save_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata) DSSH_ABI(dssh_rsa_sha2_256_save_key_file);
DSSH_PUBLIC int64_t dssh_rsa_sha2_256_get_pub_str(char *buf, size_t bufsz) DSSH_ABI(dssh_rsa_sha2_256_get_pub_str);
DSSH_PUBLIC int     dssh_rsa_sha2_256_save_pub_file(const char *path) DSSH_ABI(dssh_rsa_sha2_256_save_pub_file);
DSSH_PUBLIC int     dssh_rsa_sha2_256_generate_key(unsigned int bits) DSSH_ABI(dssh_rsa_sha2_256_generate_key);
DSSH_PUBLIC int     dssh_register_rsa_sha2_512(void) DSSH_ABI(dssh_register_rsa_sha2_512);
DSSH_PUBLIC int     dssh_rsa_sha2_512_load_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata) DSSH_ABI(dssh_rsa_sha2_512_load_key_file);
DSSH_PUBLIC int     dssh_rsa_sha2_512_save_key_file(const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata) DSSH_ABI(dssh_rsa_sha2_512_save_key_file);
DSSH_PUBLIC int64_t dssh_rsa_sha2_512_get_pub_str(char *buf, size_t bufsz) DSSH_ABI(dssh_rsa_sha2_512_get_pub_str);
DSSH_PUBLIC int     dssh_rsa_sha2_512_save_pub_file(const char *path) DSSH_ABI(dssh_rsa_sha2_512_save_pub_file);
DSSH_PUBLIC int     dssh_rsa_sha2_512_generate_key(unsigned int bits) DSSH_ABI(dssh_rsa_sha2_512_generate_key);

/* Encryption */
DSSH_PUBLIC int dssh_register_aes256_ctr(void) DSSH_ABI(dssh_register_aes256_ctr);
DSSH_PUBLIC int dssh_register_aes128_cbc(void) DSSH_ABI(dssh_register_aes128_cbc);
DSSH_PUBLIC int dssh_register_none_enc(void) DSSH_ABI(dssh_register_none_enc);

/* MAC */
DSSH_PUBLIC int dssh_register_hmac_sha2_256(void) DSSH_ABI(dssh_register_hmac_sha2_256);
DSSH_PUBLIC int dssh_register_hmac_sha2_512(void) DSSH_ABI(dssh_register_hmac_sha2_512);
DSSH_PUBLIC int dssh_register_none_mac(void) DSSH_ABI(dssh_register_none_mac);

/* Compression */
DSSH_PUBLIC int dssh_register_none_comp(void) DSSH_ABI(dssh_register_none_comp);

/*
 * Set the global server/fallback context pointer for a registered key algorithm.
 * Called by key algorithm modules after generating or loading a key.
 * Optional for client-only use; session-local client keys are separate.
 * Must be called before dssh_session_init() -- returns DSSH_ERROR_TOOLATE
 * if any session has been created (the global registry is read
 * concurrently by active sessions during KEX).
 * Returns 0 on success, DSSH_ERROR_INIT if the algorithm is not registered.
 */
DSSH_PUBLIC int dssh_key_algo_set_ctx(const char *name, void *ctx) DSSH_ABI(dssh_key_algo_set_ctx);

/* Client credentials are per session and per registered signing algorithm.
 * Authentication uses the local context when installed, otherwise the global
 * key. A rejected local key does not automatically try the global key.
 * Global keys are optional for clients, and remain frozen global setup.
 *
 * Serialize these operations with all authentication, handshake and lifecycle
 * operations on the same session. Different sessions are independent. Calls
 * from callbacks on this session or result-release callbacks are invalid.
 * Load/generate/set/clear require a live client session before successful
 * authentication or connection startup; later mutation returns TOOLATE.
 * They may run before or after handshake, including between failed attempts.
 * Failed replacement preserves the installed key. Clearing is idempotent and
 * restores global fallback. Session cleanup releases installed local keys.
 *
 * Builtin load/generate/private-save support ssh-ed25519, rsa-sha2-256 and
 * rsa-sha2-512. Generation uses 256 bits for Ed25519 or the RSA modulus size.
 * File/password rules match the global helpers. Export functions select the
 * same local/global key as authentication and also work after authentication.
 * Public-text counts include NUL; NULL buf or zero size queries the count.
 * Custom modules use set_client_key_ctx and their own private-file helpers.
 * Missing algorithms/keys return INIT; unsupported builtin operations or
 * invalid arguments return INVALID; other errors use DSSH_ERROR_*.
 */
DSSH_PUBLIC int dssh_session_load_client_key_file(dssh_session sess, const char *algo_name,
    const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata) DSSH_ABI(dssh_session_load_client_key_file);
DSSH_PUBLIC int dssh_session_generate_client_key(dssh_session sess, const char *algo_name,
    unsigned int bits) DSSH_ABI(dssh_session_generate_client_key);
DSSH_PUBLIC int dssh_session_clear_client_key(dssh_session sess,
    const char *algo_name) DSSH_ABI(dssh_session_clear_client_key);
DSSH_PUBLIC int dssh_session_save_client_key_file(dssh_session sess, const char *algo_name,
    const char *path, dssh_pem_password_cb pw_cb, void *pw_cbdata) DSSH_ABI(dssh_session_save_client_key_file);
DSSH_PUBLIC int64_t dssh_session_get_client_key_pub_str(dssh_session sess, const char *algo_name,
    char *buf, size_t bufsz) DSSH_ABI(dssh_session_get_client_key_pub_str);
DSSH_PUBLIC int dssh_session_save_client_pub_file(dssh_session sess, const char *algo_name,
    const char *path) DSSH_ABI(dssh_session_save_client_pub_file);

/* ctx must suit the registered module's haskey/pubkey/sign callbacks. Those
 * callbacks and the descriptor remain borrowed for process lifetime. A real
 * release callback is required, including for static or stateless contexts;
 * ctx and lease->owner may be NULL. The producer retains allocation ownership.
 * On success the session stores ctx and moves/zeros *lease. On every failure
 * ctx and *lease remain with the caller. Release runs exactly once on later
 * replacement, clear or session cleanup, without library locks, under the
 * common result-release restrictions. Independent installations need their
 * own producer leases, including when they refer to shared/refcounted storage.
 * Keep key material stable until release; synchronize producer operation state.
 */
DSSH_PUBLIC int dssh_session_set_client_key_ctx(dssh_session sess, const char *algo_name,
    void *ctx, struct dssh_result_lease *lease) DSSH_ABI(dssh_session_set_client_key_ctx);

/*
 * Set borrowed process-lifetime configuration for a registered KEX algorithm.
 * This becomes kctx->config and the initializer's config argument; per-session
 * state is created/reset by that module's init callback and cleaned by its
 * cleanup callback. Neither callback owns this global configuration.
 * Must be called before dssh_session_init() -- returns DSSH_ERROR_TOOLATE
 * if any session has been created (the global registry is read
 * concurrently by active sessions during KEX).
 * Returns 0 on success, DSSH_ERROR_INIT if the algorithm is not registered.
 */
DSSH_PUBLIC int dssh_kex_set_ctx(const char *name, void *ctx) DSSH_ABI(dssh_kex_set_ctx);

#ifdef __cplusplus
}
#endif

#endif   // ifndef DSSH_ALGORITHMS_H
