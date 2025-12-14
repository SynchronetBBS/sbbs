/* Synchronet DNS MX-record lookup routines */

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

#ifndef MXLOOKUP_H_
#define MXLOOKUP_H_

#include "gen_defs.h"

#if defined(__cplusplus)
extern "C" {
#endif

int dns_getmx(char* name, char* mx, char* mx2
              , DWORD intf, DWORD ip_addr, bool use_tcp, int timeout);

#if defined(__cplusplus)
}
#endif

#endif  /* Don't add anything after this line */
