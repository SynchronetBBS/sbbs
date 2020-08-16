/* Synchronet FTP server */

/* $Id: ftpsrvr.h,v 1.58 2019/03/22 21:28:27 rswindell Exp $ */

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

#ifndef _FTPSRVR_H_
#define _FTPSRVR_H_

#include "startup.h"

typedef struct {

	DWORD	size;				/* sizeof(ftp_startup_t) */
	WORD	port;
	WORD	max_clients;
#define FTP_DEFAULT_MAX_CLIENTS		10
	WORD	max_inactivity;
#define FTP_DEFAULT_MAX_INACTIVITY	300
	WORD	qwk_timeout;
#define FTP_DEFAULT_QWK_TIMEOUT		600
	WORD	sem_chk_freq;		/* semaphore file checking frequency (in seconds) */
	struct in_addr outgoing4;
	struct in6_addr	outgoing6;
    str_list_t	interfaces;
	struct in_addr pasv_ip_addr;
	struct in6_addr	pasv_ip6_addr;
	WORD	pasv_port_low;
	WORD	pasv_port_high;
    DWORD	options;			/* See FTP_OPT definitions */
	uint64_t	min_fsize;		/* Minimum file size accepted for upload */
	uint64_t	max_fsize;		/* Maximum file size accepted for upload (0=unlimited) */

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
    char	index_file_name[64];
    char	html_index_file[64];
    char	html_index_script[64];
    char	temp_dir[128];
	char	answer_sound[128];
	char	hangup_sound[128];
    char	hack_sound[128];
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

} ftp_startup_t;

/* startup options that requires re-initialization/recycle when changed */
#if defined(STARTUP_INIT_FIELD_TABLES)
static struct init_field ftp_init_fields[] = { 
	 OFFSET_AND_SIZE(ftp_startup_t,port)
	,OFFSET_AND_SIZE(ftp_startup_t,interfaces)
	,OFFSET_AND_SIZE(ftp_startup_t,ctrl_dir)
	,OFFSET_AND_SIZE(ftp_startup_t,temp_dir)
	,{ 0,0 }	/* terminator */
};
#endif

#define FTP_OPT_DEBUG_RX			(1<<0)
#define FTP_OPT_DEBUG_DATA			(1<<1)
#define FTP_OPT_INDEX_FILE			(1<<2)	/* Auto-generate ASCII Index files */
#define FTP_OPT_DEBUG_TX			(1<<3)
#define FTP_OPT_ALLOW_QWK			(1<<4)
#define FTP_OPT_NO_LOCAL_FSYS		(1<<5)
#define FTP_OPT_DIR_FILES			(1<<6)	/* Allow access to files in dir but not in database */
#define FTP_OPT_KEEP_TEMP_FILES		(1<<7)	/* Don't delete temp files (for debugging) */
#define FTP_OPT_HTML_INDEX_FILE		(1<<8)	/* Auto-generate HTML index files */
#define FTP_OPT_LOOKUP_PASV_IP		(1<<9)	/* resolve public IP address for PASV response */
#define FTP_OPT_NO_HOST_LOOKUP		(1<<11)
#define FTP_OPT_NO_RECYCLE			(1<<27)	/* Disable recycling of server		*/
#define FTP_OPT_NO_JAVASCRIPT		(1<<29)	/* JavaScript disabled				*/
#define FTP_OPT_MUTE				(1<<31)

/* ftp_startup_t.options bits that require re-init/recycle when changed */
#define FTP_INIT_OPTS	(0)

#if defined(STARTUP_INI_BITDESC_TABLES)
static ini_bitdesc_t ftp_options[] = {

	{ FTP_OPT_DEBUG_RX				,"DEBUG_RX"				},
	{ FTP_OPT_DEBUG_DATA			,"DEBUG_DATA"			},	
	{ FTP_OPT_INDEX_FILE			,"INDEX_FILE"			},
	{ FTP_OPT_DEBUG_TX				,"DEBUG_TX"				},
	{ FTP_OPT_ALLOW_QWK				,"ALLOW_QWK"			},
	{ FTP_OPT_NO_LOCAL_FSYS			,"NO_LOCAL_FSYS"		},
	{ FTP_OPT_DIR_FILES				,"DIR_FILES"			},
	{ FTP_OPT_KEEP_TEMP_FILES		,"KEEP_TEMP_FILES"		},
	{ FTP_OPT_HTML_INDEX_FILE		,"HTML_INDEX_FILE"		},
	{ FTP_OPT_LOOKUP_PASV_IP		,"LOOKUP_PASV_IP"		},
	{ FTP_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ FTP_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ FTP_OPT_NO_JAVASCRIPT			,"NO_JAVASCRIPT"		},
	{ FTP_OPT_MUTE					,"MUTE"					},
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
	#ifdef FTPSRVR_EXPORTS
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
/* arg is pointer to static ftp_startup_t */
DLLEXPORT void			DLLCALL ftp_server(void* arg);
DLLEXPORT void			DLLCALL ftp_terminate(void);
DLLEXPORT const char*	DLLCALL ftp_ver(void);
#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
