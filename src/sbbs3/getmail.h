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

#ifndef GETMAIL_H_
#define GETMAIL_H_

#include "scfgdefs.h"
#include "smblib.h"
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Why a user's mailbox takes no more mail (see user_mailbox_full()) */
enum mailbox_full {
	MAILBOX_NOT_FULL = 0,
	MAILBOX_FULL_MSGS,      /* More than cfg->max_msgs_waiting messages in the Inbox */
	MAILBOX_FULL_BYTES      /* The user's mail has reached cfg->mailbox_quota */
};

DLLEXPORT int       getmail(scfg_t* cfg, int usernumber, bool sent, int attr);
DLLEXPORT enum mailbox_full user_mailbox_full(scfg_t*, const user_t*, bool system_sender, uint* waiting);
DLLEXPORT uint64_t  user_mailbox_bytes(scfg_t*, uint usernumber);
DLLEXPORT mail_t *  loadmail(smb_t* smb, uint32_t* msgs, uint usernumber
                             , int which, long mode);
DLLEXPORT void      freemail(mail_t* mail);
DLLEXPORT int       delusermail(scfg_t* cfg, uint usernumber);
DLLEXPORT bool      delfattach(scfg_t*, smbmsg_t*);
DLLEXPORT int       add_fwd_mail_copy(smb_t*, smbmsg_t* fwdmsg, uint usernumber, int storage);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
