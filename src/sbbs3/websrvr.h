/* Synchronet Web Server */

/* $Id: websrvr.h,v 1.57 2020/03/07 00:18:50 deuce Exp $ */

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

#ifndef _WEBSRVR_H_
#define _WEBSRVR_H_

#include "client.h"				/* client_t */
#include "startup.h"			/* BBS_OPT_* */
#include "semwrap.h"			/* sem_t */

typedef struct {
	size_t		size;				/* sizeof(web_startup_t) */
	uint16_t	max_clients;
#define WEB_DEFAULT_MAX_CLIENTS			0	/* 0=unlimited */
	uint16_t	max_inactivity;
#define WEB_DEFAULT_MAX_INACTIVITY		120	/* seconds */
	uint16_t	max_cgi_inactivity;
#define WEB_DEFAULT_MAX_CGI_INACTIVITY	120	/* seconds */
	uint16_t	sem_chk_freq;		/* semaphore file checking frequency (in seconds) */
    uint32_t	options;
	uint16_t	port;
	uint16_t	tls_port;
    str_list_t	interfaces;
    str_list_t	tls_interfaces;
	
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
	BOOL	(*setuid)(BOOL);

	/* Paths */
	char	ssjs_ext[16];			/* Server-Side JavaScript file extension */
	char**	cgi_ext;				/* CGI Extensions */
	char	cgi_dir[128];			/* relative to root_dir (all files executable) */
    char    ctrl_dir[128];
    char	root_dir[128];			/* HTML root directory */
    char	error_dir[128];			/* relative to root_dir */
    char	temp_dir[128];
    char**	index_file_name;		/* Index filenames */
	char	logfile_base[128];		/* Logfile base name (date is appended) */
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
	char	default_cgi_content[128];
	char	default_auth_list[128];
	uint16_t	outbuf_drain_timeout;

	/* JavaScript operating parameters */
	js_startup_t js;

	/* Login Attempt parameters */
	struct login_attempt_settings login_attempt;
	link_list_t* login_attempt_list;

} web_startup_t;

#if defined(STARTUP_INIT_FIELD_TABLES)
/* startup options that requires re-initialization/recycle when changed */
static struct init_field web_init_fields[] = { 
	 OFFSET_AND_SIZE(web_startup_t,port)
	,OFFSET_AND_SIZE(web_startup_t,interfaces)
	,OFFSET_AND_SIZE(web_startup_t,ctrl_dir)
	,OFFSET_AND_SIZE(web_startup_t,root_dir)
	,OFFSET_AND_SIZE(web_startup_t,error_dir)
	,OFFSET_AND_SIZE(web_startup_t,cgi_dir)
	,OFFSET_AND_SIZE(web_startup_t,logfile_base)
	,{ 0,0 }	/* terminator */
};
#endif

#define WEB_OPT_DEBUG_RX			(1<<0)	/* Log all received requests		*/
#define WEB_OPT_DEBUG_TX			(1<<1)	/* Log all transmitted responses	*/
#define WEB_OPT_DEBUG_SSJS			(1<<2)	/* Don't delete sbbs_ssjs.*.html	*/
#define WEB_OPT_VIRTUAL_HOSTS		(1<<4)	/* Use virtual host html subdirs	*/
#define WEB_OPT_NO_CGI				(1<<5)	/* Disable CGI support				*/
#define WEB_OPT_HTTP_LOGGING		(1<<6)	/* Create/write-to HttpLogFile		*/
#define WEB_OPT_ALLOW_TLS			(1<<7)	/* Enable HTTPS						*/
#define WEB_OPT_HSTS_SAFE			(1<<8)	/* All URLs can be served over HTTPS*/

/* web_startup_t.options bits that require re-init/recycle when changed */
#define WEB_INIT_OPTS	(WEB_OPT_HTTP_LOGGING)

#if defined(STARTUP_INI_BITDESC_TABLES)
static ini_bitdesc_t web_options[] = {

	{ WEB_OPT_DEBUG_RX				,"DEBUG_RX"				},
	{ WEB_OPT_DEBUG_TX				,"DEBUG_TX"				},
	{ WEB_OPT_DEBUG_SSJS			,"DEBUG_SSJS"			},
	{ WEB_OPT_VIRTUAL_HOSTS			,"VIRTUAL_HOSTS"		},
	{ WEB_OPT_NO_CGI				,"NO_CGI"				},
	{ WEB_OPT_HTTP_LOGGING			,"HTTP_LOGGING"			},
	{ WEB_OPT_ALLOW_TLS				,"ALLOW_TLS"			},
	{ WEB_OPT_HSTS_SAFE				,"HSTS_SAFE"			},

	/* shared bits */
	{ BBS_OPT_NO_HOST_LOOKUP		,"NO_HOST_LOOKUP"		},
	{ BBS_OPT_NO_RECYCLE			,"NO_RECYCLE"			},
	{ BBS_OPT_NO_JAVASCRIPT			,"NO_JAVASCRIPT"		},
	{ BBS_OPT_MUTE					,"MUTE"					},

	/* terminator */										
	{ 0								,NULL					}
};
#endif

#define WEB_DEFAULT_ROOT_DIR		"../web/root"
#define WEB_DEFAULT_ERROR_DIR		"error"
#define WEB_DEFAULT_CGI_DIR			"cgi-bin"
#define WEB_DEFAULT_AUTH_LIST		"Basic,Digest,TLS-PSK"
#define WEB_DEFAULT_CGI_CONTENT		"text/plain"

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
DLLEXPORT void			DLLCALL web_server(void* arg);
DLLEXPORT void			DLLCALL web_terminate(void);
DLLEXPORT const char*	DLLCALL web_ver(void);
#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
