/* websrvr.h */

/* Synchronet Web Server */

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

#ifndef _WEBSRVR_H_
#define _WEBSRVR_H_

#include "client.h"				/* client_t */

typedef struct {
	DWORD	size;				/* sizeof(web_startup_t) */
	WORD	port;
	WORD	max_clients;
	WORD	max_inactivity;
	WORD	reserved_word4;
	WORD	reserved_word3;
	WORD	reserved_word2;
	WORD	reserved_word1;
    DWORD   interface_addr;
    DWORD	options;
    DWORD	js_max_bytes;
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
    char	index_file_name[64];
	char	js_ext[16];			/* Server-Side JavaScript file extension */
	char	reserved_str4[16];
	char	reserved_str3[32];
	char	reserved_str2[64];
	char	reserved_str1[64];
    char    ctrl_dir[128];
    char	root_dir[128];
    char	error_dir[128];
    char	cgi_temp_dir[128];
    char	reserved_path7[128];
    char	reserved_path6[128];
    char	reserved_path5[128];
    char	reserved_path4[128];
    char	reserved_path3[128];
    char	reserved_path2[128];
    char	reserved_path1[128];
    char	host_name[128];
	BOOL	recycle_now;
} web_startup_t;

#define WEB_OPT_DEBUG_RX			(1<<0)	/* Log all received requests		*/
#define WEB_OPT_DEBUG_TX			(1<<1)	/* Log all transmitted responses	*/
#define WEB_OPT_VIRTUAL_HOSTS		(1<<4)	/* Use virutal host html subdirs	*/

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif

#ifdef _WIN32
	#ifdef WEBSRVR_EXPORTS
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
DLLEXPORT void			DLLCALL web_server(void* arg);
DLLEXPORT void			DLLCALL web_terminate(void);
DLLEXPORT const char*	DLLCALL web_ver(void);
#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
