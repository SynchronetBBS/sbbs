#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "datewrap.h"
#include "xp_ca.h"
#include "xp_ca_file.h"
#include "xp_ca_policy.h"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>

struct xp_ca_key {
	EVP_PKEY *native;
};
struct xp_ca_csr {
	X509_REQ *native;
};
struct xp_ca_cert {
	X509 *native;
};
struct xp_ca_crl {
	X509_CRL *native;
};
static _Thread_local char error_text[256];
static int
fail(int code, const char *what)
{
	snprintf(error_text, sizeof error_text, "%s", what);
	return code;
}
const char     *
xp_ca_last_error(void)
{
	return error_text[0] ? error_text : "no error";
}
static int
der(int n, unsigned char *p, void *out, size_t * len)
{
	if (n <= 0 || p == NULL || !len) {
		return XP_CA_ERR;
	}
	if (!out) {
		*len = n;
		return XP_CA_OK;
	}
	if (*len < (size_t) n) {
		*len = n;
		return XP_CA_ERR;
	}
	memcpy(out, p, n);
	*len = n;
	return XP_CA_OK;
}
static int
add(X509 * x, X509 * i, int nid, const char *v)
{
	X509V3_CTX      c;
	X509V3_set_ctx(&c, i, x, 0, 0, 0);
	X509_EXTENSION *e = X509V3_EXT_conf_nid(0, &c, nid, (char *)v);
	if (!e) {
		return 0;
	}
	int             r = X509_add_ext(x, e, -1);
	X509_EXTENSION_free(e);
	return r;
}

static int
add_crl_extension(X509_CRL *crl, X509 *issuer, int nid, const char *value)
{
	X509V3_CTX context;
	X509V3_set_ctx(&context, issuer, NULL, NULL, crl, 0);
	X509_EXTENSION *extension = X509V3_EXT_conf_nid(NULL, &context, nid,
		(char *)value);
	if (extension == NULL)
		return 0;
	int result = X509_CRL_add_ext(crl, extension, -1);
	X509_EXTENSION_free(extension);
	return result;
}

static GENERAL_NAME *
make_general_name(int type, const char *value)
{
	GENERAL_NAME *name = GENERAL_NAME_new();
	ASN1_IA5STRING *encoded = ASN1_IA5STRING_new();

	if (name == NULL || encoded == NULL
	    || ASN1_STRING_set(encoded, value, -1) != 1) {
		GENERAL_NAME_free(name);
		ASN1_IA5STRING_free(encoded);
		return NULL;
	}
	GENERAL_NAME_set0_value(name, type, encoded);
	return name;
}

static int
add_subject_alt_names(X509 *certificate, const struct xp_ca_identity *identity)
{
	GENERAL_NAMES *names = sk_GENERAL_NAME_new_null();

	if (names == NULL)
		return 0;
	for (size_t i = 0; i < identity->dns_name_count; i++) {
		GENERAL_NAME *name = make_general_name(GEN_DNS, identity->dns_names[i]);

		if (name == NULL || sk_GENERAL_NAME_push(names, name) == 0) {
			GENERAL_NAME_free(name);
			GENERAL_NAMES_free(names);
			return 0;
		}
	}
	X509_EXTENSION *extension = X509V3_EXT_i2d(
		NID_subject_alt_name, 0, names);
	int result = extension != NULL
		&& X509_add_ext(certificate, extension, -1) == 1;

	X509_EXTENSION_free(extension);
	GENERAL_NAMES_free(names);
	return result;
}

static DIST_POINT_NAME *
make_distribution_point_name(const char *uri)
{
	DIST_POINT_NAME *distribution_point = DIST_POINT_NAME_new();
	GENERAL_NAME *name = make_general_name(GEN_URI, uri);

	if (distribution_point == NULL || name == NULL)
		goto cleanup;
	distribution_point->type = 0;
	distribution_point->name.fullname = sk_GENERAL_NAME_new_null();
	if (distribution_point->name.fullname == NULL
	    || sk_GENERAL_NAME_push(distribution_point->name.fullname, name) == 0)
		goto cleanup;
	name = NULL;
	return distribution_point;

cleanup:
	GENERAL_NAME_free(name);
	DIST_POINT_NAME_free(distribution_point);
	return NULL;
}

static int
add_crl_distribution_point(X509 *certificate, const char *uri)
{
	CRL_DIST_POINTS *points = sk_DIST_POINT_new_null();
	DIST_POINT *point = DIST_POINT_new();
	int result = 0;

	if (points == NULL || point == NULL)
		goto cleanup;
	point->distpoint = make_distribution_point_name(uri);
	if (point->distpoint == NULL)
		goto cleanup;
	if (sk_DIST_POINT_push(points, point) == 0)
		goto cleanup;
	point = NULL;
	X509_EXTENSION *extension = X509V3_EXT_i2d(
		NID_crl_distribution_points, 0, points);
	result = extension != NULL
		&& X509_add_ext(certificate, extension, -1) == 1;
	X509_EXTENSION_free(extension);

cleanup:
	DIST_POINT_free(point);
	CRL_DIST_POINTS_free(points);
	return result;
}

static int
add_issuing_distribution_point(X509_CRL *crl, const char *uri)
{
	ISSUING_DIST_POINT *distribution_point = ISSUING_DIST_POINT_new();

	if (distribution_point == NULL)
		return 0;
	distribution_point->distpoint = make_distribution_point_name(uri);
	int result = distribution_point->distpoint != NULL
		&& X509_CRL_add1_ext_i2d(
			crl, NID_issuing_distribution_point, distribution_point, 1,
			X509V3_ADD_DEFAULT) == 1;
	ISSUING_DIST_POINT_free(distribution_point);
	return result;
}

