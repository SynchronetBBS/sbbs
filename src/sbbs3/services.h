/* services.h */

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

#ifndef _SERVICES_H_
#define _SERVICES_H_

#ifdef _WIN32
#include <windows.h>
#endif

#include "client.h"

typedef struct {

	DWORD	size;				/* sizeof(bbs_struct_t) */
    DWORD   interface_addr;
    DWORD	options;			/* See BBS_OPT definitions */
    DWORD	js_max_bytes;
	DWORD	js_cx_stack;
	DWORD	js_branch_limit;
	DWORD	js_yield_interval;
	DWORD	js_gc_interval;
	WORD	sem_chk_freq;		/* semaphore file checking frequency (in seconds) */
	int 	(*lputs)(char*);		/* Log - put string */
	void	(*status)(char*);
    void	(*started)(void);
    void	(*terminated)(int code);
    void	(*clients)(int active);
    void	(*thread_up)(BOOL up, BOOL setuid);
	void	(*socket_open)(BOOL open);
    void	(*client_on)(BOOL on, int sock, client_t*, BOOL update);
    BOOL	(*seteuid)(BOOL user);
    char    ctrl_dir[128];
    char	cfg_file[128];
	char	ini_file[128];
	char	answer_sound[128];
	char	hangup_sound[128];
    char	host_name[128];
	BOOL	recycle_now;

} services_startup_t;

/* Option bit definitions	*/
#define SERVICE_OPT_UDP			(1<<0)	/* UDP Socket */
#define SERVICE_OPT_STATIC		(1<<1)	/* Static service (accepts client connectsions) */
#define SERVICE_OPT_STATIC_LOOP (1<<2)	/* Loop static service until terminated */
#define SERVICE_OPT_NATIVE		(1<<3)	/* non-JavaScript service */
#define SERVICE_OPT_FULL_ACCEPT	(1<<4)	/* Accept/close connections when server is full */

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
	#ifdef SERVICES_EXPORTS
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
DLLEXPORT void			DLLCALL services_thread(void* arg);
DLLEXPORT void			DLLCALL services_terminate(void);
DLLEXPORT const char*	DLLCALL services_ver(void);

#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
