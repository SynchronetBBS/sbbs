/* mailsrvr.h */

/* Synchronet Mail (SMTP/POP3/SendMail) server */

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


#ifdef _WIN32
#include <windows.h>
#endif
#include "client.h"

typedef struct {

	DWORD	size;				/* sizeof(mail_startup_t) */
	WORD	smtp_port;
	WORD	pop3_port;
	WORD	max_clients;
	WORD	max_inactivity;
	WORD	max_delivery_attempts;
	WORD	rescan_frequency;	/* In seconds */
	WORD	reserved_word3;
	WORD	reserved_word2;
	WORD	reserved_word1;
    DWORD   interface_addr;
    DWORD	options;			/* See MAIL_OPT definitions */
    DWORD	reserved_dword8;
    DWORD	reserved_dword7;
    DWORD	reserved_dword6;
    DWORD	reserved_dword5;
    DWORD	reserved_dword4;
    DWORD	reserved_dword3;
    DWORD	reserved_dword2;
    DWORD	reserved_dword1;
	int 	(*lputs)(char*);
	void	(*status)(char*);
    void	(*started)(void);
    void	(*terminated)(int code);
    void	(*clients)(int active);
    void	(*thread_up)(BOOL up);
	void	(*socket_open)(BOOL open);
    void	(*client_on)(BOOL on, int sock, client_t*);
    void	(*reserved_fptr4)(void);
    void	(*reserved_fptr3)(void);
    void	(*reserved_fptr2)(void);
    void	(*reserved_fptr1)(void);
    char    ctrl_dir[128];
    char	relay_server[128];
    char	dns_server[128];
    char	reserved_path8[128];
    char	reserved_path7[128];
    char	reserved_path6[128];
    char	reserved_path5[128];
    char	reserved_path4[128];
    char	reserved_path3[128];
	char	inbound_sound[128];
	char	outbound_sound[128];
    char	pop3_sound[128];
    char	reserved_path1[128];

} mail_startup_t;

#define MAIL_OPT_DEBUG_RX_HEADER	(1<<0)
#define MAIL_OPT_DEBUG_RX_BODY		(1<<1)
#define MAIL_OPT_ALLOW_POP3			(1<<2)
#define MAIL_OPT_DEBUG_TX			(1<<3)
#define MAIL_OPT_DEBUG_RX_RSP		(1<<4)
#define MAIL_OPT_RELAY_TX			(1<<5)
#define MAIL_OPT_DEBUG_POP3			(1<<6)
#define MAIL_OPT_ALLOW_RX_BY_NUMBER	(1<<7)
#define MAIL_OPT_USE_RBL			(1<<8)
#define MAIL_OPT_USE_DUL			(1<<9)
#define MAIL_OPT_USE_RSS			(1<<10)
#define MAIL_OPT_NO_HOST_LOOKUP		(1<<11)
#define MAIL_OPT_MUTE				(1<<31)

#ifdef MAILSRVR_EXPORTS
#define MAIL_CALL __declspec( dllexport )
#else
#define MAIL_CALL __declspec( dllimport )
#endif

#ifdef __cplusplus
extern "C" {
#endif
/* arg is pointer to static mail_startup_t* */
MAIL_CALL void	mail_server(void* arg);
MAIL_CALL void	mail_terminate(void);
MAIL_CALL char*	mail_ver(void);
#ifdef __cplusplus
}
#endif