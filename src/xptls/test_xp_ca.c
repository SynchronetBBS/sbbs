#include <stdio.h>
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
	xp_ca_key_t root_key = NULL;
	xp_ca_key_t alternate_root_key = NULL;
	xp_ca_key_t intermediate_key = NULL;
	xp_ca_key_t child_key = NULL;
	xp_ca_key_t reloaded = NULL;
	xp_ca_cert_t root = NULL;
	xp_ca_cert_t alternate_root = NULL;
	xp_ca_cert_t intermediate = NULL;
	xp_ca_cert_t child = NULL;
	xp_ca_cert_t imported_child = NULL;
	xp_ca_cert_t rejected_cert = NULL;
	xp_ca_csr_t csr = NULL;
	xp_ca_csr_t imported_csr = NULL;
	xp_ca_csr_t intermediate_csr = NULL;
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

	CHECK(xp_ca_key_generate(&root_key, XP_CA_KEY_ED25519) == XP_CA_OK);
	CHECK(xp_ca_key_save_pem(root_key, key_path, "") == XP_CA_ERR);
	CHECK(xp_ca_key_save_pem(root_key, key_path, "test-password") == XP_CA_OK);
	reloaded = root_key;
	CHECK(xp_ca_key_load_pem(&reloaded, key_path, "wrong-password") != XP_CA_OK);
	CHECK(reloaded == NULL);
	CHECK(xp_ca_key_load_pem(&reloaded, key_path, "test-password") == XP_CA_OK);
	xp_ca_key_free(reloaded);
	reloaded = NULL;
	CHECK(xp_ca_key_save_pem(root_key, key_path, "replacement-password") == XP_CA_OK);
	CHECK(xp_ca_key_load_pem(&reloaded, key_path, "test-password") != XP_CA_OK);
	CHECK(reloaded == NULL);
	CHECK(xp_ca_key_load_pem(&reloaded, key_path, "replacement-password") == XP_CA_OK);
	xp_ca_key_free(reloaded);
	reloaded = NULL;
#if defined(__unix__)
	struct stat key_stat;

	CHECK(stat(key_path, &key_stat) == 0);
	CHECK((key_stat.st_mode & 0777) == 0600);
#endif
	CHECK(xp_ca_key_delete_pem(key_path) == XP_CA_OK);

	CHECK(xp_ca_cert_create_self_signed(&root, root_key, &request) == XP_CA_OK);
	CHECK(xp_ca_key_generate(&alternate_root_key, XP_CA_KEY_ED25519) == XP_CA_OK);
	CHECK(xp_ca_cert_create_self_signed(
		&alternate_root, alternate_root_key, &request) == XP_CA_OK);

	CHECK(xp_ca_key_generate(&intermediate_key, XP_CA_KEY_ED25519) == XP_CA_OK);
	CHECK(xp_ca_csr_create(&intermediate_csr, intermediate_key) == XP_CA_OK);
	request.subject.common_name = intermediate_names[0];
	request.subject.dns_names = intermediate_names;
	request.policy.crl_distribution_point = root_crl_url;
	CHECK(xp_ca_cert_issue(&intermediate, root_key, root, intermediate_csr,
	                       &request) == XP_CA_OK);

	CHECK(xp_ca_key_generate(&child_key, XP_CA_KEY_ED25519) == XP_CA_OK);
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

	const xp_ca_cert_t chain[] = { intermediate };

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
	xp_ca_key_free(child_key);
	xp_ca_cert_free(intermediate);
	xp_ca_csr_free(intermediate_csr);
	xp_ca_key_free(intermediate_key);
	xp_ca_cert_free(alternate_root);
	xp_ca_key_free(alternate_root_key);
	xp_ca_cert_free(root);
	xp_ca_key_free(root_key);
	return 0;
}