static bool
distribution_point_name_matches(DIST_POINT_NAME *distribution_point,
	const char *uri)
{
	if (distribution_point == NULL || distribution_point->type != 0
	    || sk_GENERAL_NAME_num(distribution_point->name.fullname) != 1)
		return false;
	GENERAL_NAME *name = sk_GENERAL_NAME_value(
		distribution_point->name.fullname, 0);
	if (name->type != GEN_URI)
		return false;
	ASN1_IA5STRING *value = name->d.uniformResourceIdentifier;
	size_t uri_length = strlen(uri);
	return ASN1_STRING_length(value) == (int)uri_length
		&& memcmp(ASN1_STRING_get0_data(value), uri, uri_length) == 0;
}

static bool
certificate_has_distribution_point(X509 *certificate, const char *uri)
{
	CRL_DIST_POINTS *points = X509_get_ext_d2i(
		certificate, NID_crl_distribution_points, NULL, NULL);
	bool found = false;

	for (int i = 0; points != NULL && i < sk_DIST_POINT_num(points); i++) {
		DIST_POINT *point = sk_DIST_POINT_value(points, i);
		if (distribution_point_name_matches(point->distpoint, uri)) {
			found = true;
			break;
		}
	}
	CRL_DIST_POINTS_free(points);
	return found;
}

