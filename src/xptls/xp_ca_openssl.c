#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef _MSC_VER
/* MSVC leaves this defined even with /experimental:c11atomics enabled. */
#undef __STDC_NO_ATOMICS__
#endif
#include <stdatomic.h>
#include "datewrap.h"
#include "xp_ca.h"
#include "xp_ca_file.h"
#include "xp_key_internal.h"
#include "xp_ca_policy.h"
#include "xp_sign.h"
#include <openssl/core_names.h>
#include <openssl/buffer.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509v3.h>

struct xp_key {
	atomic_size_t references;
	EVP_PKEY *native;
	bool exportable;
	enum xp_key_store_kind storage;
	unsigned char *reference;
	size_t reference_len;
	char err[256];
};

static xp_key_t
key_new(EVP_PKEY *native)
{
	xp_key_t key = calloc(1, sizeof(*key));
	if (key != NULL) {
		atomic_init(&key->references, 1);
		key->native = native;
		key->exportable = true;
		key->storage = XP_KEY_STORE_MEMORY;
	}
	return key;
}
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

int
xp_ca_cert_tls_server_usable(xp_ca_cert_t certificate)
{
	if (certificate == NULL || certificate->native == NULL)
		return XP_CRYPTO_ERR_INVALID;
	return X509_check_ca(certificate->native) == 0 &&
	    X509_check_purpose(certificate->native, X509_PURPOSE_SSL_SERVER, 0) == 1
	    ? XP_CRYPTO_OK : XP_CRYPTO_ERR_POLICY;
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

static const EVP_MD *
key_md(EVP_PKEY *key)
{
	int type = EVP_PKEY_base_id(key);
	if (type == EVP_PKEY_ED25519)
		return NULL;
	if (type == EVP_PKEY_EC) {
		int bits = EVP_PKEY_get_bits(key);
		return bits <= 256 ? EVP_sha256() : bits <= 384 ? EVP_sha384() : EVP_sha512();
	}
	return EVP_sha256();
}

static bool
supported_key(EVP_PKEY *key)
{
	int type = key == NULL ? EVP_PKEY_NONE : EVP_PKEY_base_id(key);
	return type == EVP_PKEY_ED25519 || type == EVP_PKEY_RSA || type == EVP_PKEY_EC;
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

static int add_identity_name(
	X509_NAME *name, const struct xp_ca_identity *identity);

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
	int        ok = add_identity_name(n, &r->subject)
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
		sep = ",";
	}
	if (r->policy.key_usage & XP_CA_KEY_USE_KEY_ENCIPHERMENT) {
		strcat(ku, sep); strcat(ku, "keyEncipherment"); sep = ",";
	}
	if (r->policy.key_usage & XP_CA_KEY_USE_DATA_ENCIPHERMENT) {
		strcat(ku, sep); strcat(ku, "dataEncipherment"); sep = ",";
	}
	if (r->policy.key_usage & XP_CA_KEY_USE_KEY_AGREEMENT) {
		strcat(ku, sep); strcat(ku, "keyAgreement"); sep = ",";
	}
	if (r->policy.key_usage & XP_CA_KEY_USE_NON_REPUDIATION) {
		strcat(ku, sep); strcat(ku, "nonRepudiation"); sep = ",";
	}
	if (r->policy.key_usage & XP_CA_KEY_USE_ENCIPHER_ONLY) {
		strcat(ku, sep); strcat(ku, "encipherOnly"); sep = ",";
	}
	if (r->policy.key_usage & XP_CA_KEY_USE_DECIPHER_ONLY) {
		strcat(ku, sep); strcat(ku, "decipherOnly");
	}
	if (r->policy.extended_key_usage & XP_CA_EKU_SERVER_AUTH)
		strcat(eku, "serverAuth");
	if (r->policy.extended_key_usage & XP_CA_EKU_CLIENT_AUTH) {
		if (eku[0])
			strcat(eku, ",");
		strcat(eku, "clientAuth");
	}
	if (r->policy.extended_key_usage & XP_CA_EKU_CODE_SIGNING)
		strcat(eku, eku[0] ? ",codeSigning" : "codeSigning");
	if (r->policy.extended_key_usage & XP_CA_EKU_EMAIL_PROTECTION)
		strcat(eku, eku[0] ? ",emailProtection" : "emailProtection");
	if (r->policy.extended_key_usage & XP_CA_EKU_TIME_STAMPING)
		strcat(eku, eku[0] ? ",timeStamping" : "timeStamping");
	if (r->policy.extended_key_usage & XP_CA_EKU_OCSP_SIGNING)
		strcat(eku, eku[0] ? ",OCSPSigning" : "OCSPSigning");
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
xp_key_generate(xp_key_t *out, const struct xp_key_spec *spec)
{
	if (out == NULL)
		return XP_CA_ERR;
	*out = NULL;
	if (spec == NULL)
		return XP_CRYPTO_ERR_INVALID;
	int id = EVP_PKEY_NONE;
	int bits = 0;
	int curve = NID_undef;
	switch (spec->algorithm) {
		case XP_KEY_ED25519:
			if (spec->bits != 0 || spec->curve != XP_KEY_CURVE_NONE)
				return XP_CA_ERR_POLICY;
			id = EVP_PKEY_ED25519;
			break;
		case XP_KEY_RSA:
			if ((spec->bits != 2048 && spec->bits != 3072 && spec->bits != 4096)
			    || spec->curve != XP_KEY_CURVE_NONE)
				return XP_CA_ERR_POLICY;
			id = EVP_PKEY_RSA;
			bits = (int)spec->bits;
			break;
		case XP_KEY_ECDSA:
			if (spec->bits != 0)
				return XP_CA_ERR_POLICY;
			id = EVP_PKEY_EC;
			curve = spec->curve == XP_KEY_CURVE_P256 ? NID_X9_62_prime256v1
				: spec->curve == XP_KEY_CURVE_P384 ? NID_secp384r1
				: spec->curve == XP_KEY_CURVE_P521 ? NID_secp521r1 : NID_undef;
			if (curve == NID_undef)
				return XP_CA_ERR_POLICY;
			break;
		default:
			return XP_CA_ERR_POLICY;
	}
	EVP_PKEY_CTX *c = EVP_PKEY_CTX_new_id(id, 0);
	EVP_PKEY *    p = 0;
	int ok = c != NULL && EVP_PKEY_keygen_init(c) > 0;
	if (ok && bits != 0)
		ok = EVP_PKEY_CTX_set_rsa_keygen_bits(c, bits) > 0;
	if (ok && curve != NID_undef)
		ok = EVP_PKEY_CTX_set_ec_paramgen_curve_nid(c, curve) > 0;
	if (!ok || EVP_PKEY_keygen(c, &p) <= 0) {
		EVP_PKEY_CTX_free(c);
		return fail(XP_CA_ERR, "key generation failed");
	}
	EVP_PKEY_CTX_free(c);
	xp_key_t result = key_new(p);
	if (result == NULL) {
		EVP_PKEY_free(p);
		return fail(XP_CA_ERR, "out of memory");
	}
	*out = result;
	return XP_CA_OK;
}
int
xp_key_load_private_pem_file(xp_key_t * out, const char *path, const char *pass)
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
	if (!supported_key(p)) {
		EVP_PKEY_free(p);
		return fail(XP_CA_ERR, "private key load failed");
	}
	xp_key_t result = key_new(p);
	if (result == NULL) {
		EVP_PKEY_free(p);
		return fail(XP_CA_ERR, "out of memory");
	}
	*out = result;
	return XP_CA_OK;
}

