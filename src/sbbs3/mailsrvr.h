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

#ifndef _MAILSRVR_H_
#define _MAILSRVR_H_

#include "client.h"				/* client_t */
#include "sockwrap.h"           /* SOCKET */

typedef struct {

	DWORD	size;				/* sizeof(mail_startup_t) */
	WORD	smtp_port;
	WORD	pop3_port;
	WORD	max_clients;
	WORD	max_inactivity;
	WORD	max_delivery_attempts;
	WORD	rescan_frequency;	/* In seconds */
	WORD	relay_port;
	WORD	lines_per_yield;
	WORD	max_recipients;
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
    void	(*thread_up)(BOOL up, BOOL setuid);
	void	(*socket_open)(BOOL open);
    void	(*client_on)(BOOL on, int sock, client_t*, BOOL update);
    BOOL	(*seteuid)(BOOL user);
    void	(*reserved_fptr3)(void);
    void	(*reserved_fptr2)(void);
    void	(*reserved_fptr1)(void);
    char    ctrl_dir[128];
    char	relay_server[128];
    char	dns_server[128];
    char	default_user[128];
    char	dnsbl_tag[32];		// Tag to add to blacklisted subject
	char	dnsbl_hdr[32];		// Header field to add to msg header
	char	reserved_string[64];
    char	reserved_path6[128];
    char	reserved_path5[128];
    char	reserved_path4[128];
    char	reserved_path3[128];
	char	inbound_sound[128];
	char	outbound_sound[128];
    char	pop3_sound[128];
    char	host_name[128];
	BOOL	recycle_now;

} mail_startup_t;

#define MAIL_OPT_DEBUG_RX_HEADER	(1<<0)
#define MAIL_OPT_DEBUG_RX_BODY		(1<<1)
#define MAIL_OPT_ALLOW_POP3			(1<<2)
#define MAIL_OPT_DEBUG_TX			(1<<3)
#define MAIL_OPT_DEBUG_RX_RSP		(1<<4)
#define MAIL_OPT_RELAY_TX			(1<<5)	/* Use SMTP relay server */
#define MAIL_OPT_DEBUG_POP3			(1<<6)
#define MAIL_OPT_ALLOW_RX_BY_NUMBER	(1<<7)	/* Allow mail sent to user # */
#define MAIL_OPT_NO_HOST_LOOKUP		(1<<11)	/* Don't look-up hostnames */
#define MAIL_OPT_USE_TCP_DNS		(1<<12)	/* Use TCP vs UDP for DNS req */
#define MAIL_OPT_NO_SENDMAIL		(1<<13)	/* Don't run SendMail thread */
#define MAIL_OPT_ALLOW_RELAY		(1<<14)	/* Allow relays from stored user IPs */
#define MAIL_OPT_DNSBL_REFUSE		(1<<15) /* Refuse session, return error */
#define MAIL_OPT_DNSBL_IGNORE		(1<<16) /* Dump mail, return success */
#define MAIL_OPT_DNSBL_BADUSER		(1<<17) /* Refuse mail (bad user name) */
#define MAIL_OPT_DNSBL_DEBUG		(1<<20) /* Debug DNSBL activity */
#define MAIL_OPT_NO_RECYCLE			(1<<27)	/* Disable recycling of server		*/
#define MAIL_OPT_LOCAL_TIMEZONE		(1<<30)	/* Don't force UTC/GMT */
#define MAIL_OPT_MUTE				(1<<31)

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif

#ifdef _WIN32
	#ifdef MAILSRVR_EXPORTS
		#define DLLEXPORT __declspec(dllexport)
	#else
		#define DLLEXPORT __declspec(dllimport)
	#endif
	#ifdef __BORLANDC__
		#define DLLCALL __stdcall
	#else
		#define DLLCALL
	#endif
#else
	#define DLLEXPORT
	#define DLLCALL
#endif

#ifdef __cplusplus
extern "C" {
#endif
/* arg is pointer to static mail_startup_t* */
DLLEXPORT void			DLLCALL mail_server(void* arg);
DLLEXPORT void			DLLCALL mail_terminate(void);
DLLEXPORT const	char*	DLLCALL mail_ver(void);
#ifdef __cplusplus
}
#endif

int sockprintf(SOCKET sock, char *fmt, ...);

#endif /* Don't add anything after this line */
