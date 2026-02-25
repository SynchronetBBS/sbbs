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

/*
 * General notes: (ToDo stuff)
 *
 * Support the ident protocol... the standard log format supports it.
 *
 * Add in support to pass connections through to a different webserver...
 *      probobly in webctrl.ini... with like a simplified mod_rewrite.
 *      This would allow people to run apache and Synchronet as the same site.
 *
 * Add support for multipart/form-data
 *
 * Improved Win32 support for POST data... currently will read past Content-Length
 *
 */

/* Headers for CGI stuff */
#if defined(__unix__)
	#include <sys/wait.h>       /* waitpid() */
	#include <sys/types.h>
	#include <signal.h>         /* kill() */
#elif defined(_WIN32) && defined(UDS_SUPPORT)
	#include <afunix.h>
#endif

#ifndef JAVASCRIPT
#define JAVASCRIPT
#endif

#undef SBBS /* this shouldn't be defined unless building sbbs.dll/libsbbs.so */
#include "sbbs.h"

#include "base64.h"
#include "fastcgi.h"
#include "filedat.h"
#include "git_branch.h"
#include "git_hash.h"
#include "hex.h"
#include "js_request.h"
#include "js_rtpool.h"
#include "js_socket.h"
#include "md5.h"
#include "multisock.h"
#include "os_info.h"
#include "sbbsdefs.h"
#include "semwrap.h"
#include "sockwrap.h"       /* sendfilesocket() */
#include "ssl.h"
#include "threadwrap.h"
#include "websrvr.h"
#include "xpendian.h"
#include "xpmap.h"
#include "xpprintf.h"
#include "ratelimit.hpp"
#include "filterfile.hpp"

static const char*  server_name = "Synchronet Web Server";
static const char*  server_abbrev = "web";
static const char*  newline = "\r\n";
static const char*  http_scheme = "http://";
static const size_t http_scheme_len = 7;
static const char*  https_scheme = "https://";
static const size_t https_scheme_len = 8;
static const char*  error_301 = "301 Moved Permanently";
static const char*  error_302 = "302 Moved Temporarily";
static const char*  error_307 = "307 Temporary Redirect";
static const char*  error_404 = "404 Not Found";
static const char*  error_416 = "416 Requested Range Not Satisfiable";
static const char*  error_429 = "429 Too Many Requests";
static const char*  error_500 = "500 Internal Server Error";
static const char*  error_503 = "503 Service Unavailable\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
static const char*  unknown = STR_UNKNOWN_USER;
static char*        text[TOTAL_TEXT];
static int          len_503 = 0;

#define TIMEOUT_THREAD_WAIT     60      /* Seconds */
#define MAX_REQUEST_LINE        1024    /* NOT including terminator */
#define MAX_HEADERS_SIZE        16384   /* Maximum total size of all headers (Including terminator) */
#define MAX_REDIR_LOOPS         20      /* Max. times to follow internal redirects for a single request */
#define MAX_POST_LEN            (4 * 1048576) /* Max size of body for POSTS */
#define OUTBUF_LEN              (256 * 1024)  /* Size of output thread ring buffer */

enum {
	CLEANUP_SSJS_TMP_FILE
	, CLEANUP_POST_DATA
	, MAX_CLEANUPS
};

static scfg_t             scfg;
static volatile bool      http_logging_thread_running = false;
static protected_uint32_t active_clients;
static protected_uint32_t thread_count;
static volatile uint32_t  client_highwater = 0;
static volatile uint32_t  con_conn_highwater = 0;
static bool               terminate_server = false;
static volatile bool      terminate_js = false;
static volatile bool      terminate_http_logging_thread = false;
static struct xpms_set *  ws_set = NULL;
static char               root_dir[MAX_PATH + 1];
static char               error_dir[MAX_PATH + 1];
static char               cgi_dir[MAX_PATH + 1];
static char               cgi_env_ini[MAX_PATH + 1];
static char               default_auth_list[MAX_PATH + 1];
static volatile time_t    uptime = 0;
static volatile ulong     served = 0;
static web_startup_t*     startup = NULL;
static js_server_props_t  js_server_props;
static str_list_t         recycle_semfiles;
static str_list_t         shutdown_semfiles;
static str_list_t         pause_semfiles;
static str_list_t         cgi_env;
static struct mqtt        mqtt;
static link_list_t        current_connections;

static named_string_t**   mime_types;
static named_string_t**   cgi_handlers;
static named_string_t**   xjs_handlers;
static named_string_t**   alias_list; // request path aliases

static rateLimiter*       request_rate_limiter = nullptr;
static trashCan*          ip_can = nullptr;
static trashCan*          ip_silent_can = nullptr;
static trashCan*          host_can = nullptr;
static filterFile*        host_exempt = nullptr;

/* Logging stuff */
link_list_t               log_list;
struct log_data {
	char *hostname;
	char *ident;
	char *user;
	char *request;
	char *referrer;
	char *agent;
	char *vhost;
	int status;
	off_t size;
	struct tm completed;
	client_t client;
	uint16_t server_port;
};

enum auth_type {
	AUTHENTICATION_UNKNOWN
	, AUTHENTICATION_BASIC
	, AUTHENTICATION_DIGEST
	, AUTHENTICATION_TLS_PSK
};

const char *auth_type_names[] = {
	"Unknown"
	, "Basic"
	, "Digest"
	, "TLS-PSK"
	, NULL
};

enum algorithm {
	ALGORITHM_UNKNOWN
	, ALGORITHM_MD5
	, ALGORITHM_MD5_SESS
};

enum qop_option {
	QOP_NONE
	, QOP_AUTH
	, QOP_AUTH_INT
	, QOP_UNKNOWN
};

struct authentication_request_t {
	enum auth_type type;
	char username[(LEN_ALIAS > LEN_NAME ? LEN_ALIAS : LEN_NAME) + 1];
	char password[LEN_PASS + 1];
	char *digest_uri;
	char *realm;
	char *nonce;
	enum algorithm algorithm;
	enum qop_option qop_value;
	char *cnonce;
	char *nonce_count;
	unsigned char digest[16];       /* MD5 digest */
	bool stale;
};

struct http_request_t {
	int method;
	char virtual_path[MAX_PATH + 1];
	char physical_path[MAX_PATH + 1];
	bool expect_go_ahead;
	time_t if_modified_since;
	bool keep_alive;
	char ars[256];
	authentication_request_t auth;
	char host[128];                     /* The requested host. (as used for self-referencing URLs) */
	char vhost[128];                    /* The requested host. (virtual host) */
	int send_location;
	bool send_content;
	bool upgrading;
	char* location_to_send;
	char* vary_list;
	const char* mime_type;
	str_list_t headers;
	char status[MAX_REQUEST_LINE + 1];
	char * post_data;
	struct xpmapping *post_map;
	size_t post_len;
	int dynamic;
	char xjs_handler[MAX_PATH + 1];
	struct log_data *ld;
	char request_line[MAX_REQUEST_LINE + 1];
	char orig_request_line[MAX_REQUEST_LINE + 1];
	bool finished;                      /* Done processing request. */
	bool read_chunked;
	bool write_chunked;
	off_t range_start;
	off_t range_end;
	bool got_range;
	bool accept_ranges;
	time_t if_range;
	bool path_info_index;

	/* CGI parameters */
	char query_str[MAX_REQUEST_LINE + 1];
	char extra_path_info[MAX_REQUEST_LINE + 1];
	str_list_t cgi_env;
	str_list_t dynamic_heads;
	bool got_extra_path;

	/* Dynamically (sever-side JS) generated HTML parameters */
	FILE* fp;
	char *cleanup_file[MAX_CLEANUPS];
	bool sent_headers;
	bool prev_write;
	bool manual_length;

	/* webctrl.ini overrides */
	char *error_dir;
	char *cgi_dir;
	char *auth_list;
	char *realm;
	char *digest_realm;
	char *fastcgi_socket;
};

struct http_session_t {
	SOCKET socket;
	union xp_sockaddr addr;
	socklen_t addr_len;
	http_request_t req;
	uint requests;
	char host_ip[INET6_ADDRSTRLEN];
	char host_name[128];            /* Resolved remote host */
	int http_ver;                   /* Request HTTP version.  0 = HTTP/0.9, 1=HTTP/1.0, 2=HTTP/1.1 */
	volatile bool send_failed;
	bool finished;                  /* Do not accept any more input from client */
	enum parsed_vpath parsed_vpath; /* file area/base access */
	int libnum;
	file_t file;
	user_t user;
	int last_user_num;
	time_t logon_time;
	char username[LEN_NAME + 1];
	int last_js_user_num;
	char redir_req[MAX_REQUEST_LINE + 1];

	/* JavaScript parameters */
	JSRuntime* js_runtime;
	JSContext* js_cx;
	JSObject* js_glob;
	JSObject* js_query;
	JSObject* js_header;
	JSObject* js_cookie;
	JSObject* js_request;
	js_callback_t js_callback;
	subscan_t *subscan;

	/* Ring Buffer Stuff */
	RingBuf outbuf;
	sem_t output_thread_terminated;
	int outbuf_write_initialized;
	pthread_mutex_t outbuf_write;

	/* Client info */
	client_t client;

	/* Synchronization stuff */
	pthread_mutex_t struct_filled;

	/* TLS Stuff */
	bool is_tls;
	CRYPT_SESSION tls_sess;
	bool tls_pending;
	bool peeked_valid;
	char peeked;
};

enum {
	HTTP_0_9
	, HTTP_1_0
	, HTTP_1_1
};
static const char* http_vers[] = {
	""
	, "HTTP/1.0"
	, "HTTP/1.1"
	, NULL   /* terminator */
};
static const char* response_http_vers[] = {
	""
	, "HTTP/1.1"
	, "HTTP/1.1"
	, NULL   /* terminator */
};

enum {
	HTTP_HEAD
	, HTTP_GET
	, HTTP_POST
	, HTTP_OPTIONS
};

static const char* methods[] = {
	"HEAD"
	, "GET"
	, "POST"
	, "OPTIONS"
	, NULL   /* terminator */
};

enum {
	IS_STATIC
	, IS_CGI
	, IS_SSJS
	, IS_FASTCGI
};

enum {
	HEAD_DATE
	, HEAD_HOST
	, HEAD_IFMODIFIED
	, HEAD_LENGTH
	, HEAD_TYPE
	, HEAD_AUTH
	, HEAD_CONNECTION
	, HEAD_WWWAUTH
	, HEAD_STATUS
	, HEAD_ALLOW
	, HEAD_EXPIRES
	, HEAD_LASTMODIFIED
	, HEAD_LOCATION
	, HEAD_PRAGMA
	, HEAD_SERVER
	, HEAD_REFERER
	, HEAD_AGENT
	, HEAD_TRANSFER_ENCODING
	, HEAD_ACCEPT_RANGES
	, HEAD_CONTENT_RANGE
	, HEAD_RANGE
	, HEAD_IFRANGE
	, HEAD_COOKIE
	, HEAD_STS
	, HEAD_UPGRADEINSECURE
	, HEAD_VARY
	, HEAD_CSP
};

static struct {
	int id;
	const char* text;
} headers[] = {
	{ HEAD_DATE,            "Date"                  },
	{ HEAD_HOST,            "Host"                  },
	{ HEAD_IFMODIFIED,      "If-Modified-Since"     },
	{ HEAD_LENGTH,          "Content-Length"        },
	{ HEAD_TYPE,            "Content-Type"          },
	{ HEAD_AUTH,            "Authorization"         },
	{ HEAD_CONNECTION,      "Connection"            },
	{ HEAD_WWWAUTH,         "WWW-Authenticate"      },
	{ HEAD_STATUS,          "Status"                },
	{ HEAD_ALLOW,           "Allow"                 },
	{ HEAD_EXPIRES,         "Expires"               },
	{ HEAD_LASTMODIFIED,    "Last-Modified"         },
	{ HEAD_LOCATION,        "Location"              },
	{ HEAD_PRAGMA,          "Pragma"                },
	{ HEAD_SERVER,          "Server"                },
	{ HEAD_REFERER,         "Referer"               },
	{ HEAD_AGENT,           "User-Agent"            },
	{ HEAD_TRANSFER_ENCODING,           "Transfer-Encoding"         },
	{ HEAD_ACCEPT_RANGES,   "Accept-Ranges"         },
	{ HEAD_CONTENT_RANGE,   "Content-Range"         },
	{ HEAD_RANGE,           "Range"                 },
	{ HEAD_IFRANGE,         "If-Range"              },
	{ HEAD_COOKIE,          "Cookie"                },
	{ HEAD_STS,         "Strict-Transport-Security"     },
	{ HEAD_UPGRADEINSECURE,     "Upgrade-Insecure-Requests"     },
	{ HEAD_VARY,            "Vary"                  },
	{ HEAD_CSP,         "Content-Security-Policy"       },
	{ -1,                   NULL /* terminator */   },
};

/* Everything MOVED_TEMP and everything after is a magical internal redirect */
enum  {
	NO_LOCATION
	, MOVED_PERM
	, MOVED_TEMPREDIR
	, MOVED_TEMP
	, MOVED_STAT
};

#define GCES(status, sess, action) do {                                             \
			char *GCES_estr;                                                                \
			int   GCES_level;                                                                 \
			get_crypt_error_string(status, sess->tls_sess, &GCES_estr, action, &GCES_level); \
			if (GCES_estr) {                                                                \
				if (GCES_level < startup->tls_error_level)                                  \
				GCES_level = startup->tls_error_level;                                  \
				if (GCES_level > LOG_INFO)                                                  \
				GCES_level = LOG_INFO;                                                  \
				lprintf(GCES_level, "%04d TLS %s", sess->socket, GCES_estr);                \
				free_crypt_attrstr(GCES_estr);                                              \
			}                                                                               \
} while (0)

static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static void respond(http_session_t * session);
static bool js_setup_cx(http_session_t* session);
static bool js_setup(http_session_t* session);
static char *find_last_slash(char *str);
static bool check_extra_path(http_session_t * session);
static bool exec_ssjs(http_session_t* session, char* script);
static bool ssjs_send_headers(http_session_t* session, int chunked);
static int sess_recv(http_session_t *session, char *buf, size_t length, int flags);

static time_t
sub_mkgmt(struct tm *tm)
{
	int                         y, nleapdays;
	time_t                      t;
	/* days before the month */
	static const unsigned short moff[12] = {
		0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
	};

	/*
	     * XXX: This code assumes the given time to be normalized.
	     * Normalizing here is impossible in case the given time is a leap
	     * second but the local time library is ignorant of leap seconds.
	     */

	/* minimal sanity checking not to access outside of the array */
	if ((unsigned) tm->tm_mon >= 12)
		return (time_t) -1;
	if (tm->tm_year < 1970 - 1900)
		return (time_t) -1;

	y = tm->tm_year + 1900 - (tm->tm_mon < 2);
	nleapdays = y / 4 - y / 100 + y / 400 -
	            ((1970 - 1) / 4 - (1970 - 1) / 100 + (1970 - 1) / 400);
	t = ((((time_t) (tm->tm_year - (1970 - 1900)) * 365 +
	       moff[tm->tm_mon] + tm->tm_mday - 1 + nleapdays) * 24 +
	      tm->tm_hour) * 60 + tm->tm_min) * 60 + tm->tm_sec;

	return t < 0 ? (time_t) -1 : t;
}

time_t
time_gm(struct tm *tm)
{
	time_t     t, t2;
	struct tm *tm2;
	int        sec;

	/* Do the first guess. */
	if ((t = sub_mkgmt(tm)) == (time_t) -1)
		return (time_t) -1;

	/* save value in case *tm is overwritten by gmtime() */
	sec = tm->tm_sec;

	tm2 = gmtime(&t);       /* why not use gmtime_r instead? */
	if (tm2 == NULL || (t2 = sub_mkgmt(tm2)) == (time_t) -1)
		return (time_t) -1;

	if (t2 < t || tm2->tm_sec != sec) {
		/*
		         * Adjust for leap seconds.
		         *
		         *     real time_t time
		         *           |
		         *          tm
		         *         /        ... (a) first sub_mkgmt() conversion
		         *       t
		         *       |
		         *      tm2
		         *     /        ... (b) second sub_mkgmt() conversion
		         *   t2
		         *                        --->time
		         */
		/*
		         * Do the second guess, assuming (a) and (b) are almost equal.
		         */
		t += t - t2;
		tm2 = gmtime(&t);

		/*
		         * Either (a) or (b), may include one or two extra
		         * leap seconds.  Try t, t + 2, t - 2, t + 1, and t - 1.
		         */
		if (tm2->tm_sec == sec
		    || (t += 2, tm2 = gmtime(&t), tm2->tm_sec == sec)
		    || (t -= 4, tm2 = gmtime(&t), tm2->tm_sec == sec)
		    || (t += 3, tm2 = gmtime(&t), tm2->tm_sec == sec)
		    || (t -= 2, tm2 = gmtime(&t), tm2->tm_sec == sec))
			;                    /* found */
		else {
			/*
			             * Not found.
			             */
			if (sec >= 60)
				/*
				                 * The given time is a leap second
				                 * (sec 60 or 61), but the time library
				                 * is ignorant of the leap second.
				                 */
				;                        /* treat sec 60 as 59,
			                               sec 61 as 0 of the next minute */
			else
				/* The given time may not be normalized. */
				t++;                        /* restore t */
		}
	}

	return t < 0 ? (time_t) -1 : t;
}

static int lputs(int level, const char* str)
{
	mqtt_lputs(&mqtt, TOPIC_SERVER, level, str);
	if (level <= LOG_ERR) {
		char errmsg[1024];
		SAFEPRINTF2(errmsg, "%-4s %s", server_abbrev, str);
		errorlog(&scfg, &mqtt, level, startup == NULL ? NULL : startup->host_name, errmsg);
		if (startup != NULL && startup->errormsg != NULL)
			startup->errormsg(startup->cbdata, level, errmsg);
	}

	if (startup == NULL || startup->lputs == NULL || str == NULL || level > startup->log_level)
		return 0;

#if defined(_WIN32)
	if (IsBadCodePtr((FARPROC)startup->lputs))
		return 0;
#endif

	return startup->lputs(startup->cbdata, level, str);
}

#if defined(__GNUC__)   // Catch printf-format errors with lprintf
static int lprintf(int level, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
#endif
static int lprintf(int level, const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);

	return lputs(level, sbuf);
}

#if defined(__GNUC__)   // Catch printf-format errors with errprintf
static int errprintf(int level, int line, const char* function, const char* file, const char *fmt, ...) __attribute__ ((format (printf, 5, 6)));
#endif
static int errprintf(int level, int line, const char* function, const char* file, const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);
	if (repeated_error(line, function)) {
		if (level < LOG_WARNING)
			level = LOG_WARNING;
	}
	return lputs(level, sbuf);
}

static int writebuf(http_session_t  *session, const char *buf, size_t len)
{
	size_t sent = 0;
	size_t avail;

	if (session->req.sent_headers && session->req.send_content == false) {
		lprintf(LOG_INFO, "%04d %-5s [%s] Not sending data because session->req.send_content == false", session->socket, session->client.protocol, session->host_ip);
		return 0;
	}
	while (sent < len) {
		ResetEvent(session->outbuf.empty_event);
		avail = RingBufFree(&session->outbuf);
		if (!avail) {
			(void)WaitForEvent(session->outbuf.empty_event, 1);
			continue;
		}
		if (avail > len - sent)
			avail = len - sent;
		sent += RingBufWrite(&(session->outbuf), ((const BYTE *)buf) + sent, avail);
	}
	return sent;
}

#define HANDLE_CRYPT_CALL(status, session, action)  handle_crypt_call_except2(status, session, action, CRYPT_OK, CRYPT_OK, __FILE__, __LINE__)
#define HANDLE_CRYPT_CALL_EXCEPT(status, session, action, ignore)  handle_crypt_call_except2(status, session, action, ignore, ignore, __FILE__, __LINE__)
#define HANDLE_CRYPT_CALL_EXCEPT2(status, session, action, ignore, ignore2)  handle_crypt_call_except2(status, session, action, ignore, ignore2, __FILE__, __LINE__)

static bool handle_crypt_call_except2(int status, http_session_t *session, const char *action, int ignore, int ignore2, const char *file, int line)
{
	if (status == CRYPT_OK)
		return true;
	if (status == ignore)
		return false;
	if (status == ignore2)
		return false;
	GCES(status, session, action);
	return false;
}

static bool session_check(http_session_t *session, bool *rd, bool *wr, unsigned timeout)
{
	bool  ret = false;
	bool  lcl_rd = 0;
	bool *rd_ptr = rd?rd:&lcl_rd;

	if (session->is_tls) {
		if (wr)
			*wr = 1;
		if (rd || wr == NULL) {
			if (session->tls_pending) {
				*rd_ptr = true;
				return true;
			}
		}
		ret = socket_check(session->socket, rd_ptr, wr, timeout);
		if (ret && *rd_ptr) {
			session->tls_pending = true;
			return true;
		}
		return ret;
	}
	return socket_check(session->socket, rd, wr, timeout);
}

static int sess_sendbuf(http_session_t *session, const char *buf, size_t len, volatile bool *failed)
{
	size_t sent = 0;
	int    tls_sent;
	int    result;
	int    status;
	bool   local_failed = false;

	if (failed == NULL)
		failed = &local_failed;

	while (sent < len && session->socket != INVALID_SOCKET && *failed == false) {
		if (socket_writable(session->socket, startup->max_inactivity * 1000)) {
			if (session->is_tls) {
				/*
				 * Limit as per js_socket.c.
				 * Sure, this is TLS, not SSH, but we see weird stuff here in sz file transfers.
				 */
				size_t sendbytes = len - sent;
				if (sendbytes > 0x2000)
					sendbytes = 0x2000;
				status = cryptPushData(session->tls_sess, buf + sent, sendbytes, &tls_sent);
				GCES(status, session, "pushing data");
				if (status == CRYPT_ERROR_TIMEOUT) {
					tls_sent = 0;
					char tmp; // hack to avoid warning about about unexpected NULL
					if (!cryptStatusOK(status = cryptPopData(session->tls_sess, &tmp, 0, &status))) {
						if (status != CRYPT_ERROR_TIMEOUT && status != CRYPT_ERROR_PARAM2)
							GCES(status, session, "popping data after timeout");
					}
					status = CRYPT_OK;
				}
				if (cryptStatusOK(status)) {
					HANDLE_CRYPT_CALL_EXCEPT(status = cryptFlushData(session->tls_sess), session, "flushing data", CRYPT_ERROR_COMPLETE);
					if (cryptStatusError(status))
						*failed = true;
				}
				else
					*failed = true;
				result = tls_sent;
			}
			else {
				result = sendsocket(session->socket, buf + sent, len - sent);
				if (result == SOCKET_ERROR) {
					if (SOCKET_ERRNO == ECONNRESET)
						lprintf(LOG_NOTICE, "%04d %-5s [%s] Connection reset by peer on send", session->socket, session->client.protocol, session->host_ip);
					else if (SOCKET_ERRNO == ECONNABORTED)
						lprintf(LOG_NOTICE, "%04d %-5s [%s] Connection aborted by peer on send", session->socket, session->client.protocol, session->host_ip);
#ifdef EPIPE
					else if (SOCKET_ERRNO == EPIPE)
						lprintf(LOG_NOTICE, "%04d %-5s [%s] Unable to send to peer", session->socket, session->client.protocol, session->host_ip);
#endif
					else if (session->socket != INVALID_SOCKET)
						lprintf(LOG_WARNING, "%04d %-5s [%s] !ERROR %d sending on socket", session->socket, session->client.protocol, session->host_ip, SOCKET_ERRNO);
					*failed = true;
					return sent;
				}
			}
		}
		else {
			lprintf(LOG_WARNING, "%04d %-5s [%s] Timeout waiting for socket to become writable", session->socket, session->client.protocol, session->host_ip);
			*failed = true;
			return sent;
		}
		sent += result;
	}
	if (sent < len)
		*failed = true;
	if (session->is_tls)
		HANDLE_CRYPT_CALL(cryptFlushData(session->tls_sess), session, "flushing data");
	return sent;
}

#ifdef _WINSOCKAPI_

static WSADATA WSAData;
#define SOCKLIB_DESC WSAData.szDescription
static bool    WSAInitialized = false;

static bool winsock_startup(void)
{
	int status;                 /* Status Code */

	if ((status = WSAStartup(MAKEWORD(1, 1), &WSAData)) == 0) {
		lprintf(LOG_DEBUG, "%s %s", WSAData.szDescription, WSAData.szSystemStatus);
		WSAInitialized = true;
		return true;
	}

	errprintf(LOG_CRIT, WHERE, "!WinSock startup ERROR %d", status);
	return false;
}

#else /* No WINSOCK */

#define winsock_startup()   (true)
#define SOCKLIB_DESC NULL

#endif

static char* server_host_name(void)
{
	return startup->host_name[0] ? startup->host_name : scfg.sys_inetaddr;
}

static void set_state(enum server_state state)
{
	static int curr_state;

	if (state == curr_state)
		return;

	if (startup != NULL) {
		if (startup->set_state != NULL)
			startup->set_state(startup->cbdata, state);
		mqtt_server_state(&mqtt, state);
	}
	curr_state = state;
}

static void update_clients(void)
{
	if (startup != NULL) {
		uint32_t count = protected_uint32_value(active_clients);
		if (startup->clients != NULL)
			startup->clients(startup->cbdata, count);
	}
}

static void client_on(SOCKET sock, client_t* client, bool update)
{
	if (!update)
		listAddNodeData(&current_connections, client->addr, strlen(client->addr) + 1, sock, LAST_NODE);
	if (startup != NULL && startup->client_on != NULL)
		startup->client_on(startup->cbdata, true, sock, client, update);
	mqtt_client_on(&mqtt, true, sock, client, update);
}

static void client_off(SOCKET sock)
{
	if (startup != NULL && startup->client_on != NULL)
		startup->client_on(startup->cbdata, false, sock, NULL, false);
	mqtt_client_on(&mqtt, false, sock, NULL, false);
}

static void thread_up(bool setuid)
{
	if (startup != NULL && startup->thread_up != NULL)
		startup->thread_up(startup->cbdata, true, setuid);
}

static uint32_t thread_down(void)
{
	uint32_t count = protected_uint32_adjust_fetch(&thread_count, -1);
	if (startup != NULL && startup->thread_up != NULL)
		startup->thread_up(startup->cbdata, false, false);
	return count;
}

/*********************************************************************/
/* Adds an environment variable to the sessions  cgi_env linked list */
/*********************************************************************/
static void add_env(http_session_t *session, const char *name, const char *value)  {
	char  newname[129];
	char *p;

	if (name == NULL || value == NULL)  {
		lprintf(LOG_WARNING, "%04d Attempt to set NULL env variable", session->socket);
		return;
	}
	SAFECOPY(newname, name);

	for (p = newname; *p; p++)  {
		*p = toupper(*p);
		if (*p == '-')
			*p = '_';
	}
	p = xp_asprintf("%s=%s", newname, value);
	if (p == NULL) {
		lprintf(LOG_WARNING, "%04d Cannot allocate memory for string", session->socket);
		return;
	}
	strListPush(&session->req.cgi_env, p);
	free(p);
}

/***************************************/
/* Initializes default CGI envirnoment */
/***************************************/
static void init_enviro(http_session_t *session)  {
	char              str[128];
	union xp_sockaddr sockaddr;
	socklen_t         socklen = sizeof(sockaddr);

	add_env(session, "SERVER_SOFTWARE", VERSION_NOTICE);
	if (getsockname(session->socket, &sockaddr.addr, &socklen) == 0) {
		sprintf(str, "%d", inet_addrport(&sockaddr));
		add_env(session, "SERVER_PORT", str);
	}
	add_env(session, "GATEWAY_INTERFACE", "CGI/1.1");
	if (!strcmp(session->host_name, session->host_ip))
		add_env(session, "REMOTE_HOST", session->host_name);
	add_env(session, "REMOTE_ADDR", session->host_ip);
	add_env(session, "REQUEST_URI", session->req.orig_request_line);
}

/*
 * Sends string str to socket sock... returns number of bytes written, or 0 on an error
 * Can not close the socket since it can not set it to INVALID_SOCKET
 */
static int bufprint(http_session_t *session, const char *str)
{
	int len;

	len = strlen(str);
	return writebuf(session, str, len);
}

/**********************************************************/
/* Converts a month name/abbr to the 0-based month number */
/* ToDo: This probobly exists somewhere else already	  */
/**********************************************************/
static int getmonth(char *mon)
{
	int i;
	for (i = 0; i < 12; i++)
		if (!stricmp(mon, months[i]))
			return i;

	return 0;
}

/*******************************************************************/
/* Converts a date string in any of the common formats to a time_t */
/*******************************************************************/
static time_t decode_date(char *date)
{
	struct  tm ti;
	char *     token;
	char *     last;
	time_t     t;

	ti.tm_sec = 0;        /* seconds (0 - 60) */
	ti.tm_min = 0;        /* minutes (0 - 59) */
	ti.tm_hour = 0;       /* hours (0 - 23) */
	ti.tm_mday = 1;       /* day of month (1 - 31) */
	ti.tm_mon = 0;        /* month of year (0 - 11) */
	ti.tm_year = 0;       /* year - 1900 */
	ti.tm_isdst = 0;      /* is summer time in effect? */

	token = strtok_r(date, ",", &last);
	if (token == NULL)
		return 0;
	/* This probobly only needs to be 9, but the extra one is for luck. */
	if (strlen(date) > 15) {
		/* asctime() */
		/* Toss away week day */
		token = strtok_r(date, " ", &last);
		if (token == NULL)
			return 0;
		token = strtok_r(NULL, " ", &last);
		if (token == NULL)
			return 0;
		ti.tm_mon = getmonth(token);
		token = strtok_r(NULL, " ", &last);
		if (token == NULL)
			return 0;
		ti.tm_mday = atoi(token);
		token = strtok_r(NULL, ":", &last);
		if (token == NULL)
			return 0;
		ti.tm_hour = atoi(token);
		token = strtok_r(NULL, ":", &last);
		if (token == NULL)
			return 0;
		ti.tm_min = atoi(token);
		token = strtok_r(NULL, " ", &last);
		if (token == NULL)
			return 0;
		ti.tm_sec = atoi(token);
		token = strtok_r(NULL, "", &last);
		if (token == NULL)
			return 0;
		ti.tm_year = atoi(token) - 1900;
	}
	else  {
		/* RFC 1123 or RFC 850 */
		token = strtok_r(NULL, " -", &last);
		if (token == NULL)
			return 0;
		ti.tm_mday = atoi(token);
		token = strtok_r(NULL, " -", &last);
		if (token == NULL)
			return 0;
		ti.tm_mon = getmonth(token);
		token = strtok_r(NULL, " ", &last);
		if (token == NULL)
			return 0;
		ti.tm_year = atoi(token);
		token = strtok_r(NULL, ":", &last);
		if (token == NULL)
			return 0;
		ti.tm_hour = atoi(token);
		token = strtok_r(NULL, ":", &last);
		if (token == NULL)
			return 0;
		ti.tm_min = atoi(token);
		token = strtok_r(NULL, " ", &last);
		if (token == NULL)
			return 0;
		ti.tm_sec = atoi(token);
		if (ti.tm_year > 1900)
			ti.tm_year -= 1900;
	}

	t = time_gm(&ti);
	return t;
}

static void open_socket(SOCKET sock, void *cbdata)
{
	char                     error[256];
#ifdef SO_ACCEPTFILTER
	struct accept_filter_arg afa;
#endif

	if (startup != NULL && startup->socket_open != NULL)
		startup->socket_open(startup->cbdata, true);
	if (cbdata != NULL && !strcmp(static_cast<const char *>(cbdata), "TLS")) {
		if (set_socket_options(&scfg, sock, "web|http|tls", error, sizeof(error)))
			lprintf(LOG_ERR, "%04d !ERROR %s", sock, error);
	}
	else {
		if (set_socket_options(&scfg, sock, "web|http", error, sizeof(error)))
			lprintf(LOG_ERR, "%04d !ERROR %s", sock, error);
	}
#ifdef SO_ACCEPTFILTER
	memset(&afa, 0, sizeof(afa));
	strcpy(afa.af_name, "httpready");
	setsockopt(sock, SOL_SOCKET, SO_ACCEPTFILTER, &afa, sizeof(afa));
#endif
}

static void close_socket_cb(SOCKET sock, void *cbdata)
{
	if (startup != NULL && startup->socket_open != NULL)
		startup->socket_open(startup->cbdata, false);
}

static int close_socket(SOCKET *sock)
{
	int    result;
	char   ch;
	time_t end = time(NULL) + startup->max_inactivity;

	if (sock == NULL || *sock == INVALID_SOCKET)
		return -1;

	/* required to ensure all data is sent */
	shutdown(*sock, SHUT_WR);
	while (socket_readable(*sock, startup->max_inactivity * 1000))  {
		if (recv(*sock, &ch, 1, 0) <= 0)
			break;
		if (time(NULL) >= end)
			break;
	}
	result = closesocket(*sock);
	*sock = INVALID_SOCKET;
	if (startup != NULL && startup->socket_open != NULL) {
		startup->socket_open(startup->cbdata, false);
	}
	if (result != 0) {
		if (SOCKET_ERRNO != ENOTSOCK)
			lprintf(LOG_WARNING, "%04d !ERROR %d closing socket", *sock, SOCKET_ERRNO);
	}

	return result;
}

static int close_session_socket(http_session_t *session)
{
	char buf[1];
	int  len;
	int locked;

	if (session == NULL || session->socket == INVALID_SOCKET)
		return -1;

	if (session->is_tls) {
		// First, wait for the ringbuffer to drain...
		len = 1;
		while (RingBufFull(&session->outbuf) && session->socket != INVALID_SOCKET) {
			if (len) {
				if (cryptPopData(session->tls_sess, buf, 1, &len) != CRYPT_OK)
					len = 0;
			}
			SLEEP(1);
		}
		// Now wait for tranmission to complete
		len = 1;
		while ((locked = pthread_mutex_trylock(&session->outbuf_write)) == EBUSY) {
			if (len) {
				if (cryptPopData(session->tls_sess, buf, 1, &len) != CRYPT_OK)
					len = 0;
			}
			SLEEP(1);
		}
		if (locked == 0)
			pthread_mutex_unlock(&session->outbuf_write);
		HANDLE_CRYPT_CALL(destroy_session(lprintf, session->tls_sess), session, "destroying session");
	}
	return close_socket(&session->socket);
}

/* Waits for the outbuf to drain */
static void drain_outbuf(http_session_t * session)
{
	if (session->socket == INVALID_SOCKET)
		return;
	/* Force the output thread to go NOW */
	SetEvent(session->outbuf.highwater_event);
	/* ToDo: This should probably timeout eventually... */
	while (RingBufFull(&session->outbuf) && session->socket != INVALID_SOCKET) {
		SetEvent(session->outbuf.highwater_event);
		SLEEP(1);
	}
	/* Lock the mutex to ensure data has been sent */
	while (session->socket != INVALID_SOCKET && !session->outbuf_write_initialized)
		SLEEP(1);
	if (session->socket == INVALID_SOCKET)
		return;
	pthread_mutex_lock(&session->outbuf_write);     /* Win32 Access violation here on Jan-11-2006 - shutting down webserver while in use */
	pthread_mutex_unlock(&session->outbuf_write);
}

