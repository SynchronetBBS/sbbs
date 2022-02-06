/* Synchronet client information to share with SBBSCTRL */

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

#ifndef _CLIENT_H
#define _CLIENT_H

#include "gen_defs.h"	/* WORD, DWORD */
#include "sockwrap.h"	/* INET6_ADDRSTRLEN */
#include <time.h>		/* time_t */

/* Used for sbbsctrl->client window */
typedef struct {
	size_t		size;		/* size of this struct */
	char		addr[128];	/* IP address */
	char		host[256];	/* host name */
	uint16_t	port;		/* TCP port number */
	time32_t	time;		/* connect time */
	const char*	protocol;	/* protocol description */
	const char*	user;		/* user name */
	uint32_t	usernum;	/* user number (authenticated when non-zero) */
} client_t;

/* Used for ctrl/client.dab */
typedef struct client_rec {
	DWORD		local_addr;
	DWORD		remote_addr;
	DWORD		local_port;
	DWORD		remote_port;
	DWORD		socket;
	/* 20 */
	DWORD		time;
	/* 24 */
	char		protocol[16];
	/* 40 */
	char		user_name[32];
	/* 72 */
	char		local_host[64];
	/* 136 */
	char		remote_host[64];
	/* 200 */
	char		pad[512-200];
} client_rec_t;

#endif /* Don't add anything after this line */