static int
password_value(xp_ca_password_callback_t callback, void *context,
               unsigned char **out, size_t *len)
{
	*out = NULL;
	*len = 0;
	if (callback == NULL)
		return XP_CA_OK;
	if (callback(context, NULL, 0, len) != 0 || *len == 0 || *len > INT_MAX)
		return XP_CA_ERR;
	*out = malloc(*len + 1);
	if (*out == NULL)
		return XP_CA_ERR;
	size_t actual = *len;
	if (callback(context, *out, *len, &actual) != 0 || actual != *len) {
		xp_ca_scrub_memory(*out, *len + 1);
		free(*out);
		*out = NULL;
		return XP_CA_ERR;
	}
	(*out)[*len] = 0;
	return XP_CA_OK;
}

int
xp_key_get_info(xp_key_t key, struct xp_key_info *info)
{
	if (key == NULL || info == NULL || !supported_key(key->native))
		return XP_CA_ERR;
	memset(info, 0, sizeof(*info));
	int type = EVP_PKEY_base_id(key->native);
	info->spec.algorithm = type == EVP_PKEY_RSA ? XP_KEY_RSA
		: type == EVP_PKEY_EC ? XP_KEY_ECDSA
		: XP_KEY_ED25519;
	info->spec.bits = (unsigned)EVP_PKEY_get_bits(key->native);
	if (type == EVP_PKEY_EC) {
		char group[64];
		size_t group_len = 0;
		if (EVP_PKEY_get_utf8_string_param(key->native, OSSL_PKEY_PARAM_GROUP_NAME,
		                                  group, sizeof(group), &group_len) != 1)
			return XP_CA_ERR;
		int nid = OBJ_txt2nid(group);
		info->spec.curve = nid == NID_X9_62_prime256v1 ? XP_KEY_CURVE_P256
			: nid == NID_secp384r1 ? XP_KEY_CURVE_P384
			: nid == NID_secp521r1 ? XP_KEY_CURVE_P521 : XP_KEY_CURVE_NONE;
		if (info->spec.curve == XP_KEY_CURVE_NONE)
			return XP_CA_ERR_FORMAT;
	}
	EVP_PKEY_CTX *check = EVP_PKEY_CTX_new(key->native, NULL);
	info->has_private = check != NULL && EVP_PKEY_private_check(check) == 1;
	EVP_PKEY_CTX_free(check);
	info->exportable = key->exportable;
	info->storage = key->storage;
	return XP_CA_OK;
}

int
xp_key_import_private_pem(xp_key_t *out, const void *pem, size_t len,
                     xp_ca_password_callback_t password, void *context)
{
	if (out == NULL)
		return XP_CA_ERR;
	*out = NULL;
	if (pem == NULL || len == 0 || len > INT_MAX)
		return XP_CA_ERR;
	unsigned char *pass = NULL;
	size_t pass_len = 0;
	if (password_value(password, context, &pass, &pass_len) != XP_CA_OK)
		return XP_CA_ERR;
	BIO *bio = BIO_new_mem_buf(pem, (int)len);
	EVP_PKEY *native = bio == NULL ? NULL
		: PEM_read_bio_PrivateKey(bio, NULL, NULL, pass);
	BIO_free(bio);
	if (pass != NULL) {
		xp_ca_scrub_memory(pass, pass_len + 1);
		free(pass);
	}
	if (!supported_key(native)) {
		EVP_PKEY_free(native);
		return fail(XP_CA_ERR_FORMAT, "private key PEM import failed");
	}
	xp_key_t result = key_new(native);
	if (result == NULL) {
		EVP_PKEY_free(native);
		return fail(XP_CA_ERR, "out of memory");
	}
	*out = result;
	return XP_CA_OK;
}

int
xp_key_export_private_pem(xp_key_t key, xp_ca_password_callback_t password,
                     void *context, void *out, size_t *len)
{
	if (key == NULL || len == NULL)
		return XP_CA_ERR;
	struct xp_key_info info;
	if (xp_key_get_info(key, &info) != XP_CA_OK || !info.has_private)
		return XP_CA_ERR_POLICY;
	if (!info.exportable)
		return XP_CRYPTO_ERR_NOT_EXPORTABLE;
	unsigned char *pass = NULL;
	size_t pass_len = 0;
	if (password_value(password, context, &pass, &pass_len) != XP_CA_OK)
		return XP_CA_ERR;
	BIO *bio = BIO_new(BIO_s_mem());
	const EVP_CIPHER *cipher = pass == NULL ? NULL : EVP_aes_256_cbc();
	int ok = bio != NULL && PEM_write_bio_PKCS8PrivateKey(
		bio, key->native, cipher, (char *)pass, (int)pass_len, NULL, NULL) == 1;
	if (pass != NULL) {
		xp_ca_scrub_memory(pass, pass_len + 1);
		free(pass);
	}
	BUF_MEM *memory = NULL;
	if (ok)
		BIO_get_mem_ptr(bio, &memory);
	if (!ok || memory == NULL) {
		BIO_free(bio);
		return fail(XP_CA_ERR, "private key PEM export failed");
	}
	size_t required = memory->length;
	if (out == NULL) {
		*len = required;
		xp_ca_scrub_memory(memory->data, memory->length);
		BIO_free(bio);
		return XP_CA_OK;
	}
	if (*len < required) {
		*len = required;
		xp_ca_scrub_memory(memory->data, memory->length);
		BIO_free(bio);
		return XP_CA_ERR;
	}
	memcpy(out, memory->data, required);
	*len = required;
	xp_ca_scrub_memory(memory->data, memory->length);
	BIO_free(bio);
	return XP_CA_OK;
}

int
xp_key_import_spki_der(xp_key_t *out, const void *data, size_t len)
{
	if (out == NULL)
		return XP_CA_ERR;
	*out = NULL;
	if (data == NULL || len == 0)
		return XP_CA_ERR;
	const unsigned char *cursor = data;
	EVP_PKEY *native = d2i_PUBKEY(NULL, &cursor, (long)len);
	if (!supported_key(native) || cursor != (const unsigned char *)data + len) {
		EVP_PKEY_free(native);
		return XP_CA_ERR_FORMAT;
	}
	xp_key_t result = key_new(native);
	if (result == NULL) {
		EVP_PKEY_free(native);
		return XP_CA_ERR;
	}
	*out = result;
	return XP_CA_OK;
}

int
xp_key_export_spki_der(xp_key_t key, void *out, size_t *len)
{
	if (key == NULL)
		return XP_CA_ERR;
	int size = i2d_PUBKEY(key->native, NULL);
	if (size <= 0)
		return XP_CA_ERR;
	unsigned char *encoded = malloc((size_t)size), *cursor = encoded;
	if (encoded == NULL || i2d_PUBKEY(key->native, &cursor) != size) {
		free(encoded);
		return XP_CA_ERR;
	}
	int status = der(size, encoded, out, len);
	free(encoded);
	return status;
}