static int
apply(X509 * x, X509 * i, const struct xp_ca_issue_request *r)
{
	if (!xp_ca_issue_request_is_valid(r))
		return 0;
	X509_NAME *n = X509_NAME_new();
	if (!n) {
		return 0;
	}
	int        ok = X509_NAME_add_entry_by_NID(n, NID_commonName,
	                                           MBSTRING_UTF8,
	                                           (const unsigned char *)r->subject.common_name,
	                                           -1, -1, 0)
	                && X509_set_subject_name(x, n);
	X509_NAME_free(n);
	if (!ok) {
		return 0;
	}
	if (r->subject.dns_name_count != 0
	    && !add_subject_alt_names(x, &r->subject))
		return 0;
	char        bc[64];
	char        ku[128] = "";
	char        eku[64] = "";
	const char *sep = "";

	if (r->policy.path_length >= 0)
		snprintf(bc, sizeof bc, "%sCA:%s,pathlen:%d",
		         r->policy.basic_constraints_critical ? "critical," : "",
		         r->policy.is_ca ? "TRUE" : "FALSE", r->policy.path_length);
	else
		snprintf(bc, sizeof bc, "%sCA:%s",
		         r->policy.basic_constraints_critical ? "critical," : "",
		         r->policy.is_ca ? "TRUE" : "FALSE");
	if (r->policy.key_usage_critical)
		strcat(ku, "critical,");
	if (r->policy.key_usage & XP_CA_KEY_USE_SIGN) {
		strcat(ku, "digitalSignature");
		sep = ",";
	}
	if (r->policy.key_usage & XP_CA_KEY_USE_CERT_SIGN) {
		strcat(ku, sep);
		strcat(ku, "keyCertSign");
		sep = ",";
	}
	if (r->policy.key_usage & XP_CA_KEY_USE_CRL_SIGN) {
		strcat(ku, sep);
		strcat(ku, "cRLSign");
	}
	if (r->policy.extended_key_usage & XP_CA_EKU_SERVER_AUTH)
		strcat(eku, "serverAuth");
	if (r->policy.extended_key_usage & XP_CA_EKU_CLIENT_AUTH) {
		if (eku[0])
			strcat(eku, ",");
		strcat(eku, "clientAuth");
	}
	if (!ku[0] || !add(x, i, NID_basic_constraints, bc)
	    || !add(x, i, NID_key_usage, ku)
	    || !add(x, i, NID_subject_key_identifier, "hash")
	    || !add(x, i, NID_authority_key_identifier, "keyid:always")
	    || (eku[0] && !add(x, i, NID_ext_key_usage, eku))) {
		return 0;
	}
	if (r->policy.crl_distribution_point != NULL) {
		if (!add_crl_distribution_point(
		        x, r->policy.crl_distribution_point))
			return 0;
	}

	return 1;
}
int
xp_ca_key_generate(xp_ca_key_t * out, enum xp_ca_key_algorithm a)
{
	if (!out)
		return XP_CA_ERR;
	*out = NULL;
	if (a != XP_CA_KEY_ED25519)
		return XP_CA_ERR_POLICY;
	EVP_PKEY_CTX *c = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, 0);
	EVP_PKEY *    p = 0;
	if (!c || EVP_PKEY_keygen_init(c) <= 0 || EVP_PKEY_keygen(c, &p) <= 0) {
		EVP_PKEY_CTX_free(c);
		return fail(XP_CA_ERR, "Ed25519 key generation failed");
	}
	EVP_PKEY_CTX_free(c);
	xp_ca_key_t result = calloc(1, sizeof(*result));
	if (result == NULL) {
		EVP_PKEY_free(p);
		return fail(XP_CA_ERR, "out of memory");
	}
	result->native = p;
	*out = result;
	return XP_CA_OK;
}
int
xp_ca_key_load_pem(xp_ca_key_t * out, const char *path, const char *pass)
{
	if (!out) {
		return XP_CA_ERR;
	}
	*out = NULL;
	if (!path || !pass || pass[0] == '\0') {
		return XP_CA_ERR;
	}
	FILE *    f = fopen(path, "rb");
	EVP_PKEY *p = f ? PEM_read_PrivateKey(f, 0, 0, (void *)pass) : 0;
	if (f) {
		fclose(f);
	}
	if (!p || EVP_PKEY_base_id(p) != EVP_PKEY_ED25519) {
		EVP_PKEY_free(p);
		return fail(XP_CA_ERR, "private key load failed");
	}
	xp_ca_key_t result = calloc(1, sizeof(*result));
	if (result == NULL) {
		EVP_PKEY_free(p);
		return fail(XP_CA_ERR, "out of memory");
	}
	result->native = p;
	*out = result;
	return XP_CA_OK;
}
int
xp_ca_key_save_pem(xp_ca_key_t k, const char *path, const char *pass)
{
	if (!k || !path || !pass || pass[0] == '\0') {
		return XP_CA_ERR;
	}
	char  temporary[4096];
	FILE *f = xp_ca_open_private_temporary(path, temporary, sizeof(temporary));
	int   ok = f && PEM_write_PrivateKey(f, k->native, EVP_aes_256_cbc(),
	                                     (unsigned char *)pass, (int)strlen(pass), NULL, NULL);
	if (ok)
		ok = xp_ca_commit_private_temporary(f, temporary, path) == 0;
	else if (f != NULL)
		xp_ca_discard_private_temporary(f, temporary);
	return ok ? XP_CA_OK : fail(XP_CA_ERR, "private key save failed");
}
int
xp_ca_key_delete_pem(const char *path)
{
	if (path == NULL || remove(path) != 0)
		return fail(XP_CA_ERR, "private key deletion failed");
	return XP_CA_OK;
}
void
xp_ca_key_free(xp_ca_key_t k)
{
	if (k) {
		EVP_PKEY_free(k->native);
		free(k);
	}
}
int
xp_ca_csr_create(xp_ca_csr_t * out, xp_ca_key_t k)
{
	if (!out) {
		return XP_CA_ERR;
	}
	*out = NULL;
	if (!k) {
		return XP_CA_ERR;
	}
	X509_REQ *r = X509_REQ_new();
	if (!r || !X509_REQ_set_pubkey(r, k->native) || !X509_REQ_sign(r, k->native, 0)) {
		X509_REQ_free(r);
		return fail(XP_CA_ERR, "CSR creation failed");
	}
	xp_ca_csr_t result = calloc(1, sizeof(*result));
	if (result == NULL) {
		X509_REQ_free(r);
		return fail(XP_CA_ERR, "out of memory");
	}
	result->native = r;
	*out = result;
	return XP_CA_OK;
}
int
xp_ca_csr_import_der(xp_ca_csr_t * out, const void *d, size_t n)
{
	if (!out)
		return XP_CA_ERR;
	*out = NULL;
	if (!d || n == 0)
		return XP_CA_ERR;
	const unsigned char *p = d;
	X509_REQ *r = d2i_X509_REQ(NULL, &p, n);
	if (!r || p != (const unsigned char *)d + n) {
		X509_REQ_free(r);
		return fail(XP_CA_ERR_FORMAT, "invalid DER CSR");
	}
	xp_ca_csr_t result = calloc(1, sizeof(*result));
	if (result == NULL) {
		X509_REQ_free(r);
		return fail(XP_CA_ERR, "out of memory");
	}
	result->native = r;
	*out = result;
	return XP_CA_OK;
}
int
xp_ca_csr_export_der(xp_ca_csr_t c, void *out, size_t * n)
{
	if (!c) {
		return XP_CA_ERR;
	}
	int z = i2d_X509_REQ(c->native, NULL);
	if (z <= 0)
		return fail(XP_CA_ERR, "CSR DER encoding failed");
	unsigned char *p = malloc((size_t)z);
	if (p == NULL)
		return fail(XP_CA_ERR, "out of memory");
	unsigned char *q = p;
	if (i2d_X509_REQ(c->native, &q) != z) {
		free(p);
		return fail(XP_CA_ERR, "CSR DER encoding failed");
	}
	int            r = der(z, p, out, n);
	free(p);
	return r;
}
int
xp_ca_csr_verify(xp_ca_csr_t c)
{
	EVP_PKEY *p = c ? X509_REQ_get_pubkey(c->native) : 0;
	int r = p && EVP_PKEY_base_id(p) == EVP_PKEY_ED25519
		&& X509_REQ_verify(c->native, p) == 1 ? XP_CA_OK : XP_CA_ERR_VERIFY;
	EVP_PKEY_free(p);
	return r;
}
void
xp_ca_csr_free(xp_ca_csr_t c)
{
	if (c) {
		X509_REQ_free(c->native);
		free(c);
	}
}
static int
issue(
	xp_ca_cert_t *out,
	xp_ca_key_t key,
	xp_ca_cert_t issuer,
	xp_ca_csr_t csr,
	const struct xp_ca_issue_request *request)
{
	if (!out)
		return XP_CA_ERR;
	*out = NULL;
	if (!key)
		return XP_CA_ERR;
	if (!xp_ca_issue_request_is_valid(request))
		return XP_CA_ERR_POLICY;
	if (csr && xp_ca_csr_verify(csr) != XP_CA_OK)
		return XP_CA_ERR_VERIFY;
	if (issuer != NULL) {
		BASIC_CONSTRAINTS *constraints = X509_get_ext_d2i(issuer->native,
		                                                  NID_basic_constraints, NULL, NULL);
		ASN1_BIT_STRING *  usage = X509_get_ext_d2i(issuer->native,
		                                            NID_key_usage, NULL, NULL);
		int                can_sign = constraints != NULL && constraints->ca != 0 && usage != NULL
		                              && ASN1_BIT_STRING_get_bit(usage, 5)
		                              && X509_check_private_key(issuer->native, key->native) == 1;
		BASIC_CONSTRAINTS_free(constraints);
		ASN1_BIT_STRING_free(usage);
		if (!can_sign)
			return XP_CA_ERR_POLICY;
		ASN1_TIME *requested_start = ASN1_TIME_set(NULL, request->not_before);
		ASN1_TIME *requested_expiry = ASN1_TIME_set(NULL, request->not_after);
		if (requested_start == NULL || requested_expiry == NULL
		    || ASN1_TIME_compare(requested_start,
			X509_get0_notBefore(issuer->native)) < 0
		    || ASN1_TIME_compare(requested_expiry,
		                         X509_get0_notAfter(issuer->native)) > 0) {
			ASN1_TIME_free(requested_start);
			ASN1_TIME_free(requested_expiry);
			return XP_CA_ERR_POLICY;
		}
		ASN1_TIME_free(requested_start);
		ASN1_TIME_free(requested_expiry);
	}
	X509 *        x = X509_new();
	EVP_PKEY *    p = csr ? X509_REQ_get_pubkey(csr->native) : key->native;
	unsigned char s[20];
	int random_ok = RAND_bytes(s, sizeof s);
	if (random_ok == 1) {
		s[0] &= 0x7f;
		bool all_zero = true;
		for (size_t i = 0; i < sizeof(s); i++)
			all_zero &= s[i] == 0;
		if (all_zero)
			s[sizeof(s) - 1] = 1;
	}
	if (!x || !p || random_ok != 1
	    || !X509_set_version(x, 2)
	    || !ASN1_STRING_set(X509_get_serialNumber(x), s, sizeof s)
	    || !ASN1_TIME_set(X509_getm_notBefore(x), request->not_before)
	    || !ASN1_TIME_set(X509_getm_notAfter(x), request->not_after)
	    || !X509_set_pubkey(x, p)
	    || !apply(x, issuer ? issuer->native : x, request)
	    || !X509_set_issuer_name(x, issuer ? X509_get_subject_name(issuer->native)
	        : X509_get_subject_name(x))
	    || !X509_sign(x, key->native, 0)) {
		if (csr) {
			EVP_PKEY_free(p);
		}
		X509_free(x);
		return fail(XP_CA_ERR, "certificate issuance failed");
	}
	if (csr) {
		EVP_PKEY_free(p);
	}
	xp_ca_cert_t result = calloc(1, sizeof(*result));
	if (result == NULL) {
		X509_free(x);
		return fail(XP_CA_ERR, "out of memory");
	}
	result->native = x;
	*out = result;
	return XP_CA_OK;
}
int
xp_ca_cert_create_self_signed(
	xp_ca_cert_t *out,
	xp_ca_key_t key,
	const struct xp_ca_issue_request *request)
{
	return issue(out, key, NULL, NULL, request);
}
int
xp_ca_cert_issue(
	xp_ca_cert_t *out,
	xp_ca_key_t issuer_key,
	xp_ca_cert_t issuer,
	xp_ca_csr_t csr,
	const struct xp_ca_issue_request *request)
{
	if (out == NULL)
		return XP_CA_ERR;
	*out = NULL;
	return issuer == NULL
		? XP_CA_ERR
		: issue(out, issuer_key, issuer, csr, request);
}
int
xp_ca_cert_import_der(xp_ca_cert_t * out, const void *d, size_t n)
{
	if (!out)
		return XP_CA_ERR;
	*out = NULL;
	if (!d || n == 0)
		return XP_CA_ERR;
	const unsigned char *p = d;
	X509 *x = d2i_X509(NULL, &p, n);
	if (!x || p != (const unsigned char *)d + n) {
		X509_free(x);
		return XP_CA_ERR_FORMAT;
	}
	xp_ca_cert_t result = calloc(1, sizeof(*result));
	if (result == NULL) {
		X509_free(x);
		return fail(XP_CA_ERR, "out of memory");
	}
	result->native = x;
	*out = result;
	return XP_CA_OK;
}
int
xp_ca_cert_export_der(xp_ca_cert_t c, void *out, size_t * n)
{
	if (!c) {
		return XP_CA_ERR;
	}
	int z = i2d_X509(c->native, NULL);
	if (z <= 0)
		return fail(XP_CA_ERR, "certificate DER encoding failed");
	unsigned char *p = malloc((size_t)z);
	if (p == NULL)
		return fail(XP_CA_ERR, "out of memory");
	unsigned char *q = p;
	if (i2d_X509(c->native, &q) != z) {
		free(p);
		return fail(XP_CA_ERR, "certificate DER encoding failed");
	}
	int            r = der(z, p, out, n);
	free(p);
	return r;
}
void
xp_ca_cert_free(xp_ca_cert_t c)
{
	if (c) {
		X509_free(c->native);
		free(c);
	}
}

