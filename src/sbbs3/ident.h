/* Synchronet Indentification (RFC1413) functions */

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


#ifndef _IDENT_H_
#define _IDENT_H_

#define IDENT_DEFAULT_TIMEOUT   5   /* seconds */

#ifdef __cplusplus
extern "C" {
#endif

bool identify(union xp_sockaddr* client_addr, u_short local_port, char* buf
              , size_t maxlen, int timeout /* in seconds */);

#ifdef __cplusplus
}
#endif

#endif
