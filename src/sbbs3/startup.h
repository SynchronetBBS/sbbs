/* startup.h */

/* Synchronet main/telnet server thread startup structure */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifdef _WIN32
#include <windows.h>
#endif

#include "client.h"
#include "ringbuf.h"
#include "threadwrap.h"	/* sem_t */

typedef struct {

	DWORD	size;				// sizeof(bbs_struct_t)
    WORD	first_node;
    WORD	last_node;
	WORD	telnet_port;
	WORD	rlogin_port;
	WORD	outbuf_highwater_mark;	// output block size control
	WORD	outbuf_drain_timeout;
	WORD	sem_chk_freq;		/* semaphore file checking frequency (in seconds) */
    DWORD   telnet_interface;
    DWORD	options;			// See BBS_OPT definitions
    DWORD	rlogin_interface;
    DWORD	xtrn_polls_before_yield;
    DWORD	js_max_bytes;
	DWORD	js_cx_stack;
	DWORD	js_branch_limit;
	DWORD	js_yield_interval;
	DWORD	js_gc_interval;
    RingBuf** node_spybuf;		// Spy output buffer (each node)
    RingBuf** node_inbuf;		// User input buffer (each node)
    sem_t**	node_spysem;		// Spy output semaphore (each node)
    int 	(*event_log)(char*);	// Event log - put string
	int 	(*lputs)(char*);		// Log - put string
	void	(*status)(char*);
    void	(*started)(void);
    void	(*terminated)(int code);
    void	(*clients)(int active);
    void	(*thread_up)(BOOL up, BOOL setuid);
	void	(*socket_open)(BOOL open);
    void	(*client_on)(BOOL on, int sock, client_t*, BOOL update);
    BOOL	(*seteuid)(BOOL user);	// Set Unix uid for thread (bind)
	BOOL	(*setuid)(BOOL force);
    char    ctrl_dir[128];
    char	dosemu_path[128];
    char	temp_dir[128];
	char	answer_sound[128];
	char	hangup_sound[128];
	char	xtrn_term_ansi[32];		/* external ANSI terminal type (e.g. "ansi-bbs") */
	char	xtrn_term_dumb[32];		/* external dumb terminal type (e.g. "dumb") */
    char	host_name[128];
	BOOL	recycle_now;

} bbs_startup_t;

#define BBS_OPT_KEEP_ALIVE			(1<<0)	/* Send keep-alives					*/
#define BBS_OPT_XTRN_MINIMIZED		(1<<1)	/* Run externals minimized			*/
#define BBS_OPT_AUTO_LOGON			(1<<2)	/* Auto-logon via IP				*/
#define BBS_OPT_DEBUG_TELNET		(1<<3)	/* Debug telnet commands			*/
#define BBS_OPT_SYSOP_AVAILABLE		(1<<4)	/* Available for chat				*/
#define BBS_OPT_ALLOW_RLOGIN		(1<<5)	/* Allow logins via BSD RLogin		*/
#define BBS_OPT_USE_2ND_RLOGIN		(1<<6)	/* Use 2nd username in BSD RLogin	*/
#define BBS_OPT_NO_QWK_EVENTS		(1<<7)	/* Don't run QWK-related events		*/
#define BBS_OPT_NO_TELNET_GA		(1<<8)	/* Don't send periodic Telnet GAs	*/
#define BBS_OPT_NO_EVENTS			(1<<9)	/* Don't run event thread */
#define BBS_OPT_NO_HOST_LOOKUP		(1<<11)
#define BBS_OPT_NO_RECYCLE			(1<<27)	/* Disable recycling of server		*/
#define BBS_OPT_GET_IDENT			(1<<28)	/* Get Identity (RFC 1413)			*/
#define BBS_OPT_NO_JAVASCRIPT		(1<<29)	/* JavaScript disabled				*/
#define BBS_OPT_LOCAL_TIMEZONE		(1<<30)	/* Don't force UTC/GMT				*/
#define BBS_OPT_MUTE				(1<<31)	/* Mute sounds						*/

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
