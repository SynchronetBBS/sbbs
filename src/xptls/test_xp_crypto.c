#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "xp_ca.h"
#include "xp_digest.h"
#include "xp_sign.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, #x, xp_ca_last_error()); return 1; } } while (0)

static int password(void *context, void *out, size_t out_size, size_t *out_len)
{
	const char *value = context;
	size_t length = strlen(value);
	if (out_len == NULL) return -1;
	*out_len = length;
	if (out == NULL) return 0;
	if (out_size < length) return -1;
	memcpy(out, value, length);
	return 0;
}
static int rejected_password(void *context, void *out, size_t out_size, size_t *out_len)
{
	(void)context; (void)out; (void)out_size; (void)out_len;
	return -1;
}

static int matches_hex(const unsigned char *value, size_t length, const char *hex)
{
	if (strlen(hex) != length * 2) return 0;
	for (size_t i = 0; i < length; i++) {
		unsigned high = hex[i*2] <= '9' ? (unsigned)(hex[i*2]-'0') : (unsigned)(hex[i*2]-'a'+10);
		unsigned low = hex[i*2+1] <= '9' ? (unsigned)(hex[i*2+1]-'0') : (unsigned)(hex[i*2+1]-'a'+10);
		if (value[i] != (unsigned char)((high << 4) | low)) return 0;
	}
	return 1;
}

static int cert_equal(xp_ca_cert_t first, xp_ca_cert_t second)
{
	size_t first_len=0, second_len=0;
	if (xp_ca_cert_export_der(first, NULL, &first_len) != XP_CA_OK
	    || xp_ca_cert_export_der(second, NULL, &second_len) != XP_CA_OK
	    || first_len != second_len) return 0;
	unsigned char *a=malloc(first_len), *b=malloc(second_len);
	if (!a || !b) { free(a); free(b); return 0; }
	int equal = xp_ca_cert_export_der(first, a, &first_len) == XP_CA_OK
		&& xp_ca_cert_export_der(second, b, &second_len) == XP_CA_OK
		&& memcmp(a,b,first_len) == 0;
	free(a); free(b); return equal;
}

static int check_digest(void)
{
	static const unsigned char expected[32] = {
		0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
		0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad
	};
	unsigned char actual[64];
	unsigned char too_small[1];
	size_t length = sizeof(actual);
	size_t small = sizeof(too_small);
	CHECK(xp_digest(XP_DIGEST_SHA256, "abc", 3, too_small, &small) != 0 && small == 32);
	CHECK(xp_digest(XP_DIGEST_SHA256, "abc", 3, actual, &length) == 0);
	CHECK(length == sizeof(expected) && memcmp(actual, expected, length) == 0);
	length = sizeof(actual);
	CHECK(xp_digest(XP_DIGEST_SHA384, "abc", 3, actual, &length) == 0);
	CHECK(length == 48 && matches_hex(actual, length,
		"cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
		"8086072ba1e7cc2358baeca134c825a7"));
	xp_digest_t context = xp_digest_create(XP_DIGEST_SHA512);
	CHECK(context != NULL);
	CHECK(xp_digest_update(context, "a", 1) == 0);
	CHECK(xp_digest_update(context, "bc", 2) == 0);
	length = 0;
	CHECK(xp_digest_final(context, NULL, &length) == 0 && length == 64);
	length = sizeof(actual);
	CHECK(xp_digest_final(context, actual, &length) == 0 && length == 64);
	CHECK(matches_hex(actual, length,
		"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
		"2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"));
	xp_digest_free(context);
	return 0;
}