static bool
crl_matches_issuer(
	X509_CRL *crl,
	X509 *issuer,
	EVP_PKEY *issuer_public_key,
	const char *issuing_distribution_point)
{
	AUTHORITY_KEYID *authority_key_id = X509_CRL_get_ext_d2i(
		crl, NID_authority_key_identifier, NULL, NULL);
	ASN1_OCTET_STRING *subject_key_id = X509_get_ext_d2i(
		issuer, NID_subject_key_identifier, NULL, NULL);
	ISSUING_DIST_POINT *scope = X509_CRL_get_ext_d2i(
		crl, NID_issuing_distribution_point, NULL, NULL);
	int scope_index = X509_CRL_get_ext_by_NID(
		crl, NID_issuing_distribution_point, -1);
	int duplicate_scope_index = scope_index < 0 ? -1
		: X509_CRL_get_ext_by_NID(
			crl, NID_issuing_distribution_point, scope_index);
	bool scope_is_critical = scope_index >= 0
		&& X509_EXTENSION_get_critical(X509_CRL_get_ext(crl, scope_index));
	bool matches = authority_key_id != NULL
		&& authority_key_id->keyid != NULL
		&& subject_key_id != NULL
		&& scope != NULL
		&& duplicate_scope_index < 0
		&& scope_is_critical
		&& distribution_point_name_matches(
			scope->distpoint, issuing_distribution_point)
		&& ASN1_OCTET_STRING_cmp(authority_key_id->keyid, subject_key_id) == 0
		&& X509_NAME_cmp(X509_CRL_get_issuer(crl),
		                 X509_get_subject_name(issuer)) == 0
		&& X509_CRL_verify(crl, issuer_public_key) == 1;

	AUTHORITY_KEYID_free(authority_key_id);
	ASN1_OCTET_STRING_free(subject_key_id);
	ISSUING_DIST_POINT_free(scope);
	return matches;
}

