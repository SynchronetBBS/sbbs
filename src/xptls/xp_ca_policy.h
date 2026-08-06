#ifndef _XP_CA_POLICY_H
#define _XP_CA_POLICY_H

#include <string.h>

#include "xp_ca.h"

static unsigned char
xp_ca_ascii_lower(unsigned char value)
{
	return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

static bool
xp_ca_dns_names_equal(const char *first, const char *second)
{
	while (*first != '\0' && *second != '\0') {
		if (xp_ca_ascii_lower((unsigned char)*first)
		    != xp_ca_ascii_lower((unsigned char)*second))
			return false;
		first++;
		second++;
	}
	return *first == *second;
}

static bool
xp_ca_uri_is_valid(const char *uri)
{
	if (uri == NULL || uri[0] == '\0')
		return false;
	for (const unsigned char *value = (const unsigned char *)uri;
	     *value != '\0'; value++) {
		if (*value <= 0x20 || *value >= 0x7f)
			return false;
	}
	return true;
}

static bool
xp_ca_text_is_valid(const char *text, bool required)
{
	if (text == NULL)
		return !required;
	if (text[0] == '\0')
		return false;
	for (const unsigned char *value = (const unsigned char *)text;
	     *value != '\0'; value++)
		if (*value < 0x20 || *value == 0x7f)
			return false;
	return true;
}

static bool
xp_ca_identity_is_valid(const struct xp_ca_identity *identity,
	bool require_common_name)
{
	if (identity == NULL
	    || !xp_ca_text_is_valid(identity->common_name, require_common_name)
	    || !xp_ca_text_is_valid(identity->organization, false)
	    || !xp_ca_text_is_valid(identity->organizational_unit, false)
	    || !xp_ca_text_is_valid(identity->country, false)
	    || !xp_ca_text_is_valid(identity->state_or_province, false)
	    || !xp_ca_text_is_valid(identity->locality, false)
	    || !xp_ca_text_is_valid(identity->email_address, false)
	    || (identity->country != NULL && strlen(identity->country) != 2)
	    || (identity->dns_name_count != 0 && identity->dns_names == NULL))
		return false;
	for (size_t i = 0; i < identity->dns_name_count; i++) {
		const char *name = identity->dns_names[i];
		if (name == NULL || name[0] == '\0')
			return false;
		for (const unsigned char *value = (const unsigned char *)name;
		     *value != '\0'; value++)
			if (*value <= 0x20 || *value >= 0x7f || *value == ',')
				return false;
		for (size_t earlier = 0; earlier < i; earlier++)
			if (xp_ca_dns_names_equal(identity->dns_names[earlier], name))
				return false;
	}
	return true;
}

static bool
xp_ca_issue_request_is_valid(const struct xp_ca_issue_request *request)
{
	if (request == NULL || !xp_ca_identity_is_valid(&request->subject, true)
	    || request->not_after <= request->not_before)
		return false;
	if ((request->policy.key_usage
	     & ~(XP_CA_KEY_USE_SIGN | XP_CA_KEY_USE_CERT_SIGN
	         | XP_CA_KEY_USE_CRL_SIGN | XP_CA_KEY_USE_KEY_ENCIPHERMENT
	         | XP_CA_KEY_USE_DATA_ENCIPHERMENT | XP_CA_KEY_USE_KEY_AGREEMENT
	         | XP_CA_KEY_USE_NON_REPUDIATION | XP_CA_KEY_USE_ENCIPHER_ONLY
	         | XP_CA_KEY_USE_DECIPHER_ONLY)) != 0)
		return false;
	if ((request->policy.extended_key_usage
	     & ~(XP_CA_EKU_SERVER_AUTH | XP_CA_EKU_CLIENT_AUTH
	         | XP_CA_EKU_CODE_SIGNING | XP_CA_EKU_EMAIL_PROTECTION
	         | XP_CA_EKU_TIME_STAMPING | XP_CA_EKU_OCSP_SIGNING)) != 0)
		return false;
	if (request->policy.key_usage == XP_CA_KEY_USE_NONE
	    || request->policy.path_length < -1
	    || (!request->policy.is_ca && request->policy.path_length >= 0)
	    || (!request->policy.is_ca
	        && (request->policy.key_usage
	            & (XP_CA_KEY_USE_CERT_SIGN | XP_CA_KEY_USE_CRL_SIGN)) != 0)
	    || (request->policy.is_ca
	        && (request->policy.key_usage & XP_CA_KEY_USE_CERT_SIGN) == 0))
		return false;
	if (request->policy.crl_distribution_point != NULL
	    && !xp_ca_uri_is_valid(request->policy.crl_distribution_point))
		return false;
	return true;
}

static bool
xp_ca_crl_request_is_valid(const struct xp_ca_crl_request *request)
{
	if (request == NULL
	    || !xp_ca_uri_is_valid(request->issuing_distribution_point)
	    || request->next_update <= request->this_update
	    || (request->newly_revoked_count != 0
	        && request->newly_revoked == NULL))
		return false;
	return true;
}

#endif