static int check_legacy_rsa(void)
{
	static const char vector[] = "xptls interoperability vector";
	static const unsigned char expected[] = {
		0xa0,0xff,0x63,0xb4,0xd6,0x91,0x19,0xdc,0xc5,0x0f,0x6c,0x74,0x17,0x0f,0xbc,0x61,
		0xde,0x41,0xcd,0xdc,0x80,0xa2,0x86,0x0a,0x67,0x4e,0x0f,0xd6,0x3a,0x12,0x46,0xe1,
		0x6f,0xb4,0x50,0x23,0x56,0x4d,0x17,0xcf,0x9b,0x19,0x82,0x98,0x27,0xcf,0xcb,0xd7,
		0xbd,0x92,0x23,0xca,0x6c,0x86,0x3c,0xfa,0xc7,0xdc,0x25,0xd0,0xe0,0x38,0x2a,0x6b,
		0x4c,0x15,0x60,0x64,0x09,0xb8,0x43,0xc2,0xb5,0x69,0x4f,0x0f,0x5f,0xdb,0x00,0x94,
		0xe1,0x81,0xc5,0xbd,0xb4,0x5d,0x27,0x0b,0xdb,0xc7,0xe7,0xec,0xb4,0x1a,0x20,0x62,
		0x82,0xfb,0xff,0x75,0x3c,0x0e,0xe9,0x39,0x60,0x82,0xba,0x87,0x26,0xce,0x4b,0x97,
		0x71,0xb1,0x63,0xa1,0x05,0x0f,0x31,0xb7,0x6a,0x6c,0x2f,0xd0,0x1e,0xad,0x7a,0x2b,
		0x2b,0x06,0xa4,0xf6,0xb4,0x72,0x48,0x56,0x39,0xc2,0xe7,0x0b,0x7e,0xe3,0xe0,0x7a,
		0x3d,0x9c,0x1f,0xe0,0xe6,0xc7,0x09,0xb5,0x55,0xb6,0x32,0xe4,0xa1,0xe5,0x00,0xfa,
		0x1d,0xcd,0xf9,0x63,0x00,0x65,0x45,0x6d,0x7d,0x6e,0x4a,0x84,0x0f,0x5c,0x44,0x20,
		0xb4,0xf3,0x02,0xb3,0x7d,0x2c,0x91,0xa3,0x06,0x3a,0xac,0x1a,0x56,0x07,0x72,0xf6,
		0xfc,0x32,0x3e,0xa8,0xd1,0x1b,0xd4,0xe7,0x68,0x75,0x4e,0xec,0x31,0x3b,0xed,0xb1,
		0xc3,0xe4,0x56,0x1d,0x1b,0x41,0xbf,0xd5,0x79,0xa9,0x29,0x15,0x62,0x5b,0x7e,0xc1,
		0x85,0xdb,0x39,0x8b,0x8a,0x86,0xe6,0xcb,0x39,0x8d,0x40,0x95,0x1b,0x38,0x2b,0xdc,
		0x69,0xed,0xc1,0x1f,0xff,0x18,0x16,0x1c,0x8c,0xb6,0xe3,0x92,0x46,0xa7,0xb1,0x44
	};
	FILE *file = fopen(TEST_PKCS1_PEM, "rb");
	CHECK(file != NULL);
	CHECK(fseek(file, 0, SEEK_END) == 0);
	long length = ftell(file);
	CHECK(length > 0 && fseek(file, 0, SEEK_SET) == 0);
	unsigned char *pem = malloc((size_t)length);
	CHECK(pem != NULL && fread(pem, 1, (size_t)length, file) == (size_t)length);
	fclose(file);
	xp_key_t key = NULL;
	CHECK(xp_key_import_private_pem(&key, pem, (size_t)length, NULL, NULL) == XP_CA_OK);
	struct xp_key_info info;
	CHECK(xp_key_get_info(key, &info) == XP_CA_OK);
	CHECK(info.spec.algorithm == XP_KEY_RSA && info.has_private && info.spec.bits == 2048);
	unsigned char actual[sizeof(expected)];
	size_t actual_len = sizeof(actual);
	CHECK(xp_sign(key, XP_SIGN_RSA_PKCS1_SHA256, XP_SIGNATURE_ENCODING_STANDARD,
		vector, sizeof(vector)-1, actual, &actual_len) == XP_CA_OK);
	CHECK(actual_len == sizeof(expected) && memcmp(actual, expected, sizeof(expected)) == 0);
	CHECK(xp_verify(key, XP_SIGN_RSA_PKCS1_SHA256, XP_SIGNATURE_ENCODING_STANDARD,
		vector, sizeof(vector)-1, expected, sizeof(expected)) == XP_CA_OK);
	xp_key_release(key);
	free(pem);
	return 0;
}