int
xp_ca_crl_create(xp_ca_crl_t *out, xp_ca_key_t key, xp_ca_cert_t issuer,
	             xp_ca_crl_t previous,
	             const struct xp_ca_crl_request *request)
{
	if (!out) {
		return XP_CA_ERR;
	}
	*out = NULL;
	if (!key || !issuer)
		return XP_CA_ERR;
	if (!xp_ca_crl_request_is_valid(request))
		return XP_CA_ERR_POLICY;
	BASIC_CONSTRAINTS *constraints = X509_get_ext_d2i(issuer->native,
	                                                  NID_basic_constraints, NULL, NULL);
	ASN1_BIT_STRING *  usage = X509_get_ext_d2i(issuer->native,
	                                            NID_key_usage, NULL, NULL);
	int                can_sign = constraints != NULL && constraints->ca != 0 && usage != NULL
	                              && ASN1_BIT_STRING_get_bit(usage, 6)
	                              && X509_check_private_key(issuer->native, key->native) == 1;
	BASIC_CONSTRAINTS_free(constraints);
	ASN1_BIT_STRING_free(usage);
	if (!can_sign)
		return XP_CA_ERR_POLICY;
	EVP_PKEY *issuer_public_key = X509_get_pubkey(issuer->native);
	if (issuer_public_key == NULL)
		return XP_CA_ERR;
	if (previous != NULL) {
		bool valid_previous = crl_matches_issuer(
			previous->native, issuer->native, issuer_public_key,
			request->issuing_distribution_point);
		if (!valid_previous) {
			EVP_PKEY_free(issuer_public_key);
			return XP_CA_ERR_VERIFY;
		}
	}
	for (size_t i = 0; i < request->newly_revoked_count; i++) {
		if (request->newly_revoked[i] == NULL
		    || X509_NAME_cmp(
			X509_get_issuer_name(request->newly_revoked[i]->native),
		                     X509_get_subject_name(issuer->native)) != 0
		    || X509_verify(request->newly_revoked[i]->native,
		                   issuer_public_key) != 1
		    || !certificate_has_distribution_point(
			request->newly_revoked[i]->native,
			request->issuing_distribution_point)) {
			EVP_PKEY_free(issuer_public_key);
			return XP_CA_ERR_VERIFY;
		}
	}
	EVP_PKEY_free(issuer_public_key);
	X509_CRL * c = X509_CRL_new();
	ASN1_TIME *a = ASN1_TIME_set(0, request->this_update);
	ASN1_TIME *b = ASN1_TIME_set(0, request->next_update);
	if (!c || !a || !b || !X509_CRL_set_version(c, 1)
	    || !X509_CRL_set_issuer_name(c, X509_get_subject_name(issuer->native))
	    || !X509_CRL_set1_lastUpdate(c, a) || !X509_CRL_set1_nextUpdate(c, b)) {
		ASN1_TIME_free(a);
		ASN1_TIME_free(b);
		X509_CRL_free(c);
		return XP_CA_ERR;
	}
	ASN1_TIME_free(a);
	ASN1_TIME_free(b);
	ASN1_INTEGER *number = previous == NULL ? ASN1_INTEGER_new()
		: X509_CRL_get_ext_d2i(previous->native, NID_crl_number, NULL, NULL);
	uint64_t next_number = 1;
	if (number == NULL
	    || (previous != NULL && !ASN1_INTEGER_get_uint64(&next_number, number))) {
		ASN1_INTEGER_free(number);
		X509_CRL_free(c);
		return XP_CA_ERR_FORMAT;
	}
	if (previous != NULL && next_number == UINT64_MAX) {
		ASN1_INTEGER_free(number);
		X509_CRL_free(c);
		return XP_CA_ERR_POLICY;
	}
	if (previous != NULL)
		next_number++;
	if (!ASN1_INTEGER_set_uint64(number, next_number)
	    || !X509_CRL_add1_ext_i2d(c, NID_crl_number, number, 0,
		X509V3_ADD_DEFAULT)
	    || !add_crl_extension(c, issuer->native,
		NID_authority_key_identifier, "keyid:always")
	    || !add_issuing_distribution_point(
		c, request->issuing_distribution_point)) {
		ASN1_INTEGER_free(number);
		X509_CRL_free(c);
		return XP_CA_ERR;
	}
	ASN1_INTEGER_free(number);
	if (previous != NULL) {
		const STACK_OF(X509_REVOKED) *entries = X509_CRL_get_REVOKED(previous->native);
		for (int i = 0; i < sk_X509_REVOKED_num(entries); i++) {
			X509_REVOKED *entry = X509_REVOKED_dup(
				sk_X509_REVOKED_value(entries, i));
			if (entry == NULL || !X509_CRL_add0_revoked(c, entry)) {
				X509_REVOKED_free(entry);
				X509_CRL_free(c);
				return XP_CA_ERR;
			}
		}
	}
	for (size_t i = 0; i < request->newly_revoked_count; i++) {
		X509_REVOKED *v = X509_REVOKED_new();
		ASN1_TIME *   revocation_time = ASN1_TIME_set(
			NULL, request->this_update);
		if (!v || !revocation_time
		    || !X509_REVOKED_set_serialNumber(v,
			X509_get_serialNumber(request->newly_revoked[i]->native))
		    || !X509_REVOKED_set_revocationDate(v, revocation_time)
		    || !X509_CRL_add0_revoked(c, v)) {
			ASN1_TIME_free(revocation_time);
			X509_REVOKED_free(v);
			X509_CRL_free(c);
			return XP_CA_ERR;
		}
		ASN1_TIME_free(revocation_time);
	}
	if (!X509_CRL_sort(c) || !X509_CRL_sign(c, key->native, 0)) {
		X509_CRL_free(c);
		return XP_CA_ERR;
	}
	xp_ca_crl_t result = calloc(1, sizeof(*result));
	if (result == NULL) {
		X509_CRL_free(c);
		return fail(XP_CA_ERR, "out of memory");
	}
	result->native = c;
	*out = result;
	return XP_CA_OK;
}
int
xp_ca_crl_import_der(xp_ca_crl_t * out, const void *d, size_t n)
{
	if (!out)
		return XP_CA_ERR;
	*out = NULL;
	if (!d || n == 0)
		return XP_CA_ERR;
	const unsigned char *p = d;
	X509_CRL *c = d2i_X509_CRL(NULL, &p, n);
	if (!c || p != (const unsigned char *)d + n) {
		X509_CRL_free(c);
		return XP_CA_ERR_FORMAT;
	}
	xp_ca_crl_t result = calloc(1, sizeof(*result));
	if (result == NULL) {
		X509_CRL_free(c);
		return fail(XP_CA_ERR, "out of memory");
	}
	result->native = c;
	*out = result;
	return XP_CA_OK;
}
int
xp_ca_crl_export_der(xp_ca_crl_t c, void *out, size_t * n)
{
	if (!c) {
		return XP_CA_ERR;
	}
	int z = i2d_X509_CRL(c->native, NULL);
	if (z <= 0)
		return fail(XP_CA_ERR, "CRL DER encoding failed");
	unsigned char *p = malloc((size_t)z);
	if (p == NULL)
		return fail(XP_CA_ERR, "out of memory");
	unsigned char *q = p;
	if (i2d_X509_CRL(c->native, &q) != z) {
		free(p);
		return fail(XP_CA_ERR, "CRL DER encoding failed");
	}
	int            r = der(z, p, out, n);
	free(p);
	return r;
}

