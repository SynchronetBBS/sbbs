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
xp_ca_issue_request_is_valid(const struct xp_ca_issue_request *request)
{
	if (request == NULL || request->subject.common_name == NULL
	    || request->subject.common_name[0] == '\0'
	    || request->not_after <= request->not_before)
		return false;
	for (const unsigned char *value =
	         (const unsigned char *)request->subject.common_name;
	     *value != '\0'; value++) {
		if (*value < 0x20 || *value == 0x7f)
			return false;
	}
	if (request->subject.dns_name_count != 0
	    && request->subject.dns_names == NULL)
		return false;
	for (size_t i = 0; i < request->subject.dns_name_count; i++) {
		const char *name = request->subject.dns_names[i];
		if (name == NULL || name[0] == '\0')
			return false;
		for (const unsigned char *value = (const unsigned char *)name;
		     *value != '\0'; value++) {
			if (*value <= 0x20 || *value >= 0x7f || *value == ',')
				return false;
		}
		for (size_t earlier = 0; earlier < i; earlier++) {
			if (xp_ca_dns_names_equal(
			        request->subject.dns_names[earlier], name))
				return false;
		}
	}
	if ((request->policy.key_usage
	     & ~(XP_CA_KEY_USE_SIGN | XP_CA_KEY_USE_CERT_SIGN
	         | XP_CA_KEY_USE_CRL_SIGN)) != 0)
		return false;
	if ((request->policy.extended_key_usage
	     & ~(XP_CA_EKU_SERVER_AUTH | XP_CA_EKU_CLIENT_AUTH)) != 0)
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
