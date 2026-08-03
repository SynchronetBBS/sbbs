#ifndef _XP_KEY_H
#define _XP_KEY_H

#include <stdbool.h>
#include <stddef.h>

#include "xp_crypto.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct xp_key *xp_key_t;

enum xp_key_algorithm {
	XP_KEY_ED25519 = 1,
	XP_KEY_RSA,
	XP_KEY_ECDSA,
};

enum xp_key_curve {
	XP_KEY_CURVE_NONE = 0,
	XP_KEY_CURVE_P256,
	XP_KEY_CURVE_P384,
	XP_KEY_CURVE_P521,
};

struct xp_key_spec {
	enum xp_key_algorithm algorithm;
	unsigned bits; /* RSA modulus size; zero for Ed25519 and named EC curves. */
	enum xp_key_curve curve;
};

enum xp_key_store_kind {
	XP_KEY_STORE_MEMORY = 0,
	XP_KEY_STORE_FILE,
	XP_KEY_STORE_PKCS11,
	XP_KEY_STORE_TPM2,
	XP_KEY_STORE_PLATFORM,
};

struct xp_key_info {
	struct xp_key_spec spec;
	bool has_private;
	bool exportable;
	enum xp_key_store_kind storage;
};

enum xp_key_storage_policy {
	XP_KEY_STORAGE_AUTO = 0,
	XP_KEY_STORAGE_HARDWARE,
	XP_KEY_STORAGE_FILE,
	XP_KEY_STORAGE_NAMED,
};

struct xp_key_store_config {
	enum xp_key_storage_policy policy;
	const char *store;     /* file, pkcs11, tpm2, or platform */
	const char *store_uri; /* Store-specific non-secret hardware locator. */
	const char *file_path; /* FILE destination or AUTO fallback. */
	xp_crypto_secret_callback_t authorize;
	void *authorize_context;
};

enum xp_key_store_operation {
	XP_KEY_STORE_CAN_GENERATE = 1u << 0,
	XP_KEY_STORE_CAN_IMPORT = 1u << 1,
	XP_KEY_STORE_CAN_OPEN = 1u << 2,
	XP_KEY_STORE_CAN_SIGN = 1u << 3,
	XP_KEY_STORE_CAN_DESTROY = 1u << 4,
	XP_KEY_STORE_CAN_EXPORT = 1u << 5,
	XP_KEY_STORE_TRANSACTIONAL_IMPORT = 1u << 6,
	XP_KEY_STORE_TRANSACTIONAL_DESTROY = 1u << 7,
};

struct xp_key_store_capabilities {
	bool available;
	int availability_status;
	unsigned operations;
	enum xp_key_store_kind kind;
};

DLLEXPORT int xp_key_generate(xp_key_t *out, const struct xp_key_spec *spec);
DLLEXPORT int xp_key_get_info(xp_key_t key, struct xp_key_info *info);
DLLEXPORT void xp_key_retain(xp_key_t key);
DLLEXPORT void xp_key_release(xp_key_t key);

DLLEXPORT int xp_key_import_private_pem(
	xp_key_t *out, const void *pem, size_t len,
	xp_crypto_secret_callback_t password, void *password_context);
DLLEXPORT int xp_key_export_private_pem(
	xp_key_t key, xp_crypto_secret_callback_t password,
	void *password_context, void *out, size_t *len);
DLLEXPORT int xp_key_load_private_pem_file(
	xp_key_t *out, const char *path, const char *password);
DLLEXPORT int xp_key_save_private_pem_file(
	xp_key_t key, const char *path, const char *password);
DLLEXPORT int xp_key_delete_private_pem_file(const char *path);
DLLEXPORT int xp_key_import_spki_der(xp_key_t *out, const void *der, size_t len);
DLLEXPORT int xp_key_export_spki_der(xp_key_t key, void *out, size_t *len);
DLLEXPORT int xp_key_get_rsa_public(
	xp_key_t key, void *modulus, size_t *modulus_len,
	void *exponent, size_t *exponent_len);
DLLEXPORT int xp_key_get_ec_public(
	xp_key_t key, void *x, size_t *x_len, void *y, size_t *y_len);
DLLEXPORT int xp_key_fingerprint_sha256(
	xp_key_t key, void *out, size_t *len);

DLLEXPORT int xp_key_store_query(
	const struct xp_key_store_config *store, const struct xp_key_spec *spec,
	struct xp_key_store_capabilities *capabilities);
DLLEXPORT int xp_key_generate_stored(
	xp_key_t *out, const struct xp_key_store_config *store,
	const struct xp_key_spec *spec);
DLLEXPORT int xp_key_import_stored(
	xp_key_t *out, const struct xp_key_store_config *store, xp_key_t source);
DLLEXPORT int xp_key_reference(xp_key_t key, void *out, size_t *len);
DLLEXPORT int xp_key_open_stored(
	xp_key_t *out, const struct xp_key_store_config *store,
	const void *reference, size_t reference_len);
DLLEXPORT int xp_key_destroy_stored(
	const struct xp_key_store_config *store,
	const void *reference, size_t reference_len,
	const void *expected_fingerprint, size_t fingerprint_len);

DLLEXPORT const char *xp_key_errstr(xp_key_t key);

#if defined(__cplusplus)
}
#endif

#endif
