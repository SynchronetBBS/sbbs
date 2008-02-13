/* mailsrvr.h */

/* Synchronet Mail (SMTP/POP3/SendMail) server */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2008 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include "startup.h"
#include "sockwrap.h"           /* SOCKET */

typedef struct {

	DWORD	size;				/* sizeof(mail_startup_t) */
	WORD	smtp_port;
	WORD	pop3_port;
	WORD	submission_port;
	WORD	max_clients;
	WORD	max_inactivity;
	WORD	max_delivery_attempts;
	WORD	rescan_frequency;	/* In seconds */
	WORD	relay_port;
	WORD	lines_per_yield;
	WORD	max_recipients;
	WORD	sem_chk_freq;		/* semaphore file checking frequency (in seconds) */
    DWORD   interface_addr;
    DWORD	options;			/* See MAIL_OPT definitions */
    DWORD	max_msg_size;

	void*	cbdata;				/* Private data passed to callbacks */ 

	/* Callbacks (NULL if unused) */
	int 	(*lputs)(void*, int, char*);
	void	(*status)(void*, char*);
    void	(*started)(void*);
	void	(*recycle)(void*);
    void	(*terminated)(void*, int code);
    void	(*clients)(void*, int active);
    void	(*thread_up)(void*, BOOL up, BOOL setuid);
	void	(*socket_open)(void*, BOOL open);
    void	(*client_on)(void*, BOOL on, int sock, client_t*, BOOL update);
    BOOL	(*seteuid)(BOOL user);
	BOOL	(*setuid)(BOOL force);

	/* Paths */
    char    ctrl_dir[128];
	char    temp_dir[128];

	/* Strings */
    char	dns_server[128];
    char	default_user[128];
    char	dnsbl_tag[32];		// Tag to add to blacklisted subject
	char	dnsbl_hdr[32];		// Header field to add to msg header
	char	inbound_sound[128];
	char	outbound_sound[128];
    char	pop3_sound[128];
	char	default_charset[128];

	/* Misc */
    char	host_name[128];
	BOOL	recycle_now;
	BOOL	shutdown_now;
	int		log_level;
	uint	bind_retry_count;		/* Number of times to retry bind() calls */
	uint	bind_retry_delay;		/* Time to wait between each bind() retry */

	/* Relay Server */
    char	relay_server[128];
	/* Relay authentication required */
	char	relay_user[128];
	char	relay_pass[128];

	/* JavaScript operating parameters */
	js_startup_t js;

} mail_startup_t;

/* startup options that requires re-initialization/recycle when changed */
#if defined(STARTUP_INIT_FIELD_TABLES)
static struct init_field mail_init_fields[] = { 
	 OFFSET_AND_SIZE(mail_startup_t,smtp_port)
	,OFFSET_AND_SIZE(mail_startup_t,pop3_port)
	,OFFSET_AND_SIZE(mail_startup_t,interface_addr)
	,OFFSET_AND_SIZE(mail_startup_t,ctrl_dir)
	,{ 0,0 }	/* terminator */
};
#endif

#define MAIL_OPT_DEBUG_RX_HEADER		(1<<0)
#define MAIL_OPT_DEBUG_RX_BODY			(1<<1)
#define MAIL_OPT_ALLOW_POP3				(1<<2)
#define MAIL_OPT_DEBUG_TX				(1<<3)
#define MAIL_OPT_DEBUG_RX_RSP			(1<<4)
#define MAIL_OPT_RELAY_TX				(1<<5)	/* Use SMTP relay server */
#define MAIL_OPT_DEBUG_POP3				(1<<6)
#define MAIL_OPT_ALLOW_RX_BY_NUMBER		(1<<7)	/* Receive mail sent to user # */
#define MAIL_OPT_NO_NOTIFY				(1<<8)	/* Don't notify local recipients */
#define MAIL_OPT_ALLOW_SYSOP_ALIASES	(1<<9)	/* Receive mail sent to built-in sysop aliases (i.e. "sysop" and "postmaster") */ 
#define MAIL_OPT_USE_SUBMISSION_PORT	(1<<10)	/* Listen on the "MSA" service port for mail submissions */
#define MAIL_OPT_NO_HOST_LOOKUP			(1<<11)	/* Don't look-up hostnames */
#define MAIL_OPT_USE_TCP_DNS			(1<<12)	/* Use TCP vs UDP for DNS req */
#define MAIL_OPT_NO_SENDMAIL			(1<<13)	/* Don't run SendMail thread */
#define MAIL_OPT_ALLOW_RELAY			(1<<14)	/* Allow relays from stored user IPs */
#define MAIL_OPT_DNSBL_REFUSE			(1<<15) /* Refuse session, return error */
#define MAIL_OPT_DNSBL_IGNORE			(1<<16) /* Dump mail, return success */
#define MAIL_OPT_DNSBL_BADUSER			(1<<17) /* Refuse mail (bad user name) */
#define MAIL_OPT_DNSBL_CHKRECVHDRS		(1<<18)	/* Check all Recieved: from addresses */
#define MAIL_OPT_DNSBL_THROTTLE			(1<<19)	/* Throttle receive from blacklisted servers */
#define MAIL_OPT_DNSBL_DEBUG			(1<<20) /* Debug DNSBL activity */
#define MAIL_OPT_SMTP_AUTH_VIA_IP		(1<<21)	/* Allow SMTP authentication via IP */
#define MAIL_OPT_SEND_INTRANSIT			(1<<22)	/* Send mail, even if already "in transit" */
#define MAIL_OPT_RELAY_AUTH_PLAIN		(1<<23)
#define MAIL_OPT_RELAY_AUTH_LOGIN		(1<<24)
#define MAIL_OPT_RELAY_AUTH_CRAM_MD5	(1<<25)
#define MAIL_OPT_NO_AUTO_EXEMPT			(1<<26)	/* Do not auto DNSBL-exempt recipient e-mail addresses */
#define MAIL_OPT_NO_RECYCLE				(1<<27)	/* Disable recycling of server		*/
#define MAIL_OPT_LOCAL_TIMEZONE			(1<<30)	/* Don't force UTC/GMT */
#define MAIL_OPT_MUTE					(1<<31)