static int check_key_storage(void)
{
	char path[160];
	snprintf(path, sizeof(path), "xptls-key-store-%lu.pem",
		(unsigned long)time(NULL));
	remove(path);
	const struct xp_key_spec spec = {
		XP_KEY_RSA, 2048, XP_KEY_CURVE_NONE
	};
	struct xp_key_store_config store = {
		.policy = XP_KEY_STORAGE_FILE,
		.file_path = path,
		.authorize = password,
		.authorize_context = "stored-secret",
	};
	struct xp_key_store_capabilities capabilities;
	struct xp_key_store_config inline_authorization = {
		.policy = XP_KEY_STORAGE_NAMED,
		.store = "pkcs11",
		.store_uri = "pkcs11:token=test?pin-value=must-not-be-here",
	};
	CHECK(xp_key_store_query(&inline_authorization, &spec, &capabilities)
		== XP_CRYPTO_ERR_INVALID);
	CHECK(xp_key_store_query(&store, &spec, &capabilities) == XP_CRYPTO_OK);
	CHECK(capabilities.available
		&& capabilities.availability_status == XP_CRYPTO_OK
		&& capabilities.kind == XP_KEY_STORE_FILE
		&& (capabilities.operations & XP_KEY_STORE_CAN_GENERATE) != 0
		&& (capabilities.operations & XP_KEY_STORE_TRANSACTIONAL_DESTROY) != 0);
	xp_key_t generated = NULL, opened = NULL;
	CHECK(xp_key_generate_stored(&generated, &store, &spec) == XP_CRYPTO_OK);
	struct xp_key_info info;
	CHECK(xp_key_get_info(generated, &info) == XP_CRYPTO_OK
		&& info.storage == XP_KEY_STORE_FILE && info.exportable);
	size_t reference_len = 0;
	CHECK(xp_key_reference(generated, NULL, &reference_len) == XP_CRYPTO_OK
		&& reference_len != 0);
	unsigned char *reference = malloc(reference_len);
	CHECK(reference != NULL);
	size_t actual_reference_len = reference_len;
	CHECK(xp_key_reference(generated, reference, &actual_reference_len)
		== XP_CRYPTO_OK && actual_reference_len == reference_len);
	static const char malformed_reference[] = "xptls-keyref-v1:file:A";
	CHECK(xp_key_open_stored(&opened, &store, malformed_reference,
		sizeof(malformed_reference) - 1) == XP_CRYPTO_ERR_FORMAT
		&& opened == NULL);
	char alternate_path[sizeof(path) + 3];
	snprintf(alternate_path, sizeof(alternate_path), "./%s", path);
	store.file_path = alternate_path;
	struct xp_key_store_config wrong_password = store;
	wrong_password.authorize_context = "wrong-stored-secret";
	CHECK(xp_key_open_stored(&opened, &wrong_password, reference, reference_len)
		!= XP_CRYPTO_OK && opened == NULL);
	CHECK(xp_key_open_stored(&opened, &store, reference, reference_len)
		== XP_CRYPTO_OK);
	unsigned char first[32], second[32];
	size_t first_len = sizeof(first), second_len = sizeof(second);
	CHECK(xp_key_fingerprint_sha256(generated, first, &first_len) == XP_CRYPTO_OK);
	CHECK(xp_key_fingerprint_sha256(opened, second, &second_len) == XP_CRYPTO_OK);
	CHECK(first_len == sizeof(first) && second_len == sizeof(second)
		&& memcmp(first, second, sizeof(first)) == 0);
	second[0] ^= 1;
	CHECK(xp_key_destroy_stored(&store, reference, reference_len,
		second, sizeof(second)) == XP_CRYPTO_ERR_CONFLICT);
	xp_key_release(opened);
	opened = NULL;
	CHECK(xp_key_open_stored(&opened, &store, reference, reference_len)
		== XP_CRYPTO_OK);
	xp_key_release(opened);
	CHECK(xp_key_destroy_stored(&store, reference, reference_len,
		first, sizeof(first)) == XP_CRYPTO_OK);
	CHECK(xp_key_open_stored(&opened, &store, reference, reference_len)
		== XP_CRYPTO_ERR_NOT_FOUND && opened == NULL);

	/* Import is a real encrypted-file transaction and produces another
	 * stable, reopenable reference rather than aliasing the source handle. */
	xp_key_t source = NULL;
	CHECK(xp_key_generate(&source, &spec) == XP_CRYPTO_OK);
	CHECK(xp_key_import_stored(&opened, &store, source) == XP_CRYPTO_OK);
	size_t imported_reference_len = 0;
	CHECK(xp_key_reference(opened, NULL, &imported_reference_len)
		== XP_CRYPTO_OK && imported_reference_len != 0);
	unsigned char *imported_reference = malloc(imported_reference_len);
	CHECK(imported_reference != NULL);
	CHECK(xp_key_reference(opened, imported_reference, &imported_reference_len)
		== XP_CRYPTO_OK);
	first_len = sizeof(first);
	second_len = sizeof(second);
	CHECK(xp_key_fingerprint_sha256(source, first, &first_len) == XP_CRYPTO_OK);
	CHECK(xp_key_fingerprint_sha256(opened, second, &second_len) == XP_CRYPTO_OK);
	CHECK(memcmp(first, second, sizeof(first)) == 0);
	xp_key_release(opened);
	opened = NULL;
	CHECK(xp_key_destroy_stored(&store, imported_reference,
		imported_reference_len, first, sizeof(first)) == XP_CRYPTO_OK);
	xp_key_release(source);
	free(imported_reference);

	/* AUTO falls back only because the configured hardware probe is
	 * unavailable; an explicit hardware policy fails closed. */
	store.policy = XP_KEY_STORAGE_AUTO;
	store.store = "tpm2";
	store.store_uri = "tpm2:device=missing";
	CHECK(xp_key_generate_stored(&opened, &store, &spec) == XP_CRYPTO_OK);
	size_t fallback_reference_len = 0;
	CHECK(xp_key_reference(opened, NULL, &fallback_reference_len) == XP_CRYPTO_OK);
	xp_key_release(opened);
	opened = NULL;
	store.policy = XP_KEY_STORAGE_NAMED;
	CHECK(xp_key_generate_stored(&opened, &store, &spec)
		== XP_CRYPTO_ERR_UNAVAILABLE && opened == NULL);
	remove(path);
	xp_key_release(generated);
	free(reference);
	return 0;
}

