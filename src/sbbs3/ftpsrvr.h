/* ftpsrvr.h */

/* Synchronet FTP server */

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

#ifndef _FTPSRVR_H_
#define _FTPSRVR_H_

#include "client.h"				/* client_t */

typedef struct {

	DWORD	size;				/* sizeof(ftp_startup_t) */
	WORD	port;
	WORD	max_clients;
	WORD	max_inactivity;
	WORD	qwk_timeout;
	WORD	sem_chk_freq;		/* semaphore file checking frequency (in seconds) */
    DWORD   interface_addr;
    DWORD	options;			/* See FTP_OPT definitions */
    DWORD	js_max_bytes;
	DWORD	js_cx_stack;

	void*	cbdata;				/* Private data passed to callbacks */ 

	/* Callbacks (NULL if unused) */
	int 	(*lputs)(void*, char*);
	void	(*status)(void*, char*);
    void	(*started)(void*);
    void	(*terminated)(void*, int code);
    void	(*clients)(void*, int active);
    void	(*thread_up)(void*, BOOL up, BOOL setuid);
	void	(*socket_open)(void*, BOOL open);
    void	(*client_on)(void*, BOOL on, int sock, client_t*, BOOL update);
    BOOL	(*seteuid)(BOOL user);
    BOOL	(*setuid)(BOOL force);

	/* Paths */
    char    ctrl_dir[128];
    char	index_file_name[64];
    char	html_index_file[64];
    char	html_index_script[64];
    char	temp_dir[128];
	char	answer_sound[128];
	char	hangup_sound[128];
    char	hack_sound[128];

	/* Misc */
    char	host_name[128];
	BOOL	recycle_now;

} ftp_startup_t;

#define FTP_OPT_DEBUG_RX			(1<<0)
#define FTP_OPT_DEBUG_DATA			(1<<1)
#define FTP_OPT_INDEX_FILE			(1<<2)	/* Auto-generate ASCII Index files */
#define FTP_OPT_DEBUG_TX			(1<<3)
#define FTP_OPT_ALLOW_QWK			(1<<4)
#define FTP_OPT_NO_LOCAL_FSYS		(1<<5)
#define FTP_OPT_DIR_FILES			(1<<6)	/* Allow access to files in dir but not in database */
#define FTP_OPT_KEEP_TEMP_FILES		(1<<7)	/* Don't delete temp files (for debugging) */
#define FTP_OPT_HTML_INDEX_FILE		(1<<8)	/* Auto-generate HTML index files */
#define FTP_OPT_NO_HOST_LOOKUP		(1<<11)
#define FTP_OPT_NO_RECYCLE			(1<<27)	/* Disable recycling of server		*/
#define FTP_OPT_NO_JAVASCRIPT		(1<<29)	/* JavaScript disabled				*/
#define FTP_OPT_LOCAL_TIMEZONE		(1<<30)	/* Don't force UTC/GMT */
#define FTP_OPT_MUTE				(1<<31)

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif

#ifdef _WIN32
	#ifdef FTPSRVR_EXPORTS
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
/* arg is pointer to static ftp_startup_t */
DLLEXPORT void			DLLCALL ftp_server(void* arg);
DLLEXPORT void			DLLCALL ftp_terminate(void);
DLLEXPORT const char*	DLLCALL ftp_ver(void);
#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