int
xp_key_fingerprint_sha256(xp_key_t key, void *out, size_t *len)
{
	if (key == NULL || len == NULL)
		return XP_CRYPTO_ERR_INVALID;
	if (out == NULL) {
		*len = 32;
		return XP_CRYPTO_OK;
	}
	if (*len < 32) {
		*len = 32;
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	int encoded_len = i2d_PUBKEY(key->native, NULL);
	unsigned char *encoded = encoded_len > 0 ? malloc((size_t)encoded_len) : NULL;
	unsigned char *cursor = encoded;
	unsigned int digest_len = 0;
	int ok = encoded != NULL
		&& i2d_PUBKEY(key->native, &cursor) == encoded_len
		&& EVP_Digest(encoded, (size_t)encoded_len, out, &digest_len,
		              EVP_sha256(), NULL) == 1;
	free(encoded);
	if (!ok || digest_len != 32)
		return fail(XP_CRYPTO_ERR, "public-key fingerprint failed");
	*len = digest_len;
	return XP_CRYPTO_OK;
}

static int
copy_bn(const BIGNUM *value, void *out, size_t *len, size_t width)
{
	if (value == NULL || len == NULL)
		return XP_CA_ERR;
	size_t required = width == 0 ? (size_t)BN_num_bytes(value) : width;
	if (out == NULL) {
		*len = required;
		return XP_CA_OK;
	}
	if (*len < required) {
		*len = required;
		return XP_CA_ERR;
	}
	if (BN_bn2binpad(value, out, (int)required) != (int)required)
		return XP_CA_ERR;
	*len = required;
	return XP_CA_OK;
}

int
xp_key_get_rsa_public(xp_key_t key, void *modulus, size_t *modulus_len,
                         void *exponent, size_t *exponent_len)
{
	if (key == NULL || EVP_PKEY_base_id(key->native) != EVP_PKEY_RSA)
		return XP_CA_ERR_POLICY;
	BIGNUM *n = NULL, *e = NULL;
	int ok = EVP_PKEY_get_bn_param(key->native, OSSL_PKEY_PARAM_RSA_N, &n)
		&& EVP_PKEY_get_bn_param(key->native, OSSL_PKEY_PARAM_RSA_E, &e);
	int status = !ok ? XP_CA_ERR
		: copy_bn(n, modulus, modulus_len, 0) != XP_CA_OK ? XP_CA_ERR
		: copy_bn(e, exponent, exponent_len, 0);
	BN_free(n);
	BN_free(e);
	return status;
}

int
xp_key_get_ec_public(xp_key_t key, void *x, size_t *x_len,
                        void *y, size_t *y_len)
{
	if (key == NULL || EVP_PKEY_base_id(key->native) != EVP_PKEY_EC)
		return XP_CA_ERR_POLICY;
	BIGNUM *bx = NULL, *by = NULL;
	int ok = EVP_PKEY_get_bn_param(key->native, OSSL_PKEY_PARAM_EC_PUB_X, &bx)
		&& EVP_PKEY_get_bn_param(key->native, OSSL_PKEY_PARAM_EC_PUB_Y, &by);
	size_t width = (EVP_PKEY_get_bits(key->native) + 7u) / 8u;
	int status = !ok ? XP_CA_ERR
		: copy_bn(bx, x, x_len, width) != XP_CA_OK ? XP_CA_ERR
		: copy_bn(by, y, y_len, width);
	BN_free(bx);
	BN_free(by);
	return status;
}
int
xp_key_save_private_pem_file(xp_key_t k, const char *path, const char *pass)
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
xp_key_delete_private_pem_file(const char *path)
{
	if (path == NULL || remove(path) != 0)
		return fail(XP_CA_ERR, "private key deletion failed");
	return XP_CA_OK;
}
void
xp_key_release(xp_key_t k)
{
	if (k != NULL && atomic_fetch_sub_explicit(
	        &k->references, 1, memory_order_acq_rel) == 1) {
		EVP_PKEY_free(k->native);
		free(k->reference);
		free(k);
	}
}

void
xp_key_retain(xp_key_t key)
{
	if (key != NULL)
		atomic_fetch_add_explicit(&key->references, 1, memory_order_relaxed);
}

const char *
xp_key_errstr(xp_key_t key)
{
	return key == NULL ? "(null xp_key_t)"
		: key->err[0] == '\0' ? "no error" : key->err;
}

void
xp_key_set_storage_metadata(
	xp_key_t key, enum xp_key_store_kind storage, bool exportable)
{
	if (key != NULL) {
		key->storage = storage;
		key->exportable = exportable;
	}
}

void *xp_key_native_private(xp_key_t key)
{
	struct xp_key_info info;
	return key != NULL && xp_key_get_info(key, &info) == XP_CRYPTO_OK
		&& info.has_private ? key->native : NULL;
}

xp_key_t
xp_key_wrap_native_private(
	void *native, enum xp_key_store_kind storage, bool exportable)
{
	if (native == NULL)
		return NULL;
	xp_key_t key = key_new(native);
	if (key != NULL) {
		key->storage = storage;
		key->exportable = exportable;
	}
	return key;
}

int
xp_key_set_reference(xp_key_t key, const void *reference, size_t len)
{
	if (key == NULL || (reference == NULL && len != 0))
		return XP_CRYPTO_ERR_INVALID;
	unsigned char *copy = len == 0 ? NULL : malloc(len);
	if (len != 0 && copy == NULL)
		return XP_CRYPTO_ERR;
	if (len != 0)
		memcpy(copy, reference, len);
	free(key->reference);
	key->reference = copy;
	key->reference_len = len;
	return XP_CRYPTO_OK;
}

int
xp_key_reference(xp_key_t key, void *out, size_t *len)
{
	if (key == NULL || len == NULL)
		return XP_CRYPTO_ERR_INVALID;
	if (key->reference_len == 0) {
		*len = 0;
		return XP_CRYPTO_ERR_NOT_FOUND;
	}
	if (out == NULL) {
		*len = key->reference_len;
		return XP_CRYPTO_OK;
	}
	if (*len < key->reference_len) {
		*len = key->reference_len;
		return XP_CRYPTO_ERR_BUFFER_TOO_SMALL;
	}
	memcpy(out, key->reference, key->reference_len);
	*len = key->reference_len;
	return XP_CRYPTO_OK;
}
static bool
csr_identity_valid(const struct xp_ca_identity *identity)
{
	return xp_ca_identity_is_valid(identity, true);
}

static int
add_name_value(X509_NAME *name, int nid, const char *value)
{
	return value == NULL || X509_NAME_add_entry_by_NID(name, nid, MBSTRING_UTF8,
		(const unsigned char *)value, -1, -1, 0) == 1;
}

static int
add_identity_name(X509_NAME *name, const struct xp_ca_identity *identity)
{
	return add_name_value(name, NID_countryName, identity->country)
		&& add_name_value(name, NID_stateOrProvinceName,
			identity->state_or_province)
		&& add_name_value(name, NID_localityName, identity->locality)
		&& add_name_value(name, NID_organizationName, identity->organization)
		&& add_name_value(name, NID_organizationalUnitName,
			identity->organizational_unit)
		&& add_name_value(name, NID_commonName, identity->common_name)
		&& add_name_value(name, NID_pkcs9_emailAddress,
			identity->email_address);
}

static int
csr_create(xp_ca_csr_t *out, xp_key_t k,
           const struct xp_ca_identity *identity,
	       const struct xp_ca_extension *requested, size_t requested_count)
{
	if (!out) {
		return XP_CA_ERR;
	}
	*out = NULL;
	if (!k) {
		return XP_CA_ERR;
	}
	if ((identity != NULL && !csr_identity_valid(identity))
	    || (requested_count != 0 && requested == NULL))
		return XP_CA_ERR_POLICY;
	struct xp_key_info info;
	if (xp_key_get_info(k, &info) != XP_CA_OK || !info.has_private)
		return XP_CA_ERR_POLICY;
	X509_REQ *r = X509_REQ_new();
	X509_NAME *subject = r == NULL ? NULL : X509_REQ_get_subject_name(r);
	int ok = r != NULL && X509_REQ_set_version(r, 0) == 1
		&& X509_REQ_set_pubkey(r, k->native) == 1;
	if (ok && identity != NULL)
		ok = add_identity_name(subject, identity);
	STACK_OF(X509_EXTENSION) *extensions = NULL;
	if (ok && ((identity != NULL && identity->dns_name_count != 0)
	    || requested_count != 0))
		extensions = sk_X509_EXTENSION_new_null();
	if (ok && ((identity != NULL && identity->dns_name_count != 0)
	    || requested_count != 0) && extensions == NULL)
		ok = 0;
	if (ok && identity != NULL && identity->dns_name_count != 0) {
		GENERAL_NAMES *names = sk_GENERAL_NAME_new_null();
		ok = names != NULL;
		for (size_t i = 0; ok && i < identity->dns_name_count; i++) {
			GENERAL_NAME *name = make_general_name(GEN_DNS, identity->dns_names[i]);
			if (name == NULL || !sk_GENERAL_NAME_push(names, name)) {
				GENERAL_NAME_free(name);
				ok = 0;
			}
		}
		X509_EXTENSION *extension = ok
			? X509V3_EXT_i2d(NID_subject_alt_name, 0, names) : NULL;
		if (extension == NULL)
			ok = 0;
		else if (!sk_X509_EXTENSION_push(extensions, extension))
			ok = 0;
		else
			extension = NULL;
		X509_EXTENSION_free(extension);
		GENERAL_NAMES_free(names);
	}
	for (size_t i = 0; ok && i < requested_count; i++) {
		const struct xp_ca_extension *item = &requested[i];
		ASN1_OBJECT *object = item->oid == NULL ? NULL : OBJ_txt2obj(item->oid, 1);
		ASN1_OCTET_STRING *value = ASN1_OCTET_STRING_new();
		X509_EXTENSION *extension = NULL;
		if (object == NULL || value == NULL || item->value_der == NULL
		    || item->value_der_len == 0 || item->value_der_len > INT_MAX
		    || ASN1_OCTET_STRING_set(value, item->value_der,
			(int)item->value_der_len) != 1)
			ok = 0;
		if (ok)
			extension = X509_EXTENSION_create_by_OBJ(NULL, object,
				item->critical ? 1 : 0, value);
		if (ok && (extension == NULL
		    || !sk_X509_EXTENSION_push(extensions, extension)))
			ok = 0;
		else
			extension = NULL;
		X509_EXTENSION_free(extension);
		ASN1_OCTET_STRING_free(value);
		ASN1_OBJECT_free(object);
	}
	if (ok && extensions != NULL && X509_REQ_add_extensions(r, extensions) != 1)
		ok = 0;
	sk_X509_EXTENSION_pop_free(extensions, X509_EXTENSION_free);
	if (ok)
		ok = X509_REQ_sign(r, k->native, key_md(k->native)) > 0;
	if (!ok) {
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
xp_ca_csr_create(xp_ca_csr_t *out, xp_key_t key)
{
	return csr_create(out, key, NULL, NULL, 0);
}

int
xp_ca_csr_create_with_identity(xp_ca_csr_t *out, xp_key_t key,
                               const struct xp_ca_identity *identity)
{
	return csr_create(out, key, identity, NULL, 0);
}

int
xp_ca_csr_create_request(xp_ca_csr_t *out, xp_key_t key,
	const struct xp_ca_csr_request *request)
{
	if (request == NULL)
		return XP_CRYPTO_ERR_INVALID;
	return csr_create(out, key, &request->subject,
		request->extensions, request->extension_count);
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
xp_ca_csr_get_public_key(xp_key_t *out, xp_ca_csr_t csr)
{
	if (out == NULL)
		return XP_CRYPTO_ERR_INVALID;
	*out = NULL;
	EVP_PKEY *native = csr == NULL ? NULL : X509_REQ_get_pubkey(csr->native);
	if (native == NULL || !supported_key(native)) {
		EVP_PKEY_free(native);
		return XP_CA_ERR_FORMAT;
	}
	*out = key_new(native);
	if (*out == NULL) {
		EVP_PKEY_free(native);
		return XP_CA_ERR;
	}
	return XP_CA_OK;
}

int
xp_ca_csr_import(xp_ca_csr_t *out, enum xp_ca_encoding encoding,
	const void *data, size_t len)
{
	if (encoding == XP_CA_ENCODING_DER)
		return xp_ca_csr_import_der(out, data, len);
	if (out == NULL || encoding != XP_CA_ENCODING_PEM || data == NULL
	    || len == 0 || len > INT_MAX)
		return XP_CRYPTO_ERR_INVALID;
	*out = NULL;
	BIO *bio = BIO_new_mem_buf(data, (int)len);
	X509_REQ *native = bio == NULL ? NULL
		: PEM_read_bio_X509_REQ(bio, NULL, NULL, NULL);
	char trailing[64];
	int got;
	int clean = native != NULL;
	while (clean && (got = BIO_read(bio, trailing, sizeof(trailing))) > 0)
		for (int i = 0; i < got; i++)
			if (!isspace((unsigned char)trailing[i])) clean = 0;
	BIO_free(bio);
	if (!clean) {
		X509_REQ_free(native);
		return XP_CA_ERR_FORMAT;
	}
	xp_ca_csr_t result = calloc(1, sizeof(*result));
	if (result == NULL) {
		X509_REQ_free(native);
		return XP_CA_ERR;
	}
	result->native = native;
	*out = result;
	return XP_CA_OK;
}

int
xp_ca_csr_export(xp_ca_csr_t csr, enum xp_ca_encoding encoding,
	void *out, size_t *len)
{
	if (encoding == XP_CA_ENCODING_DER)
		return xp_ca_csr_export_der(csr, out, len);
	if (csr == NULL || encoding != XP_CA_ENCODING_PEM || len == NULL)
		return XP_CRYPTO_ERR_INVALID;
	BIO *bio = BIO_new(BIO_s_mem());
	BUF_MEM *memory = NULL;
	if (bio == NULL || PEM_write_bio_X509_REQ(bio, csr->native) != 1) {
		BIO_free(bio);
		return XP_CA_ERR;
	}
	BIO_get_mem_ptr(bio, &memory);
	size_t required = memory == NULL ? 0 : memory->length;
	if (out == NULL) *len = required;
	else if (*len < required) { *len = required; BIO_free(bio); return XP_CRYPTO_ERR_BUFFER_TOO_SMALL; }
	else { memcpy(out, memory->data, required); *len = required; }
	BIO_free(bio);
	return required == 0 ? XP_CA_ERR : XP_CA_OK;
}
int
xp_ca_csr_verify(xp_ca_csr_t c)
{
	EVP_PKEY *p = c ? X509_REQ_get_pubkey(c->native) : 0;
	int r = p && supported_key(p) && X509_REQ_verify(c->native, p) == 1
		? XP_CA_OK : XP_CA_ERR_VERIFY;
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
	xp_key_t key,
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
	    || !X509_sign(x, key->native, key_md(key->native))) {
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
	xp_key_t key,
	const struct xp_ca_issue_request *request)
{
	return issue(out, key, NULL, NULL, request);
}
int
xp_ca_cert_issue(
	xp_ca_cert_t *out,
	xp_key_t issuer_key,
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

int
xp_ca_cert_chain_import_pem(xp_ca_cert_t **out, size_t *count,
                            const void *pem, size_t len)
{
	if (out == NULL || count == NULL)
		return XP_CA_ERR;
	*out = NULL;
	*count = 0;
	if (pem == NULL || len == 0 || len > INT_MAX)
		return XP_CA_ERR;
	xp_ca_cert_t *items = NULL;
	size_t used = 0;
	const char *input = pem;
	const char begin[] = "-----BEGIN CERTIFICATE-----";
	const char end[] = "-----END CERTIFICATE-----";
	size_t position = 0;
	while (position < len) {
		while (position < len && isspace((unsigned char)input[position]))
			position++;
		if (position == len)
			break;
		if (len - position < sizeof(begin) - 1
		    || memcmp(input + position, begin, sizeof(begin) - 1) != 0) {
			xp_ca_cert_chain_free(items, used);
			return XP_CA_ERR_FORMAT;
		}
		const char *finish = NULL;
		for (size_t scan = position + sizeof(begin) - 1;
		     scan + sizeof(end) - 1 <= len; scan++) {
			if (memcmp(input + scan, end, sizeof(end) - 1) == 0) {
				finish = input + scan;
				break;
			}
		}
		if (finish == NULL) {
			xp_ca_cert_chain_free(items, used);
			return XP_CA_ERR_FORMAT;
		}
		size_t object_len = (size_t)(finish - (input + position)) + sizeof(end) - 1;
		BIO *bio = BIO_new_mem_buf(input + position, (int)object_len);
		X509 *native = bio == NULL ? NULL : PEM_read_bio_X509(bio, NULL, NULL, NULL);
		BIO_free(bio);
		if (native == NULL) {
			xp_ca_cert_chain_free(items, used);
			return XP_CA_ERR_FORMAT;
		}
		xp_ca_cert_t item = calloc(1, sizeof(*item));
		xp_ca_cert_t *grown = realloc(items, (used + 1) * sizeof(*items));
		if (item == NULL || grown == NULL) {
			free(item);
			X509_free(native);
			xp_ca_cert_chain_free(items, used);
			return XP_CA_ERR;
		}
		items = grown;
		item->native = native;
		items[used++] = item;
		position += object_len;
	}
	ERR_clear_error();
	if (used == 0) {
		xp_ca_cert_chain_free(items, used);
		return XP_CA_ERR_FORMAT;
	}
	*out = items;
	*count = used;
	return XP_CA_OK;
}

int
xp_ca_cert_chain_export_pem(const xp_ca_cert_t *certs, size_t count,
                            void *out, size_t *len)
{
	if (certs == NULL || count == 0 || len == NULL)
		return XP_CA_ERR;
	BIO *bio = BIO_new(BIO_s_mem());
	int ok = bio != NULL;
	for (size_t i = 0; ok && i < count; i++)
		ok = certs[i] != NULL && PEM_write_bio_X509(bio, certs[i]->native) == 1;
	BUF_MEM *memory = NULL;
	if (ok)
		BIO_get_mem_ptr(bio, &memory);
	if (!ok || memory == NULL) {
		BIO_free(bio);
		return XP_CA_ERR;
	}
	if (out == NULL) {
		*len = memory->length;
		BIO_free(bio);
		return XP_CA_OK;
	}
	if (*len < memory->length) {
		*len = memory->length;
		BIO_free(bio);
		return XP_CA_ERR;
	}
	memcpy(out, memory->data, memory->length);
	*len = memory->length;
	BIO_free(bio);
	return XP_CA_OK;
}

void
xp_ca_cert_chain_free(xp_ca_cert_t *certs, size_t count)
{
	if (certs != NULL) {
		for (size_t i = 0; i < count; i++)
			xp_ca_cert_free(certs[i]);
		free(certs);
	}
}

int
xp_ca_cert_bundle_import(xp_ca_cert_t **out, size_t *count,
	enum xp_ca_encoding encoding, const void *data, size_t len)
{
	if (out == NULL || count == NULL)
		return XP_CRYPTO_ERR_INVALID;
	*out = NULL;
	*count = 0;
	if (encoding == XP_CA_ENCODING_PEM)
		return xp_ca_cert_chain_import_pem(out, count, data, len);
	if (encoding == XP_CA_ENCODING_DER) {
		xp_ca_cert_t cert = NULL;
		int status = xp_ca_cert_import_der(&cert, data, len);
		if (status != XP_CA_OK) return status;
		*out = malloc(sizeof(**out));
		if (*out == NULL) { xp_ca_cert_free(cert); return XP_CA_ERR; }
		(*out)[0] = cert; *count = 1; return XP_CA_OK;
	}
	if ((encoding != XP_CA_ENCODING_PKCS7_DER
	    && encoding != XP_CA_ENCODING_PKCS7_PEM) || data == NULL
	    || len == 0 || len > INT_MAX)
		return XP_CRYPTO_ERR_INVALID;
	PKCS7 *p7 = NULL;
	if (encoding == XP_CA_ENCODING_PKCS7_DER) {
		const unsigned char *cursor = data;
		p7 = d2i_PKCS7(NULL, &cursor, (long)len);
		if (p7 != NULL && cursor != (const unsigned char *)data + len) {
			PKCS7_free(p7); p7 = NULL;
		}
	}
	else {
		BIO *bio = BIO_new_mem_buf(data, (int)len);
		p7 = bio == NULL ? NULL : PEM_read_bio_PKCS7(bio, NULL, NULL, NULL);
		BIO_free(bio);
	}
	STACK_OF(X509) *native = p7 != NULL && PKCS7_type_is_signed(p7)
		? p7->d.sign->cert : NULL;
	int total = native == NULL ? 0 : sk_X509_num(native);
	if (total <= 0) { PKCS7_free(p7); return XP_CA_ERR_FORMAT; }
	xp_ca_cert_t *items = calloc((size_t)total, sizeof(*items));
	int status = items == NULL ? XP_CA_ERR : XP_CA_OK;
	for (int i = 0; status == XP_CA_OK && i < total; i++) {
		items[i] = calloc(1, sizeof(*items[i]));
		if (items[i] == NULL || X509_up_ref(sk_X509_value(native, i)) != 1)
			status = XP_CA_ERR;
		else items[i]->native = sk_X509_value(native, i);
	}
	PKCS7_free(p7);
	if (status != XP_CA_OK) { xp_ca_cert_chain_free(items, (size_t)total); return status; }
	*out = items; *count = (size_t)total; return XP_CA_OK;
}

int
xp_ca_cert_bundle_export(const xp_ca_cert_t *certs, size_t count,
	enum xp_ca_encoding encoding, void *out, size_t *len)
{
	if (encoding == XP_CA_ENCODING_PEM)
		return xp_ca_cert_chain_export_pem(certs, count, out, len);
	if (encoding == XP_CA_ENCODING_DER)
		return count == 1 ? xp_ca_cert_export_der(certs[0], out, len)
			: XP_CRYPTO_ERR_INVALID;
	if (certs == NULL || count == 0 || len == NULL
	    || (encoding != XP_CA_ENCODING_PKCS7_DER
	        && encoding != XP_CA_ENCODING_PKCS7_PEM))
		return XP_CRYPTO_ERR_INVALID;
	PKCS7 *p7 = PKCS7_new();
	int ok = p7 != NULL && PKCS7_set_type(p7, NID_pkcs7_signed) == 1
		&& PKCS7_content_new(p7, NID_pkcs7_data) == 1;
	for (size_t i = 0; ok && i < count; i++)
		ok = certs[i] != NULL && PKCS7_add_certificate(p7, certs[i]->native) == 1;
	BIO *bio = ok ? BIO_new(BIO_s_mem()) : NULL;
	if (bio != NULL)
		ok = encoding == XP_CA_ENCODING_PKCS7_DER
			? i2d_PKCS7_bio(bio, p7) == 1 : PEM_write_bio_PKCS7(bio, p7) == 1;
	BUF_MEM *memory = NULL;
	if (ok) BIO_get_mem_ptr(bio, &memory);
	if (!ok || memory == NULL) { BIO_free(bio); PKCS7_free(p7); return XP_CA_ERR; }
	size_t required = memory->length;
	int status = XP_CA_OK;
	if (out == NULL) *len = required;
	else if (*len < required) { *len = required; status = XP_CRYPTO_ERR_BUFFER_TOO_SMALL; }
	else { memcpy(out, memory->data, required); *len = required; }
	BIO_free(bio); PKCS7_free(p7); return status;
}

int
xp_ca_cert_get_validity(xp_ca_cert_t cert, time_t *not_before, time_t *not_after)
{
	if (cert == NULL || not_before == NULL || not_after == NULL)
		return XP_CA_ERR;
	struct tm first = { 0 }, last = { 0 };
	if (!ASN1_TIME_to_tm(X509_get0_notBefore(cert->native), &first)
	    || !ASN1_TIME_to_tm(X509_get0_notAfter(cert->native), &last))
		return XP_CA_ERR_FORMAT;
	*not_before = timegm(&first);
	*not_after = timegm(&last);
	return XP_CA_OK;
}

int
xp_ca_cert_get_public_key(xp_key_t *out, xp_ca_cert_t cert)
{
	if (out == NULL)
		return XP_CA_ERR;
	*out = NULL;
	if (cert == NULL)
		return XP_CA_ERR;
	EVP_PKEY *native = X509_get_pubkey(cert->native);
	if (!supported_key(native)) {
		EVP_PKEY_free(native);
		return XP_CA_ERR_FORMAT;
	}
	xp_key_t result = key_new(native);
	if (result == NULL) {
		EVP_PKEY_free(native);
		return XP_CA_ERR;
	}
	*out = result;
	return XP_CA_OK;
}

int
xp_ca_cert_verify_signature(xp_ca_cert_t cert, xp_ca_cert_t issuer)
{
	if (cert == NULL)
		return XP_CRYPTO_ERR_INVALID;
	EVP_PKEY *key = X509_get_pubkey(issuer == NULL ? cert->native : issuer->native);
	int result = key != NULL && X509_verify(cert->native, key) == 1
		? XP_CA_OK : XP_CA_ERR_VERIFY;
	EVP_PKEY_free(key);
	return result;
}

int
xp_ca_cert_get_info(xp_ca_cert_t cert, struct xp_ca_cert_info *info)
{
	if (cert == NULL || info == NULL)
		return XP_CRYPTO_ERR_INVALID;
	memset(info, 0, sizeof(*info));
	info->version = (unsigned)X509_get_version(cert->native) + 1;
	info->self_signed = X509_NAME_cmp(X509_get_subject_name(cert->native),
		X509_get_issuer_name(cert->native)) == 0
		&& xp_ca_cert_verify_signature(cert, NULL) == XP_CA_OK;
	BASIC_CONSTRAINTS *bc = X509_get_ext_d2i(cert->native,
		NID_basic_constraints, NULL, NULL);
	if (bc != NULL) {
		info->is_ca = bc->ca != 0;
		info->has_path_length = bc->pathlen != NULL;
		if (bc->pathlen != NULL) info->path_length = (int)ASN1_INTEGER_get(bc->pathlen);
		BASIC_CONSTRAINTS_free(bc);
	}
	uint32_t usage = X509_get_key_usage(cert->native);
	if (usage & KU_DIGITAL_SIGNATURE) info->key_usage |= XP_CA_KEY_USE_SIGN;
	if (usage & KU_KEY_CERT_SIGN) info->key_usage |= XP_CA_KEY_USE_CERT_SIGN;
	if (usage & KU_CRL_SIGN) info->key_usage |= XP_CA_KEY_USE_CRL_SIGN;
	if (usage & KU_KEY_ENCIPHERMENT) info->key_usage |= XP_CA_KEY_USE_KEY_ENCIPHERMENT;
	if (usage & KU_DATA_ENCIPHERMENT) info->key_usage |= XP_CA_KEY_USE_DATA_ENCIPHERMENT;
	if (usage & KU_KEY_AGREEMENT) info->key_usage |= XP_CA_KEY_USE_KEY_AGREEMENT;
	if (usage & KU_NON_REPUDIATION) info->key_usage |= XP_CA_KEY_USE_NON_REPUDIATION;
	if (usage & KU_ENCIPHER_ONLY) info->key_usage |= XP_CA_KEY_USE_ENCIPHER_ONLY;
	if (usage & KU_DECIPHER_ONLY) info->key_usage |= XP_CA_KEY_USE_DECIPHER_ONLY;
	uint32_t eku = X509_get_extended_key_usage(cert->native);
	if (eku & XKU_SSL_SERVER) info->extended_key_usage |= XP_CA_EKU_SERVER_AUTH;
	if (eku & XKU_SSL_CLIENT) info->extended_key_usage |= XP_CA_EKU_CLIENT_AUTH;
	if (eku & XKU_CODE_SIGN) info->extended_key_usage |= XP_CA_EKU_CODE_SIGNING;
	if (eku & XKU_SMIME) info->extended_key_usage |= XP_CA_EKU_EMAIL_PROTECTION;
	if (eku & XKU_TIMESTAMP) info->extended_key_usage |= XP_CA_EKU_TIME_STAMPING;
	if (eku & XKU_OCSP_SIGN) info->extended_key_usage |= XP_CA_EKU_OCSP_SIGNING;
	return XP_CA_OK;
}

static int
copy_result(const void *data, size_t size, void *out, size_t *len)
{
	if (len == NULL) return XP_CRYPTO_ERR_INVALID;
	if (out == NULL) { *len = size; return XP_CA_OK; }
	if (*len < size) { *len = size; return XP_CRYPTO_ERR_BUFFER_TOO_SMALL; }
	memcpy(out, data, size); *len = size; return XP_CA_OK;
}

int
xp_ca_cert_get_serial(xp_ca_cert_t cert, void *out, size_t *len)
{
	if (cert == NULL) return XP_CRYPTO_ERR_INVALID;
	const ASN1_INTEGER *serial = X509_get0_serialNumber(cert->native);
	const unsigned char *data = ASN1_STRING_get0_data(serial);
	size_t size = (size_t)ASN1_STRING_length(serial);
	while (size > 1 && *data == 0) { data++; size--; }
	return copy_result(data, size, out, len);
}

int
xp_ca_cert_get_fingerprint(xp_ca_cert_t cert,
	enum xp_digest_algorithm digest, void *out, size_t *len)
{
	if (cert == NULL || len == NULL) return XP_CRYPTO_ERR_INVALID;
	const EVP_MD *md = digest == XP_DIGEST_SHA256 ? EVP_sha256()
		: digest == XP_DIGEST_SHA384 ? EVP_sha384()
		: digest == XP_DIGEST_SHA512 ? EVP_sha512() : NULL;
	if (md == NULL) return XP_CRYPTO_ERR_UNSUPPORTED;
	unsigned char value[EVP_MAX_MD_SIZE]; unsigned int size = 0;
	if (X509_digest(cert->native, md, value, &size) != 1) return XP_CA_ERR;
	return copy_result(value, size, out, len);
}

static int
name_field_nid(enum xp_ca_name_field field)
{
	switch (field) {
		case XP_CA_NAME_COUNTRY: return NID_countryName;
		case XP_CA_NAME_STATE_OR_PROVINCE: return NID_stateOrProvinceName;
		case XP_CA_NAME_LOCALITY: return NID_localityName;
		case XP_CA_NAME_ORGANIZATION: return NID_organizationName;
		case XP_CA_NAME_ORGANIZATIONAL_UNIT: return NID_organizationalUnitName;
		case XP_CA_NAME_COMMON_NAME: return NID_commonName;
		case XP_CA_NAME_EMAIL_ADDRESS: return NID_pkcs9_emailAddress;
		default: return NID_undef;
	}
}

static X509_NAME *
certificate_name(xp_ca_cert_t cert, enum xp_ca_name_kind kind)
{
	if (cert == NULL) return NULL;
	return kind == XP_CA_NAME_SUBJECT ? X509_get_subject_name(cert->native)
		: kind == XP_CA_NAME_ISSUER ? X509_get_issuer_name(cert->native) : NULL;
}

int
xp_ca_cert_get_name_count(xp_ca_cert_t cert, enum xp_ca_name_kind kind,
	enum xp_ca_name_field field, size_t *count)
{
	X509_NAME *name = certificate_name(cert, kind); int nid = name_field_nid(field);
	if (name == NULL || nid == NID_undef || count == NULL) return XP_CRYPTO_ERR_INVALID;
	*count = 0; int position = -1;
	while ((position = X509_NAME_get_index_by_NID(name, nid, position)) >= 0) (*count)++;
	return XP_CA_OK;
}

int
xp_ca_cert_get_name(xp_ca_cert_t cert, enum xp_ca_name_kind kind,
	enum xp_ca_name_field field, size_t index, void *out, size_t *len)
{
	X509_NAME *name = certificate_name(cert, kind); int nid = name_field_nid(field);
	if (name == NULL || nid == NID_undef) return XP_CRYPTO_ERR_INVALID;
	int position = -1;
	for (size_t i = 0; i <= index; i++) {
		position = X509_NAME_get_index_by_NID(name, nid, position);
		if (position < 0) return XP_CRYPTO_ERR_NOT_FOUND;
	}
	ASN1_STRING *value = X509_NAME_ENTRY_get_data(X509_NAME_get_entry(name, position));
	unsigned char *utf8 = NULL; int size = ASN1_STRING_to_UTF8(&utf8, value);
	if (size < 0) return XP_CA_ERR_FORMAT;
	int status = copy_result(utf8, (size_t)size, out, len); OPENSSL_free(utf8); return status;
}

static int
san_general_type(enum xp_ca_san_type type)
{
	return type == XP_CA_SAN_DNS ? GEN_DNS : type == XP_CA_SAN_EMAIL ? GEN_EMAIL
		: type == XP_CA_SAN_URI ? GEN_URI : type == XP_CA_SAN_IP_ADDRESS ? GEN_IPADD : -1;
}

int
xp_ca_cert_get_san_count(xp_ca_cert_t cert, enum xp_ca_san_type type,
	size_t *count)
{
	if (cert == NULL || count == NULL || san_general_type(type) < 0)
		return XP_CRYPTO_ERR_INVALID;
	GENERAL_NAMES *names = X509_get_ext_d2i(cert->native, NID_subject_alt_name, NULL, NULL);
	*count = 0;
	for (int i = 0; names != NULL && i < sk_GENERAL_NAME_num(names); i++)
		if (sk_GENERAL_NAME_value(names, i)->type == san_general_type(type)) (*count)++;
	GENERAL_NAMES_free(names); return XP_CA_OK;
}

int
xp_ca_cert_get_san(xp_ca_cert_t cert, enum xp_ca_san_type type,
	size_t index, void *out, size_t *len)
{
	if (cert == NULL || san_general_type(type) < 0) return XP_CRYPTO_ERR_INVALID;
	GENERAL_NAMES *names = X509_get_ext_d2i(cert->native, NID_subject_alt_name, NULL, NULL);
	GENERAL_NAME *found = NULL; size_t current = 0;
	for (int i = 0; names != NULL && i < sk_GENERAL_NAME_num(names); i++) {
		GENERAL_NAME *name = sk_GENERAL_NAME_value(names, i);
		if (name->type == san_general_type(type) && current++ == index) { found = name; break; }
	}
	if (found == NULL) { GENERAL_NAMES_free(names); return XP_CRYPTO_ERR_NOT_FOUND; }
	ASN1_STRING *value = type == XP_CA_SAN_IP_ADDRESS ? found->d.iPAddress
		: type == XP_CA_SAN_DNS ? found->d.dNSName
		: type == XP_CA_SAN_EMAIL ? found->d.rfc822Name
		: found->d.uniformResourceIdentifier;
	int status = copy_result(ASN1_STRING_get0_data(value),
		(size_t)ASN1_STRING_length(value), out, len);
	GENERAL_NAMES_free(names); return status;
}
void
xp_ca_cert_free(xp_ca_cert_t c)
{
	if (c) {
		X509_free(c->native);
		free(c);
	}
}

static const EVP_MD *
sign_md(enum xp_sign_algorithm algorithm)
{
	switch (algorithm) {
		case XP_SIGN_RSA_PKCS1_SHA256:
		case XP_SIGN_ECDSA_SHA256: return EVP_sha256();
		case XP_SIGN_RSA_PKCS1_SHA384:
		case XP_SIGN_ECDSA_SHA384: return EVP_sha384();
		case XP_SIGN_RSA_PKCS1_SHA512:
		case XP_SIGN_ECDSA_SHA512: return EVP_sha512();
		case XP_SIGN_ED25519: return NULL;
		default: return NULL;
	}
}

static int
sign_family(enum xp_sign_algorithm algorithm)
{
	if (algorithm >= XP_SIGN_RSA_PKCS1_SHA256
	    && algorithm <= XP_SIGN_RSA_PKCS1_SHA512)
		return EVP_PKEY_RSA;
	if (algorithm >= XP_SIGN_ECDSA_SHA256
	    && algorithm <= XP_SIGN_ECDSA_SHA512)
		return EVP_PKEY_EC;
	return algorithm == XP_SIGN_ED25519 ? EVP_PKEY_ED25519 : EVP_PKEY_NONE;
}

static int
ecdsa_der_to_p1363(EVP_PKEY *key, const unsigned char *der_signature,
	               size_t der_len, unsigned char **out, size_t *out_len)
{
	const unsigned char *cursor = der_signature;
	ECDSA_SIG *decoded = d2i_ECDSA_SIG(NULL, &cursor, (long)der_len);
	if (decoded == NULL || cursor != der_signature + der_len) {
		ECDSA_SIG_free(decoded);
		return XP_CA_ERR_FORMAT;
	}
	const BIGNUM *r, *s;
	ECDSA_SIG_get0(decoded, &r, &s);
	size_t width = (EVP_PKEY_get_bits(key) + 7u) / 8u;
	unsigned char *encoded = malloc(width * 2);
	if (encoded == NULL || BN_bn2binpad(r, encoded, (int)width) != (int)width
	    || BN_bn2binpad(s, encoded + width, (int)width) != (int)width) {
		free(encoded);
		ECDSA_SIG_free(decoded);
		return XP_CA_ERR;
	}
	ECDSA_SIG_free(decoded);
	*out = encoded;
	*out_len = width * 2;
	return XP_CA_OK;
}

static int
ecdsa_p1363_to_der(EVP_PKEY *key, const unsigned char *signature, size_t len,
	               unsigned char **out, size_t *out_len)
{
	size_t width = (EVP_PKEY_get_bits(key) + 7u) / 8u;
	if (len != width * 2)
		return XP_CA_ERR_FORMAT;
	ECDSA_SIG *value = ECDSA_SIG_new();
	BIGNUM *r = BN_bin2bn(signature, (int)width, NULL);
	BIGNUM *s = BN_bin2bn(signature + width, (int)width, NULL);
	if (value == NULL || r == NULL || s == NULL || !ECDSA_SIG_set0(value, r, s)) {
		ECDSA_SIG_free(value);
		BN_free(r);
		BN_free(s);
		return XP_CA_ERR;
	}
	int required = i2d_ECDSA_SIG(value, NULL);
	unsigned char *encoded = required <= 0 ? NULL : malloc((size_t)required);
	unsigned char *cursor = encoded;
	if (encoded == NULL || i2d_ECDSA_SIG(value, &cursor) != required) {
		free(encoded);
		ECDSA_SIG_free(value);
		return XP_CA_ERR;
	}
	ECDSA_SIG_free(value);
	*out = encoded;
	*out_len = (size_t)required;
	return XP_CA_OK;
}

int
xp_sign(xp_key_t key, enum xp_sign_algorithm algorithm,
	    enum xp_signature_encoding format, const void *data, size_t data_len,
	    void *signature, size_t *signature_len)
{
	if (key == NULL || signature_len == NULL || (data == NULL && data_len != 0)
	    || sign_family(algorithm) != EVP_PKEY_base_id(key->native)
	    || (format == XP_SIGNATURE_ENCODING_P1363
	        && sign_family(algorithm) != EVP_PKEY_EC)
	    || (format != XP_SIGNATURE_ENCODING_STANDARD && format != XP_SIGNATURE_ENCODING_P1363))
		return XP_CA_ERR_POLICY;
	struct xp_key_info info;
	if (xp_key_get_info(key, &info) != XP_CA_OK || !info.has_private)
		return XP_CA_ERR_POLICY;
	if (signature == NULL) {
		*signature_len = format == XP_SIGNATURE_ENCODING_P1363
			? 2u * ((EVP_PKEY_get_bits(key->native) + 7u) / 8u)
			: (size_t)EVP_PKEY_get_size(key->native);
		return XP_CA_OK;
	}
	EVP_MD_CTX *context = EVP_MD_CTX_new();
	EVP_PKEY_CTX *parameters = NULL;
	int ok = context != NULL && EVP_DigestSignInit(
		context, &parameters, sign_md(algorithm), NULL, key->native) == 1;
	if (ok && sign_family(algorithm) == EVP_PKEY_RSA)
		ok = EVP_PKEY_CTX_set_rsa_padding(parameters, RSA_PKCS1_PADDING) == 1;
	size_t native_len = (size_t)EVP_PKEY_get_size(key->native);
	unsigned char *native = ok && native_len != 0 ? malloc(native_len) : NULL;
	if (native == NULL)
		ok = 0;
	if (ok)
		ok = EVP_DigestSign(context, native, &native_len, data, data_len) == 1;
	EVP_MD_CTX_free(context);
	if (!ok) {
		free(native);
		return fail(XP_CA_ERR, "signature creation failed");
	}
	unsigned char *encoded = native;
	size_t encoded_len = native_len;
	if (format == XP_SIGNATURE_ENCODING_P1363) {
		if (ecdsa_der_to_p1363(key->native, native, native_len,
		                         &encoded, &encoded_len) != XP_CA_OK) {
			free(native);
			return XP_CA_ERR;
		}
		free(native);
	}
	if (*signature_len < encoded_len) {
		*signature_len = encoded_len;
		free(encoded);
		return XP_CA_ERR;
	}
	memcpy(signature, encoded, encoded_len);
	*signature_len = encoded_len;
	free(encoded);
	return XP_CA_OK;
}

int
xp_verify(xp_key_t key, enum xp_sign_algorithm algorithm,
	      enum xp_signature_encoding format, const void *data, size_t data_len,
	      const void *signature, size_t signature_len)
{
	if (key == NULL || signature == NULL || (data == NULL && data_len != 0)
	    || sign_family(algorithm) != EVP_PKEY_base_id(key->native)
	    || (format == XP_SIGNATURE_ENCODING_P1363
	        && sign_family(algorithm) != EVP_PKEY_EC)
	    || (format != XP_SIGNATURE_ENCODING_STANDARD && format != XP_SIGNATURE_ENCODING_P1363))
		return XP_CA_ERR_POLICY;
	const unsigned char *native = signature;
	size_t native_len = signature_len;
	unsigned char *converted = NULL;
	if (format == XP_SIGNATURE_ENCODING_P1363
	    && ecdsa_p1363_to_der(key->native, signature, signature_len,
	                         &converted, &native_len) != XP_CA_OK)
		return XP_CA_ERR_FORMAT;
	if (converted != NULL)
		native = converted;
	EVP_MD_CTX *context = EVP_MD_CTX_new();
	EVP_PKEY_CTX *parameters = NULL;
	int ok = context != NULL && EVP_DigestVerifyInit(
		context, &parameters, sign_md(algorithm), NULL, key->native) == 1;
	if (ok && sign_family(algorithm) == EVP_PKEY_RSA)
		ok = EVP_PKEY_CTX_set_rsa_padding(parameters, RSA_PKCS1_PADDING) == 1;
	if (ok)
		ok = EVP_DigestVerify(context, native, native_len, data, data_len) == 1;
	EVP_MD_CTX_free(context);
	free(converted);
	return ok ? XP_CA_OK : XP_CA_ERR_VERIFY;
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
xp_ca_crl_create(xp_ca_crl_t *out, xp_key_t key, xp_ca_cert_t issuer,
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
	if (!X509_CRL_sort(c) || !X509_CRL_sign(c, key->native, key_md(key->native))) {
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