static int check_pkcs11_fixture(void)
{
	const char *uri = getenv("XPTLS_TEST_PKCS11_URI");
	if (uri == NULL || uri[0] == 0)
		return 0;
	const char *pin = getenv("XPTLS_TEST_PKCS11_PIN");
	if (pin == NULL)
		pin = "";
	const struct xp_key_spec spec = {
		XP_KEY_RSA, 2048, XP_KEY_CURVE_NONE
	};
	struct xp_key_store_config store = {
		.policy = XP_KEY_STORAGE_NAMED,
		.store = "pkcs11",
		.store_uri = uri,
		.authorize = password,
		.authorize_context = (void *)pin,
	};
	struct xp_key_store_capabilities capabilities;
	xp_key_t key = NULL;
	unsigned char *reference = NULL;
	size_t reference_len = 0;
	unsigned char fingerprint[32];
	size_t fingerprint_len = sizeof(fingerprint);
	unsigned char *signature = NULL;
	bool object_exists = false;
	bool fingerprint_valid = false;
	int result = 1;
#define FIXTURE_CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "%s:%d: PKCS11 fixture: %s: %s\n", __FILE__, \
		    __LINE__, #condition, xp_crypto_last_error()); \
		goto done; \
	} \
} while (0)
	FIXTURE_CHECK(xp_key_store_query(&store, &spec, &capabilities)
		== XP_CRYPTO_OK);
	FIXTURE_CHECK(capabilities.available
		&& capabilities.kind == XP_KEY_STORE_PKCS11
		&& (capabilities.operations & (XP_KEY_STORE_CAN_GENERATE
		    | XP_KEY_STORE_CAN_OPEN | XP_KEY_STORE_CAN_SIGN
		    | XP_KEY_STORE_CAN_DESTROY))
		    == (XP_KEY_STORE_CAN_GENERATE | XP_KEY_STORE_CAN_OPEN
		        | XP_KEY_STORE_CAN_SIGN | XP_KEY_STORE_CAN_DESTROY));
	FIXTURE_CHECK(xp_key_generate_stored(&key, &store, &spec) == XP_CRYPTO_OK);
	object_exists = true;
	FIXTURE_CHECK(xp_key_reference(key, NULL, &reference_len) == XP_CRYPTO_OK
		&& reference_len != 0);
	reference = malloc(reference_len);
	FIXTURE_CHECK(reference != NULL);
	FIXTURE_CHECK(xp_key_reference(key, reference, &reference_len)
		== XP_CRYPTO_OK);
	FIXTURE_CHECK(xp_key_fingerprint_sha256(key, fingerprint,
		&fingerprint_len) == XP_CRYPTO_OK && fingerprint_len == 32);
	fingerprint_valid = true;
	struct xp_key_info info;
	FIXTURE_CHECK(xp_key_get_info(key, &info) == XP_CRYPTO_OK
		&& info.storage == XP_KEY_STORE_PKCS11 && !info.exportable);
	size_t private_len = 0;
	FIXTURE_CHECK(xp_key_export_private_pem(key, NULL, NULL, NULL,
		&private_len) == XP_CRYPTO_ERR_NOT_EXPORTABLE);
	xp_key_release(key);
	key = NULL;
	FIXTURE_CHECK(xp_key_open_stored(&key, &store, reference, reference_len)
		== XP_CRYPTO_OK);
	static const char message[] = "PKCS11 provider-neutral signing fixture";
	size_t signature_len = 0;
	FIXTURE_CHECK(xp_sign(key, XP_SIGN_RSA_PKCS1_SHA256,
		XP_SIGNATURE_ENCODING_STANDARD, message, sizeof(message) - 1,
		NULL, &signature_len) == XP_CRYPTO_OK && signature_len != 0);
	signature = malloc(signature_len);
	FIXTURE_CHECK(signature != NULL);
	FIXTURE_CHECK(xp_sign(key, XP_SIGN_RSA_PKCS1_SHA256,
		XP_SIGNATURE_ENCODING_STANDARD, message, sizeof(message) - 1,
		signature, &signature_len) == XP_CRYPTO_OK);
	FIXTURE_CHECK(xp_verify(key, XP_SIGN_RSA_PKCS1_SHA256,
		XP_SIGNATURE_ENCODING_STANDARD, message, sizeof(message) - 1,
		signature, signature_len) == XP_CRYPTO_OK);
	xp_key_release(key);
	key = NULL;
	FIXTURE_CHECK(xp_key_destroy_stored(&store, reference, reference_len,
		fingerprint, fingerprint_len) == XP_CRYPTO_OK);
	object_exists = false;
	result = 0;
