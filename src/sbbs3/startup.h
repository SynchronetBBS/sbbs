/* Synchronet main/telnet server thread startup structure */

/* $Id: startup.h,v 1.84 2019/03/22 21:28:27 rswindell Exp $ */
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

#ifndef _STARTUP_H_
#define _STARTUP_H_

#include <stddef.h>		/* offsetof */
#include "client.h"
#include "ringbuf.h"
#include "semwrap.h"	/* sem_t */
#include "ini_file.h"	/* INI_MAX_VALUE_LEN */
#include "sbbsdefs.h"	/* LOG_* (syslog.h) values */
#ifndef LINK_LIST_THREADSAFE
	#define LINK_LIST_THREADSAFE
#endif
#include "link_list.h"

typedef struct {
	ulong	max_bytes;		/* max allocated bytes before garbage collection */
	ulong	cx_stack;		/* bytes for script execution stack */
	ulong	time_limit;		/* maximum number of ticks (for infinite loop detection) */
	ulong	gc_interval;	/* number of ticks between garbage collection attempts */
	ulong	yield_interval;	/* number of ticks between time-slice yields */
	char	load_path[INI_MAX_VALUE_LEN];	/* additional (comma-separated) directories to search for load()ed scripts */
} js_startup_t;

/* Login Attempt parameters */
struct login_attempt_settings {
	ulong	delay;				/* in milliseconds */
	ulong	throttle;			/* in milliseconds */
	ulong	hack_threshold;
	ulong	tempban_threshold;
	ulong	tempban_duration;	/* in seconds */
	ulong	filter_threshold;
};

typedef struct {

	char	ctrl_dir[INI_MAX_VALUE_LEN];
	char	temp_dir[INI_MAX_VALUE_LEN];
	char	host_name[INI_MAX_VALUE_LEN];
	ushort	sem_chk_freq;
	struct in_addr		outgoing4;
	struct in6_addr	outgoing6;
	str_list_t		interfaces;
	int		log_level;
	js_startup_t js;
	uint	bind_retry_count;		/* Number of times to retry bind() calls */
	uint	bind_retry_delay;		/* Time to wait between each bind() retry */
	struct login_attempt_settings login_attempt;

} global_startup_t;

typedef struct {

	DWORD	size;				/* sizeof(bbs_struct_t) */
    WORD	first_node;
    WORD	last_node;
	WORD	telnet_port;
	WORD	rlogin_port;
	WORD	pet40_port;			// 40-column PETSCII terminal server
	WORD	pet80_port;			// 80-column PETSCII terminal server
	WORD	ssh_port;
	WORD	ssh_connect_timeout;
	WORD	outbuf_highwater_mark;	/* output block size control */
	WORD	outbuf_drain_timeout;
	WORD	sem_chk_freq;		/* semaphore file checking frequency (in seconds) */
	struct in_addr outgoing4;
	struct in6_addr	outgoing6;
    str_list_t	telnet_interfaces;
    uint32_t	options;			/* See BBS_OPT definitions */
    str_list_t	rlogin_interfaces;
    str_list_t	ssh_interfaces;
    RingBuf** node_spybuf;			/* Spy output buffer (each node)	*/
    RingBuf** node_inbuf;			/* User input buffer (each node)	*/
    sem_t**	node_spysem;			/* Spy output semaphore (each node)	*/

	void*	cbdata;					/* Private data passed to callbacks */ 
	void*	event_cbdata;			/* Private data passed to event_lputs callback */

	/* Callbacks (NULL if unused) */
	int 	(*lputs)(void*, int , const char*);			/* Log - put string					*/
    int 	(*event_lputs)(void*, int, const char*);	/* Event log - put string			*/
	void	(*errormsg)(void*, int level, const char* msg);
	void	(*status)(void*, const char*);
    void	(*started)(void*);
	void	(*recycle)(void*);
    void	(*terminated)(void*, int code);
    void	(*clients)(void*, int active);
    void	(*thread_up)(void*, BOOL up, BOOL setuid);
	void	(*socket_open)(void*, BOOL open);
    void	(*client_on)(void*, BOOL on, int sock, client_t*, BOOL update);
    BOOL	(*seteuid)(BOOL user);	/* Set Unix uid for thread (bind) */
	BOOL	(*setuid)(BOOL force);

	/* Paths */
    char    ctrl_dir[128];
    char	dosemu_path[128];
    char	temp_dir[128];
	char	answer_sound[128];
	char	hangup_sound[128];
	char	ini_fname[128];

	/* Miscellaneous */
	char	xtrn_term_ansi[32];		/* external ANSI terminal type (e.g. "ansi-bbs") */
	char	xtrn_term_dumb[32];		/* external dumb terminal type (e.g. "dumb") */
	char	host_name[128];
	BOOL	recycle_now;
	BOOL	shutdown_now;
	int		log_level;
	uint	bind_retry_count;		/* Number of times to retry bind() calls */
	uint	bind_retry_delay;		/* Time to wait between each bind() retry */

	/* JavaScript operating parameters */
	js_startup_t js;

	struct login_attempt_settings login_attempt;
	link_list_t* login_attempt_list;
	uint	max_concurrent_connections;

} bbs_startup_t;

#define DEFAULT_SEM_CHK_FREQ	2

/* startup options that requires re-initialization/recycle when changed */
#define OFFSET_AND_SIZE(s, f)	{ offsetof(s,f), sizeof(((s *)0)->f) }