int
xp_ca_crl_get_info(xp_ca_crl_t crl, struct xp_ca_crl_info *info)
{
	if (crl == NULL || info == NULL)
		return XP_CA_ERR;
	ASN1_INTEGER *number = X509_CRL_get_ext_d2i(crl->native,
		NID_crl_number, NULL, NULL);
	struct tm this_update = { 0 };
	struct tm next_update = { 0 };
	if (number == NULL
	    || !ASN1_INTEGER_get_uint64(&info->number, number)
	    || !ASN1_TIME_to_tm(X509_CRL_get0_lastUpdate(crl->native), &this_update)
	    || X509_CRL_get0_nextUpdate(crl->native) == NULL
	    || !ASN1_TIME_to_tm(X509_CRL_get0_nextUpdate(crl->native), &next_update)) {
		ASN1_INTEGER_free(number);
		return XP_CA_ERR_FORMAT;
	}
	ASN1_INTEGER_free(number);
	info->this_update = timegm(&this_update);
	info->next_update = timegm(&next_update);
	return XP_CA_OK;
}
void
xp_ca_crl_free(xp_ca_crl_t c)
{
	if (c) {
		X509_CRL_free(c->native);
		free(c);
	}
}
static bool
certificate_matches_identity(X509 *cert, const struct xp_ca_identity *identity)
{
	if (identity == NULL
	    || (identity->dns_name_count != 0 && identity->dns_names == NULL))
		return false;
	GENERAL_NAMES *names = X509_get_ext_d2i(cert, NID_subject_alt_name,
		NULL, NULL);
	int extension_index = X509_get_ext_by_NID(cert, NID_subject_alt_name, -1);
	int duplicate_index = extension_index < 0 ? -1
		: X509_get_ext_by_NID(cert, NID_subject_alt_name, extension_index);
	int name_count = names == NULL ? 0 : sk_GENERAL_NAME_num(names);
	if (duplicate_index >= 0
	    || (identity->dns_name_count == 0 && extension_index >= 0)
	    || (identity->dns_name_count != 0 && extension_index < 0)
	    || name_count != (int)identity->dns_name_count) {
		GENERAL_NAMES_free(names);
		return false;
	}
	bool *matched = name_count == 0 ? NULL
		: calloc((size_t)name_count, sizeof(*matched));
	if (name_count != 0 && matched == NULL) {
		GENERAL_NAMES_free(names);
		return false;
	}
	for (size_t expected = 0; expected < identity->dns_name_count; expected++) {
		bool found = false;
		if (identity->dns_names[expected] == NULL) {
			free(matched);
			GENERAL_NAMES_free(names);
			return false;
		}
		for (size_t earlier = 0; earlier < expected; earlier++) {
			if (xp_ca_dns_names_equal(identity->dns_names[earlier],
			                          identity->dns_names[expected])) {
				free(matched);
				GENERAL_NAMES_free(names);
				return false;
			}
		}
		for (int actual = 0; actual < name_count; actual++) {
			if (matched[actual])
				continue;
			GENERAL_NAME *name = sk_GENERAL_NAME_value(names, actual);
			if (name->type != GEN_DNS)
				continue;
			const unsigned char *value = ASN1_STRING_get0_data(name->d.dNSName);
			int length = ASN1_STRING_length(name->d.dNSName);
			if ((int)strlen(identity->dns_names[expected]) == length
			    && OPENSSL_strncasecmp((const char *)value,
				identity->dns_names[expected], (size_t)length) == 0) {
				matched[actual] = true;
				found = true;
				break;
			}
		}
		if (!found) {
			free(matched);
			GENERAL_NAMES_free(names);
			return false;
		}
	}
	free(matched);
	GENERAL_NAMES_free(names);
	if (identity->common_name != NULL) {
		X509_NAME *subject = X509_get_subject_name(cert);
		int first = X509_NAME_get_index_by_NID(subject, NID_commonName, -1);
		int second = first < 0 ? -1
			: X509_NAME_get_index_by_NID(subject, NID_commonName, first);
		if (first < 0 || second >= 0)
			return false;
		X509_NAME_ENTRY *entry = X509_NAME_get_entry(subject, first);
		ASN1_STRING *value = X509_NAME_ENTRY_get_data(entry);
		unsigned char *utf8 = NULL;
		int length = ASN1_STRING_to_UTF8(&utf8, value);
		bool matches = length >= 0
			&& strlen(identity->common_name) == (size_t)length
			&& memcmp(utf8, identity->common_name, (size_t)length) == 0;
		OPENSSL_free(utf8);
		if (!matches)
			return false;
	}
	return true;
}

