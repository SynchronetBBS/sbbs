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

#ifndef _MSG_ID_H_
#define _MSG_ID_H_

#include "smbdefs.h"
#include "scfgdefs.h"
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT char *	ftn_msgid(sub_t*, smbmsg_t*, char* msgid, size_t);
DLLEXPORT char *	get_msgid(scfg_t*, uint subnum, smbmsg_t*, char* msgid, size_t);
DLLEXPORT char *	get_replyid(scfg_t*, smb_t*, smbmsg_t*, char* msgid, size_t maxlen);
DLLEXPORT uint32_t	get_new_msg_number(smb_t*);
DLLEXPORT BOOL		add_msg_ids(scfg_t*, smb_t*, smbmsg_t*, smbmsg_t* remsg);
DLLEXPORT BOOL		add_reply_ids(scfg_t*, smb_t*, smbmsg_t*, smbmsg_t* remsg);
DLLEXPORT char*		msg_program_id(char* pid, size_t);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