#if defined(STARTUP_INIT_FIELD_TABLES)
static struct init_field {
	size_t	offset;
	size_t	size;
} bbs_init_fields[] = { 
	 OFFSET_AND_SIZE(bbs_startup_t,first_node)
	,OFFSET_AND_SIZE(bbs_startup_t,last_node)
	,OFFSET_AND_SIZE(bbs_startup_t,telnet_port)
	,OFFSET_AND_SIZE(bbs_startup_t,rlogin_port)
	,OFFSET_AND_SIZE(bbs_startup_t,telnet_interfaces)
	,OFFSET_AND_SIZE(bbs_startup_t,rlogin_interfaces)
	,OFFSET_AND_SIZE(bbs_startup_t,ssh_interfaces)
	,OFFSET_AND_SIZE(bbs_startup_t,ctrl_dir)
	,OFFSET_AND_SIZE(bbs_startup_t,temp_dir)
	,{ 0,0 }	/* terminator */
};
#endif

#define BBS_OPT_XTRN_MINIMIZED		(1<<1)	/* Run externals minimized			*/
#define BBS_OPT_AUTO_LOGON			(1<<2)	/* Auto-logon via IP				*/
#define BBS_OPT_DEBUG_TELNET		(1<<3)	/* Debug telnet commands			*/
#define BBS_OPT_SYSOP_AVAILABLE		(1<<4)	/* Available for chat - DEPRECATED (controlled via semfile) */
#define BBS_OPT_ALLOW_RLOGIN		(1<<5)	/* Allow logins via BSD RLogin		*/
#define BBS_OPT_USE_2ND_RLOGIN		(1<<6)	/* Use 2nd username in BSD RLogin - DEPRECATED (Always enabled)	*/
#define BBS_OPT_NO_QWK_EVENTS		(1<<7)	/* Don't run QWK-related events		*/
#define BBS_OPT_NO_TELNET_GA		(1<<8)	/* Don't send periodic Telnet GAs	*/
#define BBS_OPT_NO_EVENTS			(1<<9)	/* Don't run event thread			*/
#define BBS_OPT_NO_SPY_SOCKETS		(1<<10)	/* Don't create spy sockets			*/
#define BBS_OPT_NO_HOST_LOOKUP		(1<<11)
#define BBS_OPT_ALLOW_SSH			(1<<12)	/* Allow logins via BSD SSH			*/
#define BBS_OPT_NO_DOS				(1<<13) /* Don't attempt to run 16-bit DOS programs */
#define BBS_OPT_NO_NEWDAY_EVENTS	(1<<14)	/* Don't check for a new day in event thread */
#define BBS_OPT_NO_RECYCLE			(1<<27)	/* Disable recycling of server		*/
#define BBS_OPT_GET_IDENT			(1<<28)	/* Get Identity (RFC 1413)			*/
#define BBS_OPT_NO_JAVASCRIPT		(1<<29)	/* JavaScript disabled				*/
#define BBS_OPT_MUTE				(1<<31)	/* Mute sounds						*/

/* bbs_startup_t.options bits that require re-init/recycle when changed */
#define BBS_INIT_OPTS	(BBS_OPT_ALLOW_RLOGIN|BBS_OPT_ALLOW_SSH|BBS_OPT_NO_EVENTS|BBS_OPT_NO_SPY_SOCKETS \
						|BBS_OPT_NO_JAVASCRIPT)

#if defined(STARTUP_INI_BITDESC_TABLES)
static ini_bitdesc_t bbs_options[] = {

	{ BBS_OPT_XTRN_MINIMIZED		,"XTRN_MINIMIZED"		},
	{ BBS_OPT_AUTO_LOGON			,"AUTO_LOGON"			},
	{ BBS_OPT_DEBUG_TELNET			,"DEBUG_TELNET"			},
	{ BBS_OPT_SYSOP_AVAILABLE		,"SYSOP_AVAILABLE"		},
	{ BBS_OPT_ALLOW_RLOGIN			,"ALLOW_RLOGIN"			},
	{ BBS_OPT_NO_QWK_EVENTS			,"NO_QWK_EVENTS"		},
	{ BBS_OPT_NO_TELNET_GA			,"NO_TELNET_GA"			},
	{ BBS_OPT_NO_EVENTS				,"NO_EVENTS"			},
	{ BBS_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ BBS_OPT_NO_SPY_SOCKETS		,"NO_SPY_SOCKETS"		},
	{ BBS_OPT_ALLOW_SSH				,"ALLOW_SSH"			},
	{ BBS_OPT_NO_DOS				,"NO_DOS"				},
	{ BBS_OPT_NO_NEWDAY_EVENTS		,"NO_NEWDAY_EVENTS"		},
	{ BBS_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ BBS_OPT_GET_IDENT				,"GET_IDENT"			},
	{ BBS_OPT_NO_JAVASCRIPT			,"NO_JAVASCRIPT"		},
	{ BBS_OPT_MUTE					,"MUTE"					},
	/* terminator */										
	{ 0								,NULL					}
};

#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif

#ifdef _WIN32
	#ifdef SBBS_EXPORTS
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

/* arg is pointer to static bbs_startup_t* */
DLLEXPORT void			DLLCALL bbs_thread(void* arg);
DLLEXPORT void			DLLCALL bbs_terminate(void);
DLLEXPORT const char*	DLLCALL js_ver(void);
DLLEXPORT const char*	DLLCALL bbs_ver(void);
DLLEXPORT long			DLLCALL	bbs_ver_num(void);

#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
