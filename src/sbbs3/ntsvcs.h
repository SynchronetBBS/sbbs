/* Synchronet BBS Windows NT Service Names */

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

#ifndef _NTSVCS_H_
#define _NTSVCS_H_

#define NTSVC_NAME_BBS		"SynchronetBBS"
#define NTSVC_NAME_EVENT	"SynchronetEvent"	/* This is not (currently) a separate service */
#define NTSVC_NAME_FTP		"SynchronetFTP"
#define NTSVC_NAME_WEB		"SynchronetWeb"
#define NTSVC_NAME_MAIL		"SynchronetMail"
#define NTSVC_NAME_SERVICES "SynchronetServices"

/* User-defined control codes */
enum {
	 SERVICE_CONTROL_RECYCLE=128
};

typedef struct {
	SYSTEMTIME	time;
	char		level;
	char		buf[1000];
	uint32_t	repeated;
} log_msg_t;

#endif	/* Don't add anything after this line */
