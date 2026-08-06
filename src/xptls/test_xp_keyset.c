#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "genwrap.h"
#include "xp_keyset.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s (%s)\n", \
										  __FILE__, __LINE__, #x, xp_crypto_status_string(status)); goto fail; } } while (0)

static int
secret(void *context, void *out, size_t size, size_t *len) {
	const char *value = context;
	size_t      required = strlen(value);
	if (len == NULL)
		return -1;
	*len = required;
	if (out == NULL)
		return 0;
	if (size < required)
		return -1;
	memcpy(out, value, required);
	return 0;
}

static int
exercise(const char *path, enum xp_keyset_format format,
         xp_key_t key, xp_ca_cert_t cert) {
	const char *            password = "keyset test password";
	int                     status = 0;
	struct xp_keyset_config config = {path, XP_KEYSET_CREATE, format,
		                              secret, (void *)password, NULL};
	xp_keyset_t             set = NULL;
	xp_key_t                loaded = NULL;
	xp_ca_cert_t *          chain = NULL;
	size_t                  chain_count = 0;
	unsigned char           first[32], second[32];
	size_t                  first_len = sizeof(first), second_len = sizeof(second);
	CHECK((status = xp_keyset_open(&set, &config)) == XP_CRYPTO_OK);
	CHECK((status = xp_keyset_add_private_key(set, "identity", key)) == XP_CRYPTO_OK);
	CHECK((status = xp_keyset_add_certificate_chain(set, "identity", &cert, 1)) == XP_CRYPTO_OK);
	CHECK((status = xp_keyset_close(set)) == XP_CRYPTO_OK);
	set = NULL;
	if (format == XP_KEYSET_FORMAT_PKCS12) {
		config.mode = XP_KEYSET_READ_ONLY;
		config.password_context = (void *)"wrong password";
		CHECK((status = xp_keyset_open(&set, &config))
		      == XP_CRYPTO_ERR_AUTHORIZATION && set == NULL);
		config.password_context = (void *)password;
	}
	config.mode = XP_KEYSET_READ_ONLY;
	CHECK((status = xp_keyset_open(&set, &config)) == XP_CRYPTO_OK);
	CHECK((status = xp_keyset_get_private_key(&loaded, set, "identity")) == XP_CRYPTO_OK);
	CHECK((status = xp_key_fingerprint_sha256(key, first, &first_len)) == XP_CRYPTO_OK);
	CHECK((status = xp_key_fingerprint_sha256(loaded, second, &second_len)) == XP_CRYPTO_OK);
	CHECK(first_len == second_len && memcmp(first, second, first_len) == 0);
	CHECK((status = xp_keyset_get_certificate_chain(&chain, &chain_count, set,
	                                                "identity")) == XP_CRYPTO_OK && chain_count == 1);
	CHECK((status = xp_keyset_delete(set, "identity",
	                                 XP_KEYSET_PRIVATE_KEY)) == XP_CRYPTO_ERR_READ_ONLY);
	xp_ca_cert_chain_free(chain, chain_count);
	chain = NULL;
	chain_count = 0;
	xp_key_release(loaded);
	loaded = NULL;
	xp_keyset_close(set);
	set = NULL;
	config.mode = XP_KEYSET_READ_WRITE;
	CHECK((status = xp_keyset_open(&set, &config)) == XP_CRYPTO_OK);
	if (format == XP_KEYSET_FORMAT_PKCS12) {
		CHECK((status = xp_keyset_delete(set, "identity",
		                                 XP_KEYSET_PRIVATE_KEY)) == XP_CRYPTO_OK);
		CHECK((status = xp_keyset_get_private_key(&loaded, set,
		                                          "identity")) == XP_CRYPTO_ERR_NOT_FOUND);
		xp_keyset_close(set);
		set = NULL;
		config.mode = XP_KEYSET_READ_ONLY;
		CHECK((status = xp_keyset_open(&set, &config)) == XP_CRYPTO_OK);
		CHECK((status = xp_keyset_get_certificate_chain(&chain, &chain_count,
		                                                set, "identity")) == XP_CRYPTO_OK
		      && chain_count == 1);
		xp_ca_cert_chain_free(chain, chain_count);
		chain = NULL;
		chain_count = 0;
		xp_keyset_close(set);
		set = NULL;
		config.mode = XP_KEYSET_READ_WRITE;
		CHECK((status = xp_keyset_open(&set, &config)) == XP_CRYPTO_OK);
		CHECK((status = xp_keyset_delete(set, "identity",
		                                 XP_KEYSET_CERTIFICATE_CHAIN)) == XP_CRYPTO_OK);
		xp_keyset_close(set);
		set = NULL;
		config.mode = XP_KEYSET_READ_ONLY;
		CHECK((status = xp_keyset_open(&set, &config)) == XP_CRYPTO_OK);
		CHECK((status = xp_keyset_get_certificate_chain(&chain, &chain_count,
		                                                set, "identity")) == XP_CRYPTO_ERR_NOT_FOUND);
	} else {
		CHECK((status = xp_keyset_delete(set, "identity",
		                                 XP_KEYSET_CERTIFICATE_CHAIN)) == XP_CRYPTO_OK);
		CHECK((status = xp_keyset_get_certificate_chain(&chain, &chain_count,
		                                                set, "identity")) == XP_CRYPTO_ERR_NOT_FOUND);
	}
	xp_keyset_close(set);
	remove(path);
	char lock[512];
	snprintf(lock, sizeof(lock), "%s.lock", path);
	remove(lock);
	return 0;
fail:
	xp_ca_cert_chain_free(chain, chain_count);
	xp_key_release(loaded);
	if (set)
		xp_keyset_close(set);
	remove(path);
	{
		char lock[512];
		snprintf(lock, sizeof(lock), "%s.lock", path);
		remove(lock);
	}
	return 1;
}

static int
exercise_fixture(const char *path) {
	const char *            password = getenv("XPTLS_PKCS12_FIXTURE_PASSWORD");
	if (password == NULL)
		password = "keyset test password";
	struct xp_keyset_config config = {
		.path = path,
		.mode = XP_KEYSET_READ_ONLY,
		.format = XP_KEYSET_FORMAT_PKCS12,
		.password = secret,
		.password_context = (void *)password,
	};
	xp_keyset_t             set = NULL;
	xp_key_t                key = NULL;
	xp_ca_cert_t *          chain = NULL;
	size_t                  count = 0;
	int                     status = xp_keyset_open(&set, &config);
	if (status == XP_CRYPTO_OK)
		status = xp_keyset_get_private_key(&key, set, "identity");
	if (status == XP_CRYPTO_OK)
		status = xp_keyset_get_certificate_chain(&chain, &count, set,
		                                         "identity");
	xp_ca_cert_chain_free(chain, count);
	xp_key_release(key);
	if (set != NULL)
		xp_keyset_close(set);
	if (status != XP_CRYPTO_OK || count == 0) {
		fprintf(stderr, "PKCS#12 fixture import failed: %s\n",
		        xp_crypto_status_string(status));
		return 1;
	}
	return 0;
}

static int
exercise_mismatch(const char *path, xp_key_t key,
                  xp_ca_cert_t wrong_certificate) {
	const char *            password = "keyset test password";
	struct xp_keyset_config config = {
		.path = path,
		.mode = XP_KEYSET_CREATE,
		.format = XP_KEYSET_FORMAT_MANIFEST,
		.password = secret,
		.password_context = (void *)password,
	};
	xp_keyset_t             set = NULL;
	int                     status = xp_keyset_open(&set, &config);
	if (status == XP_CRYPTO_OK)
		status = xp_keyset_add_private_key(set, "identity", key);
	if (status == XP_CRYPTO_OK
	    && xp_keyset_add_certificate_chain(set, "identity",
	                                       &wrong_certificate, 1) != XP_CRYPTO_ERR_VERIFY)
		status = XP_CRYPTO_ERR;
	if (set != NULL)
		xp_keyset_close(set);
	remove(path);
	char lock[512];
	snprintf(lock, sizeof(lock), "%s.lock", path);
	remove(lock);
	return status == XP_CRYPTO_OK ? 0 : 1;
}

int
main(void) {
	int                        status = 0, result = 1;
	xp_key_t                   key = NULL, wrong_key = NULL;
	xp_ca_cert_t               cert = NULL, wrong_cert = NULL;
	struct xp_key_spec         spec = {XP_KEY_RSA, 2048, XP_KEY_CURVE_NONE};
	const char *               dns[] = {"keyset.example.test"};
	struct xp_ca_issue_request request = {
		.subject = {
			.common_name = "keyset.example.test",
			.dns_names = dns,
			.dns_name_count = 1,
		},
		.policy = {
			.basic_constraints_critical = true,
			.key_usage_critical = true,
			.key_usage = XP_CA_KEY_USE_SIGN,
			.extended_key_usage = XP_CA_EKU_SERVER_AUTH,
			.path_length = -1,
		},
	};
	request.not_before = time(NULL) - 60;
	request.not_after = time(NULL) + 3600;
	CHECK((status = xp_key_generate(&key, &spec)) == XP_CRYPTO_OK);
	CHECK((status = xp_ca_cert_create_self_signed(&cert, key, &request)) == XP_CRYPTO_OK);
	CHECK((status = xp_key_generate(&wrong_key, &spec)) == XP_CRYPTO_OK);
	CHECK((status = xp_ca_cert_create_self_signed(&wrong_cert, wrong_key, &request)) == XP_CRYPTO_OK);
	char manifest[128], p12[128], mismatch[128];
	snprintf(manifest, sizeof(manifest), "xptls-keyset-%ld.manifest", (long)getpid());
	snprintf(p12, sizeof(p12), "xptls-keyset-%ld.p12", (long)getpid());
	snprintf(mismatch, sizeof(mismatch), "xptls-keyset-%ld-mismatch.manifest", (long)getpid());
	CHECK(exercise(manifest, XP_KEYSET_FORMAT_MANIFEST, key, cert) == 0);
	CHECK(exercise(p12, XP_KEYSET_FORMAT_PKCS12, key, cert) == 0);
	CHECK(exercise_mismatch(mismatch, key, wrong_cert) == 0);
	const char *fixture = getenv("XPTLS_PKCS12_FIXTURE");
	if (fixture != NULL)
		CHECK(exercise_fixture(fixture) == 0);
	result = 0;
fail:
	xp_ca_cert_free(wrong_cert);
	xp_ca_cert_free(cert);
	xp_key_release(wrong_key);
	xp_key_release(key);
	return result;
}
