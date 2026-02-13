/* Synchronet main/telnet server thread startup structure */

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

#ifndef _STARTUP_H_
#define _STARTUP_H_

#include <stddef.h>     /* offsetof */
#include "client.h"
#include "server.h"
#include "ringbuf.h"
#include "semwrap.h"    /* sem_t */
#include "ini_file.h"   /* INI_MAX_VALUE_LEN */
#include "sbbsdefs.h"   /* LOG_* (syslog.h) values */
#ifndef LINK_LIST_THREADSAFE
	#define LINK_LIST_THREADSAFE
#endif
#include "link_list.h"

typedef struct {
	uint max_bytes;         /* max allocated bytes before garbage collection */
	uint time_limit;        /* maximum number of ticks (for infinite loop detection) */
	uint gc_interval;       /* number of ticks between garbage collection attempts */
	uint yield_interval;    /* number of ticks between time-slice yields */
	uint options;
	char load_path[INI_MAX_VALUE_LEN];      /* additional (comma-separated) directories to search for load()ed scripts */
} js_startup_t;

/* Login Attempt parameters */
struct login_attempt_settings {
	uint delay;                 /* in milliseconds */
	uint throttle;              /* in milliseconds */
	uint hack_threshold;
	uint tempban_threshold;
	uint tempban_duration;      /* in seconds */
	uint filter_threshold;
	uint filter_duration;       /* in seconds */
};

struct startup_sound_settings {
	char answer[INI_MAX_VALUE_LEN];
	char login[INI_MAX_VALUE_LEN];
	char logout[INI_MAX_VALUE_LEN];
	char hangup[INI_MAX_VALUE_LEN];
	char hack[INI_MAX_VALUE_LEN];
};

typedef struct {

	size_t size;
	char ctrl_dir[INI_MAX_VALUE_LEN];
	char temp_dir[INI_MAX_VALUE_LEN];
	char host_name[INI_MAX_VALUE_LEN];
	char login_ars[LEN_ARSTR + 1];
	uint16_t sem_chk_freq;
	struct in_addr outgoing4;
	struct in6_addr outgoing6;
	str_list_t interfaces;
	int log_level;
	int tls_error_level;            /* Cap the severity of TLS error log messages */
	js_startup_t js;
	uint bind_retry_count;          /* Number of times to retry bind() calls */
	uint bind_retry_delay;          /* Time to wait between each bind() retry */
	struct login_attempt_settings login_attempt;
	struct startup_sound_settings sound;

} global_startup_t;

// The elements common to all Synchronet server startup structures
// C++ doesn't support anonymous structs, or I would have just used one here :-(
#define STARTUP_COMMON_ELEMENTS \
		int      size; \
		int      type; \
		uint32_t options; \
		int      log_level; \
		char     login_ars[LEN_ARSTR + 1]; \
		void*    cbdata; \
		int (*lputs)(void*, int level, const char*); \
		void (*errormsg)(void*, int level, const char* msg); \
		void (*set_state)(void*, enum server_state); \
		void (*recycle)(void*); \
		void (*terminated)(void*, int code); \
		void (*clients)(void*, int active); \
		void (*thread_up)(void*, bool up, bool setuid); \
		void (*socket_open)(void*, bool open); \
		void (*client_on)(void*, bool on, int sock, client_t*, bool update); \
		bool (*seteuid)(bool user); \
		bool (*setuid)(bool force); \
		char                          ctrl_dir[INI_MAX_VALUE_LEN]; \
		char                          temp_dir[INI_MAX_VALUE_LEN]; \
		char                          ini_fname[INI_MAX_VALUE_LEN]; \
		char                          host_name[128]; \
		bool                          recycle_now; \
		bool                          shutdown_now; \
		bool                          paused; \
		int                           sem_chk_freq; \
		uint                          bind_retry_count; \
		uint                          bind_retry_delay; \
		struct startup_sound_settings sound; \
		struct login_attempt_settings login_attempt; \
		link_list_t*                  login_attempt_list;

struct startup {
	STARTUP_COMMON_ELEMENTS
};