/**************************************************/
/* End of a single request...					  */
/* This is called at the end of EVERY request	  */
/*  Log the request       						  */
/*  Free request-specific data ie: dynamic stuff  */
/*  Close socket unless it's being kept alive     */
/*   If the socket is closed, the session is done */
/**************************************************/
static void close_request(http_session_t * session)
{
	time_t now;
	int    i;

	if (session->req.write_chunked) {
		drain_outbuf(session);
		session->req.write_chunked = 0;
		writebuf(session, "0\r\n", 3);
		if (session->req.dynamic == IS_SSJS)
			ssjs_send_headers(session, false);
		else
			/* Non-ssjs isn't capable of generating headers during execution */
			writebuf(session, newline, 2);
	}

	/* Force the output thread to go NOW */
	SetEvent(session->outbuf.highwater_event);

	if (session->req.ld != NULL) {
		union xp_sockaddr sockaddr;
		socklen_t         socklen = sizeof(sockaddr);
		if (getsockname(session->socket, &sockaddr.addr, &socklen) == 0)
			session->req.ld->server_port = inet_addrport(&sockaddr);
		now = time(NULL);
		localtime_r(&now, &session->req.ld->completed);
		session->req.ld->client = session->client;
		listPushNode(&log_list, session->req.ld);
		session->req.ld = NULL;
	}

	strListFree(&session->req.headers);
	strListFree(&session->req.dynamic_heads);
	strListFree(&session->req.cgi_env);
	if (session->req.post_map != NULL) {
		xpunmap(session->req.post_map);
		session->req.post_data = NULL;
		session->req.post_map = NULL;
	}
	FREE_AND_NULL(session->req.post_data);
	FREE_AND_NULL(session->req.error_dir);
	FREE_AND_NULL(session->req.cgi_dir);
	FREE_AND_NULL(session->req.auth_list);
	FREE_AND_NULL(session->req.realm);
	FREE_AND_NULL(session->req.digest_realm);
	FREE_AND_NULL(session->req.fastcgi_socket);
	FREE_AND_NULL(session->req.location_to_send);
	FREE_AND_NULL(session->req.vary_list);

	FREE_AND_NULL(session->req.auth_list);
	FREE_AND_NULL(session->req.auth.digest_uri);
	FREE_AND_NULL(session->req.auth.cnonce);
	FREE_AND_NULL(session->req.auth.realm);
	FREE_AND_NULL(session->req.auth.nonce);
	FREE_AND_NULL(session->req.auth.nonce_count);

	/*
	 * This causes all active http_session_threads to terminate.
	 */
	if ((!session->req.keep_alive) || terminate_server) {
		drain_outbuf(session);
		close_session_socket(session);
	}
	if (session->socket == INVALID_SOCKET)
		session->finished = true;

	if (session->js_cx != NULL && (session->req.dynamic == IS_SSJS)) {
		JS_BEGINREQUEST(session->js_cx);
		JS_GC(session->js_cx);
		JS_ENDREQUEST(session->js_cx);
	}
	if (session->subscan != NULL)
		putmsgptrs(&scfg, &session->user, session->subscan);

	FCLOSE_OPEN_FILE(session->req.fp);

	for (i = 0; i < MAX_CLEANUPS; i++) {
		if (session->req.cleanup_file[i] != NULL) {
			if (i != CLEANUP_SSJS_TMP_FILE
			    || !(startup->options & WEB_OPT_DEBUG_SSJS))
				remove(session->req.cleanup_file[i]);
			free(session->req.cleanup_file[i]);
		}
	}

	smb_freefilemem(&session->file);
	memset(&session->req, 0, sizeof(session->req));
}

// Opens the response content (SSJS output) file if necessary
static bool content_file_open(http_session_t* session)
{
	const char* path = session->req.cleanup_file[CLEANUP_SSJS_TMP_FILE];
	if (session->req.fp == NULL) {
		if (path == NULL)
			lprintf(LOG_WARNING, "%04d %-5s [%s] Response file path is NULL"
			        , session->socket, session->client.protocol, session->host_ip);
		else {
			session->req.fp = fopen(path, "wb");
			if (session->req.fp == NULL) {
				lprintf(LOG_ERR, "%04d %-5s [%s] Error %d (%s) opening/creating %s"
				        , session->socket, session->client.protocol, session->host_ip, errno, strerror(errno), path);
			}
		}
	}
	return session->req.fp != NULL;
}

static int get_header_type(char *header)
{
	int i;
	for (i = 0; headers[i].text != NULL; i++) {
		if (!stricmp(header, headers[i].text)) {
			return headers[i].id;
		}
	}
	return -1;
}

/* Opposite of get_header_type() */
static const char *get_header(int id)
{
	int i;
	if (headers[id].id == id)
		return headers[id].text;

	for (i = 0; headers[i].text != NULL; i++) {
		if (headers[i].id == id) {
			return headers[i].text;
		}
	}
	return NULL;
}

static const char* unknown_mime_type = "application/octet-stream";

static const char* get_mime_type(char *ext)
{
	uint i;

	if (ext == NULL || mime_types == NULL)
		return unknown_mime_type;

	for (i = 0; mime_types[i] != NULL; i++)
		if (stricmp(ext + 1, mime_types[i]->name) == 0)
			return mime_types[i]->value;

	return unknown_mime_type;
}

static char* get_cgi_handler(const char* fname)
{
	char*  ext;
	size_t i;

	if (cgi_handlers == NULL || (ext = getfext(fname)) == NULL)
		return NULL;
	for (i = 0; cgi_handlers[i] != NULL; i++) {
		if (stricmp(cgi_handlers[i]->name, ext + 1) == 0)
			return cgi_handlers[i]->value;
	}
	return NULL;
}

static bool get_xjs_handler(char* ext, http_session_t* session)
{
	size_t i;

	if (ext == NULL || xjs_handlers == NULL || ext[0] == 0)
		return false;

	for (i = 0; xjs_handlers[i] != NULL; i++) {
		if (stricmp(xjs_handlers[i]->name, ext + 1) == 0) {
			if (getfname(xjs_handlers[i]->value) == xjs_handlers[i]->value)    /* no path specified */
				SAFEPRINTF2(session->req.xjs_handler, "%s%s", scfg.exec_dir, xjs_handlers[i]->value);
			else
				SAFECOPY(session->req.xjs_handler, xjs_handlers[i]->value);
			return true;
		}
	}
	return false;
}

/* This function appends append plus a newline IF the final dst string would have a length less than maxlen */
static void safecat(char *dst, const char *append, size_t maxlen) {
	size_t dstlen, appendlen;
	dstlen = strlen(dst);
	appendlen = strlen(append);
	if (dstlen + appendlen + 2 < maxlen) {
		strcat(dst, append);
		strcat(dst, newline);
	}
}

/*************************************************/
/* Sends headers for the reply.					 */
/* HTTP/0.9 doesn't use headers, so just returns */
/*************************************************/
static bool send_headers(http_session_t *session, const char *status, int chunked)
{
	int         ret;
	int         stat_code;
	time_t      ti;
	size_t      idx;
	const char *status_line;
	struct stat stats;
	struct tm   tm;
	char *      headers;
	char        header[MAX_REQUEST_LINE + 1];
	bool        send_entity = true;

	if (session->socket == INVALID_SOCKET) {
		session->req.sent_headers = true;
		return false;
	}
	lprintf(LOG_DEBUG, "%04d %-5s [%s] Request resolved to: %s"
	        , session->socket, session->client.protocol, session->host_ip, session->req.physical_path);
	if (session->http_ver <= HTTP_0_9) {
		session->req.sent_headers = true;
		if (session->req.ld != NULL)
			session->req.ld->status = atoi(status);
		return true;
	}
	headers = static_cast<char *>(malloc(MAX_HEADERS_SIZE));
	if (headers == NULL)  {
		errprintf(LOG_CRIT, WHERE, "Could not allocate memory for response headers.");
		session->req.sent_headers = true;
		return false;
	}
	*headers = 0;
	/* send_headers() is called a second time when using chunked
	 * transfer encoding.  This allows setting headers while sending
	 * the response, and the CRLF at the end is required to terminate
	 * the response. */
	if (!session->req.sent_headers) {
		session->req.send_content = true;
		status_line = status;
		ret = stat(session->req.physical_path, &stats);
		if (session->req.method == HTTP_HEAD)
			session->req.send_content = false;
		if (session->req.method == HTTP_OPTIONS) {
			ret = -1;
			session->req.send_content = false;
		}
		if (!ret && session->req.if_modified_since && (stats.st_mtime <= session->req.if_modified_since) && !session->req.dynamic) {
			status_line = "304 Not Modified";
			ret = -1; // Avoid forcing 200 OK next
			/* 304 is further handled in stat_code == below */
		}
		if (!ret && session->req.if_range && (stats.st_mtime > session->req.if_range || session->req.dynamic)) {
			status_line = "200 OK";
			session->req.range_start = 0;
			session->req.range_end = 0;
			session->req.got_range = false;
		}
		if (session->req.send_location) {
			ret = -1;
			if (session->req.dynamic != IS_CGI && session->req.dynamic != IS_FASTCGI && (!chunked) && (!session->req.manual_length)) {
				session->req.send_content = false;
			}
			switch (session->req.send_location) {
				case MOVED_PERM:
					status_line = error_301;
					break;
				case MOVED_TEMPREDIR:
					status_line = error_307;
					break;
				case MOVED_TEMP:
					status_line = error_302;
					break;
			}
		}

		stat_code = atoi(status_line);
		if (session->req.ld != NULL)
			session->req.ld->status = stat_code;

		if (stat_code == 304 || stat_code == 204 || (stat_code >= 100 && stat_code <= 199)) {
			session->req.send_content = false;
			send_entity = false;
		}

		if (!session->req.send_content) {
			chunked = false;
		}

		/* Status-Line */
		safe_snprintf(header, sizeof(header), "%s %s", response_http_vers[session->http_ver > HTTP_0_9 ? HTTP_1_1 : HTTP_0_9], status_line);

		lprintf(LOG_DEBUG, "%04d %-5s [%s] Result: %s", session->socket, session->client.protocol, session->host_ip, header);

		safecat(headers, header, MAX_HEADERS_SIZE);

		/* General Headers */
		if (session->is_tls && startup->options & WEB_OPT_HSTS_SAFE) {
			safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_STS), "max-age=10886400; preload");
			safecat(headers, header, MAX_HEADERS_SIZE);
			safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_CSP), "block-all-mixed-content");
			safecat(headers, header, MAX_HEADERS_SIZE);
		}
		ti = time(NULL);
		if (gmtime_r(&ti, &tm) == NULL)
			memset(&tm, 0, sizeof(tm));
		safe_snprintf(header, sizeof(header), "%s: %s, %02d %s %04d %02d:%02d:%02d GMT"
		              , get_header(HEAD_DATE)
		              , days[tm.tm_wday], tm.tm_mday, months[tm.tm_mon]
		              , tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
		safecat(headers, header, MAX_HEADERS_SIZE);
		if (session->req.keep_alive) {
			safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_CONNECTION), "Keep-Alive");
			safecat(headers, header, MAX_HEADERS_SIZE);
		}
		else {
			safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_CONNECTION), "Close");
			safecat(headers, header, MAX_HEADERS_SIZE);
		}

		/* Response Headers */
		safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_SERVER), VERSION_NOTICE);
		safecat(headers, header, MAX_HEADERS_SIZE);
		if (session->req.vary_list) {
			safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_VARY), session->req.vary_list);
			safecat(headers, header, MAX_HEADERS_SIZE);
		}

		/* Entity Headers */
		if (session->req.dynamic) {
			safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_ALLOW), "GET, HEAD, POST, OPTIONS");
			safecat(headers, header, MAX_HEADERS_SIZE);
			safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_ACCEPT_RANGES), "none");
			safecat(headers, header, MAX_HEADERS_SIZE);
		}
		else {
			safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_ALLOW), "GET, HEAD, OPTIONS");
			safecat(headers, header, MAX_HEADERS_SIZE);
			safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_ACCEPT_RANGES), "bytes");
			safecat(headers, header, MAX_HEADERS_SIZE);
		}

		if (session->req.send_location) {
			if (session->req.location_to_send)
				safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_LOCATION), (session->req.location_to_send));
			else
				safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_LOCATION), (session->req.virtual_path));
			safecat(headers, header, MAX_HEADERS_SIZE);
		}

		if (chunked) {
			safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_TRANSFER_ENCODING), "Chunked");
			safecat(headers, header, MAX_HEADERS_SIZE);
		}

		/* DO NOT send a content-length for chunked */
		if (send_entity) {
			if (session->req.dynamic != IS_CGI && session->req.dynamic != IS_FASTCGI && (!chunked) && (!session->req.manual_length)) {
				if (ret)  {
					safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_LENGTH), "0");
					safecat(headers, header, MAX_HEADERS_SIZE);
				}
				else  {
					if ((session->req.got_range) && stat_code == 206) {
						safe_snprintf(header, sizeof(header), "%s: %" PRIdOFF, get_header(HEAD_LENGTH), session->req.range_end - session->req.range_start + 1);
						safecat(headers, header, MAX_HEADERS_SIZE);
					}
					else {
						safe_snprintf(header, sizeof(header), "%s: %" PRIdOFF, get_header(HEAD_LENGTH), stats.st_size);
						safecat(headers, header, MAX_HEADERS_SIZE);
					}
				}
			}

			if (!ret && !session->req.dynamic)  {
				safe_snprintf(header, sizeof(header), "%s: %s", get_header(HEAD_TYPE), session->req.mime_type);
				safecat(headers, header, MAX_HEADERS_SIZE);
				gmtime_r(&stats.st_mtime, &tm);
				safe_snprintf(header, sizeof(header), "%s: %s, %02d %s %04d %02d:%02d:%02d GMT"
				              , get_header(HEAD_LASTMODIFIED)
				              , days[tm.tm_wday], tm.tm_mday, months[tm.tm_mon]
				              , tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
				safecat(headers, header, MAX_HEADERS_SIZE);
			}

			if (session->req.got_range) {
				switch (stat_code) {
					case 206:   /* Partial reply */
						safe_snprintf(header, sizeof(header), "%s: bytes %" PRIdOFF "-%" PRIdOFF "/%" PRIdOFF, get_header(HEAD_CONTENT_RANGE), session->req.range_start, session->req.range_end, stats.st_size);
						safecat(headers, header, MAX_HEADERS_SIZE);
						break;
					default:
						safe_snprintf(header, sizeof(header), "%s: *", get_header(HEAD_CONTENT_RANGE));
						safecat(headers, header, MAX_HEADERS_SIZE);
						break;
				}
			}
		}
	}

	if (session->req.dynamic)  {
		/* Dynamic headers */
		/* Set up environment */
		for (idx = 0; session->req.dynamic_heads[idx] != NULL; idx++)
			safecat(headers, session->req.dynamic_heads[idx], MAX_HEADERS_SIZE);
		/* free() the headers so they don't get sent again if more are sent at the end of the request (chunked) */
		strListFreeStrings(session->req.dynamic_heads);
	}

	safecat(headers, "", MAX_HEADERS_SIZE);
	ret = bufprint(session, headers);
	session->req.sent_headers = true;
	drain_outbuf(session);
	session->req.write_chunked = chunked;
	free(headers);
	return ret;
}

static off_t sock_sendfile(http_session_t *session, char *path, off_t start, off_t end)
{
	int      file;
	off_t    ret = 0;
	ssize_t  i;
	char     buf[OUTBUF_LEN];       /* Input buffer */
	uint64_t remain;

	if (startup->options & WEB_OPT_DEBUG_TX) {
		if (start || end)
			lprintf(LOG_DEBUG, "%04d %-5s [%s] Sending bytes %" PRIdOFF "-%" PRIdOFF " of %s"
			        , session->socket, session->client.protocol, session->host_ip, start, end, path);
		else
			lprintf(LOG_DEBUG, "%04d %-5s [%s] Sending %s"
			        , session->socket, session->client.protocol, session->host_ip, path);
	}
	if ((file = open(path, O_RDONLY | O_BINARY)) == -1)
		lprintf(LOG_WARNING, "%04d !ERROR %d opening %s", session->socket, errno, path);
	else {
		if (start || end) {
			if (lseek(file, start, SEEK_SET) == -1) {
				lprintf(LOG_WARNING, "%04d !ERROR %d seeking to position %" PRIdOFF " in %s", session->socket, errno, start, path);
				close(file);
				return 0;
			}
			remain = end - start + 1;
		}
		else {
			remain = -1L;
		}
		while ((i = read(file, buf, (size_t)(remain > sizeof(buf)?sizeof(buf):remain))) > 0) {
			int wr = writebuf(session, buf, i);
			if (wr != i) {
				lprintf(LOG_WARNING, "%04d %-5s [%s] !ERROR sending %s (sent %d of %d bytes)"
					,session->socket, session->client.protocol, session->host_ip, path, wr, (int)i);
				close(file);
				return 0;
			}
			ret += i;
			remain -= i;
		}
		close(file);
	}
	return ret;
}

/********************************************************/
/* Sends a specified error message, closes the request, */
/* and marks the session to be closed 					*/
/********************************************************/
static void send_error(http_session_t * session, unsigned line, const char* message)
{
	char        error_code[4];
	struct stat sb;
	char        sbuf[MAX_PATH + 1];
	char        sbuf2[MAX_PATH + 1];
	bool        sent_ssjs = false;

	if (session->socket == INVALID_SOCKET)
		return;
	session->req.if_modified_since = 0;
	strlcpy(error_code, message, sizeof error_code);
	lprintf(LOG_INFO, "%04d %-5s [%s] !ERROR: %s (line %u) request: %s"
	        , session->socket, session->client.protocol, session->host_ip, str_has_ctrl(message) ? error_code: message, line, session->req.request_line);
	session->req.keep_alive = false;
	session->req.send_location = NO_LOCATION;
	SAFECOPY(session->req.status, message);
	if (atoi(error_code) < 500) {
		/*
		 * Attempt to run SSJS error pages
		 * If this fails, do the standard error page instead,
		 * ie: Don't "upgrade" to a 500 error
		 */

		if (session->req.error_dir) {
			/* We have a custom error directory from webctrl.ini look there first */
			SAFEPRINTF3(sbuf, "%s%s%s", session->req.error_dir, error_code, startup->ssjs_ext);
			if (stat(sbuf, &sb)) {
				/* No custom .ssjs error message... check for custom .html */
				SAFEPRINTF2(sbuf2, "%s%s.html", session->req.error_dir, error_code);
				if (stat(sbuf2, &sb)) {
					/* Nope, no custom .html error either, check for global ssjs one */
					SAFEPRINTF3(sbuf, "%s%s%s", error_dir, error_code, startup->ssjs_ext);
				}
			}
		}
		else {
			SAFEPRINTF3(sbuf, "%s%s%s", error_dir, error_code, startup->ssjs_ext);
		}
		if (!stat(sbuf, &sb)) {
			lprintf(LOG_DEBUG, "%04d %-5s [%s] Using SSJS error page %s", session->socket, session->client.protocol, session->host_ip, error_code);
			session->req.dynamic = IS_SSJS;
			if (js_setup(session)) {
				sent_ssjs = exec_ssjs(session, sbuf);
				if (sent_ssjs) {
					off_t snt = 0;

					lprintf(LOG_INFO, "%04d %-5s [%s] Sending generated error page %s", session->socket, session->client.protocol, session->host_ip, error_code);
					snt = sock_sendfile(session, session->req.physical_path, 0, 0);
					if (snt < 0)
						snt = 0;
					if (session->req.ld != NULL)
						session->req.ld->size = snt;
				}
				else
					session->req.dynamic = IS_STATIC;
			}
			else
				session->req.dynamic = IS_STATIC;
		}
	}
	if (!sent_ssjs) {
		if (session->req.error_dir) {
			SAFEPRINTF2(session->req.physical_path, "%s%s.html", session->req.error_dir, error_code);
			if (stat(session->req.physical_path, &sb))
				SAFEPRINTF2(session->req.physical_path, "%s%s.html", error_dir, error_code);
		}
		else
			SAFEPRINTF2(session->req.physical_path, "%s%s.html", error_dir, error_code);
		session->req.mime_type = get_mime_type(strrchr(session->req.physical_path, '.'));
		if (send_headers(session, message, false) && session->req.send_content) {
			if (!stat(session->req.physical_path, &sb)) {
				off_t snt = 0;
				snt = sock_sendfile(session, session->req.physical_path, 0, 0);
				if (snt < 0)
					snt = 0;
				if (session->req.ld != NULL)
					session->req.ld->size = snt;
			}
			else {
				lprintf(LOG_NOTICE, "%04d Error message file %s doesn't exist"
				        , session->socket, session->req.physical_path);
				safe_snprintf(sbuf, sizeof(sbuf)
				              , "<HTML><HEAD><TITLE>%s Error</TITLE></HEAD>"
				              "<BODY><H1>%s Error</H1><BR><H3>In addition, "
				              "I can't seem to find the %s error file</H3><br>"
				              "please notify <a href=\"mailto:sysop@%s\">"
				              "%s</a></BODY></HTML>"
				              , error_code, error_code, error_code, scfg.sys_inetaddr, scfg.sys_op);
				bufprint(session, sbuf);
				if (session->req.ld != NULL)
					session->req.ld->size = strlen(sbuf);
			}
		}
	}
	drain_outbuf(session);
	session->req.finished = true;
}

void http_logon(http_session_t * session, user_t *usr)
{
	if (usr == NULL)
		getuserdat(&scfg, &session->user);
	else
		session->user = *usr;

	if (session->user.number == session->last_user_num)
		return;

	if (session->subscan != NULL)
		getmsgptrs(&scfg, &session->user, session->subscan, NULL, NULL);

	session->logon_time = time(NULL);
	if (session->user.number == 0)
		SAFECOPY(session->username, unknown);
	else {
		SAFECOPY(session->username, session->user.alias);
		int result = loginuserdat(&scfg, &session->user, &session->client, /* use_prot: */true, startup->login_info_save);
		if (result != 0)
			errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] <%s> !Error %d writing user data for user #%d"
			          , session->socket, session->client.protocol, session->host_ip
			          , session->username, result, session->user.number);

	}
	SAFECOPY(session->client.user, session->username);
	session->client.usernum = session->user.number;
	client_on(session->socket, &session->client, /* update existing client record? */ true);
	if (session->user.number > 0)
		mqtt_user_login(&mqtt, &session->client);

	session->last_user_num = session->user.number;

	lprintf(LOG_DEBUG, "%04d %-5s [%s] <%s> logged-in"
	        , session->socket, session->client.protocol, session->host_ip, session->username);
}

void http_logoff(http_session_t* session, SOCKET socket, int line)
{
	if (session->last_user_num <= 0)
		return;

	lprintf(LOG_DEBUG, "%04d %-5s [%s] <%s> logged-out from line %d"
	        , socket, session->client.protocol, session->host_ip, session->user.alias, line);

	SAFECOPY(session->username, unknown);
	if (chk_ars(&scfg, startup->login_info_save, &session->user, &session->client)) {
		int i = logoutuserdat(&scfg, &session->user, session->logon_time);
		if (i != USER_SUCCESS)
			errprintf(LOG_ERR, WHERE, "%04d !ERROR %d in logoutuserdat", socket, i);
	}
	memset(&session->user, 0, sizeof(session->user));
	SAFECOPY(session->user.alias, unknown);
	session->last_user_num = session->user.number;

#ifdef _WIN32
	if (startup->sound.logout[0] && !sound_muted(&scfg))
		PlaySound(startup->sound.logout, NULL, SND_ASYNC | SND_FILENAME);
#endif

	mqtt_user_logout(&mqtt, &session->client, session->logon_time);
}

bool http_checkuser(http_session_t * session)
{
	if (session->req.dynamic == IS_SSJS) {
		if (session->last_js_user_num == session->user.number)
			return true;
		lprintf(LOG_DEBUG, "%04d %-5s [%s] JavaScript: Initializing User Objects"
			, session->socket, session->client.protocol, session->host_ip);
		JS_BEGINREQUEST(session->js_cx);
		if (session->user.number > 0) {
			if (!js_CreateUserObjects(session->js_cx, session->js_glob, &scfg, &session->user, &session->client
			                          , startup->file_vpath_prefix, session->subscan /* subscan */, &mqtt)) {
				JS_ENDREQUEST(session->js_cx);
				errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !JavaScript ERROR creating user objects"
					, session->socket, session->client.protocol, session->host_ip);
				send_error(session, __LINE__, "500 Error initializing JavaScript User Objects");
				return false;
			}
		}
		else {
			if (!js_CreateUserObjects(session->js_cx, session->js_glob, &scfg, /* user: */ NULL, &session->client
			                          , startup->file_vpath_prefix, session->subscan /* subscan */, &mqtt)) {
				JS_ENDREQUEST(session->js_cx);
				errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !ERROR initializing JavaScript User Objects"
					, session->socket, session->client.protocol, session->host_ip);
				send_error(session, __LINE__, "500 Error initializing JavaScript User Objects");
				return false;
			}
		}
		JS_ENDREQUEST(session->js_cx);
		session->last_js_user_num = session->user.number;
	}
	return true;
}

static void calculate_digest(http_session_t * session, char *ha1, char *ha2, unsigned char digest[MD5_DIGEST_SIZE])
{
	MD5 ctx;

	MD5_open(&ctx);
	MD5_digest(&ctx, ha1, strlen(ha1));
	MD5_digest(&ctx, ":", 1);
	/* exception on next line (session->req.auth.nonce==NULL) */
	MD5_digest(&ctx, session->req.auth.nonce, strlen(session->req.auth.nonce));
	MD5_digest(&ctx, ":", 1);

	if (session->req.auth.qop_value != QOP_NONE) {
		MD5_digest(&ctx, session->req.auth.nonce_count, strlen(session->req.auth.nonce_count));
		MD5_digest(&ctx, ":", 1);
		MD5_digest(&ctx, session->req.auth.cnonce, strlen(session->req.auth.cnonce));
		MD5_digest(&ctx, ":", 1);
		switch (session->req.auth.qop_value) {
			case QOP_AUTH:
				MD5_digest(&ctx, "auth", 4);
				break;
			case QOP_AUTH_INT:
				MD5_digest(&ctx, "auth-int", 7);
				break;
			default:
				break;
		}
		MD5_digest(&ctx, ":", 1);
	}
	MD5_digest(&ctx, ha2, strlen(ha2));
	MD5_close(&ctx, digest);
}

static bool digest_authentication(http_session_t* session, int auth_allowed, user_t* thisuser, const char** reason)
{
	unsigned char digest[MD5_DIGEST_SIZE];
	char          ha1[MD5_DIGEST_SIZE * 2 + 1];
	char          ha1l[MD5_DIGEST_SIZE * 2 + 1];
	char          ha1u[MD5_DIGEST_SIZE * 2 + 1];
	char          ha2[MD5_DIGEST_SIZE * 2 + 1];
	char *        pass;
	char *        p;
	time32_t      nonce_time;
	time32_t      now;
	MD5           ctx;

	if ((auth_allowed & (1 << AUTHENTICATION_DIGEST)) == 0) {
		*reason = "digest auth not allowed";
		return false;
	}
	if (session->req.auth.qop_value == QOP_UNKNOWN) {
		*reason = "QOP unknown";
		return false;
	}
	if (session->req.auth.algorithm == ALGORITHM_UNKNOWN) {
		*reason = "algorithm unknown";
		return false;
	}
	/* Validate rules from RFC-2617 */
	if (session->req.auth.qop_value == QOP_AUTH
	    || session->req.auth.qop_value == QOP_AUTH_INT) {
		if (session->req.auth.cnonce == NULL) {
			*reason = "no cnonce";
			return false;
		}
		if (session->req.auth.nonce_count == NULL) {
			*reason = "no nonce count";
			return false;
		}
	}
	else {
		if (session->req.auth.cnonce != NULL) {
			*reason = "unexpected cnonce present";
			return false;
		}
		if (session->req.auth.nonce_count != NULL) {
			*reason = "unexpected nonce count present";
			return false;
		}
	}

	/* H(A1) */
	MD5_open(&ctx);
	MD5_digest(&ctx, session->req.auth.username, strlen(session->req.auth.username));
	MD5_digest(&ctx, ":", 1);
	MD5_digest(&ctx, session->req.digest_realm?session->req.digest_realm:(session->req.realm?session->req.realm:scfg.sys_name), strlen(session->req.digest_realm?session->req.digest_realm:(session->req.realm?session->req.realm:scfg.sys_name)));
	MD5_digest(&ctx, ":", 1);
	MD5_digest(&ctx, thisuser->pass, strlen(thisuser->pass));
	MD5_close(&ctx, digest);
	MD5_hex(ha1, digest);

	/* H(A1)l */
	pass = strdup(thisuser->pass);
	strlwr(pass);
	MD5_open(&ctx);
	MD5_digest(&ctx, session->req.auth.username, strlen(session->req.auth.username));
	MD5_digest(&ctx, ":", 1);
	MD5_digest(&ctx, session->req.digest_realm?session->req.digest_realm:(session->req.realm?session->req.realm:scfg.sys_name), strlen(session->req.digest_realm?session->req.digest_realm:(session->req.realm?session->req.realm:scfg.sys_name)));
	MD5_digest(&ctx, ":", 1);
	MD5_digest(&ctx, pass, strlen(pass));
	MD5_close(&ctx, digest);
	MD5_hex(ha1l, digest);

	/* H(A1)u */
	strupr(pass);
	MD5_open(&ctx);
	MD5_digest(&ctx, session->req.auth.username, strlen(session->req.auth.username));
	MD5_digest(&ctx, ":", 1);
	MD5_digest(&ctx, session->req.digest_realm?session->req.digest_realm:(session->req.realm?session->req.realm:scfg.sys_name), strlen(session->req.digest_realm?session->req.digest_realm:(session->req.realm?session->req.realm:scfg.sys_name)));
	MD5_digest(&ctx, ":", 1);
	MD5_digest(&ctx, thisuser->pass, strlen(thisuser->pass));
	MD5_close(&ctx, digest);
	MD5_hex(ha1u, digest);
	free(pass);

	/* H(A2) */
	MD5_open(&ctx);
	MD5_digest(&ctx, methods[session->req.method], strlen(methods[session->req.method]));
	MD5_digest(&ctx, ":", 1);
	/* exception here, session->req.auth.digest_uri==NULL */
	MD5_digest(&ctx, session->req.auth.digest_uri, strlen(session->req.auth.digest_uri));
	/* TODO QOP==AUTH_INT */
	if (session->req.auth.qop_value == QOP_AUTH_INT) {
		*reason = "QOP value";
		return false;
	}
	MD5_close(&ctx, digest);
	MD5_hex(ha2, digest);

	/* Check password as in user base */
	calculate_digest(session, ha1, ha2, digest);
	if (thisuser->pass[0]) { // Zero-length password is "special" (any password will work)
		if (memcmp(digest, session->req.auth.digest, sizeof(digest))) {
			/* Check against lower-case password */
			calculate_digest(session, ha1l, ha2, digest);
			if (memcmp(digest, session->req.auth.digest, sizeof(digest))) {
				/* Check against upper-case password */
				calculate_digest(session, ha1u, ha2, digest);
				if (memcmp(digest, session->req.auth.digest, sizeof(digest))) {
					*reason = "digest mismatch";
					return false;
				}
			}
		}
	}

	/* Validate nonce */
	p = strchr(session->req.auth.nonce, '@');
	if (p == NULL) {
		session->req.auth.stale = true;
		*reason = "nonce lacks @";
		return false;
	}
	*p = 0;
	if (strcmp(session->req.auth.nonce, session->client.addr)) {
		session->req.auth.stale = true;
		*reason = "nonce doesn't match client IP address";
		return false;
	}
	*p = '@';
	p++;
	nonce_time = strtoul(p, &p, 10);
	if (*p) {
		session->req.auth.stale = true;
		*reason = "unexpected data after nonce time";
		return false;
	}
	now = (time32_t)time(NULL);
	if (nonce_time > now) {
		session->req.auth.stale = true;
		*reason = "nonce time in the future";
		return false;
	}
	if (nonce_time < now - 1800) {
		session->req.auth.stale = true;
		*reason = "stale nonce time";
		return false;
	}

	return true;
}

static void badlogin(SOCKET sock, const char* user, const char* passwd, client_t* client, union xp_sockaddr* addr)
{
	char            tmp[128];
	char            reason[128];
	ulong           count;
	login_attempt_t attempt;

	SAFEPRINTF(reason, "%s LOGIN", client->protocol);
	count = loginFailure(startup->login_attempt_list, addr, client->protocol, user, passwd, &attempt);
	if (count > 1)
		lprintf(LOG_NOTICE, "%04d %-5s [%s] !%lu " STR_FAILED_LOGIN_ATTEMPTS " in %s"
		        , sock, client->protocol, client->addr, count, duration_estimate_to_vstr(attempt.time - attempt.first, tmp, sizeof tmp, 1, 1));
	mqtt_user_login_fail(&mqtt, client, user);
	if (startup->login_attempt.hack_threshold && count >= startup->login_attempt.hack_threshold) {
		hacklog(&scfg, &mqtt, reason, user, passwd, client->host, addr);
#ifdef _WIN32
		if (startup->sound.hack[0] && !sound_muted(&scfg))
			PlaySound(startup->sound.hack, NULL, SND_ASYNC | SND_FILENAME);
#endif
	}
	if (startup->login_attempt.filter_threshold && count >= startup->login_attempt.filter_threshold) {
		snprintf(reason, sizeof reason, "%lu " STR_FAILED_LOGIN_ATTEMPTS " in %s"
		         , count, duration_estimate_to_str(attempt.time - attempt.first, tmp, sizeof tmp, 1, 1));
		filter_ip(&scfg, client->protocol, reason
		          , client->host, client->addr, user, /* fname: */ NULL, startup->login_attempt.filter_duration);
	}
	if (count > 1)
		mswait(startup->login_attempt.delay);
}