done:
	xp_key_release(key);
	if (object_exists && reference != NULL && fingerprint_valid)
		(void)xp_key_destroy_stored(&store, reference, reference_len,
			fingerprint, fingerprint_len);
	free(signature);
	free(reference);
#undef FIXTURE_CHECK
	return result;
}

static int exercise_key(const struct xp_key_spec *spec,
	enum xp_key_algorithm family, enum xp_sign_algorithm scheme,
	size_t p1363_size)
{
	static const char message[] = "provider-neutral signature test";
	const char *names[] = { "example.test", "www.example.test" };
	struct xp_ca_identity identity = { "example.test", names, 2 };
	struct xp_ca_issue_request request = {
		.subject = { "example.test", names, 2 },
		.policy = { false, true, true, XP_CA_KEY_USE_SIGN,
			XP_CA_EKU_SERVER_AUTH, -1, NULL },
		.not_before = time(NULL) - 60,
		.not_after = time(NULL) + 3600,
	};
	xp_key_t key = NULL, imported = NULL, public_key = NULL, cert_key = NULL;
	xp_ca_csr_t csr = NULL;
	xp_ca_cert_t cert = NULL, second_cert = NULL;
	struct xp_key_info info;
	unsigned char *encoded = NULL, *signature = NULL;
	size_t encoded_len = 0, signature_len = 0;

	CHECK(xp_key_generate(&key, spec) == XP_CA_OK);
	CHECK(xp_key_get_info(key, &info) == XP_CA_OK);
	CHECK(info.spec.algorithm == family && info.has_private);
	CHECK(xp_key_export_spki_der(key, NULL, &encoded_len) == XP_CA_OK && encoded_len != 0);
	size_t small_len = encoded_len - 1;
	encoded = malloc(small_len);
	CHECK(encoded != NULL);
	CHECK(xp_key_export_spki_der(key, encoded, &small_len) == XP_CA_ERR
		&& small_len == encoded_len);
	free(encoded); encoded = NULL;
	encoded = malloc(encoded_len);
	CHECK(encoded != NULL);
	CHECK(xp_key_export_spki_der(key, encoded, &encoded_len) == XP_CA_OK);
	CHECK(xp_key_import_spki_der(&public_key, encoded, encoded_len) == XP_CA_OK);
	CHECK(xp_key_get_info(public_key, &info) == XP_CA_OK && !info.has_private);
	free(encoded); encoded = NULL;

	CHECK(xp_sign(key, scheme, XP_SIGNATURE_ENCODING_STANDARD, message, sizeof(message)-1,
		NULL, &signature_len) == XP_CA_OK && signature_len != 0);
	signature = malloc(signature_len);
	CHECK(signature != NULL);
	CHECK(xp_sign(key, scheme, XP_SIGNATURE_ENCODING_STANDARD, message, sizeof(message)-1,
		signature, &signature_len) == XP_CA_OK);
	CHECK(xp_verify(public_key, scheme, XP_SIGNATURE_ENCODING_STANDARD,
		message, sizeof(message)-1, signature, signature_len) == XP_CA_OK);
	signature[0] ^= 1;
	CHECK(xp_verify(public_key, scheme, XP_SIGNATURE_ENCODING_STANDARD,
		message, sizeof(message)-1, signature, signature_len) == XP_CA_ERR_VERIFY);
	signature[0] ^= 1;
	CHECK(xp_sign(public_key, scheme, XP_SIGNATURE_ENCODING_STANDARD, message,
		sizeof(message)-1, NULL, &encoded_len) == XP_CA_ERR_POLICY);
	free(signature); signature = NULL;

	if (family == XP_KEY_ECDSA) {
		signature_len = 0;
		CHECK(xp_sign(key, scheme, XP_SIGNATURE_ENCODING_P1363, message,
			sizeof(message)-1, NULL, &signature_len) == XP_CA_OK);
		CHECK(signature_len == p1363_size);
		signature = malloc(signature_len);
		CHECK(signature != NULL);
		CHECK(xp_sign(key, scheme, XP_SIGNATURE_ENCODING_P1363, message,
			sizeof(message)-1, signature, &signature_len) == XP_CA_OK);
		CHECK(xp_verify(public_key, scheme, XP_SIGNATURE_ENCODING_P1363, message,
			sizeof(message)-1, signature, signature_len) == XP_CA_OK);
		free(signature); signature = NULL;
		size_t xlen=0, ylen=0;
		CHECK(xp_key_get_ec_public(key, NULL, &xlen, NULL, &ylen) == XP_CA_OK);
		CHECK(xlen * 2 == p1363_size && ylen == xlen);
	} else if (family == XP_KEY_RSA) {
		size_t nlen=0, elen=0;
		CHECK(xp_key_get_rsa_public(key, NULL, &nlen, NULL, &elen) == XP_CA_OK);
		CHECK(nlen >= 256 && elen != 0);
	}

	CHECK(xp_key_export_private_pem(key, NULL, NULL, NULL, &encoded_len) == XP_CA_OK);
	encoded = malloc(encoded_len);
	CHECK(encoded != NULL);
	CHECK(xp_key_export_private_pem(key, NULL, NULL, encoded, &encoded_len) == XP_CA_OK);
	CHECK(xp_key_import_private_pem(&imported, encoded, encoded_len, NULL, NULL) == XP_CA_OK);
	xp_key_release(imported); imported = NULL; free(encoded); encoded = NULL;
	encoded_len = 0;
	CHECK(xp_key_export_private_pem(key, rejected_password, NULL, NULL, &encoded_len)
		== XP_CA_ERR);
	CHECK(xp_key_export_private_pem(key, password, "secret", NULL, &encoded_len) == XP_CA_OK);
	encoded = malloc(encoded_len);
	CHECK(encoded != NULL);
	CHECK(xp_key_export_private_pem(key, password, "secret", encoded, &encoded_len) == XP_CA_OK);
	CHECK(xp_key_import_private_pem(&imported, encoded, encoded_len, password, "wrong") != XP_CA_OK);
	CHECK(imported == NULL);
	CHECK(xp_key_import_private_pem(&imported, encoded, encoded_len, password, "secret") == XP_CA_OK);
	xp_key_release(imported); imported = NULL; free(encoded); encoded = NULL;

	CHECK(xp_ca_csr_create_with_identity(&csr, key, &identity) == XP_CA_OK);
	CHECK(xp_ca_csr_verify(csr) == XP_CA_OK);
	const char *bad_names[] = { "bad name" };
	struct xp_ca_identity bad_identity = { "example.test", bad_names, 1 };
	xp_ca_csr_t rejected = NULL;
	CHECK(xp_ca_csr_create_with_identity(&rejected, key, &bad_identity)
		== XP_CA_ERR_POLICY && rejected == NULL);
	CHECK(xp_ca_cert_create_self_signed(&cert, key, &request) == XP_CA_OK);
	time_t from=0, to=0;
	CHECK(xp_ca_cert_get_validity(cert, &from, &to) == XP_CA_OK);
	CHECK(from == request.not_before && to == request.not_after);
	CHECK(xp_ca_cert_get_public_key(&cert_key, cert) == XP_CA_OK);
	CHECK(xp_verify(cert_key, scheme, XP_SIGNATURE_ENCODING_STANDARD,
		message, sizeof(message)-1, (const unsigned char *)"bad", 3) == XP_CA_ERR_VERIFY);
	xp_key_release(cert_key);
	const char *second_names[] = { "second.example.test" };
	request.subject.common_name = second_names[0];
	request.subject.dns_names = second_names;
	request.subject.dns_name_count = 1;
	CHECK(xp_ca_cert_create_self_signed(&second_cert, key, &request) == XP_CA_OK);
	const xp_ca_cert_t chain[] = { cert, second_cert };
	encoded_len = 0;
	CHECK(xp_ca_cert_chain_export_pem(chain, 2, NULL, &encoded_len) == XP_CA_OK);
	encoded = malloc(encoded_len + 4);
	CHECK(encoded != NULL);
	CHECK(xp_ca_cert_chain_export_pem(chain, 2, encoded, &encoded_len) == XP_CA_OK);
	xp_ca_cert_t *parsed = NULL; size_t count = 0;
	CHECK(xp_ca_cert_chain_import_pem(&parsed, &count, encoded, encoded_len) == XP_CA_OK);
	CHECK(count == 2 && cert_equal(parsed[0], cert) && cert_equal(parsed[1], second_cert));
	xp_ca_cert_chain_free(parsed, count);
	memcpy(encoded + encoded_len, "junk", 4);
	CHECK(xp_ca_cert_chain_import_pem(&parsed, &count, encoded, encoded_len + 4) == XP_CA_ERR_FORMAT);
	free(encoded);
	xp_ca_cert_free(second_cert); xp_ca_cert_free(cert); xp_ca_csr_free(csr);
	xp_key_release(public_key); xp_key_release(key);
	return 0;
}

