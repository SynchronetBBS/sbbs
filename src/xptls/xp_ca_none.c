#include "xp_ca.h"

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
xp_ca_csr_create(xp_ca_csr_t *out, xp_ca_key_t key)
{
	if (out != NULL)
		*out = NULL;
	(void)key;
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
