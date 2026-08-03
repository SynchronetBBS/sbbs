/****************************************************************************
 * xp_ca -- provider-neutral private-PKI primitives for Synchronet.
 *
 * Applications describe certificate policy in these types; provider objects
 * never cross this interface. QWK is one policy consumer, not a special case
 * in this library.
 ****************************************************************************/
#ifndef _XP_CA_H
#define _XP_CA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "wrapdll.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct xp_ca_key *xp_ca_key_t;
typedef struct xp_ca_csr *xp_ca_csr_t;
typedef struct xp_ca_cert *xp_ca_cert_t;
typedef struct xp_ca_crl *xp_ca_crl_t;

enum xp_ca_status {
	XP_CA_OK = 0,
	XP_CA_ERR = -1,
	XP_CA_ERR_FORMAT = -2,
	XP_CA_ERR_POLICY = -3,
	XP_CA_ERR_VERIFY = -4,
	XP_CA_ERR_DISABLED = -5,
};

enum xp_ca_key_algorithm {
	XP_CA_KEY_ED25519 = 1,
};

enum xp_ca_key_usage {
	XP_CA_KEY_USE_NONE = 0,
	XP_CA_KEY_USE_SIGN = 1,
	XP_CA_KEY_USE_CERT_SIGN = 2,
	XP_CA_KEY_USE_CRL_SIGN = 4,
};

enum xp_ca_eku {
	XP_CA_EKU_NONE = 0,
	XP_CA_EKU_SERVER_AUTH = 1,
	XP_CA_EKU_CLIENT_AUTH = 2,
};

/* All strings and arrays are borrowed for the duration of the call. */
struct xp_ca_identity {
	const char *common_name;
	const char *const *dns_names;
	size_t dns_name_count;
};

struct xp_ca_certificate_policy {
	bool is_ca;
	bool basic_constraints_critical;
	bool key_usage_critical;
	unsigned key_usage;
	unsigned extended_key_usage;
	int path_length; /* -1 omits the constraint. */
	const char *crl_distribution_point; /* Optional URI. */
};

struct xp_ca_issue_request {
	struct xp_ca_identity subject;
	struct xp_ca_certificate_policy policy;
	time_t not_before;
	time_t not_after;
};

struct xp_ca_validation_policy {
	/* CN is checked when non-NULL; the complete DNS SAN set is exact. */
	const struct xp_ca_identity *expected_identity;
	/* Every represented certificate-profile field must match exactly. */
	const struct xp_ca_certificate_policy *required_policy;
	time_t validation_time; /* Zero selects the current time. */
	bool require_crl; /* Require applicable CRLs throughout the chain. */
};

struct xp_ca_crl_info {
	uint64_t number;
	time_t this_update;
	time_t next_update;
};

struct xp_ca_crl_request {
	/* URL encoded into the CRL's critical issuingDistributionPoint. */
	const char *issuing_distribution_point;
	const xp_ca_cert_t *newly_revoked;
	size_t newly_revoked_count;
	time_t this_update;
	time_t next_update;
};

/*
 * Key persistence always uses an encrypted private-key PEM. The password must
 * be non-empty. Saving atomically replaces path with a private temporary file;
 * deleting removes the named file but makes no physical-erasure guarantee.
 * On any failed constructor/import, a non-NULL output parameter is set to NULL.
 */
DLLEXPORT int xp_ca_key_generate(
	xp_ca_key_t *out,
	enum xp_ca_key_algorithm algorithm);
DLLEXPORT int xp_ca_key_load_pem(
	xp_ca_key_t *out,
	const char *path,
	const char *password);
DLLEXPORT int xp_ca_key_save_pem(
	xp_ca_key_t key,
	const char *path,
	const char *password);
DLLEXPORT int xp_ca_key_delete_pem(const char *path);
DLLEXPORT void xp_ca_key_free(xp_ca_key_t key);

/*
 * DER exporters use *len as the destination capacity. Passing out == NULL
 * reports the required size. An undersized destination also updates *len.
 */

/*
 * A generated CSR proves possession of its public key but carries no requested
 * identity or policy. The issuer supplies all certificate fields separately.
 */
DLLEXPORT int xp_ca_csr_create(xp_ca_csr_t *out, xp_ca_key_t key);
DLLEXPORT int xp_ca_csr_import_der(
	xp_ca_csr_t *out,
	const void *der,
	size_t len);
DLLEXPORT int xp_ca_csr_export_der(
	xp_ca_csr_t csr,
	void *out,
	size_t *len);
DLLEXPORT int xp_ca_csr_verify(xp_ca_csr_t csr);
DLLEXPORT void xp_ca_csr_free(xp_ca_csr_t csr);

DLLEXPORT int xp_ca_cert_create_self_signed(
	xp_ca_cert_t *out,
	xp_ca_key_t key,
	const struct xp_ca_issue_request *request);
DLLEXPORT int xp_ca_cert_issue(
	xp_ca_cert_t *out,
	xp_ca_key_t issuer_key,
	xp_ca_cert_t issuer,
	xp_ca_csr_t subject_key,
	const struct xp_ca_issue_request *request);
DLLEXPORT int xp_ca_cert_import_der(
	xp_ca_cert_t *out,
	const void *der,
	size_t len);
DLLEXPORT int xp_ca_cert_export_der(
	xp_ca_cert_t cert,
	void *out,
	size_t *len);
DLLEXPORT void xp_ca_cert_free(xp_ca_cert_t cert);

/*
 * CRLs are complete for the scope named by issuing_distribution_point.
 * Passing previous preserves its entries and increments its CRL number before
 * adding newly_revoked certificates. Every newly revoked certificate must
 * have been directly signed by issuer.
 */
DLLEXPORT int xp_ca_crl_create(
	xp_ca_crl_t *out,
	xp_ca_key_t issuer_key,
	xp_ca_cert_t issuer,
	xp_ca_crl_t previous,
	const struct xp_ca_crl_request *request);
DLLEXPORT int xp_ca_crl_import_der(
	xp_ca_crl_t *out,
	const void *der,
	size_t len);
DLLEXPORT int xp_ca_crl_export_der(
	xp_ca_crl_t crl,
	void *out,
	size_t *len);
DLLEXPORT int xp_ca_crl_get_info(
	xp_ca_crl_t crl,
	struct xp_ca_crl_info *info);
DLLEXPORT void xp_ca_crl_free(xp_ca_crl_t crl);

/*
 * chain contains intermediates in leaf-to-root order and excludes leaf/root.
 * root is the sole trust anchor. crls contains applicable full CRLs.
 */
DLLEXPORT int xp_ca_cert_validate(
	xp_ca_cert_t leaf,
	const xp_ca_cert_t *chain,
	size_t chain_count,
	xp_ca_cert_t root,
	const xp_ca_crl_t *crls,
	size_t crl_count,
	const struct xp_ca_validation_policy *policy);

/* Thread-local provider diagnostic; meaningful only after a failed call. */
DLLEXPORT const char *xp_ca_last_error(void);

#if defined(__cplusplus)
}
#endif

#endif