static bool check_ars(http_session_t * session)
{
	uchar *   ar;
	bool      authorized;
	int       i;
	user_t    thisuser;
	int       auth_allowed = 0;
	unsigned *auth_list;
	unsigned  auth_list_len;

	auth_list = parseEnumList(session->req.auth_list?session->req.auth_list:default_auth_list, ",", (str_list_t)auth_type_names, &auth_list_len);
	for (i = 0; ((unsigned)i) < auth_list_len; i++)
		auth_allowed |= 1 << auth_list[i];
	if (auth_list)
		free(auth_list);

	/* No authentication provided */
	if (session->req.auth.type == AUTHENTICATION_UNKNOWN) {
		/* No authentication information... */
		if (session->last_user_num != 0) {
			if (session->last_user_num > 0)
				http_logoff(session, session->socket, __LINE__);
			session->user.number = 0;
			http_logon(session, NULL);
		}
		if (!http_checkuser(session))
			return false;
		if (session->req.ars[0]) {
			/* There *IS* an ARS string  ie: Auth is required */
			if (startup->options & WEB_OPT_DEBUG_RX)
				lprintf(LOG_NOTICE, "%04d !No authentication information", session->socket);
			return false;
		}
		if (session->user.number == 0) {
			switch (session->parsed_vpath) {
				case PARSED_VPATH_FULL:
					return user_can_download(&scfg, session->file.dir, &session->user, &session->client, NULL);
				case PARSED_VPATH_ROOT:
					return user_can_access_all_libs(&scfg, &session->user, &session->client);
				case PARSED_VPATH_LIB:
					return user_can_access_all_dirs(&scfg, session->libnum, &session->user, &session->client);
				case PARSED_VPATH_DIR:
					return user_can_access_dir(&scfg, session->file.dir, &session->user, &session->client);
				default:
					return true;
			}
		}
		/* No auth required, allow */
		return true;
	}
	/* Require a password */
	i = find_login_id(&scfg, session->req.auth.username);
	if (i == 0) {
		if (session->last_user_num != 0) {
			if (session->last_user_num > 0)
				http_logoff(session, session->socket, __LINE__);
			session->user.number = 0;
			http_logon(session, NULL);
		}
		if (!http_checkuser(session))
			return false;
		if (scfg.sys_misc & SM_ECHO_PW && session->req.auth.type == AUTHENTICATION_BASIC)
			lprintf(LOG_NOTICE, "%04d !UNKNOWN USER: '%s' (password: %s)"
			        , session->socket, session->req.auth.username, session->req.auth.password);
		else
			lprintf(LOG_NOTICE, "%04d !UNKNOWN USER: '%s'"
			        , session->socket, session->req.auth.username);
		return false;
	}
	thisuser.number = i;
	getuserdat(&scfg, &thisuser);
	switch (session->req.auth.type) {
		case AUTHENTICATION_TLS_PSK:
			if ((auth_allowed & (1 << AUTHENTICATION_TLS_PSK)) == 0)
				return false;
			if (session->last_user_num != 0) {
				if (session->last_user_num > 0)
					http_logoff(session, session->socket, __LINE__);
				session->user.number = 0;
				http_logon(session, NULL);
			}
			if (!http_checkuser(session))
				return false;
			break;
		case AUTHENTICATION_BASIC:
			if ((auth_allowed & (1 << AUTHENTICATION_BASIC)) == 0)
				return false;
			if (thisuser.pass[0] && stricmp(thisuser.pass, session->req.auth.password)) {
				if (session->last_user_num != 0) {
					if (session->last_user_num > 0)
						http_logoff(session, session->socket, __LINE__);
					session->user.number = 0;
					http_logon(session, NULL);
				}
				if (!http_checkuser(session))
					return false;
				if (scfg.sys_misc & SM_ECHO_PW)
					lprintf(LOG_WARNING, "%04d <%s> !BASIC AUTHENTICATION FAILURE (password: %s)"
					        , session->socket, session->req.auth.username, session->req.auth.password);
				else
					lprintf(LOG_WARNING, "%04d <%s> !BASIC AUTHENTICATION FAILURE"
					        , session->socket, session->req.auth.username);
				badlogin(session->socket, session->req.auth.username, session->req.auth.password, &session->client, &session->addr);
				return false;
			}
			break;
		case AUTHENTICATION_DIGEST:
		{
			const char* reason = "unknown";
			if (!digest_authentication(session, auth_allowed, &thisuser, &reason)) {
				lprintf(LOG_NOTICE, "%04d <%s> !DIGEST AUTHENTICATION FAILURE (reason: %s)"
				        , session->socket, session->req.auth.username, reason);
				badlogin(session->socket, session->req.auth.username, "<digest>", &session->client, &session->addr);
				return false;
			}
			break;
		}
		default:
			break;
	}

	switch (session->parsed_vpath) {
		case PARSED_VPATH_LIB:
			if (!user_can_access_lib(&scfg, session->libnum, &thisuser, &session->client))
				return false;
			break;
		case PARSED_VPATH_DIR:
			if (!user_can_access_dir(&scfg, session->file.dir, &thisuser, &session->client))
				return false;
			break;
		default:
			break;
	}

	if (!chk_ars(&scfg, startup->login_ars, &thisuser, &session->client)) {
		lprintf(LOG_NOTICE, "%04d <%s> !Insufficient server access: %s"
		        , session->socket, thisuser.alias, startup->login_ars);
		badlogin(session->socket, thisuser.alias, NULL, &session->client, &session->addr);
		return false;
	}

	if (i != session->last_user_num) {
		http_logoff(session, session->socket, __LINE__);
		session->user.number = i;
		http_logon(session, &thisuser);
	}
	if (!http_checkuser(session))
		return false;

	if (session->req.ld != NULL) {
		FREE_AND_NULL(session->req.ld->user);
		/* FREE()d in http_logging_thread */
		session->req.ld->user = strdup(session->req.auth.username);
	}

	uint reason = CantDownloadFromDir;
	if (session->parsed_vpath == PARSED_VPATH_FULL) {
		if (download_is_free(&scfg, session->file.dir, &session->user, &session->client)
		    || session->user.cdt >= session->file.cost)
			authorized = user_can_download(&scfg, session->file.dir, &session->user, &session->client, &reason);
		else
			authorized = false;
	} else {
		ar = arstr(NULL, session->req.ars, &scfg, NULL);
		authorized = chk_ar(&scfg, ar, &session->user, &session->client);
		free(ar);
	}
	if (authorized)  {
		switch (session->req.auth.type) {
			case AUTHENTICATION_TLS_PSK:
				add_env(session, "AUTH_TYPE", "TLS-PSK");
				break;
			case AUTHENTICATION_BASIC:
				add_env(session, "AUTH_TYPE", "Basic");
				break;
			case AUTHENTICATION_DIGEST:
				add_env(session, "AUTH_TYPE", "Digest");
				break;
			default:
				break;
		}
		/* Should use real name if set to do so somewhere ToDo */
		add_env(session, "REMOTE_USER", session->user.alias);

		if (thisuser.pass[0])
			loginSuccess(startup->login_attempt_list, &session->addr);

		return true;
	}

	/* Should go to the hack log? */
	lprintf(LOG_WARNING, "%04d %-5s [%s] <%s> !AUTHORIZATION FAILURE (reason: %u), ARS: %s"
	        , session->socket, session->client.protocol, session->host_ip, session->req.auth.username, reason + 1, session->req.ars);

#ifdef _WIN32
	if (startup->sound.hack[0] && !sound_muted(&scfg))
		PlaySound(startup->sound.hack, NULL, SND_ASYNC | SND_FILENAME);
#endif

	return false;
}

static named_string_t** read_ini_list(char* path, const char* section, const char* desc
                                      , named_string_t** list, bool warn)
{
	size_t i;
	FILE*  fp;

	list = iniFreeNamedStringList(list);

	if ((fp = iniOpenFile(path, /* for_modify: */ false)) != NULL) {
		list = iniReadNamedStringList(fp, section);
		iniCloseFile(fp);
		COUNT_LIST_ITEMS(list, i);
		if (i)
			lprintf(LOG_DEBUG, "Read %lu %s from %s section of %s"
			        , (ulong)i, desc, section == NULL ? "root":section, path);
	} else
		lprintf(warn ? LOG_WARNING : LOG_DEBUG, "Error %d (%s) opening %s", errno, strerror(errno), path);
	return list;
}

static int sess_recv(http_session_t *session, char *buf, size_t length, int flags)
{
	int len = 0;

	if (session->is_tls) {
		if (flags == MSG_PEEK) {
			if (session->peeked_valid) {
				buf[0] = session->peeked;
				return 1;
			}
			if (HANDLE_CRYPT_CALL_EXCEPT2(cryptPopData(session->tls_sess, &session->peeked, 1, &len), session, "popping data", CRYPT_ERROR_TIMEOUT, CRYPT_ERROR_COMPLETE)) {
				if (len == 1) {
					session->peeked_valid = true;
					buf[0] = session->peeked;
					return 1;
				}
				return 0;
			}
			return -1;
		}
		else {
			if (session->peeked_valid) {
				buf[0] = session->peeked;
				session->peeked_valid = false;
				return 1;
			}
			if (HANDLE_CRYPT_CALL_EXCEPT2(cryptPopData(session->tls_sess, buf, length, &len), session, "popping data", CRYPT_ERROR_TIMEOUT, CRYPT_ERROR_COMPLETE)) {
				if (len == 0) {
					session->tls_pending = false;
					len = -1;
				}
				return len;
			}
			return -1;
		}
	}
	else {
		return recv(session->socket, buf, length, flags);
	}
}

static int sockreadline(http_session_t * session, char *buf, size_t length)
{
	char  ch;
	DWORD i;
	DWORD chucked = 0;

	for (i = 0; true;) {
		if (session->socket == INVALID_SOCKET)
			return -1;
		if ((!session->is_tls) || (!session->tls_pending)) {
			if (socket_readable(session->socket, startup->max_inactivity * 1000)) {
				if (session->is_tls)
					session->tls_pending = true;
			}
			else {
				/* Timeout */
				lprintf(LOG_NOTICE, "%04d %-5s [%s] Session timeout due to inactivity (%d seconds)"
					, session->socket, session->client.protocol, session->host_ip, startup->max_inactivity);
				return -1;
			}
		}

		switch (sess_recv(session, &ch, 1, 0)) {
			case -1:
				if (session->is_tls || SOCKET_ERRNO != EAGAIN) {
					if (!session->is_tls) {
						if (startup->options & WEB_OPT_DEBUG_RX)
							lprintf(LOG_DEBUG, "%04d %-5s [%s] !ERROR %d receiving on socket"
								, session->socket, session->client.protocol, session->host_ip, SOCKET_ERRNO);
					}
					close_session_socket(session);
					return -1;
				}
				break;
			case 0:
				/* Socket has been closed */
				close_session_socket(session);
				return -1;
		}

		if (ch == '\n')
			break;

		if (i < length)
			buf[i++] = ch;
		else
			chucked++;
	}

	/* Terminate at length if longer */
	if (i > length)
		i = length;

	while (i > 0 && buf[i - 1] == '\r')
		i--;

	buf[i] = 0;

	if (startup->options & WEB_OPT_DEBUG_RX) {
		lprintf(LOG_DEBUG, "%04d %-5s [%s] RX: %s", session->socket, session->client.protocol, session->host_ip, buf);
		if (chucked)
			lprintf(LOG_DEBUG, "%04d %-5s [%s] Long header, chucked %d bytes", session->socket, session->client.protocol, session->host_ip, chucked);
	}
	return i;
}

#if defined(_WIN32)
static int pipereadline(HANDLE pipe, char *buf, size_t length, char *fullbuf, size_t fullbuf_len)
#else
static int pipereadline(int pipe, char *buf, size_t length, char *fullbuf, size_t fullbuf_len)
#endif
{
	char  ch;
	DWORD i;
	int   ret = 0;

	/* Terminate buffers */
	if (buf != NULL)
		buf[0] = 0;
	if (fullbuf != NULL)
		fullbuf[0] = 0;
	for (i = 0; true;) {
#if defined(_WIN32)
		ret = 0;
		ReadFile(pipe, &ch, 1, (DWORD*)&ret, NULL);
#else
		if (!socket_readable(pipe, startup->max_cgi_inactivity * 1000))
			return -1;
		ret = read(pipe, &ch, 1);
#endif
		if (ret == 1)  {
			if (fullbuf != NULL && i < (fullbuf_len - 1)) {
				fullbuf[i] = ch;
				fullbuf[i + 1] = 0;
			}

			if (ch == '\n')
				break;

			if (buf != NULL && i < length)
				buf[i] = ch;

			i++;
		}
		else
			return -1;
	}

	/* Terminate at length if longer */
	if (i > length)
		i = length;

	if (i > 0 && buf != NULL && buf[i - 1] == '\r')
		buf[--i] = 0;
	else {
		if (buf != NULL)
			buf[i] = 0;
	}

	return i;
}

static int recvbufsocket(http_session_t *session, char *buf, long count)
{
	int rd = 0;
	int i;

	if (count < 1) {
		errno = ERANGE;
		return 0;
	}

	while (rd < count && session_check(session, NULL, NULL, startup->max_inactivity * 1000))  {
		i = sess_recv(session, buf + rd, count - rd, 0);
		switch (i) {
			case -1:
				if (SOCKET_ERRNO == EAGAIN && !session->is_tls)
					break;
			// Fall-through...
			case 0:
				close_session_socket(session);
				*buf = 0;
				return 0;
		}

		rd += i;
	}

	if (rd == count)  {
		return rd;
	}

	*buf = 0;
	return 0;
}

static void unescape(char *p)
{
	char * dst;
	char   code[3];

	dst = p;
	for (; *p; p++) {
		if (*p == '%' && IS_HEXDIGIT(*(p + 1)) && IS_HEXDIGIT(*(p + 2))) {
			sprintf(code, "%.2s", p + 1);
			*(dst++) = (char)strtol(code, NULL, 16);
			p += 2;
		}
		else  {
			if (*p == '+')  {
				*(dst++) = ' ';
			}
			else  {
				*(dst++) = *p;
			}
		}
	}
	*(dst) = 0;
}

static void js_add_queryval(http_session_t * session, const char *key, const char *value)
{
	JSObject* keyarray;
	jsval     val;
	jsuint    len;
	int       alen;

	/* Return existing object if it's already been created */
	if (JS_GetProperty(session->js_cx, session->js_query, key, &val) && !JSVAL_NULL_OR_VOID(val))  {
		keyarray = JSVAL_TO_OBJECT(val);
		alen = -1;
	}
	else {
		keyarray = JS_NewArrayObject(session->js_cx, 0, NULL);
		if (!JS_DefineProperty(session->js_cx, session->js_query, key, OBJECT_TO_JSVAL(keyarray)
		                       , NULL, NULL, JSPROP_ENUMERATE))
			return;
		alen = 0;
	}

	if (alen == -1) {
		if (JS_GetArrayLength(session->js_cx, keyarray, &len) == JS_FALSE)
			return;
		alen = len;
	}

	JSString* js_str = JS_NewStringCopyZ(session->js_cx, value);
	if (js_str == NULL)
		errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] failed to create JSString for query value '%s', key=%s"
		          , session->socket, session->client.protocol, session->host_ip, value, key);
	else {
		lprintf(LOG_DEBUG, "%04d %-5s [%s] Adding query value %s=%s at pos %d"
		        , session->socket, session->client.protocol, session->host_ip, key, value, alen);
		val = STRING_TO_JSVAL(js_str);
		JS_SetElement(session->js_cx, keyarray, alen, &val);
	}
}

static void js_add_cookieval(http_session_t * session, const char *key, const char *value)
{
	JSObject* keyarray;
	jsval     val;
	jsuint    len;
	int       alen;

	/* Return existing object if it's already been created */
	if (JS_GetProperty(session->js_cx, session->js_cookie, key, &val) && !JSVAL_NULL_OR_VOID(val))  {
		keyarray = JSVAL_TO_OBJECT(val);
		alen = -1;
	}
	else {
		keyarray = JS_NewArrayObject(session->js_cx, 0, NULL);
		if (!JS_DefineProperty(session->js_cx, session->js_cookie, key, OBJECT_TO_JSVAL(keyarray)
		                       , NULL, NULL, JSPROP_ENUMERATE))
			return;
		alen = 0;
	}

	if (alen == -1) {
		if (JS_GetArrayLength(session->js_cx, keyarray, &len) == JS_FALSE)
			return;
		alen = len;
	}

	JSString* js_str = JS_NewStringCopyZ(session->js_cx, value);
	if (js_str == NULL)
		errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] failed to create JSString for cookie value '%s', key=%s"
		          , session->socket, session->client.protocol, session->host_ip, value, key);
	else {
		lprintf(LOG_DEBUG, "%04d %-5s [%s] Adding cookie value %s=%s at pos %d"
		        , session->socket, session->client.protocol, session->host_ip, key, value, alen);
		val = STRING_TO_JSVAL(js_str);
		JS_SetElement(session->js_cx, keyarray, alen, &val);
	}
}

static void js_add_request_property(http_session_t * session, const char *key, const char *value, size_t len, bool writeable)
{
	JSString* js_str;

	if (session->js_cx == NULL || session->js_request == NULL)
		return;
	if (key == NULL)
		return;
	if (value == NULL) {
		// Remove property
		JS_DeleteProperty(session->js_cx, session->js_request, key);
	} else {
		if (len)
			js_str = JS_NewStringCopyN(session->js_cx, value, len);
		else
			js_str = JS_NewStringCopyZ(session->js_cx, value);

		if (js_str == NULL)
			return;

		uintN attrs = JSPROP_ENUMERATE;
		if (!writeable)
			attrs |= JSPROP_READONLY;
		JS_DefineProperty(session->js_cx, session->js_request, key, STRING_TO_JSVAL(js_str)
		                  , NULL, NULL, attrs);
	}
}

static void js_add_request_prop_writeable(http_session_t * session, const char *key, const char *value)
{
	js_add_request_property(session, key, value, 0, /* writeable: */ true);
}

static void js_add_request_prop(http_session_t * session, const char *key, const char *value)
{
	js_add_request_property(session, key, value, 0, /* writeable: */ false);
}

