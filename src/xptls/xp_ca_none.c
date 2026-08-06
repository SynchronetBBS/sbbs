#include "xp_ca.h"
#include "xp_sign.h"
#include "xp_key_internal.h"

#include <string.h>

int
xp_key_generate(xp_key_t *out, const struct xp_key_spec *spec)
{
	if (out != NULL)
		*out = NULL;
	(void)spec;
	return XP_CA_ERR_DISABLED;
}

int
xp_key_load_private_pem_file(xp_key_t *out, const char *path, const char *password)
{
	if (out != NULL)
		*out = NULL;
	(void)path;
	(void)password;
	return XP_CA_ERR_DISABLED;
}

int
xp_key_save_private_pem_file(xp_key_t key, const char *path, const char *password)
{
	(void)key;
	(void)path;
	(void)password;
	return XP_CA_ERR_DISABLED;
}

int
xp_key_delete_private_pem_file(const char *path)
{
	(void)path;
	return XP_CA_ERR_DISABLED;
}

void
xp_key_release(xp_key_t key)
{
	(void)key;
}

void
xp_key_retain(xp_key_t key)
{
	(void)key;
}

const char *
xp_key_errstr(xp_key_t key)
{
	(void)key;
	return "crypto support disabled";
}

void
xp_key_set_storage_metadata(
	xp_key_t key, enum xp_key_store_kind storage, bool exportable)
{
	(void)key;
	(void)storage;
	(void)exportable;
}

void *xp_key_native_private(xp_key_t key) { (void)key; return NULL; }

int xp_key_set_reference(xp_key_t key, const void *reference, size_t len)
{
	(void)key; (void)reference; (void)len;
	return XP_CRYPTO_ERR_DISABLED;
}

int xp_key_reference(xp_key_t key, void *out, size_t *len)
{
	(void)key; (void)out;
	if (len != NULL) *len = 0;
	return XP_CRYPTO_ERR_DISABLED;
}

int xp_key_provider_store_query(const struct xp_key_store_config *store,
	const struct xp_key_spec *spec, struct xp_key_store_capabilities *capabilities)
{
	(void)store; (void)spec;
	if (capabilities != NULL) {
		memset(capabilities, 0, sizeof(*capabilities));
		capabilities->kind = store != NULL && store->store != NULL
			&& strcmp(store->store, "pkcs11") == 0 ? XP_KEY_STORE_PKCS11
			: store != NULL && store->store != NULL
			&& strcmp(store->store, "tpm2") == 0 ? XP_KEY_STORE_TPM2
			: XP_KEY_STORE_PLATFORM;
		capabilities->availability_status = XP_CRYPTO_ERR_DISABLED;
	}
	return XP_CRYPTO_OK;
}

int xp_key_provider_generate_stored(xp_key_t *out, void *locator,
	size_t *locator_len, const struct xp_key_store_config *store,
	const struct xp_key_spec *spec)
{
	if (out != NULL) *out = NULL;
	(void)locator; (void)locator_len; (void)store; (void)spec;
	return XP_CRYPTO_ERR_DISABLED;
}

int xp_key_provider_import_stored(xp_key_t *out, void *locator,
	size_t *locator_len, const struct xp_key_store_config *store, xp_key_t source)
{
	if (out != NULL) *out = NULL;
	(void)locator; (void)locator_len; (void)store; (void)source;
	return XP_CRYPTO_ERR_DISABLED;
}

int xp_key_provider_open_stored(xp_key_t *out,
	const struct xp_key_store_config *store, const void *locator, size_t locator_len)
{
	if (out != NULL) *out = NULL;
	(void)store; (void)locator; (void)locator_len;
	return XP_CRYPTO_ERR_DISABLED;
}

int xp_key_provider_destroy_stored(const struct xp_key_store_config *store,
	const void *locator, size_t locator_len,
	const void *expected_fingerprint, size_t fingerprint_len)
{
	(void)store; (void)locator; (void)locator_len;
	(void)expected_fingerprint; (void)fingerprint_len;
	return XP_CRYPTO_ERR_DISABLED;
}

int
xp_key_get_info(xp_key_t key, struct xp_key_info *info)
{
	(void)key;
	(void)info;
	return XP_CA_ERR_DISABLED;
}

int
xp_key_import_private_pem(xp_key_t *out, const void *pem, size_t len,
	xp_ca_password_callback_t callback, void *context)
{
	if (out != NULL)
		*out = NULL;
	(void)pem;
	(void)len;
	(void)callback;
	(void)context;
	return XP_CA_ERR_DISABLED;
}