typedef struct {

	STARTUP_COMMON_ELEMENTS
	uint16_t first_node;
	uint16_t last_node;
	uint16_t telnet_port;
	uint16_t rlogin_port;
	uint16_t pet40_port;            // 40-column PETSCII terminal server
	uint16_t pet80_port;            // 80-column PETSCII terminal server
	uint16_t ssh_port;
	uint16_t ssh_connect_timeout;
	int ssh_error_level;
	int event_log_level;
	uint16_t outbuf_highwater_mark;     /* output block size control */
	uint16_t outbuf_drain_timeout;
	struct in_addr outgoing4;
	struct in6_addr outgoing6;
	str_list_t telnet_interfaces;
	str_list_t rlogin_interfaces;
	str_list_t ssh_interfaces;
	RingBuf** node_spybuf;          /* Spy output buffer (each node)	*/
	RingBuf** node_inbuf;           /* User input buffer (each node)	*/

	void* event_cbdata;             /* Private data passed to event_lputs callback */
	int (*event_lputs)(void*, int, const char*);

	/* Paths */
	char dosemu_path[INI_MAX_VALUE_LEN];
	char dosemuconf_path[INI_MAX_VALUE_LEN];
	char web_file_vpath_prefix[INI_MAX_VALUE_LEN];

	/* Miscellaneous */
	uint max_concurrent_connections;
	uint default_term_height;
	uint default_term_width;
	bool usedosemu;
	char xtrn_term_ansi[32];        /* external ANSI terminal type (e.g. "ansi-bbs") */
	char xtrn_term_dumb[32];        /* external dumb terminal type (e.g. "dumb") */
	uint16_t max_dumbterm_inactivity;   // seconds
	uint16_t max_login_inactivity;      // seconds
	uint16_t max_newuser_inactivity;    // seconds
	uint16_t max_session_inactivity;    // seconds
	uint16_t max_sftp_inactivity;       // seconds

	/* JavaScript operating parameters */
	js_startup_t js;

} bbs_startup_t;

#define DEFAULT_SEM_CHK_FREQ    2

/* startup options that requires re-initialization/recycle when changed */
#define OFFSET_AND_SIZE(s, f)   { offsetof(s, f), sizeof(((s *)0)->f) }

#if defined(STARTUP_INIT_FIELD_TABLES)
static struct init_field {
	size_t offset;
	size_t size;
} bbs_init_fields[] = {
	OFFSET_AND_SIZE(bbs_startup_t, first_node)
	, OFFSET_AND_SIZE(bbs_startup_t, last_node)
	, OFFSET_AND_SIZE(bbs_startup_t, telnet_port)
	, OFFSET_AND_SIZE(bbs_startup_t, rlogin_port)
	, OFFSET_AND_SIZE(bbs_startup_t, telnet_interfaces)
	, OFFSET_AND_SIZE(bbs_startup_t, rlogin_interfaces)
	, OFFSET_AND_SIZE(bbs_startup_t, ssh_interfaces)
	, OFFSET_AND_SIZE(bbs_startup_t, ctrl_dir)
	, OFFSET_AND_SIZE(bbs_startup_t, temp_dir)
	, { 0, 0 }    /* terminator */
};
#endif

#define BBS_OPT_XTRN_MINIMIZED      (1 << 1)  /* Run externals minimized			*/
#define BBS_OPT_AUTO_LOGON          (1 << 2)  /* Auto-logon via IP				*/
#define BBS_OPT_DEBUG_TELNET        (1 << 3)  /* Debug telnet commands			*/
#define BBS_OPT_ALLOW_RLOGIN        (1 << 5)  /* Allow logins via BSD RLogin		*/
#define BBS_OPT_USE_2ND_RLOGIN      (1 << 6)  /* Use 2nd username in BSD RLogin - DEPRECATED (Always enabled)	*/
#define BBS_OPT_NO_QWK_EVENTS       (1 << 7)  /* Don't run QWK-related events		*/
#define BBS_OPT_NO_TELNET_GA        (1 << 8)  /* Don't send periodic Telnet GAs	*/
#define BBS_OPT_NO_EVENTS           (1 << 9)  /* Don't run event thread			*/
#define BBS_OPT_NO_SPY_SOCKETS      (1 << 10) /* Don't create spy sockets			*/
#define BBS_OPT_NO_HOST_LOOKUP      (1 << 11)
#define BBS_OPT_ALLOW_SSH           (1 << 12) /* Allow logins via BSD SSH			*/
#define BBS_OPT_NO_DOS              (1 << 13) /* Don't attempt to run 16-bit DOS programs */
#define BBS_OPT_NO_NEWDAY_EVENTS    (1 << 14) /* Don't check for a new day in event thread */
#define BBS_OPT_NO_TELNET           (1 << 15) /* Don't accept incoming telnet connections */
#define BBS_OPT_ALLOW_SFTP          (1 << 16) /* Allow logins via BSD SFTP		*/
#define BBS_OPT_SSH_ANYAUTH         (1 << 17) /* Blindly accept any SSH credentials */
#define BBS_OPT_HAPROXY_PROTO       (1 << 26) /* Incoming requests are via HAproxy */
#define BBS_OPT_NO_RECYCLE          (1 << 27) /* Disable recycling of server		*/
#define BBS_OPT_GET_IDENT           (1 << 28) /* Get Identity (RFC 1413)			*/
#define BBS_OPT_NO_JAVASCRIPT       (1 << 29) /* JavaScript disabled - Not supported */
#define BBS_OPT_MUTE                (1U << 31)/* Mute sounds - DEPRECATED (controlled via semfile) */

