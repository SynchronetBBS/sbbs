/* msg_id.c */

/* Synchronet Message-ID generation routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include "sbbs.h"

static msg_number(smbmsg_t* msg)
{
	if(msg->idx.number)
		return(msg->idx.number);
	return(msg->hdr.number);
}

static msg_time(smbmsg_t* msg)
{
	if(msg->idx.time)
		return(msg->idx.time);
	return(msg->hdr.when_imported.time);
}

static ulong msgid_serialno(smbmsg_t* msg)
{
	return (msg_time(msg)-1000000000) + msg_number(msg);
}

/****************************************************************************/
/* Returns a FidoNet (FTS-9) message-ID										*/
/****************************************************************************/
char* DLLCALL ftn_msgid(sub_t *sub, smbmsg_t* msg)
{
	static char msgid[256];

	if(msg->ftn_msgid!=NULL && *msg->ftn_msgid!=0)
		return(msg->ftn_msgid);

	snprintf(msgid,sizeof(msgid)
		,"%lu.%s@%s %08lx"
		,msg_number(msg)
		,sub->code
		,smb_faddrtoa(&sub->faddr,NULL)
		,msgid_serialno(msg)
		);

	return(msgid);
}

/****************************************************************************/
/* Return a general purpose (RFC-822) message-ID							*/
/****************************************************************************/
char* DLLCALL get_msgid(scfg_t* cfg, uint subnum, smbmsg_t* msg)
{
	static char msgid[256];

	if(msg->id!=NULL && *msg->id!=0)
		return(msg->id);

	if(subnum>=cfg->total_subs)
		snprintf(msgid,sizeof(msgid)
			,"<%08lX.%lu@%s>"
			,msg_time(msg)
			,msg_number(msg)
			,cfg->sys_inetaddr);
	else
		snprintf(msgid,sizeof(msgid)
			,"<%08lX.%lu.%s@%s>"
			,msg_time(msg)
			,msg_number(msg)
			,cfg->sub[subnum]->code
			,cfg->sys_inetaddr);

	return(msgid);
}
