/* Synchronet RFC822 message date/time string conversion routines */

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

#ifndef _MSGDATE_H_
#define _MSGDATE_H_

#include <time.h>		// time_t
#include "scfgdefs.h"	// scfg_t
#include "smbdefs.h"	// when_t
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT when_t	rfc822date(char* p);
DLLEXPORT char *	msgdate(when_t when, char* buf);
DLLEXPORT BOOL		newmsgs(smb_t*, time_t);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */