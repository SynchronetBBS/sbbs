/* client.h */

/* Synchronet client information to share with SBBSCTRL */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _CLIENT_H
#define _CLIENT_H

#include "gen_defs.h"	/* WORD, DWORD */
#include <time.h>		/* time_t */

/* Used for sbbsctrl->client window */
typedef struct {
	size_t		size;		/* size of this struct */
	char		addr[16];	/* IP address */
	char		host[64];	/* host name */
	WORD		port;		/* TCP port number */
	time_t		time;		/* connect time */
	const char*	protocol;	/* protocol description */
	char*		user;		/* user name */
	char		pad[32];	/* padding for future expansion */
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