static bool
certificate_matches_policy(X509 *cert, const struct xp_ca_validation_policy *policy)
{
	if (policy == NULL)
		return true;
	if (policy->expected_identity != NULL
	    && !certificate_matches_identity(cert, policy->expected_identity))
		return false;
	if (policy->required_policy != NULL) {
		const struct xp_ca_certificate_policy *required = policy->required_policy;
		BASIC_CONSTRAINTS *                    constraints = X509_get_ext_d2i(cert,
			NID_basic_constraints, NULL, NULL);
		bool                                   is_ca = constraints != NULL && constraints->ca != 0;
		bool path_length_matches = required->path_length < 0
			? constraints != NULL && constraints->pathlen == NULL
			: constraints != NULL && constraints->pathlen != NULL
				&& ASN1_INTEGER_get(constraints->pathlen) == required->path_length;
		BASIC_CONSTRAINTS_free(constraints);
		if (is_ca != required->is_ca || !path_length_matches)
			return false;
		int basic_index = X509_get_ext_by_NID(cert, NID_basic_constraints, -1);
		int usage_index = X509_get_ext_by_NID(cert, NID_key_usage, -1);
		int duplicate_basic = basic_index < 0 ? -1
			: X509_get_ext_by_NID(cert, NID_basic_constraints, basic_index);
		int duplicate_usage = usage_index < 0 ? -1
			: X509_get_ext_by_NID(cert, NID_key_usage, usage_index);
		bool basic_critical = basic_index >= 0
			&& X509_EXTENSION_get_critical(X509_get_ext(cert, basic_index));
		bool usage_critical = usage_index >= 0
			&& X509_EXTENSION_get_critical(X509_get_ext(cert, usage_index));
		if (basic_index < 0 || usage_index < 0
		    || duplicate_basic >= 0 || duplicate_usage >= 0
		    || basic_critical != required->basic_constraints_critical
		    || usage_critical != required->key_usage_critical)
			return false;
		ASN1_BIT_STRING *usage = X509_get_ext_d2i(cert, NID_key_usage, NULL, NULL);
		bool             has_sign = usage != NULL && ASN1_BIT_STRING_get_bit(usage, 0);
		bool             has_cert_sign = usage != NULL && ASN1_BIT_STRING_get_bit(usage, 5);
		bool             has_crl_sign = usage != NULL && ASN1_BIT_STRING_get_bit(usage, 6);
		bool             has_other_usage = false;
		for (int bit = 1; usage != NULL && bit <= 8; bit++) {
			if (bit != 5 && bit != 6 && ASN1_BIT_STRING_get_bit(usage, bit))
				has_other_usage = true;
		}
		ASN1_BIT_STRING_free(usage);
		unsigned actual_usage = (has_sign ? XP_CA_KEY_USE_SIGN : 0)
			| (has_cert_sign ? XP_CA_KEY_USE_CERT_SIGN : 0)
			| (has_crl_sign ? XP_CA_KEY_USE_CRL_SIGN : 0);
		if (has_other_usage || actual_usage != required->key_usage)
			return false;
		int eku_index = X509_get_ext_by_NID(cert, NID_ext_key_usage, -1);
		int duplicate_eku = eku_index < 0 ? -1
			: X509_get_ext_by_NID(cert, NID_ext_key_usage, eku_index);
		EXTENDED_KEY_USAGE *eku = X509_get_ext_d2i(cert, NID_ext_key_usage,
		                                           NULL, NULL);
		bool                has_server_auth = false;
		bool                has_client_auth = false;
		for (int i = 0; eku != NULL && i < sk_ASN1_OBJECT_num(eku); i++) {
			ASN1_OBJECT *object = sk_ASN1_OBJECT_value(eku, i);
			has_server_auth |= OBJ_obj2nid(object) == NID_server_auth;
			has_client_auth |= OBJ_obj2nid(object) == NID_client_auth;
		}
		bool eku_decoded = eku != NULL;
		int actual_eku_count = eku == NULL ? 0 : sk_ASN1_OBJECT_num(eku);
		EXTENDED_KEY_USAGE_free(eku);
		unsigned actual_eku = (has_server_auth ? XP_CA_EKU_SERVER_AUTH : 0)
			| (has_client_auth ? XP_CA_EKU_CLIENT_AUTH : 0);
		unsigned expected_eku_count = has_server_auth + has_client_auth;
		if (duplicate_eku >= 0
		    || (required->extended_key_usage == XP_CA_EKU_NONE
		        && eku_index >= 0)
		    || (required->extended_key_usage != XP_CA_EKU_NONE
		        && (eku_index < 0 || !eku_decoded))
		    || actual_eku != required->extended_key_usage
		    || actual_eku_count != (int)expected_eku_count)
			return false;
		int distribution_point_index = X509_get_ext_by_NID(
			cert, NID_crl_distribution_points, -1);
		int duplicate_distribution_point = distribution_point_index < 0 ? -1
			: X509_get_ext_by_NID(cert, NID_crl_distribution_points,
			                      distribution_point_index);
		CRL_DIST_POINTS *distribution_points = X509_get_ext_d2i(cert,
			NID_crl_distribution_points, NULL, NULL);
		const char *actual_distribution_point = NULL;
		if (distribution_points != NULL
		    && sk_DIST_POINT_num(distribution_points) == 1) {
			DIST_POINT *point = sk_DIST_POINT_value(distribution_points, 0);
			if (point->distpoint != NULL && point->distpoint->type == 0
			    && sk_GENERAL_NAME_num(point->distpoint->name.fullname) == 1) {
				GENERAL_NAME *name = sk_GENERAL_NAME_value(
					point->distpoint->name.fullname, 0);
				if (name->type == GEN_URI)
					actual_distribution_point = (const char *)
						ASN1_STRING_get0_data(name->d.uniformResourceIdentifier);
			}
		}
		bool distribution_point_matches = duplicate_distribution_point < 0
			&& (required->crl_distribution_point == NULL
				? distribution_point_index < 0
				: distribution_point_index >= 0
					&& actual_distribution_point != NULL
				&& ASN1_STRING_length(sk_GENERAL_NAME_value(
					sk_DIST_POINT_value(distribution_points, 0)->distpoint->name.fullname,
					0)->d.uniformResourceIdentifier)
					== (int)strlen(required->crl_distribution_point)
				&& memcmp(actual_distribution_point,
					required->crl_distribution_point,
					strlen(required->crl_distribution_point)) == 0);
		CRL_DIST_POINTS_free(distribution_points);
		if (!distribution_point_matches)
			return false;
	}
	return true;
}
int
xp_ca_cert_validate(xp_ca_cert_t leaf, const xp_ca_cert_t *chain, size_t count,
                    xp_ca_cert_t root, const xp_ca_crl_t *crls, size_t n,
                    const struct xp_ca_validation_policy *policy)
{
	if (!leaf || !root || (count != 0 && chain == NULL)
	    || (n != 0 && crls == NULL)) {
		return XP_CA_ERR;
	}
	X509_STORE *s = X509_STORE_new();
	STACK_OF(X509) *u = sk_X509_new_null();
	X509_STORE_CTX *x = X509_STORE_CTX_new();
	if (s == NULL || u == NULL || x == NULL) {
		X509_STORE_CTX_free(x);
		sk_X509_free(u);
		X509_STORE_free(s);
		return fail(XP_CA_ERR, "validation allocation failed");
	}
	int ok = X509_STORE_add_cert(s, root->native) == 1;
	for (size_t i = 0; i < count; i++) {
		if (chain[i] == NULL || !sk_X509_push(u, chain[i]->native))
			ok = 0;
	}
	for (size_t i = 0; i < n; i++) {
		if (crls[i] == NULL || X509_STORE_add_crl(s, crls[i]->native) != 1)
			ok = 0;
	}
	if (policy && policy->require_crl) {
		if (X509_STORE_set_flags(s,
			X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL) != 1)
			ok = 0;
	}
	if (ok)
		ok = X509_STORE_CTX_init(x, s, leaf->native, u);
	if (ok && policy != NULL && policy->validation_time != 0)
		X509_VERIFY_PARAM_set_time(X509_STORE_CTX_get0_param(x),
		                           policy->validation_time);
	if (ok)
		ok = X509_verify_cert(x) == 1;
	X509_STORE_CTX_free(x);
	sk_X509_free(u);
	X509_STORE_free(s);
	return ok && certificate_matches_policy(leaf->native, policy)
	    ? XP_CA_OK : XP_CA_ERR_VERIFY;
}
