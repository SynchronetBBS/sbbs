/* Synchronet main/telnet server thread startup structure */

/* $Id: services.h,v 1.45 2019/03/22 21:28:27 rswindell Exp $ */

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

#ifndef _SERVICES_H_
#define _SERVICES_H_

#include "startup.h"

typedef struct {

	DWORD	size;				/* sizeof(bbs_struct_t) */
	struct in_addr outgoing4;
	struct in6_addr	outgoing6;
	str_list_t		interfaces;
    DWORD	options;			/* See BBS_OPT definitions */
	WORD	sem_chk_freq;			/* semaphore file checking frequency (in seconds) */

	void*	cbdata;					/* Private data passed to callbacks */ 

	/* Callbacks (NULL if unused) */
	int 	(*lputs)(void*, int level, const char*);		/* Log - put string */
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
	char	answer_sound[128];
	char	hangup_sound[128];
	char	ini_fname[128];

	/* Misc */
    char	host_name[128];
	BOOL	recycle_now;
	BOOL	shutdown_now;
	int		log_level;
	uint	bind_retry_count;		/* Number of times to retry bind() calls */
	uint	bind_retry_delay;		/* Time to wait between each bind() retry */

	/* JavaScript operating parameters */
	js_startup_t js;

	/* Login Attempt parameters */
	struct login_attempt_settings login_attempt;
	link_list_t* login_attempt_list;

} services_startup_t;

#if 0
/* startup options that requires re-initialization/recycle when changed */
static struct init_field services_init_fields[] = { 
	 OFFSET_AND_SIZE(services_startup_t,outgoing4)
	 OFFSET_AND_SIZE(services_startup_t,outgoing6)
	,OFFSET_AND_SIZE(services_startup_t,ctrl_dir)
	,{ 0,0 }	/* terminator */
};
#endif

/* Option bit definitions	*/
#define SERVICE_OPT_UDP			(1<<0)	/* UDP Socket */
#define SERVICE_OPT_STATIC		(1<<1)	/* Static service (accepts client connectsions) */
#define SERVICE_OPT_STATIC_LOOP (1<<2)	/* Loop static service until terminated */
#define SERVICE_OPT_NATIVE		(1<<3)	/* non-JavaScript service */
#define SERVICE_OPT_FULL_ACCEPT	(1<<4)	/* Accept/close connections when server is full */
#define SERVICE_OPT_TLS			(1<<5)	/* Use TLS */

/* services_startup_t.options bits that require re-init/recycle when changed */
#define SERVICE_INIT_OPTS	(0)

#if defined(STARTUP_INI_BITDESC_TABLES) || defined(SERVICES_INI_BITDESC_TABLE)
static ini_bitdesc_t service_options[] = {

	{ BBS_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ BBS_OPT_GET_IDENT				,"GET_IDENT"			},
	{ BBS_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ BBS_OPT_MUTE					,"MUTE"					},
	{ SERVICE_OPT_UDP				,"UDP"					},
	{ SERVICE_OPT_STATIC			,"STATIC"				},
	{ SERVICE_OPT_STATIC_LOOP		,"LOOP"					},
	{ SERVICE_OPT_NATIVE			,"NATIVE"				},
	{ SERVICE_OPT_FULL_ACCEPT		,"FULL_ACCEPT"			},
	{ SERVICE_OPT_TLS				,"TLS"					},
	/* terminator */				
	{ 0 							,NULL					}
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
	#ifdef SERVICES_EXPORTS
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
DLLEXPORT void			DLLCALL services_thread(void* arg);
DLLEXPORT void			DLLCALL services_terminate(void);
DLLEXPORT const char*	DLLCALL services_ver(void);

#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