int
xp_key_export_private_pem(xp_key_t key, xp_ca_password_callback_t callback,
	void *context, void *out, size_t *len)
{
	(void)key;
	(void)callback;
	(void)context;
	(void)out;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

int
xp_key_import_spki_der(xp_key_t *out, const void *der, size_t len)
{
	if (out != NULL)
		*out = NULL;
	(void)der;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

int
xp_key_export_spki_der(xp_key_t key, void *out, size_t *len)
{
	(void)key;
	(void)out;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

int
xp_key_get_rsa_public(xp_key_t key, void *modulus, size_t *modulus_len,
	void *exponent, size_t *exponent_len)
{
	(void)key;
	(void)modulus;
	(void)modulus_len;
	(void)exponent;
	(void)exponent_len;
	return XP_CA_ERR_DISABLED;
}

int
xp_key_get_ec_public(xp_key_t key, void *x, size_t *x_len,
	void *y, size_t *y_len)
{
	(void)key;
	(void)x;
	(void)x_len;
	(void)y;
	(void)y_len;
	return XP_CA_ERR_DISABLED;
}

int
xp_key_fingerprint_sha256(xp_key_t key, void *out, size_t *len)
{
	(void)key;
	(void)out;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_csr_create(xp_ca_csr_t *out, xp_key_t key)
{
	if (out != NULL)
		*out = NULL;
	(void)key;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_csr_create_with_identity(xp_ca_csr_t *out, xp_key_t key,
	const struct xp_ca_identity *identity)
{
	if (out != NULL)
		*out = NULL;
	(void)key;
	(void)identity;
	return XP_CA_ERR_DISABLED;
}

int xp_ca_csr_create_request(xp_ca_csr_t *out, xp_key_t key,
	const struct xp_ca_csr_request *request)
{
	if (out != NULL) *out = NULL;
	(void)key; (void)request; return XP_CA_ERR_DISABLED;
}

int xp_ca_csr_import(xp_ca_csr_t *out, enum xp_ca_encoding encoding,
	const void *data, size_t len)
{
	if (out != NULL) *out = NULL;
	(void)encoding; (void)data; (void)len; return XP_CA_ERR_DISABLED;
}

int xp_ca_csr_export(xp_ca_csr_t csr, enum xp_ca_encoding encoding,
	void *out, size_t *len)
{
	(void)csr; (void)encoding; (void)out; (void)len; return XP_CA_ERR_DISABLED;
}

int
xp_ca_csr_import_der(xp_ca_csr_t *out, const void *der, size_t len)
{
	if (out != NULL)
		*out = NULL;
	(void)der;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_csr_export_der(xp_ca_csr_t csr, void *out, size_t *len)
{
	(void)csr;
	(void)out;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_csr_verify(xp_ca_csr_t csr)
{
	(void)csr;
	return XP_CA_ERR_DISABLED;
}

int xp_ca_csr_get_public_key(xp_key_t *out, xp_ca_csr_t csr)
{
	if (out != NULL) *out = NULL;
	(void)csr; return XP_CA_ERR_DISABLED;
}

void
xp_ca_csr_free(xp_ca_csr_t csr)
{
	(void)csr;
}

int
xp_ca_cert_create_self_signed(
	xp_ca_cert_t *out,
	xp_key_t key,
	const struct xp_ca_issue_request *request)
{
	if (out != NULL)
		*out = NULL;
	(void)key;
	(void)request;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_cert_issue(
	xp_ca_cert_t *out,
	xp_key_t issuer_key,
	xp_ca_cert_t issuer,
	xp_ca_csr_t csr,
	const struct xp_ca_issue_request *request)
{
	if (out != NULL)
		*out = NULL;
	(void)issuer_key;
	(void)issuer;
	(void)csr;
	(void)request;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_cert_import_der(xp_ca_cert_t *out, const void *der, size_t len)
{
	if (out != NULL)
		*out = NULL;
	(void)der;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_cert_export_der(xp_ca_cert_t cert, void *out, size_t *len)
{
	(void)cert;
	(void)out;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

void
xp_ca_cert_free(xp_ca_cert_t cert)
{
	(void)cert;
}

int
xp_ca_cert_chain_import_pem(xp_ca_cert_t **out, size_t *count,
	const void *pem, size_t len)
{
	if (out != NULL)
		*out = NULL;
	if (count != NULL)
		*count = 0;
	(void)pem;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_cert_chain_export_pem(const xp_ca_cert_t *certs, size_t count,
	void *out, size_t *len)
{
	(void)certs;
	(void)count;
	(void)out;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

int xp_ca_cert_bundle_import(xp_ca_cert_t **out, size_t *count,
	enum xp_ca_encoding encoding, const void *data, size_t len)
{
	if (out != NULL) *out = NULL; if (count != NULL) *count = 0;
	(void)encoding; (void)data; (void)len; return XP_CA_ERR_DISABLED;
}

int xp_ca_cert_bundle_export(const xp_ca_cert_t *certs, size_t count,
	enum xp_ca_encoding encoding, void *out, size_t *len)
{
	(void)certs; (void)count; (void)encoding; (void)out; (void)len;
	return XP_CA_ERR_DISABLED;
}

void
xp_ca_cert_chain_free(xp_ca_cert_t *certs, size_t count)
{
	(void)certs;
	(void)count;
}

int
xp_ca_cert_get_validity(xp_ca_cert_t cert, time_t *not_before,
	time_t *not_after)
{
	(void)cert;
	(void)not_before;
	(void)not_after;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_cert_get_public_key(xp_key_t *out, xp_ca_cert_t cert)
{
	if (out != NULL)
		*out = NULL;
	(void)cert;
	return XP_CA_ERR_DISABLED;
}

int xp_ca_cert_get_info(xp_ca_cert_t cert, struct xp_ca_cert_info *info)
{ (void)cert; (void)info; return XP_CA_ERR_DISABLED; }
int xp_ca_cert_get_serial(xp_ca_cert_t cert, void *out, size_t *len)
{ (void)cert; (void)out; (void)len; return XP_CA_ERR_DISABLED; }
int xp_ca_cert_get_fingerprint(xp_ca_cert_t cert,
	enum xp_digest_algorithm digest, void *out, size_t *len)
{ (void)cert; (void)digest; (void)out; (void)len; return XP_CA_ERR_DISABLED; }
int xp_ca_cert_get_name_count(xp_ca_cert_t cert, enum xp_ca_name_kind kind,
	enum xp_ca_name_field field, size_t *count)
{ (void)cert; (void)kind; (void)field; (void)count; return XP_CA_ERR_DISABLED; }
int xp_ca_cert_get_name(xp_ca_cert_t cert, enum xp_ca_name_kind kind,
	enum xp_ca_name_field field, size_t index, void *out, size_t *len)
{ (void)cert; (void)kind; (void)field; (void)index; (void)out; (void)len; return XP_CA_ERR_DISABLED; }
int xp_ca_cert_get_san_count(xp_ca_cert_t cert, enum xp_ca_san_type type,
	size_t *count)
{ (void)cert; (void)type; (void)count; return XP_CA_ERR_DISABLED; }
int xp_ca_cert_get_san(xp_ca_cert_t cert, enum xp_ca_san_type type,
	size_t index, void *out, size_t *len)
{ (void)cert; (void)type; (void)index; (void)out; (void)len; return XP_CA_ERR_DISABLED; }
int xp_ca_cert_verify_signature(xp_ca_cert_t cert, xp_ca_cert_t issuer)
{ (void)cert; (void)issuer; return XP_CA_ERR_DISABLED; }

int
xp_sign(xp_key_t key, enum xp_sign_algorithm algorithm,
	enum xp_signature_encoding format, const void *data, size_t data_len,
	void *signature, size_t *signature_len)
{
	(void)key;
	(void)algorithm;
	(void)format;
	(void)data;
	(void)data_len;
	(void)signature;
	(void)signature_len;
	return XP_CA_ERR_DISABLED;
}

int
xp_verify(xp_key_t key, enum xp_sign_algorithm algorithm,
	enum xp_signature_encoding format, const void *data, size_t data_len,
	const void *signature, size_t signature_len)
{
	(void)key;
	(void)algorithm;
	(void)format;
	(void)data;
	(void)data_len;
	(void)signature;
	(void)signature_len;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_crl_create(
	xp_ca_crl_t *out,
	xp_key_t issuer_key,
	xp_ca_cert_t issuer,
	xp_ca_crl_t previous,
	const struct xp_ca_crl_request *request)
{
	if (out != NULL)
		*out = NULL;
	(void)issuer_key;
	(void)issuer;
	(void)previous;
	(void)request;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_crl_import_der(xp_ca_crl_t *out, const void *der, size_t len)
{
	if (out != NULL)
		*out = NULL;
	(void)der;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_crl_export_der(xp_ca_crl_t crl, void *out, size_t *len)
{
	(void)crl;
	(void)out;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_crl_get_info(xp_ca_crl_t crl, struct xp_ca_crl_info *info)
{
	(void)crl;
	(void)info;
	return XP_CA_ERR_DISABLED;
}

void
xp_ca_crl_free(xp_ca_crl_t crl)
{
	(void)crl;
}

int
xp_ca_cert_validate(
	xp_ca_cert_t leaf,
	const xp_ca_cert_t *chain,
	size_t chain_count,
	xp_ca_cert_t root,
	const xp_ca_crl_t *crls,
	size_t crl_count,
	const struct xp_ca_validation_policy *policy)
{
	(void)leaf;
	(void)chain;
	(void)chain_count;
	(void)root;
	(void)crls;
	(void)crl_count;
	(void)policy;
	return XP_CA_ERR_DISABLED;
}

const char *
xp_ca_last_error(void)
{
	return "certificate authority support disabled";
}

int
xp_ca_cert_tls_server_usable(xp_ca_cert_t certificate)
{
	(void)certificate;
	return XP_CRYPTO_ERR_DISABLED;
}
