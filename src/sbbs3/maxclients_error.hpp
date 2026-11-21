// Error logged when a server starts refusing connections at its capacity,
// shared by the terminal, web, ftp, mail and services servers (#1384)
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

#ifndef MAXCLIENTS_ERROR_HPP_
#define MAXCLIENTS_ERROR_HPP_

#include <time.h>
#include "genwrap.h"            // duration_to_vstr(), SAFECOPY
#include "sockwrap.h"           // INET6_ADDRSTRLEN
#include "sbbsdefs.h"           // LOG_ERR, LOG_WARNING

// An episode ends once this long passes with no connection refused
#define MAX_CLIENTS_EPISODE_QUIET   (5 * 60)

// Plain data (services keeps one per service in a realloc'd array).
// Not thread-safe: callers that refuse connections from more than one
// thread must serialize their calls.
struct max_clients_episode {
	time_t   start;             // 0 = no episode in progress
	time_t   last;              // most recent refusal
	unsigned refused;
	char     reason[128];
	char     first_client[INET6_ADDRSTRLEN];
};

// Call for each connection refused because the server is at capacity.
// The first refusal of an episode is logged at LOG_ERR, which reaches
// error.log (and the sysop's error notifications, per the configured
// error level); later refusals are only counted.  `reason` describes the limit, e.g. "MAXIMUM CLIENTS (200) reached".
// `log` is the caller's lprintf (per-server file-static).
static inline void max_clients_refused(struct max_clients_episode* ep, bool enabled
                                       , const char* reason, const char* client
                                       , int (*log)(int, const char*, ...))
{
	if (!enabled)
		return;
	time_t now = time(NULL);
	if (ep->start == 0) {
		ep->start = now;
		ep->refused = 0;
		SAFECOPY(ep->reason, reason);
		SAFECOPY(ep->first_client, client == NULL ? "" : client);
		log(LOG_ERR, "!%s: refusing new connections (first refused: %s)"
		    , ep->reason, ep->first_client);
	}
	ep->refused++;
	ep->last = now;
}

// Call periodically (e.g. each pass of the accept loop): ends an episode,
// logging a summary, once no connection has been refused for
// MAX_CLIENTS_EPISODE_QUIET seconds.
static inline void max_clients_check(struct max_clients_episode* ep
                                     , int (*log)(int, const char*, ...))
{
	if (ep->start == 0 || time(NULL) - ep->last < MAX_CLIENTS_EPISODE_QUIET)
		return;
	char span[64];
	duration_to_vstr((double)(ep->last - ep->start), span, sizeof span);
	log(LOG_WARNING, "No longer at capacity (%s): %u connections refused over %s (first refused: %s)"
	    , ep->reason, ep->refused, span, ep->first_client);
	ep->start = 0;
}

#endif // MAXCLIENTS_ERROR_HPP_
