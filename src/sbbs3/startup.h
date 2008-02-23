/* startup.h */

/* Synchronet main/telnet server thread startup structure */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2006 Rob Swindell - http://www.synchro.net/copyright.html		*
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

typedef struct {
	ulong	max_bytes;		/* max allocated bytes before garbage collection */
	ulong	cx_stack;		/* bytes for script execution stack */
	ulong	thread_stack;	/* limit of stack size for native execution thread */
	ulong	branch_limit;	/* maximum number of branches (for infinite loop detection) */
	ulong	gc_interval;	/* number of branches between garbage collection attempts */
	ulong	yield_interval;	/* number of branches between time-slice yields */
} js_startup_t;

typedef struct {

	char	ctrl_dir[INI_MAX_VALUE_LEN];
	char	temp_dir[INI_MAX_VALUE_LEN];
	char	host_name[INI_MAX_VALUE_LEN];
	ushort	sem_chk_freq;
	ulong	interface_addr;
	int		log_level;
	js_startup_t js;
	uint	bind_retry_count;		/* Number of times to retry bind() calls */
	uint	bind_retry_delay;		/* Time to wait between each bind() retry */

} global_startup_t;

typedef struct {

	DWORD	size;				/* sizeof(bbs_struct_t) */
    WORD	first_node;
    WORD	last_node;
	WORD	telnet_port;
	WORD	rlogin_port;
	WORD	ssh_port;
	WORD	outbuf_highwater_mark;	/* output block size control */
	WORD	outbuf_drain_timeout;
	WORD	sem_chk_freq;		/* semaphore file checking frequency (in seconds) */
    DWORD   telnet_interface;
    DWORD	options;			/* See BBS_OPT definitions */
    DWORD	rlogin_interface;
	DWORD	ssh_interface;
    RingBuf** node_spybuf;			/* Spy output buffer (each node)	*/
    RingBuf** node_inbuf;			/* User input buffer (each node)	*/
    sem_t**	node_spysem;			/* Spy output semaphore (each node)	*/

	void*	cbdata;					/* Private data passed to callbacks */ 

	/* Callbacks (NULL if unused) */
	int 	(*lputs)(void*, int, char*);	/* Log - put string					*/
    int 	(*event_lputs)(int, char*);		/* Event log - put string			*/
	void	(*status)(void*, char*);
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

} bbs_startup_t;

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
	,OFFSET_AND_SIZE(bbs_startup_t,telnet_interface)
	,OFFSET_AND_SIZE(bbs_startup_t,rlogin_interface)
	,OFFSET_AND_SIZE(bbs_startup_t,ctrl_dir)
	,OFFSET_AND_SIZE(bbs_startup_t,temp_dir)
	,{ 0,0 }	/* terminator */
};
#endif

#define BBS_OPT_XTRN_MINIMIZED		(1<<1)	/* Run externals minimized			*/
#define BBS_OPT_AUTO_LOGON			(1<<2)	/* Auto-logon via IP				*/
#define BBS_OPT_DEBUG_TELNET		(1<<3)	/* Debug telnet commands			*/
#define BBS_OPT_SYSOP_AVAILABLE		(1<<4)	/* Available for chat				*/
#define BBS_OPT_ALLOW_RLOGIN		(1<<5)	/* Allow logins via BSD RLogin		*/
#define BBS_OPT_USE_2ND_RLOGIN		(1<<6)	/* Use 2nd username in BSD RLogin	*/
#define BBS_OPT_NO_QWK_EVENTS		(1<<7)	/* Don't run QWK-related events		*/
#define BBS_OPT_NO_TELNET_GA		(1<<8)	/* Don't send periodic Telnet GAs	*/
#define BBS_OPT_NO_EVENTS			(1<<9)	/* Don't run event thread			*/
#define BBS_OPT_NO_SPY_SOCKETS		(1<<10)	/* Don't create spy sockets			*/
#define BBS_OPT_NO_HOST_LOOKUP		(1<<11)
#define BBS_OPT_ALLOW_SSH			(1<<12)	/* Allow logins via BSD SSH			*/
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
	{ BBS_OPT_USE_2ND_RLOGIN		,"USE_2ND_RLOGIN"		},
	{ BBS_OPT_NO_QWK_EVENTS			,"NO_QWK_EVENTS"		},
	{ BBS_OPT_NO_TELNET_GA			,"NO_TELNET_GA"			},
	{ BBS_OPT_NO_EVENTS				,"NO_EVENTS"			},
	{ BBS_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ BBS_OPT_NO_SPY_SOCKETS		,"NO_SPY_SOCKETS"		},
	{ BBS_OPT_ALLOW_SSH				,"ALLOW_SSH"			},
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
		#define DLLCALL __stdcall
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