int main(void)
{
	static const struct xp_key_spec ed25519 = { XP_KEY_ED25519, 0, XP_KEY_CURVE_NONE };
	static const struct xp_key_spec rsa2048 = { XP_KEY_RSA, 2048, XP_KEY_CURVE_NONE };
	static const struct xp_key_spec rsa3072 = { XP_KEY_RSA, 3072, XP_KEY_CURVE_NONE };
	static const struct xp_key_spec rsa4096 = { XP_KEY_RSA, 4096, XP_KEY_CURVE_NONE };
	static const struct xp_key_spec p256 = { XP_KEY_ECDSA, 0, XP_KEY_CURVE_P256 };
	static const struct xp_key_spec p384 = { XP_KEY_ECDSA, 0, XP_KEY_CURVE_P384 };
	static const struct xp_key_spec p521 = { XP_KEY_ECDSA, 0, XP_KEY_CURVE_P521 };
	CHECK(check_digest() == 0);
	CHECK(check_legacy_rsa() == 0);
	CHECK(check_key_storage() == 0);
	CHECK(check_pkcs11_fixture() == 0);
	CHECK(exercise_key(&ed25519, XP_KEY_ED25519, XP_SIGN_ED25519, 0) == 0);
	CHECK(exercise_key(&rsa2048, XP_KEY_RSA, XP_SIGN_RSA_PKCS1_SHA256, 0) == 0);
	CHECK(exercise_key(&rsa3072, XP_KEY_RSA, XP_SIGN_RSA_PKCS1_SHA384, 0) == 0);
	CHECK(exercise_key(&rsa4096, XP_KEY_RSA, XP_SIGN_RSA_PKCS1_SHA512, 0) == 0);
	CHECK(exercise_key(&p256, XP_KEY_ECDSA, XP_SIGN_ECDSA_SHA256, 64) == 0);
	CHECK(exercise_key(&p384, XP_KEY_ECDSA, XP_SIGN_ECDSA_SHA384, 96) == 0);
	CHECK(exercise_key(&p521, XP_KEY_ECDSA, XP_SIGN_ECDSA_SHA512, 132) == 0);
	return 0;
}
