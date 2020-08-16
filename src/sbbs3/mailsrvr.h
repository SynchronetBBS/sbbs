/* Synchronet Mail (SMTP/POP3/SendMail) server */

/* $Id: mailsrvr.h,v 1.88 2019/03/22 21:28:27 rswindell Exp $ */
// vi: tabstop=4

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
	WORD	pop3s_port;
	WORD	submission_port;
	WORD	submissions_port;
	WORD	max_clients;
#define MAIL_DEFAULT_MAX_CLIENTS			10
	WORD	max_inactivity;
#define MAIL_DEFAULT_MAX_INACTIVITY			120
	WORD	max_delivery_attempts;
#define MAIL_DEFAULT_MAX_DELIVERY_ATTEMPTS	50
	WORD	rescan_frequency;	/* In seconds */
#define MAIL_DEFAULT_RESCAN_FREQUENCY		3600
	WORD	relay_port;
	WORD	lines_per_yield;	/* 0=none */
#define MAIL_DEFAULT_LINES_PER_YIELD		10
	WORD	max_recipients;
#define MAIL_DEFAULT_MAX_RECIPIENTS			100
	WORD	sem_chk_freq;		/* semaphore file checking frequency (in seconds) */
	struct in_addr outgoing4;
	struct in6_addr	outgoing6;
    str_list_t   interfaces;
    str_list_t   pop3_interfaces;
    DWORD	options;			/* See MAIL_OPT definitions */
    DWORD	max_msg_size;		/* Max msg size in bytes (0=unlimited) */
#define MAIL_DEFAULT_MAX_MSG_SIZE			(20*1024*1024)	/* 20MB */
	DWORD	max_msgs_waiting;	/* Max msgs in user's inbox (0=unlimited) */
#define MAIL_DEFAULT_MAX_MSGS_WAITING		100
	DWORD	connect_timeout;	/* in seconds, for non-blocking connect (0=blocking socket) */
#define MAIL_DEFAULT_CONNECT_TIMEOUT		30		/* seconds */
	void*	cbdata;				/* Private data passed to callbacks */ 

	/* Callbacks (NULL if unused) */
	int 	(*lputs)(void*, int level, const char* msg);
	void	(*errormsg)(void*, int level, const char* msg);
	void	(*status)(void*, const char*);
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
	char	ini_fname[128];

	/* Strings */
    char	dns_server[128];
    char	default_user[128];
    char	dnsbl_tag[32];		// Tag to add to blacklisted subject
	char	dnsbl_hdr[32];		// Header field to add to msg header
	char	inbound_sound[128];
	char	outbound_sound[128];
    char	pop3_sound[128];
	char	default_charset[128];
	char	newmail_notice[256];
	char	forward_notice[256];

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

	/* Login Attempt parameters */
	struct login_attempt_settings login_attempt;
	link_list_t* login_attempt_list;

} mail_startup_t;

/* startup options that requires re-initialization/recycle when changed */
#if defined(STARTUP_INIT_FIELD_TABLES)
static struct init_field mail_init_fields[] = { 
	 OFFSET_AND_SIZE(mail_startup_t,smtp_port)
	,OFFSET_AND_SIZE(mail_startup_t,pop3_port)
	,OFFSET_AND_SIZE(mail_startup_t,pop3s_port)
	,OFFSET_AND_SIZE(mail_startup_t,submission_port)
	,OFFSET_AND_SIZE(mail_startup_t,submissions_port)
	,OFFSET_AND_SIZE(mail_startup_t,interfaces)
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
#define MAIL_OPT_DNSBL_SPAMHASH			(1<<20) /* Store hashes of ignored or tagged messages in spam.hash */
#define MAIL_OPT_SMTP_AUTH_VIA_IP		(1<<21)	/* Allow SMTP authentication via IP */
#define MAIL_OPT_SEND_INTRANSIT			(1<<22)	/* Send mail, even if already "in transit" */
#define MAIL_OPT_RELAY_AUTH_PLAIN		(1<<23)
#define MAIL_OPT_RELAY_AUTH_LOGIN		(1<<24)
#define MAIL_OPT_RELAY_AUTH_CRAM_MD5	(1<<25)
#define MAIL_OPT_NO_AUTO_EXEMPT			(1<<26)	/* Do not auto DNSBL-exempt recipient e-mail addresses */
#define MAIL_OPT_NO_RECYCLE				(1<<27)	/* Disable recycling of server		*/
#define MAIL_OPT_KILL_READ_SPAM			(1<<28)	/* Set the KILLREAD flag on SPAM msgs */
#define MAIL_OPT_TLS_SUBMISSION			(1<<29)	/* Listen on the TLS "MSA" service port for mail submissions */
#define MAIL_OPT_TLS_POP3				(1<<30)	/* POP3S */
#define MAIL_OPT_MUTE					(1<<31)

#define MAIL_OPT_RELAY_AUTH_MASK		(MAIL_OPT_RELAY_AUTH_PLAIN|MAIL_OPT_RELAY_AUTH_LOGIN|MAIL_OPT_RELAY_AUTH_CRAM_MD5)

/* mail_startup_t.options bits that require re-init/recycle when changed */
#define MAIL_INIT_OPTS	(MAIL_OPT_ALLOW_POP3|MAIL_OPT_NO_SENDMAIL)

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
	{ MAIL_OPT_DNSBL_SPAMHASH		,"DNSBL_SPAMHASH"		},
	{ MAIL_OPT_SEND_INTRANSIT		,"SEND_INTRANSIT"		},
	{ MAIL_OPT_RELAY_AUTH_PLAIN		,"RELAY_AUTH_PLAIN"		},
	{ MAIL_OPT_RELAY_AUTH_LOGIN		,"RELAY_AUTH_LOGIN"		},
	{ MAIL_OPT_RELAY_AUTH_CRAM_MD5	,"RELAY_AUTH_CRAM_MD5"	},
	{ MAIL_OPT_NO_AUTO_EXEMPT		,"NO_AUTO_EXEMPT"		},
	{ MAIL_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ MAIL_OPT_KILL_READ_SPAM		,"KILL_READ_SPAM"		},
	{ MAIL_OPT_TLS_SUBMISSION		,"TLS_SUBMISSION"		},
	{ MAIL_OPT_TLS_POP3				,"TLS_POP3"				},
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
		#define DLLCALL
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

/* for mxlookup.c: */
void mail_open_socket(SOCKET sock, void* cb_protocol);
int mail_close_socket(SOCKET *sock, int *sess);
#ifdef __cplusplus
}
#endif

#if defined(__GNUC__)   // passing an empty string to sockprintf() is expected/valid
#pragma GCC diagnostic ignored "-Wformat-zero-length"
#endif

int sockprintf(SOCKET sock, const char* prot, int sess, char *fmt, ...)
#if defined(__GNUC__)   // Catch printf-format errors 
	__attribute__ ((format (printf, 4, 5)));
#endif
;

#endif /* Don't add anything after this line */
