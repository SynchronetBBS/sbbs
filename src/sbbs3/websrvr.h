/* Synchronet Web Server */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _WEBSRVR_H_
#define _WEBSRVR_H_

#include "client.h"             /* client_t */
#include "startup.h"            /* BBS_OPT_* */
#include "semwrap.h"            /* sem_t */

typedef struct {

	STARTUP_COMMON_ELEMENTS
	uint max_clients;
#define WEB_DEFAULT_MAX_CLIENTS         100 /* 0=unlimited */
	uint max_inactivity;
#define WEB_DEFAULT_MAX_INACTIVITY      120 /* seconds */
	uint max_cgi_inactivity;
#define WEB_DEFAULT_MAX_CGI_INACTIVITY  120 /* seconds */
	uint max_concurrent_connections;
#define WEB_DEFAULT_MAX_CON_CONN        10  /* 0=unlimited */
	uint max_requests_per_period;
	uint request_rate_limit_period;
	uint16_t port;
	uint16_t tls_port;
	str_list_t interfaces;
	str_list_t tls_interfaces;

	/* Paths */
	char ssjs_ext[16];                              /* Server-Side JavaScript file extension */
	char** cgi_ext;                                 /* CGI Extensions */
	char cgi_dir[INI_MAX_VALUE_LEN];                /* relative to root_dir (all files executable) */
	char root_dir[INI_MAX_VALUE_LEN];               /* HTML root directory */
	char error_dir[INI_MAX_VALUE_LEN];              /* relative to root_dir */
	char** index_file_name;                         /* Index filenames */
	char logfile_base[INI_MAX_VALUE_LEN];           /* Logfile base name (date is appended) */
	char file_index_script[INI_MAX_VALUE_LEN];
	char file_vpath_prefix[INI_MAX_VALUE_LEN];
	bool file_vpath_for_vhosts;

	/* Misc */
	int tls_error_level;            /* Cap the severity of TLS error log messages */
	char default_cgi_content[128];
	char default_auth_list[128];
	uint outbuf_drain_timeout;
	char login_info_save[INI_MAX_VALUE_LEN];
	char proxy_ip_header[128];
	char custom_log_fmt[INI_MAX_VALUE_LEN];

	/* JavaScript operating parameters */
	js_startup_t js;

} web_startup_t;

#if defined(STARTUP_INIT_FIELD_TABLES)
/* startup options that requires re-initialization/recycle when changed */
static struct init_field web_init_fields[] = {
	OFFSET_AND_SIZE(web_startup_t, port)
	, OFFSET_AND_SIZE(web_startup_t, interfaces)
	, OFFSET_AND_SIZE(web_startup_t, ctrl_dir)
	, OFFSET_AND_SIZE(web_startup_t, root_dir)
	, OFFSET_AND_SIZE(web_startup_t, error_dir)
	, OFFSET_AND_SIZE(web_startup_t, cgi_dir)
	, OFFSET_AND_SIZE(web_startup_t, logfile_base)
	, { 0, 0 }    /* terminator */
};
#endif

#define WEB_OPT_DEBUG_RX            (1 << 0)  /* Log all received requests		*/
#define WEB_OPT_DEBUG_TX            (1 << 1)  /* Log all transmitted responses	*/
#define WEB_OPT_DEBUG_SSJS          (1 << 2)  /* Don't delete sbbs_ssjs.*.html	*/
#define WEB_OPT_VIRTUAL_HOSTS       (1 << 4)  /* Use virtual host html subdirs	*/
#define WEB_OPT_NO_CGI              (1 << 5)  /* Disable CGI support				*/
#define WEB_OPT_HTTP_LOGGING        (1 << 6)  /* Create/write-to HttpLogFile		*/
#define WEB_OPT_ALLOW_TLS           (1 << 7)  /* Enable HTTPS support				*/
#define WEB_OPT_HSTS_SAFE           (1 << 8)  /* All URLs can be served over HTTPS*/
#define WEB_OPT_NO_HTTP             (1 << 9)  /* Disable HTTP support				*/
#define WEB_OPT_NO_FILEBASE         (1 << 10) /* Disable FileBase support			*/
#define WEB_OPT_ONE_HTTP_LOG        (1 << 12) /* Don't use requested-host in log filenames */

/* web_startup_t.options bits that require re-init/recycle when changed */
#define WEB_INIT_OPTS   (WEB_OPT_HTTP_LOGGING)

#if defined(STARTUP_INI_BITDESC_TABLES)
static ini_bitdesc_t web_options[] = {

	{ WEB_OPT_DEBUG_RX, "DEBUG_RX"             },
	{ WEB_OPT_DEBUG_TX, "DEBUG_TX"             },
	{ WEB_OPT_DEBUG_SSJS, "DEBUG_SSJS"           },
	{ WEB_OPT_VIRTUAL_HOSTS, "VIRTUAL_HOSTS"        },
	{ WEB_OPT_NO_CGI, "NO_CGI"               },
	{ WEB_OPT_NO_HTTP, "NO_HTTP"              },
	{ WEB_OPT_HTTP_LOGGING, "HTTP_LOGGING"         },
	{ WEB_OPT_ALLOW_TLS, "ALLOW_TLS"            },
	{ WEB_OPT_HSTS_SAFE, "HSTS_SAFE"            },
	{ WEB_OPT_NO_FILEBASE, "NO_FILEBASE"          },
	{ WEB_OPT_ONE_HTTP_LOG, "ONE_HTTP_LOG"          },

	/* shared bits */
	{ BBS_OPT_NO_HOST_LOOKUP, "NO_HOST_LOOKUP"       },
	{ BBS_OPT_NO_RECYCLE, "NO_RECYCLE"           },
	{ BBS_OPT_NO_JAVASCRIPT, "NO_JAVASCRIPT"        },
	{ BBS_OPT_MUTE, "MUTE"                 },
	{ BBS_OPT_HAPROXY_PROTO, "HAPROXY_PROTO" },

	/* terminator */
	{ 0, NULL                   }
};
#endif

#define WEB_DEFAULT_ROOT_DIR        "../web/root"
#define WEB_DEFAULT_ERROR_DIR       "error"
#define WEB_DEFAULT_CGI_DIR         "cgi-bin"
#define WEB_DEFAULT_AUTH_LIST       "Basic,Digest,TLS-PSK"
#define WEB_DEFAULT_CGI_CONTENT     "text/plain"

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif

#ifdef _WIN32
	#ifdef WEBSRVR_EXPORTS
		#define DLLEXPORT __declspec(dllexport)
	#else
		#define DLLEXPORT __declspec(dllimport)
	#endif
#else
	#define DLLEXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif
DLLEXPORT void          web_server(void* arg);
DLLEXPORT void          web_terminate(void);
DLLEXPORT const char*   web_ver(void);
#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
