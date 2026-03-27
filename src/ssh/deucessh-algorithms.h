/*
 * deuce-ssh-algorithms.h — Built-in algorithm registration and key management.
 *
 * Include this header to access register_*() functions for built-in
 * algorithms and key load/save/generate functions for key algorithms.
 */

#ifndef DSSH_ALGORITHMS_H
#define DSSH_ALGORITHMS_H

#include "deucessh.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Password callback for PEM key files.  Same signature as OpenSSL's
 * pem_password_cb — buf receives the passphrase, size is the buffer
 * length, rwflag is 0 for reading / 1 for writing, userdata is the
 * opaque pointer passed alongside.  Return the passphrase length.
 */
typedef int (*dssh_pem_password_cb)(char *buf, int size, int rwflag,
    void *userdata);

/* Key exchange */
DSSH_PUBLIC int dssh_register_curve25519_sha256(void);
DSSH_PUBLIC int dssh_register_dh_gex_sha256(void);
DSSH_PUBLIC int dssh_register_sntrup761x25519_sha512(void);
DSSH_PUBLIC int dssh_register_mlkem768x25519_sha256(void);

/* Host key algorithms */
DSSH_PUBLIC int dssh_register_ssh_ed25519(void);
DSSH_PUBLIC int dssh_ed25519_load_key_file(const char *path,
    dssh_pem_password_cb pw_cb, void *pw_cbdata);
DSSH_PUBLIC int dssh_ed25519_save_key_file(const char *path,
    dssh_pem_password_cb pw_cb, void *pw_cbdata);
DSSH_PUBLIC int64_t dssh_ed25519_get_pub_str(char *buf, size_t bufsz);
DSSH_PUBLIC int dssh_ed25519_save_pub_file(const char *path);
DSSH_PUBLIC int dssh_ed25519_generate_key(void);
DSSH_PUBLIC int dssh_register_rsa_sha2_256(void);
DSSH_PUBLIC int dssh_rsa_sha2_256_load_key_file(const char *path,
    dssh_pem_password_cb pw_cb, void *pw_cbdata);
DSSH_PUBLIC int dssh_rsa_sha2_256_save_key_file(const char *path,
    dssh_pem_password_cb pw_cb, void *pw_cbdata);
DSSH_PUBLIC int64_t dssh_rsa_sha2_256_get_pub_str(char *buf, size_t bufsz);
DSSH_PUBLIC int dssh_rsa_sha2_256_save_pub_file(const char *path);
DSSH_PUBLIC int dssh_rsa_sha2_256_generate_key(unsigned int bits);

/* Encryption */
DSSH_PUBLIC int dssh_register_aes256_ctr(void);
DSSH_PUBLIC int dssh_register_none_enc(void);

/* MAC */
DSSH_PUBLIC int dssh_register_hmac_sha2_256(void);
DSSH_PUBLIC int dssh_register_none_mac(void);

/* Compression */
DSSH_PUBLIC int dssh_register_none_comp(void);

/*
 * Set the opaque context pointer for a registered key algorithm.
 * Called by key algorithm modules after generating or loading a key.
 * Returns 0 on success, DSSH_ERROR_INIT if the algorithm is not registered.
 */
DSSH_PUBLIC int dssh_key_algo_set_ctx(const char *name, void *ctx);

/* DH group provider (server-side, for DH-GEX) */
struct dssh_dh_gex_provider {
	int   (*select_group)(uint32_t min, uint32_t preferred, uint32_t max,
	    uint8_t **p, size_t *p_len,
	    uint8_t **g, size_t *g_len,
	    void *cbdata);
	void *cbdata;
};

DSSH_PUBLIC void dssh_dh_gex_set_provider(dssh_session sess,
    struct dssh_dh_gex_provider                       *provider);

#ifdef __cplusplus
}
#endif

#endif // ifndef DSSH_ALGORITHMS_H
