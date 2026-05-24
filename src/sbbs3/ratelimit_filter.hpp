// Rate-limit auto-filter helpers shared by web, ftp, mail, services
// (kept header-only / static-inline so no shared .cpp needs to be added
// to each server's project files).

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef RATELIMIT_FILTER_HPP_
#define RATELIMIT_FILTER_HPP_

#include <string>
#include "ratelimit.hpp"        // rateLimiter
#include "scfgdefs.h"           // scfg_t
#include "trash.h"              // filter_ip()
#include "sockwrap.h"           // SOCKET, INET6_ADDRSTRLEN, inet_pton, inet_ntop
#include "genwrap.h"            // safe_snprintf
#include "sbbsdefs.h"           // LOG_NOTICE

/* Compute a rate-limit bucket key for the client IP.  When a non-zero
 * subnet prefix is configured for the address' family, the key is the
 * network address in CIDR notation (e.g. "47.82.14.0/24") so that requests
 * (or connections) from an entire subnet are counted (and may be filtered)
 * together; otherwise the key is simply the client IP. */
static inline std::string rate_limit_key(const char* ip, uint prefix4, uint prefix6)
{
	if (ip == NULL || *ip == '\0')
		return std::string();

	bool ipv6 = strchr(ip, ':') != NULL;
	uint prefix = ipv6 ? prefix6 : prefix4;
	uint maxbits = ipv6 ? 128 : 32;
	if (prefix == 0 || prefix >= maxbits)
		return std::string(ip); // per-host-IP (no subnet aggregation)

	char net[INET6_ADDRSTRLEN];
	char key[INET6_ADDRSTRLEN + 8];
	if (ipv6) {
		uint8_t addr[16];
		if (inet_pton(AF_INET6, ip, addr) != 1)
			return std::string(ip);
		for (uint bit = prefix; bit < 128; ++bit)
			addr[bit / 8] &= ~(0x80 >> (bit % 8));
		if (inet_ntop(AF_INET6, addr, net, sizeof net) == NULL)
			return std::string(ip);
	} else {
		struct in_addr addr;
		if (inet_pton(AF_INET, ip, &addr) != 1)
			return std::string(ip);
		uint32_t host = ntohl(addr.s_addr) & (0xFFFFFFFFu << (32 - prefix));
		addr.s_addr = htonl(host);
		if (inet_ntop(AF_INET, &addr, net, sizeof net) == NULL)
			return std::string(ip);
	}
	safe_snprintf(key, sizeof key, "%s/%u", net, prefix);
	return std::string(key);
}

/* Auto-filter a rate-limit abuser once it has exceeded the limit at least
 * `threshold` times while continuously active.  The (sub)net key is written
 * to ip-silent.can (dropped at accept) when `silent` is true, otherwise to
 * ip.can.  Distinct-IP guard: when the bucket is an aggregated subnet (key
 * differs from the host IP), only filter the entire subnet if at least
 * `subnet_threshold` distinct client IPs have abused it; otherwise filter
 * just that host IP, so innocent neighbors that share the subnet are spared.
 * `subnet_threshold` of 0 or 1 disables the guard (subnet filtered on the
 * first abuser).  `log` is the caller's lprintf (per-server file-static). */
static inline void rate_limit_filter(SOCKET sock, scfg_t* cfg, const char* prot
                                     , const char* host_ip, const char* host_name
                                     , const std::string& key, unsigned denials, rateLimiter* limiter
                                     , uint threshold, uint duration, bool silent, uint subnet_threshold
                                     , int (*log)(int, const char*, ...))
{
	if (threshold == 0 || denials < threshold)
		return;

	std::string target = key;
	size_t      distinct = (key == host_ip) ? 1 : limiter->distinctMembers(key);
	uint        subnet_min = subnet_threshold ? subnet_threshold : 1;
	if (key != host_ip && distinct < subnet_min)
		target = host_ip;

	char reason[128];
	char fpath[MAX_PATH + 1];
	const char* fname = NULL; // NULL => filter_ip() uses ip.can

	if (silent) {
		safe_snprintf(fpath, sizeof fpath, "%sip-silent.can", cfg->text_dir);
		fname = fpath;
	}
	if (target == host_ip)
		safe_snprintf(reason, sizeof reason, "%u rate-limit violations", denials);
	else
		safe_snprintf(reason, sizeof reason, "%u rate-limit violations from %zu IPs", denials, distinct);
	if (filter_ip(cfg, prot, reason, host_name, target.c_str(), /* username: */ NULL, fname, duration))
		log(LOG_NOTICE, "%04d %s !BLOCKING %s%s: %s%s"
		    , sock, prot
		    , target == host_ip ? "IP ADDRESS" : "SUBNET", silent ? " (silently)" : ""
		    , target.c_str(), silent ? "" : " in ip.can");
}

#endif // RATELIMIT_FILTER_HPP_