static void js_add_header(http_session_t * session, const char *key, const char *value)
{
	JSString* js_str;
	char *    lckey;

	if ((lckey = strdup(key)) == NULL)
		return;
	strlwr(lckey);
	if ((js_str = JS_NewStringCopyZ(session->js_cx, value)) == NULL) {
		free(lckey);
		return;
	}
	JS_DefineProperty(session->js_cx, session->js_header, lckey, STRING_TO_JSVAL(js_str)
	                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
	free(lckey);
}

#if 0
static void js_parse_multipart(http_session_t * session, char *p)  {
	size_t key_len;
	size_t value_len;
	char * lp;
	char * key;
	char * value;

	if (p == NULL)
		return;

	lp = p;

	while ((key_len = strcspn(lp, "=")) != 0)  {
		key = lp;
		lp += key_len;
		if (*lp) {
			*lp = 0;
			lp++;
		}
		value_len = strcspn(lp, "&");
		value = lp;
		lp += value_len;
		if (*lp) {
			*lp = 0;
			lp++;
		}
		unescape(value);
		unescape(key);
		js_add_queryval(session, key, value);
	}
}
#endif

static void js_parse_query(http_session_t * session, char *p)  {
	size_t key_len;
	size_t value_len;
	char * lp;
	char * key;
	char * value;

	if (p == NULL)
		return;

	lp = p;

	while ((key_len = strcspn(lp, "=")) != 0)  {
		key = lp;
		lp += key_len;
		if (*lp) {
			*lp = 0;
			lp++;
		}
		value_len = strcspn(lp, "&");
		value = lp;
		lp += value_len;
		if (*lp) {
			*lp = 0;
			lp++;
		}
		unescape(value);
		unescape(key);
		js_add_queryval(session, key, value);
	}
}

static char *get_token_value(char **p)
{
	char *pos = *p;
	char *start;
	char *out;
	bool  escaped = false;

	start = pos;
	out = start;
	if (*pos == '"') {
		for (pos++; *pos; pos++) {
			if (escaped && *pos)
				*(out++) = *pos;
			else if (*pos == '"') {
				pos++;
				break;
			}
			else if (*pos == '\\')
				escaped = true;
			else
				*(out++) = *pos;
		}
	}
	else {
		for (; *pos; pos++) {
			if (iscntrl(*pos))
				break;
			switch (*pos) {
				case 0:
				case '(':
				case ')':
				case '<':
				case '>':
				case '@':
				case ',':
				case ';':
				case ':':
				case '\\':
				case '"':
				case '/':
				case '[':
				case ']':
				case '?':
				case '=':
				case '{':
				case '}':
				case ' ':
				case '\t':
					goto end_of_text;
			}
			*(out++) = *pos;
		}
	}
end_of_text:
	while (*pos == ',' || IS_WHITESPACE(*pos))
		pos++;
	*out = 0;
	*p = pos;
	return start;
}

static int hexval(unsigned char ch)
{
	ch -= '0';
	if (ch < 10)
		return ch;
	ch -= 7;
	if (ch < 16 && ch > 9)
		return ch;
	if (ch > 41) {
		ch -= 32;
		if (ch < 16 && ch > 9)
			return ch;
	}
	return 0;
}

static bool parse_headers(http_session_t * session)
{
	char * head_line;
	char * value;
	char * tvalue;
	char * last;
	char * p;
	int    i;
	size_t idx;
	size_t content_len = 0;
	char   env_name[128];
	char   portstr[7];  // ":65535"

	for (idx = 0; session->req.headers[idx] != NULL; idx++) {
		/* TODO: strdup() is possibly too slow here... */
		head_line = strdup(session->req.headers[idx]);
		if ((strtok_r(head_line, ":", &last)) != NULL && (value = strtok_r(NULL, "", &last)) != NULL) {
			i = get_header_type(head_line);
			while (*value && *value <= ' ') value++;
			if (startup->proxy_ip_header[0] && stricmp(startup->proxy_ip_header, head_line) == 0) {
				lprintf(LOG_INFO, "%04d %-5s [%s] New IP Address from %s header: %s", session->socket, session->client.protocol, session->host_ip, startup->proxy_ip_header, value);
				SAFECOPY(session->host_ip, value);
			}
			switch (i) {
				case HEAD_AUTH:
					/* If you're authenticated via TLS-PSK, you can't use basic or digest */
					if (session->req.auth.type != AUTHENTICATION_TLS_PSK) {
						if ((p = strtok_r(value, " ", &last)) != NULL) {
							if (stricmp(p, "Basic") == 0) {
								p = strtok_r(NULL, " ", &last);
								if (p == NULL)
									break;
								while (*p && *p < ' ') p++;
								b64_decode(p, strlen(p), p, strlen(p));
								p = strtok_r(p, ":", &last);
								if (p) {
									if (strlen(p) >= sizeof(session->req.auth.username))
										break;
									SAFECOPY(session->req.auth.username, p);
									p = strtok_r(NULL, ":", &last);
									if (p) {
										if (strlen(p) >= sizeof(session->req.auth.password))
											break;
										SAFECOPY(session->req.auth.password, p);
										session->req.auth.type = AUTHENTICATION_BASIC;
									}
								}
							}
							else if (stricmp(p, "Digest") == 0) {
								p = strtok_r(NULL, "", &last);
								/* Defaults */
								session->req.auth.algorithm = ALGORITHM_MD5;
								session->req.auth.type = AUTHENTICATION_DIGEST;
								/* Parse out values one at a time and store */
								while (p != NULL && *p) {
									while (IS_WHITESPACE(*p))
										p++;
									if (strnicmp(p, "username=", 9) == 0) {
										p += 9;
										tvalue = get_token_value(&p);
										if (strlen(tvalue) >= sizeof(session->req.auth.username))
											break;
										SAFECOPY(session->req.auth.username, tvalue);
									}
									else if (strnicmp(p, "realm=", 6) == 0) {
										p += 6;
										session->req.auth.realm = strdup(get_token_value(&p));
									}
									else if (strnicmp(p, "nonce=", 6) == 0) {
										p += 6;
										session->req.auth.nonce = strdup(get_token_value(&p));
									}
									else if (strnicmp(p, "uri=", 4) == 0) {
										p += 4;
										session->req.auth.digest_uri = strdup(get_token_value(&p));
									}
									else if (strnicmp(p, "response=", 9) == 0) {
										p += 9;
										tvalue = get_token_value(&p);
										if (strlen(tvalue) == 32) {
											for (i = 0; i < 16; i++) {
												session->req.auth.digest[i] = hexval(tvalue[i * 2]) << 4 | hexval(tvalue[i * 2 + 1]);
											}
										}
									}
									else if (strnicmp(p, "algorithm=", 10) == 0) {
										p += 10;
										tvalue = get_token_value(&p);
										if (stricmp(tvalue, "MD5") == 0) {
											session->req.auth.algorithm = ALGORITHM_MD5;
										}
										else {
											session->req.auth.algorithm = ALGORITHM_UNKNOWN;
										}
									}
									else if (strnicmp(p, "cnonce=", 7) == 0) {
										p += 7;
										session->req.auth.cnonce = strdup(get_token_value(&p));
									}
									else if (strnicmp(p, "qop=", 4) == 0) {
										p += 4;
										tvalue = get_token_value(&p);
										if (stricmp(tvalue, "auth") == 0) {
											session->req.auth.qop_value = QOP_AUTH;
										}
										else if (stricmp(tvalue, "auth-int") == 0) {
											session->req.auth.qop_value = QOP_AUTH_INT;
										}
										else {
											session->req.auth.qop_value = QOP_UNKNOWN;
										}
									}
									else if (strnicmp(p, "nc=", 3) == 0) {
										p += 3;
										session->req.auth.nonce_count = strdup(get_token_value(&p));
									}
									else {
										while (*p && *p != '=')
											p++;
										if (*p == '=')
											get_token_value(&p);
									}
								}
								if (session->req.auth.digest_uri == NULL)
									session->req.auth.digest_uri = strdup(session->req.request_line);
								/* Validate that we have the required values... */
								switch (session->req.auth.qop_value) {
									case QOP_NONE:
										if (session->req.auth.realm == NULL
										    || session->req.auth.nonce == NULL
										    || session->req.auth.digest_uri == NULL)
											send_error(session, __LINE__, "400 Bad Request");
										break;
									case QOP_AUTH:
									case QOP_AUTH_INT:
										if (session->req.auth.realm == NULL
										    || session->req.auth.nonce == NULL
										    || session->req.auth.nonce_count == NULL
										    || session->req.auth.cnonce == NULL
										    || session->req.auth.digest_uri == NULL)
											send_error(session, __LINE__, "400 Bad Request");
										break;
									default:
										send_error(session, __LINE__, "400 Bad Request");
										break;
								}
							}
						}
					}
					break;
				case HEAD_LENGTH:
					add_env(session, "CONTENT_LENGTH", value);
					content_len = strtol(value, NULL, 10);
					break;
				case HEAD_IFMODIFIED:
					session->req.if_modified_since = decode_date(value);
					break;
				case HEAD_CONNECTION:
					if (!stricmp(value, "Keep-Alive")) {
						session->req.keep_alive = true;
					}
					if (!stricmp(value, "Close")) {
						session->req.keep_alive = false;
					}
					break;
				case HEAD_REFERER:
					if (session->req.ld != NULL) {
						FREE_AND_NULL(session->req.ld->referrer);
						/* FREE()d in http_logging_thread() */
						session->req.ld->referrer = strdup(value);
					}
					break;
				case HEAD_AGENT:
					lprintf(LOG_INFO, "%04d %-5s [%s] User-Agent: %s", session->socket, session->client.protocol, session->host_ip, value);
					if (session->req.ld != NULL) {
						FREE_AND_NULL(session->req.ld->agent);
						/* FREE()d in http_logging_thread() */
						session->req.ld->agent = strdup(value);
					}
					break;
				case HEAD_TRANSFER_ENCODING:
					if (!stricmp(value, "chunked"))
						session->req.read_chunked = true;
					else
						send_error(session, __LINE__, "501 Not Implemented");
					break;
				case HEAD_RANGE:
					if (strnicmp(value, "bytes=", 6)) {
						send_error(session, __LINE__, error_416);
						break;
					}
					value += 6;
					if (strchr(value, ',') != NULL) {   /* We don't do multiple ranges yet - TODO */
						send_error(session, __LINE__, error_416);
						break;
					}
					if (*value == '-') {
						if (!isdigit(*(value + 1))) {     // Presumably Win32 will assert if it's negative
							send_error(session, __LINE__, error_416);
							break;
						}
						errno = 0;
						session->req.range_start = strtoll(value, &value, 10);
						if (errno || session->req.range_start >= 0) {
							send_error(session, __LINE__, error_416);
							break;
						}
						session->req.range_end = -1;
					}
					else {
						if (!isdigit(*value)) {     // Presumably Win32 will assert if it's negative
							send_error(session, __LINE__, error_416);
							break;
						}
						errno = 0;
						session->req.range_start = strtoll(value, &value, 10);
						if (errno) {
							send_error(session, __LINE__, error_416);
							break;
						}
						if (*value != '-') {
							send_error(session, __LINE__, error_416);
							break;
						}
						value++;
						if (*value == 0) {
							session->req.range_end = -1;
						}
						else {
							if (!isdigit(*value)) {     // Presumably Win32 will assert if it's negative
								send_error(session, __LINE__, error_416);
								break;
							}
							session->req.range_end = strtoll(value, &value, 10);
							if (errno) {
								send_error(session, __LINE__, error_416);
								break;
							}
						}
					}
					if (*value) {
						send_error(session, __LINE__, error_416);
						break;
					}
					session->req.got_range = true;
					break;
				case HEAD_IFRANGE:
					session->req.if_range = decode_date(value);
					break;
				case HEAD_TYPE:
					add_env(session, "CONTENT_TYPE", value);
					break;
				case HEAD_UPGRADEINSECURE:
					if (startup->options & WEB_OPT_HSTS_SAFE) {
						if (strcmp(value, "1") == 0) {
							if (!session->is_tls) {
								portstr[0] = 0;
								if (startup->tls_port != 443)
									sprintf(portstr, ":%hu", startup->tls_port);
								p = static_cast<char *>(realloc(session->req.vary_list, (session->req.vary_list ? strlen(session->req.vary_list) + 2 : 0) + strlen(get_header(HEAD_UPGRADEINSECURE)) + 1));
								if (p == NULL)
									send_error(session, __LINE__, error_500);
								else {
									if (session->req.vary_list)
										strcat(p, ", ");
									else
										*p = '\0';
									strcat(p, get_header(HEAD_UPGRADEINSECURE));
									session->req.vary_list = p;

									session->req.send_location = MOVED_TEMPREDIR;
									session->req.upgrading = true;
									session->req.keep_alive = false;
									FREE_AND_NULL(session->req.location_to_send);
									if (asprintf(&session->req.location_to_send, "https://%s%s%s", session->req.vhost, portstr, session->req.virtual_path) < 0)
										send_error(session, __LINE__, error_500);
								}
							}
						}
					}
					break;
				default:
					break;
			}
			SAFEPRINTF(env_name, "HTTP_%s", head_line);
			add_env(session, env_name, value);
		}
		free(head_line);
	}
	if (content_len)
		session->req.post_len = content_len;
	add_env(session, "SERVER_NAME", session->req.host[0] ? session->req.host : server_host_name());
	return true;
}

static bool parse_js_headers(http_session_t * session)
{
	char * head_line;
	char * value;
	char * last;
	char * p;
	int    i;
	size_t idx;

	for (idx = 0; session->req.headers[idx] != NULL; idx++) {
		head_line = session->req.headers[idx];
		if ((strtok_r(head_line, ":", &last)) != NULL && (value = strtok_r(NULL, "", &last)) != NULL) {
			i = get_header_type(head_line);
			while (*value && *value <= ' ') value++;
			js_add_header(session, head_line, value);
			switch (i) {
				case HEAD_TYPE:
					if (session->req.dynamic == IS_SSJS) {
						/*
						 * We need to parse out the files based on RFC1867
						 *
						 * And example reponse looks like this:
						 * Content-type: multipart/form-data, boundary=AaB03x
						 *
						 * --AaB03x
						 * content-disposition: form-data; name="field1"
						 *
						 * Joe Blow
						 * --AaB03x
						 * content-disposition: form-data; name="pics"
						 * Content-type: multipart/mixed, boundary=BbC04y
						 *
						 * --BbC04y
						 * Content-disposition: attachment; filename="file1.txt"
						 *
						 * Content-Type: text/plain
						 *
						 * ... contents of file1.txt ...
						 * --BbC04y
						 * Content-disposition: attachment; filename="file2.gif"
						 * Content-type: image/gif
						 * Content-Transfer-Encoding: binary
						 *
						 * ...contents of file2.gif...
						 * --BbC04y--
						 * --AaB03x--
						 */
					}
					break;
				case HEAD_COOKIE:
					if (session->req.dynamic == IS_SSJS) {
						char *key;
						char *val;

						p = value;
						while ((key = strtok_r(p, "=", &last)) != NULL) {
							while (IS_WHITESPACE(*key))
								key++;
							p = NULL;
							if ((val = strtok_r(p, ";\t\n\v\f\r ", &last)) != NULL) {  /* Whitespace */
								js_add_cookieval(session, key, val);
							}
						}
					}
					break;
				default:
					break;
			}
		}
	}
	return true;
}

static int get_version(char *p)
{
	int i;
	if (p == NULL)
		return 0;
	while (*p && *p < ' ') p++;
	if (*p == 0)
		return 0;
	for (i = 1; http_vers[i] != NULL; i++) {
		if (!stricmp(p, http_vers[i])) {
			return i;
		}
	}
	return i - 1;
}

static int is_dynamic_req(http_session_t* session)
{
	int  i = 0;
	char drive[4];
	char cgidrive[4];
	char dir[MAX_PATH + 1];
	char cgidir[MAX_PATH + 1];
	char fname[MAX_PATH + 1];
	char ext[MAX_PATH + 1];

	if (session->parsed_vpath == PARSED_VPATH_FULL)
		return IS_STATIC;

	check_extra_path(session);
	_splitpath(session->req.physical_path, drive, dir, fname, ext);

	if (!(startup->options & WEB_OPT_NO_CGI)) {
		if (session->req.fastcgi_socket) {
			init_enviro(session);
			return IS_FASTCGI;
		}
	}

	if (stricmp(ext, startup->ssjs_ext) == 0)
		i = IS_SSJS;
	else if (get_xjs_handler(ext, session))
		i = IS_SSJS;
	if (!(startup->options & BBS_OPT_NO_JAVASCRIPT) && i)  {
		lprintf(LOG_DEBUG, "%04d %-5s [%s] Setting up JavaScript support", session->socket, session->client.protocol, session->host_ip);
		if (!js_setup(session)) {
			send_error(session, __LINE__, error_500);
			return IS_STATIC;
		}
		return i;
	}

	if (!(startup->options & WEB_OPT_NO_CGI)) {
		for (i = 0; startup->cgi_ext != NULL && startup->cgi_ext[i] != NULL; i++)  {
			if (stricmp(ext, startup->cgi_ext[i]) == 0)  {
				init_enviro(session);
				return IS_CGI;
			}
		}
		_splitpath(session->req.cgi_dir?session->req.cgi_dir:cgi_dir, cgidrive, cgidir, fname, ext);
		if (stricmp(dir, cgidir) == 0 && stricmp(drive, cgidrive) == 0)  {
			init_enviro(session);
			return IS_CGI;
		}
	}

	return IS_STATIC;
}

static char * split_port_part(char *host)
{
	char *ret = NULL;
	char *p = strchr(host, 0) - 1;

	if (IS_DIGIT(*p)) {
		/*
		 * If the first and last : are not the same, and it doesn't
		 * start with '[', there's no port part.
		 */
		if (host[0] != '[') {
			if (strchr(host, ':') != strrchr(host, ':'))
				return NULL;
		}
		for (; p >= host; p--) {
			if (*p == ':') {
				*p = 0;
				ret = p + 1;
				break;
			}
			if (!IS_DIGIT(*p))
				break;
		}
	}
	// Now, remove []s...
	if (host[0] == '[') {
		memmove(host, host + 1, strlen(host));
		p = strchr(host, ']');
		if (p)
			*p = 0;
	}
	return ret;
}

static void remove_port_part(char *host)
{
	split_port_part(host);
}

static char *get_request(http_session_t * session, char *req_line)
{
	char*       p;
	char*       query;
	char*       retval;
	char*       last;
	int         offset;
	const char* scheme = NULL;
	size_t      scheme_len;

	SKIP_WHITESPACE(req_line);
	SAFECOPY(session->req.virtual_path, req_line);
	if (strtok_r(session->req.virtual_path, " \t", &last))
		retval = strtok_r(NULL, " \t", &last);
	else
		retval = NULL;
	SAFECOPY(session->req.request_line, session->req.virtual_path);
	if (!session->req.orig_request_line[0])
		SAFECOPY(session->req.orig_request_line, session->req.virtual_path);
	if (strtok_r(session->req.virtual_path, "?", &last))
		query = strtok_r(NULL, "", &last);
	else
		query = NULL;

	/* Must initialize physical_path before calling is_dynamic_req() */
	SAFECOPY(session->req.physical_path, session->req.virtual_path);
	hex_decode('%', session->req.physical_path);

	if (strnicmp(session->req.physical_path, http_scheme, http_scheme_len) == 0) {
		scheme = http_scheme;
		scheme_len = http_scheme_len;
	}
	else if (strnicmp(session->req.physical_path, https_scheme, https_scheme_len) == 0) {
		scheme = https_scheme;
		scheme_len = https_scheme_len;
	}
	if (scheme != NULL) {
		/* Remove http:// from start of physical_path */
		memmove(session->req.physical_path, session->req.physical_path + scheme_len, strlen(session->req.physical_path + scheme_len) + 1);

		/* Set HOST value... ignore HOST header */
		SAFECOPY(session->req.host, session->req.physical_path);

		/* Remove path if present (everything after the first /) */
		strtok_r(session->req.host, "/", &last);

		/* Lower-case the host */
		strlwr(session->req.host);

		/* Set vhost value to host value */
		SAFECOPY(session->req.vhost, session->req.host);

		/* Remove port specification from vhost (if present) */
		remove_port_part(session->req.vhost);

		p = strchr(session->req.physical_path, '/');

		/*
		 * If we have a slash, make it the first char in the string.
		 * otherwise, set path to "/"
		 */
		if (p == NULL) {
			strcpy(session->req.physical_path, "/");
		}
		else {
			offset = p - session->req.physical_path;
			memmove(session->req.physical_path
			        , session->req.physical_path + offset
			        , strlen(session->req.physical_path + offset) + 1 /* move '\0' terminator too */
			        );
		}
	}
	if (query != NULL)
		SAFECOPY(session->req.query_str, query);

	return retval;
}

static char *get_method(http_session_t * session, char *req_line)
{
	int i;

	for (i = 0; methods[i] != NULL; i++) {
		if (!strnicmp(req_line, methods[i], strlen(methods[i]))) {
			session->req.method = i;
			if (strlen(req_line) < strlen(methods[i]) + 2) {
				send_error(session, __LINE__, "400 Bad Request");
				return NULL;
			}
			return req_line + strlen(methods[i]) + 1;
		}
	}
	if (req_line != NULL && *req_line >= ' ')
		send_error(session, __LINE__, "501 Not Implemented");
	return NULL;
}

static bool get_request_headers(http_session_t * session)
{
	char  head_line[MAX_REQUEST_LINE + 1];
	char  next_char;
	char *value;
	char *last;
	int   i;

	while (sockreadline(session, head_line, sizeof(head_line) - 1) > 0) {
		/* Multi-line headers */
		while ((i = sess_recv(session, &next_char, 1, MSG_PEEK))
		       && (next_char == '\t' || next_char == ' ')) {
			if (i == -1 && (session->is_tls || SOCKET_ERRNO != EAGAIN))
				close_session_socket(session);
			i = strlen(head_line);
			if (i > (int)sizeof(head_line) - 1) {
				errprintf(LOG_ERR, WHERE, "%04d !ERROR long multi-line header. The web server is broken!", session->socket);
				i = sizeof(head_line) / 2;
				break;
			}
			sockreadline(session, head_line + i, sizeof(head_line) - i - 1);
		}
		strListPush(&session->req.headers, head_line);

		if ((strtok_r(head_line, ":", &last)) != NULL && (value = strtok_r(NULL, "", &last)) != NULL) {
			i = get_header_type(head_line);
			while (*value && *value <= ' ') value++;
			switch (i) {
				case HEAD_HOST:
					if (session->req.host[0] == 0) {
						lprintf(LOG_DEBUG, "%04d %-5s [%s] Requested Host: %s", session->socket, session->client.protocol, session->host_ip, value);
						/* Lower-case for normalization */
						strlwr(value);
						SAFECOPY(session->req.host, value);
						SAFECOPY(session->req.vhost, value);
						/* Remove port part of host (Win32 doesn't allow : in dir names) */
						/* Either an existing : will be replaced with a null, or nothing */
						/* Will happen... the return value is not relevent here */
						remove_port_part(session->req.vhost);
					}
					break;
				default:
					break;
			}
		}
	}

	if (!(session->req.vhost[0])) {
		SAFECOPY(session->req.vhost, server_host_name());
		/* Lower-case for normalization */
		strlwr(session->req.vhost);
	}
	if (!(session->req.host[0])) {
		SAFECOPY(session->req.host, server_host_name());
		/* Lower-case for normalization */
		strlwr(session->req.host);
	}
	return true;
}

static enum parsed_vpath resolve_vpath(http_session_t* session, char* vpath)
{
	char* filename = NULL;
	enum parsed_vpath result = parse_vpath(&scfg, vpath + strlen(startup->file_vpath_prefix), &session->libnum, &session->file.dir, &filename);
	if (result != PARSED_VPATH_FULL)
		return result;
	char path[MAX_PATH + 1];
	safe_snprintf(path, sizeof(path), "%s%s", scfg.dir[session->file.dir]->path, filename);
	if (!fexistcase(path))
		return PARSED_VPATH_NONE;
	if (!loadfile(&scfg, session->file.dir, filename, &session->file, file_detail_index, NULL))
		return PARSED_VPATH_NONE;
	strlcpy(vpath, path, MAX_PATH + 1);
	return PARSED_VPATH_FULL;
}

enum get_fullpath {
	FULLPATH_INVALID,
	FULLPATH_VALID,
	FULLPATH_NOEXIST
};
static enum get_fullpath get_fullpath(http_session_t * session)
{
	char str[MAX_PATH + 1];
	bool vhost = false;

	if (alias_list != NULL) {
		for (size_t i = 0; alias_list[i] != NULL; i++) {
			named_string_t* alias = alias_list[i];
			size_t len = strlen(alias->name);
			if (strncmp(session->req.physical_path, alias->name, len) == 0) {
				char extra[MAX_PATH + 1];
				SAFECOPY(extra, session->req.physical_path + len);
				SAFEPRINTF2(session->req.physical_path, "%s%s", alias->value, extra);
				break;
			}
		}
	}

	if (session->req.vhost[0] && startup->options & WEB_OPT_VIRTUAL_HOSTS) {
		safe_snprintf(str, sizeof(str), "%s/%s", root_dir, session->req.vhost);
		if (isdir(str)) {
			vhost = true;
			safe_snprintf(str, sizeof(str), "%s/%s%s", root_dir, session->req.vhost, session->req.physical_path);
		} else
			safe_snprintf(str, sizeof(str), "%s%s", root_dir, session->req.physical_path);
	} else
		safe_snprintf(str, sizeof(str), "%s%s", root_dir, session->req.physical_path);

	if (!(startup->options & WEB_OPT_NO_FILEBASE)
	    && startup->file_vpath_prefix[0] && (vhost == false || startup->file_vpath_for_vhosts == true)
	    && strncmp(session->req.physical_path, startup->file_vpath_prefix, strlen(startup->file_vpath_prefix)) == 0) {
		session->parsed_vpath = resolve_vpath(session, session->req.physical_path);
		switch (session->parsed_vpath) {
			case PARSED_VPATH_NONE:
				return FULLPATH_NOEXIST;
			case PARSED_VPATH_FULL:
				return FULLPATH_VALID;
			default:
				if (getfname(startup->file_index_script) == startup->file_index_script)  { // no path specified
					SAFEPRINTF2(str, "%s%s", scfg.mods_dir, startup->file_index_script);
					if (!scfg.mods_dir[0] || !fexist(str))
						SAFEPRINTF2(str, "%s%s", scfg.exec_dir, startup->file_index_script);
				} else
					SAFECOPY(str, startup->file_index_script);
				break;
		}
	}

	if (FULLPATH(session->req.physical_path, str, sizeof(session->req.physical_path)) == NULL)
		return FULLPATH_INVALID;

	return isabspath(session->req.physical_path) ? FULLPATH_VALID : FULLPATH_INVALID;
}

// Create root-directory aliases from File Areas ... Virtual Shortcut config strings
static void add_vshortcuts()
{
	int count;

	COUNT_LIST_ITEMS(alias_list, count);
	for (int i = 0; i < scfg.total_dirs; ++i) {
		if (scfg.dir[i]->vshortcut[0] == '\0')
			continue;
		named_string_t** np = (named_string_t**)realloc(alias_list, sizeof(named_string_t*) * (count + 2));
		if (np == NULL)
			break;
		alias_list = np;
		if ((alias_list[count] = (named_string_t*)malloc(sizeof(named_string_t))) == NULL)
			break;
		char name[LEN_DIR + 1];
		snprintf(name, sizeof name, "/%s/", scfg.dir[i]->vshortcut);
		if ((alias_list[count]->name = strdup(name)) == NULL)
			break;
		char vpath[LEN_DIR + 1];
		snprintf(vpath, sizeof vpath, "%s%s/%s/"
		         , startup->file_vpath_prefix
		         , scfg.lib[scfg.dir[i]->lib]->vdir
		         , scfg.dir[i]->vdir);
		if ((alias_list[count]->value = strdup(vpath)) == NULL)
			break;
		count++;
	}
	if (alias_list != NULL)
		alias_list[count] = NULL;   /* terminate list */
}

static bool is_legal_host(const char *host, bool strip_port)
{
	char * stripped = NULL;

	if (strip_port) {
		stripped = strdup(host);
		remove_port_part(stripped);
		host = stripped;
	}
	if (host[0] == '-' || host[0] == '.' || host[0] == 0) {
		FREE_AND_NULL(stripped);
		return false;
	}
	if (strspn(host, ":abcdefghijklmnopqrstuvwxyz0123456789-.") != strlen(host)) {
		FREE_AND_NULL(stripped);
		return false;
	}
	FREE_AND_NULL(stripped);
	return true;
}

static bool is_legal_path(const char* path)
{
#ifdef _WIN32   // Fix for Issue 269 (NTFS Alternate Data Stream vulnerability) and other potential unexpected pathname issues on Windows
	if (strchr(path, ':') != NULL)
		return false;
#endif
	return true;
}

static bool get_req(http_session_t * session, char *request_line)
{
	char   req_line[MAX_REQUEST_LINE + 1];
	char * p;
	int    is_redir = 0;
	int    len;

	req_line[0] = 0;
	if (request_line == NULL) {
		/* Eat leading blank lines... as Apache does...
		 * "This is a legacy issue. The CERN webserver required POST data to have an extra
		 * CRLF following it. Thus many clients send an extra CRLF that is not included in the
		 * Content-Length of the request. Apache works around this problem by eating any empty
		 * lines which appear before a request."
		 * http://httpd.apache.org/docs/misc/known_client_problems.html
		 */
		while ((len = sockreadline(session, req_line, sizeof(req_line) - 1)) == 0);
		if (len < 0)
			return false;
		if (req_line[0])
			lprintf(LOG_INFO, "%04d %-5s [%s] Request %u: %s", session->socket, session->client.protocol, session->host_ip, ++session->requests, req_line);
		if (session->req.ld != NULL && session->req.ld->request == NULL)
			/* FREE()d in http_logging_thread() */
			session->req.ld->request = strdup(req_line);
	}
	else {
		lprintf(LOG_DEBUG, "%04d %-5s [%s] Handling Internal Redirect to: %s", session->socket, session->client.protocol, session->host_ip, request_line);
		session->req.extra_path_info[0] = 0;
		SAFECOPY(req_line, request_line);
		is_redir = 1;
	}
	if (req_line[0]) {
		p = NULL;
		p = get_method(session, req_line);
		if (p != NULL) {
			p = get_request(session, p);
			session->http_ver = get_version(p);
			if (session->http_ver >= HTTP_1_1)
				session->req.keep_alive = true;
			if (!is_redir) {
				get_request_headers(session);
			}
			if (!is_legal_host(session->req.host, true)) {
				lprintf(LOG_NOTICE, "%04d %-5s [%s] Illegal host in request: %s"
					, session->socket, session->client.protocol, session->host_ip, session->req.host);
				send_error(session, __LINE__, "400 Bad Request");
				return false;
			}
			if (!is_legal_host(session->req.vhost, false)) {
				lprintf(LOG_NOTICE, "%04d %-5s [%s] Illegal vhost in request: %s"
					, session->socket, session->client.protocol, session->host_ip, session->req.vhost);
				send_error(session, __LINE__, "400 Bad Request");
				return false;
			}
			if (!is_legal_path(session->req.physical_path)) {
				lprintf(LOG_NOTICE, "%04d %-5s [%s] Illegal path in request: %s"
					, session->socket, session->client.protocol, session->host_ip, session->req.physical_path);
				send_error(session, __LINE__, "400 Bad Request");
				return false;
			}
			if (!host_exempt->listed(session->host_ip, session->host_name) && request_rate_limiter->allowRequest(session->host_ip) == false) {
				lprintf(LOG_NOTICE, "%04d %-5s [%s] Too many requests per rate limit (%u over %us)"
					, session->socket, session->client.protocol, session->host_ip, request_rate_limiter->maxRequests, request_rate_limiter->timeWindowSeconds);
				send_error(session, __LINE__, error_429);
				return false;
			}
			enum get_fullpath fullpath_valid = get_fullpath(session);
			if (fullpath_valid != FULLPATH_VALID) {
				send_error(session, __LINE__, fullpath_valid == FULLPATH_NOEXIST ? error_404 : error_500);
				return false;
			}
			if (session->req.ld != NULL && session->req.ld->vhost == NULL)
				/* FREE()d in http_logging_thread() */
				session->req.ld->vhost = strdup(session->req.vhost);
			session->req.dynamic = is_dynamic_req(session);
			add_env(session, "QUERY_STRING", session->req.query_str);

			add_env(session, "REQUEST_METHOD", methods[session->req.method]);
			add_env(session, "SERVER_PROTOCOL", session->http_ver ?
			        http_vers[session->http_ver] : "HTTP/0.9");
			return true;
		}
	}
	session->req.keep_alive = false;
	send_error(session, __LINE__, "400 Bad Request");
	return false;
}

/* This may exist somewhere else - ToDo */
static char *find_last_slash(char *str)
{
#ifdef _WIN32
	char * LastFSlash;
	char * LastBSlash;

	LastFSlash = strrchr(str, '/');
	LastBSlash = strrchr(str, '\\');
	if (LastFSlash == NULL)
		return LastBSlash;
	if (LastBSlash == NULL)
		return LastFSlash;
	if (LastBSlash < LastFSlash)
		return LastFSlash;
	return LastBSlash;
#else
	return strrchr(str, '/');
#endif
}

/* This may exist somewhere else - ToDo */
static char *find_first_slash(char *str)
{
#ifdef _WIN32
	char * FirstFSlash;
	char * FirstBSlash;

	FirstFSlash = strchr(str, '/');
	FirstBSlash = strchr(str, '\\');
	if (FirstFSlash == NULL)
		return FirstBSlash;
	if (FirstBSlash == NULL)
		return FirstFSlash;
	if (FirstBSlash > FirstFSlash)
		return FirstFSlash;
	return FirstBSlash;
#else
	return strchr(str, '/');
#endif
}

static bool check_extra_path(http_session_t * session)
{
	char *       rp_slash;
	char *       vp_slash;
	char         rpath[MAX_PATH + 1];
	char         vpath[MAX_PATH + 1];
	char         epath[MAX_PATH + 1];
	char         str[MAX_PATH + 1];
	struct  stat sb;
	int          i;
	char *       end;

	if (session->req.got_extra_path)
		return true;
	epath[0] = 0;
	epath[1] = 0;
	if (IS_PATH_DELIM(*lastchar(session->req.physical_path)) || stat(session->req.physical_path, &sb) == -1 /* && errno==ENOTDIR */)
	{
		SAFECOPY(vpath, session->req.virtual_path);
		SAFECOPY(rpath, session->req.physical_path);
		while ((vp_slash = find_last_slash(vpath)) != NULL)
		{
			*vp_slash = 0;
			if ((rp_slash = find_last_slash(rpath)) == NULL)
				return false;
			SAFECOPY(str, epath);
			if (*rp_slash)
				sprintf(epath, "%s%s", rp_slash, str);
			*(rp_slash + 1) = 0;

			/* Check if this contains an index */
			end = strchr(rpath, 0);
			if (session->req.path_info_index) {
				if (isdir(rpath) && !isdir(session->req.physical_path)) {
					for (i = 0; startup->index_file_name != NULL && startup->index_file_name[i] != NULL ; i++)  {
						*end = 0;
						SAFECAT(rpath, startup->index_file_name[i]);
						if (!stat(rpath, &sb)) {
							sprintf(vp_slash, "/%s", startup->index_file_name[i]);
							SAFECOPY(session->req.extra_path_info, epath);
							SAFECOPY(session->req.virtual_path, vpath);
							SAFECOPY(session->req.physical_path, rpath);
							session->req.got_extra_path = true;
							return true;
						}
					}
					/* rpath was an existing path and DID NOT contain an index. */
					/* We do not allow scripts to mask existing dirs/files */
					return false;
				}
			}
			else {
				if (isdir(rpath))
					return false;
			}

			if (vp_slash == vpath)
				return false;

			/* Check if this is a script */
			*rp_slash = 0;
			if (vp_slash != vpath) {
				if (stat(rpath, &sb) != -1 && (!(sb.st_mode & S_IFDIR)))
				{
					SAFECOPY(session->req.extra_path_info, epath);
					SAFECOPY(session->req.virtual_path, vpath);
					SAFECOPY(session->req.physical_path, rpath);
					session->req.got_extra_path = true;
					return true;
				}
			}
		}
	}
	return false;
}

static bool exec_js_webctrl(http_session_t* session, const char *name, const char* script, const char *curdir, bool rewrite)  {
	jsval     rval;
	jsval     val;
	JSString* js_str;
	bool      retval = true;
	char      redir_req[MAX_REQUEST_LINE + 1];

	if (!js_setup_cx(session)) {
		errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !ERROR setting up JavaScript context for %s", session->socket, session->client.protocol, session->host_ip, name);
		return false;
	}

	JS_BEGINREQUEST(session->js_cx);
	js_add_request_prop(session, "scheme", session->is_tls ? "https" : "http");
	js_add_request_prop(session, "real_path", session->req.physical_path);
	js_add_request_prop(session, "virtual_path", session->req.virtual_path);
	js_add_request_prop(session, "ars", session->req.ars);
	if (rewrite)
		js_add_request_prop_writeable(session, "request_string", session->req.request_line);
	else
		js_add_request_prop(session, "request_string", session->req.request_line);
	js_add_request_prop(session, "host", session->req.host);
	js_add_request_prop(session, "vhost", session->req.vhost);
	js_add_request_prop(session, "http_ver", http_vers[session->http_ver]);
	js_add_request_prop(session, "remote_ip", session->host_ip);
	js_add_request_prop(session, "remote_host", session->host_name);
	if (session->req.query_str[0]) {
		js_add_request_prop(session, "query_string", session->req.query_str);
		js_parse_query(session, session->req.query_str);
	}
	if (session->req.post_data && session->req.post_data[0]) {
		if (session->req.post_len <= MAX_POST_LEN) {
			js_add_request_property(session, "post_data", session->req.post_data, session->req.post_len, /*writeable: */ false);
			js_parse_query(session, session->req.post_data);
		}
	}
	if (JS_GetProperty(session->js_cx, session->js_glob, "js", &val)
	    && JSVAL_IS_OBJECT(val) && !JSVAL_NULL_OR_VOID(val)) {
		if ((js_str = JS_NewStringCopyZ(session->js_cx, curdir)) != NULL) {
			JS_DefineProperty(session->js_cx, JSVAL_TO_OBJECT(val), "startup_dir", STRING_TO_JSVAL(js_str)
			                  , NULL, NULL, JSPROP_ENUMERATE);
			JS_DefineProperty(session->js_cx, JSVAL_TO_OBJECT(val), "exec_dir", STRING_TO_JSVAL(js_str)
			                  , NULL, NULL, JSPROP_ENUMERATE);
		}
	}
	parse_js_headers(session);

	do {
		/* RUN SCRIPT */
		JS_ClearPendingException(session->js_cx);

		session->js_callback.counter = 0;

		lprintf(LOG_DEBUG, "%04d %-5s [%s] JavaScript: Compiling %s", session->socket, session->client.protocol, session->host_ip, name);
		if (!JS_EvaluateScript(session->js_cx, session->js_glob, script, strlen(script), name, 1, &rval)) {
			lprintf(LOG_WARNING, "%04d %-5s [%s] !JavaScript FAILED to compile rewrite %s:%s"
			        , session->socket, session->client.protocol, session->host_ip, name, script);
			JS_ReportPendingException(session->js_cx);
			JS_RemoveObjectRoot(session->js_cx, &session->js_glob);
			JS_ENDREQUEST(session->js_cx);
			return false;
		}
		JS_ReportPendingException(session->js_cx);
		js_EvalOnExit(session->js_cx, session->js_glob, &session->js_callback);
		JS_ReportPendingException(session->js_cx);
		if (rewrite && JSVAL_IS_BOOLEAN(rval) && JSVAL_TO_BOOLEAN(rval)) {
			session->req.send_location = MOVED_STAT;
			if (JS_GetProperty(session->js_cx, session->js_request, "request_string", &val)) {
				JSVALUE_TO_STRBUF(session->js_cx, val, redir_req, sizeof(redir_req), NULL);
				safe_snprintf(session->redir_req, sizeof(session->redir_req), "%s %s%s%s", methods[session->req.method]
				              , redir_req, session->http_ver < HTTP_1_0?"":" ", http_vers[session->http_ver]);
			}
		}
		JS_RemoveObjectRoot(session->js_cx, &session->js_glob);
	} while (0);

	JS_ENDREQUEST(session->js_cx);

	return retval;
}

static void read_webctrl_section(FILE *file, char *section, http_session_t *session, char *curdir, bool *recheck_dynamic)
{
	int              i;
	char             str[MAX_PATH + 1];
	named_string_t **values;
	char *           p;

	p = iniReadExistingString(file, section, "AccessRequirements", session->req.ars, str);
	/*
	 * If p == NULL, the key doesn't exist, retain default
	 * If p == default, zero-length string present, truncate req.ars
	 * Otherwise, p is new value and is updated
	 */
	if (p == session->req.ars)
		session->req.ars[0] = 0;
	else if (p != NULL)
		SAFECOPY(session->req.ars, p);
	if (iniReadString(file, section, "Realm", scfg.sys_name, str) == str) {
		FREE_AND_NULL(session->req.realm);
		/* FREE()d in close_request() */
		session->req.realm = strdup(str);
	}
	if (iniReadString(file, section, "DigestRealm", scfg.sys_name, str) == str) {
		FREE_AND_NULL(session->req.digest_realm);
		/* FREE()d in close_request() */
		session->req.digest_realm = strdup(str);
	}
	if (iniReadString(file, section, "ErrorDirectory", error_dir, str) == str) {
		prep_dir(root_dir, str, sizeof(str));
		FREE_AND_NULL(session->req.error_dir);
		/* FREE()d in close_request() */
		session->req.error_dir = strdup(str);
	}
	if (iniReadString(file, section, "CGIDirectory", cgi_dir, str) == str) {
		prep_dir(root_dir, str, sizeof(str));
		FREE_AND_NULL(session->req.cgi_dir);
		/* FREE()d in close_request() */
		session->req.cgi_dir = strdup(str);
		*recheck_dynamic = true;
	}
	if (iniReadString(file, section, "Authentication", default_auth_list, str) == str) {
		FREE_AND_NULL(session->req.auth_list);
		/* FREE()d in close_request() */
		session->req.auth_list = strdup(str);
	}
	if ((session->req.path_info_index = iniReadBool(file, section, "PathInfoIndex", false)))
		check_extra_path(session);
	if (iniReadString(file, section, "FastCGISocket", "", str) == str) {
		FREE_AND_NULL(session->req.fastcgi_socket);
		session->req.fastcgi_socket = strdup(str);
		*recheck_dynamic = true;
	}
	if (iniReadString(file, section, "JSPreExec", "", str) == str) {
		exec_js_webctrl(session, "JSPreExec", str, curdir, true);
	}
	values = iniReadNamedStringList(file, section);
	for (i = 0; values && values[i]; i++) {
		if (strnicmp(values[i]->name, "Rewrite", 7) == 0)
			exec_js_webctrl(session, values[i]->name, values[i]->value, curdir, true);
	}
	iniFreeNamedStringList(values);
}

static bool check_request(http_session_t * session)
{
	char        path[MAX_PATH + 1];
	char        curdir[MAX_PATH + 1];
	char        str[MAX_PATH + 1];      /* Apr-7-2013: bounds of str can be exceeded, e.g. "s:\sbbs\web\root\http:\vert.synchro.net\todolist.ssjs\todolist.ssjs\todolist.ssjs\todolist.ssjs\todolist.ssjs\todolist.ssjs\todolist.ssjs\todolist.ssjs\todolist.ssjs\todolist.ssjs\todolist.ssjs\todolist.ssjs\todolist.ssjs\todolist.ssjs\todolist.ssjs\webctrl.ini"	char [261] */
	char        last_ch;
	char*       last_slash;
	char*       p;
	FILE*       file;
	int         i;
	struct stat sb;
	int         send404 = 0;
	char        filename[MAX_PATH + 1];
	char *      spec;
	str_list_t  specs;
	bool        recheck_dynamic = false;
	char *      spath, *sp, *nsp, *pspec;
	size_t      len;
	bool        old_path_info_index;

	if (session->req.finished)
		return false;
	if (session->req.upgrading)
		return true;

	SAFECOPY(path, session->req.physical_path);
	if (startup->options & WEB_OPT_DEBUG_TX)
		lprintf(LOG_DEBUG, "%04d %-5s [%s] Path is: %s", session->socket, session->client.protocol, session->host_ip, path);
	if (isdir(path)) {
		last_ch = *lastchar(path);
		if (!IS_PATH_DELIM(last_ch))  {
			session->req.send_location = MOVED_PERM;
			SAFECAT(path, "/");
			SAFECAT(session->req.physical_path, "/");
		}
		last_ch = *lastchar(session->req.virtual_path);
		if (!IS_PATH_DELIM(last_ch))  {
			session->req.send_location = MOVED_PERM;
			SAFECAT(session->req.virtual_path, "/");
		}
		last_slash = find_last_slash(path);
		if (last_slash == NULL) {
			send_error(session, __LINE__, error_500);
			return false;
		}
		last_slash++;
		for (i = 0; startup->index_file_name != NULL && startup->index_file_name[i] != NULL ; i++)  {
			*last_slash = 0;
			SAFECAT(path, startup->index_file_name[i]);
			if (startup->options & WEB_OPT_DEBUG_TX)
				lprintf(LOG_DEBUG, "%04d %-5s [%s] Checking for %s", session->socket, session->client.protocol, session->host_ip, path);
			if (!stat(path, &sb))
				break;
			SAFECOPY(path, session->req.physical_path);
		}

		/* Don't send 404 unless authourized... prevent info leak */
		if (startup->index_file_name == NULL || startup->index_file_name[i] == NULL)
			send404 = 1;
		else {
			SAFECAT(session->req.virtual_path, startup->index_file_name[i]);
			if (session->req.send_location != MOVED_PERM)
				session->req.send_location = MOVED_STAT;
		}
		filename[0] = 0;
	}
	else {
		last_slash = find_last_slash(path);
		if (last_slash == NULL)
			last_slash = path;
		else
			last_slash++;
		SAFECOPY(filename, last_slash);
	}

	/* Set default ARS to a 0-length string */
	session->req.ars[0] = 0;

	if (session->parsed_vpath == PARSED_VPATH_NONE) {

		if (strnicmp(path, root_dir, strlen(root_dir))) {
			session->req.keep_alive = false;
			send_error(session, __LINE__, "400 Bad Request");
			SAFEPRINTF2(str, "Request for '%s' is outside of web root: %s", path, root_dir);
			lprintf(LOG_NOTICE, "%04d %-5s [%s] !ERROR %s", session->socket, session->client.protocol, session->host_ip, str);
			hacklog(&scfg, &mqtt, session->client.protocol, session->username, str, session->client.host, &session->addr);
#ifdef _WIN32
			if (startup->sound.hack[0] && !sound_muted(&scfg))
				PlaySound(startup->sound.hack, NULL, SND_ASYNC | SND_FILENAME);
#endif
			return false;
		}

		/* Walk up from root_dir checking for webctrl.ini */
		SAFECOPY(curdir, path);
		last_slash = curdir + strlen(root_dir) - 1;
		/* Loop while there's more /s in path*/
		p = last_slash;

		while ((last_slash = find_first_slash(p + 1)) != NULL) {
			old_path_info_index = session->req.path_info_index;
			p = last_slash;
			/* Terminate the path after the slash */
			*(last_slash + 1) = 0;
			SAFEPRINTF(str, "%swebctrl.ini", curdir);
			/* NEVER serve up a webctrl.ini file */
			if (!strcmp(path, str)) {
				send_error(session, __LINE__, "403 Forbidden");
				return false;
			}
			if (!stat(str, &sb)) {
				/* Read webctrl.ini file */
				if ((file = fopen(str, "r")) != NULL) {
					/* FREE()d in this block */
					specs = iniReadSectionList(file, NULL);
					/* Read in globals */
					read_webctrl_section(file, NULL, session, curdir, &recheck_dynamic);
					/* Now, PathInfoIndex may have been set, so we need to re-expand the index so it will match here. */
					if (old_path_info_index != session->req.path_info_index) {
						// Now that we may have gotten a new filename, we need to use that to compare with.
						SAFECOPY(filename, getfname(session->req.physical_path));
					}
					/* Read in per-filespec */
					while ((spec = strListPop(&specs)) != NULL) {
						len = strlen(spec);
						if (spec[0] && IS_PATH_DELIM(spec[len - 1])) {
							/* Search for matching path elements... */
							spath = strdup(path + (p - curdir + 1));
							pspec = strdup(spec);
							pspec[len - 1] = 0;
							for (sp = spath, nsp = find_first_slash(sp + 1); nsp; nsp = find_first_slash(sp + 1)) {
								*nsp = 0;
								nsp++;
								if (wildmatch(sp, pspec, true, /* case_sensitive: */ true)) {
									read_webctrl_section(file, spec, session, curdir, &recheck_dynamic);
								}
								sp = nsp;
							}
							free(spath);
							free(pspec);
						}
						else if (wildmatch(filename, spec, true, /* case_sensitive: */ true)) {
							read_webctrl_section(file, spec, session, curdir, &recheck_dynamic);
						}
						free(spec);
					}
					iniFreeStringList(specs);
					fclose(file);
					if (session->req.path_info_index)
						recheck_dynamic = true;
				}
				else  {
					/* If cannot open webctrl.ini, only allow sysop access */
					SAFECOPY(session->req.ars, "LEVEL 90");
					break;
				}
				/* Truncate at \r or \n - can use last_slash since I'm done with it.*/
				truncsp(session->req.ars);
			}
			SAFECOPY(curdir, path);
		}

		if (recheck_dynamic) {
			session->req.dynamic = is_dynamic_req(session);
			if (session->req.dynamic)    /* Need to re-copy path here in case of re-checked PathInfoIndex change */
				SAFECOPY(path, session->req.physical_path);
		}

		if (!session->req.dynamic && session->req.extra_path_info[0])
			send404 = true;
	}
	if (!check_ars(session)) {
		unsigned *auth_list;
		unsigned  auth_list_len;

		/* No authentication provided */
		strcpy(str, "401 Unauthorized");
		auth_list = parseEnumList(session->req.auth_list?session->req.auth_list:default_auth_list, ",", (str_list_t)auth_type_names, &auth_list_len);
		for (i = 0; ((unsigned)i) < auth_list_len; i++) {
			p = strchr(str, 0);
			switch (auth_list[i]) {
				case AUTHENTICATION_BASIC:
					snprintf(p, sizeof(str) - (p - str), "%s%s: Basic realm=\"%s\""
					         , newline, get_header(HEAD_WWWAUTH), session->req.realm?session->req.realm:scfg.sys_name);
					str[sizeof(str) - 1] = 0;
					break;
				case AUTHENTICATION_DIGEST:
					snprintf(p, sizeof(str) - (p - str), "%s%s: Digest realm=\"%s\", nonce=\"%s@%u\", qop=\"auth\"%s"
					         , newline, get_header(HEAD_WWWAUTH), session->req.digest_realm?session->req.digest_realm:(session->req.realm?session->req.realm:scfg.sys_name), session->client.addr, (unsigned)time(NULL), session->req.auth.stale?", stale=true":"");
					str[sizeof(str) - 1] = 0;
					break;
			}
		}
		if (auth_list)
			free(auth_list);
		send_error(session, __LINE__, str);
		return false;
	}

	if (session->req.send_location >= MOVED_TEMP && session->redir_req[0])
		return true;
	if (stat(path, &sb) || IS_PATH_DELIM(*(lastchar(path))) || send404) {
		/* OPTIONS requests never return 404 errors (ala Apache) */
		if (session->req.method != HTTP_OPTIONS) {
			if (startup->options & WEB_OPT_DEBUG_TX)
				lprintf(LOG_DEBUG, "%04d %-5s [%s] 404 - %s does not exist", session->socket, session->client.protocol, session->host_ip, path);
			SAFECAT(session->req.physical_path, session->req.extra_path_info);
			SAFECAT(session->req.virtual_path, session->req.extra_path_info);
			send_error(session, __LINE__, error_404);
			return false;
		}
	}
	if (session->req.got_range) {
		if (session->req.range_start < 0)
			session->req.range_start = sb.st_size + session->req.range_start;
		if (session->req.range_end < 0)
			session->req.range_end = sb.st_size + session->req.range_end;
		if (session->req.range_end >= sb.st_size)
			session->req.range_end = sb.st_size - 1;
		if (session->req.range_end < session->req.range_start || session->req.dynamic) {
			send_error(session, __LINE__, error_416);
			return false;
		}
		if (session->req.range_start < 0 || session->req.range_end < 0) {
			send_error(session, __LINE__, error_416);
			return false;
		}
		if (session->req.range_start >= sb.st_size) {
			send_error(session, __LINE__, error_416);
			return false;
		}
		SAFECOPY(session->req.status, "206 Partial Content");
	}
	SAFECOPY(session->req.physical_path, path);
	add_env(session, "SCRIPT_NAME", session->req.virtual_path);
	add_env(session, "SCRIPT_FILENAME", session->req.physical_path);
	SAFECOPY(str, session->req.virtual_path);
	last_slash = find_last_slash(str);
	if (last_slash != NULL)
		*(last_slash + 1) = 0;
	if (*(session->req.extra_path_info))
	{
		sprintf(str, "%s%s", startup->root_dir, session->req.extra_path_info);
		add_env(session, "PATH_TRANSLATED", str);
		add_env(session, "PATH_INFO", session->req.extra_path_info);
	}

	return true;
}

static str_list_t get_cgi_env(http_session_t *session)
{
	char       value[INI_MAX_VALUE_LEN + 1];
	char*      deflt;
	char       defltbuf[INI_MAX_VALUE_LEN + 1];
	char       append[INI_MAX_VALUE_LEN + 1];
	char       prepend[INI_MAX_VALUE_LEN + 1];
	char       env_str[(INI_MAX_VALUE_LEN * 4) + 2];
	size_t     i;
	str_list_t env_list;
	str_list_t add_list;

	/* Return value */
	if ((env_list = strListInit()) == NULL)
		return NULL;

	strListAppendList(&env_list, session->req.cgi_env);

	/* FREE()d in this block */
	if ((add_list = iniGetSectionList(cgi_env, NULL)) != NULL) {

		for (i = 0; add_list[i] != NULL; i++) {
			if ((deflt = getenv(add_list[i])) == NULL)
				deflt = iniGetString(cgi_env, add_list[i], "default", NULL, defltbuf);
			if (iniGetString(cgi_env, add_list[i], "value", deflt, value) == NULL)
				continue;
			iniGetString(cgi_env, add_list[i], "append", "", append);
			iniGetString(cgi_env, add_list[i], "prepend", "", prepend);
			safe_snprintf(env_str, sizeof(env_str), "%s=%s%s%s"
			              , add_list[i], prepend, value, append);
			strListPush(&env_list, env_str);
		}
		iniFreeStringList(add_list);
	}

	return env_list;
}

static SOCKET fastcgi_connect(const char *orig_path, SOCKET client_sock)
{
	int    result;
	char * path = strdup(orig_path);
	ulong  val;
	SOCKET sock = INVALID_SOCKET;

	if (*path == '/' ||  *path == '.' || strncmp(path, "unix:", 5) == 0) {
#if defined(_WIN32) && !defined(UDS_SUPPORT)
		lprintf(LOG_ERR, "%04d UNIX DOMAIN SOCKETS ARE NOT SUPPORTED in %s", client_sock, __FUNCTION__);
		free(path);
		return INVALID_SOCKET;
#else
		// UNIX-domain socket
		struct sockaddr_un addr;
		socklen_t          addr_len;
		sock = socket(AF_UNIX, SOCK_STREAM, 0);
		if (sock == INVALID_SOCKET) {
			lprintf(LOG_ERR, "%04d ERROR creating UNIX domain FastCGI socket", client_sock);
			free(path);
			return sock;
		}

		addr.sun_family = AF_UNIX;
		if (*path == '/' || *path == '.')
			SAFECOPY(addr.sun_path, path);
		else
			SAFECOPY(addr.sun_path, path + 5);
#ifdef SUN_LEN
		addr_len = SUN_LEN(&addr);
#else
		addr_len = sizeof(addr);
#endif
		if (connect(sock, (struct sockaddr*)&addr, addr_len) != 0) {
			lprintf(LOG_ERR, "%04d ERROR %d connecting to UNIX domain FastCGI socket: %s"
			        , client_sock, SOCKET_ERRNO, addr.sun_path);
			closesocket(sock);
			sock = INVALID_SOCKET;
		}
#endif
	} else {
		// TCP Socket
		char *          port = split_port_part(path);
		struct addrinfo hints, *res, *cur;

		memset(&hints, 0, sizeof(hints));
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_ADDRCONFIG;
		result = getaddrinfo(path, port, &hints, &res);
		if (result != 0) {
			errprintf(LOG_ERR, WHERE, "%04d ERROR resolving FastCGI address %s port %s", client_sock, path, port);
			free(path);
			return INVALID_SOCKET;
		}
		for (cur = res, result = 1; result && cur; cur = cur->ai_next) {
			sock = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
			if (sock == INVALID_SOCKET)
				continue;
			val = 1;
			ioctlsocket(sock, FIONBIO, &val);
			result = connect(sock, cur->ai_addr, cur->ai_addrlen);

			if (result == SOCKET_ERROR) {
				if ((SOCKET_ERRNO == EWOULDBLOCK || SOCKET_ERRNO == EINPROGRESS)) {
					if (socket_writable(sock, 1000 /* TODO: Make configurable! */))
						result = 0; /* success */
					else
						closesocket(sock);
				}
				else
					closesocket(sock);
			}
			if (result == 0)
				break;
		}

		freeaddrinfo(res);
	}
	if (sock == INVALID_SOCKET) {
		errprintf(LOG_ERR, WHERE, "%04d ERROR unable to make FastCGI connection to %s", client_sock, orig_path);
		free(path);
		return sock;
	}

	val = 0;
	ioctlsocket(sock, FIONBIO, &val);
	free(path);
	return sock;
}

static void fastcgi_init_header(struct fastcgi_header *head, uint8_t type)
{
	head->ver = FCGI_VERSION_1;
	head->type = type;
	head->id = htons(1);
	head->len = 0;
	head->padlen = 0;
	head->reserved = 0;
}

static bool fastcgi_add_param(struct fastcgi_message **msg, size_t *end, size_t *size, const char *env)
{
	const char * sep;
	void *   p;
	size_t   namelen, vallen, new_len;
	size_t   need_bytes;
	uint32_t l;

	sep = strchr(env, '=');
	if (sep == NULL)
		return false;
	namelen = (sep - env);
	vallen = strlen(sep + 1);
	need_bytes = namelen + vallen;

	if (namelen > 127)
		need_bytes += 4;
	else
		need_bytes++;

	if (vallen > 127)
		need_bytes += 4;
	else
		need_bytes++;

	new_len = *end + need_bytes;
	if (new_len > *size) {
		// Realloc
		while (new_len > *size)
			*size *= 2;
		p = realloc(*msg, *size + sizeof(struct fastcgi_header));
		if (p == NULL)
			return false;
		*msg = static_cast<fastcgi_message *>(p);
	}
	if (namelen > 127) {
		l = htonl(namelen | 0x80000000);
		memcpy((*msg)->body + *end, &l, 4);
		*end += 4;
	}
	else {
		(*msg)->body[(*end)++] = (char)namelen;
	}
	if (vallen > 127) {
		l = htonl(vallen | 0x80000000);
		memcpy((*msg)->body + *end, &l, 4);
		*end += 4;
	}
	else {
		(*msg)->body[(*end)++] = (char)vallen;
	}
	memcpy((*msg)->body + *end, env, namelen);
	*end += namelen;
	memcpy((*msg)->body + *end, sep + 1, vallen);
	*end += vallen;

	return true;
}

static bool fastcgi_send_params(SOCKET sock, http_session_t *session)
{
	int                     i;
	size_t                  end = 0;
	size_t                  size = 1024 + sizeof(struct fastcgi_header);
	struct fastcgi_message *msg = (struct fastcgi_message *)malloc(size + sizeof(struct fastcgi_header));

	if (msg == NULL)
		return false;
	fastcgi_init_header(&msg->head, FCGI_PARAMS);
	str_list_t env = get_cgi_env(session);
	for (i = 0; env[i]; i++) {
		if (!fastcgi_add_param(&msg, &end, &size, env[i])) {
			free(msg);
			strListFree(&env);
			return false;
		}
		if (end > 32000) {
			msg->head.len = htons((uint16_t)end);
			if (sendsocket(sock, (const char *)msg, sizeof(struct fastcgi_header) + end) != (ssize_t)(sizeof(struct fastcgi_header) + end)) {
				errprintf(LOG_ERR, WHERE, "%04d ERROR sending FastCGI params", session->socket);
				free(msg);
				strListFree(&env);
				return false;
			}
			end = 0;
		}
	}
	strListFree(&env);
	if (end) {
		msg->head.len = htons((uint16_t)end);
		if (sendsocket(sock, (const char *)msg, sizeof(struct fastcgi_header) + end) != (ssize_t)(sizeof(struct fastcgi_header) + end)) {
			errprintf(LOG_ERR, WHERE, "%04d ERROR sending FastCGI params", session->socket);
			free(msg);
			return false;
		}
		end = 0;
	}
	msg->head.len = htons((uint16_t)end);
	if (sendsocket(sock, (const char *)msg, sizeof(struct fastcgi_header) + end) != (ssize_t)(sizeof(struct fastcgi_header) + end)) {
		errprintf(LOG_ERR, WHERE, "%04d ERROR sending FastCGI params", session->socket);
		free(msg);
		return false;
	}
	free(msg);
	return true;
}

#define CGI_OUTPUT_READY        (1 << 0)
#define CGI_ERROR_READY         (1 << 1)
#define CGI_PROCESS_TERMINATED  (1 << 2)
#define CGI_INPUT_READY         (1 << 3)

struct fastcgi_body {
	uint16_t len;
	char data[];
};

struct fastcgi_data {
	SOCKET sock;
	struct fastcgi_header header;
	struct fastcgi_body *body;
	size_t used;
	int request_ended;
};

static struct fastcgi_body * fastcgi_read_body(SOCKET sock)
{
	char                  padding[255];
	struct fastcgi_header header {};
	struct fastcgi_body * body;

	if (recv(sock, (char*)&header.len
	         , sizeof(header) - offsetof(struct fastcgi_header, len), MSG_WAITALL)
	    != sizeof(header) - offsetof(struct fastcgi_header, len)) {
		errprintf(LOG_ERR, WHERE, "Error reading FastCGI message header");
		return NULL;
	}
	body = (struct fastcgi_body *)malloc(offsetof(struct fastcgi_body, data) + htons(header.len));
	if (!body) {
		errprintf(LOG_ERR, WHERE, "Error allocating %zu bytes for FastCGI message", offsetof(struct fastcgi_body, data) + htons(header.len));
		return NULL;
	}
	body->len = htons(header.len);
	if (recv(sock, body->data, body->len, MSG_WAITALL) != body->len) {
		free(body);
		errprintf(LOG_ERR, WHERE, "Error reading FastCGI message");
		return NULL;
	}
	if (recv(sock, padding, header.padlen, MSG_WAITALL) != header.padlen) {
		free(body);
		errprintf(LOG_ERR, WHERE, "Error reading FastCGI padding");
		return NULL;
	}
	return body;
}

static int fastcgi_read_wait_timeout(void *arg)
{
	int                  ret = 0;
	struct fastcgi_data *cd = (struct fastcgi_data *)arg;
	struct fastcgi_body *body;

	if (cd->request_ended)
		return CGI_PROCESS_TERMINATED;
	switch (cd->header.type) {
		case FCGI_STDOUT:
			return CGI_OUTPUT_READY;
			break;
		case FCGI_STDERR:
			return CGI_ERROR_READY;
			break;
	}

	if (socket_readable(cd->sock, startup->max_cgi_inactivity * 1000)) {
		if (recv(cd->sock, (char *)&cd->header, offsetof(struct fastcgi_header, len), MSG_WAITALL) != offsetof(struct fastcgi_header, len)) {
			errprintf(LOG_ERR, WHERE, "FastCGI failed to read header");
			return ret;
		}
		if (cd->header.ver != FCGI_VERSION_1) {
			errprintf(LOG_ERR, WHERE, "Unknown FastCGI version %d", cd->header.ver);
			return ret;
		}
		if (htons(cd->header.id) != 1) {
			errprintf(LOG_ERR, WHERE, "Unknown FastCGI session ID %d", htons(cd->header.id));
			return ret;
		}
		switch (cd->header.type) {
			case FCGI_STDOUT:
				ret |= CGI_OUTPUT_READY;
				break;
			case FCGI_STDERR:
				ret |= CGI_OUTPUT_READY;
				break;
			case FCGI_END_REQUEST:
				ret |= CGI_PROCESS_TERMINATED;
				cd->request_ended = 1;
			// Fall-through
			case FCGI_BEGIN_REQUEST:
			case FCGI_ABORT_REQUEST:
			case FCGI_PARAMS:
			case FCGI_STDIN:
			case FCGI_DATA:
			case FCGI_GET_VALUES:
			case FCGI_GET_VALUES_RESULT:
			case FCGI_UNKNOWN_TYPE:
				// Read and discard the entire message...
				body = fastcgi_read_body(cd->sock);
				if (body == NULL)
					return ret;
				free(body);
				break;
			default:
				errprintf(LOG_ERR, WHERE, "Unhandled FastCGI message type %d", cd->header.type);
				// Read and discard the entire message...
				body = fastcgi_read_body(cd->sock);
				if (body == NULL)
					return ret;
				free(body);
				break;
		}
	}
	else
		ret |= CGI_PROCESS_TERMINATED;

	return ret;
}

static int fastcgi_read(void *arg, char *buf, size_t sz)
{
	struct fastcgi_data *cd = (struct fastcgi_data *)arg;

	if (cd->request_ended)
		return -1;
	if (cd->body == NULL) {
		if (cd->header.type != 0)
			cd->body = fastcgi_read_body(cd->sock);
		if (cd->body == NULL)
			return -1;
	}

	if (sz > (cd->body->len - cd->used))
		sz = cd->body->len - cd->used;

	memcpy(buf, cd->body->data + cd->used, sz);
	cd->used += sz;
	if (cd->used >= cd->body->len) {
		FREE_AND_NULL(cd->body);
		cd->header.type = 0;
		cd->used = 0;
	}
	return sz;
}

/*
 * This one is extra tricky since it may need multiple messages to fill...
 * and those messages may not follow each other in the stream.
 * For now, we just hack and hope.
 */
static int fastcgi_readln_out(void *arg, char *buf, size_t bufsz, char *fbuf, size_t fbufsz)
{
	size_t               inpos, outpos;
	struct fastcgi_data *cd = (struct fastcgi_data *)arg;

	outpos = 0;

	if (cd->request_ended)
		return -1;
	if (cd->body == NULL) {
		if (cd->header.type != 0)
			cd->body = fastcgi_read_body(cd->sock);
		if (cd->body == NULL)
			return -1;
	}

	for (outpos = 0, inpos = cd->used; inpos < cd->body->len && outpos < bufsz; inpos++) {
		if (cd->body->data[inpos] == '\n') {
			inpos++;
			break;
		}
		buf[outpos++] = cd->body->data[inpos];
	}
	if (outpos > 0 && buf[outpos - 1] == '\r')
		outpos--;
	// Terminate... even if we need to truncate.
	if (outpos >= bufsz)
		outpos--;
	buf[outpos] = 0;

	cd->used = inpos;

	if (cd->used >= cd->body->len) {
		FREE_AND_NULL(cd->body);
		cd->header.type = 0;
		cd->used = 0;
	}
	return outpos;
}

static int fastcgi_write_in(void *arg, char *buf, size_t bufsz)
{
	struct fastcgi_header head;
	struct fastcgi_data * cd = (struct fastcgi_data *)arg;
	size_t                pos;
	size_t                chunk_size;

	fastcgi_init_header(&head, FCGI_STDIN);
	for (pos = 0; pos < bufsz;) {
		chunk_size = bufsz - pos;
		if (chunk_size > UINT16_MAX)
			chunk_size = UINT16_MAX;
		head.len = htons((uint16_t)chunk_size);
		if (sendsocket(cd->sock, (const char *)&head, sizeof(head)) != sizeof(head))
			return -1;
		if (sendsocket(cd->sock, buf + pos, chunk_size) != (ssize_t)chunk_size)
			return -1;
		pos += chunk_size;
	}
	return bufsz;
}

static int fastcgi_done_wait(void *arg)
{
	struct fastcgi_data *cd = (struct fastcgi_data *)arg;

	if (cd->request_ended)
		return 1;
	return socket_recvdone(cd->sock, 0);
}

#ifdef __unix__
struct cgi_data {
	pid_t child;    // child
	struct pollfd fds[2];
};

static int cgi_read_wait_timeout(void *arg)
{
	int              ret = 0;
	int              status = 0;
	struct cgi_data *cd = (struct cgi_data *)arg;

	cd->fds[0].events = POLLIN;
	cd->fds[1].events = POLLIN;

	if (poll(cd->fds, 2, startup->max_cgi_inactivity * 1000) > 0)  {
		if (cd->fds[0].revents)
			ret |= CGI_OUTPUT_READY;
		if (cd->fds[1].revents)
			ret |= CGI_ERROR_READY;
	}

	if (waitpid(cd->child, &status, WNOHANG) == cd->child)
		ret |= CGI_PROCESS_TERMINATED;
	return ret;
}

static int cgi_read_out(void *arg, char *buf, size_t sz)
{
	struct cgi_data *cd = (struct cgi_data *)arg;

	return read(cd->fds[0].fd, buf, sz);
}

static int cgi_read_err(void *arg, char *buf, size_t sz)
{
	struct cgi_data *cd = (struct cgi_data *)arg;

	return read(cd->fds[1].fd, buf, sz);
}

static int cgi_readln_out(void *arg, char *buf, size_t bufsz, char *fbuf, size_t fbufsz)
{
	struct cgi_data *cd = (struct cgi_data *)arg;

	return pipereadline(cd->fds[0].fd, buf, bufsz, fbuf, fbufsz);
}

static int cgi_write_in(void *arg, char *buf, size_t bufsz)
{
	// *nix doesn't have an input pipe
	return 0;
}

static int cgi_done_wait(void *arg)
{
	int              status = 0;
	struct cgi_data *cd = (struct cgi_data *)arg;

	return waitpid(cd->child, &status, WNOHANG) == cd->child;
}
#else
struct cgi_data {
	HANDLE rdpipe;
	HANDLE wrpipe;
	HANDLE child;
	http_session_t *session;
};

static int cgi_read_wait_timeout(void *arg)
{
	int              ret = 0;
	bool             rd;
	struct cgi_data *cd = (struct cgi_data *)arg;

	DWORD            waiting;
	time_t           end = time(NULL) + startup->max_cgi_inactivity;

	while (ret == 0) {
		if (WaitForSingleObject(cd->child, 0) == WAIT_OBJECT_0)
			ret |= CGI_PROCESS_TERMINATED;
		waiting = 0;
		PeekNamedPipe(
			cd->rdpipe,         /* handle to pipe to copy from */
			NULL,               /* pointer to data buffer */
			0,                  /* size, in bytes, of data buffer */
			NULL,               /* pointer to number of bytes read */
			&waiting,           /* pointer to total number of bytes available */
			NULL                /* pointer to unread bytes in this message */
			);
		if (waiting)
			ret |= CGI_OUTPUT_READY;
		if (!session_check(cd->session, &rd, NULL, /* timeout: */ 0))
			ret |= CGI_INPUT_READY;
		if (rd)
			ret |= CGI_INPUT_READY;
		if (time(NULL) >= end)
			break;
		if (ret == 0)
			Sleep(1);
	}
	return ret;
}

static int cgi_read_out(void *arg, char *buf, size_t sz)
{
	DWORD            msglen = 0;
	struct cgi_data *cd = (struct cgi_data *)arg;

	if (ReadFile(cd->rdpipe, buf, sz, &msglen, NULL) == false) {
		errprintf(LOG_ERR, WHERE, "%04d !ERROR %d reading from pipe"
		          , cd->session->socket, GetLastError());
		return -1;
	}

	return msglen;
}

static int cgi_read_err(void *arg, char *buf, size_t sz)
{
	// Win32 doesn't have an error pipe
	return 0;
}

static int cgi_readln_out(void *arg, char *buf, size_t bufsz, char *fbuf, size_t fbufsz)
{
	struct cgi_data *cd = (struct cgi_data *)arg;

	return pipereadline(cd->rdpipe, buf, bufsz, NULL, 0);
}

static int cgi_write_in(void *arg, char *buf, size_t bufsz)
{
	DWORD wr;
	struct cgi_data *cd = (struct cgi_data *)arg;

	(void)WriteFile(cd->wrpipe, buf, bufsz, &wr, /* Overlapped: */ NULL);
	return wr;
}

static int cgi_done_wait(void *arg)
{
	struct cgi_data *cd = (struct cgi_data *)arg;

	return WaitForSingleObject(cd->child, 0) == WAIT_OBJECT_0;
}
#endif

struct cgi_api {
	int (*read_wait_timeout)(void *arg);
	int (*read_out)(void *arg, char *buf, size_t sz);
	int (*read_err)(void *arg, char *buf, size_t sz);
	int (*readln_out)(void *arg, char *buf, size_t bufsz, char *fbuf, size_t fbufsz);
	int (*write_in)(void *arg, char *buf, size_t bufsz);
	int (*done_wait)(void *arg);
	void *arg;
};

/*
 * Need to return:
 * Success/fail
 * Timeout out or not
 * Done parsing headers or not
 * Got valid headers or not
 * Process exited or not.
 */

static int do_cgi_stuff(http_session_t *session, struct cgi_api *cgi, bool orig_keep)
{
	int ret = 0;
#define CGI_STUFF_FAILED            (1 << 0)
#define CGI_STUFF_TIMEDOUT          (1 << 1)
#define CGI_STUFF_DONE_PARSING      (1 << 2)
#define CGI_STUFF_VALID_HEADERS     (1 << 3)
#define CGI_STUFF_PROCESS_EXITED    (1 << 4)
	int        ready;
	int        i;
	char       cgi_status[MAX_REQUEST_LINE + 1];
	char       header[MAX_REQUEST_LINE + 1];
	char       buf[1024];
	char       fbuf[1026];
	char *     directive = NULL;
	char *     value = NULL;
	char *     last;
	bool       done_reading = false;
	bool       done_wait = false;
	bool       no_chunked = false;
	bool       set_chunked = false;
	time_t     start;
	str_list_t tmpbuf;

	start = time(NULL);

	/* ToDo: Magically set done_parsing_headers for nph-* scripts */
	cgi_status[0] = 0;
	/* FREE()d following this block */
	tmpbuf = strListInit();
	while (!done_reading) {
		ready = cgi->read_wait_timeout(cgi->arg);
		if (ready)  {
			if (ready & CGI_OUTPUT_READY) {
				if ((ret & CGI_STUFF_DONE_PARSING) && (ret & CGI_STUFF_VALID_HEADERS))  {
					i = cgi->read_out(cgi->arg, buf, sizeof(buf));
					if (i != -1 && i != 0)  {
						int snt = 0;
						start = time(NULL);
						snt = writebuf(session, buf, i);
						if (session->req.ld != NULL)
							session->req.ld->size += snt;
					}
					else
						done_reading = true;
				}
				else  {
					/* This is the tricky part */
					i = cgi->readln_out(cgi->arg, buf, sizeof(buf), fbuf, sizeof(fbuf));
					if (i == -1) {
						done_reading = true;
						ret |= CGI_STUFF_VALID_HEADERS;
					}
					else
						start = time(NULL);

					if (!(ret & CGI_STUFF_DONE_PARSING) && *buf)  {
						if (tmpbuf != NULL)
							strListPush(&tmpbuf, fbuf);
						SAFECOPY(header, buf);
						directive = strtok_r(header, ":", &last);
						if (directive != NULL)  {
							value = strtok_r(NULL, "", &last);
							if (value != NULL) {
								SKIP_WHITESPACE(value);
								i = get_header_type(directive);
								switch (i)  {
									case HEAD_LOCATION:
										ret |= CGI_STUFF_VALID_HEADERS;
										if (*value == '/')  {
											unescape(value);
											SAFECOPY(session->req.virtual_path, value);
											session->req.send_location = MOVED_STAT;
											if (cgi_status[0] == 0)
												SAFECOPY(cgi_status, error_302);
										} else  {
											SAFECOPY(session->req.virtual_path, value);
											session->req.send_location = MOVED_TEMP;
											if (cgi_status[0] == 0)
												SAFECOPY(cgi_status, error_302);
										}
										break;
									case HEAD_STATUS:
										SAFECOPY(cgi_status, value);
										/*
										 * 1xx, 204, and 304 responses don't have bodies, so don't
										 * need a Location or Content-Type header to be valid.
										 */
										if (value[0] == '1' || ((value[0] == '2' || value[0] == '3') && value[1] == '0' && value[2] == '4'))
											ret |= CGI_STUFF_VALID_HEADERS;
										break;
									case HEAD_LENGTH:
										session->req.keep_alive = orig_keep;
										strListPush(&session->req.dynamic_heads, buf);
										no_chunked = true;
										break;
									case HEAD_TYPE:
										ret |= CGI_STUFF_VALID_HEADERS;
										strListPush(&session->req.dynamic_heads, buf);
										break;
									case HEAD_TRANSFER_ENCODING:
										no_chunked = true;
										break;
									default:
										strListPush(&session->req.dynamic_heads, buf);
								}
							}
						}
						if (directive == NULL || value == NULL) {
							/* Invalid header line */
							ret |= CGI_STUFF_DONE_PARSING;
						}
					}
					else  {
						if (!no_chunked && session->http_ver >= HTTP_1_1) {
							session->req.keep_alive = orig_keep;
							if (session->req.method != HTTP_HEAD)
								set_chunked = true;
						}
						if (ret & CGI_STUFF_VALID_HEADERS)  {
							session->req.dynamic = IS_CGI;
							if (cgi_status[0] == 0)
								SAFECOPY(cgi_status, session->req.status);
							send_headers(session, cgi_status, set_chunked);
						}
						else {
							/* Invalid headers... send 'er all as plain-text */
							char content_type[MAX_REQUEST_LINE + 1];
							int  snt;

							lprintf(LOG_DEBUG, "%04d %-5s [%s] Received invalid CGI headers, sending result as plain-text"
								, session->socket, session->client.protocol, session->host_ip);

							/* free() the non-headers so they don't get sent, then recreate the list */
							strListFreeStrings(session->req.dynamic_heads);

							/* Copy current status */
							SAFECOPY(cgi_status, session->req.status);

							/* Add the content-type header (REQUIRED) */
							SAFEPRINTF2(content_type, "%s: %s", get_header(HEAD_TYPE), startup->default_cgi_content);
							strListPush(&session->req.dynamic_heads, content_type);
							send_headers(session, cgi_status, false);

							/* Now send the tmpbuf */
							for (i = 0; tmpbuf != NULL && tmpbuf[i] != NULL; i++) {
								if (strlen(tmpbuf[i]) > 0) {
									snt = writebuf(session, tmpbuf[i], strlen(tmpbuf[i]));
									if (session->req.ld != NULL) {
										session->req.ld->size += snt;
									}
								}
							}
							if (strlen(fbuf) > 0) {
								snt = writebuf(session, fbuf, strlen(fbuf));
								if (session->req.ld != NULL && snt > 0) {
									session->req.ld->size += snt;
								}
							}
							ret |= CGI_STUFF_VALID_HEADERS;
						}
						ret |= CGI_STUFF_DONE_PARSING;
					}
				}
			}
			if (ready & CGI_ERROR_READY)  {
				i = cgi->read_err(cgi->arg, buf, sizeof(buf) - 1);
				if (i > 0) {
					buf[i] = 0;
					errprintf(LOG_ERR, WHERE, "%04d CGI Error: %s", session->socket, buf);
					start = time(NULL);
				}
			}
			if (ready & CGI_INPUT_READY) {
				/* Send received POST Data to stdin of CGI process */
				if ((i = sess_recv(session, buf, sizeof(buf), 0)) > 0)  {
					lprintf(LOG_DEBUG, "%04d %-5s [%s] CGI Received %d bytes of POST data"
					        , session->socket, session->client.protocol, session->host_ip, i);
					cgi->write_in(cgi->arg, buf, i);
				}
			}
			if (ready & CGI_PROCESS_TERMINATED) {
				ret |= CGI_STUFF_PROCESS_EXITED;
				done_wait = true;
			}
			if (!done_wait)
				done_wait = cgi->done_wait(cgi->arg);
			if ((!(ready & (CGI_OUTPUT_READY | CGI_ERROR_READY))) && done_wait)
				done_reading = true;
		}
		else  {
			if ((time(NULL) - start) >= startup->max_cgi_inactivity)  {
				errprintf(LOG_ERR, WHERE, "%04d CGI Process %s Timed out", session->socket, getfname(session->req.physical_path));
				done_reading = true;
				start = 0;
				ret |= CGI_STUFF_TIMEDOUT;
			}
		}
	}

	if (tmpbuf != NULL)
		strListFree(&tmpbuf);

	return ret;
}

static bool exec_fastcgi(http_session_t *session)
{
	int                           msglen;
	bool                          orig_keep = false;
	SOCKET                        sock;
	struct fastcgi_message *      msg;
	struct fastcgi_begin_request *br;
	struct fastcgi_data           cd;
	struct cgi_api                cgi{};
	cgi.read_wait_timeout = fastcgi_read_wait_timeout;
	cgi.read_out = fastcgi_read;
	cgi.read_err = fastcgi_read;
	cgi.readln_out = fastcgi_readln_out;
	cgi.write_in = fastcgi_write_in;
	cgi.done_wait = fastcgi_done_wait;
	cgi.arg = &cd;

	lprintf(LOG_DEBUG, "%04d %-5s [%s] Executing FastCGI: %s", session->socket, session->client.protocol, session->host_ip, session->req.physical_path);
	if (session->req.fastcgi_socket == NULL) {
		errprintf(LOG_ERR, WHERE, "%04d No FastCGI socket configured!", session->socket);
		return false;
	}

	orig_keep = session->req.keep_alive;
	session->req.keep_alive = false;

	sock = fastcgi_connect(session->req.fastcgi_socket, session->socket);
	if (sock == INVALID_SOCKET)
		return false;

	// Set up request...
	msglen = sizeof(struct fastcgi_header) + sizeof(struct fastcgi_begin_request);
	msg = (struct fastcgi_message *)malloc(msglen);
	if (msg == NULL) {
		closesocket(sock);
		errprintf(LOG_ERR, WHERE, "%04d Failure to allocate memory for FastCGI message!", session->socket);
		return false;
	}
	fastcgi_init_header(&msg->head, FCGI_BEGIN_REQUEST);
	msg->head.len = htons(sizeof(struct fastcgi_begin_request));
	br = (struct fastcgi_begin_request *)&msg->body;
	br->role = htons(FCGI_RESPONDER);
	br->flags = 0;
	memset(br->reserved, 0, sizeof(br->reserved));
	int result = sendsocket(sock, (const char *)msg, msglen);
	if (result != msglen) {
		errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !ERROR %d sending %d bytes to FastCGI socket (send returned %d)"
		          , session->socket, session->client.protocol, session->host_ip, SOCKET_ERRNO, msglen, result);
		free(msg);
		closesocket(sock);
		return false;
	}
	if (!fastcgi_send_params(sock, session)) {
		free(msg);
		closesocket(sock);
		return false;
	}

	// TODO handle stdin better
	memset(&cd, 0, sizeof(cd));
	cd.sock = sock;
	cd.request_ended = 0;
	fastcgi_write_in(&cd, session->req.post_data, session->req.post_len);
	msg->head.len = 0;
	msg->head.type = FCGI_STDIN;
	if (sendsocket(sock, (const char *)msg, sizeof(struct fastcgi_header)) != sizeof(struct fastcgi_header)) {
		free(msg);
		closesocket(sock);
		lprintf(LOG_WARNING, "%04d %-s [%s] Failure to send stdin to FastCGI socket!"
			, session->socket, session->client.protocol, session->host_ip);
		return false;
	}
	free(msg);

	// Now handle stuff coming back from the FastCGI socket...
	int ret = do_cgi_stuff(session, &cgi, orig_keep);
	FREE_AND_NULL(cd.body);
	closesocket(sock);

	if (!(ret & CGI_STUFF_VALID_HEADERS)) {
		errprintf(LOG_ERR, WHERE, "%04d FastCGI Process did not generate valid headers", session->socket);
		return false;
	}

	if (!(ret & CGI_STUFF_DONE_PARSING)) {
		errprintf(LOG_ERR, WHERE, "%04d FastCGI Process did not send data header termination", session->socket);
		return false;
	}

	return true;
}

