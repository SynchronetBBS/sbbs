#include "xp_ca.h"
#include "xp_sign.h"

int
xp_ca_key_generate(xp_ca_key_t *out, enum xp_ca_key_algorithm algorithm)
{
	if (out != NULL)
		*out = NULL;
	(void)algorithm;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_key_load_pem(xp_ca_key_t *out, const char *path, const char *password)
{
	if (out != NULL)
		*out = NULL;
	(void)path;
	(void)password;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_key_save_pem(xp_ca_key_t key, const char *path, const char *password)
{
	(void)key;
	(void)path;
	(void)password;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_key_delete_pem(const char *path)
{
	(void)path;
	return XP_CA_ERR_DISABLED;
}

void
xp_ca_key_free(xp_ca_key_t key)
{
	(void)key;
}

int
xp_ca_key_get_info(xp_ca_key_t key, struct xp_ca_key_info *info)
{
	(void)key;
	(void)info;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_key_import_pem(xp_ca_key_t *out, const void *pem, size_t len,
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
xp_ca_key_export_pem(xp_ca_key_t key, xp_ca_password_callback_t callback,
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
xp_ca_key_import_spki_der(xp_ca_key_t *out, const void *der, size_t len)
{
	if (out != NULL)
		*out = NULL;
	(void)der;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_key_export_spki_der(xp_ca_key_t key, void *out, size_t *len)
{
	(void)key;
	(void)out;
	(void)len;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_key_get_rsa_public(xp_ca_key_t key, void *modulus, size_t *modulus_len,
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
xp_ca_key_get_ec_public(xp_ca_key_t key, void *x, size_t *x_len,
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
xp_ca_csr_create(xp_ca_csr_t *out, xp_ca_key_t key)
{
	if (out != NULL)
		*out = NULL;
	(void)key;
	return XP_CA_ERR_DISABLED;
}

int
xp_ca_csr_create_with_identity(xp_ca_csr_t *out, xp_ca_key_t key,
	const struct xp_ca_identity *identity)
{
	if (out != NULL)
		*out = NULL;
	(void)key;
	(void)identity;
	return XP_CA_ERR_DISABLED;
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

void
xp_ca_csr_free(xp_ca_csr_t csr)
{
	(void)csr;
}

int
xp_ca_cert_create_self_signed(
	xp_ca_cert_t *out,
	xp_ca_key_t key,
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
	xp_ca_key_t issuer_key,
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
xp_ca_cert_get_public_key(xp_ca_key_t *out, xp_ca_cert_t cert)
{
	if (out != NULL)
		*out = NULL;
	(void)cert;
	return XP_CA_ERR_DISABLED;
}

int
xp_sign(xp_ca_key_t key, enum xp_sign_algorithm algorithm,
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
xp_verify(xp_ca_key_t key, enum xp_sign_algorithm algorithm,
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
	xp_ca_key_t issuer_key,
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