/* bbs_startup_t.options bits that require re-init/recycle when changed */
#define BBS_INIT_OPTS   (BBS_OPT_ALLOW_RLOGIN | BBS_OPT_ALLOW_SSH | BBS_OPT_NO_EVENTS | BBS_OPT_NO_SPY_SOCKETS \
						 | BBS_OPT_NO_JAVASCRIPT | BBS_OPT_HAPROXY_PROTO)

#if defined(STARTUP_INI_BITDESC_TABLES)
static ini_bitdesc_t bbs_options[] = {

	{ BBS_OPT_XTRN_MINIMIZED, "XTRN_MINIMIZED"   },
	{ BBS_OPT_AUTO_LOGON, "AUTO_LOGON"       },
	{ BBS_OPT_DEBUG_TELNET, "DEBUG_TELNET"     },
	{ BBS_OPT_ALLOW_RLOGIN, "ALLOW_RLOGIN"     },
	{ BBS_OPT_NO_QWK_EVENTS, "NO_QWK_EVENTS"    },
	{ BBS_OPT_NO_TELNET_GA, "NO_TELNET_GA"     },
	{ BBS_OPT_NO_EVENTS, "NO_EVENTS"        },
	{ BBS_OPT_NO_HOST_LOOKUP, "NO_HOST_LOOKUP"   },
	{ BBS_OPT_NO_SPY_SOCKETS, "NO_SPY_SOCKETS"   },
	{ BBS_OPT_ALLOW_SSH, "ALLOW_SSH"        },
	{ BBS_OPT_NO_DOS, "NO_DOS"           },
	{ BBS_OPT_NO_NEWDAY_EVENTS, "NO_NEWDAY_EVENTS" },
	{ BBS_OPT_NO_TELNET, "NO_TELNET"        },
	{ BBS_OPT_ALLOW_SFTP, "ALLOW_SFTP"       },
	{ BBS_OPT_SSH_ANYAUTH, "SSH_ANYAUTH"      },
	{ BBS_OPT_NO_RECYCLE, "NO_RECYCLE"       },
	{ BBS_OPT_GET_IDENT, "GET_IDENT"        },
	{ BBS_OPT_NO_JAVASCRIPT, "NO_JAVASCRIPT"    },
	{ BBS_OPT_HAPROXY_PROTO, "HAPROXY_PROTO"    },
	{ BBS_OPT_MUTE, "MUTE"             },
	/* terminator */
	{ 0, NULL               }
};

#ifndef STARTUP_INI_JSOPT_BITDESC_TABLE
#define STARTUP_INI_JSOPT_BITDESC_TABLE
#endif

#endif

#ifdef STARTUP_INI_JSOPT_BITDESC_TABLE
static ini_bitdesc_t js_options[] = {

	{ 1 << 0, "STRICT"               },
	{ 1 << 1, "WERROR"               },
	{ 1 << 2, "VAROBJFIX"            },
	{ 1 << 4, "COMPILE_N_GO"         },
	{ 1 << 9, "RELIMIT"              },
	{ 1 << 10, "ANONFUNFIX"           },
	{ 1 << 11, "JIT"                  },
	{ 1 << 14, "METHODJIT"            },
	{ 1 << 15, "PROFILING"            },
	{ 1 << 16, "METHODJIT_ALWAYS"     },
	/* terminator */
	{ 0, NULL                   }
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif

#ifdef _WIN32
	#ifdef SBBS_EXPORTS
		#define DLLEXPORT __declspec(dllexport)
	#else
		#define DLLEXPORT __declspec(dllimport)
	#endif
#else
	#define DLLEXPORT
#endif

/* arg is pointer to static bbs_startup_t* */
DLLEXPORT void          bbs_thread(void* arg);
DLLEXPORT void          bbs_terminate(void);
DLLEXPORT const char*   js_ver(void);
DLLEXPORT const char*   bbs_ver(void);
DLLEXPORT int           bbs_ver_num(void);

#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
