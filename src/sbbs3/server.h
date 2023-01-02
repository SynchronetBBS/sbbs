/* Synchronet Server values */

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

#ifndef SERVER_H_
#define SERVER_H_

enum server_type {
	SERVER_TERM,
	SERVER_MAIL,
	SERVER_FTP,
	SERVER_WEB,
	SERVER_SERVICES,
	SERVER_COUNT
};

// Approximate systemd service states (see sd_notify)
enum server_state {
	SERVER_STOPPED,
	SERVER_INIT,
	SERVER_READY,
	SERVER_RELOADING,
	SERVER_STOPPING
};

#endif /* Don't add anything after this line */