static bool exec_cgi(http_session_t *session)
{
	struct cgi_data cd;
	struct cgi_api  cgi{};
	cgi.read_wait_timeout = cgi_read_wait_timeout;
	cgi.read_out = cgi_read_out;
	cgi.read_err = cgi_read_err;
	cgi.readln_out = cgi_readln_out;
	cgi.write_in = cgi_write_in;
	cgi.done_wait = cgi_done_wait;
	cgi.arg = &cd;
#ifdef __unix__
	char            cmdline[MAX_PATH + 256];
	int             i = 0;
	int             status = 0;
	pid_t           child = 0;
	int             in_pipe[2];
	int             out_pipe[2];
	int             err_pipe[2];
	char            buf[1024];
	bool            done_parsing_headers = false;
	bool            done_wait = false;
	bool            got_valid_headers = false;
	time_t          start;
	char            cgipath[MAX_PATH + 1];
	char *          p;
	bool            orig_keep = false;
	char *          handler;
	size_t          sent;

	SAFECOPY(cmdline, session->req.physical_path);

	lprintf(LOG_INFO, "%04d %-5s [%s] Executing CGI: %s", session->socket, session->client.protocol, session->host_ip, cmdline);

	orig_keep = session->req.keep_alive;
	session->req.keep_alive = false;

	/* Set up I/O pipes */

	if (session->tls_sess) {
		if (pipe(in_pipe) != 0) {
			errprintf(LOG_ERR, WHERE, "%04d Can't create in_pipe", session->socket);
			return false;
		}
	}

	if (pipe(out_pipe) != 0) {
		if (session->tls_sess) {
			close(in_pipe[0]);
			close(in_pipe[1]);
		}
		errprintf(LOG_ERR, WHERE, "%04d Can't create out_pipe", session->socket);
		return false;
	}

	if (pipe(err_pipe) != 0) {
		if (session->tls_sess) {
			close(in_pipe[0]);
			close(in_pipe[1]);
		}
		close(out_pipe[0]);
		close(out_pipe[1]);
		errprintf(LOG_ERR, WHERE, "%04d Can't create err_pipe", session->socket);
		return false;
	}

	handler = get_cgi_handler(cmdline);
	if (handler)
		lprintf(LOG_INFO, "%04d %-5s [%s] Using handler %s to execute %s"
			, session->socket, session->client.protocol, session->host_ip, handler, cmdline);

	if ((child = fork()) == 0)  {
		str_list_t env_list;

		/* Do a full suid thing. */
		if (startup->setuid != NULL)
			startup->setuid(true);

		env_list = get_cgi_env(session);
		/* Set up STDIO */
		if (session->tls_sess) {
			dup2(in_pipe[0], 0);     /* stdin */
			close(in_pipe[1]);      /* close write-end of pipe */
		}
		else
			dup2(session->socket, 0);       /* redirect stdin */
		close(out_pipe[0]);     /* close read-end of pipe */
		dup2(out_pipe[1], 1);    /* stdout */
		close(out_pipe[1]);     /* close excess file descriptor */
		close(err_pipe[0]);     /* close read-end of pipe */
		dup2(err_pipe[1], 2);    /* stderr */
		close(err_pipe[1]);     /* close excess file descriptor */

		SAFECOPY(cgipath, cmdline);
		if ((p = strrchr(cgipath, '/')) != NULL)
		{
			*p = 0;
			if (chdir(cgipath) != 0)
				lprintf(LOG_ERR, "%04d %-5s [%s] !ERROR %d changing directory to %s"
				        , session->socket, session->client.protocol, session->host_ip, errno, cgipath);
		}

		/* Execute command */
		if (handler != NULL) {
			char* shell = os_cmdshell();
			execle(shell, shell, "-c", handler, cmdline, NULL, env_list);
		}
		else {
			execle(cmdline, cmdline, NULL, env_list);
		}

		errprintf(LOG_ERR, WHERE, "%04d !ERROR %d executing execle(%s)", session->socket, errno, cmdline);
		exit(EXIT_FAILURE); /* Should never happen */
	}

	if (child == -1)  {
		errprintf(LOG_ERR, WHERE, "%04d !ERROR %d invoking fork()", session->socket, errno);
		if (session->tls_sess)
			close(in_pipe[1]);  /* close write-end of pipe */
		close(out_pipe[0]);     /* close read-end of pipe */
		close(err_pipe[0]);     /* close read-end of pipe */
	}

	if (session->tls_sess)
		close(in_pipe[0]);  /* close excess file descriptor */
	close(out_pipe[1]);     /* close excess file descriptor */
	close(err_pipe[1]);     /* close excess file descriptor */

	if (child == -1)
		return false;

	start = time(NULL);

	// TODO: For TLS-CGI, write each separate read...
	if (session->tls_sess && session->req.post_len && session->req.post_data) {
		sent = 0;
		cd.fds[0].fd = in_pipe[1];
		cd.fds[0].events = POLLOUT;
		while (sent < session->req.post_len) {
			if (poll(cd.fds, 1, 1000) > 0) {
				if (cd.fds[0].revents)
					i = write(in_pipe[1], &session->req.post_data[sent], session->req.post_len - sent);
				else
					i = 0;
				if (i > 0)
					sent += i;
				else {
					lprintf(LOG_INFO, "%04d %-5s [%s] FAILED writing CGI POST data"
						, session->socket, session->client.protocol, session->host_ip);
					close(in_pipe[1]);
					close(out_pipe[0]);
					close(err_pipe[0]);
					return false;
				}
			}
			else {
				lprintf(LOG_INFO, "%04d %-5s [%s] FAILED polling CGI stding for write"
					, session->socket, session->client.protocol, session->host_ip);
				close(in_pipe[1]);
				close(out_pipe[0]);
				close(err_pipe[0]);
				return false;
			}
		}
	}

	cd.fds[0].fd = out_pipe[0];
	cd.fds[1].fd = err_pipe[0];
	cd.child = child;

	int ret = do_cgi_stuff(session, &cgi, orig_keep);
	if (ret & CGI_STUFF_DONE_PARSING)
		done_parsing_headers = true;
	if (ret & CGI_STUFF_PROCESS_EXITED)
		done_wait = true;
	if (ret & CGI_STUFF_TIMEDOUT)
		start = 1;
	if (ret & CGI_STUFF_VALID_HEADERS)
		got_valid_headers = true;

	if (!done_wait)
		done_wait = (waitpid(child, &status, WNOHANG) == child);
	if (!done_wait)  {
		if (start)
			lprintf(LOG_NOTICE, "%04d %-5s [%s] CGI Process %s still alive on client exit"
			        , session->socket, session->client.protocol, session->host_ip, getfname(cmdline));
		kill(child, SIGTERM);
		mswait(1000);
		done_wait = (waitpid(child, &status, WNOHANG) == child);
		if (!done_wait)  {
			kill(child, SIGKILL);
			done_wait = (waitpid(child, &status, 0) == child);
		}
	}

	if (session->tls_sess)
		close(in_pipe[1]);  /* close excess file descriptor */
	/* Drain STDERR & STDOUT */
	while (poll(cd.fds, 2, 1000) > 0) {
		if (cd.fds[1].revents) {
			i = read(err_pipe[0], buf, sizeof(buf) - 1);
			if (i != -1 && i != 0) {
				buf[i] = 0;
				errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !CGI Error: %s", session->socket, session->client.protocol, session->host_ip, buf);
			}
		}

		if (cd.fds[0].revents)  {
			i = read(cd.fds[0].fd, buf, sizeof(buf));
			if (i != -1 && i != 0)  {
				int snt = 0;
				snt = writebuf(session, buf, i);
				if (session->req.ld != NULL)
					session->req.ld->size += snt;
			}
		}

		if (i == 0 || i == -1)
			break;
	}

	close(out_pipe[0]);     /* close read-end of pipe */
	close(err_pipe[0]);     /* close read-end of pipe */
	if (!got_valid_headers) {
		errprintf(LOG_ERR, WHERE, "%04d CGI Process %s did not generate valid headers"
		          , session->socket, getfname(cmdline));
		return false;
	}

	if (!done_parsing_headers) {
		errprintf(LOG_ERR, WHERE, "%04d CGI Process %s did not send data header termination"
		          , session->socket, getfname(cmdline));
		return false;
	}

	return true;
#else
	/* Win32 exec_cgi() */

	/* These are (more or less) copied from the Unix version */
	char*               p;
	char                cmdline[MAX_PATH + 256];
	char                buf[4096];
	bool                orig_keep;
	bool                done_parsing_headers = false;
	bool                got_valid_headers = false;
	char *              directive = NULL;
	char *              value = NULL;
	bool                no_chunked = false;
	int                 set_chunked = false;

	/* Win32-specific */
	char*               env_block;
	char                startup_dir[MAX_PATH + 1];
	HANDLE              rdpipe = INVALID_HANDLE_VALUE;
	HANDLE              wrpipe = INVALID_HANDLE_VALUE;
	HANDLE              rdoutpipe;
	HANDLE              wrinpipe;
	DWORD               retval;
	bool                success;
	bool                process_terminated = false;
	PROCESS_INFORMATION process_info;
	SECURITY_ATTRIBUTES sa;
	STARTUPINFO         startup_info {};
	str_list_t          env_list;

	startup_info.cb = sizeof(startup_info);
	startup_info.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	startup_info.wShowWindow = SW_HIDE;

	SAFECOPY(startup_dir, session->req.physical_path);
	if ((p = strrchr(startup_dir, '/')) != NULL || (p = strrchr(startup_dir, '\\')) != NULL)
		*p = 0;
	else
		SAFECOPY(startup_dir, session->req.cgi_dir?session->req.cgi_dir:cgi_dir);

	lprintf(LOG_DEBUG, "%04d %-5s [%s] CGI startup dir: %s"
		, session->socket, session->client.protocol, session->host_ip, startup_dir);

	if ((p = get_cgi_handler(session->req.physical_path)) != NULL)
		SAFEPRINTF2(cmdline, "%s %s", p, session->req.physical_path);
	else
		SAFECOPY(cmdline, session->req.physical_path);

	lprintf(LOG_INFO, "%04d %-5s [%s] Executing CGI: %s"
		, session->socket, session->client.protocol, session->host_ip, cmdline);

	orig_keep = session->req.keep_alive;
	session->req.keep_alive = false;

	memset(&sa, 0, sizeof(sa));
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = true;

	/* Create the child output pipe (override default 4K buffer size) */
	if (!CreatePipe(&rdoutpipe, &startup_info.hStdOutput, &sa, sizeof(buf))) {
		errprintf(LOG_ERR, WHERE, "%04d !ERROR %d creating stdout pipe", session->socket, GetLastError());
		return false;
	}
	startup_info.hStdError = startup_info.hStdOutput;

	/* Create the child input pipe. */
	if (!CreatePipe(&startup_info.hStdInput, &wrinpipe, &sa, 0 /* default buffer size */)) {
		errprintf(LOG_ERR, WHERE, "%04d !ERROR %d creating stdin pipe", session->socket, GetLastError());
		CloseHandle(rdoutpipe);
		return false;
	}

	DuplicateHandle(
		GetCurrentProcess(), rdoutpipe,
		GetCurrentProcess(), &rdpipe, 0, false, DUPLICATE_SAME_ACCESS);

	DuplicateHandle(
		GetCurrentProcess(), wrinpipe,
		GetCurrentProcess(), &wrpipe, 0, false, DUPLICATE_SAME_ACCESS);

	CloseHandle(rdoutpipe);
	CloseHandle(wrinpipe);

	env_list = get_cgi_env(session);
	env_block = strListCreateBlock(env_list);
	strListFree(&env_list);

	success = CreateProcess(
		NULL,           /* pointer to name of executable module */
		cmdline,        /* pointer to command line string */
		NULL,           /* process security attributes */
		NULL,           /* thread security attributes */
		true,           /* handle inheritance flag */
		CREATE_NEW_CONSOLE, /* creation flags */
		env_block,      /* pointer to new environment block */
		startup_dir,    /* pointer to current directory name */
		&startup_info,  /* pointer to STARTUPINFO */
		&process_info   /* pointer to PROCESS_INFORMATION */
		);

	strListFreeBlock(env_block);

	if (!success) {
		errprintf(LOG_ERR, WHERE, "%04d !ERROR %d running %s", session->socket, GetLastError(), cmdline);
		return false;
	}

	cd.wrpipe = wrpipe;
	cd.rdpipe = rdpipe;
	cd.child = process_info.hProcess;
	cd.session = session;

	int ret = do_cgi_stuff(session, &cgi, orig_keep);
	if (ret & CGI_STUFF_DONE_PARSING)
		done_parsing_headers = true;
	if (ret & CGI_STUFF_VALID_HEADERS)
		got_valid_headers = true;

	if (GetExitCodeProcess(process_info.hProcess, &retval) == false)
		errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !ERROR GetExitCodeProcess(%s) returned %d"
		          , session->socket, session->client.protocol, session->host_ip, getfname(cmdline), GetLastError());

	if (retval == STILL_ACTIVE) {
		lprintf(LOG_WARNING, "%04d %-5s [%s] Terminating CGI process: %s"
		        , session->socket, session->client.protocol, session->host_ip, getfname(cmdline));
		TerminateProcess(process_info.hProcess, GetLastError());
	}

	if (rdpipe != INVALID_HANDLE_VALUE)
		CloseHandle(rdpipe);
	if (wrpipe != INVALID_HANDLE_VALUE)
		CloseHandle(wrpipe);
	CloseHandle(process_info.hProcess);

	if (!got_valid_headers)
		lprintf(LOG_WARNING, "%04d %-5s [%s] !CGI Process %s did not generate valid headers"
		        , session->socket, session->client.protocol, session->host_ip, getfname(cmdline));

	if (!done_parsing_headers)
		lprintf(LOG_WARNING, "%04d %-5s [%s] !CGI Process %s did not send data header termination"
		        , session->socket, session->client.protocol, session->host_ip, getfname(cmdline));

	return true;
