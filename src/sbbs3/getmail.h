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

#ifndef _GETMAIL_H_
#define _GETMAIL_H_

#include "scfgdefs.h"
#include "smblib.h"
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT int		getmail(scfg_t* cfg, int usernumber, BOOL sent, int attr);
DLLEXPORT mail_t *	loadmail(smb_t* smb, uint32_t* msgs, uint usernumber
							,int which, long mode);
DLLEXPORT void		freemail(mail_t* mail);
DLLEXPORT BOOL		delfattach(scfg_t*, smbmsg_t*);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
