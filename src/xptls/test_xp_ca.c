#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "genwrap.h"
#include "xp_ca.h"

#define CHECK(condition)                                                        \
	do {                                                                        \
		if (!(condition)) {                                                     \
			fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__,             \
			        #condition, xp_ca_last_error());                              \
			return 1;                                                           \
		}                                                                       \
	} while (0)

int
main(void)
{
	static const struct xp_key_spec ed25519 = {
		XP_KEY_ED25519, 0, XP_KEY_CURVE_NONE
	};
	const char *root_names[] = { "ca.example.test" };
	const char *intermediate_names[] = { "issuer.ca.example.test" };
	const char *child_names[] = { "node.ca.example.test" };
	const char *root_crl_url = "https://ca.example.test/root.crl";
	const char *intermediate_crl_url =
		"https://issuer.ca.example.test/ca.crl";
	const char *duplicate_child_names[] = {
		"node.ca.example.test",
		"node.ca.example.test",
	};
	struct xp_ca_issue_request request = {
		.subject = { "ca.example.test", root_names, 1 },
		.policy = {
			true,
			true,
			true,
			XP_CA_KEY_USE_CERT_SIGN | XP_CA_KEY_USE_CRL_SIGN,
			XP_CA_EKU_NONE,
			-1,
			NULL,
		},
	};
	struct xp_ca_identity expected = { child_names[0], child_names, 1 };
	struct xp_ca_identity duplicate_expected = {
		child_names[0],
		duplicate_child_names,
		2,
	};
	struct xp_ca_validation_policy validation = { 0 };
	struct xp_ca_validation_policy revoked = { 0 };
	struct xp_ca_certificate_policy wrong_policy;
	struct xp_ca_crl_info crl_info;
	struct xp_ca_crl_info imported_crl_info;
	struct xp_ca_crl_request root_crl_request;
	struct xp_ca_crl_request intermediate_crl_request;
	xp_key_t root_key = NULL;
	xp_key_t alternate_root_key = NULL;
	xp_key_t intermediate_key = NULL;
	xp_key_t child_key = NULL;
	xp_key_t reloaded = NULL;
	xp_ca_cert_t root = NULL;
	xp_ca_cert_t alternate_root = NULL;
	xp_ca_cert_t intermediate = NULL;
	xp_ca_cert_t child = NULL;
	xp_ca_cert_t imported_child = NULL;
	xp_ca_cert_t rejected_cert = NULL;
	xp_ca_csr_t csr = NULL;
	xp_ca_csr_t imported_csr = NULL;
	xp_ca_csr_t intermediate_csr = NULL;
	xp_ca_csr_t rich_csr = NULL;
	xp_ca_crl_t root_crl = NULL;
	xp_ca_crl_t crl = NULL;
	xp_ca_crl_t imported_crl = NULL;
	xp_ca_crl_t updated_crl = NULL;
	xp_ca_crl_t revoked_root_crl = NULL;
	xp_ca_crl_t rejected_crl = NULL;
	unsigned char der[4096];
	size_t der_len;
	size_t encoded_len;
	time_t now = time(NULL);
	char key_path[128];
	int status;

	CHECK(snprintf(key_path, sizeof(key_path), "xptls-ca-test-%ld.pem",
	               (long)getpid()) < (int)sizeof(key_path));
	request.not_before = now - 60;
	request.not_after = now + 3600;

	CHECK(xp_key_generate(&root_key, &ed25519) == XP_CA_OK);
	CHECK(xp_key_save_private_pem_file(root_key, key_path, "") == XP_CA_ERR);
	CHECK(xp_key_save_private_pem_file(root_key, key_path, "test-password") == XP_CA_OK);
	reloaded = root_key;
	CHECK(xp_key_load_private_pem_file(&reloaded, key_path, "wrong-password") != XP_CA_OK);
	CHECK(reloaded == NULL);
	CHECK(xp_key_load_private_pem_file(&reloaded, key_path, "test-password") == XP_CA_OK);
	xp_key_release(reloaded);
	reloaded = NULL;
	CHECK(xp_key_save_private_pem_file(root_key, key_path, "replacement-password") == XP_CA_OK);
	CHECK(xp_key_load_private_pem_file(&reloaded, key_path, "test-password") != XP_CA_OK);
	CHECK(reloaded == NULL);
	CHECK(xp_key_load_private_pem_file(&reloaded, key_path, "replacement-password") == XP_CA_OK);
	xp_key_release(reloaded);
	reloaded = NULL;
#if defined(__unix__)
	struct stat key_stat;

	CHECK(stat(key_path, &key_stat) == 0);
	CHECK((key_stat.st_mode & 0777) == 0600);
#endif
	CHECK(xp_key_delete_private_pem_file(key_path) == XP_CA_OK);

	CHECK(xp_ca_cert_create_self_signed(&root, root_key, &request) == XP_CA_OK);
	CHECK(xp_key_generate(&alternate_root_key, &ed25519) == XP_CA_OK);
	CHECK(xp_ca_cert_create_self_signed(
		&alternate_root, alternate_root_key, &request) == XP_CA_OK);

	CHECK(xp_key_generate(&intermediate_key, &ed25519) == XP_CA_OK);
	CHECK(xp_ca_csr_create(&intermediate_csr, intermediate_key) == XP_CA_OK);
	request.subject.common_name = intermediate_names[0];
	request.subject.dns_names = intermediate_names;
	request.policy.crl_distribution_point = root_crl_url;
	CHECK(xp_ca_cert_issue(&intermediate, root_key, root, intermediate_csr,
	                       &request) == XP_CA_OK);

	CHECK(xp_key_generate(&child_key, &ed25519) == XP_CA_OK);
	const unsigned char extension_value[] = { 0x05, 0x00 };
	const struct xp_ca_extension extension = {
		"1.2.3.4", true, extension_value, sizeof(extension_value)
	};
	struct xp_ca_csr_request rich_request = {
		.subject = { child_names[0], child_names, 1,
			"Synchronet", "Testing", "US", "Michigan", "Detroit",
			"test@example.test" },
		.extensions = &extension,
		.extension_count = 1,
	};
	CHECK(xp_ca_csr_create_request(&rich_csr, child_key, &rich_request) == XP_CA_OK);
	CHECK(xp_ca_csr_verify(rich_csr) == XP_CA_OK);
	xp_key_t csr_public = NULL;
	CHECK(xp_ca_csr_get_public_key(&csr_public, rich_csr) == XP_CA_OK);
	xp_key_release(csr_public);
	encoded_len = 0;
	CHECK(xp_ca_csr_export(rich_csr, XP_CA_ENCODING_PEM, NULL, &encoded_len) == XP_CA_OK);
	unsigned char *csr_pem = malloc(encoded_len);
	CHECK(csr_pem != NULL);
	CHECK(xp_ca_csr_export(rich_csr, XP_CA_ENCODING_PEM, csr_pem, &encoded_len) == XP_CA_OK);
	CHECK(xp_ca_csr_import(&imported_csr, XP_CA_ENCODING_PEM, csr_pem, encoded_len) == XP_CA_OK);
	free(csr_pem);
	CHECK(xp_ca_csr_verify(imported_csr) == XP_CA_OK);
	xp_ca_csr_free(imported_csr); imported_csr = NULL;
	CHECK(xp_ca_csr_create(&csr, child_key) == XP_CA_OK);
	der_len = sizeof(der);
	CHECK(xp_ca_csr_export_der(csr, der, &der_len) == XP_CA_OK);
	encoded_len = 0;
	CHECK(xp_ca_csr_export_der(csr, NULL, &encoded_len) == XP_CA_OK);
	CHECK(encoded_len == der_len);
	CHECK(der_len < sizeof(der));
	der[der_len] = 0;
	CHECK(xp_ca_csr_import_der(&imported_csr, der, der_len + 1)
	      == XP_CA_ERR_FORMAT);
	CHECK(imported_csr == NULL);
	CHECK(xp_ca_csr_import_der(&imported_csr, der, der_len) == XP_CA_OK);
	CHECK(xp_ca_csr_verify(imported_csr) == XP_CA_OK);
	xp_ca_csr_free(imported_csr);
	imported_csr = NULL;
	der[der_len - 1] ^= 1;
	status = xp_ca_csr_import_der(&imported_csr, der, der_len);
	CHECK(status == XP_CA_OK || status == XP_CA_ERR_FORMAT);
	if (status == XP_CA_OK) {
		CHECK(xp_ca_csr_verify(imported_csr) == XP_CA_ERR_VERIFY);
		xp_ca_csr_free(imported_csr);
	}
	imported_csr = NULL;
	der[der_len - 1] ^= 1;

	request.subject.common_name = child_names[0];
	request.subject.dns_names = child_names;
	request.subject.organization = "Synchronet";
	request.subject.organizational_unit = "Testing";
	request.subject.country = "US";
	request.subject.state_or_province = "Michigan";
	request.subject.locality = "Detroit";
	request.subject.email_address = "test@example.test";
	request.policy.is_ca = false;
	request.policy.key_usage = XP_CA_KEY_USE_SIGN;
	request.policy.extended_key_usage = XP_CA_EKU_SERVER_AUTH;
	request.policy.crl_distribution_point = intermediate_crl_url;
	CHECK(xp_ca_cert_issue(&rejected_cert, alternate_root_key, intermediate, csr,
	                       &request) == XP_CA_ERR_POLICY);
	CHECK(rejected_cert == NULL);
	request.not_after++;
	CHECK(xp_ca_cert_issue(&rejected_cert, intermediate_key, intermediate, csr,
	                       &request) == XP_CA_ERR_POLICY);
	CHECK(rejected_cert == NULL);
	request.not_after--;
	request.policy.path_length = 0;
	CHECK(xp_ca_cert_issue(&rejected_cert, intermediate_key, intermediate, csr,
	                       &request) == XP_CA_ERR_POLICY);
	CHECK(rejected_cert == NULL);
	request.policy.path_length = -1;
	request.subject.dns_names = duplicate_child_names;
	request.subject.dns_name_count = 2;
	CHECK(xp_ca_cert_issue(&rejected_cert, intermediate_key, intermediate, csr,
	                       &request) == XP_CA_ERR_POLICY);
	CHECK(rejected_cert == NULL);
	request.subject.dns_names = child_names;
	request.subject.dns_name_count = 1;
	request.policy.crl_distribution_point = "https://bad url/ca.crl";
	CHECK(xp_ca_cert_issue(&rejected_cert, intermediate_key, intermediate, csr,
	                       &request) == XP_CA_ERR_POLICY);
	CHECK(rejected_cert == NULL);
	request.policy.crl_distribution_point = intermediate_crl_url;
	CHECK(xp_ca_cert_issue(&child, intermediate_key, intermediate, csr,
	                       &request) == XP_CA_OK);
	struct xp_ca_cert_info cert_info;
	CHECK(xp_ca_cert_get_info(child, &cert_info) == XP_CA_OK);
	CHECK(cert_info.version == 3 && !cert_info.is_ca
	      && (cert_info.key_usage & XP_CA_KEY_USE_SIGN)
	      && (cert_info.extended_key_usage & XP_CA_EKU_SERVER_AUTH));
	size_t value_count = 0, value_len = 0;
	CHECK(xp_ca_cert_get_name_count(child, XP_CA_NAME_SUBJECT,
		XP_CA_NAME_ORGANIZATION, &value_count) == XP_CA_OK && value_count == 1);
	CHECK(xp_ca_cert_get_name(child, XP_CA_NAME_SUBJECT,
		XP_CA_NAME_ORGANIZATION, 0, NULL, &value_len) == XP_CA_OK);
	char name_value[64]; size_t name_len = sizeof(name_value);
	CHECK(xp_ca_cert_get_name(child, XP_CA_NAME_SUBJECT,
		XP_CA_NAME_ORGANIZATION, 0, name_value, &name_len) == XP_CA_OK);
	CHECK(name_len == strlen("Synchronet") && memcmp(name_value, "Synchronet", name_len) == 0);
	CHECK(xp_ca_cert_get_san_count(child, XP_CA_SAN_DNS, &value_count) == XP_CA_OK && value_count == 1);
	CHECK(xp_ca_cert_verify_signature(child, intermediate) == XP_CA_OK);
	CHECK(xp_ca_cert_verify_signature(child, root) == XP_CA_ERR_VERIFY);
	unsigned char fingerprint[64]; size_t fingerprint_len = sizeof(fingerprint);
	CHECK(xp_ca_cert_get_fingerprint(child, XP_DIGEST_SHA512,
		fingerprint, &fingerprint_len) == XP_CA_OK && fingerprint_len == 64);

	const xp_ca_cert_t chain[] = { intermediate };
	const xp_ca_cert_t bundle[] = { child, intermediate, root };
	encoded_len = 0;
	CHECK(xp_ca_cert_bundle_export(bundle, 3, XP_CA_ENCODING_PKCS7_DER,
		NULL, &encoded_len) == XP_CA_OK);
	unsigned char *pkcs7 = malloc(encoded_len);
	CHECK(pkcs7 != NULL);
	CHECK(xp_ca_cert_bundle_export(bundle, 3, XP_CA_ENCODING_PKCS7_DER,
		pkcs7, &encoded_len) == XP_CA_OK);
	xp_ca_cert_t *imported_bundle = NULL; size_t imported_bundle_count = 0;
	CHECK(xp_ca_cert_bundle_import(&imported_bundle, &imported_bundle_count,
		XP_CA_ENCODING_PKCS7_DER, pkcs7, encoded_len) == XP_CA_OK);
	free(pkcs7);
	CHECK(imported_bundle_count == 3);
	xp_ca_cert_chain_free(imported_bundle, imported_bundle_count);

	CHECK(xp_ca_cert_validate(child, chain, 1, root, NULL, 0, NULL) == XP_CA_OK);
	CHECK(xp_ca_cert_validate(child, chain, 1, alternate_root, NULL, 0, NULL)
	      == XP_CA_ERR_VERIFY);
	der_len = sizeof(der);
	CHECK(xp_ca_cert_export_der(child, der, &der_len) == XP_CA_OK);
	encoded_len = 0;
	CHECK(xp_ca_cert_export_der(child, NULL, &encoded_len) == XP_CA_OK);
	CHECK(encoded_len == der_len);
	CHECK(der_len < sizeof(der));
	der[der_len] = 0;
	CHECK(xp_ca_cert_import_der(&imported_child, der, der_len + 1)
	      == XP_CA_ERR_FORMAT);
	CHECK(imported_child == NULL);
	CHECK(xp_ca_cert_import_der(&imported_child, der, der_len) == XP_CA_OK);
	CHECK(xp_ca_cert_validate(imported_child, chain, 1, root, NULL, 0, NULL)
	      == XP_CA_OK);
	xp_ca_cert_free(imported_child);
	imported_child = NULL;

	validation.expected_identity = &expected;
	validation.required_policy = &request.policy;
	validation.validation_time = now;
	CHECK(xp_ca_cert_validate(child, chain, 1, root, NULL, 0, &validation)
	      == XP_CA_OK);
	expected.common_name = "wrong.example.test";
	CHECK(xp_ca_cert_validate(child, chain, 1, root, NULL, 0, &validation)
	      == XP_CA_ERR_VERIFY);
	expected.common_name = child_names[0];
	validation.expected_identity = &duplicate_expected;
	CHECK(xp_ca_cert_validate(child, chain, 1, root, NULL, 0, &validation)
	      == XP_CA_ERR_VERIFY);
	validation.expected_identity = &expected;
	wrong_policy = request.policy;
	wrong_policy.extended_key_usage |= XP_CA_EKU_CLIENT_AUTH;
	validation.required_policy = &wrong_policy;
	CHECK(xp_ca_cert_validate(child, chain, 1, root, NULL, 0, &validation)
	      == XP_CA_ERR_VERIFY);
	wrong_policy = request.policy;
	wrong_policy.crl_distribution_point = "https://wrong.example.test/ca.crl";
	validation.required_policy = &wrong_policy;
	CHECK(xp_ca_cert_validate(child, chain, 1, root, NULL, 0, &validation)
	      == XP_CA_ERR_VERIFY);
	validation.required_policy = &request.policy;
	validation.validation_time = request.not_after + 1;
	CHECK(xp_ca_cert_validate(child, chain, 1, root, NULL, 0, &validation)
	      == XP_CA_ERR_VERIFY);
	validation.validation_time = now;

	root_crl_request = (struct xp_ca_crl_request) {
		root_crl_url,
		NULL,
		0,
		now,
		now + 900,
	};
	intermediate_crl_request = (struct xp_ca_crl_request) {
		intermediate_crl_url,
		NULL,
		0,
		now,
		now + 600,
	};
	CHECK(xp_ca_crl_create(&root_crl, root_key, root, NULL,
	                       &root_crl_request) == XP_CA_OK);
	CHECK(xp_ca_crl_create(&crl, intermediate_key, intermediate, NULL,
	                       &intermediate_crl_request) == XP_CA_OK);
	CHECK(xp_ca_crl_get_info(crl, &crl_info) == XP_CA_OK);
	CHECK(crl_info.this_update == now);
	CHECK(crl_info.next_update == now + 600);
	revoked.require_crl = true;
	CHECK(xp_ca_cert_validate(child, chain, 1, root, NULL, 0, &revoked)
	      == XP_CA_ERR_VERIFY);
	const xp_ca_crl_t leaf_crl_only[] = { crl };

	CHECK(xp_ca_cert_validate(child, chain, 1, root, leaf_crl_only, 1,
	                          &revoked) == XP_CA_ERR_VERIFY);
	const xp_ca_crl_t unrevoked_crls[] = { crl, root_crl };

	CHECK(xp_ca_cert_validate(child, chain, 1, root, unrevoked_crls, 2,
	                          &revoked) == XP_CA_OK);
	revoked.validation_time = now - 1;
	CHECK(xp_ca_cert_validate(child, chain, 1, root, unrevoked_crls, 2,
	                          &revoked) == XP_CA_ERR_VERIFY);
	revoked.validation_time = now + 601;
	CHECK(xp_ca_cert_validate(child, chain, 1, root, unrevoked_crls, 2,
	                          &revoked) == XP_CA_ERR_VERIFY);
	revoked.validation_time = now;
	intermediate_crl_request.next_update = now + 900;
	intermediate_crl_request.issuing_distribution_point =
		"https://wrong.example.test/ca.crl";
	CHECK(xp_ca_crl_create(&rejected_crl, intermediate_key, intermediate, crl,
	                       &intermediate_crl_request) == XP_CA_ERR_VERIFY);
	CHECK(rejected_crl == NULL);
	intermediate_crl_request.issuing_distribution_point = intermediate_crl_url;
	CHECK(xp_ca_crl_create(&rejected_crl, intermediate_key, intermediate,
	                       root_crl, &intermediate_crl_request)
	      == XP_CA_ERR_VERIFY);
	CHECK(rejected_crl == NULL);
	root_crl_request.newly_revoked = &child;
	root_crl_request.newly_revoked_count = 1;
	CHECK(xp_ca_crl_create(&rejected_crl, root_key, root, root_crl,
	                       &root_crl_request) == XP_CA_ERR_VERIFY);
	CHECK(rejected_crl == NULL);
	root_crl_request.newly_revoked = NULL;
	root_crl_request.newly_revoked_count = 0;
	intermediate_crl_request.newly_revoked = &child;
	intermediate_crl_request.newly_revoked_count = 1;
	CHECK(xp_ca_crl_create(&updated_crl, intermediate_key, intermediate, crl,
	                       &intermediate_crl_request) == XP_CA_OK);
	CHECK(xp_ca_crl_get_info(updated_crl, &imported_crl_info) == XP_CA_OK);
	CHECK(imported_crl_info.number > crl_info.number);
	der_len = sizeof(der);
	CHECK(xp_ca_crl_export_der(updated_crl, der, &der_len) == XP_CA_OK);
	encoded_len = 0;
	CHECK(xp_ca_crl_export_der(updated_crl, NULL, &encoded_len) == XP_CA_OK);
	CHECK(encoded_len == der_len);
	CHECK(der_len < sizeof(der));
	der[der_len] = 0;
	CHECK(xp_ca_crl_import_der(&imported_crl, der, der_len + 1)
	      == XP_CA_ERR_FORMAT);
	CHECK(imported_crl == NULL);
	CHECK(xp_ca_crl_import_der(&imported_crl, der, der_len) == XP_CA_OK);
	CHECK(xp_ca_crl_get_info(imported_crl, &imported_crl_info) == XP_CA_OK);
	CHECK(imported_crl_info.number > crl_info.number);
	const xp_ca_crl_t revoked_crls[] = { imported_crl, root_crl };

	CHECK(xp_ca_cert_validate(child, chain, 1, root, revoked_crls, 2,
	                          &revoked) == XP_CA_ERR_VERIFY);
	root_crl_request.newly_revoked = &intermediate;
	root_crl_request.newly_revoked_count = 1;
	CHECK(xp_ca_crl_create(&revoked_root_crl, root_key, root, root_crl,
	                       &root_crl_request) == XP_CA_OK);
	const xp_ca_crl_t root_revocation_crls[] = { crl, revoked_root_crl };

	CHECK(xp_ca_cert_validate(child, chain, 1, root, root_revocation_crls, 2,
	                          &revoked) == XP_CA_ERR_VERIFY);

	xp_ca_crl_free(revoked_root_crl);
	xp_ca_crl_free(updated_crl);
	xp_ca_crl_free(imported_crl);
	xp_ca_crl_free(crl);
	xp_ca_crl_free(root_crl);
	xp_ca_cert_free(child);
	xp_ca_csr_free(csr);
	xp_ca_csr_free(rich_csr);
	xp_key_release(child_key);
	xp_ca_cert_free(intermediate);
	xp_ca_csr_free(intermediate_csr);
	xp_key_release(intermediate_key);
	xp_ca_cert_free(alternate_root);
	xp_key_release(alternate_root_key);
	xp_ca_cert_free(root);
	xp_key_release(root_key);
	return 0;
}