#endif
}

/********************/
/* JavaScript stuff */
/********************/

JSObject* js_CreateHttpReplyObject(JSContext* cx
                                   , JSObject* parent, http_session_t *session)
{
	JSObject* reply;
	JSObject* headers;
	jsval     val;
	JSString* js_str;

	/* Return existing object if it's already been created */
	if (JS_GetProperty(cx, parent, "http_reply", &val) && !JSVAL_NULL_OR_VOID(val))  {
		reply = JSVAL_TO_OBJECT(val);
		JS_ClearScope(cx, reply);
	}
	else
		reply = JS_DefineObject(cx, parent, "http_reply", NULL
		                        , NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

	if ((js_str = JS_NewStringCopyZ(cx, session->req.status)) == NULL)
		return nullptr;
	JS_DefineProperty(cx, reply, "status", STRING_TO_JSVAL(js_str)
	                  , NULL, NULL, JSPROP_ENUMERATE);

	/* Return existing object if it's already been created */
	if (JS_GetProperty(cx, reply, "header", &val) && !JSVAL_NULL_OR_VOID(val))  {
		headers = JSVAL_TO_OBJECT(val);
		JS_ClearScope(cx, headers);
	}
	else
		headers = JS_DefineObject(cx, reply, "header", NULL
		                          , NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

	if ((js_str = JS_NewStringCopyZ(cx, "text/html")) == NULL)
		return nullptr;
	JS_DefineProperty(cx, headers, "Content-Type", STRING_TO_JSVAL(js_str)
	                  , NULL, NULL, JSPROP_ENUMERATE);

	return reply;
}

JSObject* js_CreateHttpRequestObject(JSContext* cx
                                     , JSObject* parent, http_session_t *session)
{
/*	JSObject*	cookie; */
	jsval val;

	/* Return existing object if it's already been created */
	if (JS_GetProperty(cx, parent, "http_request", &val) && !JSVAL_NULL_OR_VOID(val))  {
		session->js_request = JSVAL_TO_OBJECT(val);
	}
	else
		session->js_request = JS_DefineObject(cx, parent, "http_request", NULL
		                                      , NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

	js_add_request_prop(session, "path_info", session->req.extra_path_info);
	js_add_request_prop(session, "method", methods[session->req.method]);
	js_add_request_prop(session, "virtual_path", session->req.virtual_path);

	/* Return existing object if it's already been created */
	if (JS_GetProperty(cx, session->js_request, "query", &val) && !JSVAL_NULL_OR_VOID(val))  {
		session->js_query = JSVAL_TO_OBJECT(val);
		JS_ClearScope(cx, session->js_query);
	}
	else
		session->js_query = JS_DefineObject(cx, session->js_request, "query", NULL
		                                    , NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

	/* Return existing object if it's already been created */
	if (JS_GetProperty(cx, session->js_request, "header", &val) && !JSVAL_NULL_OR_VOID(val))  {
		session->js_header = JSVAL_TO_OBJECT(val);
		JS_ClearScope(cx, session->js_header);
	}
	else
		session->js_header = JS_DefineObject(cx, session->js_request, "header", NULL
		                                     , NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

	/* Return existing object if it's already been created */
	if (JS_GetProperty(cx, session->js_request, "cookie", &val) && !JSVAL_NULL_OR_VOID(val))  {
		session->js_cookie = JSVAL_TO_OBJECT(val);
		JS_ClearScope(cx, session->js_cookie);
	}
	else
		session->js_cookie = JS_DefineObject(cx, session->js_request, "cookie", NULL
		                                     , NULL, JSPROP_ENUMERATE | JSPROP_READONLY);


	return session->js_request;
}

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	char            line[64];
	char            file[MAX_PATH + 1];
	const char*     warning = "";
	http_session_t* session;
	int             log_level = LOG_ERR;

	if ((session = (http_session_t*)JS_GetContextPrivate(cx)) == NULL)
		return;

	if (report == NULL) {
		lprintf(LOG_ERR, "%04d %-5s [%s] !JavaScript: %s", session->socket, session->client.protocol, session->host_ip, message);
		if (content_file_open(session))
			fprintf(session->req.fp, "!JavaScript: %s", message);
		return;
	}

	if (report->filename)
		SAFEPRINTF(file, " %s", report->filename);
	else
		file[0] = 0;

	if (report->lineno)
		SAFEPRINTF(line, " line %u", report->lineno);
	else
		line[0] = 0;

	if (JSREPORT_IS_WARNING(report->flags)) {
		if (JSREPORT_IS_STRICT(report->flags))
			warning = "strict warning";
		else
			warning = "warning";
		log_level = LOG_WARNING;
	} else if (report->filename != NULL) {
		static pthread_mutex_t mutex;
		static bool            mutex_initialized;
		static char            lastfile[MAX_PATH + 1];
		static uint            lastline;
		if (!mutex_initialized) {
			pthread_mutex_init(&mutex, NULL);
			mutex_initialized = true;
		}
		pthread_mutex_lock(&mutex);
		if (lastline == report->lineno && strcmp(lastfile, report->filename) == 0)
			log_level = LOG_WARNING;
		lastline = report->lineno;
		SAFECOPY(lastfile, report->filename);
		pthread_mutex_unlock(&mutex);
	}

	lprintf(log_level, "%04d %-5s [%s] !JavaScript %s%s%s: %s, Request: %s"
	        , session->socket, session->client.protocol, session->host_ip, warning, file, line, message, session->req.request_line);
	if (content_file_open(session))
		fprintf(session->req.fp, "!JavaScript %s%s%s: %s", warning, file, line, message);
}

static void js_writebuf(http_session_t *session, const char *buf, size_t buflen)
{
	if (session->req.sent_headers) {
		if (session->req.send_content)
			writebuf(session, buf, buflen);
	}
	else if (content_file_open(session))
		fwrite(buf, 1, buflen, session->req.fp);
}

static JSBool
js_writefunc(JSContext *cx, uintN argc, jsval *arglist, bool writeln)
{
	jsval *         argv = JS_ARGV(cx, arglist);
	uintN           i;
	JSString*       str = NULL;
	http_session_t* session;
	jsrefcount      rc;
	char *          cstr = NULL;
	size_t          cstr_sz = 0;
	size_t          len;

	if ((session = (http_session_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	if (!content_file_open(session)) {
		return JS_FALSE;
	}

	if ((!session->req.prev_write) && (!session->req.sent_headers)) {
		if (session->http_ver >= HTTP_1_1 && session->req.keep_alive) {
			rc = JS_SUSPENDREQUEST(cx);
			if (!ssjs_send_headers(session, true)) {
				JS_RESUMEREQUEST(cx, rc);
				return JS_FALSE;
			}
			JS_RESUMEREQUEST(cx, rc);
		}
		else {
			/* "Fast Mode" requested? */
			jsval     val;
			JSObject* reply = NULL;
			if (JS_GetProperty(cx, session->js_glob, "http_reply", &val))
				reply = JSVAL_TO_OBJECT(val);
			if (reply != NULL
			    && JS_GetProperty(cx, reply, "fast", &val)
			    && JSVAL_IS_BOOLEAN(val) && JSVAL_TO_BOOLEAN(val)) {
				session->req.keep_alive = false;
				rc = JS_SUSPENDREQUEST(cx);
				if (!ssjs_send_headers(session, false)) {
					JS_RESUMEREQUEST(cx, rc);
					return JS_FALSE;
				}
				JS_RESUMEREQUEST(cx, rc);
			}
		}
	}

	session->req.prev_write = true;

	for (i = 0; i < argc; i++) {
		if ((str = JS_ValueToString(cx, argv[i])) == NULL)
			continue;
		JSSTRING_TO_RASTRING(cx, str, cstr, &cstr_sz, &len);
		if (cstr) {
			HANDLE_PENDING(cx, cstr);
			rc = JS_SUSPENDREQUEST(cx);
			js_writebuf(session, cstr, len);
			if (writeln)
				js_writebuf(session, newline, 2);
			JS_RESUMEREQUEST(cx, rc);
		}
	}

	if (cstr)
		free(cstr);
	if (str == NULL)
		JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	else
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));

	return JS_TRUE;
}

static JSBool
js_write(JSContext *cx, uintN argc, jsval *arglist)
{
	js_writefunc(cx, argc, arglist, false);

	return JS_TRUE;
}

static JSBool
js_writeln(JSContext *cx, uintN argc, jsval *arglist)
{
	js_writefunc(cx, argc, arglist, true);

	return JS_TRUE;
}

static JSBool
js_set_cookie(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *         argv = JS_ARGV(cx, arglist);
	char            header_buf[8192];
	char *          header;
	char *          p = NULL;
	int32           i;
	JSBool          b;
	struct tm       tm;
	http_session_t* session;
	time_t          tt;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((session = (http_session_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	if (argc < 2)
		return JS_FALSE;

	header = header_buf;
	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL);
	HANDLE_PENDING(cx, p);
	if (!p)
		return JS_FALSE;
	header += sprintf(header, "Set-Cookie: %s=", p);
	FREE_AND_NULL(p);
	JSVALUE_TO_MSTRING(cx, argv[1], p, NULL);
	HANDLE_PENDING(cx, p);
	if (!p)
		return JS_FALSE;
	header += sprintf(header, "%s", p);
	FREE_AND_NULL(p);
	if (argc > 2) {
		if (!JS_ValueToInt32(cx, argv[2], &i))
			return JS_FALSE;
		tt = i;
		if (i && gmtime_r(&tt, &tm) != NULL)
			header += strftime(header, 50, "; expires=%a, %d-%b-%Y %H:%M:%S GMT", &tm);
	}
	if (argc > 3) {
		JSVALUE_TO_MSTRING(cx, argv[3], p, NULL);
		if (p != NULL && *p) {
			header += sprintf(header, "; domain=%s", p);
		}
		FREE_AND_NULL(p);
	}
	if (argc > 4) {
		JSVALUE_TO_MSTRING(cx, argv[4], p, NULL);
		if (p != NULL && *p) {
			header += sprintf(header, "; path=%s", p);
		}
		FREE_AND_NULL(p);
	}
	if (argc > 5) {
		JS_ValueToBoolean(cx, argv[5], &b);
		if (b)
			header += sprintf(header, "; secure");
	}
	strListPush(&session->req.dynamic_heads, header_buf);

	return JS_TRUE;
}

static JSBool
js_log(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *         argv = JS_ARGV(cx, arglist);
	char            str[512];
	uintN           i = 0;
	int32           level = LOG_INFO;
	http_session_t* session;
	jsrefcount      rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((session = (http_session_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	if (startup == NULL || startup->lputs == NULL)
		return JS_FALSE;

	if (argc > 1 && JSVAL_IS_NUMBER(argv[i])) {
		if (!JS_ValueToInt32(cx, argv[i++], &level))
			return JS_FALSE;
	}

	str[0] = 0;
	for (; i < argc && strlen(str) < (sizeof(str) / 2); i++) {
		char* tp = strchr(str, 0);
		JSVALUE_TO_STRBUF(cx, argv[i], tp, sizeof(str) / 2, NULL);
		SAFECAT(str, " ");
	}
	rc = JS_SUSPENDREQUEST(cx);
	lprintf(level, "%04d %-5s [%s] %s", session->socket, session->client.protocol, session->host_ip, str);
	JS_RESUMEREQUEST(cx, rc);

	JSString* js_str = JS_NewStringCopyZ(cx, str);
	if (js_str != NULL)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));

	return JS_TRUE;
}

static JSBool
js_login(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *         argv = JS_ARGV(cx, arglist);
	char*           username;
	char*           password;
	JSBool          inc_logons = JS_FALSE;
	user_t          user;
	http_session_t* session;
	jsrefcount      rc;

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));

	if ((session = (http_session_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	/* User name */
	JSVALUE_TO_ASTRING(cx, argv[0], username, (LEN_ALIAS > LEN_NAME) ? LEN_ALIAS + 2 : LEN_NAME + 2, NULL);
	if (username == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);

	memset(&user, 0, sizeof(user));

	user.number = find_login_id(&scfg, username);

	if (getuserdat(&scfg, &user) != 0) {
		lprintf(LOG_NOTICE, "%04d %-5s [%s] !USER NOT FOUND: '%s'"
		        , session->socket, session->client.protocol, session->host_ip, username);
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}

	if (user.misc & (DELETED | INACTIVE)) {
		lprintf(LOG_WARNING, "%04d %-5s [%s] !DELETED OR INACTIVE USER #%d: %s"
		        , session->socket, session->client.protocol, session->host_ip, user.number, username);
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}

	JS_RESUMEREQUEST(cx, rc);
	/* Password */
	if (user.pass[0]) {
		JSVALUE_TO_ASTRING(cx, argv[1], password, LEN_PASS + 2, NULL);
		if (password == NULL)
			return JS_FALSE;

		if (stricmp(user.pass, password)) { /* Wrong password */
			rc = JS_SUSPENDREQUEST(cx);
			if (scfg.sys_misc & SM_ECHO_PW)
				lprintf(LOG_NOTICE, "%04d %-5s [%s] <%s> !FAILED Password attempt: '%s' expected '%s'"
				        , session->socket, session->client.protocol, session->host_ip, user.alias, password, user.pass);
			else
				lprintf(LOG_NOTICE, "%04d %-5s [%s] <%s> !FAILED Password attempt"
				        , session->socket, session->client.protocol, session->host_ip, user.alias);
			badlogin(session->socket, username, password, &session->client, &session->addr);
			JS_RESUMEREQUEST(cx, rc);
			return JS_TRUE;
		}
	}

	if (!chk_ars(&scfg, startup->login_ars, &user, &session->client)) {
		lprintf(LOG_NOTICE, "%04d %-5s [%s] <%s> !Insufficient server access: %s"
		        , session->socket, session->client.protocol, session->host_ip, user.alias, startup->login_ars);
		badlogin(session->socket, username, NULL, &session->client, &session->addr);
		return JS_TRUE;
	}

	if (argc > 2)
		JS_ValueToBoolean(cx, argv[2], &inc_logons);

	rc = JS_SUSPENDREQUEST(cx);

	if (inc_logons) {
		user.logons++;
		user.ltoday++;
	}

	http_logon(session, &user);

	JS_RESUMEREQUEST(cx, rc);

	/* user-specific objects */
	if (!js_CreateUserObjects(session->js_cx, session->js_glob, &scfg, &session->user, &session->client
	                          , startup->file_vpath_prefix, session->subscan /* subscan */, &mqtt)) {
		errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !JavaScript ERROR creating user objects"
			, session->socket, session->client.protocol, session->host_ip);
		send_error(session, __LINE__, "500 Error initializing JavaScript User Objects");
		return false;
	}

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_TRUE));

#ifdef _WIN32
	if (startup->sound.login[0] && !sound_muted(&scfg))
		PlaySound(startup->sound.login, NULL, SND_ASYNC | SND_FILENAME);
#endif

	return JS_TRUE;
}

#if 0
static char *find_next_pair(char *buffer, size_t buflen, char find)
{
	char * p;
	char * search;
	char * end;
	size_t buflen2;
	char   chars[5] = "@%^<";

	end = buffer + buflen;
	search = buffer;
	buflen2 = buflen;

	for (; search < end;) {
		p = memchr(search, chars[i], buflen2);
		/* Can't even find one... there's definatly no pair */
		if (p == NULL)
			return NULL;

		if (*(p + 1) == find)
			return p;

		/* Next search pos is at the char after the match */
		search = p + 1;
		buflen2 = end - search;
	}
}

static void js_write_escaped(JSContext *cx, JSObject *obj, char *pos, size_t len, char *name_end, char *repeat_section)
{
	char *name = pos + 2;

}

enum {
	T_AT
	, T_PERCENT
	, T_CARET
	, T_LT
};

static int js_write_template_part(JSContext *cx, JSObject *obj, char *template, size_t len, char *repeat_section)
{
	size_t len2;
	char * pos;
	char * end;
	char * p;
	char * p2;
	char * send_end;
	int    no_more[4];
	char * next[4];
	int    i, j;
	char   chars[5] = "@%^<";

	end = template + len;
	pos = template;
	memset(&next, 0, sizeof(next));
	memset(&no_more, 0, sizeof(no_more));

	while (pos < end) {
		send_end = NULL;

		/* Find next seperator */
		for (i = 0; i < 4; i++) {
			if (!no_more[i]) {
				if (next[i] < pos)
					next[i] = NULL;
				if (next[i] == NULL) {
					if ((next[i] = find_next_pair(pos, len, chars[i])) == NULL) {
						no_more[i] = true;
						continue;
					}
				}
				if (!send_end || next[i] < send_end)
					send_end = next[i];
			}
		}
		if (send_end == NULL) {
			/* Nothing else matched... we're good now! */
			js_writebuf(session, pos, len);
			pos = end;
			len = 0;
			continue;
		}
		if (send_end > pos) {
			i = send_end - pos;
			js_writebuf(session, pos, i);
			pos += i;
			len -= i;
		}

		/*
		 * At this point, pos points to a matched introducer.
		 * If it's not a repeat section, we can just output it here.
		 */
		if (*pos != '<') {
			/*
			 * If there is no corresponding terminator to this introdcer,
			 * force it to be output unchanged.
			 */
			if ((p = find_next_pair(pos, len, *pos)) == NULL) {
				no_more[strchr(chars, *pos) - char] = true;
				continue;
			}
			js_write_escaped(cx, obj, pos, len, p, repeat_section);
			continue;
		}

		/*
		 * Pos is the start of a repeat section now... this is where things
		 * start to get tricky.  Set up RepeatObj object, then call self
		 * once for each repeat.
		 */
	}
}

static JSBool
js_write_template(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *      obj = JS_THIS_OBJECT(cx, arglist);
	jsval *         argv = JS_ARGV(cx, arglist);
	JSString*       js_str;
	char *          filename;
	char *          template;
	FILE *          tfile;
	size_t          len;
	http_session_t* session;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((session = (http_session_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	if (session->req.fp == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], filename, NULL);
	if (filename == NULL)
		return JS_FALSE;

	if (!fexist(filename)) {
		free(filename);
		JS_ReportError(cx, "Template file %s does not exist.", filename);
		return JS_FALSE;
	}
	len = flength(filename);

	if ((tfile = fopen(filename, "r")) == NULL) {
		free(filename);
		JS_ReportError(cx, "Unable to open template %s for read.", filename);
		return JS_FALSE;
	}
	free(filename);

	if ((template = (char *)malloc(len)) == NULL) {
		JS_ReportError(cx, "Unable to allocate %u bytes for template.", len);
		return JS_FALSE;
	}

	if (fread(template, 1, len, tfile) != len) {
		free(template);
		fclose(tfile);
		JS_ReportError(cx, "Unable to read %u bytes from template %s.", len, filename);
		return JS_FALSE;
	}
	fclose(tfile);

	if ((!session->req.prev_write) && (!session->req.sent_headers)) {
		if (session->http_ver >= HTTP_1_1 && session->req.keep_alive) {
			if (!ssjs_send_headers(session, true)) {
				free(template);
				return JS_FALSE;
			}
		}
		else {
			/* "Fast Mode" requested? */
			jsval     val;
			JSObject* reply = NULL;
			if (JS_GetProperty(cx, session->js_glob, "http_reply", &val))
				reply = JSVAL_TO_OBJECT(val);
			if (relpy != NULL && JS_GetProperty(cx, reply, "fast", &val)
			    && JSVAL_IS_BOOLEAN(val) && JSVAL_TO_BOOLEAN(val)) {
				session->req.keep_alive = false;
				if (!ssjs_send_headers(session, false)) {
					free(template);
					return JS_FALSE;
				}
			}
		}
	}

	session->req.prev_write = true;
	js_write_template_part(cx, obj, template, len, NULL);
	free(template);

	return JS_TRUE;
}
#endif

static JSFunctionSpec js_global_functions[] = {
	{"write",           js_write,           1},     /* write to HTML file */
	{"writeln",         js_writeln,         1},     /* write line to HTML file */
	{"print",           js_writeln,         1},     /* write line to HTML file (alias) */
	{"log",             js_log,             0},     /* Log a string */
	{"login",           js_login,           2},     /* log in as a different user */
	{"set_cookie",      js_set_cookie,      2},     /* Set a cookie */
	{0}
};

static JSBool
js_OperationCallback(JSContext *cx)
{
	JSBool          ret;
	http_session_t* session;

	JS_SetOperationCallback(cx, NULL);
	if ((session = (http_session_t*)JS_GetContextPrivate(cx)) == NULL) {
		JS_SetOperationCallback(cx, js_OperationCallback);
		return JS_FALSE;
	}

	ret = js_CommonOperationCallback(cx, &session->js_callback);
	JS_SetOperationCallback(cx, js_OperationCallback);

	return ret;
}

static JSContext*
js_initcx(http_session_t *session)
{
	JSContext* js_cx;

	if ((js_cx = JS_NewContext(session->js_runtime, JAVASCRIPT_CONTEXT_STACK)) == NULL) {
		errprintf(LOG_CRIT, WHERE, "%04d %-5s [%s] JavaScript: Failed to create new context"
			, session->socket, session->client.protocol, session->host_ip);
		return NULL;
	}
	JS_SetOptions(js_cx, startup->js.options);
	JS_BEGINREQUEST(js_cx);

	lprintf(LOG_DEBUG, "%04d %-5s [%s] JavaScript: Context created with options: %lx"
	        , session->socket, session->client.protocol, session->host_ip, (long)startup->js.options);

	JS_SetErrorReporter(js_cx, js_ErrorReporter);

	JS_SetOperationCallback(js_cx, js_OperationCallback);

	lprintf(LOG_DEBUG, "%04d %-5s [%s] JavaScript: Creating Global Objects and Classes"
		, session->socket, session->client.protocol, session->host_ip);
	if (!js_CreateCommonObjects(js_cx, &scfg, NULL
	                            , NULL                          /* global */
	                            , uptime                        /* system */
	                            , server_host_name()            /* system */
	                            , SOCKLIB_DESC                  /* system */
	                            , &session->js_callback         /* js */
	                            , &startup->js                  /* js */
		                        , NULL                          /* user */
	                            , &session->client              /* client */
	                            , session->socket               /* client */
	                            , session->is_tls ? session->tls_sess : -1      /* client */
	                            , &js_server_props              /* server */
	                            , &session->js_glob
	                            , &mqtt
	                            )
	    || !JS_DefineFunctions(js_cx, session->js_glob, js_global_functions)) {
		errprintf(LOG_CRIT, WHERE, "%04d %-5s [%s] JavaScript: Failed to create global objects and classes"
			, session->socket, session->client.protocol, session->host_ip);
		JS_RemoveObjectRoot(js_cx, &session->js_glob);
		JS_ENDREQUEST(js_cx);
		JS_DestroyContext(js_cx);
		return NULL;
	}

	return js_cx;
}

static bool js_setup_cx(http_session_t* session)
{
	JSObject* argv;
	JSString* js_str;

	if (session->js_runtime == NULL) {
		lprintf(LOG_DEBUG, "%04d %-5s [%s] JavaScript: Creating runtime: %u bytes"
		        , session->socket, session->client.protocol, session->host_ip, startup->js.max_bytes);

		if ((session->js_runtime = jsrt_GetNew(startup->js.max_bytes, 5000, __FILE__, __LINE__)) == NULL) {
			errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !ERROR creating JavaScript runtime"
				, session->socket, session->client.protocol, session->host_ip);
			return false;
		}
	}

	if (session->js_cx == NULL) {  /* Context not yet created, create it now */
		/* js_initcx() begins a context */
		if ((session->js_cx = js_initcx(session)) == NULL) {
			lprintf(LOG_WARNING, "%04d %-5s [%s] !ERROR initializing JavaScript context"
				, session->socket, session->client.protocol, session->host_ip);
			return false;
		}
		argv = JS_NewArrayObject(session->js_cx, 0, NULL);

		JS_DefineProperty(session->js_cx, session->js_glob, "argv", OBJECT_TO_JSVAL(argv)
		                  , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);
		JS_DefineProperty(session->js_cx, session->js_glob, "argc", INT_TO_JSVAL(0)
		                  , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);

		if ((js_str = JS_NewStringCopyZ(session->js_cx, root_dir)) != NULL)
			JS_DefineProperty(session->js_cx, session->js_glob, "web_root_dir",
			                  STRING_TO_JSVAL(js_str)
			                  , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);
		if ((js_str = JS_NewStringCopyZ(session->js_cx, session->req.error_dir?session->req.error_dir:error_dir)) != NULL)
			JS_DefineProperty(session->js_cx, session->js_glob, "web_error_dir",
			                  STRING_TO_JSVAL(js_str)
			                  , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);
		JS_ENDREQUEST(session->js_cx);
	}
	else {
		JS_BEGINREQUEST(session->js_cx);
		JS_MaybeGC(session->js_cx);
	}

	lprintf(LOG_DEBUG, "%04d %-5s [%s] JavaScript: Initializing HttpRequest object"
		, session->socket, session->client.protocol, session->host_ip);
	if (js_CreateHttpRequestObject(session->js_cx, session->js_glob, session) == NULL) {
		errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !ERROR initializing JavaScript HttpRequest object"
			, session->socket, session->client.protocol, session->host_ip);
		JS_ENDREQUEST(session->js_cx);
		return false;
	}

	JS_SetContextPrivate(session->js_cx, session);

	return true;
}

static bool js_setup(http_session_t* session)
{
	if (!js_setup_cx(session))
		return false;

	lprintf(LOG_DEBUG, "%04d %-5s [%s] JavaScript: Initializing HttpReply object", session->socket, session->client.protocol, session->host_ip);
	if (js_CreateHttpReplyObject(session->js_cx, session->js_glob, session) == NULL) {
		errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !ERROR initializing JavaScript HttpReply object", session->socket, session->client.protocol, session->host_ip);
		JS_ENDREQUEST(session->js_cx);
		return false;
	}

	return true;
}

static bool ssjs_send_headers(http_session_t* session, int chunked)
{
	jsval      val;
	JSObject*  reply = NULL;
	JSIdArray* heads = NULL;
	JSObject*  headers = NULL;
	int        i, h;
	char       str[MAX_REQUEST_LINE + 1];
	char *     p = NULL, *p2 = NULL;
	size_t     p_sz = 0, p2_sz = 0;

	JS_BEGINREQUEST(session->js_cx);
	if (JS_GetProperty(session->js_cx, session->js_glob, "http_reply", &val)) {
		reply = JSVAL_TO_OBJECT(val);
		if (reply != NULL) {
			if (JS_GetProperty(session->js_cx, reply, "status", &val))
				JSVALUE_TO_STRBUF(session->js_cx, val, session->req.status, sizeof(session->req.status), NULL);
			if (JS_GetProperty(session->js_cx, reply, "header", &val)) {
				headers = JSVAL_TO_OBJECT(val);
				if (headers != NULL)
					heads = JS_Enumerate(session->js_cx, headers);
			}
		}
	}
	if (heads != NULL) {
		for (i = 0; i < heads->length; i++)  {
			JS_IdToValue(session->js_cx, heads->vector[i], &val);
			JSVALUE_TO_RASTRING(session->js_cx, val, p, &p_sz, NULL);
			if (p == NULL) {
				if (p2)
					free(p2);
				JS_DestroyIdArray(session->js_cx, heads);
				return false;
			}
			if (JS_GetProperty(session->js_cx, headers, p, &val))
				JSVALUE_TO_RASTRING(session->js_cx, val, p2, &p2_sz, NULL);
			if (JS_IsExceptionPending(session->js_cx)) {
				if (p)
					free(p);
				if (p2)
					free(p2);
				JS_DestroyIdArray(session->js_cx, heads);
				return false;
			}
			if (p2 != NULL && !session->req.sent_headers) {
				h = get_header_type(p);
				switch (h) {
					case HEAD_LOCATION:
						if (*p2 == '/') {
							unescape(p2);
							SAFECOPY(session->req.virtual_path, p2);
							session->req.send_location = MOVED_STAT;
						}
						else {
							SAFECOPY(session->req.virtual_path, p2);
							session->req.send_location = MOVED_TEMP;
						}
						if (atoi(session->req.status) == 200)
							SAFECOPY(session->req.status, error_302);
						break;
					case HEAD_LENGTH:
					case HEAD_TRANSFER_ENCODING:
						/* If either of these are manually set, point
						 * the gun at the script writers foot for them */
						chunked = false;
						session->req.manual_length = true;
					// fall-through
					default:
						safe_snprintf(str, sizeof(str), "%s: %s", p, p2);
						strListPush(&session->req.dynamic_heads, str);
				}
			}
			else {
				safe_snprintf(str, sizeof(str), "%s: %s", p, p2);
				strListPush(&session->req.dynamic_heads, str);
			}
		}
		if (p)
			free(p);
		if (p2)
			free(p2);
		JS_ClearScope(session->js_cx, headers);
		JS_DestroyIdArray(session->js_cx, heads);
	}
	JS_ENDREQUEST(session->js_cx);
	return send_headers(session, session->req.status, chunked);
}

static bool exec_ssjs(http_session_t* session, char* script)  {
	JSObject*   js_script;
	jsval       rval;
	char        path[MAX_PATH + 1];
	bool        retval = true;
	long double start;

	/* External JavaScript handler? */
	if (script == session->req.physical_path && session->req.xjs_handler[0])
		script = session->req.xjs_handler;

	snprintf(path, sizeof path, "%sSBBS_SSJS.%u.%u.html", scfg.temp_dir, getpid(), session->socket);
	if (session->req.cleanup_file[CLEANUP_SSJS_TMP_FILE]) {
		if (!(startup->options & WEB_OPT_DEBUG_SSJS))
			remove(session->req.cleanup_file[CLEANUP_SSJS_TMP_FILE]);
		free(session->req.cleanup_file[CLEANUP_SSJS_TMP_FILE]);
	}
	/* FREE()d in close_request() */
	session->req.cleanup_file[CLEANUP_SSJS_TMP_FILE] = strdup(path);

	JS_BEGINREQUEST(session->js_cx);
	js_add_request_prop(session, "real_path", session->req.physical_path);
	js_add_request_prop(session, "virtual_path", session->req.virtual_path);
	js_add_request_prop(session, "ars", session->req.ars);
	js_add_request_prop(session, "request_string", session->req.request_line);
	js_add_request_prop(session, "host", session->req.host);
	js_add_request_prop(session, "vhost", session->req.vhost);
	js_add_request_prop(session, "http_ver", http_vers[session->http_ver]);
	js_add_request_prop(session, "remote_ip", session->host_ip);
	js_add_request_prop(session, "remote_host", session->host_name);
	js_add_request_prop(session, "lib", libnum_is_valid(&scfg, session->libnum) ? scfg.lib[session->libnum]->sname : NULL);
	js_add_request_prop(session, "dir", dirnum_is_valid(&scfg, session->file.dir) ? scfg.dir[session->file.dir]->code : NULL);
	if (session->req.query_str[0])  {
		js_add_request_prop(session, "query_string", session->req.query_str);
		js_parse_query(session, session->req.query_str);
	}
	if (session->req.post_data && session->req.post_data[0]) {
		if (session->req.post_len <= MAX_POST_LEN) {
			js_add_request_property(session, "post_data", session->req.post_data, session->req.post_len, /* writeable: */ false);
			js_parse_query(session, session->req.post_data);
		}
	}
	parse_js_headers(session);

	do {
		/* RUN SCRIPT */
		JS_ClearPendingException(session->js_cx);

		session->js_callback.counter = 0;

		lprintf(LOG_DEBUG, "%04d %-5s [%s] JavaScript: Compiling script: %s"
			, session->socket, session->client.protocol, session->host_ip, script);
		if ((js_script = JS_CompileFile(session->js_cx, session->js_glob
		                                , script)) == NULL) {
			errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !JavaScript FAILED to compile script (%s)"
			          , session->socket, session->client.protocol, session->host_ip, script);
			JS_RemoveObjectRoot(session->js_cx, &session->js_glob);
			JS_ENDREQUEST(session->js_cx);
			return false;
		}

		lprintf(LOG_DEBUG, "%04d %-5s [%s] JavaScript: Executing script: %s"
			, session->socket, session->client.protocol, session->host_ip, script);
		start = xp_timer();
		js_PrepareToExecute(session->js_cx, session->js_glob, script, /* startup_dir */ NULL, session->js_glob);
		JS_ExecuteScript(session->js_cx, session->js_glob, js_script, &rval);
		js_EvalOnExit(session->js_cx, session->js_glob, &session->js_callback);
		JS_RemoveObjectRoot(session->js_cx, &session->js_glob);
		lprintf(LOG_DEBUG, "%04d %-5s [%s] JavaScript: Done executing script: %s (%.2Lf seconds)"
		        , session->socket, session->client.protocol, session->host_ip, script, xp_timer() - start);
	} while (0);

	SAFECOPY(session->req.physical_path, path);
	FCLOSE_OPEN_FILE(session->req.fp);

	/* Read http_reply object */
	if (!session->req.sent_headers)
		retval = ssjs_send_headers(session, false);

	/* Free up temporary resources here */

	session->req.dynamic = IS_SSJS;
	JS_ENDREQUEST(session->js_cx);

	return retval;
}

static void respond(http_session_t * session)
{
	if (session->req.method == HTTP_OPTIONS)
		send_headers(session, session->req.status, false);
	else {
		if (session->req.dynamic == IS_FASTCGI)  {
			if (!exec_fastcgi(session)) {
				send_error(session, __LINE__, error_500);
				return;
			}
			session->req.finished = true;
			return;
		}

		if (session->req.dynamic == IS_CGI)  {
			if (!exec_cgi(session))  {
				send_error(session, __LINE__, error_500);
				return;
			}
			session->req.finished = true;
			return;
		}

		if (session->req.dynamic == IS_SSJS) { /* Server-Side JavaScript */
			if (!exec_ssjs(session, session->req.physical_path))  {
				send_error(session, __LINE__, error_500);
				return;
			}
			sprintf(session->req.physical_path
			        , "%sSBBS_SSJS.%u.%u.html", scfg.temp_dir, getpid(), session->socket);
		}
		else {
			session->req.mime_type = get_mime_type(strrchr(session->req.physical_path, '.'));
			send_headers(session, session->req.status, false);
		}
	}
	int64_t content_length = flength(session->req.physical_path);
	if (session->req.send_content && content_length > 0)  {
		off_t  snt = 0;
		time_t start = time(NULL);
		if (session->req.range_start != 0 || (session->req.got_range && session->req.range_end != (content_length - 1)))
			lprintf(LOG_INFO, "%04d %-5s [%s] Sending file: %s (range %" PRIdOFF "-%" PRIdOFF " of %" PRIdOFF " bytes)"
			        , session->socket, session->client.protocol, session->client.addr, session->req.physical_path
			        , session->req.range_start, session->req.range_end, content_length);
		else
			lprintf(LOG_INFO, "%04d %-5s [%s] Sending file: %s (%" PRIdOFF " bytes)"
			        , session->socket, session->client.protocol, session->client.addr, session->req.physical_path, content_length);
		snt = sock_sendfile(session, session->req.physical_path, session->req.range_start, session->req.range_end);
		if (!session->send_failed) {
			if (session->req.ld != NULL) {
				if (snt < 0)
					snt = 0;
				session->req.ld->size = snt;
			}
			if (snt > 0) {
				char   cps[32] = "";
				time_t e = time(NULL) - start;
				if (e > 1)
					snprintf(cps, sizeof cps, ", %ld cps", (long)(snt / e));
				if (snt == content_length)
					lprintf(LOG_INFO, "%04d %-5s [%s] Sent file: %s (%" PRIdOFF " bytes%s)"
					        , session->socket, session->client.protocol, session->client.addr
					        , session->req.physical_path, snt, cps);
				else
					lprintf(LOG_INFO, "%04d %-5s [%s] Sent %" PRIdOFF " bytes%s (offset %" PRIdOFF "-%" PRIdOFF ") of file: %s (%" PRIdOFF " bytes)"
					        , session->socket, session->client.protocol, session->client.addr
					        , snt, cps, session->req.range_start, session->req.range_end
					        , session->req.physical_path, content_length);
				if (session->parsed_vpath == PARSED_VPATH_FULL && session->file.name != NULL) {
					user_downloaded_file(&scfg, &session->user, &session->client, session->file.dir, session->file.name, snt);
					mqtt_file_download(&mqtt, &session->user, session->file.dir, session->file.name, snt, &session->client);
				}
			}
		}
	}
	session->req.finished = true;
}

bool post_to_file(http_session_t *session, FILE*fp, size_t ch_len)
{
	char   buf[20 * 1024];
	size_t k;
	int    bytes_read;

	for (k = 0; k < ch_len;) {
		bytes_read = recvbufsocket(session, buf, (ch_len - k) > sizeof(buf)?sizeof(buf):(ch_len - k));
		if (!bytes_read) {
			send_error(session, __LINE__, error_500);
			return false;
		}
		if (fwrite(buf, bytes_read, 1, fp) != 1) {
			send_error(session, __LINE__, error_500);
			return false;
		}
		k += bytes_read;
		session->req.post_len += bytes_read;
	}
	return true;
}

FILE *open_post_file(http_session_t *session)
{
	char  path[MAX_PATH + 1];
	FILE *fp;

	// Create temporary file for post data.
	SAFEPRINTF3(path, "%sSBBS_POST.%u.%u.data", scfg.temp_dir, getpid(), session->socket);
	if ((fp = fopen(path, "wb")) == NULL) {
		errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !ERROR %d (%s) opening/creating %s"
			, session->socket, session->client.protocol, session->host_ip, errno, strerror(errno), path);
		return fp;
	}
	if (session->req.cleanup_file[CLEANUP_POST_DATA]) {
		remove(session->req.cleanup_file[CLEANUP_POST_DATA]);
		free(session->req.cleanup_file[CLEANUP_POST_DATA]);
	}
	/* remove()d in close_request() */
	session->req.cleanup_file[CLEANUP_POST_DATA] = strdup(path);
	if (session->req.post_data != NULL) {
		if (fwrite(session->req.post_data, session->req.post_len, 1, fp) != 1) {
			errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !ERROR %d (%s) writing to %s"
				, session->socket, session->client.protocol, session->host_ip, errno, strerror(errno), path);
			fclose(fp);
			return NULL;
		}
		FREE_AND_NULL(session->req.post_data);
	}
	return fp;
}

int read_post_data(http_session_t * session)
{
	size_t s = 0;
	FILE * fp = NULL;

	// TODO: For TLS-CGI, write each separate read...
	if ((session->req.dynamic != IS_CGI || (session->req.dynamic == IS_CGI && session->tls_sess)) && (session->req.post_len || session->req.read_chunked)) {
		if (session->req.read_chunked) {
			char * p;
			size_t ch_len = 0;
			int    bytes_read = 0;
			char   ch_lstr[12];
			session->req.post_len = 0;

			while (1) {
				/* Read chunk length */
				if (sockreadline(session, ch_lstr, sizeof(ch_lstr) - 1) > 0) {
					ch_len = strtol(ch_lstr, NULL, 16);
				}
				else {
					send_error(session, __LINE__, error_500);
					FCLOSE_OPEN_FILE(fp);
					return false;
				}
				if (ch_len == 0)
					break;
				/* Check size */
				s += ch_len;
				if (s > MAX_POST_LEN) {
					if (s > SIZE_MAX) {
						send_error(session, __LINE__, "413 Request entity too large");
						FCLOSE_OPEN_FILE(fp);
						return false;
					}
					if (fp == NULL) {
						fp = open_post_file(session);
						if (fp == NULL)
							return false;
					}
					if (!post_to_file(session, fp, ch_len)) {
						fclose(fp);
						return false;
					}
				}
				else {
					/* realloc() to new size */
					/* FREE()d in close_request */
					p = static_cast<char *>(realloc(session->req.post_data, s));
					if (p == NULL) {
						errprintf(LOG_CRIT, WHERE, "%04d %-5s [%s] !ERROR Allocating %lu bytes of memory"
							, session->socket, session->client.protocol, session->host_ip, (ulong)session->req.post_len);
						send_error(session, __LINE__, "413 Request entity too large");
						FCLOSE_OPEN_FILE(fp);
						return false;
					}
					session->req.post_data = p;
					/* read new data */
					bytes_read = recvbufsocket(session, session->req.post_data + session->req.post_len, ch_len);
					if (!bytes_read) {
						send_error(session, __LINE__, error_500);
						FCLOSE_OPEN_FILE(fp);
						return false;
					}
					session->req.post_len += bytes_read;
					/* Read chunk terminator */
					if (sockreadline(session, ch_lstr, sizeof(ch_lstr) - 1) > 0)
						send_error(session, __LINE__, error_500);
				}
			}
			if (fp) {
				fclose(fp);
				FREE_AND_NULL(session->req.post_data);
				session->req.post_map = xpmap(session->req.cleanup_file[CLEANUP_POST_DATA], XPMAP_WRITE);
				if (!session->req.post_map)
					return false;
				session->req.post_data = static_cast<char *>(session->req.post_map->addr);
			}
			/* Read more headers! */
			if (!get_request_headers(session))
				return false;
			if (!is_legal_host(session->req.vhost, false)) {
				send_error(session, __LINE__, "400 Bad Request");
				return false;
			}
			if (!parse_headers(session))
				return false;
		}
		else {
			s = session->req.post_len;
			FREE_AND_NULL(session->req.post_data);
			if (s > MAX_POST_LEN) {
				fp = open_post_file(session);
				if (fp == NULL)
					return false;
				bool success = post_to_file(session, fp, s);
				fclose(fp);
				if (!success)
					return false;
				session->req.post_map = xpmap(session->req.cleanup_file[CLEANUP_POST_DATA], XPMAP_WRITE);
				if (!session->req.post_map)
					return false;
				session->req.post_data = static_cast<char *>(session->req.post_map->addr);
			}
			else {
				/* FREE()d in close_request()  */
				if (s < (MAX_POST_LEN + 1) && (session->req.post_data = static_cast<char *>(malloc((size_t)(s + 1)))) != NULL)
					session->req.post_len = recvbufsocket(session, session->req.post_data, s);
				else  {
					errprintf(LOG_CRIT, WHERE, "%04d %-5s [%s] !ERROR Allocating %lu bytes of memory"
						, session->socket, session->client.protocol, session->host_ip, (ulong)s);
					send_error(session, __LINE__, "413 Request entity too large");
					return false;
				}
			}
		}
		if (session->req.post_len != s)
			lprintf(LOG_DEBUG, "%04d %-5s [%s] !ERROR Browser said they sent %lu bytes, but I got %lu"
				, session->socket, session->client.protocol, session->host_ip, (ulong)s, (ulong)session->req.post_len);
		if (session->req.post_len > s)
			session->req.post_len = s;
		if (session->req.post_data != NULL)
			session->req.post_data[session->req.post_len] = 0;
	}
	return true;
}

void http_output_thread(void *arg)
{
	http_session_t *session = (http_session_t *)arg;
	RingBuf *       obuf;
	char            buf[OUTBUF_LEN + 12];           /* *MUST* be large enough to hold the buffer,
	                                                	the size of the buffer in hex, and four extra bytes. */
	char *          bufdata;
	int             len;
	unsigned        avail;
	int             chunked;
	int             i;
	unsigned        mss = OUTBUF_LEN;

	SetThreadName("sbbs/httpOutput");
	thread_up(true /* setuid */);

	obuf = &(session->outbuf);
	/* Destroyed at end of function */
	if ((i = pthread_mutex_init(&session->outbuf_write, NULL)) != 0) {
		lprintf(LOG_DEBUG, "%04d %-5s [%s] Error %d initializing outbuf mutex", session->socket, session->client.protocol, session->host_ip, i);
		close_session_socket(session);
		thread_down();
		return;
	}
	session->outbuf_write_initialized = 1;

#ifdef TCP_MAXSEG
	/*
	 * Auto-tune the highwater mark to be the negotiated MSS for the
	 * socket (when possible)
	 */
	socklen_t sl;
	sl = sizeof(i);
	if (!getsockopt(session->socket, IPPROTO_TCP, TCP_MAXSEG, (char*)&i, &sl)) {
		/* Check for sanity... */
		if (i > 100) {
#ifdef _WIN32
#ifdef TCP_TIMESTAMPS
			DWORD ts;
			sl = sizeof(ts);
			if (!getsockopt(session->socket, IPPROTO_TCP, TCP_TIMESTAMPS, (char *)&ts, &sl)) {
				if (ts)
					i -= 12;
			}
#endif
#else
#if (defined(TCP_INFO) && defined(TCPI_OPT_TIMESTAMPS))
			struct tcp_info tcpi;

			sl = sizeof(tcpi);
			if (!getsockopt(session->socket, IPPROTO_TCP, TCP_INFO, &tcpi, &sl)) {
				if (tcpi.tcpi_options & TCPI_OPT_TIMESTAMPS)
					i -= 12;
			}
#endif
#endif
			obuf->highwater_mark = i;
			lprintf(LOG_DEBUG, "%04d %-5s [%s] Autotuning outbuf highwater mark to %d based on MSS"
			        , session->socket, session->client.protocol, session->host_ip, i);
			mss = obuf->highwater_mark;
			if (mss > OUTBUF_LEN) {
				mss = OUTBUF_LEN;
				lprintf(LOG_DEBUG, "%04d %-5s [%s] MSS (%d) is higher than OUTBUF_LEN (%d)"
				        , session->socket, session->client.protocol, session->host_ip, i, OUTBUF_LEN);
			}
		}
	}
#endif

	/*
	 * Do *not* exit on terminate_server... wait for session thread
	 * to close the socket and set it to INVALID_SOCKET
	 */
	while (session->socket != INVALID_SOCKET) {

		/* Wait for something to output in the RingBuffer */
		if ((avail = RingBufFull(obuf)) == 0) {  /* empty */
			if (WaitForEvent(obuf->data_event, 1000) != WAIT_OBJECT_0)
				continue;
			/* Check for spurious sem post... */
			if ((avail = RingBufFull(obuf)) == 0)
				continue;
		}

		/* Wait for full buffer or drain timeout */
		if (obuf->highwater_mark) {
			if (avail < obuf->highwater_mark) {
				WaitForEvent(obuf->highwater_event, startup->outbuf_drain_timeout);
				/* We (potentially) blocked, so get fill level again */
				avail = RingBufFull(obuf);
			}
		}

		/*
		 * At this point, there's something to send and,
		 * if the highwater mark is set, the timeout has
		 * passed or we've hit highwater.  Read ring buffer
		 * into linear buffer.
		 */
		len = avail;
		if (avail > mss)
			len = (avail = mss);

		/*
		 * Read the current value of write_chunked... since we wait until the
		 * ring buffer is empty before fiddling with it.
		 */
		chunked = session->req.write_chunked;

		bufdata = buf;
		if (chunked) {
			i = sprintf(buf, "%X\r\n", avail);
			bufdata += i;
			len += i;
		}

		pthread_mutex_lock(&session->outbuf_write);
		RingBufRead(obuf, (uchar*)bufdata, avail);
		if (chunked) {
			bufdata += avail;
			*(bufdata++) = '\r';
			*(bufdata++) = '\n';
			len += 2;
		}

		if (!session->send_failed)
			sess_sendbuf(session, buf, len, &session->send_failed);
		pthread_mutex_unlock(&session->outbuf_write);
	}
	thread_down();
	/* Ensure outbuf isn't currently being drained */
	pthread_mutex_lock(&session->outbuf_write);
	session->outbuf_write_initialized = 0;
	pthread_mutex_unlock(&session->outbuf_write);
	pthread_mutex_destroy(&session->outbuf_write);
	sem_post(&session->output_thread_terminated);
}

static int close_session_no_rb(http_session_t *session)
{
	if (session) {
		if (session->is_tls)
			HANDLE_CRYPT_CALL(destroy_session(lprintf, session->tls_sess), session, "destroying session");
		return close_socket(&session->socket);
	}
	return 0;
}

void http_session_thread(void* arg)
{
	SOCKET         socket;
	char *         redirp;
	http_session_t session;
	int            loop_count;
	ulong          login_attempts = 0;
	bool           init_error;
	uint32_t       client_count;
	int32_t        clients_remain;
#if 0
	int            i;
	int            last;
	user_t         user;
	char *         uname;
#endif

	SetThreadName("sbbs/httpSess");
	pthread_mutex_lock(&((http_session_t*)arg)->struct_filled);
	pthread_mutex_unlock(&((http_session_t*)arg)->struct_filled);
	pthread_mutex_destroy(&((http_session_t*)arg)->struct_filled);

	session = *(http_session_t*)arg;  /* copies arg BEFORE it's freed */
	FREE_AND_NULL(arg);

	thread_up(true /* setuid */);

	socket = session.socket;
	if (socket == INVALID_SOCKET) {
		thread_down();
		return;
	}
	SAFECOPY(session.client.protocol, session.is_tls ? "HTTPS":"HTTP");
	lprintf(LOG_DEBUG, "%04d %-5s [%s] Session thread started", session.socket, session.client.protocol, session.host_ip);

	if (startup->index_file_name == NULL || startup->cgi_ext == NULL)
		lprintf(LOG_DEBUG, "%04d !!! DANGER WILL ROBINSON, DANGER !!!", session.socket);

#ifdef _WIN32
	if (startup->sound.answer[0] && !sound_muted(&scfg))
		PlaySound(startup->sound.answer, NULL, SND_ASYNC | SND_FILENAME);
#endif

	session.finished = false;

	if (session.is_tls) {
		BOOL looking_good;
		/* Create and initialize the TLS session */
		if (!HANDLE_CRYPT_CALL(cryptCreateSession(&session.tls_sess, CRYPT_UNUSED, CRYPT_SESSION_TLS_SERVER), &session, "creating session")) {
			close_session_no_rb(&session);
			thread_down();
			return;
		}
		/* Add all the user/password combinations */
#if 0 // TLS-PSK is currently broken in cryptlib
		last = lastuser(&scfg);
		for (i = 1; i <= last; i++) {
			user.number = i;
			getuserdat(&scfg, &user);
			if (user.misc & (DELETED | INACTIVE))
				continue;
			if (user.alias[0] && user.pass[0]) {
				if (HANDLE_CRYPT_CALL(cryptSetAttributeString(session.tls_sess, CRYPT_SESSINFO_USERNAME, user.alias, strlen(user.alias)), &session, "getting username"))
					HANDLE_CRYPT_CALL(cryptSetAttributeString(session.tls_sess, CRYPT_SESSINFO_PASSWORD, user.pass, strlen(user.pass)), &session, "getting password");
			}
		}
#endif

		looking_good = do_cryptInit(lprintf);
		if (looking_good)
			looking_good = HANDLE_CRYPT_CALL(add_private_key(&scfg, lprintf, session.tls_sess), &session, "setting private key");
		if (!looking_good) {
			cryptDestroySession(session.tls_sess);
			session.tls_sess = -1;
			close_session_no_rb(&session);
			thread_down();
			return;
		}
		bool nodelay = true;
		setsockopt(session.socket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));

		if (looking_good)
			looking_good = HANDLE_CRYPT_CALL(cryptSetAttribute(session.tls_sess, CRYPT_SESSINFO_TLS_OPTIONS, CRYPT_TLSOPTION_MINVER_TLS12), &session, "setting TLS minver to 1.2");
		if (looking_good)
			looking_good = HANDLE_CRYPT_CALL(cryptSetAttribute(session.tls_sess, CRYPT_SESSINFO_NETWORKSOCKET, session.socket), &session, "setting network socket");
		if (looking_good)
			looking_good = HANDLE_CRYPT_CALL(cryptSetAttribute(session.tls_sess, CRYPT_SESSINFO_ACTIVE, 1), &session, "setting session active");
		if (!looking_good) {
			close_session_no_rb(&session);
			thread_down();
			return;
		}
		HANDLE_CRYPT_CALL(cryptSetAttribute(session.tls_sess, CRYPT_OPTION_NET_READTIMEOUT, 0), &session, "setting read timeout");
	}

	/* Start up the output buffer */
	/* FREE()d in this block (RingBufDispose before all returns) */
	if (RingBufInit(&(session.outbuf), OUTBUF_LEN)) {
		errprintf(LOG_ERR, WHERE, "%04d Canot create output ringbuffer!", session.socket);
		close_session_no_rb(&session);
		thread_down();
		return;
	}

	/* Destroyed in this block (before all returns) */
	sem_init(&session.output_thread_terminated, 0, 0);
	(void)protected_uint32_adjust(&thread_count, 1);
	_beginthread(http_output_thread, 0, &session);

	sbbs_srand();   /* Seed random number generator */

	struct trash trash;
	char         host_name[128] = "";
	if (!(startup->options & BBS_OPT_NO_HOST_LOOKUP))  {
		getnameinfo(&session.addr.addr, session.addr_len, host_name, sizeof(host_name), NULL, 0, NI_NAMEREQD);
		lprintf(LOG_INFO, "%04d %-5s [%s] Hostname: %s"
		        , session.socket, session.client.protocol, session.host_ip, host_name[0] ? host_name : STR_NO_HOSTNAME);
#if 0 /* gethostbyaddr() is apparently not (always) thread-safe
		   and getnameinfo() doesn't return alias information */
		for (i = 0; host != NULL && host->h_aliases != NULL
		     && host->h_aliases[i] != NULL; i++)
			lprintf(LOG_INFO, "%04d HostAlias: %s", session.socket, host->h_aliases[i]);
#endif
		if (host_name[0] && !host_exempt->listed(host_name, nullptr) && host_can->listed(host_name, nullptr, &trash)) {
			if (!trash.quiet) {
				char details[128];
				lprintf(LOG_NOTICE, "%04d %-5s [%s] !CLIENT BLOCKED in %s: %s %s"
						, session.socket, session.client.protocol, session.host_ip, host_can->fname, host_name, trash_details(&trash, details, sizeof details));
			}
			close_session_socket(&session);
			sem_wait(&session.output_thread_terminated);
			sem_destroy(&session.output_thread_terminated);
			RingBufDispose(&session.outbuf);
			thread_down();
			return;
		}
	}
	if (host_name[0])
		SAFECOPY(session.host_name, host_name);
	else {
		SAFECOPY(session.host_name, session.host_ip);
		SAFECOPY(host_name, STR_NO_HOSTNAME);
	}

	if (!host_exempt->listed(session.host_ip, session.host_name)) {
		login_attempt_t attempted;
		ulong           banned = loginBanned(&scfg, startup->login_attempt_list, session.socket, host_name, startup->login_attempt, &attempted);

		/* host_ip wasn't defined in http_session_thread */
		if (banned || ip_can->listed(session.host_ip, nullptr, &trash)) {
			if (banned) {
				char ban_duration[128];
				lprintf(LOG_NOTICE, "%04d %-5s [%s] !TEMPORARY BAN (%lu login attempts, last: %s) - remaining: %s"
						, session.socket, session.client.protocol
						, session.host_ip, attempted.count - attempted.dupes, attempted.user
						, duration_estimate_to_vstr(banned, ban_duration, sizeof ban_duration, 1, 1));
			} else if (!trash.quiet) {
				char details[128];
				lprintf(LOG_NOTICE, "%04d %-5s [%s] !CLIENT BLOCKED in %s %s"
						, session.socket, session.client.protocol, session.host_ip, ip_can->fname, trash_details(&trash, details, sizeof details));
			}
			close_session_socket(&session);
			sem_wait(&session.output_thread_terminated);
			sem_destroy(&session.output_thread_terminated);
			RingBufDispose(&session.outbuf);
			thread_down();
			return;
		}
	}

	client_count = protected_uint32_adjust_fetch(&active_clients, 1);
	update_clients();
	SAFECOPY(session.username, unknown);

	SAFECOPY(session.client.addr, session.host_ip);
	SAFECOPY(session.client.host, host_name);
	session.client.port = inet_addrport(&session.addr);
	session.client.time = time32(NULL);
	SAFECOPY(session.client.user, session.username);
	session.client.size = sizeof(session.client);
	session.client.usernum = 0;
	client_on(session.socket, &session.client, /* update existing client record? */ false);

	if (startup->login_attempt.throttle
	    && (login_attempts = loginAttempts(startup->login_attempt_list, &session.addr)) > 1) {
		lprintf(LOG_DEBUG, "%04d %-5s [%s] Throttling suspicious connection (%lu login attempts)"
		        , socket, session.client.protocol, session.host_ip, login_attempts);
		mswait(login_attempts * startup->login_attempt.throttle);
	}

	session.last_user_num = -1;
	session.last_js_user_num = -1;
	session.logon_time = 0;

	session.subscan = (subscan_t*)calloc(scfg.total_subs, sizeof(subscan_t));

	/*
	 * If we don't parse a request method, assume GET / HTTP/1.0
	 */
	session.req.method = HTTP_GET;
	session.http_ver = HTTP_1_0;
	if (startup->max_clients && client_count > startup->max_clients) {
		lprintf(LOG_WARNING, "%04d %-5s [%s] !MAXIMUM CLIENTS (%u) exceeded by %u, access denied"
		        , socket, session.client.protocol, session.host_ip, startup->max_clients, client_count - startup->max_clients);
		send_error(&session, __LINE__, error_503);
		session.finished = true;
	} else {
		uint connections = listCountMatches(&current_connections, session.host_ip, strlen(session.host_ip) + 1);
		if (startup->max_concurrent_connections > 0 && connections > startup->max_concurrent_connections
		    && !host_exempt->listed(session.host_ip, nullptr)) {
			lprintf(LOG_NOTICE, "%04d %-5s [%s] !Maximum concurrent connections (%u) exceeded"
			        , socket, session.client.protocol, session.host_ip, startup->max_concurrent_connections);
			send_error(&session, __LINE__, error_429);
			session.finished = true;
		} else {
			if (connections > con_conn_highwater) {
				con_conn_highwater = connections;
				if (con_conn_highwater > 1)
					lprintf(LOG_NOTICE, "%04d %-5s [%s] New concurrent connections per client highwater mark: %u"
					        , socket, session.client.protocol, session.host_ip, con_conn_highwater);
			}
			if (client_count > client_highwater) {
				client_highwater = client_count;
				if (client_highwater > 1)
					lprintf(LOG_NOTICE, "%04d %-5s [%s] New active client highwater mark: %u"
					        , socket, session.client.protocol, session.host_ip, client_highwater);
				mqtt_pub_uintval(&mqtt, TOPIC_SERVER, "highwater", mqtt.highwater = client_highwater);
			}
		}
	}

	while (!session.finished) {
		init_error = false;
		memset(&(session.req), 0, sizeof(session.req));
		if (session.is_tls) {
#if 0 // TLS-PSK is currently broken in cryptlib
			uname = get_crypt_attribute(session.tls_sess, CRYPT_SESSINFO_USERNAME);
			if (uname) {
				SAFECOPY(session.req.auth.username, uname);
				free_crypt_attrstr(uname);
				session.req.auth.type = AUTHENTICATION_TLS_PSK;
			}
#endif
		}
		redirp = NULL;
		loop_count = 0;
		if (startup->options & WEB_OPT_HTTP_LOGGING) {
			/* FREE()d in http_logging_thread... passed there by close_request() */
			if ((session.req.ld = (struct log_data*)malloc(sizeof(struct log_data))) == NULL)
				errprintf(LOG_ERR, WHERE, "%04d Cannot allocate memory for log data!", session.socket);
		}
		if (session.req.ld != NULL) {
			memset(session.req.ld, 0, sizeof(struct log_data));
			/* FREE()d in http_logging_thread */
			session.req.ld->hostname = strdup(session.host_name);
		}
		while ((redirp == NULL || session.req.send_location >= MOVED_TEMP)
		       && !session.finished && !session.req.finished
		       && session.socket != INVALID_SOCKET) {
			SAFECOPY(session.req.status, "200 OK");
			session.req.send_location = NO_LOCATION;
			if (session.req.headers == NULL) {
				/* FREE()d in close_request() */
				if ((session.req.headers = strListInit()) == NULL) {
					errprintf(LOG_ERR, WHERE, "%04d !ERROR allocating memory for header list", session.socket);
					init_error = true;
				}
			}
			if (session.req.cgi_env == NULL) {
				/* FREE()d in close_request() */
				if ((session.req.cgi_env = strListInit()) == NULL) {
					errprintf(LOG_ERR, WHERE, "%04d !ERROR allocating memory for CGI environment list", session.socket);
					init_error = true;
				}
			}
			if (session.req.dynamic_heads == NULL) {
				/* FREE()d in close_request() */
				if ((session.req.dynamic_heads = strListInit()) == NULL) {
					errprintf(LOG_ERR, WHERE, "%04d !ERROR allocating memory for dynamic header list", session.socket);
					init_error = true;
				}
			}

			if (get_req(&session, redirp)) {
				if (redirp)
					redirp[0] = 0;
				if (init_error) {
					send_error(&session, __LINE__, error_500);
				}
				/* At this point, if redirp is non-NULL then the headers have already been parsed */
				if ((session.http_ver < HTTP_1_0) || redirp != NULL || parse_headers(&session)) {
					if (check_request(&session)) {
						if (session.req.send_location < MOVED_TEMP || session.req.virtual_path[0] != '/' || loop_count++ >= MAX_REDIR_LOOPS) {
							if (read_post_data(&session))
								respond(&session);
						}
						else {
							if (!session.redir_req[0]) {
								safe_snprintf(session.redir_req, sizeof(session.redir_req), "%s %s%s%s", methods[session.req.method]
								              , session.req.virtual_path, session.http_ver < HTTP_1_0?"":" ", http_vers[session.http_ver]);
							}
							lprintf(LOG_DEBUG, "%04d %-5s [%s] Internal Redirect to: %s"
							        , socket, session.client.protocol, session.host_ip, session.redir_req);
							redirp = session.redir_req;
						}
					}
				}
			}
			else {
				session.req.keep_alive = false;
				break;
			}
		}
		close_request(&session);
	}
	listRemoveTaggedNode(&current_connections, socket, /* free_data */ true);

	http_logoff(&session, socket, __LINE__);

	if (session.js_cx != NULL) {
		lprintf(LOG_DEBUG, "%04d %-5s [%s] JavaScript: Destroying context", socket, session.client.protocol, session.host_ip);
		JS_BEGINREQUEST(session.js_cx);
		JS_RemoveObjectRoot(session.js_cx, &session.js_glob);
		JS_ENDREQUEST(session.js_cx);
		JS_DestroyContext(session.js_cx);   /* Free Context */
		session.js_cx = NULL;
	}

	if (session.js_runtime != NULL) {
		lprintf(LOG_DEBUG, "%04d %-5s [%s] JavaScript: Destroying runtime", socket, session.client.protocol, session.host_ip);
		jsrt_Release(session.js_runtime);
		session.js_runtime = NULL;
	}

#ifdef _WIN32
	if (startup->sound.hangup[0] && !sound_muted(&scfg))
		PlaySound(startup->sound.hangup, NULL, SND_ASYNC | SND_FILENAME);
#endif

	close_session_socket(&session);
	sem_wait(&session.output_thread_terminated);
	sem_destroy(&session.output_thread_terminated);
	RingBufDispose(&session.outbuf);
	FREE_AND_NULL(session.subscan);

	clients_remain = protected_uint32_adjust_fetch(&active_clients, -1);
	update_clients();
	client_off(socket);

	uint32_t threads_remain = thread_down();

	if (startup->index_file_name == NULL || startup->cgi_ext == NULL)
		lprintf(LOG_DEBUG, "%04d !!! ALL YOUR BASE ARE BELONG TO US !!!", socket);

	lprintf(LOG_INFO, "%04d %-5s [%s] Session thread terminated after %u requests (%u clients and %u threads remain, %lu served, %u concurrently)"
	        , socket, session.client.protocol, session.host_ip, session.requests, clients_remain, threads_remain, ++served, client_highwater);

}

void web_terminate(void)
{
	lprintf(LOG_INFO, "Web Server terminate");
	terminate_server = true;
}

static void cleanup(int code)
{
	bool     waited = false;
	uint32_t threads;

	while ((threads = protected_uint32_value(thread_count)) > 1) {
		lprintf(LOG_INFO, "0000 Waiting for %d child threads to terminate", threads - 1);
		mswait(1000);
		waited = true;
		listSemPost(&log_list);
	}
	if (waited)
		lprintf(LOG_INFO, "0000 Done waiting");
	terminate_js = false;
	free_cfg(&scfg);

	listFree(&log_list);

	mime_types = iniFreeNamedStringList(mime_types);

	cgi_handlers = iniFreeNamedStringList(cgi_handlers);
	xjs_handlers = iniFreeNamedStringList(xjs_handlers);
	alias_list = iniFreeNamedStringList(alias_list);

	cgi_env = iniFreeStringList(cgi_env);

	semfile_list_free(&pause_semfiles);
	semfile_list_free(&recycle_semfiles);
	semfile_list_free(&shutdown_semfiles);

	if (ws_set != NULL) {
		xpms_destroy(ws_set, close_socket_cb, NULL);
		ws_set = NULL;
	}

	update_clients();   /* active_clients is destroyed below */

	listFree(&current_connections);

	if (protected_uint32_value(active_clients))
		lprintf(LOG_WARNING, "!!!! Terminating with %u active clients", protected_uint32_value(active_clients));
	else
		protected_uint32_destroy(active_clients);

#ifdef _WINSOCKAPI_
	if (WSAInitialized && WSACleanup() != 0)
		lprintf(LOG_ERR, "0000 !WSACleanup ERROR %d", SOCKET_ERRNO);
#endif

	thread_down();
	if (terminate_server || code) {
		lprintf(LOG_INFO, "#### Web Server thread terminated (%lu clients served, %u concurrently, denied: %u due to rate limit, %u due to IP address, %u due to hostname)"
		        , served, client_highwater, request_rate_limiter->disallowed.load(), ip_can->total_found.load() + ip_silent_can->total_found.load(), host_can->total_found.load());
		set_state(SERVER_STOPPED);
		if (startup != NULL && startup->terminated != NULL)
			startup->terminated(startup->cbdata, code);
	}

	delete ip_can, ip_can = nullptr;
	delete ip_silent_can, ip_silent_can = nullptr;
	delete host_can, host_can = nullptr;
	delete host_exempt, host_exempt = nullptr;

	mqtt_shutdown(&mqtt);
}

const char* web_ver(void)
{
	static char ver[256];
	char        compiler[32];

	DESCRIBE_COMPILER(compiler);

	sprintf(ver, "%s %s%c%s  "
	        "Compiled %s/%s %s with %s"
	        , server_name
	        , VERSION, REVISION
#ifdef _DEBUG
	        , " Debug"
#else
	        , ""
#endif
	        , GIT_BRANCH, GIT_HASH
	        , GIT_DATE, compiler);

	return ver;
}

static char* clf_str(char* str)
{
	if (str == nullptr || *str == '\0')
		return (char*)"-";
	return str;
}

void format_log_line(char* buf, size_t buflen, const char* fmt, struct log_data* ld, char* timestr, char* sizestr)
{
	named_string_t str_vars[] = {
		{ (char*)"a", ld->client.addr },
		{ (char*)"H", ld->client.protocol },
		{ (char*)"b", sizestr },
		{ (char*)"t", timestr },
		{ (char*)"h", clf_str(ld->hostname) },
		{ (char*)"l", clf_str(ld->ident) },
		{ (char*)"u", clf_str(ld->user) },
		{ (char*)"r", clf_str(ld->request) },
		{ (char*)"v", clf_str(ld->vhost) },
		{ (char*)"{Referer}i", clf_str(ld->referrer) }, // sic
		{ (char*)"{User-agent}i", clf_str(ld->agent) },
		{ nullptr, nullptr }
	};

	named_long_t int_vars[] = {
		{ (char*)"B", (long)ld->size },
		{ (char*)"s", (long)ld->status },
		{ (char*)"p", (long)ld->server_port },
		{ (char*)"{remote}p", ld->client.port },
		{ nullptr },
	};

	replace_named_values(fmt, buf, buflen, "%", NULL, str_vars, int_vars, /* case-sensitive: */true);
}

void http_logging_thread(void* arg)
{
	char  base[MAX_PATH + 1];
	char  filename[MAX_PATH + 1];
	char  newfilename[MAX_PATH + 1];
	FILE* logfile = NULL;

	terminate_http_logging_thread = false;

	SAFECOPY(base, static_cast<char *>(arg));
	if (!base[0])
		SAFEPRINTF(base, "%slogs/http-", scfg.logs_dir);

	SetThreadName("sbbs/httpLog");
	filename[0] = 0;
	newfilename[0] = 0;

	thread_up(true /* setuid */);

	lprintf(LOG_INFO, "Web Server access logging thread started");

	while (!terminate_http_logging_thread) {
		struct log_data *ld;

		if (!listSemTryWait(&log_list)) {
			if (logfile != NULL)
				fflush(logfile);
			listSemWait(&log_list);
		}

		ld = (log_data *)listShiftNode(&log_list);
		/*
		 * Because the sem is posted when terminate_http_logging_thread is set, this will
		 * ensure that all pending log entries are written to disk
		 */
		if (ld == NULL) {
			if (terminate_http_logging_thread)
				break;
			errprintf(LOG_ERR, WHERE, "Web Server access-logging thread received NULL linked list log entry");
			continue;
		}
		SAFECOPY(newfilename, base);
		if ((startup->options & WEB_OPT_VIRTUAL_HOSTS) && !(startup->options & WEB_OPT_ONE_HTTP_LOG) && ld->vhost != NULL) {
			char  vhost[128];
			SAFECOPY(vhost, ld->vhost);
#ifdef _WIN32
			char* p;
			REPLACE_CHARS(vhost, ':', '!', p);
#endif
			SAFECAT(newfilename, vhost);
			if (vhost[0])
				SAFECAT(newfilename, "-");
		}
		strftime(strchr(newfilename, 0), 15, "%Y-%m-%d.log", &ld->completed);
		if (logfile == NULL || strcmp(newfilename, filename)) {
			FCLOSE_OPEN_FILE(logfile);
			SAFECOPY(filename, newfilename);
			logfile = fopen(filename, "ab");
			if (logfile == NULL)
				errprintf(LOG_ERR, WHERE, "Web Server error %d (%s) opening/creating access-log file %s", errno, strerror(errno), filename);
			else
				lprintf(LOG_INFO, "Web Server access-log file is now: %s", filename);
		}
		if (logfile != NULL && ld->status) {
			char timestr[128];
			char sizestr[100] = "-";

			if (ld->size > 0)
				snprintf(sizestr, sizeof sizestr, "%" PRIdOFF, ld->size);

			strftime(timestr, sizeof timestr, "[%d/%b/%Y:%H:%M:%S %z]", &ld->completed);
			/*
				* In case of a termination, do no block for a lock... just discard
				* the output.
				*/
			while (lock(fileno(logfile), 0, 1) && !terminate_http_logging_thread) {
				SLEEP(10);
			}
			if (startup->custom_log_fmt[0] != '\0') {
				/* Custom log format */
				char logline[2048];
				format_log_line(logline, sizeof logline, startup->custom_log_fmt, ld, timestr, sizestr);
				fprintf(logfile, "%s\n", logline);
			}
			else {
				char vhost[256]{};
				if ((startup->options & WEB_OPT_VIRTUAL_HOSTS) && (startup->options & WEB_OPT_ONE_HTTP_LOG))
					SAFECOPY(vhost, clf_str(ld->vhost));
				fprintf(logfile, "%s%s%s %s %s %s \"%s\" %d %s \"%s\" \"%s\"\n"
						, vhost, *vhost ? " " : ""
						, clf_str(ld->hostname)
						, clf_str(ld->ident)
						, clf_str(ld->user)
						, timestr
						, clf_str(ld->request)
						, ld->status
						, sizestr
						, clf_str(ld->referrer)
						, clf_str(ld->agent)
				);
			}
			unlock(fileno(logfile), 0, 1);
		}
		FREE_AND_NULL(ld->hostname);
		FREE_AND_NULL(ld->ident);
		FREE_AND_NULL(ld->user);
		FREE_AND_NULL(ld->request);
		FREE_AND_NULL(ld->referrer);
		FREE_AND_NULL(ld->agent);
		FREE_AND_NULL(ld->vhost);
		FREE_AND_NULL(ld);
	}
	FCLOSE_OPEN_FILE(logfile);
	thread_down();
	lprintf(LOG_INFO, "Web Server access-logging thread terminated");

	http_logging_thread_running = false;
}

void web_server(void* arg)
{
	time_t            start;
	WORD              host_port;
	char              host_ip[INET6_ADDRSTRLEN];
	char              path[MAX_PATH + 1];
	char              logstr[256];
	char              mime_types_ini[MAX_PATH + 1];
	char              web_handler_ini[MAX_PATH + 1];
	char              web_alias_ini[MAX_PATH + 1];
	union xp_sockaddr client_addr;
	socklen_t         client_addr_len;
	SOCKET            client_socket;
	time_t            t;
	time_t            initialized = 0;
	FILE*             fp;
	char*             p;
	char              compiler[32];
	http_session_t *  session = NULL;
	void *            acc_type;
	int               i;

	startup = (web_startup_t*)arg;

	SetThreadName("sbbs/webServer");

	if (startup == NULL) {
		sbbs_beep(100, 500);
		fprintf(stderr, "No startup structure passed!\n");
		return;
	}

	if (startup->size != sizeof(web_startup_t)) {  /* verify size */
		sbbs_beep(100, 500);
		sbbs_beep(300, 500);
		sbbs_beep(100, 500);
		fprintf(stderr, "Invalid startup structure!\n");
		return;
	}
	set_state(SERVER_INIT);

#ifdef _THREAD_SUID_BROKEN
	if (thread_suid_broken)
		startup->seteuid(true);
#endif

	ZERO_VAR(js_server_props);
	SAFEPRINTF3(js_server_props.version, "%s %s%c", server_name, VERSION, REVISION);
	js_server_props.version_detail = web_ver();
	js_server_props.clients = &active_clients;
	js_server_props.options = &startup->options;
	js_server_props.interfaces = &startup->interfaces;

	uptime = 0;
	served = 0;
	startup->recycle_now = false;
	startup->shutdown_now = false;
	terminate_server = false;
	protected_uint32_init(&thread_count, 0);
	request_rate_limiter = new rateLimiter(startup->max_requests_per_period, startup->request_rate_limit_period);

	do {
		listInit(&current_connections, LINK_LIST_MUTEX);
		protected_uint32_init(&active_clients, 0);

		/* Setup intelligent defaults */
		if (startup->port == 0)
			startup->port = IPPORT_HTTP;
		if (startup->root_dir[0] == 0)
			SAFECOPY(startup->root_dir, WEB_DEFAULT_ROOT_DIR);
		if (startup->error_dir[0] == 0)
			SAFECOPY(startup->error_dir, WEB_DEFAULT_ERROR_DIR);
		if (startup->default_auth_list[0] == 0)
			SAFECOPY(startup->default_auth_list, WEB_DEFAULT_AUTH_LIST);
		if (startup->cgi_dir[0] == 0)
			SAFECOPY(startup->cgi_dir, WEB_DEFAULT_CGI_DIR);
		if (startup->default_cgi_content[0] == 0)
			SAFECOPY(startup->default_cgi_content, WEB_DEFAULT_CGI_CONTENT);
		if (startup->max_inactivity == 0)
			startup->max_inactivity = WEB_DEFAULT_MAX_INACTIVITY;                                        /* seconds */
		if (startup->max_cgi_inactivity == 0)
			startup->max_cgi_inactivity = WEB_DEFAULT_MAX_CGI_INACTIVITY;                                        /* seconds */
		if (startup->sem_chk_freq == 0)
			startup->sem_chk_freq = DEFAULT_SEM_CHK_FREQ;                                        /* seconds */
		if (startup->js.max_bytes == 0)
			startup->js.max_bytes = JAVASCRIPT_MAX_BYTES;
		if (startup->ssjs_ext[0] == 0)
			SAFECOPY(startup->ssjs_ext, ".ssjs");

		(void)protected_uint32_adjust(&thread_count, 1);
		thread_up(false /* setuid */);

		/* Copy html directories */
		SAFECOPY(root_dir, startup->root_dir);
		SAFECOPY(error_dir, startup->error_dir);
		SAFECOPY(default_auth_list, startup->default_auth_list);
		SAFECOPY(cgi_dir, startup->cgi_dir);

		/* Change to absolute path */
		prep_dir(startup->ctrl_dir, root_dir, sizeof(root_dir));
		prep_dir(root_dir, error_dir, sizeof(error_dir));
		prep_dir(root_dir, cgi_dir, sizeof(cgi_dir));

		/* Trim off trailing slash/backslash */
		if (IS_PATH_DELIM(*(p = lastchar(root_dir))))
			*p = 0;

		memset(&scfg, 0, sizeof(scfg));

		lprintf(LOG_INFO, "%s Version %s%c%s"
		        , server_name
		        , VERSION, REVISION
#ifdef _DEBUG
		        , " Debug"
#else
		        , ""
#endif
		        );

		DESCRIBE_COMPILER(compiler);

		lprintf(LOG_INFO, "Compiled %s/%s %s with %s", GIT_BRANCH, GIT_HASH, GIT_DATE, compiler);

		if (!winsock_startup()) {
			cleanup(1);
			return;
		}

		t = time(NULL);
		lprintf(LOG_INFO, "Initializing on %.24s with options: %x"
		        , ctime_r(&t, logstr), startup->options);

		if (chdir(startup->ctrl_dir) != 0)
			errprintf(LOG_ERR, WHERE, "!ERROR %d (%s) changing directory to: %s", errno, strerror(errno), startup->ctrl_dir);

		/* Initial configuration and load from CNF files */
		SAFECOPY(scfg.ctrl_dir, startup->ctrl_dir);
		lprintf(LOG_INFO, "Loading configuration files from %s", scfg.ctrl_dir);
		scfg.size = sizeof(scfg);
		SAFECOPY(logstr, UNKNOWN_LOAD_ERROR);
		if (!load_cfg(&scfg, text, TOTAL_TEXT, /* prep: */ true, /* node: */ false, logstr, sizeof(logstr))) {
			errprintf(LOG_CRIT, WHERE, "!ERROR loading configuration files: %s", logstr);
			cleanup(1);
			return;
		}
		if (logstr[0] != '\0')
			lprintf(LOG_WARNING, "!WARNING loading configuration files: %s", logstr);

		mqtt_startup(&mqtt, &scfg, (struct startup*)startup, web_ver(), lputs);

		if (startup->temp_dir[0])
			SAFECOPY(scfg.temp_dir, startup->temp_dir);
		else
			SAFECOPY(scfg.temp_dir, "../temp");
		prep_dir(startup->ctrl_dir, scfg.temp_dir, sizeof(scfg.temp_dir));
		if ((i = md(scfg.temp_dir)) != 0) {
			errprintf(LOG_CRIT, WHERE, "!ERROR %d (%s) creating directory: %s", i, strerror(i), scfg.temp_dir);
			cleanup(1);
			return;
		}
		lprintf(LOG_DEBUG, "Temporary file directory: %s", scfg.temp_dir);
		lprintf(LOG_DEBUG, "Root directory: %s", root_dir);
		lprintf(LOG_DEBUG, "Error directory: %s", error_dir);
		lprintf(LOG_DEBUG, "CGI directory: %s", cgi_dir);

		iniFileName(mime_types_ini, sizeof(mime_types_ini), scfg.ctrl_dir, "mime_types.ini");
		mime_types = read_ini_list(mime_types_ini, NULL /* root section */, "MIME types"
		                           , mime_types, true);
		iniFileName(web_handler_ini, sizeof(web_handler_ini), scfg.ctrl_dir, "web_handler.ini");
		if ((cgi_handlers = read_ini_list(web_handler_ini, "CGI." PLATFORM_DESC, "CGI content handlers"
		                                  , cgi_handlers, true)) == NULL)
			cgi_handlers = read_ini_list(web_handler_ini, "CGI", "CGI content handlers"
			                             , cgi_handlers, true);
		xjs_handlers = read_ini_list(web_handler_ini, "JavaScript", "JavaScript content handlers"
		                             , xjs_handlers, true);
		iniFileName(web_alias_ini, sizeof(web_alias_ini), scfg.ctrl_dir, "web_alias.ini");
		alias_list = read_ini_list(web_alias_ini, ROOT_SECTION, "Request path aliases", alias_list, false);
		if (!(startup->options & WEB_OPT_NO_FILEBASE))
			add_vshortcuts();

		/* Don't do this for *each* CGI request, just once here during [re]init */
		iniFileName(cgi_env_ini, sizeof(cgi_env_ini), scfg.ctrl_dir, "cgi_env.ini");
		if ((fp = iniOpenFile(cgi_env_ini, /* for_modify: */ false)) != NULL) {
			cgi_env = iniReadFile(fp);
			iniCloseFile(fp);
		}

		if (uptime == 0)
			uptime = time(NULL); /* this must be done *after* setting the timezone */

		update_clients();

		/* open a socket and wait for a client */
		ws_set = xpms_create(startup->bind_retry_count, startup->bind_retry_delay, lprintf);

		if (ws_set == NULL) {
			errprintf(LOG_CRIT, WHERE, "!ERROR %d creating HTTP socket set", SOCKET_ERRNO);
			cleanup(1);
			return;
		}
		lprintf(LOG_DEBUG, "Web Server socket set created");

		/*
		 * Add interfaces
		 */

		if (!(startup->options & WEB_OPT_NO_HTTP))
			xpms_add_list(ws_set, PF_UNSPEC, SOCK_STREAM, 0, startup->interfaces, startup->port, "Web Server", &terminate_server, open_socket, startup->seteuid, NULL);
		if (startup->options & WEB_OPT_ALLOW_TLS) {
			if (!ssl_sync(&scfg, lprintf))
				errprintf(LOG_CRIT, WHERE, "!ssl_sync() failure trying to enable TLS support");
			else
				xpms_add_list(ws_set, PF_UNSPEC, SOCK_STREAM, 0, startup->tls_interfaces, startup->tls_port, "Secure Web Server", &terminate_server, open_socket, startup->seteuid, (void*)"TLS");
		}

		request_rate_limiter->maxRequests = startup->max_requests_per_period;
		request_rate_limiter->timeWindowSeconds = startup->request_rate_limit_period;

		ip_can = new trashCan(&scfg, "ip");
		ip_silent_can = new trashCan(&scfg, "ip-silent");
		host_can = new trashCan(&scfg, "host");
		host_exempt = new filterFile(&scfg, strIpFilterExemptConfigFile);

		listInit(&log_list, /* flags */ LINK_LIST_MUTEX | LINK_LIST_SEMAPHORE);
		if (startup->options & WEB_OPT_HTTP_LOGGING) {
			/********************/
			/* Start log thread */
			/********************/
			http_logging_thread_running = true;
			(void)protected_uint32_adjust(&thread_count, 1);
			_beginthread(http_logging_thread, 0, startup->logfile_base);
		}

		/* Setup recycle/shutdown semaphore file lists */
		shutdown_semfiles = semfile_list_init(scfg.ctrl_dir, "shutdown", server_abbrev);
		pause_semfiles = semfile_list_init(scfg.ctrl_dir, "pause", server_abbrev);
		recycle_semfiles = semfile_list_init(scfg.ctrl_dir, "recycle", server_abbrev);
		semfile_list_add(&recycle_semfiles, startup->ini_fname);
		SAFEPRINTF(path, "%swebsrvr.rec", scfg.ctrl_dir); /* legacy */
		semfile_list_add(&recycle_semfiles, path);
		semfile_list_add(&recycle_semfiles, mime_types_ini);
		semfile_list_add(&recycle_semfiles, web_handler_ini);
		semfile_list_add(&recycle_semfiles, cgi_env_ini);
		semfile_list_add(&recycle_semfiles, web_alias_ini);
		if (!initialized) {
			initialized = time(NULL);
			semfile_list_check(&initialized, recycle_semfiles);
			semfile_list_check(&initialized, shutdown_semfiles);
		}

		lprintf(LOG_INFO, "Web Server thread started");
		mqtt_client_max(&mqtt, startup->max_clients);

		char rate_limit_report[512]{};
		time_t last_rate_limit_report = time(NULL);
		while (!terminate_server) {
			YIELD();
			/* check for re-cycle/shutdown/pause semaphores */
			if (!(startup->options & BBS_OPT_NO_RECYCLE)
			    && protected_uint32_value(thread_count)
			    <= (unsigned int)(2 /* web_server() and http_session_thread() */ + (http_logging_thread_running?1:0))) {
				if ((p = semfile_list_check(&initialized, recycle_semfiles)) != NULL) {
					lprintf(LOG_INFO, "Recycle semaphore file (%s) detected", p);
					if (session != NULL) {
						pthread_mutex_unlock(&session->struct_filled);
						session = NULL;
					}
					break;
				}
				if (startup->recycle_now == true) {
					lprintf(LOG_INFO, "Recycle semaphore signaled");
					startup->recycle_now = false;
					if (session != NULL) {
						pthread_mutex_unlock(&session->struct_filled);
						session = NULL;
					}
					break;
				}
			}
			if (((p = semfile_list_check(&initialized, shutdown_semfiles)) != NULL
			     && lprintf(LOG_INFO, "Shutdown semaphore file (%s) detected", p))
			    || (startup->shutdown_now == true
			        && lprintf(LOG_INFO, "Shutdown semaphore signaled"))) {
				startup->shutdown_now = false;
				terminate_server = true;
				if (session != NULL) {
					pthread_mutex_unlock(&session->struct_filled);
					session = NULL;
				}
				break;
			}
			if (((p = semfile_list_check(NULL, pause_semfiles)) != NULL
			     && lprintf(LOG_INFO, "Pause semaphore file (%s) detected", p))
			    || (startup->paused
			        && lprintf(LOG_INFO, "Pause semaphore signaled"))) {
				set_state(SERVER_PAUSED);
				SLEEP(startup->sem_chk_freq * 1000);
				continue;
			}
			if (startup->max_requests_per_period > 0 && startup->request_rate_limit_period > 0
				&& time(NULL) - last_rate_limit_report >= startup->sem_chk_freq) {
				last_rate_limit_report = time(NULL);
				request_rate_limiter->cleanup();
				size_t most_active_count = 0;
				std::string most_active = request_rate_limiter->most_active(&most_active_count);
				char str[sizeof rate_limit_report];
				char tmp[128];
				snprintf(str, sizeof str, "Rate limiting current: clients=%zu, requests=%zu, most-active=%s (%zu), highest: %s (%u) on %s, limited: %u, last: %s on %s (repeat: %u)"
					, request_rate_limiter->client_count(), request_rate_limiter->total(), most_active.c_str(), most_active_count
					, request_rate_limiter->currHighwater.client.c_str(), request_rate_limiter->currHighwater.count
					, timestr(&scfg, (time32_t)request_rate_limiter->currHighwater.time, logstr)
					, request_rate_limiter->disallowed.load()
					, request_rate_limiter->lastLimited.client.c_str(), timestr(&scfg, (time32_t)request_rate_limiter->lastLimited.time, tmp)
					, request_rate_limiter->repeat.load());
				if (strcmp(str, rate_limit_report) != 0) {
					SAFECOPY(rate_limit_report, str);
					lprintf(LOG_DEBUG, "%s", rate_limit_report);
				}
			}
			/* signal caller that we've started up successfully */
			set_state(SERVER_READY);

			/* Startup next session thread */
			if (session == NULL) {
				/* FREE()d at the start of the session thread */
				if ((session = (http_session_t*)malloc(sizeof(http_session_t))) == NULL) {
					errprintf(LOG_CRIT, WHERE, "!ERROR allocating %lu bytes of memory for http_session_t", (ulong)sizeof(http_session_t));
					continue;
				}
				memset(session, 0, sizeof(http_session_t));
				SAFECOPY(session->user.alias, unknown);
				session->socket = INVALID_SOCKET;
				/* Destroyed in http_session_thread */
				pthread_mutex_init(&session->struct_filled, NULL);
				pthread_mutex_lock(&session->struct_filled);
				(void)protected_uint32_adjust(&thread_count, 1);
				_beginthread(http_session_thread, 0, session);
			}

			/* now wait for connection */
			client_addr_len = sizeof(client_addr);
			client_socket = xpms_accept(ws_set, &client_addr, &client_addr_len, startup->sem_chk_freq * 1000,
			    (startup->options & BBS_OPT_HAPROXY_PROTO) ? XPMS_ACCEPT_FLAG_HAPROXY : XPMS_FLAGS_NONE,
			    &acc_type);

			if (client_socket == INVALID_SOCKET)
				continue;

			if (acc_type != NULL && !strcmp((const char*)acc_type, "TLS"))
				session->is_tls = true;

			if (startup->socket_open != NULL)
				startup->socket_open(startup->cbdata, true);

			inet_addrtop(&client_addr, host_ip, sizeof(host_ip));

			if (!host_exempt->listed(host_ip, nullptr)) {

				if (session->is_tls == false && startup->max_concurrent_connections > 0) {
					int ip_len = strlen(host_ip) + 1;
					uint connections = listCountMatches(&current_connections, host_ip, ip_len);
					if(connections >= startup->max_concurrent_connections) {
						lprintf(LOG_NOTICE, "%04d HTTP [%s] !Maximum concurrent connections (%u) exceeded"
							,client_socket, host_ip, startup->max_concurrent_connections);
						static int len_429;
						if (len_429 < 1)
							len_429 = strlen(error_429);
						sendsocket(client_socket, error_429, len_429);
						close_socket(&client_socket);
						continue;
					}
				}

				if (ip_silent_can->listed(host_ip)) {
					close_socket(&client_socket);
					continue;
				}
			}

			uint32_t client_count = protected_uint32_value(active_clients);
			uint32_t threshold = startup->max_clients;
			if (session->is_tls) // Successfully sending a 503 error over TLS requires a session_thread
				threshold += 10; // so allow some extra clients/threads in that case
			if (startup->max_clients && client_count >= threshold) {
				lprintf(LOG_WARNING, "%04d [%s] !MAXIMUM CLIENTS (%u) %s (%u), access denied"
				        , client_socket, host_ip, startup->max_clients, client_count > startup->max_clients ? "exceeded" : "reached", client_count);
				if (!len_503)
					len_503 = strlen(error_503);
				if (session->is_tls == false && sendsocket(client_socket, error_503, len_503) != len_503)
					lprintf(LOG_WARNING, "%04d FAILED sending error 503", client_socket);
				close_socket(&client_socket);
				continue;
			}

			union xp_sockaddr local_addr;
			memset(&local_addr, 0, sizeof(local_addr));
			socklen_t         addr_len = sizeof(local_addr);
			if (getsockname(client_socket, (struct sockaddr *)&local_addr, &addr_len) != 0) {
				errprintf(LOG_CRIT, WHERE, "%04d %-5s !ERROR %d getting local address/port of socket"
				          , client_socket, session->is_tls ? "HTTPS":"HTTP", SOCKET_ERRNO);
				close_socket(&client_socket);
				continue;
			}
			char local_ip[INET6_ADDRSTRLEN];
			inet_addrtop(&local_addr, local_ip, sizeof local_ip);

			host_port = inet_addrport(&client_addr);

			lprintf(LOG_INFO, "%04d %-5s [%s] Connection accepted on %s port %u from port %u"
			        , client_socket
			        , session->is_tls ? "HTTPS":"HTTP"
			        , host_ip, local_ip, inet_addrport(&local_addr), host_port);

			SAFECOPY(session->host_ip, host_ip);
			memcpy(&session->addr, &client_addr, sizeof(session->addr));
			session->addr_len = client_addr_len;
			session->socket = client_socket;
			session->js_callback.auto_terminate = true;
			session->js_callback.terminated = &terminate_js;
			session->js_callback.limit = startup->js.time_limit;
			session->js_callback.gc_interval = startup->js.gc_interval;
			session->js_callback.yield_interval = startup->js.yield_interval;
			session->js_callback.events_supported = false;
			pthread_mutex_unlock(&session->struct_filled);
			session = NULL;
		}

		set_state(terminate_server ? SERVER_STOPPING : SERVER_RELOADING);

		if (session) {
			pthread_mutex_unlock(&session->struct_filled);
			session = NULL;
		}

		terminate_js = true;
		/* Wait for active clients to terminate */
		if (protected_uint32_value(active_clients)) {
			lprintf(LOG_INFO, "Waiting for %d active clients to disconnect..."
			        , protected_uint32_value(active_clients));
			start = time(NULL);
			while (protected_uint32_value(active_clients)) {
				if (time(NULL) - start > startup->max_inactivity) { // crash here Nov-14-2025, startup is invalid non-NULL pointer
					lprintf(LOG_WARNING, "!TIMEOUT waiting for %d active clients"
					        , protected_uint32_value(active_clients));
					break;
				}
				mswait(100);
			}
			lprintf(LOG_INFO, "Done waiting for active clients to disconnect");
		}

		if (http_logging_thread_running) {
			terminate_http_logging_thread = true;
			listSemPost(&log_list);
			mswait(100);
		}
		if (http_logging_thread_running) {
			lprintf(LOG_INFO, "Waiting for HTTP logging thread to terminate...");
			start = time(NULL);
			while (http_logging_thread_running) {
				if (time(NULL) - start > TIMEOUT_THREAD_WAIT) {
					lprintf(LOG_WARNING, "!TIMEOUT waiting for HTTP logging thread to "
					        "terminate");
					break;
				}
				listSemPost(&log_list);
				mswait(100);
			}
			lprintf(LOG_INFO, "Done waiting for HTTP logging thread to terminate");
		}

		if (!terminate_server) {
			lprintf(LOG_INFO, "Recycling server...");
			if (startup->recycle != NULL)
				startup->recycle(startup->cbdata);
		}
		cleanup(0);

	} while (!terminate_server);

	protected_uint32_destroy(thread_count);
	delete request_rate_limiter, request_rate_limiter = nullptr;
}