#define MAIL_OPT_RELAY_AUTH_MASK		(MAIL_OPT_RELAY_AUTH_PLAIN|MAIL_OPT_RELAY_AUTH_LOGIN|MAIL_OPT_RELAY_AUTH_CRAM_MD5)

/* mail_startup_t.options bits that require re-init/recycle when changed */
#define MAIL_INIT_OPTS	(MAIL_OPT_ALLOW_POP3|MAIL_OPT_NO_SENDMAIL|MAIL_OPT_LOCAL_TIMEZONE)

#if defined(STARTUP_INI_BITDESC_TABLES)
static ini_bitdesc_t mail_options[] = {

	{ MAIL_OPT_DEBUG_RX_HEADER		,"DEBUG_RX_HEADER"		},
	{ MAIL_OPT_DEBUG_RX_BODY		,"DEBUG_RX_BODY"		},	
	{ MAIL_OPT_ALLOW_POP3			,"ALLOW_POP3"			},
	{ MAIL_OPT_DEBUG_TX				,"DEBUG_TX"				},
	{ MAIL_OPT_DEBUG_RX_RSP			,"DEBUG_RX_RSP"			},
	{ MAIL_OPT_RELAY_TX				,"RELAY_TX"				},
	{ MAIL_OPT_DEBUG_POP3			,"DEBUG_POP3"			},
	{ MAIL_OPT_ALLOW_RX_BY_NUMBER	,"ALLOW_RX_BY_NUMBER"	},
	{ MAIL_OPT_ALLOW_SYSOP_ALIASES	,"ALLOW_SYSOP_ALIASES"	},
	{ MAIL_OPT_USE_SUBMISSION_PORT	,"USE_SUBMISSION_PORT"	},
	{ MAIL_OPT_NO_NOTIFY			,"NO_NOTIFY"			},
	{ MAIL_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ MAIL_OPT_USE_TCP_DNS			,"USE_TCP_DNS"			},
	{ MAIL_OPT_NO_SENDMAIL			,"NO_SENDMAIL"			},
	{ MAIL_OPT_ALLOW_RELAY			,"ALLOW_RELAY"			},
	{ MAIL_OPT_SMTP_AUTH_VIA_IP		,"SMTP_AUTH_VIA_IP"		},
	{ MAIL_OPT_DNSBL_REFUSE			,"DNSBL_REFUSE"			},
	{ MAIL_OPT_DNSBL_IGNORE			,"DNSBL_IGNORE"			},
	{ MAIL_OPT_DNSBL_BADUSER		,"DNSBL_BADUSER"		},
	{ MAIL_OPT_DNSBL_CHKRECVHDRS	,"DNSBL_CHKRECVHDRS"	},
	{ MAIL_OPT_DNSBL_THROTTLE		,"DNSBL_THROTTLE"		},
	{ MAIL_OPT_DNSBL_DEBUG			,"DNSBL_DEBUG"			},
	{ MAIL_OPT_SEND_INTRANSIT		,"SEND_INTRANSIT"		},
	{ MAIL_OPT_RELAY_AUTH_PLAIN		,"RELAY_AUTH_PLAIN"		},
	{ MAIL_OPT_RELAY_AUTH_LOGIN		,"RELAY_AUTH_LOGIN"		},
	{ MAIL_OPT_RELAY_AUTH_CRAM_MD5	,"RELAY_AUTH_CRAM_MD5"	},
	{ MAIL_OPT_NO_AUTO_EXEMPT		,"NO_AUTO_EXEMPT"		},
	{ MAIL_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ MAIL_OPT_LOCAL_TIMEZONE		,"LOCAL_TIMEZONE"		},
	{ MAIL_OPT_MUTE					,"MUTE"					},
	/* terminator */
	{ 0 							,NULL					}
};
#endif

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
