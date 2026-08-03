#ifndef _XP_KEY_INTERNAL_H
#define _XP_KEY_INTERNAL_H

#include "xp_key.h"

#if defined(__cplusplus)
extern "C" {
#endif

void xp_key_set_storage_metadata(
	xp_key_t key, enum xp_key_store_kind storage, bool exportable);
int xp_key_set_reference(xp_key_t key, const void *reference, size_t len);
void *xp_key_native_private(xp_key_t key);
/* Takes ownership of a provider-native private key. */
xp_key_t xp_key_wrap_native_private(
	void *native, enum xp_key_store_kind storage, bool exportable);

int xp_key_provider_store_query(
	const struct xp_key_store_config *store, const struct xp_key_spec *spec,
	struct xp_key_store_capabilities *capabilities);
int xp_key_provider_generate_stored(
	xp_key_t *out, void *locator, size_t *locator_len,
	const struct xp_key_store_config *store, const struct xp_key_spec *spec);
int xp_key_provider_import_stored(
	xp_key_t *out, void *locator, size_t *locator_len,
	const struct xp_key_store_config *store, xp_key_t source);
int xp_key_provider_open_stored(
	xp_key_t *out, const struct xp_key_store_config *store,
	const void *locator, size_t locator_len);
int xp_key_provider_destroy_stored(
	const struct xp_key_store_config *store,
	const void *locator, size_t locator_len,
	const void *expected_fingerprint, size_t fingerprint_len);

#if defined(__cplusplus)
}
#endif

#endif
