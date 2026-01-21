/* Synchronet terminal server thread and related functions */

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

#include "sbbs.h"
#include "ident.h"
#include "telnet.h"
#include "netwrap.h"
#include "petdefs.h"
#include "filedat.h"
#include "js_rtpool.h"
#include "js_request.h"
#include "os_info.h"
#include "ssl.h"
#include "ver.h"
#include "eventwrap.h"
#include <multisock.h>
#include <limits.h>     // HOST_NAME_MAX

#ifdef __unix__
	#include <sys/un.h>
#endif

//#define SBBS_TELNET_ENVIRON_SUPPORT 1
//---------------------------------------------------------------------------

#define TELNET_SERVER "Synchronet Terminal Server"
static const char* server_abbrev = "term";

static const char* eventIniSection = "Events";
static ini_style_t eventIniStyle = {
	/* key_len: */ LEN_CODE
};
static ini_style_t qhubIniStyle = {
	/* key_len: */ LEN_QWKID
};
static const char* qhubIniSection = "QWK_NetworkHubs";

#define TIMEOUT_THREAD_WAIT     60          // Seconds (was 15)
#define IO_THREAD_BUF_SIZE      20000       // Bytes
#define TIMEOUT_MUTEX_FILE      12 * 60 * 60

#ifdef USE_CRYPTLIB

static protected_uint32_t ssh_sessions;

void ssh_session_destroy(SOCKET sock, CRYPT_SESSION session, int line)
{
	int result = cryptDestroySession(session);

	if (result != 0)
		lprintf(LOG_ERR, "%04d SSH ERROR %d destroying Cryptlib Session %d from line %d"
		        , sock, result, session, line);
	else {
		uint32_t remain = protected_uint32_adjust_fetch(&ssh_sessions, -1);
		lprintf(LOG_DEBUG, "%04d SSH Cryptlib Session: %d destroyed from line %d (%u remain)"
		        , sock, session, line, remain);
	}
}

	#define SSH_END(sock) do {                                      \
				if (ssh) {                                                   \
					pthread_mutex_lock(&sbbs->ssh_mutex);                   \
					ssh_session_destroy(sock, sbbs->ssh_session, __LINE__); \
					sbbs->ssh_mode = false;                                 \
					pthread_mutex_unlock(&sbbs->ssh_mutex);                 \
				}                                                           \
} while (0)
#else
	#define SSH_END(x)
#endif

volatile time_t            uptime = 0;
volatile uint              served = 0;

static std::atomic<time_t> event_thread_tick;
static bool                event_thread_blocked;
static protected_uint32_t  node_threads_running;
static volatile uint32_t   client_highwater = 0;

time_t                     laston_time;
char                       lastuseron[LEN_ALIAS + 1]; /* Name of user last online */
RingBuf*                   node_inbuf[MAX_NODES];
SOCKET                     spy_socket[MAX_NODES];
#ifdef __unix__
SOCKET                     uspy_socket[MAX_NODES]; /* UNIX domain spy sockets */
#endif
SOCKET                     node_socket[MAX_NODES];
struct xpms_set *          ts_set;
static scfg_t              scfg;
static struct mqtt         mqtt;
static char *              text[TOTAL_TEXT];
static scfg_t              node_scfg[MAX_NODES];
static char *              node_text[MAX_NODES][TOTAL_TEXT];
static WORD                first_node;
static WORD                last_node;
static bool                terminate_server = false;
static str_list_t          pause_semfiles;
static str_list_t          recycle_semfiles;
static str_list_t          shutdown_semfiles;
static str_list_t          clear_attempts_semfiles;
static link_list_t         current_logins;
static link_list_t         current_connections;
#ifdef _THREAD_SUID_BROKEN
int                        thread_suid_broken = true; /* NPTL is no longer broken */
#endif

/* convenient space-saving global variables */
extern "C" {
const char* crlf = "\r\n";
const char* nulstr = "";
};

#define GCES(status, node, sess, action) do {                          \
			char *GCES_estr;                                                    \
			int   GCES_level;                                                      \
			get_crypt_error_string(status, sess, &GCES_estr, action, &GCES_level); \
			if (GCES_estr) {                                                       \
				if (GCES_level < startup->ssh_error_level)                          \
				GCES_level = startup->ssh_error_level;                           \
				lprintf(GCES_level, "Node %d SSH %s from %s", node, GCES_estr, __FUNCTION__);             \
				free_crypt_attrstr(GCES_estr);                                       \
			}                                                                         \
} while (0)

#define GCESNN(status, sess, action) do {                              \
			char *GCES_estr;                                                    \
			int   GCES_level;                                                      \
			get_crypt_error_string(status, sess, &GCES_estr, action, &GCES_level); \
			if (GCES_estr) {                                                       \
				if (GCES_level < startup->ssh_error_level)                          \
				GCES_level = startup->ssh_error_level;                           \
				lprintf(GCES_level, "SSH %s from %s", GCES_estr, __FUNCTION__);     \
				free_crypt_attrstr(GCES_estr);                                       \
			}                                                                         \
} while (0)

#define GCESS(status, sock, sess, action) do {                         \
			char *GCES_estr;                                                    \
			int   GCES_level;                                                      \
			get_crypt_error_string(status, sess, &GCES_estr, action, &GCES_level); \
			if (GCES_estr) {                                                       \
				if (GCES_level < startup->ssh_error_level)                          \
				GCES_level = startup->ssh_error_level;                           \
				lprintf(GCES_level, "%04d SSH %s from %s", sock, GCES_estr, __FUNCTION__);                \
				free_crypt_attrstr(GCES_estr);                                       \
			}                                                                         \
} while (0)

#define GCESSTR(status, str, sess, action) do {                         \
			char *GCES_estr;                                                    \
			int   GCES_level;                                                      \
			get_crypt_error_string(status, sess, &GCES_estr, action, &GCES_level); \
			if (GCES_estr) {                                                       \
				if (GCES_level < startup->ssh_error_level)                          \
				GCES_level = startup->ssh_error_level;                           \
				lprintf(GCES_level, "%s SSH %s from %s (session %d)", str, GCES_estr, __FUNCTION__, sess);                \
				free_crypt_attrstr(GCES_estr);                                       \
			}                                                                         \
} while (0)


extern "C" {

static bbs_startup_t* startup = NULL;

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

static void update_clients()
{
	if (startup != NULL) {
		if (startup->clients != NULL)
			startup->clients(startup->cbdata, protected_uint32_value(node_threads_running));
	}
}

void client_on(SOCKET sock, client_t* client, BOOL update)
{
	if (!update)
		listAddNodeData(&current_connections, client->addr, strlen(client->addr) + 1, sock, LAST_NODE);
	if (startup != NULL && startup->client_on != NULL)
		startup->client_on(startup->cbdata, true, sock, client, update);
	mqtt_client_on(&mqtt, true, sock, client, update);
}

static void client_off(SOCKET sock)
{
	listRemoveTaggedNode(&current_connections, sock, /* free_data */ true);
	if (startup != NULL && startup->client_on != NULL)
		startup->client_on(startup->cbdata, false, sock, NULL, false);
	mqtt_client_on(&mqtt, false, sock, NULL, false);
}

static void thread_up(BOOL setuid)
{
	if (startup != NULL && startup->thread_up != NULL)
		startup->thread_up(startup->cbdata, true, setuid);
}

static void thread_down()
{
	if (startup != NULL && startup->thread_up != NULL)
		startup->thread_up(startup->cbdata, false, false);
}

int lputs(int level, const char* str)
{
	mqtt_lputs(&mqtt, TOPIC_SERVER, level, str);
	if (level <= LOG_ERR) {
		char errmsg[1024];
		SAFEPRINTF2(errmsg, "%-4s %s", server_abbrev, str);
		errorlog(&scfg, &mqtt, level, startup == NULL ? NULL:startup->host_name, errmsg);
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

int eputs(int level, const char *str)
{
	if (*str == 0)
		return 0;

	mqtt_lputs(&mqtt, TOPIC_HOST_EVENT, level, str);

	if (level <= LOG_ERR) {
		char errmsg[1024];
		SAFEPRINTF(errmsg, "evnt %s", str);
		errorlog(&scfg, &mqtt, level, startup == NULL ? NULL:startup->host_name, errmsg);
		if (startup != NULL && startup->errormsg != NULL)
			startup->errormsg(startup->cbdata, level, errmsg);
	}

	if (startup == NULL || startup->event_lputs == NULL || level > startup->event_log_level)
		return 0;

	return startup->event_lputs(startup->event_cbdata, level, str);
}

int lprintf(int level, const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);
	return lputs(level, sbuf);
}

DLLEXPORT uint repeated_error(int line, const char* function)
{
	static int         lastline;
	static const char* lastfunc;
	static uint        repeat_count;

	if (line == lastline && function == lastfunc)
		++repeat_count;
	else
		repeat_count = 0;
	lastline = line;
	lastfunc = function;
	return repeat_count;
}

int errprintf(int level, int line, const char* function, const char* file, const char *fmt, ...)
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

void
sbbs_t::log_crypt_error_status_sock(int status, const char *action)
{
	char *estr;
	int   level;
	if (cryptStatusError(status)) {
		get_crypt_error_string(status, ssh_session, &estr, action, &level);
		if (estr) {
			if (level < startup->ssh_error_level)
				level = startup->ssh_error_level;
			lprintf(level, "%04d SSH %s", client_socket.load(), estr);
			free_crypt_attrstr(estr);
		}
	}
}

/* Picks the right log callback function (event or term) based on the sbbs->cfg.node_num value */
/* Prepends the current node number and user alias (if applicable) */
int sbbs_t::lputs(int level, const char* str)
{
	char msg[2048];
	char prefix[32] = "";
	char user_str[64] = "";

	if (is_event_thread && event_code != NULL && *event_code)
		SAFEPRINTF(prefix, "%s ", event_code);
	else if (cfg.node_num && !is_event_thread)
		SAFEPRINTF(prefix, "Node %d ", cfg.node_num);
	else if (client_name[0])
		SAFEPRINTF(prefix, "%s ", client_name);
	if (useron.number)
		SAFEPRINTF(user_str, "<%s> ", useron.alias);
	SAFEPRINTF3(msg, "%s%s%s", prefix, user_str, str);
	strip_ctrl(msg, msg);
	if (is_event_thread) {
		if (level <= startup->event_log_level) {
			if (logfile_fp == nullptr) {
				char str[128];
				if(startup->first_node > 1)
					snprintf(str, sizeof str, "%u", startup->first_node);
				else
					str[0] = '\0';
				char path[MAX_PATH + 1];
				snprintf(path, sizeof path, "%sevents%s.log", cfg.logs_dir, str);
				logfile_fp = fopenlog(&cfg, path, /* shareable: */true);
			}
			if (logfile_fp != nullptr)
				fprintlog(&cfg, &logfile_fp, msg);
		}
		return ::eputs(level, msg);
	}
	return ::lputs(level, msg);
}

int sbbs_t::lprintf(int level, const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);
	return lputs(level, sbuf);
}

int sbbs_t::errprintf(int level, int line, const char* function, const char* file, const char* fmt, ...)
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

struct main_sock_cb_data {
	bbs_startup_t *startup;
	const char *protocol;
};

void sock_cb(SOCKET sock, void *cb_data)
{
	char                      error_str[256];
	struct main_sock_cb_data *cb = (struct main_sock_cb_data *)cb_data;

	if (cb->startup && cb->startup->socket_open)
		cb->startup->socket_open(cb->startup->cbdata, true);
	if (set_socket_options(&scfg, sock, cb->protocol, error_str, sizeof(error_str)))
		lprintf(LOG_ERR, "%04d !ERROR %s", sock, error_str);
}

void sock_close_cb(SOCKET sock, void *cb_data)
{
	bbs_startup_t *su = (bbs_startup_t *)cb_data;

	if (su && su->socket_open)
		su->socket_open(su->cbdata, false);
}

void call_socket_open_callback(bool open)
{
	if (startup != NULL && startup->socket_open != NULL)
		startup->socket_open(startup->cbdata, open);
}

SOCKET open_socket(int domain, int type, const char* protocol)
{
	SOCKET sock;
	char   error[256];

	sock = socket(domain, type, IPPROTO_IP);
	if (sock != INVALID_SOCKET)
		call_socket_open_callback(true);
	if (sock != INVALID_SOCKET && set_socket_options(&scfg, sock, protocol, error, sizeof(error)))
		lprintf(LOG_ERR, "%04d !ERROR %s", sock, error);

	return sock;
}

// Used by sbbs_t::ftp_put() and js_accept()
SOCKET accept_socket(SOCKET s, union xp_sockaddr* addr, socklen_t* addrlen)
{
	SOCKET sock;

	sock = accept(s, &addr->addr, addrlen);
	if (sock != INVALID_SOCKET)
		call_socket_open_callback(true);

	return sock;
}

int close_socket(SOCKET sock)
{
	int result;

	if (sock == INVALID_SOCKET || sock == 0)
		return 0;

	shutdown(sock, SHUT_RDWR);   /* required on Unix */
	result = closesocket(sock);
	call_socket_open_callback(false);
	if (result != 0 && SOCKET_ERRNO != ENOTSOCK)
		lprintf(LOG_WARNING, "!ERROR %d closing socket %d", SOCKET_ERRNO, sock);
	return result;
}

/* TODO: IPv6 */
in_addr_t resolve_ip(char *addr)
{
	char*    p;
	struct addrinfo* res;
	struct addrinfo hints = {0};
	in_addr_t ipa = INADDR_NONE;

	if (*addr == 0)
		return INADDR_NONE;

	for (p = addr; *p; p++)
		if (*p != '.' && !IS_DIGIT(*p))
			break;
	if (!(*p))
		return parseIPv4Address(addr);

	hints.ai_family = AF_INET;
	if (getaddrinfo(addr, NULL, &hints, &res) != 0)
		return INADDR_NONE;

	if (res->ai_family == AF_INET)
		ipa = ((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
	freeaddrinfo(res);
	return ipa;
}

} /* extern "C" */

#ifdef _WINSOCKAPI_

WSADATA     WSAData;
#define SOCKLIB_DESC WSAData.szDescription
static BOOL WSAInitialized = false;

static BOOL winsock_startup(void)
{
	int status;                 /* Status Code */

	if ((status = WSAStartup(MAKEWORD(1, 1), &WSAData)) == 0) {
		lprintf(LOG_DEBUG, "%s %s", WSAData.szDescription, WSAData.szSystemStatus);
		WSAInitialized = true;
		return true;
	}

	lprintf(LOG_CRIT, "!WinSock startup ERROR %d", status);
	return false;
}

#else /* No WINSOCK */

#define winsock_startup()   (true)
#define SOCKLIB_DESC NULL

#endif

DLLEXPORT void sbbs_srand()
{
	DWORD seed;

	xp_randomize();
#if defined(HAS_DEV_RANDOM) && defined(RANDOM_DEV)
	int   rf, rd = 0;

	if ((rf = open(RANDOM_DEV, O_RDONLY | O_NONBLOCK)) != -1) {
		rd = read(rf, &seed, sizeof(seed));
		close(rf);
	}
	if (rd != sizeof(seed))
#endif
	seed = time32(NULL) ^ (uintmax_t)GetCurrentThreadId();

	srand(seed);
	sbbs_random(10);    /* Throw away first number */
}

int sbbs_random(int n)
{
	return xp_random(n);
}

#ifdef JAVASCRIPT

static js_server_props_t js_server_props;

void* js_GetClassPrivate(JSContext *cx, JSObject *obj, JSClass* cls)
{
	void *ret = JS_GetInstancePrivate(cx, obj, cls, NULL);

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 */
	if (ret == NULL)
		JS_ReportError(cx, "'%s' instance: No Private Data or Class Mismatch"
		               , cls == NULL ? "???" : cls->name);
	return ret;
}

/* Convert from Synchronet-specific jsSyncMethodSpec to JSAPI's JSFunctionSpec */

JSBool
js_DescribeSyncObject(JSContext* cx, JSObject* obj, const char* str, int ver)
{
	JSString* js_str = JS_NewStringCopyZ(cx, str);

	if (js_str == NULL)
		return JS_FALSE;

	if (ver < 10000)     /* auto convert 313 to 31300 */
		ver *= 100;

	return JS_DefineProperty(cx, obj, "_description"
	                         , STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_READONLY) != JS_FALSE
	       && JS_DefineProperty(cx, obj, "_ver"
	                            , INT_TO_JSVAL(ver), NULL, NULL, JSPROP_READONLY) != JS_FALSE;
}

JSBool
js_DescribeSyncConstructor(JSContext* cx, JSObject* obj, const char* str)
{
	JSString* js_str = JS_NewStringCopyZ(cx, str);

	if (js_str == NULL)
		return JS_FALSE;

	return JS_DefineProperty(cx, obj, "_constructor"
	                         , STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_READONLY);
}

#ifdef BUILD_JSDOCS

static const char* method_array_name = "_method_list";
static const char* property_array_name = "_property_list";

/*
 * from jsatom.c:
 * Keep this in sync with jspubtd.h -- an assertion below will insist that
 * its length match the JSType enum's JSTYPE_LIMIT limit value.
 */
static const char *js_type_str[] = {
	"void",         // changed from "undefined"
	"object",
	"function",
	"string",
	"number",
	"boolean",
	"null",
	"xml",
	"array",
	"alias",
	"undefined"
};

JSBool
js_DefineSyncProperties(JSContext *cx, JSObject *obj, jsSyncPropertySpec* props)
{
	uint      i;
	int       ver;
	jsval     val;
	JSString* js_str;
	JSObject* array;
	JSObject* prop;

	if ((array = JS_NewObject(cx, NULL, NULL, obj)) == NULL)
		return JS_FALSE;

	for (i = 0; props[i].name != NULL; ++i) {
		if (props[i].tinyid < 256 && props[i].tinyid > -129) {
			if (!JS_DefinePropertyWithTinyId(cx, obj, /* Never reserve any "slots" for properties */
			                                 props[i].name, props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags | JSPROP_SHARED))
				return JS_FALSE;
		}
		else {
			if (!JS_DefineProperty(cx, obj, props[i].name, JSVAL_VOID, NULL, NULL, props[i].flags | JSPROP_SHARED))
				return JS_FALSE;
		}
		if (!(props[i].flags & JSPROP_ENUMERATE))   /* No need to document invisible props */
			continue;

		prop = JS_NewObject(cx, NULL, NULL, array);
		if (prop == NULL)
			return JS_FALSE;

		if ((ver = props[i].ver) < 10000)      /* auto convert 313 to 31300 */
			ver *= 100;
		val = INT_TO_JSVAL(ver);
		JS_SetProperty(cx, prop, "ver", &val);

		if (props[i].desc != NULL) {
			if ((js_str = JS_NewStringCopyZ(cx, props[i].desc)) == NULL)
				return JS_FALSE;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, prop, "desc", &val);
		}
		if (!JS_DefineProperty(cx, array, props[i].name, OBJECT_TO_JSVAL(prop), NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE))
			return JS_FALSE;
	}

	return JS_DefineProperty(cx, obj, property_array_name, OBJECT_TO_JSVAL(array), NULL, NULL, JSPROP_READONLY);
}

JSBool
js_DefineSyncMethods(JSContext* cx, JSObject* obj, jsSyncMethodSpec *funcs)
{
	int       i;
	jsuint    len = 0;
	int       ver;
	jsval     val;
	JSObject* method;
	JSObject* method_array;
	JSString* js_str;
	size_t    str_len = 0;
	char *    str = NULL;

	/* Return existing method_list array if it's already been created */
	if (JS_GetProperty(cx, obj, method_array_name, &val) && val != JSVAL_VOID) {
		method_array = JSVAL_TO_OBJECT(val);
		// If the first item is already in the list, don't do anything.
		if (method_array == NULL || !JS_GetArrayLength(cx, method_array, &len))
			return JS_FALSE;
		for (i = 0; i < (int)len; i++) {
			if (JS_GetElement(cx, method_array, i, &val) != JS_TRUE || val == JSVAL_VOID)
				continue;
			JS_GetProperty(cx, JSVAL_TO_OBJECT(val), "name", &val);
			JSVALUE_TO_RASTRING(cx, val, str, &str_len, NULL);
			if (str == NULL)
				continue;
			if (strcmp(str, funcs[0].name) == 0)
				return JS_TRUE;
		}
	}
	else {
		if ((method_array = JS_NewArrayObject(cx, 0, NULL)) == NULL)
			return JS_FALSE;
		if (!JS_DefineProperty(cx, obj, method_array_name, OBJECT_TO_JSVAL(method_array)
		                       , NULL, NULL, 0))
			return JS_FALSE;
	}

	for (i = 0; funcs[i].name != NULL; ++i) {

		if (!JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0))
			return JS_FALSE;

		if (funcs[i].type == JSTYPE_ALIAS)
			continue;

		method = JS_NewObject(cx, NULL, NULL, method_array);
		if (method == NULL)
			return JS_FALSE;

		if (funcs[i].name != NULL) {
			if ((js_str = JS_NewStringCopyZ(cx, funcs[i].name)) == NULL)
				return JS_FALSE;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, method, "name", &val);
		}

		val = INT_TO_JSVAL(funcs[i].nargs);
		if (!JS_SetProperty(cx, method, "nargs", &val))
			return JS_FALSE;

		if ((js_str = JS_NewStringCopyZ(cx, js_type_str[funcs[i].type])) == NULL)
			return JS_FALSE;
		val = STRING_TO_JSVAL(js_str);
		JS_SetProperty(cx, method, "type", &val);

		if (funcs[i].args != NULL) {
			if ((js_str = JS_NewStringCopyZ(cx, funcs[i].args)) == NULL)
				return JS_FALSE;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, method, "args", &val);
		}

		if (funcs[i].desc != NULL) {
			if ((js_str = JS_NewStringCopyZ(cx, funcs[i].desc)) == NULL)
				return JS_FALSE;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, method, "desc", &val);
		}

		if (funcs[i].ver) {
			if ((ver = funcs[i].ver) < 10000)      /* auto convert 313 to 31300 */
				ver *= 100;
			val = INT_TO_JSVAL(ver);
			JS_SetProperty(cx, method, "ver", &val);
		}

		val = OBJECT_TO_JSVAL(method);
		if (!JS_SetElement(cx, method_array, len + i, &val))
			return JS_FALSE;
	}

	return JS_TRUE;
}

/*
 * Always resolve all here since
 * 1) We'll always be enumerating anyways
 * 2) The speed penalty won't be seen in production code anyways
 */
JSBool
js_SyncResolve(JSContext* cx, JSObject* obj, char *name, jsSyncPropertySpec* props, jsSyncMethodSpec* funcs, jsConstIntSpec* consts, int flags)
{
	JSBool ret = JS_TRUE;

	if (props) {
		if (!js_DefineSyncProperties(cx, obj, props))
			ret = JS_FALSE;
	}

	if (funcs) {
		if (!js_DefineSyncMethods(cx, obj, funcs))
			ret = JS_FALSE;
	}

	if (consts) {
		if (!js_DefineConstIntegers(cx, obj, consts, flags))
			ret = JS_FALSE;
	}

	return ret;
}

#else // NON-JSDOCS

JSBool
js_DefineSyncProperties(JSContext *cx, JSObject *obj, jsSyncPropertySpec* props)
{
	uint i;

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 */
	for (i = 0; props[i].name; i++) {
		if (props[i].tinyid < 256 && props[i].tinyid > -129) {
			if (!JS_DefinePropertyWithTinyId(cx, obj,
			                                 props[i].name, props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags | JSPROP_SHARED))
				return JS_FALSE;
		}
		else {
			if (!JS_DefineProperty(cx, obj, props[i].name, JSVAL_VOID, NULL, NULL, props[i].flags | JSPROP_SHARED))
				return JS_FALSE;
		}
	}

	return JS_TRUE;
}


JSBool
js_DefineSyncMethods(JSContext* cx, JSObject* obj, jsSyncMethodSpec *funcs)
{
	uint i;

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 */
	for (i = 0; funcs[i].name; i++)
		if (!JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0))
			return JS_FALSE;
	return JS_TRUE;
}

JSBool
js_SyncResolve(JSContext* cx, JSObject* obj, char *name, jsSyncPropertySpec* props, jsSyncMethodSpec* funcs, jsConstIntSpec* consts, int flags)
{
	uint  i;
	jsval val;

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 */
	if (props) {
		for (i = 0; props[i].name; i++) {
			if (name == NULL || strcmp(name, props[i].name) == 0) {
				if (props[i].tinyid < 256 && props[i].tinyid > -129) {
					if (!JS_DefinePropertyWithTinyId(cx, obj,
					                                 props[i].name, props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags | JSPROP_SHARED))
						return JS_FALSE;
				}
				else {
					if (!JS_DefineProperty(cx, obj, props[i].name, JSVAL_VOID, NULL, NULL, props[i].flags | JSPROP_SHARED))
						return JS_FALSE;
				}
				if (name)
					return JS_TRUE;
			}
		}
	}
	if (funcs) {
		for (i = 0; funcs[i].name; i++) {
			if (name == NULL || strcmp(name, funcs[i].name) == 0) {
				if (!JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0))
					return JS_FALSE;
				if (name)
					return JS_TRUE;
			}
		}
	}
	if (consts) {
		for (i = 0; consts[i].name; i++) {
			if (name == NULL || strcmp(name, consts[i].name) == 0) {
				val = INT_TO_JSVAL(consts[i].val);

				if (!JS_DefineProperty(cx, obj, consts[i].name, val, NULL, NULL, flags))
					return JS_FALSE;

				if (name)
					return JS_TRUE;
			}
		}
	}

	return JS_TRUE;
}

#endif

/* This is a stream-lined version of JS_DefineConstDoubles */
JSBool
js_DefineConstIntegers(JSContext* cx, JSObject* obj, jsConstIntSpec* ints, int flags)
{
	uint  i;
	jsval val;

	for (i = 0; ints[i].name; i++) {
		val = INT_TO_JSVAL(ints[i].val);

		if (!JS_DefineProperty(cx, obj, ints[i].name, val, NULL, NULL, flags))
			return JS_FALSE;
	}

	return JS_TRUE;
}

static JSBool
js_log(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	uintN      i = 0;
	int32      level = LOG_INFO;
	JSString*  str = NULL;
	sbbs_t*    sbbs;
	jsrefcount rc;
	char *     line = NULL;
	size_t     line_sz = 0;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	if (argc > 1 && JSVAL_IS_NUMBER(argv[i])) {
		if (!JS_ValueToInt32(cx, argv[i++], &level))
			return JS_FALSE;
	}

	for (; i < argc; i++) {
		if ((str = JS_ValueToString(cx, argv[i])) == NULL) {
			FREE_AND_NULL(line);
			return JS_FALSE;
		}
		JSSTRING_TO_RASTRING(cx, str, line, &line_sz, NULL);
		if (line == NULL)
			return JS_FALSE;
		rc = JS_SUSPENDREQUEST(cx);
		sbbs->lputs(level, line);
		JS_RESUMEREQUEST(cx, rc);
	}
	if (line != NULL)
		free(line);

	if (str == NULL)
		JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	else
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	return JS_TRUE;
}

static JSBool
js_read(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	uchar*     buf;
	int32      len = 128;
	sbbs_t*    sbbs;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	if (argc) {
		if (js_argvIsNullOrVoid(cx, argv, 0))
			return JS_FALSE;
		if (!JS_ValueToInt32(cx, argv[0], &len))
			return JS_FALSE;
	}

	if ((buf = (uchar*)malloc(len)) == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	len = RingBufRead(&sbbs->inbuf, buf, len);
	JS_RESUMEREQUEST(cx, rc);

	if (len > 0)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyN(cx, (char*)buf, len)));

	free(buf);
	return JS_TRUE;
}

static JSBool
js_readln(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      buf;
	int32      len = 128;
	sbbs_t*    sbbs;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	if (argc) {
		if (js_argvIsNullOrVoid(cx, argv, 0))
			return JS_FALSE;
		if (!JS_ValueToInt32(cx, argv[0], &len))
			return JS_FALSE;
	}

	if (len > 0) {
		if ((buf = (char*)malloc(len)) == NULL)
			return JS_TRUE;

		rc = JS_SUSPENDREQUEST(cx);
		len = sbbs->getstr(buf, len, K_NONE);
		JS_RESUMEREQUEST(cx, rc);

		if (len > 0)
			JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buf)));

		free(buf);
	}
	return JS_TRUE;
}

static JSBool
js_write(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	uintN      i;
	JSString*  str = NULL;
	sbbs_t*    sbbs;
	jsrefcount rc;
	char *     cstr = NULL;
	size_t     cstr_sz = 0;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	for (i = 0; i < argc; i++) {
		if ((str = JS_ValueToString(cx, argv[i])) == NULL) {
			FREE_AND_NULL(cstr);
			return JS_FALSE;
		}
		JSSTRING_TO_RASTRING(cx, str, cstr, &cstr_sz, NULL);
		if (cstr == NULL)
			return JS_FALSE;
		rc = JS_SUSPENDREQUEST(cx);
		if (sbbs->online != ON_REMOTE)
			sbbs->lputs(LOG_INFO, cstr);
		else
			sbbs->bputs(cstr);
		JS_RESUMEREQUEST(cx, rc);
	}
	FREE_AND_NULL(cstr);

	return JS_TRUE;
}

static JSBool
js_write_raw(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	uintN      i;
	char*      str = NULL;
	size_t     str_sz = 0;
	size_t     len;
	sbbs_t*    sbbs;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	for (i = 0; i < argc; i++) {
		JSVALUE_TO_RASTRING(cx, argv[i], str, &str_sz, &len);
		if (str == NULL)
			return JS_FALSE;
		if (len < 1)
			continue;
		rc = JS_SUSPENDREQUEST(cx);
		sbbs->putcom(str, len);
		JS_RESUMEREQUEST(cx, rc);
	}
	if (str != NULL)
		free(str);

	return JS_TRUE;
}

static JSBool
js_writeln(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*    sbbs;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	js_write(cx, argc, arglist);
	rc = JS_SUSPENDREQUEST(cx);
	if (sbbs->online == ON_REMOTE)
		sbbs->bputs(crlf);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_printf(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p;
	sbbs_t*    sbbs;
	jsrefcount rc;

	if (argc < 1) {
		JS_SET_RVAL(cx, arglist, JS_GetEmptyStringValue(cx));
		return JS_TRUE;
	}

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	if ((p = js_sprintf(cx, 0, argc, argv)) == NULL) {
		JS_ReportError(cx, "js_sprintf failed");
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (sbbs->online != ON_REMOTE)
		sbbs->lputs(LOG_INFO, p);
	else
		sbbs->bputs(p);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p)));

	js_sprintf_free(p);

	return JS_TRUE;
}

static JSBool
js_alert(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	sbbs_t*    sbbs;
	jsrefcount rc;
	char *     cstr;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], cstr, NULL);
	if (cstr == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	if (sbbs->online != ON_REMOTE)
		sbbs->lputs(LOG_WARNING, cstr);
	else {
		sbbs->attr(sbbs->cfg.color[clr_err]);
		sbbs->bputs(cstr);
		sbbs->attr(LIGHTGRAY);
		sbbs->bputs(crlf);
	}
	free(cstr);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_confirm(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	sbbs_t*    sbbs;
	jsrefcount rc;
	char *     cstr;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], cstr, NULL);
	if (cstr == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->yesno(cstr)));
	free(cstr);
	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}

static JSBool
js_deny(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	sbbs_t*    sbbs;
	jsrefcount rc;
	char *     cstr;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], cstr, NULL);
	if (cstr == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->noyes(cstr)));
	free(cstr);
	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}


static JSBool
js_prompt(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char       instr[128] = "";
	JSString * str;
	sbbs_t*    sbbs;
	jsrefcount rc;
	char*      prompt = NULL;
	int32      mode = K_EDIT;
	size_t     result;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	uintN argn = 0;
	if (argc > argn && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], prompt, NULL);
		if (prompt == NULL)
			return JS_FALSE;
		argn++;
	}
	if (argc > argn && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_STRBUF(cx, argv[argn], instr, sizeof(instr), NULL);
		argn++;
	}
	if (argc > argn && JSVAL_IS_NUMBER(argv[argn])) {
		if (!JS_ValueToInt32(cx, argv[argn], &mode)) {
			free(prompt);
			return JS_FALSE;
		}
		argn++;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (prompt != NULL) {
		sbbs->bprintf("\1n\1y\1h%s\1w: ", prompt);
		free(prompt);
	}

	result = sbbs->getstr(instr, sizeof(instr) - 1, mode);
	sbbs->attr(LIGHTGRAY);
	if (!result) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}
	JS_RESUMEREQUEST(cx, rc);

	if ((str = JS_NewStringCopyZ(cx, instr)) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	return JS_TRUE;
}

static jsSyncMethodSpec js_global_functions[] = {
	{"log",             js_log,             1,  JSTYPE_STRING,  JSDOCSTR("[<i>number</i> level=LOG_INFO,] value [,value]")
	 , JSDOCSTR("Add a line of text to the server and/or system log.<br>"
		        "<i>values</i> are typically string constants or variables (each logged as a separate log message),<br>"
		        "<i>level</i> is the severity of the message to be logged, one of the globally-defined values, in decreasing severity:<br>"
		        "<tt>LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR, LOG_WARNING, LOG_NOTICE, LOG_INFO, and LOG_DEBUG</tt> (default: <tt>LOG_INFO</tt>)")
	 , 311
	},
	{"read",            js_read,            0,  JSTYPE_STRING,  JSDOCSTR("[count=128]")
	 , JSDOCSTR("Read up to <tt>count</tt> characters from input stream and return as a string or <tt>undefined</tt> upon error")
	 , 311
	},
	{"readln",          js_readln,          0,  JSTYPE_STRING,  JSDOCSTR("[count=128]")
	 , JSDOCSTR("Read a single line, up to <tt>count</tt> characters, from input stream and return as a string or <tt>undefined</tt> upon error")
	 , 311
	},
	{"write",           js_write,           0,  JSTYPE_VOID,    JSDOCSTR("value [,value]")
	 , JSDOCSTR("Send one or more values (typically strings) to the output stream")
	 , 311
	},
	{"write_raw",       js_write_raw,           0,  JSTYPE_VOID,    JSDOCSTR("value [,value]")
	 , JSDOCSTR("Send a stream of bytes (possibly containing NUL or special control code sequences) to the output stream")
	 , 314
	},
	{"print",           js_writeln,         0,  JSTYPE_ALIAS },
	{"writeln",         js_writeln,         0,  JSTYPE_VOID,    JSDOCSTR("value [,value]")
	 , JSDOCSTR("Send a line of text to the output stream with automatic line termination (CRLF), "
		        "<i>values</i> are typically string constants or variables (AKA print)")
	 , 311
	},
	{"printf",          js_printf,          1,  JSTYPE_STRING,  JSDOCSTR("<i>string</i> format [,value][,value]")
	 , JSDOCSTR("Send a C-style formatted string of text to the output stream.  See also the <tt>format()</tt> function.")
	 , 310
	},
	{"alert",           js_alert,           1,  JSTYPE_VOID,    JSDOCSTR("value")
	 , JSDOCSTR("Send an alert message (ala client-side JS) to the output stream")
	 , 310
	},
	{"prompt",          js_prompt,          1,  JSTYPE_STRING,  JSDOCSTR("[<i>string</i> text] [,<i>string</i> value] [,<i>number</i> k_mode=K_EDIT]")
	 , JSDOCSTR("Display a prompt (<tt>text</tt>) and return a string of user input (ala client-side JS) or <tt>null</tt> upon no-input<br>"
		        "<tt>value</tt> is an optional default string to be edited (used with the <tt>k_mode K_EDIT</tt> flag)<br>"
		        "See <tt>sbbsdefs.js</tt> for all valid <tt>K_</tt> (keyboard-input) mode flags.")
	 , 310
	},
	{"confirm",         js_confirm,         1,  JSTYPE_BOOLEAN, JSDOCSTR("value")
	 , JSDOCSTR("Display a Yes/No prompt and return <tt>true</tt> or <tt>false</tt> "
		        "based on user's confirmation (ala client-side JS, <tt>true</tt> = yes)<br>"
		        "see also <tt>console.yesno()<tt>")
	 , 310
	},
	{"deny",            js_deny,            1,  JSTYPE_BOOLEAN, JSDOCSTR("value")
	 , JSDOCSTR("Display a No/Yes prompt and returns <tt>true</tt> or <tt>false</tt> "
		        "based on user's denial (<tt>true</tt> = no)<br>"
		        "see also <tt>console.noyes()<tt>")
	 , 31501
	},
	{0}
};

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	char        line[64];
	char        file[MAX_PATH + 1];
	sbbs_t*     sbbs;
	const char* warning;
	jsrefcount  rc;
	int         log_level;

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return;

	if (report == NULL) {
		sbbs->lprintf(LOG_ERR, "!JavaScript: %s", message);
		return;
	}

	if (report->filename)
		SAFEPRINTF(file, " %s", report->filename);
	else
		file[0] = 0;

	if (report->lineno)
		SAFEPRINTF(line, " line %d", report->lineno);
	else
		line[0] = 0;

	if (JSREPORT_IS_WARNING(report->flags)) {
		if (JSREPORT_IS_STRICT(report->flags))
			warning = "strict warning ";
		else
			warning = "warning ";
		log_level = LOG_WARNING;
	} else {
		warning = nulstr;
		log_level = LOG_ERR;
	}

	rc = JS_SUSPENDREQUEST(cx);
	sbbs->lprintf(log_level, "!JavaScript %s%s%s: %s", warning, file, line, message);
	if (sbbs->online == ON_REMOTE)
		sbbs->bprintf("!JavaScript %s%s%s: %s\r\n", warning, getfname(file), line, message);
	JS_RESUMEREQUEST(cx, rc);
}

JSContext* sbbs_t::js_init(JSRuntime** runtime, JSObject** glob, const char* desc)
{
	JSContext* js_cx;

	if (startup->js.max_bytes == 0)
		startup->js.max_bytes = JAVASCRIPT_MAX_BYTES;

	lprintf(LOG_DEBUG, "JavaScript: Creating %s runtime: %u bytes"
	        , desc, startup->js.max_bytes);
	if ((*runtime = jsrt_GetNew(startup->js.max_bytes, 1000, __FILE__, __LINE__)) == NULL) {
		lprintf(LOG_NOTICE, "Failed to created new JavaScript runtime");
		return NULL;
	}

	if ((js_cx = JS_NewContext(*runtime, JAVASCRIPT_CONTEXT_STACK)) == NULL) {
		lprintf(LOG_NOTICE, "Failed to create new JavaScript context");
		return NULL;
	}
	JS_SetOptions(js_cx, startup->js.options);
	JS_BEGINREQUEST(js_cx);

	memset(&js_callback, 0, sizeof(js_callback));
	js_callback.limit = startup->js.time_limit;
	js_callback.gc_interval = startup->js.gc_interval;
	js_callback.yield_interval = startup->js.yield_interval;
	js_callback.terminated = &terminated;
	js_callback.auto_terminate = true;
	js_callback.events_supported = true;

	bool success = false;
	bool rooted = false;

	do {

		JS_SetErrorReporter(js_cx, js_ErrorReporter);

		JS_SetContextPrivate(js_cx, this);  /* Store a pointer to sbbs_t instance */

		/* Global Objects (including system, js, client, Socket, MsgBase, File, User, etc. */
		if (!js_CreateCommonObjects(js_cx, &scfg, &cfg, js_global_functions
		                            , uptime, server_host_name(), SOCKLIB_DESC /* system */
		                            , &js_callback              /* js */
		                            , &startup->js
			                        , &useron
		                            , client_socket == INVALID_SOCKET ? NULL : &client, client_socket.load(), -1 /* client */
		                            , &js_server_props          /* server */
		                            , glob
		                            , mqtt
		                            ))
			break;
		rooted = true;

#ifdef BUILD_JSDOCS
		js_CreateUifcObject(js_cx, *glob);
		js_CreateConioObject(js_cx, *glob);
#endif

		/* BBS Object */
		if (js_CreateBbsObject(js_cx, *glob) == NULL)
			break;

		/* Console Object */
		if (js_CreateConsoleObject(js_cx, *glob) == NULL)
			break;

		success = true;

	} while (0);

	if (!success) {
		if (rooted)
			JS_RemoveObjectRoot(js_cx, glob);
		JS_ENDREQUEST(js_cx);
		JS_DestroyContext(js_cx);
		js_cx = NULL;
		return NULL;
	}
	else
		JS_ENDREQUEST(js_cx);

	return js_cx;
}

void sbbs_t::js_cleanup(void)
{
	/* Free Context */
	if (js_cx != NULL) {
		lprintf(LOG_DEBUG, "JavaScript: Destroying context");
		JS_BEGINREQUEST(js_cx);
		JS_RemoveObjectRoot(js_cx, &js_glob);
		JS_ENDREQUEST(js_cx);
		JS_DestroyContext(js_cx);
		js_cx = NULL;
	}

	if (js_runtime != NULL) {
		lprintf(LOG_DEBUG, "JavaScript: Destroying runtime");
		jsrt_Release(js_runtime);
		js_runtime = NULL;
	}

	if (js_hotkey_cx != NULL) {
		lprintf(LOG_DEBUG, "JavaScript: Destroying HotKey context");
		JS_BEGINREQUEST(js_hotkey_cx);
		JS_RemoveObjectRoot(js_hotkey_cx, &js_hotkey_glob);
		JS_ENDREQUEST(js_hotkey_cx);
		JS_DestroyContext(js_hotkey_cx);
		js_hotkey_cx = NULL;
	}

	if (js_hotkey_runtime != NULL) {
		lprintf(LOG_DEBUG, "JavaScript: Destroying HotKey runtime");
		jsrt_Release(js_hotkey_runtime);
		js_hotkey_runtime = NULL;
	}

}

bool sbbs_t::js_create_user_objects(JSContext* cx, JSObject* glob)
{
	bool result = false;
	if (cx != NULL) {
		JS_BEGINREQUEST(cx);
		if (!js_CreateUserObjects(cx, glob, &cfg, &useron, &client, startup == NULL ? NULL :startup->web_file_vpath_prefix, subscan, mqtt))
			errprintf(LOG_ERR, WHERE, "!JavaScript ERROR creating user objects");
		else
			result = true;
		JS_ENDREQUEST(cx);
	}
	return result;
}

bool js_CreateCommonObjects(JSContext* js_cx
                                       , scfg_t* cfg                /* common */
                                       , scfg_t* node_cfg           /* node-specific */
                                       , jsSyncMethodSpec* methods  /* global */
                                       , time_t uptime              /* system */
                                       , const char* host_name      /* system */
                                       , const char* socklib_desc   /* system */
                                       , js_callback_t* cb          /* js */
                                       , js_startup_t* js_startup   /* js */
                                       , user_t* user               /* user */
                                       , client_t* client           /* client */
                                       , SOCKET client_socket       /* client */
                                       , CRYPT_CONTEXT session      /* client */
                                       , js_server_props_t* props   /* server */
                                       , JSObject** glob
                                       , struct mqtt* mqtt
                                       )
{
	bool success = false;

	if (node_cfg == NULL)
		node_cfg = cfg;

	/* Global Object */
	if (!js_CreateGlobalObject(js_cx, node_cfg, methods, js_startup, glob))
		return false;

	do {
		/*
		 * NOTE: Where applicable, anything added here should also be added to
		 *       the same function in jsdoor.c (ie: anything not Synchronet
		 *       specific).
		 */

		/* System Object */
		if (js_CreateSystemObject(js_cx, *glob, node_cfg, uptime, host_name, socklib_desc, mqtt) == NULL)
			break;

		/* Internal JS Object */
		if (cb != NULL
		    && js_CreateInternalJsObject(js_cx, *glob, cb, js_startup) == NULL)
			break;

		/* Client Object */
		if (client != NULL
		    && js_CreateClientObject(js_cx, *glob, "client", client, client_socket, session) == NULL)
			break;

		/* Server */
		if (props != NULL
		    && js_CreateServerObject(js_cx, *glob, props) == NULL)
			break;

		/* Socket Class */
		if (js_CreateSocketClass(js_cx, *glob) == NULL)
			break;

		/* Queue Class */
		if (js_CreateQueueClass(js_cx, *glob) == NULL)
			break;

		/* MsgBase Class */
		if (js_CreateMsgBaseClass(js_cx, *glob) == NULL)
			break;

		/* FileBase Class */
		if (js_CreateFileBaseClass(js_cx, *glob) == NULL)
			break;

		/* File Class */
		if (js_CreateFileClass(js_cx, *glob) == NULL)
			break;

		/* Archive Class */
		if (js_CreateArchiveClass(js_cx, *glob, cfg->supported_archive_formats) == NULL)
			break;

		/* User class */
		if (js_CreateUserClass(js_cx, *glob) == NULL)
			break;

		/* COM Class */
		if (js_CreateCOMClass(js_cx, *glob) == NULL)
			break;

		/* CryptContext Class */
		if (js_CreateCryptContextClass(js_cx, *glob) == NULL)
			break;

		/* CryptKeyset Class */
		if (js_CreateCryptKeysetClass(js_cx, *glob) == NULL)
			break;

		/* CryptCert Class */
		if (js_CreateCryptCertClass(js_cx, *glob) == NULL)
			break;

#if defined USE_MOSQUITTO
		if (js_CreateMQTTClass(js_cx, *glob) == NULL)
			break;
#endif
		/* Area Objects */
		if (!js_CreateUserObjects(js_cx, *glob, cfg, user, client, startup == NULL ? NULL :startup->web_file_vpath_prefix, /* subscan: */ NULL, mqtt))
			break;

		success = true;
	} while (0);

	if (!success)
		JS_RemoveObjectRoot(js_cx, glob);

	return success;
}

#endif  /* JAVASCRIPT */

static BYTE* telnet_interpret(sbbs_t* sbbs, BYTE* inbuf, int inlen,
                              BYTE* outbuf, int& outlen)
{
	BYTE* first_iac = NULL;
	BYTE* first_cr = NULL;
	int   i;

	outlen = 0;

	if (inlen < 1) {
		return inbuf;  // no length? No interpretation
	}

	first_iac = (BYTE*)memchr(inbuf, TELNET_IAC, inlen);

	if (!(sbbs->telnet_mode & TELNET_MODE_GATE)
	    && sbbs->telnet_remote_option[TELNET_BINARY_TX] != TELNET_WILL
	    && !(sbbs->console & CON_RAW_IN)) {
		if (sbbs->telnet_last_rxch == CR)
			first_cr = inbuf;
		else
			first_cr = (BYTE*)memchr(inbuf, CR, inlen);
	}

	if (!sbbs->telnet_cmdlen) {
		if (first_iac == NULL && first_cr == NULL) {
			outlen = inlen;
			return inbuf;  // no interpretation needed
		}

		if (first_iac != NULL || first_cr != NULL) {
			if (first_iac != NULL && (first_cr == NULL || first_iac < first_cr))
				outlen = first_iac - inbuf;
			else
				outlen = first_cr - inbuf;
			memcpy(outbuf, inbuf, outlen);
		}
	}

	for (i = outlen; i < inlen; i++) {
		if (!(sbbs->telnet_mode & TELNET_MODE_GATE)
		    && sbbs->telnet_remote_option[TELNET_BINARY_TX] != TELNET_WILL
		    && !(sbbs->console & CON_RAW_IN)) {
			if (sbbs->telnet_last_rxch == CR
			    && (inbuf[i] == LF || inbuf[i] == 0)) { // CR/LF or CR/NUL, ignore 2nd char
#if 0 /* Debug CR/LF problems */
				lprintf(LOG_INFO, "Node %d CR/%02Xh detected and ignored"
				        , sbbs->cfg.node_num, inbuf[i]);
#endif
				sbbs->telnet_last_rxch = inbuf[i];
				continue;
			}
			sbbs->telnet_last_rxch = inbuf[i];
		}

		if (inbuf[i] == TELNET_IAC && sbbs->telnet_cmdlen == 1) { /* escaped 255 */
			sbbs->telnet_cmdlen = 0;
			outbuf[outlen++] = TELNET_IAC;
			continue;
		}
		if (inbuf[i] == TELNET_IAC || sbbs->telnet_cmdlen) {

			if (sbbs->telnet_cmdlen < sizeof(sbbs->telnet_cmd))
				sbbs->telnet_cmd[sbbs->telnet_cmdlen++] = inbuf[i];
			else {
				lprintf(LOG_WARNING, "Node %d telnet command (%d, %d) buffer limit reached (%u bytes)"
				        , sbbs->cfg.node_num, sbbs->telnet_cmd[1], sbbs->telnet_cmd[2], sbbs->telnet_cmdlen);
				sbbs->telnet_cmdlen = 0;
			}

			uchar command   = sbbs->telnet_cmd[1];
			uchar option    = sbbs->telnet_cmd[2];

			if (sbbs->telnet_cmdlen == 2 && command == TELNET_SE) {
				lprintf(LOG_WARNING, "Node %d unexpected telnet sub-negotiation END command"
				        , sbbs->cfg.node_num);
				sbbs->telnet_cmdlen = 0;
			}
			else if (sbbs->telnet_cmdlen >= 2 && command == TELNET_SB) {
				if (inbuf[i] == TELNET_SE
				    && sbbs->telnet_cmd[sbbs->telnet_cmdlen - 2] == TELNET_IAC) {
					sbbs->telnet_cmds_received++;
					if (startup->options & BBS_OPT_DEBUG_TELNET)
						lprintf(LOG_DEBUG, "Node %d %s telnet sub-negotiation command: %s (%u bytes)"
						        , sbbs->cfg.node_num
						        , sbbs->telnet_mode & TELNET_MODE_GATE ? "passed-through" : "received"
						        , telnet_opt_desc(option)
						        , sbbs->telnet_cmdlen);

					/* sub-option terminated */
					if (option == TELNET_TERM_TYPE
					    && sbbs->telnet_cmd[3] == TELNET_TERM_IS) {
						safe_snprintf(sbbs->telnet_terminal, sizeof(sbbs->telnet_terminal), "%.*s"
						              , (int)sbbs->telnet_cmdlen - 6, sbbs->telnet_cmd + 4);
						lprintf(LOG_DEBUG, "Node %d %s telnet terminal type: %s"
						        , sbbs->cfg.node_num
						        , sbbs->telnet_mode & TELNET_MODE_GATE ? "passed-through" : "received"
						        , sbbs->telnet_terminal);

					} else if (option == TELNET_TERM_SPEED
					           && sbbs->telnet_cmd[3] == TELNET_TERM_IS) {
						char speed[128];
						safe_snprintf(speed, sizeof(speed), "%.*s", (int)sbbs->telnet_cmdlen - 6, sbbs->telnet_cmd + 4);
						lprintf(LOG_DEBUG, "Node %d %s telnet terminal speed: %s"
						        , sbbs->cfg.node_num
						        , sbbs->telnet_mode & TELNET_MODE_GATE ? "passed-through" : "received"
						        , speed);
						sbbs->telnet_speed = atoi(speed);
#ifdef SBBS_TELNET_ENVIRON_SUPPORT
					} else if (option == TELNET_NEW_ENVIRON
					           && sbbs->telnet_cmd[3] == TELNET_ENVIRON_IS) {
						BYTE* p;
						BYTE* end = sbbs->telnet_cmd + (sbbs->telnet_cmdlen - 2);
						for (p = sbbs->telnet_cmd + 4; p < end; ) {
							if (*p == TELNET_ENVIRON_VAR || *p == TELNET_ENVIRON_USERVAR) {
								BYTE  type = *p++;
								char* name = (char*)p;
								/* RFC 1572: The characters following a "type" up to the next "type" or VALUE specify the variable name. */
								while (p < end) {
									if (*p == TELNET_ENVIRON_VAR || *p == TELNET_ENVIRON_USERVAR || *p == TELNET_ENVIRON_VALUE)
										break;
									p++;
								}
								if (p < end) {
									char* value = (char*)p + 1;
									*(p++) = 0;
									while (p < end) {
										if (*p == TELNET_ENVIRON_VAR || *p == TELNET_ENVIRON_USERVAR || *p == TELNET_ENVIRON_VALUE)
											break;
										p++;
									}
									*p = 0;
									lprintf(LOG_DEBUG, "Node %d telnet %s %s environment variable '%s' = '%s'"
									        , sbbs->cfg.node_num
									        , sbbs->telnet_mode & TELNET_MODE_GATE ? "passed-through" : "received"
									        , type == TELNET_ENVIRON_VAR ? "well-known" : "user-defined"
									        , name
									        , value);
									if (strcmp(name, "USER") == 0) {
										SAFECOPY(sbbs->rlogin_name, value);
									}
								}
							} else
								p++;
						}
#endif
					} else if (option == TELNET_SEND_LOCATION) {
						safe_snprintf(sbbs->telnet_location
						              , sizeof(sbbs->telnet_location)
						              , "%.*s", (int)sbbs->telnet_cmdlen - 5, sbbs->telnet_cmd + 3);
						lprintf(LOG_DEBUG, "Node %d %s telnet location: %s"
						        , sbbs->cfg.node_num
						        , sbbs->telnet_mode & TELNET_MODE_GATE ? "passed-through" : "received"
						        , sbbs->telnet_location);
					} else if (option == TELNET_TERM_LOCATION_NUMBER && sbbs->telnet_cmd[3] == 0) {
						SAFEPRINTF4(sbbs->telnet_location, "%u.%u.%u.%u"
						            , sbbs->telnet_cmd[4]
						            , sbbs->telnet_cmd[5]
						            , sbbs->telnet_cmd[6]
						            , sbbs->telnet_cmd[7]
						            );
						lprintf(LOG_DEBUG, "Node %d %s telnet location number (IP address): %s"
						        , sbbs->cfg.node_num
						        , sbbs->telnet_mode & TELNET_MODE_GATE ? "passed-through" : "received"
						        , sbbs->telnet_location);
					} else if (option == TELNET_NEGOTIATE_WINDOW_SIZE) {
						int cols = (sbbs->telnet_cmd[3] << 8) | sbbs->telnet_cmd[4];
						int rows = (sbbs->telnet_cmd[5] << 8) | sbbs->telnet_cmd[6];
						lprintf(LOG_DEBUG, "Node %d %s telnet window size: %dx%d"
						        , sbbs->cfg.node_num
						        , sbbs->telnet_mode & TELNET_MODE_GATE ? "passed-through" : "received"
						        , cols
						        , rows);
						sbbs->telnet_cols = cols;
						sbbs->telnet_rows = rows;
					} else if (startup->options & BBS_OPT_DEBUG_TELNET)
						lprintf(LOG_DEBUG, "Node %d %s unsupported telnet sub-negotiation cmd: %s, 0x%02X"
						        , sbbs->cfg.node_num
						        , sbbs->telnet_mode & TELNET_MODE_GATE ? "passed-through" : "received"
						        , telnet_opt_desc(option)
						        , sbbs->telnet_cmd[3]);
					sbbs->telnet_cmdlen = 0;
				}
			} // Sub-negotiation command
			else if (sbbs->telnet_cmdlen == 2 && inbuf[i] < TELNET_WILL) {
				sbbs->telnet_cmds_received++;
				if (startup->options & BBS_OPT_DEBUG_TELNET)
					lprintf(LOG_DEBUG, "Node %d %s telnet cmd: %s"
					        , sbbs->cfg.node_num
					        , sbbs->telnet_mode & TELNET_MODE_GATE ? "passed-through" : "received"
					        , telnet_cmd_desc(option));
				sbbs->telnet_cmdlen = 0;
			}
			else if (sbbs->telnet_cmdlen >= 3) {   /* telnet option negotiation */
				sbbs->telnet_cmds_received++;
				if (startup->options & BBS_OPT_DEBUG_TELNET)
					lprintf(LOG_DEBUG, "Node %d %s telnet cmd: %s %s"
					        , sbbs->cfg.node_num
					        , sbbs->telnet_mode & TELNET_MODE_GATE ? "passed-through" : "received"
					        , telnet_cmd_desc(command)
					        , telnet_opt_desc(option));

				if (!(sbbs->telnet_mode & TELNET_MODE_GATE)) {
					if (command == TELNET_DO || command == TELNET_DONT) {    /* local options */
						if (sbbs->telnet_local_option[option] == command)
							SetEvent(sbbs->telnet_ack_event);
						else {
							sbbs->telnet_local_option[option] = command;
							sbbs->send_telnet_cmd(telnet_opt_ack(command), option);
						}
					} else { /* WILL/WONT (remote options) */
						if (sbbs->telnet_remote_option[option] == command)
							SetEvent(sbbs->telnet_ack_event);
						else {
							switch (option) {
								case TELNET_BINARY_TX:
								case TELNET_ECHO:
								case TELNET_TERM_TYPE:
								case TELNET_TERM_SPEED:
								case TELNET_SUP_GA:
								case TELNET_NEGOTIATE_WINDOW_SIZE:
								case TELNET_SEND_LOCATION:
								case TELNET_TERM_LOCATION_NUMBER:
#ifdef SBBS_TELNET_ENVIRON_SUPPORT
								case TELNET_NEW_ENVIRON:
#endif
									sbbs->telnet_remote_option[option] = command;
									sbbs->send_telnet_cmd(telnet_opt_ack(command), option);
									break;
								default: /* unsupported remote options */
									if (command == TELNET_WILL) /* NAK */
										sbbs->send_telnet_cmd(telnet_opt_nak(command), option);
									break;
							}
						}

						if (command == TELNET_WILL && option == TELNET_TERM_TYPE) {
							if (startup->options & BBS_OPT_DEBUG_TELNET)
								lprintf(LOG_DEBUG, "Node %d requesting telnet terminal type"
								        , sbbs->cfg.node_num);

							char buf[64];
							snprintf(buf, sizeof buf, "%c%c%c%c%c%c"
							         , TELNET_IAC, TELNET_SB
							         , TELNET_TERM_TYPE, TELNET_TERM_SEND
							         , TELNET_IAC, TELNET_SE);
							sbbs->putcom(buf, 6);
						}
						else if (command == TELNET_WILL && option == TELNET_TERM_SPEED) {
							if (startup->options & BBS_OPT_DEBUG_TELNET)
								lprintf(LOG_DEBUG, "Node %d requesting telnet terminal speed"
								        , sbbs->cfg.node_num);

							char buf[64];
							snprintf(buf, sizeof buf, "%c%c%c%c%c%c"
							         , TELNET_IAC, TELNET_SB
							         , TELNET_TERM_SPEED, TELNET_TERM_SEND
							         , TELNET_IAC, TELNET_SE);
							sbbs->putcom(buf, 6);
						}
#ifdef SBBS_TELNET_ENVIRON_SUPPORT
						else if (command == TELNET_WILL && option == TELNET_NEW_ENVIRON) {
							if (startup->options & BBS_OPT_DEBUG_TELNET)
								lprintf(LOG_DEBUG, "Node %d requesting USER environment variable value"
								        , sbbs->cfg.node_num);

							char buf[64];
							int  len = snprintf(buf, sizeof buf, "%c%c%c%c%c%c"
							                    , TELNET_IAC, TELNET_SB
							                    , TELNET_NEW_ENVIRON, TELNET_ENVIRON_SEND //,TELNET_ENVIRON_VAR
							                    , TELNET_IAC, TELNET_SE);
							sbbs->putcom(buf, len);
						}
#endif
					}
				}
				sbbs->telnet_cmdlen = 0;
			}
			if (sbbs->telnet_mode & TELNET_MODE_GATE)  // Pass-through commands
				outbuf[outlen++] = inbuf[i];
		} else
			outbuf[outlen++] = inbuf[i];
	}
	return outbuf;
}

void sbbs_t::send_telnet_cmd(uchar cmd, uchar opt)
{
	char buf[16];
	int  sz;
	int  result;

	if (telnet_mode & TELNET_MODE_OFF)
		return;

	if (cmd < TELNET_WILL) {
		if (startup->options & BBS_OPT_DEBUG_TELNET)
			lprintf(LOG_DEBUG, "sending telnet cmd: %s"
			        , telnet_cmd_desc(cmd));
		snprintf(buf, sizeof buf, "%c%c", TELNET_IAC, cmd);
		result = sendsocket(client_socket, buf, sz = 2);
	} else {
		if (startup->options & BBS_OPT_DEBUG_TELNET)
			lprintf(LOG_DEBUG, "sending telnet cmd: %s %s"
			        , telnet_cmd_desc(cmd)
			        , telnet_opt_desc(opt));
		snprintf(buf, sizeof buf, "%c%c%c", TELNET_IAC, cmd, opt);
		result = sendsocket(client_socket, buf, sz = 3);
	}
	if (result != sz)
		lprintf(LOG_DEBUG, "ERROR sending telnet command (%s): send returned %d instead of %d"
		        , telnet_cmd_desc(cmd)
		        , result
		        , sz);
}

bool sbbs_t::request_telnet_opt(uchar cmd, uchar opt, unsigned waitforack)
{
	if (telnet_mode & TELNET_MODE_OFF)
		return false;

	switch (cmd) {
		case TELNET_DO: /* remote option */
		case TELNET_DONT:
			if (telnet_remote_option[opt] == telnet_opt_ack(cmd))
				return true;    /* already set in this mode, do nothing */
			telnet_remote_option[opt] = telnet_opt_ack(cmd);
			break;
		case TELNET_WILL: /* local option */
		case TELNET_WONT:
			if (telnet_local_option[opt] == telnet_opt_ack(cmd))
				return true;    /* already set in this mode, do nothing */
			telnet_local_option[opt] = telnet_opt_ack(cmd);
			break;
		default:
			return false;
	}
	if (waitforack)
		ResetEvent(telnet_ack_event);
	send_telnet_cmd(cmd, opt);
	if (waitforack)
		return WaitForEvent(telnet_ack_event, waitforack) == WAIT_OBJECT_0;
	return true;
}

static bool channel_open(sbbs_t *sbbs, int channel)
{
	int rval;

	if (cryptStatusError(cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sbbs->session_channel)))
		return false;
	if (cryptStatusError(cryptGetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_OPEN, &rval)))
		return false;
	return rval;
}

static int crypt_pop_channel_data(sbbs_t *sbbs, char *inbuf, int want, int *got)
{
	int   status;
	int   cid;
	char *cname = nullptr;
	char *ssname = nullptr;
	int   ret;
	int   closing_channel = -1;
	int   tgot;

	*got = 0;
	while (sbbs->online && sbbs->client_socket != INVALID_SOCKET
	       && node_socket[sbbs->cfg.node_num - 1] != INVALID_SOCKET) {
		ret = cryptPopData(sbbs->ssh_session, inbuf, want, &tgot);
		if (ret == CRYPT_OK) {
			status = cryptGetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, &cid);
			if (status == CRYPT_OK) {
				if (cid == closing_channel)
					continue;
				if (cid == sbbs->sftp_channel) {
					pthread_mutex_unlock(&sbbs->ssh_mutex);
					if (!sftps_recv(sbbs->sftp_state, reinterpret_cast<uint8_t *>(inbuf), tgot))
						sbbs->sftp_end();
					pthread_mutex_lock(&sbbs->ssh_mutex);
				}
				else if (cid == sbbs->session_channel) {
					*got = tgot;
				}
				else {
					if (cryptStatusError(status = cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, cid))) {
						sbbs->log_crypt_error_status_sock(status, "setting channel");
						return status;
					}
					cname = get_crypt_attribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_TYPE);
					if (cname && strcmp(cname, "subsystem") == 0) {
						ssname = get_crypt_attribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ARG1);
					}
					if (((startup->options & (BBS_OPT_ALLOW_SFTP | BBS_OPT_SSH_ANYAUTH)) == BBS_OPT_ALLOW_SFTP) && ssname && cname && sbbs->sftp_channel == -1 && strcmp(ssname, "sftp") == 0) {
						if (sbbs->init_sftp(cid)) {
							if (tgot > 0) {
								pthread_mutex_unlock(&sbbs->ssh_mutex);
								if (!sftps_recv(sbbs->sftp_state, reinterpret_cast<uint8_t *>(inbuf), tgot))
									sbbs->sftp_end();
								pthread_mutex_lock(&sbbs->ssh_mutex);
							}
							sbbs->sftp_channel = cid;
						}
					}
					if (cname && sbbs->session_channel == -1 && strcmp(cname, "shell") == 0) {
						sbbs->session_channel = cid;
					}

					if (cid != sbbs->sftp_channel && cid != sbbs->session_channel) {
						if (cname == nullptr || strcmp(cname, "session") != 0) {
							lprintf(LOG_WARNING, "Node %d SSH WARNING: attempt to use channel '%s' (%d != %d or %d)"
							        , sbbs->cfg.node_num, cname ? cname : "<unknown>", cid, sbbs->session_channel, sbbs->sftp_channel);
							if (cname) {
								free_crypt_attrstr(cname);
								cname = nullptr;
							}
							if (ssname) {
								free_crypt_attrstr(ssname);
								ssname = nullptr;
							}
							closing_channel = cid;
							if (cryptStatusError(status = cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 0))) {
								sbbs->log_crypt_error_status_sock(status, "closing channel");
								return status;
							}
						}
					}
					free_crypt_attrstr(cname);
					cname = nullptr;
					free_crypt_attrstr(ssname);
					ssname = nullptr;

					continue;
				}
				if (cid != -1) {
					if (cryptStatusOK(cryptGetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_OPEN, &ret))) {
						if (!ret) {
							closing_channel = -1;
							ret = CRYPT_ERROR_NOTFOUND;
						}
					}
				}
			}
			else {
				/*
				 * CRYPT_ERROR_NOTFOUND indicates this is the last data on the channel (whatever it was)
				 * and it was destroyed, so it's no longer possible to get the channel id.
				 */
				bool closed {false};
				if (status != CRYPT_ERROR_NOTFOUND)
					sbbs->log_crypt_error_status_sock(status, "getting channel id");
				closing_channel = -1;
				if (sbbs->sftp_channel != -1) {
					if (!channel_open(sbbs, sbbs->sftp_channel)) {
						if (cryptStatusOK(cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sbbs->sftp_channel)))
							cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 0);
						sbbs->sftp_channel = -1;
						closed = true;
					}
				}
				if (sbbs->session_channel != -1) {
					if (!channel_open(sbbs, sbbs->session_channel)) {
						if (cryptStatusOK(cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sbbs->session_channel)))
							cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 0);
						sbbs->session_channel = -1;
						closed = true;
					}
				}
				// All channels are now closed.
				if (closed && sbbs->sftp_channel == -1 && sbbs->session_channel == -1)
					return CRYPT_ERROR_COMPLETE;
			}
		}
		if (ret == CRYPT_ENVELOPE_RESOURCE)
			return CRYPT_ERROR_TIMEOUT;
		return ret;
	}
	return CRYPT_ERROR_TIMEOUT;
}

void input_thread(void *arg)
{
	BYTE          inbuf[4000];
	BYTE          telbuf[sizeof(inbuf)];
	BYTE *        wrbuf;
	int           i, wr, avail;
	uint64_t      total_recv = 0;
	uint          total_pkts = 0;
	sbbs_t*       sbbs = (sbbs_t*) arg;
	SOCKET        sock;
#ifdef PREFER_POLL
	struct pollfd fds[2];
	int           nfds;
#endif

	SetThreadName("sbbs/termInput");
	thread_up(true /* setuid */);

#ifdef _DEBUG
	lprintf(LOG_DEBUG, "Node %d input thread started", sbbs->cfg.node_num);
#endif

	while (sbbs->online && sbbs->client_socket != INVALID_SOCKET
	       && node_socket[sbbs->cfg.node_num - 1] != INVALID_SOCKET) {

		if (sbbs->max_socket_inactivity && !(sbbs->console & CON_NO_INACT)) {
			if (sbbs->cfg.inactivity_warn
			    && sbbs->socket_inactive >= sbbs->max_socket_inactivity * (sbbs->cfg.inactivity_warn / 100.0)
			    && !sbbs->socket_inactivity_warning_sent) {
				sbbs->bputs(text[InactivityAlert]);
				sbbs->socket_inactivity_warning_sent = true;
			}
			if (sbbs->socket_inactive > sbbs->max_socket_inactivity) {
				lprintf(LOG_NOTICE, "Node %d maximum socket inactivity exceeded: %u seconds"
				        , sbbs->cfg.node_num, sbbs->max_socket_inactivity.load());
				sbbs->bputs(text[CallBackWhenYoureThere]);
				break;
			}
		}

#ifdef _WIN32   // No spy sockets
		if (!socket_readable(sbbs->client_socket, 1000)) {
			++sbbs->socket_inactive;
			continue;
		}
#else
#ifdef PREFER_POLL
		fds[0].fd = sbbs->client_socket;
		fds[0].events = POLLIN;
		nfds = 1;
		if (uspy_socket[sbbs->cfg.node_num - 1] != INVALID_SOCKET) {
			fds[1].fd = uspy_socket[sbbs->cfg.node_num - 1];
			fds[1].events = POLLIN;
			nfds++;
		}

		if (poll(fds, nfds, 1000) < 1) {
			++sbbs->socket_inactive;
			continue;
		}
#else
#error Spy sockets without poll() was removed in commit 3971ef4dcc3db19f400a648b6110718e56a64cf3
#endif
#endif

		if (sbbs->client_socket == INVALID_SOCKET)
			break;

		sbbs->socket_inactive = 0;
		sbbs->socket_inactivity_warning_sent = false;

/*         ^          ^
 *      \______    ______/
 *       \  * \   / *   /
 *        -----   ------           /----\
 *              ||               -< Boo! |
 *             /__\                \----/
 *       \______________/
 *        \/\/\/\/\/\/\/
 *         ------------
 */

#ifdef _WIN32   // No spy sockets
		sock = sbbs->client_socket;
#else
#ifdef PREFER_POLL
		if (fds[0].revents & POLLIN)
			sock = sbbs->client_socket;
		else if (uspy_socket[sbbs->cfg.node_num - 1] != INVALID_SOCKET && fds[1].revents & POLLIN) {
			if (socket_recvdone(uspy_socket[sbbs->cfg.node_num - 1], 0)) {
				close_socket(uspy_socket[sbbs->cfg.node_num - 1]);
				lprintf(LOG_NOTICE, "Node %d Closing local spy socket: %d", sbbs->cfg.node_num, uspy_socket[sbbs->cfg.node_num - 1]);
				uspy_socket[sbbs->cfg.node_num - 1] = INVALID_SOCKET;
				continue;
			}
			sock = uspy_socket[sbbs->cfg.node_num - 1];
		}
		else {
			continue;
		}
#else
#error Spy sockets without poll() was removed in commit 3971ef4dcc3db19f400a648b6110718e56a64cf3
#endif
#endif
		avail = RingBufFree(&sbbs->inbuf);

		if (avail < 1) { // input buffer full
			lprintf(LOG_WARNING, "Node %d !WARNING input buffer full after receiving %" PRIu64 " bytes ", sbbs->cfg.node_num, total_recv);
			// wait up to 5 seconds to empty (1 byte min)
			time_t start = time(NULL);
			while ((avail = RingBufFree(&sbbs->inbuf)) < 1 && time(NULL) - start < 5) {
				YIELD();
			}
			if (avail < 1)   /* input buffer still full */
				continue;
		}

		if (avail > (int)sizeof inbuf)
			avail = sizeof inbuf;

		if (pthread_mutex_lock(&sbbs->input_thread_mutex) != 0)
			sbbs->errormsg(WHERE, ERR_LOCK, "input_thread_mutex", 0);

		int rd; // number of bytes read
#ifdef USE_CRYPTLIB
		if (sbbs->ssh_mode && sock == sbbs->client_socket) {
			int err;
			if (WaitForEvent(sbbs->ssh_active, 1000) == WAIT_TIMEOUT) {
				pthread_mutex_unlock(&sbbs->input_thread_mutex);
				continue;
			}
			pthread_mutex_lock(&sbbs->ssh_mutex);
			if (cryptStatusError((err = crypt_pop_channel_data(sbbs, (char*)inbuf, avail, &i)))) {
				pthread_mutex_unlock(&sbbs->ssh_mutex);
				if (pthread_mutex_unlock(&sbbs->input_thread_mutex) != 0)
					sbbs->errormsg(WHERE, ERR_UNLOCK, "input_thread_mutex", 0);
				if (err == CRYPT_ERROR_COMPLETE) {
					lprintf(LOG_WARNING, "Node %d SSH Operation complete", sbbs->cfg.node_num);
					break;
				}
				if (err == CRYPT_ERROR_TIMEOUT) {
					lprintf(LOG_NOTICE, "Node %d SSH Timeout", sbbs->cfg.node_num);
					continue;
				}
				/* Handle the SSH error here... */
				GCES(err, sbbs->cfg.node_num, sbbs->ssh_session, "popping data");
				break;
			}
			else {
				pthread_mutex_unlock(&sbbs->ssh_mutex);
				if (i < 1) {
					if (pthread_mutex_unlock(&sbbs->input_thread_mutex) != 0)
						sbbs->errormsg(WHERE, ERR_UNLOCK, "input_thread_mutex", 0);
					continue;
				}
				rd = i;
			}
		}
		else
#endif
			rd = recv(sock, (char*)inbuf, avail, 0);

		if (pthread_mutex_unlock(&sbbs->input_thread_mutex) != 0)
			sbbs->errormsg(WHERE, ERR_UNLOCK, "input_thread_mutex", 0);

		if (rd == 0 && !socket_recvdone(sock, 0))
			continue;

		if (rd == SOCKET_ERROR)
		{
#ifdef __unix__
			if (sock == sbbs->client_socket)  {
#endif
			if (!sbbs->online)      // sbbs_t::hangup() called?
				break;
			if (SOCKET_ERRNO == EAGAIN)
				continue;
			if (SOCKET_ERRNO == ENOTSOCK)
				lprintf(LOG_NOTICE, "Node %d socket closed by peer on receive", sbbs->cfg.node_num);
			else if (SOCKET_ERRNO == ECONNRESET)
				lprintf(LOG_NOTICE, "Node %d connection reset by peer on receive", sbbs->cfg.node_num);
			else if (SOCKET_ERRNO == ESHUTDOWN)
				lprintf(LOG_NOTICE, "Node %d socket shutdown on receive", sbbs->cfg.node_num);
			else if (SOCKET_ERRNO == ECONNABORTED)
				lprintf(LOG_NOTICE, "Node %d connection aborted by peer on receive", sbbs->cfg.node_num);
			else
				lprintf(LOG_WARNING, "Node %d !ERROR %d receiving from socket %d"
				        , sbbs->cfg.node_num, SOCKET_ERRNO, sock);
			break;
#ifdef __unix__
		} else  {
			if (SOCKET_ERRNO != EAGAIN)  {
				errprintf(LOG_ERR, WHERE, "Node %d !ERROR %d (%s) on local spy socket %d receive"
				          , sbbs->cfg.node_num, errno, strerror(errno), sock);
				close_socket(uspy_socket[sbbs->cfg.node_num - 1]);
				uspy_socket[sbbs->cfg.node_num - 1] = INVALID_SOCKET;
			}
			continue;
		}
#endif
		}

		if (rd == 0 && sock == sbbs->client_socket)
		{
			lprintf(LOG_NOTICE, "Node %d disconnected", sbbs->cfg.node_num);
			break;
		}

		total_recv += rd;
		total_pkts++;

		// telbuf and wr are modified to reflect telnet escaped data
		wr = rd;
#ifdef __unix__
		if (sock != sbbs->client_socket)
			wrbuf = inbuf;
		else
#endif
		if (sbbs->telnet_mode & TELNET_MODE_OFF)
			wrbuf = inbuf;
		else
			wrbuf = telnet_interpret(sbbs, inbuf, rd, telbuf, wr);
		if (wr > (int)sizeof(telbuf)) {
			errprintf(LOG_ERR, WHERE, "Node %d !TELBUF OVERFLOW (%d>%d)", sbbs->cfg.node_num, wr, (int)sizeof(telbuf));
			wr = sizeof(telbuf);
		}

		if (!(sbbs->console & CON_RAW_IN))
			sbbs->translate_input(wrbuf, wr);

		if (sbbs->passthru_socket_active == true) {
			bool writable = false;
			if (socket_check(sbbs->passthru_socket, NULL, &writable, 1000) && writable) {
				if (sendsocket(sbbs->passthru_socket, (char*)wrbuf, wr) != wr)
					errprintf(LOG_ERR, WHERE, "Node %d ERROR %d writing to passthru socket"
					          , sbbs->cfg.node_num, SOCKET_ERRNO);
			} else
				lprintf(LOG_WARNING, "Node %d could not write to passthru socket (writable=%d)"
				        , sbbs->cfg.node_num, (int)writable);
			continue;
		}

		/* First level Ctrl-C checking */
		if (!(sbbs->cfg.ctrlkey_passthru & (1 << CTRL_C))
		    && sbbs->rio_abortable
		    && !(sbbs->telnet_mode & TELNET_MODE_GATE)
		    && !(sbbs->sys_status & SS_FILEXFER)
		    && memchr(wrbuf, CTRL_C, wr)) {
			if (RingBufFull(&sbbs->inbuf))
				lprintf(LOG_DEBUG, "Node %d Ctrl-C hit with %u bytes in input buffer"
				        , sbbs->cfg.node_num, RingBufFull(&sbbs->inbuf));
			if (RingBufFull(&sbbs->outbuf))
				lprintf(LOG_DEBUG, "Node %d Ctrl-C hit with %u bytes in output buffer"
				        , sbbs->cfg.node_num, RingBufFull(&sbbs->outbuf));
			sbbs->sys_status |= SS_ABORT;
			RingBufReInit(&sbbs->inbuf);    /* Purge input buffer */
			RingBufReInit(&sbbs->outbuf);   /* Purge output buffer */
			SetEvent(sbbs->inbuf.data_event);
			SetEvent(sbbs->inbuf.highwater_event);
			continue;   // Ignore the entire buffer
		}

		avail = RingBufFree(&sbbs->inbuf);

		if (avail < wr)
			errprintf(LOG_ERR, WHERE, "Node %d !INPUT BUFFER FULL (%d free, %d bytes discarded)", sbbs->cfg.node_num, avail, wr);
		else
			RingBufWrite(&sbbs->inbuf, wrbuf, wr);
//		if(wr>100)
//			mswait(500);	// Throttle sender
	}
	sbbs->sftp_end();
	sbbs->online = false;
	sbbs->sys_status |= SS_ABORT; /* as though Ctrl-C were hit */
	SetEvent(sbbs->inbuf.data_event); // terminate incom() wait

	sbbs->input_thread_running = false;
	sbbs->terminate_output_thread = true;
	if (node_socket[sbbs->cfg.node_num - 1] == INVALID_SOCKET)   // Shutdown locally
		sbbs->terminated = true;    // Signal JS to stop execution

	thread_down();
	lprintf(LOG_DEBUG, "Node %d input thread terminated (received %" PRIu64 " bytes in %u blocks)"
	        , sbbs->cfg.node_num, total_recv, total_pkts);
}

// Flush the duplicate client_socket when activating the passthru socket
// to eliminate any stale data from the previous passthru session
void sbbs_t::passthru_socket_activate(bool activate)
{
	time_t timeout = time(NULL) + 60;

	if (activate) {
		bool rd = false;
		while (socket_check(client_socket_dup, &rd, /* wr_p */ NULL, /* timeout */ 0) && rd && time(NULL) < timeout) {
			char ch;
			if (recv(client_socket_dup, &ch, sizeof(ch), /* flags: */ 0) != sizeof(ch))
				break;
		}
	} else {
		/* Re-enable blocking (in case disabled by external program) */
		u_long l = 0;
		ioctlsocket(client_socket_dup, FIONBIO, &l);

		/* Re-set socket options */
		char err[512];
		if (set_socket_options(&cfg, client_socket_dup, "passthru", err, sizeof(err)))
			errprintf(LOG_ERR, WHERE, "%04d !ERROR %s setting passthru socket options", client_socket.load(), err);

		do { // Allow time for the passthru_thread to move any pending socket data to the outbuf
			SLEEP(100); // Before the node_thread starts sending its own data to the outbuf
		} while (RingBufFull(&outbuf) && time(NULL) < timeout);
	}
	passthru_socket_active = activate;
}

/*
 * This thread simply copies anything it manages to read from the
 * passthru_socket into the output ringbuffer.
 */
void passthru_thread(void* arg)
{
	sbbs_t  *sbbs = (sbbs_t*) arg;
	int rd;
	char inbuf[IO_THREAD_BUF_SIZE / 2];

	SetThreadName("sbbs/passthru");
	thread_up(false /* setuid */);
	lprintf(LOG_DEBUG, "Node %d passthru thread started", sbbs->cfg.node_num);

	while (sbbs->online && sbbs->passthru_socket != INVALID_SOCKET && !terminate_server) {
		if (!socket_readable(sbbs->passthru_socket, 1000))
			continue;

		if (sbbs->passthru_socket == INVALID_SOCKET)
			break;

		rd = RingBufFree(&sbbs->outbuf) / 2;
		if (rd > (int)sizeof(inbuf))
			rd = sizeof(inbuf);

		rd = recv(sbbs->passthru_socket, inbuf, rd, 0);

		if (rd == SOCKET_ERROR)
		{
			if (SOCKET_ERRNO == ENOTSOCK)
				lprintf(LOG_NOTICE, "Node %d passthru socket closed by peer on receive", sbbs->cfg.node_num);
			else if (SOCKET_ERRNO == ECONNRESET)
				lprintf(LOG_NOTICE, "Node %d passthru connection reset by peer on receive", sbbs->cfg.node_num);
			else if (SOCKET_ERRNO == ESHUTDOWN)
				lprintf(LOG_NOTICE, "Node %d passthru socket shutdown on receive", sbbs->cfg.node_num);
			else if (SOCKET_ERRNO == ECONNABORTED)
				lprintf(LOG_NOTICE, "Node %d passthru connection aborted by peer on receive", sbbs->cfg.node_num);
			else
				lprintf(LOG_WARNING, "Node %d !ERROR %d receiving from passthru socket %d"
				        , sbbs->cfg.node_num, SOCKET_ERRNO, sbbs->passthru_socket.load());
			break;
		}

		if (rd == 0)
		{
			char ch;
			if (recv(sbbs->passthru_socket, &ch, sizeof(ch), MSG_PEEK) == 0) {
				lprintf(sbbs->online ? LOG_WARNING : LOG_DEBUG
				        , "Node %d passthru socket disconnected", sbbs->cfg.node_num);
				break;
			}
			YIELD();
			continue;
		}

		if (sbbs->xtrn_mode & EX_BIN) {
			BYTE telnet_buf[sizeof(inbuf) * 2];
			BYTE*   bp = (BYTE*)inbuf;

			if (!(sbbs->telnet_mode & TELNET_MODE_OFF))
				rd = telnet_expand((BYTE*)inbuf, rd, telnet_buf, sizeof(telnet_buf), /* expand_cr */ false, &bp);

			int wr = RingBufWrite(&sbbs->outbuf, bp, rd);
			if (wr != rd) {
				errprintf(LOG_ERR, WHERE, "Node %d Short-write (%d of %d bytes) from passthru socket to outbuf"
				          , sbbs->cfg.node_num, wr, rd);
				break;
			}
		} else {
			sbbs->rputs(inbuf, rd);
		}
	}
	if (sbbs->passthru_socket != INVALID_SOCKET) {
		close_socket(sbbs->passthru_socket);
		sbbs->passthru_socket = INVALID_SOCKET;
	}
	thread_down();

	sbbs->passthru_thread_running = false;
	sbbs->passthru_socket_active = false;
	lprintf(LOG_DEBUG, "Node %d passthru thread terminated", sbbs->cfg.node_num);
}

void output_thread(void* arg)
{
	char node[128];
	char errmsg[128];
	char stats[128];
	char spy_topic[128];
	BYTE buf[IO_THREAD_BUF_SIZE];
	int i = 0;          // Assignment to silence Valgrind
	uint avail;
	uint total_sent = 0;
	uint total_pkts = 0;
	uint short_sends = 0;
	uint bufbot = 0;
	uint buftop = 0;
	sbbs_t*     sbbs = (sbbs_t*) arg;
	uint mss = IO_THREAD_BUF_SIZE;

	SetThreadName("sbbs/termOutput");
	thread_up(true /* setuid */);

	if (sbbs->cfg.node_num) {
		SAFEPRINTF(node, "Node %d", sbbs->cfg.node_num);
		SAFEPRINTF(spy_topic, "node/%d/output", sbbs->cfg.node_num);
	} else
		SAFECOPY(node, sbbs->client_name);
#ifdef _DEBUG
	lprintf(LOG_DEBUG, "%s output thread started", node);
#endif

#ifdef TCP_MAXSEG
	/*
	 * Auto-tune the highwater mark to be the negotiated MSS for the
	 * socket (when possible)
	 */
	socklen_t sl;
	sl = sizeof(i);
	if (!getsockopt(sbbs->client_socket, IPPROTO_TCP, TCP_MAXSEG,
#ifdef _WIN32
	                (char *)
#endif
	                &i, &sl)) {
		/* Check for sanity... */
		if (i > 100) {
#ifdef _WIN32
#ifdef TCP_TIMESTAMPS
			DWORD ts;
			sl = sizeof(ts);
			if (!getsockopt(sbbs->client_socket, IPPROTO_TCP, TCP_TIMESTAMPS, (char *)&ts, &sl)) {
				if (ts)
					i -= 12;
			}
#endif
#else
#if (defined(TCP_INFO) && defined(TCPI_OPT_TIMESTAMPS))
			struct tcp_info tcpi;

			sl = sizeof(tcpi);
			if (!getsockopt(sbbs->client_socket, IPPROTO_TCP, TCP_INFO, &tcpi, &sl)) {
				if (tcpi.tcpi_options & TCPI_OPT_TIMESTAMPS)
					i -= 12;
			}
#endif
#endif
			if (i > IO_THREAD_BUF_SIZE)
				i = IO_THREAD_BUF_SIZE;
			sbbs->outbuf.highwater_mark = i;
			mss = sbbs->outbuf.highwater_mark;
			lprintf(LOG_DEBUG, "%s outbuf highwater mark tuned to %d based on MSS", node, sbbs->outbuf.highwater_mark);
		}
	}
#endif
	sbbs->terminate_output_thread = false;

	/* Note: do not terminate when online==false, that is expected for the terminal server output_thread */
	while (sbbs->client_socket != INVALID_SOCKET && !terminate_server && !sbbs->terminate_output_thread) {
		/*
		 * I'd like to check the linear buffer against the highwater
		 * at this point, but it would get too clumsy imho - Deuce
		 *
		 * Actually, another option would just be to have the size
		 * of the linear buffer equal to the MSS... any larger and
		 * you could have small sends off the end.  this would
		 * probably be even clumsier
		 */
		if (bufbot == buftop) {
			/* Wait for something to output in the RingBuffer */
			if (WaitForEvent(sbbs->outbuf.data_event, 1000) != WAIT_OBJECT_0)
				continue;

			/* Wait for full buffer or drain timeout */
			if (sbbs->outbuf.highwater_mark)
				WaitForEvent(sbbs->outbuf.highwater_event, startup->outbuf_drain_timeout);

			/* Get fill level */
			avail = RingBufFull(&sbbs->outbuf);

			// If flushing or terminating, there will be nothing available
			if (avail == 0) {
				// Reset data/highwater events
				RingBufRead(&sbbs->outbuf, NULL, 0);
				continue;
			}

			/*
			 * At this point, there's something to send and,
			 * if the highwater mark is set, the timeout has
			 * passed or we've hit highwater.  Read ring buffer
			 * into linear buffer.
			 */
			if (avail > sizeof(buf)) {
				lprintf(LOG_WARNING, "%s !Insufficient linear output buffer (%u > %d)"
				        , node, avail, (int)sizeof(buf));
				avail = sizeof(buf);
			}
			/* If we know the MSS, use it as the max linear buffer size. */
			if (avail > mss)
				avail = mss;
			buftop = RingBufRead(&sbbs->outbuf, buf, avail);
			bufbot = 0;
			if (buftop == 0)
				continue;
		}

		/* Check socket for writability */
		if (!socket_writable(sbbs->client_socket, 1)) {
			continue;
		}

#ifdef USE_CRYPTLIB
		if (sbbs->ssh_mode) {
			int err;
			pthread_mutex_lock(&sbbs->ssh_mutex);
			if (sbbs->terminate_output_thread) {
				pthread_mutex_unlock(&sbbs->ssh_mutex);
				break;
			}
			if (!sbbs->ssh_mode) {
				pthread_mutex_unlock(&sbbs->ssh_mutex);
				continue;
			}
			if (sbbs->session_channel == -1) {
				i = buftop - bufbot;    // Pretend we sent it all
			}
			else {
				if (cryptStatusError((err = cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sbbs->session_channel)))) {
					GCESSTR(err, node, sbbs->ssh_session, "setting channel");
					sbbs->online = false;
					i = buftop - bufbot;    // Pretend we sent it all
				}
				else {
					/*
					 * Limit as per js_socket.c.
					 * Sure, this is TLS, not SSH, but we see weird stuff here in sz file transfers.
					 */
					size_t sendbytes = buftop - bufbot;
					if (sendbytes > 0x2000)
						sendbytes = 0x2000;
					if (cryptStatusError((err = cryptPushData(sbbs->ssh_session, (char*)buf + bufbot, buftop - bufbot, &i)))) {
						/* Handle the SSH error here... */
						GCESSTR(err, node, sbbs->ssh_session, "pushing data");
						sbbs->online = false;
						i = buftop - bufbot;    // Pretend we sent it all
					}
					else {
						// READ = WRITE TIMEOUT HACK... REMOVE WHEN FIXED
						/* This sets the write timeout for the flush, then sets it to zero
						 * afterward... presumably because the read timeout gets set to
						 * what the current write timeout is.
						 */
						if (cryptStatusError(err = cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_WRITETIMEOUT, 5)))
							GCESSTR(err, node, sbbs->ssh_session, "setting write timeout");
						if (cryptStatusError((err = cryptFlushData(sbbs->ssh_session)))) {
							GCESSTR(err, node, sbbs->ssh_session, "flushing data");
							if (err != CRYPT_ERROR_TIMEOUT) {
								sbbs->online = false;
								i = buftop - bufbot;    // Pretend we sent it all
							}
						}
						// READ = WRITE TIMEOUT HACK... REMOVE WHEN FIXED
						if (cryptStatusError(err = cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_WRITETIMEOUT, 0)))
							GCESSTR(err, node, sbbs->ssh_session, "setting write timeout");
					}
				}
			}
			pthread_mutex_unlock(&sbbs->ssh_mutex);
		}
		else
#endif
		i = sendsocket(sbbs->client_socket, (char*)buf + bufbot, buftop - bufbot);
		if (i == SOCKET_ERROR) {
			if (SOCKET_ERRNO == ENOTSOCK)
				lprintf(LOG_NOTICE, "%s client socket closed on send", node);
			else if (SOCKET_ERRNO == ECONNRESET)
				lprintf(LOG_NOTICE, "%s connection reset by peer on send", node);
			else if (SOCKET_ERRNO == ECONNABORTED)
				lprintf(LOG_NOTICE, "%s connection aborted by peer on send", node);
			else
				lprintf(LOG_WARNING, "%s !ERROR %d (%s) sending on socket %d"
				        , node, SOCKET_ERRNO, SOCKET_STRERROR(errmsg, sizeof errmsg), sbbs->client_socket.load());
			sbbs->online = false;
			/* was break; on 4/7/00 */
			i = buftop - bufbot;    // Pretend we sent it all
		}

		if (sbbs->cfg.node_num > 0 && !(sbbs->sys_status & SS_FILEXFER)) {
			/* Spy on the user locally */
			if (startup->node_spybuf != NULL
			    && startup->node_spybuf[sbbs->cfg.node_num - 1] != NULL) {
				int result = RingBufWrite(startup->node_spybuf[sbbs->cfg.node_num - 1], buf + bufbot, i);
				if (result != i)
					lprintf(LOG_WARNING, "%s SHORT WRITE to spy ring buffer (%u < %u)"
					        , node, result, i);
			}
			/* Spy on the user remotely */
			if (mqtt.connected) {
				int result = mqtt_pub_message(&mqtt, TOPIC_BBS, spy_topic, buf + bufbot, i, /* retain: */ false);
				if (result != MQTT_SUCCESS)
					lprintf(LOG_WARNING, "%s ERROR %d (%d) publishing node output (%u bytes): %s"
					        , node, result, errno, i, spy_topic);
			}
			if (spy_socket[sbbs->cfg.node_num - 1] != INVALID_SOCKET)
				if (sendsocket(spy_socket[sbbs->cfg.node_num - 1], (char*)buf + bufbot, i) != i && SOCKET_ERRNO != EPIPE)
					errprintf(LOG_ERR, WHERE, "%s ERROR %d writing to spy socket", node, SOCKET_ERRNO);
#ifdef __unix__
			if (uspy_socket[sbbs->cfg.node_num - 1] != INVALID_SOCKET)
				if (sendsocket(uspy_socket[sbbs->cfg.node_num - 1], (char*)buf + bufbot, i) != i)
					errprintf(LOG_ERR, WHERE, "%s ERROR %d writing to UNIX spy socket", node, SOCKET_ERRNO);
#endif
		}

		if (i != (int)(buftop - bufbot)) {
			lprintf(LOG_WARNING, "%s !Short socket send (%u instead of %u)"
			        , node, i, buftop - bufbot);
			short_sends++;
		}
		bufbot += i;
		total_sent += i;
		total_pkts++;
	}

	sbbs->spymsg("Disconnected");

	sbbs->output_thread_running = false;

	if (total_sent)
		safe_snprintf(stats, sizeof(stats), "(sent %u bytes in %u blocks, %u average, %u short)"
		              , total_sent, total_pkts, total_sent / total_pkts, short_sends);
	else
		stats[0] = 0;

	thread_down();
	lprintf(LOG_DEBUG, "%s output thread terminated %s", node, stats);
}

static bool is_day_to_run(const struct tm* now, uint8_t days, uint16_t months = 0, uint32_t mdays = 0)
{
	if (!(days & (1 << now->tm_wday)))
		return false;

	if ((mdays > 1) && !(mdays & (1 << now->tm_mday)))
		return false;

	if ((months != 0) && !(months & (1 << now->tm_mon)))
		return false;

	return true;
}

static bool is_time_to_run(time_t now, const struct tm* now_tm, time_t last, uint16_t freq, uint16_t time)
{
	if (freq != 0) {
		if ((now - last) / 60 < freq)
			return false;
	} else {
		struct tm last_tm = {};
		localtime_r(&last, &last_tm);
		if (now_tm->tm_mday == last_tm.tm_mday && now_tm->tm_mon == last_tm.tm_mon) // Already ran today
			return false;
		if (((now_tm->tm_hour * 60) + now_tm->tm_min) < time)
			return false;
	}
	return true;
}

static bool is_time_to_run(time_t now, const event_t* event)
{
	if (event->last == -1)   // Always Run after init/re-init or signaled via semaphore
		return true;

	struct tm now_tm = {};
	localtime_r(&now, &now_tm);

	if (!is_day_to_run(&now_tm, event->days, event->months, event->mdays))
		return false;

	if (!is_time_to_run(now, &now_tm, event->last, event->freq, event->time))
		return false;

	if (xtrn_is_running(&scfg, getxtrnnum(&scfg, event->xtrn)))
		return false;

	return true;
}

static bool is_time_to_run(time_t now, const qhub_t* hub)
{
	if (hub->last == -1) // Always Run when signaled via semaphore
		return true;

	struct tm now_tm = {};
	localtime_r(&now, &now_tm);

	if (!is_day_to_run(&now_tm, hub->days))
		return false;

	if (!is_time_to_run(now, &now_tm, hub->last, hub->freq, hub->time))
		return false;

	return true;
}

void event_thread(void* arg)
{
	char str[MAX_PATH + 1];
	int i, j;
	int file;
	int offset;
	bool check_semaphores;
	bool packed_rep;
	uint l;
	/* TODO: This is a silly hack... */
	uint32_t l32;
	time_t now;
	time_t start;
	time_t lastsemchk = 0;
	time_t lastnodechk = 0;
	node_t node;
	glob_t g;
	sbbs_t*     sbbs = (sbbs_t*) arg;
	char event_code[LEN_CODE + 1];

	if (terminate_server)
		return;

	sbbs->lprintf(LOG_INFO, "BBS Events thread started");

	sbbs_srand();   /* Seed random number generator */

	SetThreadName("sbbs/events");
	thread_up(true /* setuid */);

	mqtt_pub_strval(&mqtt, TOPIC_HOST, "event", "thread started");

#ifdef JAVASCRIPT
	if (!(startup->options & BBS_OPT_NO_JAVASCRIPT)) {
		if ((sbbs->js_cx = sbbs->js_init(&sbbs->js_runtime, &sbbs->js_glob, "event")) == NULL) /* This must be done in the context of the events thread */
			sbbs->errprintf(LOG_CRIT, WHERE, "!JavaScript Initialization FAILURE");
	}
#endif

	// Read event "last ran" times from ctrl/time.ini
	char ini_file[MAX_PATH + 1];
	SAFEPRINTF(ini_file, "%stime.ini", sbbs->cfg.ctrl_dir);
	FILE* fp = iniOpenFile(ini_file, /* for modify: */ false);
	if (fp != nullptr) {
		for (i = 0; i < sbbs->cfg.total_events; ++i)
			sbbs->cfg.event[i]->last = (time32_t)iniReadDateTime(fp, eventIniSection, sbbs->cfg.event[i]->code, 0);
		for (i = 0; i < sbbs->cfg.total_qhubs; ++i)
			sbbs->cfg.qhub[i]->last = (time32_t)iniReadDateTime(fp, qhubIniSection, sbbs->cfg.qhub[i]->id, 0);
		iniCloseFile(fp);
	} else {
		// Read time.dab (legacy), and auto-upgrade to time.ini
		SAFEPRINTF(str, "%stime.dab", sbbs->cfg.ctrl_dir);
		if ((file = sbbs->nopen(str, O_RDONLY)) != -1) {
			for (i = 0; i < sbbs->cfg.total_events; i++) {
				if (read(file, &sbbs->cfg.event[i]->last, sizeof(sbbs->cfg.event[i]->last)) != sizeof(sbbs->cfg.event[i]->last))
					sbbs->cfg.event[i]->last = 0;
			}
			close(file);
		}
		// Read qnet.dab (legacy), and auto-upgrade to time.ini
		SAFEPRINTF(str, "%sqnet.dab", sbbs->cfg.ctrl_dir);
		if ((file = sbbs->nopen(str, O_RDONLY)) != -1) {
			for (i = 0; i < sbbs->cfg.total_qhubs; i++) {
				if (read(file, &sbbs->cfg.qhub[i]->last, sizeof(sbbs->cfg.qhub[i]->last)) != sizeof(sbbs->cfg.qhub[i]->last))
					sbbs->cfg.qhub[i]->last = 0;
			}
			close(file);
		}

		fp = iniOpenFile(ini_file, /* for modify: */ true);
		if (fp == nullptr)
			sbbs->errormsg(WHERE, ERR_CREATE, ini_file, 0);
		else {
			str_list_t ini = strListInit();
			if (ini != NULL) {
				for (i = 0; i < sbbs->cfg.total_events; ++i)
					iniSetDateTime(&ini, eventIniSection, sbbs->cfg.event[i]->code, /* include time */ true, sbbs->cfg.event[i]->last, &eventIniStyle);
				for (i = 0; i < sbbs->cfg.total_qhubs; ++i)
					iniSetDateTime(&ini, qhubIniSection, sbbs->cfg.qhub[i]->id, /* include time */ true, sbbs->cfg.qhub[i]->last, &qhubIniStyle);
				iniWriteFile(fp, ini);
				iniCloseFile(fp);
				iniFreeStringList(ini);
			}
		}
	}
	for (i = 0; i < sbbs->cfg.total_events; ++i) {
		/* Event always runs after initialization? */
		if (sbbs->cfg.event[i]->misc & EVENT_INIT)
			sbbs->cfg.event[i]->last = -1;
		if (sbbs->cfg.event[i]->node == NODE_ANY
		    || (sbbs->cfg.event[i]->node >= first_node && sbbs->cfg.event[i]->node <= last_node))
			sbbs->fremove(WHERE, sbbs->event_running_filename(str, sizeof str, i));
	}

	while (!sbbs->terminated && !terminate_server) {

		sbbs->event_thread_running = true;
		event_thread_tick = time(NULL);

		if (startup->options & BBS_OPT_NO_EVENTS) {
			SLEEP(1000);
			continue;
		}

		now = time(NULL);

		if (now - lastsemchk >= startup->sem_chk_freq) {
			check_semaphores = true;
			lastsemchk = now;
		} else
			check_semaphores = false;

		sbbs->online = false; /* reset this from ON_LOCAL */

		/* QWK events */
		if (check_semaphores && !(startup->options & BBS_OPT_NO_QWK_EVENTS)) {
			sbbs->event_code = "unpackREP";
			/* Import any REP files that have magically appeared (via FTP perhaps) */
			SAFEPRINTF(str, "%sfile/", sbbs->cfg.data_dir);
			offset = strlen(str);
			strcat(str, "*.rep");
			glob(str, 0, NULL, &g);
			for (i = 0; i < (int)g.gl_pathc && !sbbs->terminated; i++) {
				char* fname = g.gl_pathv[i];
				if (flength(fname) < 1)
					continue;
				sbbs->useron.number = 0;
				sbbs->lprintf(LOG_DEBUG, "Inbound QWK Reply Packet detected: %s", fname);
				if (sbbs->getuseron(WHERE, atoi(fname + offset)) && !(sbbs->useron.misc & (DELETED | INACTIVE))) {
					fmutex_t lockfile;
					SAFEPRINTF(lockfile.name, "%s.lock", fname);
					if (!fmutex_open(&lockfile, startup->host_name, TIMEOUT_MUTEX_FILE)) {
						if (difftime(time(NULL), lockfile.time) > 60)
							sbbs->lprintf(LOG_INFO, " %s exists (unpack in progress?) since %s", lockfile.name, time_as_hhmm(&sbbs->cfg, lockfile.time, str));
						continue;
					}
					sbbs->lprintf(LOG_DEBUG, "Opened %s", lockfile.name);
					if (!fexist(fname)) {
						sbbs->lprintf(LOG_DEBUG, "%s already gone", fname);
						if (!fmutex_close(&lockfile))
							sbbs->errormsg(WHERE, ERR_CLOSE, lockfile.name);
						continue;
					}
					sbbs->online = ON_LOCAL;
					sbbs->getusrsubs();
					bool success = sbbs->unpack_rep(fname);
					sbbs->delfiles(sbbs->cfg.temp_dir, ALLFILES);        /* clean-up temp_dir after unpacking */
					sbbs->online = false;

					/* putuserdat? */
					if (success) {
						sbbs->fremove(WHERE, fname, /* log-all-errors: */ true);
					} else if (flength(fname) > 0) {
						char badpkt[MAX_PATH + 1];
						SAFEPRINTF2(badpkt, "%s.%" PRIx64 ".bad", fname, (uint64_t)time(NULL));
						sbbs->fremove(WHERE, badpkt);
						if (flength(fname) > 0) {
							if (rename(fname, badpkt) == 0)
								sbbs->lprintf(LOG_NOTICE, "%s renamed to %s", fname, badpkt);
							else
								sbbs->lprintf(LOG_ERR, "!ERROR %d (%s) renaming %s (%ld bytes) to %s"
								              , errno, strerror(errno), fname, (long)flength(fname), badpkt);
							SAFEPRINTF(badpkt, "%u.rep.*.bad", sbbs->useron.number);
							SAFEPRINTF(str, "%sfile/", sbbs->cfg.data_dir);
							sbbs->delfiles(str, badpkt, /* keep: */ 10);
						}
					}
					if (!fmutex_close(&lockfile))
						sbbs->errormsg(WHERE, ERR_CLOSE, lockfile.name);
				}
				else {
					sbbs->lprintf(LOG_INFO, "Removing: %s", fname);
					sbbs->fremove(WHERE, fname, /* log-all-errors: */ true);
				}
			}
			globfree(&g);
			sbbs->useron.number = 0;

			/* Create any QWK files that have magically appeared (via FTP perhaps) */
			sbbs->event_code = "packQWK";
			SAFEPRINTF(str, "%spack*.now", sbbs->cfg.data_dir);
			offset = strlen(sbbs->cfg.data_dir) + 4;
			glob(str, 0, NULL, &g);
			for (i = 0; i < (int)g.gl_pathc && !sbbs->terminated; i++) {
				char* fname = g.gl_pathv[i];
				if (!fexist(fname))
					continue;
				sbbs->useron.number = 0;
				sbbs->lprintf(LOG_INFO, "QWK pack semaphore signaled: %s", fname);
				if (!sbbs->getuseron(WHERE, atoi(fname + offset))) {
					sbbs->fremove(WHERE, fname, /* log-all-errors: */ true);
					continue;
				}
				fmutex_t lockfile;
				SAFEPRINTF2(lockfile.name, "%spack%04u.lock", sbbs->cfg.data_dir, sbbs->useron.number);
				if (!fmutex_open(&lockfile, startup->host_name, TIMEOUT_MUTEX_FILE)) {
					if (difftime(time(NULL), lockfile.time) > 60)
						sbbs->lprintf(LOG_INFO, "%s exists (pack in progress?) since %s", lockfile.name, time_as_hhmm(&sbbs->cfg, lockfile.time, str));
					continue;
				}
				sbbs->lprintf(LOG_DEBUG, "Opened %s", lockfile.name);
				if (!fexist(fname)) {
					sbbs->lprintf(LOG_DEBUG, "%s already gone", fname);
					if (!fmutex_close(&lockfile))
						sbbs->errormsg(WHERE, ERR_CLOSE, lockfile.name);
					continue;
				}
				if (!(sbbs->useron.misc & (DELETED | INACTIVE))) {
					sbbs->lprintf(LOG_INFO, "Packing QWK Message Packet");
					sbbs->online = ON_LOCAL;
					sbbs->getmsgptrs();
					sbbs->getusrsubs();

					sbbs->last_ns_time = sbbs->ns_time = sbbs->useron.ns_time;

					SAFEPRINTF3(str, "%sfile%c%04u.qwk"
					            , sbbs->cfg.data_dir, PATH_DELIM, sbbs->useron.number);
					if (sbbs->pack_qwk(str, &l, true /* pre-pack/off-line */)) {
						sbbs->lprintf(LOG_INFO, "Packing completed: %s", str);
						sbbs->qwk_success(l, 0, 1);
						sbbs->putmsgptrs();
						batch_list_clear(&sbbs->cfg, sbbs->useron.number, XFER_BATCH_DOWNLOAD);
					} else
						sbbs->lputs(LOG_INFO, "No packet created (no new messages)");
					sbbs->delfiles(sbbs->cfg.temp_dir, ALLFILES);
					sbbs->online = false;
				}
				sbbs->fremove(WHERE, fname);
				if (!fmutex_close(&lockfile))
					sbbs->errormsg(WHERE, ERR_CLOSE, lockfile.name);
			}
			globfree(&g);
			sbbs->useron.number = 0;
		}

		if (check_semaphores) {

			/* Run daily maintenance? */
			sbbs->cfg.node_num = 0;
			if (!(startup->options & BBS_OPT_NO_NEWDAY_EVENTS)) {
				sbbs->event_code = "";
				sbbs->online = ON_LOCAL;
				sbbs->logonstats();
				sbbs->online = false;
				if (sbbs->sys_status & SS_DAILY)
					sbbs->daily_maint();
			}
			/* Node Daily Events */
			sbbs->event_code = "DAILY";
			for (i = first_node; i <= last_node; i++) {
				// Node Daily Event
				node.status = NODE_INVALID_STATUS;
				if (!sbbs->getnodedat(i, &node))
					continue;
				if (node.misc & NODE_EVENT && node.status == NODE_WFC) {
					if (sbbs->getnodedat(i, &node, true)) {
						node.status = NODE_EVENT_RUNNING;
						sbbs->putnodedat(i, &node);
					}
					if (sbbs->cfg.node_daily.cmd[0] && !(sbbs->cfg.node_daily.misc & EVENT_DISABLED)) {
						sbbs->cfg.node_num = i;
						SAFECOPY(sbbs->cfg.node_dir, sbbs->cfg.node_path[i - 1]);

						sbbs->lprintf(LOG_INFO, "Running node %d daily event", i);
						sbbs->online = ON_LOCAL;
						sbbs->logentry("!:", "Run node daily event");
						const char* cmd = sbbs->cmdstr(sbbs->cfg.node_daily.cmd, nulstr, nulstr, NULL, sbbs->cfg.node_daily.misc);
						int result = sbbs->external(cmd, EX_OFFLINE | sbbs->cfg.node_daily.misc);
						sbbs->lprintf(result ? LOG_ERR : LOG_INFO, "Node daily event: '%s' returned %d", cmd, result);
						sbbs->online = false;
					}
					if (sbbs->getnodedat(i, &node, true)) {
						node.misc &= ~NODE_EVENT;
						node.status = NODE_WFC;
						node.useron = 0;
						sbbs->putnodedat(i, &node);
					}
				}
			}
			sbbs->event_code = nulstr;

			/* QWK Networking Call-out semaphores */
			for (i = 0; i < sbbs->cfg.total_qhubs; i++) {
				if (!sbbs->cfg.qhub[i]->enabled)
					continue;
				if (sbbs->cfg.qhub[i]->node != NODE_ANY
				    && (sbbs->cfg.qhub[i]->node<first_node || sbbs->cfg.qhub[i]->node>last_node))
					continue;
				if (sbbs->cfg.qhub[i]->last == -1) // already signaled
					continue;
				SAFEPRINTF2(str, "%sqnet/%s.now", sbbs->cfg.data_dir, sbbs->cfg.qhub[i]->id);
				if (fexistcase(str)) {
					SAFECOPY(str, sbbs->cfg.qhub[i]->id);
					sbbs->lprintf(LOG_INFO, "Semaphore signaled for QWK Network Hub: %s", strupr(str));
					sbbs->cfg.qhub[i]->last = -1;
				}
			}

			/* Timed Event semaphores */
			for (i = 0; i < sbbs->cfg.total_events; i++) {
				if (sbbs->cfg.event[i]->node != NODE_ANY
				    && (sbbs->cfg.event[i]->node<first_node || sbbs->cfg.event[i]->node>last_node)
				    && !(sbbs->cfg.event[i]->misc & EVENT_EXCL))
					continue;   // ignore non-exclusive events for other instances
				if (sbbs->cfg.event[i]->misc & EVENT_DISABLED)
					continue;
				if (sbbs->cfg.event[i]->last == -1) // already signaled
					continue;
				SAFEPRINTF2(str, "%s%s.now", sbbs->cfg.data_dir, sbbs->cfg.event[i]->code);
				if (fexistcase(str)) {
					SAFECOPY(str, sbbs->cfg.event[i]->code);
					sbbs->lprintf(LOG_INFO, "Semaphore signaled for Timed Event: %s", strupr(str));
					sbbs->cfg.event[i]->last = -1;
				}
			}
		}

		/* QWK Networking Call-out Events */
		sbbs->event_code = "QNET";
		for (i = 0; i < sbbs->cfg.total_qhubs && !sbbs->terminated; i++) {
			if (!sbbs->cfg.qhub[i]->enabled)
				continue;
			if (sbbs->cfg.qhub[i]->node != NODE_ANY
			    && (sbbs->cfg.qhub[i]->node<first_node || sbbs->cfg.qhub[i]->node>last_node))
				continue;

			if (check_semaphores) {
				// See if any packets have come in
				SAFEPRINTF2(str, "%s%s.q??", sbbs->cfg.data_dir, sbbs->cfg.qhub[i]->id);
				glob(str, GLOB_NOSORT, NULL, &g);
				for (j = 0; j < (int)g.gl_pathc; j++) {
					SAFECOPY(str, g.gl_pathv[j]);
					if (flength(str) > 0) {    /* silently ignore 0-byte QWK packets */
						sbbs->lprintf(LOG_DEBUG, "Inbound QWK Packet detected: %s", str);
						sbbs->online = ON_LOCAL;
						if (sbbs->unpack_qwk(str, i) == false) {
							char newname[MAX_PATH + 1];
							SAFEPRINTF2(newname, "%s.%x.bad", str, (int)now);
							sbbs->fremove(WHERE, newname);
							if (rename(str, newname) == 0)
								sbbs->lprintf(LOG_NOTICE, "%s renamed to %s", str, newname);
							else
								sbbs->lprintf(LOG_ERR, "!ERROR %d (%s) renaming %s to %s"
								              , errno, strerror(errno), str, newname);
							SAFEPRINTF(newname, "%s.q??.*.bad", sbbs->cfg.qhub[i]->id);
							sbbs->delfiles(sbbs->cfg.data_dir, newname, /* keep: */ 10);
						}
						sbbs->delfiles(sbbs->cfg.temp_dir, ALLFILES);
						sbbs->online = false;
						if (fexist(str))
							sbbs->fremove(WHERE, str, /* log-all-errors: */ true);
					}
				}
				globfree(&g);
			}

			/* Qnet call out based on time */
			if (is_time_to_run(now, sbbs->cfg.qhub[i])) {
				SAFEPRINTF2(str, "%sqnet/%s.now"
				            , sbbs->cfg.data_dir, sbbs->cfg.qhub[i]->id);
				if (fexistcase(str))
					sbbs->fremove(WHERE, str);                  /* Remove semaphore file */
				SAFEPRINTF2(str, "%sqnet/%s.ptr"
				            , sbbs->cfg.data_dir, sbbs->cfg.qhub[i]->id);
				file = sbbs->nopen(str, O_RDONLY);
				for (j = 0; j < sbbs->cfg.qhub[i]->subs; j++) {
					sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]->subnum].ptr = 0;
					if (file != -1) {
						lseek(file, sbbs->cfg.sub[sbbs->cfg.qhub[i]->sub[j]->subnum]->ptridx * sizeof(int32_t), SEEK_SET);
						if (read(file, &sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]->subnum].ptr
						         , sizeof(sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]->subnum].ptr)) != sizeof(uint32_t))
							sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]->subnum].ptr = 0;
					}
				}
				if (file != -1)
					close(file);
				packed_rep = sbbs->pack_rep(i);
				if (packed_rep) {
					if ((file = sbbs->nopen(str, O_WRONLY | O_CREAT)) == -1)
						sbbs->errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT);
					else {
						for (j = l = 0; j < sbbs->cfg.qhub[i]->subs; j++) {
							while (filelength(file) <
							       sbbs->cfg.sub[sbbs->cfg.qhub[i]->sub[j]->subnum]->ptridx * 4L) {
								l32 = l;
								if (write(file, &l32, 4) != 4)     /* initialize ptrs to null */
									sbbs->errormsg(WHERE, ERR_WRITE, str, 4);
							}
							lseek(file
							      , sbbs->cfg.sub[sbbs->cfg.qhub[i]->sub[j]->subnum]->ptridx * sizeof(int32_t)
							      , SEEK_SET);
							if (write(file, &sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]->subnum].ptr, sizeof(sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]->subnum].ptr)) != sizeof sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]->subnum].ptr)
								sbbs->errormsg(WHERE, ERR_WRITE, str, 4);
						}
						close(file);
					}
				}
				sbbs->delfiles(sbbs->cfg.temp_dir, ALLFILES);

				sbbs->cfg.qhub[i]->last = time32(NULL);
				SAFEPRINTF(str, "%stime.ini", sbbs->cfg.ctrl_dir);
				FILE* fp = iniOpenFile(str, /* for_modify: */ true);
				if (fp == NULL) {
					sbbs->errormsg(WHERE, ERR_OPEN, str, O_WRONLY);
					break;
				}
				str_list_t ini = iniReadFile(fp);
				if (ini != NULL) {
					iniSetDateTime(&ini, qhubIniSection, sbbs->cfg.qhub[i]->id, /* include time: */ true, sbbs->cfg.qhub[i]->last, &qhubIniStyle);
					iniWriteFile(fp, ini);
					iniFreeStringList(ini);
				}
				iniCloseFile(fp);

				if (sbbs->cfg.qhub[i]->call[0]) {
					if (sbbs->cfg.qhub[i]->node == NODE_ANY)
						sbbs->cfg.node_num = startup->first_node;
					else
						sbbs->cfg.node_num = sbbs->cfg.qhub[i]->node;
					if (sbbs->cfg.node_num < 1)
						sbbs->cfg.node_num = 1;
					SAFECOPY(sbbs->cfg.node_dir, sbbs->cfg.node_path[sbbs->cfg.node_num - 1]);
					sbbs->lprintf(LOG_INFO, "Call-out: %s", sbbs->cfg.qhub[i]->id);
					sbbs->online = ON_LOCAL;
					int ex_mode = EX_OFFLINE | EX_SH; /* sh for Unix perl scripts */
					if (sbbs->cfg.qhub[i]->misc & QHUB_NATIVE)
						ex_mode |= EX_NATIVE;
					const char* cmd = sbbs->cmdstr(sbbs->cfg.qhub[i]->call, sbbs->cfg.qhub[i]->id, sbbs->cfg.qhub[i]->id, NULL, ex_mode);
					int result = sbbs->external(cmd, ex_mode);
					sbbs->lprintf(result ? LOG_ERR : LOG_INFO, "Call-out to: %s (%s) returned %d", sbbs->cfg.qhub[i]->id, cmd, result);
					sbbs->online = false;
				}
			}
		}

		/* Timed Events */
		for (i = 0; i < sbbs->cfg.total_events && !sbbs->terminated; i++) {
			if (sbbs->cfg.event[i]->node > sbbs->cfg.sys_nodes)
				continue;   // ignore events for invalid nodes

			if (sbbs->cfg.event[i]->misc & EVENT_DISABLED)
				continue;

			if (sbbs->cfg.event[i]->node != NODE_ANY
			    && (sbbs->cfg.event[i]->node<first_node || sbbs->cfg.event[i]->node>last_node)
			    && !(sbbs->cfg.event[i]->misc & EVENT_EXCL))
				continue;   // ignore non-exclusive events for other instances

			SAFECOPY(event_code, sbbs->cfg.event[i]->code);
			strupr(event_code);
			sbbs->event_code = event_code;

			if (is_time_to_run(now, sbbs->cfg.event[i]))
			{
				if (sbbs->cfg.event[i]->node != NODE_ANY && (sbbs->cfg.event[i]->misc & EVENT_EXCL)) { /* exclusive event */

					if (sbbs->cfg.event[i]->node<first_node
					                             || sbbs->cfg.event[i]->node>last_node) {
						sbbs->lprintf(LOG_INFO, "Waiting for node %d to run timed event: %s"
						              , sbbs->cfg.event[i]->node, event_code);
						sbbs->lprintf(LOG_DEBUG, "event last run: %s (0x%08x)"
						              , sbbs->timestr(sbbs->cfg.event[i]->last)
						              , sbbs->cfg.event[i]->last);
						lastnodechk = 0;   /* really last event time check */
						start = time(NULL);
						while (!sbbs->terminated) {
							mswait(1000);
							now = time(NULL);
							if (now - start > 10 && now - lastnodechk < 10)
								continue;
							for (j = first_node; j <= last_node; j++) {
								if (!sbbs->getnodedat(j, &node, true))
									continue;
								if (node.status == NODE_WFC)
									node.status = NODE_EVENT_LIMBO;
								node.aux = sbbs->cfg.event[i]->node;
								sbbs->putnodedat(j, &node);
							}

							lastnodechk = now;
							SAFEPRINTF(str, "%stime.ini", sbbs->cfg.ctrl_dir);
							if ((fp = iniOpenFile(str, /* for modify: */ false)) == NULL) {
								sbbs->errormsg(WHERE, ERR_OPEN, str, O_RDONLY);
								sbbs->cfg.event[i]->last = (time32_t)now;
								continue;
							}
							sbbs->cfg.event[i]->last = (time32_t)iniReadDateTime(fp, eventIniSection, sbbs->cfg.event[i]->code, 0);
							iniCloseFile(fp);
							if (now - sbbs->cfg.event[i]->last < (60 * 60))    /* event is done */
								break;
							if (now - start > (90 * 60)) {
								sbbs->lprintf(LOG_WARNING, "!TIMEOUT waiting for event to complete");
								break;
							}
						}
						SAFEPRINTF2(str, "%s%s.now", sbbs->cfg.data_dir, sbbs->cfg.event[i]->code);
						if (fexistcase(str))
							sbbs->fremove(WHERE, str);
						sbbs->cfg.event[i]->last = (time32_t)now;
					} else {    // Exclusive event to run on a node under our control
						sbbs->lprintf(LOG_INFO, "Waiting for all nodes to become inactive before running timed event");
						lastnodechk = 0;
						start = time(NULL);
						while (!sbbs->terminated) {
							mswait(1000);
							now = time(NULL);
							if (now - start > 10 && now - lastnodechk < 10)
								continue;
							lastnodechk = now;
							// Check/change the status of the nodes that we're in control of
							for (j = first_node; j <= last_node; j++) {
								if (!sbbs->getnodedat(j, &node, true))
									continue;
								if (node.status == NODE_WFC) {
									if (j == sbbs->cfg.event[i]->node)
										node.status = NODE_EVENT_WAITING;
									else
										node.status = NODE_EVENT_LIMBO;
									node.aux = sbbs->cfg.event[i]->node;
								}
								sbbs->putnodedat(j, &node);
							}

							for (j = 1; j <= sbbs->cfg.sys_nodes; j++) {
								if (!sbbs->getnodedat(j, &node))
									continue;
								if (j == sbbs->cfg.event[i]->node) {
									if (node.status != NODE_EVENT_WAITING)
										break;
								} else {
									if (node.status != NODE_OFFLINE
									    && node.status != NODE_EVENT_LIMBO)
										break;
								}
							}
							if (j > sbbs->cfg.sys_nodes) /* all nodes either offline or in limbo */
								break;
							char node_status[128];
							snprintf(node_status, sizeof node_status, "status=%d, misc=0x%X, action=%d, useron=%d, aux=%d"
								, node.status, node.misc, node.action, node.useron, node.aux);
							sbbs->lprintf(LOG_DEBUG, "Waiting for node %d (%s)", j, node_status);
							if (now - start > (30 * 60) && !(node.misc & NODE_INTR)) {
								sbbs->lprintf(LOG_NOTICE, "!Interrupting node %d (%s)", j, node_status);
								if (!set_node_interrupt(&sbbs->cfg, j, true))
									sbbs->lprintf(LOG_ERR, "!ERROR interrupting node %d (%s)", j, node_status);
							}
							if (now - start > (60 * 60) && node_socket[j - 1] != INVALID_SOCKET) {
								sbbs->lprintf(LOG_WARNING, "!TIRED of waiting for node %d to become inactive (%s), closing socket %d"
								              , j, node_status, node_socket[j - 1]);
								close_socket(node_socket[j - 1]);
								node_socket[j - 1] = INVALID_SOCKET;
							}
							if (now - start > (90 * 60)) {
								sbbs->lprintf(LOG_WARNING, "!TIMEOUT waiting for node %d to become inactive (%s), aborting wait"
								              , j, node_status);
								break;
							}
						}
					}
				}
#if 0 // removed Jun-23-2002
				else {  /* non-exclusive */
					sbbs->getnodedat(sbbs->cfg.event[i]->node, &node, 0);
					if (node.status != NODE_WFC)
						continue;
				}
#endif
				if (sbbs->cfg.event[i]->node != NODE_ANY
				    && (sbbs->cfg.event[i]->node<first_node || sbbs->cfg.event[i]->node>last_node)) {
					sbbs->lprintf(LOG_NOTICE, "Changing node status for nodes %d through %d to WFC"
					              , first_node, last_node);
					sbbs->cfg.event[i]->last = (time32_t)now;
					for (j = first_node; j <= last_node; j++) {
						node.status = NODE_INVALID_STATUS;
						if (!sbbs->getnodedat(j, &node, true))
							continue;
						node.status = NODE_WFC;
						sbbs->putnodedat(j, &node);
					}
				}
				else {
					if (sbbs->cfg.event[i]->node == NODE_ANY)
						sbbs->cfg.node_num = startup->first_node;
					else
						sbbs->cfg.node_num = sbbs->cfg.event[i]->node;
					if (sbbs->cfg.node_num < 1)
						sbbs->cfg.node_num = 1;
					SAFECOPY(sbbs->cfg.node_dir, sbbs->cfg.node_path[sbbs->cfg.node_num - 1]);

					SAFEPRINTF2(str, "%s%s.now", sbbs->cfg.data_dir, sbbs->cfg.event[i]->code);
					if (fexistcase(str))
						sbbs->fremove(WHERE, str);
					if (sbbs->cfg.event[i]->node != NODE_ANY && (sbbs->cfg.event[i]->misc & EVENT_EXCL)) {
						if (sbbs->getnodedat(sbbs->cfg.event[i]->node, &node, true)) {
							node.status = NODE_EVENT_RUNNING;
							sbbs->putnodedat(sbbs->cfg.event[i]->node, &node);
						}
					}
					const char* cmd = sbbs->cfg.event[i]->cmd;
					int ex_mode = EX_OFFLINE;
					if (!(sbbs->cfg.event[i]->misc & EVENT_EXCL)
					    && sbbs->cfg.event[i]->misc & EX_BG)
						ex_mode |= EX_BG;
					if (sbbs->cfg.event[i]->misc & XTRN_SH)
						ex_mode |= EX_SH;
					ex_mode |= (sbbs->cfg.event[i]->misc & EX_NATIVE);
					sbbs->online = ON_LOCAL;
					cmd = sbbs->cmdstr(cmd, nulstr, sbbs->cfg.event[i]->dir, NULL, ex_mode);
					sbbs->lprintf(LOG_INFO, "Running %s%stimed event: %s"
					              , native_executable(&sbbs->cfg, cmd, ex_mode) ? "native ":"16-bit DOS "
					              , (ex_mode & EX_BG)        ? "background ":""
					              , cmd);
					{
						char path[MAX_PATH + 1];
						if (!(ex_mode & EX_BG))
							ftouch(sbbs->event_running_filename(path, sizeof path, i));
						int result = sbbs->external(cmd, ex_mode, sbbs->cfg.event[i]->dir);
						if (!(ex_mode & EX_BG)) {
							sbbs->lprintf(result ? sbbs->cfg.event[i]->errlevel : LOG_INFO, "Timed event: '%s' returned %d", cmd, result);
							sbbs->fremove(WHERE, path);
						}
						else
							sbbs->lprintf(LOG_DEBUG, "Background timed event spawned: %s", cmd);
					}
					sbbs->online = false;
					sbbs->cfg.event[i]->last = time32(NULL);
					SAFEPRINTF(str, "%stime.ini", sbbs->cfg.ctrl_dir);
					if ((fp = iniOpenFile(str, /* for modify: */ true)) == NULL) {
						sbbs->errormsg(WHERE, ERR_OPEN, str, O_WRONLY);
						break;
					}
					str_list_t ini = iniReadFile(fp);
					if (ini != NULL) {
						iniSetDateTime(&ini, eventIniSection, sbbs->cfg.event[i]->code, /* include time */ true, sbbs->cfg.event[i]->last, &eventIniStyle);
						iniWriteFile(fp, ini);
						iniFreeStringList(ini);
					}
					iniCloseFile(fp);

					if (sbbs->cfg.event[i]->node != NODE_ANY
					    && sbbs->cfg.event[i]->misc & EVENT_EXCL) { /* exclusive event */
						// Check/change the status of the nodes that we're in control of
						for (j = first_node; j <= last_node; j++) {
							node.status = NODE_INVALID_STATUS;
							if (!sbbs->getnodedat(j, &node, true))
								continue;
							node.status = NODE_WFC;
							sbbs->putnodedat(j, &node);
						}
					}
				}
			}
		}
		sbbs->event_code = nulstr;
		mswait(1000);
	}
	sbbs->cfg.node_num = 0;
	sbbs->useron.number = 0;
	sbbs->js_cleanup();

	sbbs->event_thread_running = false;

	mqtt_pub_strval(&mqtt, TOPIC_HOST, "event", "thread stopped");

	thread_down();
	sbbs->lprintf(LOG_INFO, "BBS Events thread terminated");
	FCLOSE_OPEN_FILE(sbbs->logfile_fp);
}

//****************************************************************************
sbbs_t::sbbs_t(ushort node_num, union xp_sockaddr *addr, size_t addr_len, const char* name, SOCKET sd,
               scfg_t* global_cfg, char* global_text[], client_t* client_info, bool is_event_thread)
{
	char path[MAX_PATH + 1];
	uint i;

	// Initialize some member variables used by lputs:
	this->is_event_thread = is_event_thread;
	cfg.node_num = node_num;
	SAFECOPY(client_name, name);

	startup = ::startup;    // Convert from global to class member
	mqtt = &::mqtt;

	memcpy(&cfg, global_cfg, sizeof(cfg));
	cfg.node_num = node_num;    // Restore the node_num passed to the constructor
	lprintf(LOG_DEBUG, "constructor using socket %d (settings=%x)"
	        , sd, global_cfg->node_misc);

	if (node_num > 0) {
		SAFECOPY(cfg.node_dir, cfg.node_path[node_num - 1]);
		prep_dir(cfg.node_dir, cfg.temp_dir, sizeof(cfg.temp_dir));
		SAFEPRINTF2(syspage_semfile, "%ssyspage.%u", cfg.ctrl_dir, node_num);
		fremove(WHERE, syspage_semfile);
	} else {    /* event thread needs exclusive-use temp_dir */
		if (startup->temp_dir[0])
			SAFECOPY(cfg.temp_dir, startup->temp_dir);
		else
			SAFECOPY(cfg.temp_dir, "../temp");
		prep_dir(cfg.ctrl_dir, cfg.temp_dir, sizeof(cfg.temp_dir));
		if ((i = md(cfg.temp_dir)) != 0)
			errprintf(LOG_CRIT, WHERE, "!ERROR %d (%s) creating directory: %s", i, strerror(i), cfg.temp_dir);
		if (sd == INVALID_SOCKET) {    /* events thread */
			if (startup->first_node == 1)
				SAFEPRINTF(path, "%sevent", cfg.temp_dir);
			else
				SAFEPRINTF2(path, "%sevent%u", cfg.temp_dir, startup->first_node);
			backslash(path);
			SAFECOPY(cfg.temp_dir, path);
		}
		syspage_semfile[0] = 0;
	}
	lprintf(LOG_DEBUG, "temporary file directory: %s", cfg.temp_dir);

	if (client_info != nullptr)
		memcpy(&client, client_info, sizeof(client));
	client_addr.store = addr->store;
	client_socket = sd;
	client_socket_dup = INVALID_SOCKET;

	/* Init some important variables */

	SAFECOPY(connection, "Telnet");
	telnet_ack_event = CreateEvent(NULL, /* Manual Reset: */ false, /* InitialState */ false, NULL);

	listInit(&smb_list, /* flags: */ 0);
	pthread_mutex_init(&nodefile_mutex, NULL);

	for (i = 0; i < TOTAL_TEXT; i++)
		text[i] = text_sav[i] = global_text[i];

	memcpy(rainbow, cfg.rainbow, sizeof rainbow);
}

//****************************************************************************
bool sbbs_t::init()
{
	char str[MAX_PATH + 1];
	int result;
	int i, j, k, l;
	node_t node;
	socklen_t addr_len;
	union xp_sockaddr addr;

	if (cfg.total_shells < 1) {
		errormsg(WHERE, ERR_CHK, "At least one command shell must be configured (e.g. in SCFG->Command Shells)"
		         , cfg.total_shells);
		return false;
	}

	RingBufInit(&inbuf, IO_THREAD_BUF_SIZE);
	if (cfg.node_num > 0)
		node_inbuf[cfg.node_num - 1] = &inbuf;

	RingBufInit(&outbuf, IO_THREAD_BUF_SIZE);
	outbuf.highwater_mark = startup->outbuf_highwater_mark;

	if (cfg.node_num && client_socket != INVALID_SOCKET) {

		addr_len = sizeof(addr);
		if ((result = getsockname(client_socket, &addr.addr, &addr_len)) != 0) {
			errprintf(LOG_CRIT, WHERE, "%04d %s !ERROR %d (%d) getting local address/port of socket"
			          , client_socket.load(), client.protocol, result, SOCKET_ERRNO);
			return false;
		}
		inet_addrtop(&addr, local_addr, sizeof(local_addr));
		inet_addrtop(&client_addr, client_ipaddr, sizeof(client_ipaddr));
		SAFEPRINTF(str, "%sclient.ini", cfg.node_dir);
		FILE* fp = fopen(str, "wt");
		if (fp != NULL) {
			fprintf(fp, "sock=%d\n", client_socket.load());
			fprintf(fp, "addr=%s\n", client.addr);
			fprintf(fp, "host=%s\n", client.host);
			fprintf(fp, "port=%u\n", (uint)client.port);
			fprintf(fp, "time=%u\n", (uint)client.time);
			fprintf(fp, "prot=%s\n", client.protocol);
			fprintf(fp, "local_addr=%s\n", local_addr);
			fprintf(fp, "local_port=%u\n", (uint)inet_addrport(&addr));
			fclose(fp);
		}
		spymsg("Connected");
	}

	if ((comspec = os_cmdshell()) == NULL) {
		errormsg(WHERE, ERR_CHK, OS_CMD_SHELL_ENV_VAR " environment variable", 0);
		return false;
	}

	if ((i = md(cfg.temp_dir)) != 0) {
		errprintf(LOG_CRIT, WHERE, "!ERROR %d (%s) creating directory: %s", i, strerror(i), cfg.temp_dir);
		return false;
	}

	/* Shared NODE files */
	pthread_mutex_lock(&nodefile_mutex);
	if ((nodefile = opennodedat(&cfg)) == -1) {
		pthread_mutex_unlock(&nodefile_mutex);
		errormsg(WHERE, ERR_OPEN, "nodefile", cfg.node_num);
		return false;
	}
	memset(&node, 0, sizeof(node_t));  /* write NULL to node struct */
	node.status = NODE_OFFLINE;
	while (filelength(nodefile) < (int)(cfg.sys_nodes * sizeof(node_t))) {
		lseek(nodefile, 0L, SEEK_END);
		if (write(nodefile, &node, sizeof(node_t)) != sizeof(node_t)) {
			pthread_mutex_unlock(&nodefile_mutex);
			errormsg(WHERE, ERR_WRITE, "nodefile", sizeof(node_t));
			break;
		}
	}
	if (chsize(nodefile, (off_t)(cfg.sys_nodes * sizeof(node_t))) != 0) {
		pthread_mutex_unlock(&nodefile_mutex);
		errormsg(WHERE, ERR_LEN, "ndoefile", cfg.sys_nodes * sizeof(node_t));
	}
	for (i = 0; cfg.node_num > 0 && i < LOOP_NODEDAB; i++) {
		if (lock(nodefile, (cfg.node_num - 1) * sizeof(node_t), sizeof(node_t)) == 0) {
			unlock(nodefile, (cfg.node_num - 1) * sizeof(node_t), sizeof(node_t));
			break;
		}
		FILE_RETRY_DELAY(i + 1);
	}
	if (cfg.node_misc & NM_CLOSENODEDAB) {
		close(nodefile);
		nodefile = -1;
	}
	pthread_mutex_unlock(&nodefile_mutex);

	if (i >= LOOP_NODEDAB) {
		errormsg(WHERE, ERR_LOCK, "nodefile", cfg.node_num);
		return false;
	}

	if (cfg.node_num) {
		SAFEPRINTF(str, "%snode.log", cfg.node_dir);
		if ((logfile_fp = fopen(str, "a+b")) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, 0);
			lprintf(LOG_NOTICE, "Perhaps this node is already running");
			return false;
		}

		if (filelength(fileno(logfile_fp)) > 0) {
			log(crlf);
			time_t ftime = fdate(str);
			llprintf(LOG_NOTICE, "L!",
			              "End of preexisting log entry (possible crash on %s)"
			              , timestr(ftime));
			log(crlf);
			catsyslog(true);
		}

		if (getnodedat(cfg.node_num, &thisnode, true)) {
			/* thisnode.status=0; */
			thisnode.action = 0;
			thisnode.useron = 0;
			thisnode.aux = 0;
			thisnode.misc &= (NODE_EVENT | NODE_LOCK | NODE_RRUN);
			criterrs = thisnode.errors;
			putnodedat(cfg.node_num, &thisnode);
		}

		// remove any pending node messages
		safe_snprintf(str, sizeof(str), "%smsgs/n%3.3u.msg", cfg.data_dir, cfg.node_num);
		fremove(WHERE, str);
		// Delete any stale temporary files (with potentially sensitive content)
		delfiles(cfg.temp_dir, ALLFILES);
		safe_snprintf(str, sizeof(str), "%sMSGTMP", cfg.node_dir);
		removecase(str);
		safe_snprintf(str, sizeof(str), "%sQUOTES.TXT", cfg.node_dir);
		removecase(str);
	}

	/* Reset COMMAND SHELL */

	main_csi.str = (char *)malloc(1024);
	if (main_csi.str == NULL) {
		errormsg(WHERE, ERR_ALLOC, "main_csi.str", 1024);
		return false;
	}
	memset(main_csi.str, 0, 1024);
/***/

	if (cfg.total_grps) {

		usrgrp_total = cfg.total_grps;

		if ((cursub = (int *)malloc(sizeof(int) * usrgrp_total)) == NULL) {
			errormsg(WHERE, ERR_ALLOC, "cursub", sizeof(int) * usrgrp_total);
			return false;
		}

		if ((usrgrp = (int *)malloc(sizeof(int) * usrgrp_total)) == NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrgrp", sizeof(int) * usrgrp_total);
			return false;
		}

		if ((usrsubs = (int *)malloc(sizeof(int) * usrgrp_total)) == NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrsubs", sizeof(int) * usrgrp_total);
			return false;
		}

		if ((usrsub = (int **)calloc(usrgrp_total, sizeof(int *))) == NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrsub", sizeof(int) * usrgrp_total);
			return false;
		}
	}
	if (cfg.total_subs) {
		if ((subscan = (subscan_t *)calloc(cfg.total_subs, sizeof(subscan_t))) == NULL) {
			errormsg(WHERE, ERR_ALLOC, "subscan", sizeof(subscan_t) * cfg.total_subs);
			return false;
		}
	}

	for (i = l = 0; i < cfg.total_grps; i++) {
		for (j = k = 0; j < cfg.total_subs; j++)
			if (cfg.sub[j]->grp == i)
				k++;    /* k = number of subs per grp[i] */
		if (k > l)
			l = k;        /* l = the largest number of subs per grp */
	}
	if (l)
		for (i = 0; i < cfg.total_grps; i++)
			if ((usrsub[i] = (int *)malloc(sizeof(int) * l)) == NULL) {
				errormsg(WHERE, ERR_ALLOC, "usrsub[x]", sizeof(int) * l);
				return false;
			}

	if (cfg.total_libs) {

		usrlib_total = cfg.total_libs;

		if ((curdir = (int *)malloc(sizeof(int) * usrlib_total)) == NULL) {
			errormsg(WHERE, ERR_ALLOC, "curdir", sizeof(int) * usrlib_total);
			return false;
		}

		if ((usrlib = (int *)malloc(sizeof(int) * usrlib_total)) == NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrlib", sizeof(int) * usrlib_total);
			return false;
		}

		if ((usrdirs = (int *)malloc(sizeof(uint) * usrlib_total)) == NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrdirs", sizeof(int) * usrlib_total);
			return false;
		}

		if ((usrdir = (int **)calloc(usrlib_total, sizeof(int *))) == NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrdir", sizeof(int) * usrlib_total);
			return false;
		}
	}

	for (i = l = 0; i < cfg.total_libs; i++) {
		for (j = k = 0; j < cfg.total_dirs; j++)
			if (cfg.dir[j]->lib == i)
				k++;
		if (k > l)
			l = k;        /* l = largest number of dirs in a lib */
	}
	if (l) {
		l++;    /* for temp dir */
		for (i = 0; i < cfg.total_libs; i++)
			if ((usrdir[i] = (int *)malloc(sizeof(int) * l)) == NULL) {
				errormsg(WHERE, ERR_ALLOC, "usrdir[x]", sizeof(int) * l);
				return false;
			}
	}

#ifdef USE_CRYPTLIB
	pthread_mutex_init(&ssh_mutex, NULL);
	ssh_mutex_created = true;
	ssh_active = CreateEvent(nullptr, true, false, nullptr);
#endif
	pthread_mutex_init(&input_thread_mutex, NULL);
	input_thread_mutex_created = true;

	update_terminal(this);
	reset_logon_vars();

	online = ON_REMOTE;

	return true;
}

//****************************************************************************
sbbs_t::~sbbs_t()
{
	int i;

	useron.number = 0;

#ifdef _DEBUG
	lprintf(LOG_DEBUG, "destructor begin");
#endif

//	if(!cfg.node_num)
//		rmdir(cfg.temp_dir);

	if (client_socket_dup != INVALID_SOCKET && client_socket_dup != client_socket)
		closesocket(client_socket_dup); /* close duplicate handle */

	if (cfg.node_num > 0)
		node_inbuf[cfg.node_num - 1] = NULL;
	if (!input_thread_running)
		RingBufDispose(&inbuf);
	if (!output_thread_running && !passthru_thread_running)
		RingBufDispose(&outbuf);

	if (telnet_ack_event != NULL)
		CloseEvent(telnet_ack_event);

	/* Close all open files */
	pthread_mutex_lock(&nodefile_mutex);
	if (nodefile != -1) {
		close(nodefile);
		nodefile = -1;
		pthread_mutex_unlock(&nodefile_mutex);
	}
	if (node_ext != -1) {
		close(node_ext);
		node_ext = -1;
	}
	if (logfile_fp != NULL) {
		fclose(logfile_fp);
		logfile_fp = NULL;
	}

	if (syspage_semfile[0])
		fremove(WHERE, syspage_semfile);

	/********************************/
	/* Free allocated class members */
	/********************************/

	js_cleanup();

	/* Reset text.dat */

	for (i = 0; i < TOTAL_TEXT; i++)
		if (text[i] != text_sav[i]) {
			if (text[i] != nulstr)
				free(text[i]);
		}

	/* Global command shell vars */

	freevars(&main_csi);
	clearvars(&main_csi);
	FREE_AND_NULL(main_csi.str);
	FREE_AND_NULL(main_csi.cs);

	for (i = 0; i < global_str_vars && global_str_var != NULL; i++)
		FREE_AND_NULL(global_str_var[i]);

	FREE_AND_NULL(global_str_var);
	FREE_AND_NULL(global_str_var_name);
	global_str_vars = 0;

	FREE_AND_NULL(global_int_var);
	FREE_AND_NULL(global_int_var_name);
	global_int_vars = 0;

	/* Sub-board variables */
	for (i = 0; i < usrgrp_total && usrsub != NULL; i++)
		FREE_AND_NULL(usrsub[i]);

	FREE_AND_NULL(cursub);
	FREE_AND_NULL(usrgrp);
	FREE_AND_NULL(usrsubs);
	FREE_AND_NULL(usrsub);
	FREE_AND_NULL(subscan);

	/* File Directory variables */
	for (i = 0; i < usrlib_total && usrdir != NULL; i++)
		FREE_AND_NULL(usrdir[i]);

	FREE_AND_NULL(curdir);
	FREE_AND_NULL(usrlib);
	FREE_AND_NULL(usrdirs);
	FREE_AND_NULL(usrdir);

	FREE_AND_NULL(qwknode);
	total_qwknodes = 0;

	listFree(&smb_list);

	free(mod_callstack); // Don't need to free the strings themselves since they're all const ptrs

#ifdef USE_CRYPTLIB
	if (ssh_mutex_created && pthread_mutex_destroy(&ssh_mutex))
		lprintf(LOG_CRIT, "!!!! Failed to destroy ssh_mutex, system is unstable");
	if (ssh_active) {
		SetEvent(ssh_active);
		while ((!CloseEvent(ssh_active)) && errno == EBUSY)
			mswait(1);
		ssh_active = nullptr;
	}
#endif
	if (input_thread_mutex_created && pthread_mutex_destroy(&input_thread_mutex))
		lprintf(LOG_CRIT, "!!!! Failed to destroy input_thread_mutex, system is unstable");

#if 0 && defined(_WIN32) && defined(_DEBUG) && defined(_MSC_VER)
	if (!_CrtCheckMemory())
		lprintf(LOG_ERR, "!MEMORY ERRORS REPORTED IN DATA/DEBUG.LOG!");
#endif

	if (term) {
		delete term;
		term = nullptr;
	}

#ifdef _DEBUG
	lprintf(LOG_DEBUG, "destructor end");
#endif
}

int sbbs_t::smb_stack(smb_t* smb, bool push)
{
	if (push) {
		if (smb == NULL || !SMB_IS_OPEN(smb))
			return SMB_SUCCESS;   /* Msg base not open, do nothing */
		if (listPushNodeData(&smb_list, smb, sizeof(*smb)) == NULL)
			return SMB_FAILURE;
		return SMB_SUCCESS;
	}
	/* pop */
	smb_t* data = (smb_t*)listPopNode(&smb_list);
	if (data == NULL)    /* Nothing on the stack, so do nothing */
		return SMB_SUCCESS;

	*smb = *data;
	free(data);
	return SMB_SUCCESS;
}

/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.													*/
/* All files are opened in BINARY mode, unless O_TEXT access bit is set.	*/
/****************************************************************************/
int sbbs_t::nopen(char *str, int access)
{
	int file, share, count = 0;

	if (access & O_DENYNONE) {
		share = SH_DENYNO;
		access &= ~O_DENYNONE;
	}
	else if (access == O_RDONLY)
		share = SH_DENYWR;
	else
		share = SH_DENYRW;
	if (!(access & O_TEXT))
		access |= O_BINARY;
	while (((file = sopen(str, access, share, DEFFILEMODE)) == -1)
	       && FILE_RETRY_ERRNO(errno) && count++ < LOOP_NOPEN)
		FILE_RETRY_DELAY(count);
	if (count > (LOOP_NOPEN / 2) && count <= LOOP_NOPEN) {
		llprintf(LOG_WARNING, "!!", "NOPEN COLLISION - File: \"%s\" Count: %d"
		            , str, count);
	}
	if (file == -1 && (errno == EACCES || errno == EAGAIN || errno == EDEADLOCK)) {
		llprintf(LOG_WARNING, "!!", "NOPEN ACCESS DENIED - File: \"%s\" errno: %d"
		            , str, errno);
		bputs("\7\r\nNOPEN: ACCESS DENIED\r\n\7");
	}
	return file;
}

void sbbs_t::spymsg(const char* msg)
{
	char str[512];

	if (cfg.node_num < 1)
		return;

	SAFEPRINTF4(str, "\r\n\r\n*** Spy Message ***\r\nNode %d: %s [%s]\r\n*** %s ***\r\n\r\n"
	            , cfg.node_num, client_name, client_ipaddr, msg);
	if (startup->node_spybuf != NULL
	    && startup->node_spybuf[cfg.node_num - 1] != NULL) {
		RingBufWrite(startup->node_spybuf[cfg.node_num - 1], (uchar*)str, strlen(str));
	}

	if (cfg.node_num && spy_socket[cfg.node_num - 1] != INVALID_SOCKET)
		if (sendsocket(spy_socket[cfg.node_num - 1], str, strlen(str)) < 1)
			errprintf(LOG_ERR, WHERE, "Node %d ERROR %d writing to spy socket", cfg.node_num, SOCKET_ERRNO);
#ifdef __unix__
	if (cfg.node_num && uspy_socket[cfg.node_num - 1] != INVALID_SOCKET)
		if (sendsocket(uspy_socket[cfg.node_num - 1], str, strlen(str)) < 1)
			errprintf(LOG_ERR, WHERE, "Node %d ERROR %d writing to spy socket", cfg.node_num, SOCKET_ERRNO);
#endif
}

/****************************************************************************/
/* Moves or copies a file from one dir to another                           */
/* both 'src' and 'dest' must contain full path and filename                */
/* returns 0 if successful, -1 if error                                     */
/****************************************************************************/
int sbbs_t::mv(const char* path, const char* dest, bool copy)
{
	char src[MAX_PATH + 1];

	if (!stricmp(path, dest))     /* source and destination are the same! */
		return 0;

	SAFECOPY(src, path);
	if (!fexistcase(src)) {
		bprintf("\r\n\7MV ERROR: Source doesn't exist\r\n'%s'\r\n"
		        , src);
		return -1;
	}
	if (!copy && fexist(dest)) {
		bprintf("\r\n\7MV ERROR: Destination already exists\r\n'%s'\r\n"
		        , dest);
		return -1;
	}
	if (!copy && rename(src, dest) == 0)
		return 0;
	if (!CopyFile(src, dest, /* fail if exists: */ !copy)) {
		errormsg(WHERE, "CopyFile", src, 0, dest);
		return -1;
	}
	if (!copy && remove(src) != 0) {
		errormsg(WHERE, ERR_REMOVE, src, 0);
		return -1;
	}
	return 0;
}

void sbbs_t::hangup(void)
{
	if (online) {
		term->clear_hotspots();
		lprintf(LOG_DEBUG, "disconnecting client");
		online = false;   // moved from the bottom of this function on Jan-25-2009
	}
	if (client_socket_dup != INVALID_SOCKET && client_socket_dup != client_socket)
		closesocket(client_socket_dup);
	client_socket_dup = INVALID_SOCKET;

	if (client_socket != INVALID_SOCKET) {
		mswait(1000);   /* Give socket output buffer time to flush */
		client_off(client_socket);
		if (ssh_mode) {
			pthread_mutex_lock(&ssh_mutex);
			ssh_session_destroy(client_socket, ssh_session, __LINE__);
			ssh_mode = false;
			pthread_mutex_unlock(&ssh_mutex);
		}
		close_socket(client_socket);
		client_socket = INVALID_SOCKET;
	}
	SetEvent(outbuf.data_event);
	SetEvent(outbuf.highwater_event);
}

int sbbs_t::incom(unsigned int timeout)
{
	uchar ch;

	if (!online)
		return NOINP;

	// If we think we may have some input, send all our output
	if (RingBufFull(&outbuf) != 0) {
		SetEvent(outbuf.highwater_event);
		SetEvent(outbuf.data_event);
	}
#if 0   /* looping version */
	while (!RingBufRead(&inbuf, &ch, 1))
		if (WaitForEvent(inbuf.data_event, timeout) != WAIT_OBJECT_0 || sys_status & SS_ABORT)
			return NOINP;
#else
	if (!RingBufRead(&inbuf, &ch, 1)) {
		if (WaitForEvent(inbuf.data_event, timeout) != WAIT_OBJECT_0)
			return NOINP;
		if (!RingBufRead(&inbuf, &ch, 1))
			return NOINP;
	}
#endif
	return ch;
}

// Steve's original implementation (in RCIOL) did not incorporate a retry
// ... so this function does not either. :-P
int sbbs_t::_outcom(uchar ch)
{
	if (!RingBufFree(&outbuf))
		return TXBOF;
	if (!RingBufWrite(&outbuf, &ch, 1))
		return TXBOF;
	return 0;
}

// This outcom version retries - copied loop from sbbs_t::outchar()
int sbbs_t::outcom(uchar ch)
{
	int i = 0;
	while (_outcom(ch) != 0) {
		if (!online)
			break;
		i++;
		if (i >= outcom_max_attempts) {          /* timeout - beep flush outbuf */
			lprintf(LOG_NOTICE, "%04d %s TIMEOUT after %d attempts with %d bytes in transmit buffer (purging)"
			        , client_socket.load(), __FUNCTION__, i, RingBufFull(&outbuf));
			RingBufReInit(&outbuf);
			_outcom(BEL);
			return TXBOF;
		}
		if (sys_status & SS_SYSPAGE)
			sbbs_beep(i, OUTCOM_RETRY_DELAY);
		else
			flush_output(OUTCOM_RETRY_DELAY);
	}
	return 0;   // Success
}

int sbbs_t::putcom(const char *str, size_t len)
{
	size_t i;

	if (!len)
		len = strlen(str);
	for (i = 0; i < len && online; i++)
		if (outcom(str[i]) != 0)
			break;
	return i;
}

/* Legacy Remote I/O Control Interface:
 * This function mimics the RCIOL MS-DOS library written in 8086 assembler by Steven B. Deppe (1958-2014).
 * This function prototype shall remain the same in tribute to Steve (Ille Homine Albe).
 */
int sbbs_t::rioctl(ushort action)
{
	int mode;
	int state;

	switch (action) {
		case GVERS:     /* Get version */
			return 0x200;
		case GUART:     /* Get UART I/O address, not available */
			return 0xffff;
		case GIRQN:     /* Get IRQ number, not available */
			return (int)client_socket;
		case GBAUD:     /* Get current bit rate */
			return 0xffff;
		case RXBC:      /* Get receive buffer count */
			// ulong	cnt;
			// ioctlsocket (client_socket,FIONREAD,&cnt);
			return /* cnt+ */ RingBufFull(&inbuf);
		case RXBS:      /* Get receive buffer size */
			return inbuf.size;
		case TXBC:      /* Get transmit buffer count */
			return RingBufFull(&outbuf);
		case TXBS:      /* Get transmit buffer size */
			return outbuf.size;
		case TXBF:      /* Get transmit buffer free space */
			return RingBufFree(&outbuf);
		case IOMODE:
			mode = 0;
			if (rio_abortable)
				mode |= ABORT;
			return mode;
		case IOSTATE:
			state = 0;
			if (sys_status & SS_ABORT)
				state |= ABORT;
			return state;
		case IOFI:      /* Flush input buffer */
			RingBufReInit(&inbuf);
			break;
		case IOFO:      /* Flush output buffer */
			RingBufReInit(&outbuf);
			break;
		case IOFB:      /* Flush both buffers */
			RingBufReInit(&inbuf);
			RingBufReInit(&outbuf);
			break;
		case LFN81:
		case LFE71:
		case FIFOCTL:
			return 0;
	}

	if ((action & 0xff) == IOSM) {   /* Get/Set/Clear mode */
		if (action & ABORT)
			rio_abortable = true;
		return 0;
	}

	if ((action & 0xff) == IOCM) {   /* Get/Set/Clear mode */
		if (action & ABORT)
			rio_abortable = false;
		return 0;
	}

	if ((action & 0xff) == IOSS) {   /* Set state */
		if (action & ABORT)
			sys_status |= SS_ABORT;
		return 0;
	}

	if ((action & 0xff) == IOCS) {   /* Clear state */
		if (action & ABORT)
			clearabort();
		return 0;
	}

	return 0;
}

void sbbs_t::reset_logon_vars(void)
{
	int i;

	sys_status &= ~(SS_USERON | SS_TMPSYSOP | SS_LCHAT | SS_ABORT
	                | SS_PAUSEON | SS_PAUSEOFF | SS_EVENT | SS_NEWUSER | SS_DATE_CHANGED | SS_QWKLOGON | SS_FASTLOGON);
	cid[0] = 0;
	wordwrap[0] = 0;
	question[0] = 0;
	if (term) {
		term->row = 0;
		term->rows = startup->default_term_height;
		term->cols = startup->default_term_width;
		term->lncntr = 0;
		term->cterm_version = 0;
		term->lbuflen = 0;
		term->cur_output_rate = output_rate_unlimited;
	}
	autoterm = 0;
	timeleft_warn = 0;
	keybufbot = keybuftop = 0;
	usrgrps = usrlibs = 0;
	curgrp = curlib = 0;
	for (i = 0; i < cfg.total_libs; i++)
		curdir[i] = 0;
	for (i = 0; i < cfg.total_grps; i++)
		cursub[i] = 0;
	cur_rate = 30000;
	dte_rate = 38400;
	main_cmds = xfer_cmds = posts_read = 0;
	lastnodemsg = 0;
	lastnodemsguser[0] = 0;
}

/****************************************************************************/
/* Writes NODE.LOG at end of SYSTEM.LOG										*/
/****************************************************************************/
void sbbs_t::catsyslog(int crash)
{
	char str[MAX_PATH + 1] = "node.log";
	char *buf;
	FILE* fp;
	int i, file;
	int length;
	struct tm tm;

	if (logfile_fp == NULL) {
		SAFEPRINTF(str, "%snode.log", cfg.node_dir);
		if ((logfile_fp = fopen(str, "rb")) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, O_RDONLY);
			return;
		}
	}
	length = ftell(logfile_fp);
	if (length > 0) {
		if ((buf = (char *)malloc(length)) == NULL) {
			errormsg(WHERE, ERR_ALLOC, str, length);
			return;
		}
		rewind(logfile_fp);
		if (fread(buf, 1, length, logfile_fp) != (size_t)length) {
			errormsg(WHERE, ERR_READ, "log file", length);
			free((char *)buf);
			return;
		}
		now = time(NULL);
		localtime_r(&now, &tm);
		SAFEPRINTF4(str, "%slogs/%2.2d%2.2d%2.2d.log", cfg.logs_dir, tm.tm_mon + 1, tm.tm_mday
		            , TM_YEAR(tm.tm_year));
		if ((file = nopen(str, O_WRONLY | O_APPEND | O_CREAT)) == -1) {
			errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_APPEND | O_CREAT);
			free((char *)buf);
			return;
		}
		if (write(file, buf, length) != length) {
			close(file);
			errormsg(WHERE, ERR_WRITE, str, length);
			free((char *)buf);
			return;
		}
		close(file);
		if (crash) {
			for (i = 0; i < 2; i++) {
				SAFEPRINTF(str, "%scrash.log", i ? cfg.logs_dir : cfg.node_dir);
				if ((fp = fopenlog(&cfg, str, /* shareable: */false)) == NULL) {
					errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_APPEND | O_CREAT);
					free((char *)buf);
					return;
				}
				if (fwrite(buf, sizeof(uint8_t), length, fp) != (size_t)length) {
					fcloselog(fp);
					errormsg(WHERE, ERR_WRITE, str, length);
					free((char *)buf);
					return;
				}
				fcloselog(fp);
			}
		}
		free((char *)buf);
	}

	fclose(logfile_fp);

	SAFEPRINTF(str, "%snode.log", cfg.node_dir);
	if ((logfile_fp = fopen(str, "w+b")) == NULL) /* Truncate NODE.LOG */
		errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_TRUNC);
}


void sbbs_t::logoffstats()
{
	int i;
	stats_t stats;

	if (user_is_sysop(&useron) && !(cfg.sys_misc & SM_SYSSTAT))
		return;

	if (useron.rest & FLAG('Q'))   /* Don't count QWKnet nodes */
		return;

	now = time(NULL);
	if (now <= logontime) {
		lprintf(LOG_WARNING, "Logoff time (%u) <= logon time (%u): %s"
		        , (uint)now, (uint)logontime, timestr(logontime));
		return;
	}
	uint minutes_used = (uint)(now - logontime) / 60;
	if (minutes_used < 1)
		return;

	for (i = 0; i < 2; i++) {
		FILE* fp = fopen_dstats(&cfg, i ? 0 : cfg.node_num, /* for_write: */ true);
		if (fp == NULL)
			continue;
		if (!fread_dstats(fp, &stats)) {
			errormsg(WHERE, ERR_READ, "dsts.ini", i);
		} else {
			stats.total.timeon += minutes_used;
			stats.today.timeon += minutes_used;
			if (!fwrite_dstats(fp, &stats, __FUNCTION__))
				errormsg(WHERE, ERR_WRITE, "dsts.ini", i);
		}
		fclose_dstats(fp);
	}
}


void sbbs_t::register_login()
{
	if (user_login_state >= user_logged_in)
		return;
	if (useron.pass[0]) {
		loginSuccess(startup->login_attempt_list, &client_addr);
		listAddNodeData(&current_logins, client.addr, strlen(client.addr) + 1, cfg.node_num, LAST_NODE);
	}
#ifdef _WIN32
	if (startup->sound.login[0] && !sound_muted(&cfg))
		PlaySound(startup->sound.login, NULL, SND_ASYNC | SND_FILENAME);
#endif
	user_login_state = user_logged_in;
}

void node_thread(void* arg)
{
	char str[MAX_PATH + 1];
	int file;
	int curshell = -1;
	node_t node;
	uint login_attempts;
	sbbs_t*         sbbs = (sbbs_t*) arg;

	update_clients();
	SetThreadName("sbbs/termNode");
	thread_up(true /* setuid */);

#ifdef _DEBUG
	lprintf(LOG_DEBUG, "Node %d thread started", sbbs->cfg.node_num);
#endif

	sbbs_srand();       /* Seed random number generator */

#ifdef JAVASCRIPT
	if (!(startup->options & BBS_OPT_NO_JAVASCRIPT)) {
		if ((sbbs->js_cx = sbbs->js_init(&sbbs->js_runtime, &sbbs->js_glob, "node")) == NULL) /* This must be done in the context of the node thread */
			errprintf(LOG_CRIT, WHERE, "Node %d !JavaScript Initialization FAILURE", sbbs->cfg.node_num);
	}
#endif

	if (startup->login_attempt.throttle
	    && (login_attempts = loginAttempts(startup->login_attempt_list, &sbbs->client_addr)) > 1) {
		lprintf(LOG_DEBUG, "Node %d Throttling suspicious connection from: %s (%u login attempts)"
		        , sbbs->cfg.node_num, sbbs->client_ipaddr, login_attempts);
		mswait(login_attempts * startup->login_attempt.throttle);
	}

	if (sbbs->answer()) {

		sbbs->register_login();

		if (sbbs->term_output_disabled) { // e.g. SFTP
			while (sbbs->online) {
				SLEEP(1000);
				sbbs->getnodedat(sbbs->cfg.node_num, &sbbs->thisnode, /* lock: */ false);
				if ((sbbs->thisnode.misc & NODE_UDAT) && !sbbs->useron_is_guest()) {
					if (sbbs->getuseron(WHERE) && sbbs->getnodedat(sbbs->cfg.node_num, &sbbs->thisnode, /* lock: */ true)) {
						sbbs->thisnode.misc &= ~NODE_UDAT;
						sbbs->putnodedat(sbbs->cfg.node_num, &sbbs->thisnode);
					}
				}
				if (sbbs->thisnode.misc & NODE_INTR) {
					sbbs->logline(LOG_NOTICE, nulstr, "Interrupted");
					sbbs->hangup();
				}
			}
		}
		else if (sbbs->sys_status & SS_QWKLOGON) {
			sbbs->getsmsg(sbbs->useron.number);
			sbbs->qwk_sec();
		} else while (sbbs->useron.number
			          && (sbbs->main_csi.misc & CS_OFFLINE_EXEC || sbbs->online)) {

				if (curshell != sbbs->useron.shell) {
					if (sbbs->useron.shell >= sbbs->cfg.total_shells)
						sbbs->useron.shell = 0;
					sbbs->menu_dir[0] = 0;
					sbbs->menu_file[0] = 0;

					if (sbbs->cfg.mods_dir[0]) {
						SAFEPRINTF2(str, "%s%s.js", sbbs->cfg.mods_dir
						            , sbbs->cfg.shell[sbbs->useron.shell]->code);
						if (fexistcase(str)) {
							if (sbbs->js_execfile(str) != 0)
								break;
							continue;
						}
					}
					SAFEPRINTF2(str, "%s%s.js", sbbs->cfg.exec_dir
					            , sbbs->cfg.shell[sbbs->useron.shell]->code);
					if (fexistcase(str)) {
						if (sbbs->js_execfile(str) != 0)
							break;
						continue;
					}
					SAFEPRINTF2(str, "%s%s.bin", sbbs->cfg.mods_dir
					            , sbbs->cfg.shell[sbbs->useron.shell]->code);
					if (sbbs->cfg.mods_dir[0] == 0 || !fexistcase(str)) {
						SAFEPRINTF2(str, "%s%s.bin", sbbs->cfg.exec_dir
						            , sbbs->cfg.shell[sbbs->useron.shell]->code);
						(void)fexistcase(str);
					}
					if ((file = sbbs->nopen(str, O_RDONLY)) == -1) {
						sbbs->errormsg(WHERE, ERR_OPEN, str, O_RDONLY);
						sbbs->hangup();
						break;
					}
					FREE_AND_NULL(sbbs->main_csi.cs);
					sbbs->freevars(&sbbs->main_csi);
					sbbs->clearvars(&sbbs->main_csi);

					sbbs->main_csi.length = (int)filelength(file);
					if (sbbs->main_csi.length < 1) {
						close(file);
						sbbs->errormsg(WHERE, ERR_LEN, str, sbbs->main_csi.length);
						sbbs->hangup();
						break;
					}

					if ((sbbs->main_csi.cs = (uchar *)malloc(sbbs->main_csi.length)) == NULL) {
						close(file);
						sbbs->errormsg(WHERE, ERR_ALLOC, str, sbbs->main_csi.length);
						sbbs->hangup();
						break;
					}

					if (read(file, sbbs->main_csi.cs, sbbs->main_csi.length)
					    != (int)sbbs->main_csi.length) {
						sbbs->errormsg(WHERE, ERR_READ, str, sbbs->main_csi.length);
						close(file);
						free(sbbs->main_csi.cs);
						sbbs->main_csi.cs = NULL;
						sbbs->hangup();
						break;
					}
					close(file);

					curshell = sbbs->useron.shell;
					sbbs->main_csi.ip = sbbs->main_csi.cs;
				}
				if (sbbs->exec(&sbbs->main_csi))
					break;
			}
		listRemoveTaggedNode(&current_logins, sbbs->cfg.node_num, /* free_data */ true);
		sbbs->logoffstats();    /* Updates both system and node dsts.ini (daily statistics) files */
	}

#ifdef _WIN32
	if (startup->sound.hangup[0] && !sound_muted(&scfg))
		PlaySound(startup->sound.hangup, NULL, SND_ASYNC | SND_FILENAME);
#endif

	sbbs->hangup(); /* closes sockets, calls client_off, and shuts down the output_thread */

	sbbs->logout();

	time_t now = time(NULL);
	SAFEPRINTF(str, "%sclient.ini", sbbs->cfg.node_dir);
	FILE* fp = fopen(str, "at");
	if (fp != NULL) {
		fprintf(fp, "user=%u\n", sbbs->useron.number);
		fprintf(fp, "name=%s\n", sbbs->useron.alias);
		fprintf(fp, "done=%u\n", (uint)now);
		fclose(fp);
	}

	if (sbbs->sys_status & SS_DAILY) { // New day, run daily events/maintenance
		sbbs->daily_maint();
	}

#if 0   /* this is handled in the event_thread now */
	// Node Daily Event
	sbbs->getnodedat(sbbs->cfg.node_num, &node, 0);
	if (node.misc & NODE_EVENT) {
		sbbs->getnodedat(sbbs->cfg.node_num, &node, 1);
		node.status = NODE_EVENT_RUNNING;
		sbbs->putnodedat(sbbs->cfg.node_num, &node);
		if (sbbs->cfg.node_daily[0]) {
			sbbs->logentry("!:", "Run node daily event");
			sbbs->external(
				sbbs->cmdstr(sbbs->cfg.node_daily, nulstr, nulstr, NULL)
				, EX_OFFLINE);
		}
		sbbs->getnodedat(sbbs->cfg.node_num, &node, 1);
		node.misc &= ~NODE_EVENT;
		sbbs->putnodedat(sbbs->cfg.node_num, &node);
	}
#endif

	// Wait for all node threads to terminate
	if (sbbs->input_thread_running || sbbs->output_thread_running
	    || sbbs->passthru_thread_running
	    ) {
		lprintf(LOG_INFO, "Node %d Waiting for %s to terminate..."
		        , sbbs->cfg.node_num
		        , (sbbs->input_thread_running && sbbs->output_thread_running) ?
		        "I/O threads" : sbbs->input_thread_running
		        ? "input thread" : "output thread");
		time_t start = time(NULL);
		while (sbbs->input_thread_running
		       || sbbs->output_thread_running
		       || sbbs->passthru_thread_running
		       ) {
			if (time(NULL) - start > TIMEOUT_THREAD_WAIT) {
				lprintf(LOG_NOTICE, "Node %d !TIMEOUT waiting for %s to terminate"
				        , sbbs->cfg.node_num
				        , (sbbs->input_thread_running && sbbs->output_thread_running) ?
				        "I/O threads"
				    : sbbs->input_thread_running
				        ? "input thread" : "output thread");
				break;
			}
			mswait(100);
		}
	}

	if (sbbs->user_login_state > sbbs->user_not_logged_in)
		sbbs->catsyslog(/* Crash: */ false);
	else {
		rewind(sbbs->logfile_fp);
		if (chsize(fileno(sbbs->logfile_fp), 0) != 0)
			sbbs->errormsg(WHERE, "truncating", "logfile", 0);
	}

	if (sbbs->getnodedat(sbbs->cfg.node_num, &node, true)) {
		node_socket[sbbs->cfg.node_num - 1] = INVALID_SOCKET;
		if (node.misc & NODE_DOWN)
			node.status = NODE_OFFLINE;
		else
			node.status = NODE_WFC;
		node.misc &= ~(NODE_DOWN | NODE_INTR | NODE_MSGW | NODE_NMSG
		               | NODE_UDAT | NODE_POFF | NODE_AOFF | NODE_EXT);
		/*	node.useron=0; needed for hang-ups while in multinode chat */
		sbbs->putnodedat(sbbs->cfg.node_num, &node);
	} else
		node_socket[sbbs->cfg.node_num - 1] = INVALID_SOCKET;

	{
		/* crash here on Aug-4-2015:
		node_thread_running already destroyed
		bbs_thread() timed out waiting for 1 node thread(s) to terminate */
		uint32_t remain = protected_uint32_adjust_fetch(&node_threads_running, -1);
		lprintf(LOG_INFO, "Node %d thread terminated (%u node threads remain, %u clients served)"
		        , sbbs->cfg.node_num, remain, served);
	}
	if (!sbbs->input_thread_running && !sbbs->output_thread_running)
		delete sbbs;
	else
		lprintf(LOG_WARNING, "Node %d !ORPHANED I/O THREAD(s)", sbbs->cfg.node_num);

	/* crash here July-27-2018:
	ntdll.dll!77282e19()	Unknown
	[Frames below may be incorrect and/or missing, no symbols loaded for ntdll.dll]
	[External Code]
	sbbs.dll!pthread_mutex_lock(_RTL_CRITICAL_SECTION * mutex) Line 171	C
	sbbs.dll!protected_uint32_adjust(protected_uint32_t * i, int adjustment) Line 244	C
	sbbs.dll!update_clients() Line 185	C++
	>	sbbs.dll!node_thread(void * arg) Line 4568	C++
	[External Code]

	node_threads_running	{value=0 mutex={DebugInfo=0x00000000 <NULL> LockCount=-6 RecursionCount=0 ...} }	protected_uint32_t

	and again on July-10-2019:

	ntdll.dll!RtlpWaitOnCriticalSection()	Unknown
	ntdll.dll!RtlpEnterCriticalSectionContended()	Unknown
	ntdll.dll!_RtlEnterCriticalSection@4()	Unknown
	sbbs.dll!pthread_mutex_lock(_RTL_CRITICAL_SECTION * mutex) Line 171	C
	sbbs.dll!protected_uint32_adjust(protected_uint32_t * i, int adjustment) Line 244	C
	sbbs.dll!update_clients() Line 187	C++
	>	sbbs.dll!node_thread(void * arg) Line 4668	C++
	*/
	update_clients();
	thread_down();
}

// This version of backup() returns the size of the file backed-up
int64_t sbbs_t::backup(const char* fname, int backup_level, bool rename)
{
	int64_t bytes = flength(fname);
	if (bytes < 0)
		return 0;

	lprintf(LOG_DEBUG, "Backing-up %s (%" PRId64 " bytes)", fname, bytes);
	if (!backup(fname, backup_level, rename))
		return false;
	return bytes;
}

void sbbs_t::daily_maint(void)
{
	char str[128];
	char uname[LEN_ALIAS + 1];
	uint i;
	uint usernum;
	uint lastusernum;
	int  mail;
	user_t user;

	lputs(LOG_INFO, "DAILY: System maintenance begun");
	now = time(NULL);

	if (cfg.node_num) {
		if (getnodedat(cfg.node_num, &thisnode, true)) {
			thisnode.status = NODE_EVENT_RUNNING;
			putnodedat(cfg.node_num, &thisnode);
		}
	}

	if (cfg.user_backup_level && (lastusernum = lastuser(&cfg)) > 0) {
		lputs(LOG_DEBUG, "DAILY: Backing up user data...");
		SAFEPRINTF(str, "%suser/" USER_DATA_FILENAME, cfg.data_dir);
		int64_t bytes = backup(str, cfg.user_backup_level, false);
		SAFEPRINTF(str, "%suser/" USER_INDEX_FILENAME, cfg.data_dir);
		bytes += backup(str, cfg.user_backup_level, false);
		lprintf(LOG_INFO, "DAILY: Backed up %s bytes of user data (%u users)"
			, byte_estimate_to_str(bytes, str, sizeof str, 1024 * 1024, 1)
			, lastusernum);
	}

	if (cfg.mail_backup_level && (mail = getmail(&cfg, 0, false, /* attr: */ 0)) > 0) {
		lputs(LOG_DEBUG, "DAILY: Backing up mail data...");
		smb_t mail;
		int result = smb_open_sub(&cfg, &mail, INVALID_SUB);
		if (result != SMB_SUCCESS)
			errprintf(LOG_ERR, WHERE, "ERROR %d (%s) opening mail base", result, mail.last_error);
		else {
			result = smb_lock(&mail);
			if (result != SMB_SUCCESS)
				errprintf(LOG_ERR, WHERE, "ERROR %d (%s) locking mail base", result, mail.last_error);
			else {
				SAFEPRINTF(str, "%smail.shd", cfg.data_dir);
				int64_t bytes = backup(str, cfg.mail_backup_level, false);
				SAFEPRINTF(str, "%smail.sha", cfg.data_dir);
				bytes += backup(str, cfg.mail_backup_level, false);
				SAFEPRINTF(str, "%smail.sdt", cfg.data_dir);
				bytes += backup(str, cfg.mail_backup_level, false);
				SAFEPRINTF(str, "%smail.sda", cfg.data_dir);
				bytes += backup(str, cfg.mail_backup_level, false);
				SAFEPRINTF(str, "%smail.sid", cfg.data_dir);
				bytes += backup(str, cfg.mail_backup_level, false);
				SAFEPRINTF(str, "%smail.sch", cfg.data_dir);
				bytes += backup(str, cfg.mail_backup_level, false);
				SAFEPRINTF(str, "%smail.hash", cfg.data_dir);
				bytes += backup(str, cfg.mail_backup_level, false);
				SAFEPRINTF(str, "%smail.ini", cfg.data_dir);
				bytes += backup(str, cfg.mail_backup_level, false);
				lprintf(LOG_INFO, "DAILY: Backed up %s bytes of mail data (%u messages)"
					, byte_estimate_to_str(bytes, str, sizeof str, 1024 * 1024, 1)
					, mail);
				result = smb_unlock(&mail);
				if (result != SMB_SUCCESS)
					errprintf(LOG_ERR, WHERE, "ERROR %d (%s) unlocking mail base", result, mail.last_error);
			}
			smb_close(&mail);
		}
	}

	lputs(LOG_INFO, "DAILY: Checking for inactive/expired user records...");
	lastusernum = lastuser(&cfg);
	int userfile = openuserdat(&cfg, /* for_modify: */ false);
	for (usernum = 1; usernum <= lastusernum; usernum++) {
		user.number = usernum;
		if ((i = fgetuserdat(&cfg, &user, userfile)) != 0) {
			SAFEPRINTF(str, "user record %u", usernum);
			errormsg(WHERE, ERR_READ, str, i);
			continue;
		}

		/***********************************************/
		/* Fix name (name.dat and user.tab) mismatches */
		/***********************************************/
		username(&cfg, user.number, uname);
		if (user.misc & DELETED) {
			if (strcmp(uname, "DELETED USER"))
				putusername(&cfg, user.number, nulstr);
			continue;
		}

		if (strcmp(user.alias, uname))
			putusername(&cfg, user.number, user.alias);

		if (user.number == 1)
			continue;   /* skip expiration/inactivity checks for user #1 */

		if (!(user.misc & (DELETED | INACTIVE))
		    && user.expire && (uint)user.expire <= (uint)now) {
			putsmsg(user.number, text[AccountHasExpired]);
			SAFEPRINTF2(str, "DAILY: %s #%u Expired", user.alias, user.number);
			lputs(LOG_NOTICE, str);
			if (cfg.level_misc[user.level] & LEVEL_EXPTOVAL
			    && cfg.level_expireto[user.level] < 10) {
				user.flags1 = cfg.val_flags1[cfg.level_expireto[user.level]];
				user.flags2 = cfg.val_flags2[cfg.level_expireto[user.level]];
				user.flags3 = cfg.val_flags3[cfg.level_expireto[user.level]];
				user.flags4 = cfg.val_flags4[cfg.level_expireto[user.level]];
				user.exempt = cfg.val_exempt[cfg.level_expireto[user.level]];
				user.rest = cfg.val_rest[cfg.level_expireto[user.level]];
				if (cfg.val_expire[cfg.level_expireto[user.level]])
					user.expire = (time32_t)now
					              + (cfg.val_expire[cfg.level_expireto[user.level]] * 24 * 60 * 60);
				else
					user.expire = 0;
				user.level = cfg.val_level[cfg.level_expireto[user.level]];
			}
			else {
				if (cfg.level_misc[user.level] & LEVEL_EXPTOLVL)
					user.level = cfg.level_expireto[user.level];
				else
					user.level = cfg.expired_level;
				user.flags1 &= ~cfg.expired_flags1; /* expired status */
				user.flags2 &= ~cfg.expired_flags2; /* expired status */
				user.flags3 &= ~cfg.expired_flags3; /* expired status */
				user.flags4 &= ~cfg.expired_flags4; /* expired status */
				user.exempt &= ~cfg.expired_exempt;
				user.rest |= cfg.expired_rest;
				user.expire = 0;
			}
			putuserdec32(user.number, USER_LEVEL, user.level);
			putuserflags(user.number, USER_FLAGS1, user.flags1);
			putuserflags(user.number, USER_FLAGS2, user.flags2);
			putuserflags(user.number, USER_FLAGS3, user.flags3);
			putuserflags(user.number, USER_FLAGS4, user.flags4);
			putuserdatetime(user.number, USER_EXPIRE, user.expire);
			putuserflags(user.number, USER_EXEMPT, user.exempt);
			putuserflags(user.number, USER_REST, user.rest);
			useron = user;
			online = ON_LOCAL;
			exec_mod("expired user", cfg.expire_mod);
			online = false;
		}

		/***********************************************************/
		/* Auto deletion based on expiration date or days inactive */
		/***********************************************************/
		if (!(user.exempt & FLAG('P'))     /* Not a permanent account */
		    && !(user.misc & (DELETED | INACTIVE))   /* alive */
		    && (cfg.sys_autodel && (now - user.laston) / (int)(24L * 60L * 60L)
		        > cfg.sys_autodel)) {       /* Inactive too long */
			lprintf(LOG_NOTICE, "DAILY: Auto-Deleting user: %s (%s) #%u due to inactivity > %u days (pending e-mail: %d)"
			        , user.alias, user.name, user.number, cfg.sys_autodel, delallmail(user.number, MAIL_ANY));
			del_user(&cfg, &user);
		}
	}
	closeuserdat(userfile);

	lputs(LOG_INFO, "DAILY: Purging deleted/expired e-mail");
	SAFEPRINTF(smb.file, "%smail", cfg.data_dir);
	smb.retry_time = cfg.smb_retry_time;
	smb.subnum = INVALID_SUB;
	if ((i = smb_open(&smb)) != 0)
		errormsg(WHERE, ERR_OPEN, smb.file, i, smb.last_error);
	else {
		if (filelength(fileno(smb.shd_fp)) > 0) {
			if ((i = smb_lock(&smb)) != SMB_SUCCESS)
				errormsg(WHERE, ERR_LOCK, smb.file, i, smb.last_error);
			else {
				lprintf(LOG_INFO, "DAILY: Removed %d e-mail messages", delmail(0, MAIL_ALL));
				smb_unlock(&smb);
			}
		}
		smb_close(&smb);
	}

	online = ON_LOCAL;
	if (cfg.sys_daily.cmd[0] && !(cfg.sys_daily.misc & EVENT_DISABLED)) {
		lputs(LOG_INFO, "DAILY: Running system event");
		const char* cmd = cmdstr(cfg.sys_daily.cmd, nulstr, nulstr, NULL, cfg.sys_daily.misc);
		int result = external(cmd, EX_OFFLINE | cfg.sys_daily.misc);
		lprintf(result ? LOG_ERR : LOG_INFO, "Daily event: '%s' returned %d", cmd, result);
	}
	if ((sys_status & SS_NEW_WEEK) && cfg.sys_weekly.cmd[0] && !(cfg.sys_weekly.misc & EVENT_DISABLED)) {
		lputs(LOG_INFO, "DAILY: Running weekly event");
		const char* cmd = cmdstr(cfg.sys_weekly.cmd, nulstr, nulstr, NULL, cfg.sys_weekly.misc);
		int result = external(cmd, EX_OFFLINE | cfg.sys_monthly.misc);
		lprintf(result ? LOG_ERR : LOG_INFO, "Weekly event: '%s' returned %d", cmd, result);
	}
	if ((sys_status & SS_NEW_MONTH) && cfg.sys_monthly.cmd[0] && !(cfg.sys_monthly.misc & EVENT_DISABLED)) {
		lputs(LOG_INFO, "DAILY: Running monthly event");
		const char* cmd = cmdstr(cfg.sys_monthly.cmd, nulstr, nulstr, NULL, cfg.sys_monthly.misc);
		int result = external(cmd, EX_OFFLINE | cfg.sys_monthly.misc);
		lprintf(result ? LOG_ERR : LOG_INFO, "Monthly event: '%s' returned %d", cmd, result);
	}
	online = false;
	lputs(LOG_INFO, "DAILY: System maintenance ended");
	sys_status &= ~SS_DAILY;
}

const char* js_ver(void)
{
#ifdef JAVASCRIPT
	return JS_GetImplementationVersion();
#else
	return "";
#endif
}

/* Returns char string of version and revision */
const char* bbs_ver(void)
{
	static char ver[256];
	char compiler[32];

	if (ver[0] == 0) { /* uninitialized */
		DESCRIBE_COMPILER(compiler);

		safe_snprintf(ver, sizeof(ver), "%s %s%c%s  Compiled %s/%s %s with %s"
		              , TELNET_SERVER
		              , VERSION, REVISION
#ifdef _DEBUG
		              , " Debug"
#else
		              , ""
#endif
		              , git_branch, git_hash
		              , git_date, compiler
		              );
	}
	return ver;
}

/* Returns binary-coded version and revision (e.g. 0x31000 == 3.10a) */
int bbs_ver_num(void)
{
	return VERSION_HEX;
}

void bbs_terminate(void)
{
	lprintf(LOG_INFO, "BBS Server terminate");
	terminate_server = true;
}

extern bbs_startup_t bbs_startup;
static void cleanup(int code)
{
	lputs(LOG_INFO, "Terminal Server thread terminating");

	xpms_destroy(ts_set, sock_close_cb, startup);
	ts_set = NULL;

#ifdef _WINSOCKAPI_
	if (WSAInitialized && WSACleanup() != 0)
		lprintf(LOG_ERR, "!WSACleanup ERROR %d", SOCKET_ERRNO);
#endif

	free_cfg(&scfg);
	free_text(text);

	for (int i = 0; i < MAX_NODES; i++) {
		free_text(node_text[i]);
		free_cfg(&node_scfg[i]);
		memset(&node_scfg[i], 0, sizeof(node_scfg[i]));
	}

	semfile_list_free(&pause_semfiles);
	semfile_list_free(&recycle_semfiles);
	semfile_list_free(&shutdown_semfiles);
	semfile_list_free(&clear_attempts_semfiles);

	listFree(&current_logins);
	listFree(&current_connections);

	protected_uint32_destroy(node_threads_running);
	protected_uint32_destroy(ssh_sessions);

	thread_down();
	if (terminate_server || code)
		lprintf(LOG_INFO, "Terminal Server thread terminated (%u clients served)", served);
	set_state(SERVER_STOPPED);
	mqtt_shutdown(&mqtt);
	if (startup->terminated != NULL)
		startup->terminated(startup->cbdata, code);
}

void bbs_thread(void* arg)
{
	char host_name[256];
	char*           identity;
	char*           p;
	char str[MAX_PATH + 1];
	char logstr[256];
	union xp_sockaddr server_addr = {{0}};
	union xp_sockaddr client_addr;
	socklen_t client_addr_len;
	SOCKET client_socket = INVALID_SOCKET;
	int node_num;
	int result;
	time_t t;
	time_t start;
	time_t initialized = 0;
	node_t node;
	sbbs_t*         events = NULL;
	client_t client;
	startup = (bbs_startup_t*)arg;
	BOOL is_client = false;
#ifdef __unix__
	struct main_sock_cb_data uspy_cb[MAX_NODES] = {};
	union xp_sockaddr uspy_addr;
#endif
#ifdef USE_CRYPTLIB
	CRYPT_CONTEXT ssh_context;
#endif
	struct main_sock_cb_data telnet_cb;
	struct main_sock_cb_data ssh_cb;
	struct main_sock_cb_data rlogin_cb;
	void                        *ts_cb;

	if (startup == NULL) {
		sbbs_beep(100, 500);
		fprintf(stderr, "No startup structure passed!\n");
		return;
	}

	if (startup->size != sizeof(bbs_startup_t)) {  // verify size
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
	SAFEPRINTF3(js_server_props.version, "%s %s%c", TELNET_SERVER, VERSION, REVISION);
	js_server_props.version_detail = bbs_ver();
	js_server_props.clients = &node_threads_running;
	js_server_props.options = &startup->options;
	js_server_props.interfaces = &startup->telnet_interfaces;

	uptime = 0;
	served = 0;

	startup->recycle_now = false;
	startup->shutdown_now = false;
	terminate_server = false;

	SetThreadName("sbbs/termServer");

	do {
		/* Setup intelligent defaults */
		if (startup->telnet_port == 0)
			startup->telnet_port = IPPORT_TELNET;
		if (startup->rlogin_port == 0)
			startup->rlogin_port = 513;
#ifdef USE_CRYPTLIB
		if (startup->ssh_port == 0)
			startup->ssh_port = 22;
#endif
		if (startup->outbuf_drain_timeout == 0)
			startup->outbuf_drain_timeout = 10;
		if (startup->sem_chk_freq == 0)
			startup->sem_chk_freq = DEFAULT_SEM_CHK_FREQ;
		if (startup->temp_dir[0])
			backslash(startup->temp_dir);

		protected_uint32_init(&node_threads_running, 0);
		protected_uint32_init(&ssh_sessions, 0);

		thread_up(false /* setuid */);
		if (startup->seteuid != NULL)
			startup->seteuid(true);

		memset(text, 0, sizeof(text));
		memset(&scfg, 0, sizeof(scfg));

		lastuseron[0] = 0;

		char compiler[32];
		DESCRIBE_COMPILER(compiler);

		lprintf(LOG_INFO, "%s Version %s%c%s"
		        , TELNET_SERVER
		        , VERSION
		        , REVISION
#ifdef _DEBUG
		        , " Debug"
#else
		        , ""
#endif
		        );
		lprintf(LOG_INFO, "Compiled %s/%s %s with %s", git_branch, git_hash, git_date, compiler);

#ifdef _DEBUG
		lprintf(LOG_DEBUG, "sizeof: int=%d, long=%d, off_t=%d, time_t=%d"
		        , (int)sizeof(int), (int)sizeof(long), (int)sizeof(off_t), (int)sizeof(time_t));

		memset(str, 0xff, sizeof(str));
		snprintf(str, 2, "\x01\x02");
		if (str[1] != '\0') {
			lprintf(LOG_CRIT, "Non-terminating (unsafe) snprintf function detected (0x%02X instead of 0x00)", (uchar)str[1]);
			cleanup(1);
			return;
		}
#endif

		if (startup->first_node < 1 || startup->first_node > startup->last_node) {
			lprintf(LOG_CRIT, "!ILLEGAL node configuration (first: %d, last: %d)"
			        , startup->first_node, startup->last_node);
			cleanup(1);
			return;
		}

#ifdef __BORLANDC__
	#pragma warn -8008  /* Disable "Condition always false" warning */
	#pragma warn -8066  /* Disable "Unreachable code" warning */
#endif
		if (sizeof(node_t) != SIZEOF_NODE_T) {
			lprintf(LOG_CRIT, "!COMPILER ERROR: sizeof(node_t)=%d instead of %d"
			        , (int)sizeof(node_t), SIZEOF_NODE_T);
			cleanup(1);
			return;
		}

		if (!winsock_startup()) {
			cleanup(1);
			return;
		}

		t = time(NULL);
		lprintf(LOG_INFO, "Initializing on %.24s with options: %x"
		        , ctime_r(&t, str), startup->options);

		if (chdir(startup->ctrl_dir) != 0)
			lprintf(LOG_ERR, "!ERROR %d (%s) changing directory to: %s", errno, strerror(errno), startup->ctrl_dir);

		/* Initial configuration and load from CNF files */
		SAFECOPY(scfg.ctrl_dir, startup->ctrl_dir);
		lprintf(LOG_INFO, "Loading configuration files from %s", scfg.ctrl_dir);
		scfg.size = sizeof(scfg);
		scfg.node_num = startup->first_node;
		SAFECOPY(logstr, UNKNOWN_LOAD_ERROR);
		if (!load_cfg(&scfg, text, TOTAL_TEXT, /* prep: */ true, /* node_req: */ true, logstr, sizeof(logstr))) {
			lprintf(LOG_CRIT, "!ERROR loading configuration files: %s", logstr);
			cleanup(1);
			return;
		}
		if (logstr[0] != '\0')
			lprintf(LOG_WARNING, "!WARNING loading configuration files: %s", logstr);

		mqtt_startup(&mqtt, &scfg, (struct startup*)startup, bbs_ver(), lputs);

		if (scfg.total_shells < 1) {
			lprintf(LOG_CRIT, "At least one command shell must be configured (e.g. in SCFG->Command Shells)");
			cleanup(1);
			return;
		}

		if ((t = checktime()) != 0) { /* Check binary time */
			lprintf(LOG_ERR, "!TIME PROBLEM (%" PRId64 ")", (int64_t)t);
		}

		if (smb_tzutc(sys_timezone(&scfg)) != xpTimeZone_local()) {
			lprintf(LOG_WARNING, "Configured time zone (%s, 0x%04hX, offset: %d) does not match system-local time zone offset: %d"
			        , smb_zonestr(scfg.sys_timezone, str), scfg.sys_timezone, smb_tzutc(scfg.sys_timezone), xpTimeZone_local());
		}
		if (startup->max_login_inactivity > 0 && startup->max_login_inactivity <= scfg.max_getkey_inactivity) {
			lprintf(LOG_WARNING, "Configured max_login_inactivity (%u) is less than or equal to max_getkey_inactivity (%u)"
			        , startup->max_login_inactivity, scfg.max_getkey_inactivity);
		}
		if (startup->max_newuser_inactivity > 0 && startup->max_newuser_inactivity <= scfg.max_getkey_inactivity) {
			lprintf(LOG_WARNING, "Configured max_newuser_inactivity (%u) is less than or equal to max_getkey_inactivity (%u)"
			        , startup->max_newuser_inactivity, scfg.max_getkey_inactivity);
		}
		if (startup->max_session_inactivity > 0 && startup->max_session_inactivity <= scfg.max_getkey_inactivity) {
			lprintf(LOG_WARNING, "Configured max_session_inactivity (%u) is less than or equal to max_getkey_inactivity (%u)"
			        , startup->max_session_inactivity, scfg.max_getkey_inactivity);
		}
		if (uptime == 0)
			uptime = time(NULL); /* this must be done *after* setting the timezone */

		if (startup->last_node > scfg.sys_nodes) {
			lprintf(LOG_NOTICE, "Specified last_node (%d) > sys_nodes (%d), auto-corrected"
			        , startup->last_node, scfg.sys_nodes);
			startup->last_node = scfg.sys_nodes;
		}

		SAFEPRINTF(str, "%u total", scfg.sys_nodes);
		mqtt_pub_strval(&mqtt, TOPIC_BBS, "node", str);

		/* Create missing directories */
		lprintf(LOG_INFO, "Verifying/creating data directories");
		make_data_dirs(&scfg);

		/* Create missing node directories */
		lprintf(LOG_INFO, "Verifying/creating node directories");
		for (int i = 0; i <= scfg.sys_nodes; i++) {
			char newfn[MAX_PATH + 1];
			char oldfn[MAX_PATH + 1];
			if (i) {
				int err;
				if ((err = md(scfg.node_path[i - 1])) != 0) {
					lprintf(LOG_CRIT, "!ERROR %d (%s) creating directory: %s", err, strerror(err), scfg.node_path[i - 1]);
					cleanup(1);
					return;
				}
			}
			// Convert old dsts.dab -> dsts.ini
			SAFEPRINTF(oldfn, "%sdsts.dab", i ? scfg.node_path[i - 1] : scfg.ctrl_dir);
			if (fexist(oldfn) && !fexist(dstats_fname(&scfg, i, newfn, sizeof(newfn)))) {
				lprintf(LOG_NOTICE, "Auto-upgrading daily statistics data file: %s", oldfn);
				stats_t stats;
				getstats(&scfg, i, &stats);
				putstats(&scfg, i, &stats);
			}
			SAFEPRINTF(oldfn, "%scsts.dab", i ? scfg.node_path[i - 1] : scfg.ctrl_dir);
			if (fexist(oldfn) && !fexist(cstats_fname(&scfg, i, newfn, sizeof(newfn)))) {
				uint record = 0;
				lprintf(LOG_NOTICE, "Auto-upgrading cumulative statistics log file: %s", oldfn);
				stats_t stats;
				FILE* out = fopen_cstats(&scfg, i, /* write: */ true);
				if (out == NULL) {
					lprintf(LOG_ERR, "!ERROR %d (%s) creating: %s", errno, strerror(errno), newfn);
					continue;
				}
				FILE* in = fopen(oldfn, "rb");
				if (in == NULL)
					lprintf(LOG_ERR, "!ERROR %d (%s) opening: %s", errno, strerror(errno), oldfn);
				else {
					ZERO_VAR(stats);
					while (!feof(in)) {
						struct {                                /* System/Node Statistics */
							uint32_t date,                      /* When last rolled-over */
							         logons,                    /* Total Logons on System */
							         timeon,                    /* Total Time on System */
							         uls,                       /* Total Uploads Today */
							         ulb,                       /* Total Upload Bytes Today */
							         dls,                       /* Total Downloads Today */
							         dlb,                       /* Total Download Bytes Today */
							         ptoday,                    /* Total Posts Today */
							         etoday,                    /* Total Emails Today */
							         ftoday;                    /* Total Feedbacks Today */
						} legacy_stats;
						if (fread(&legacy_stats, sizeof(legacy_stats), 1, in) != 1)
							break;
						record++;
						if (legacy_stats.logons > 1000
						    || legacy_stats.timeon > 10000) {
							lprintf(LOG_WARNING, "Skipped corrupted record #%u in %s", record, oldfn);
							continue;
						}
						ZERO_VAR(stats);
						stats.date         = legacy_stats.date;
						stats.today.logons = legacy_stats.logons;
						stats.today.timeon = legacy_stats.timeon;
						stats.today.uls    = legacy_stats.uls;
						stats.today.ulb    = legacy_stats.ulb;
						stats.today.dls    = legacy_stats.dls;
						stats.today.dlb    = legacy_stats.dlb;
						stats.today.posts  = legacy_stats.ptoday;
						stats.today.email  = legacy_stats.etoday;
						stats.today.fbacks = legacy_stats.ftoday;
						if (!fwrite_cstats(out, &stats)) {
							lprintf(LOG_ERR, "!WRITE ERROR, %s line %d", __FILE__, __LINE__);
							break;
						}
					}
					fclose(in);
				}
				fclose_cstats(out);
				lprintf(LOG_INFO, "Done (%u daily-statistics records converted)", record);
			}
		}

		/* Initial global node variables */
		for (int i = 0; i < MAX_NODES; i++) {
			node_inbuf[i] = NULL;
			node_socket[i] = INVALID_SOCKET;
			spy_socket[i] = INVALID_SOCKET;
#ifdef __unix__
			uspy_socket[i] = INVALID_SOCKET;
#endif
		}

		startup->node_inbuf = node_inbuf;

		/* open a socket and wait for a client */
		ts_set = xpms_create(startup->bind_retry_count, startup->bind_retry_delay, lprintf);
		if (ts_set == NULL) {
			lprintf(LOG_CRIT, "!ERROR %d creating Terminal Server socket set", SOCKET_ERRNO);
			cleanup(1);
			return;
		}
		if (!(startup->options & BBS_OPT_NO_TELNET)) {
			telnet_cb.protocol = "telnet";
			telnet_cb.startup = startup;

			/*
			 * Add interfaces
			 */
			xpms_add_list(ts_set, PF_UNSPEC, SOCK_STREAM, 0, startup->telnet_interfaces, startup->telnet_port, "Telnet Server", &terminate_server, sock_cb, startup->seteuid, &telnet_cb);
		}

		if (startup->options & BBS_OPT_ALLOW_RLOGIN) {
			/* open a socket and wait for a client */
			rlogin_cb.protocol = "rlogin";
			rlogin_cb.startup = startup;
			xpms_add_list(ts_set, PF_UNSPEC, SOCK_STREAM, 0, startup->rlogin_interfaces, startup->rlogin_port, "RLogin Server", &terminate_server, sock_cb, startup->seteuid, &rlogin_cb);
		}

#ifdef USE_CRYPTLIB
#if CRYPTLIB_VERSION < 3300 && CRYPTLIB_VERSION > 999
	#warning This version of Cryptlib is known to crash Synchronet.  Upgrade to at least version 3.3 or do not build with Cryptlib support.
#endif
		if (startup->options & BBS_OPT_ALLOW_SSH) {
			CRYPT_KEYSET ssh_keyset;

			if (!do_cryptInit(lprintf)) {
				lprintf(LOG_ERR, "SSH cryptInit failure");
				goto NO_SSH;
			}
			int i;
			/* Get the private key... first try loading it from a file... */
			SAFEPRINTF2(str, "%s%s", scfg.ctrl_dir, "cryptlib.key");
			if (cryptStatusOK(cryptKeysetOpen(&ssh_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, str, CRYPT_KEYOPT_READONLY))) {
				if (cryptStatusError(i = cryptGetPrivateKey(ssh_keyset, &ssh_context, CRYPT_KEYID_NAME, "ssh_server", scfg.sys_pass))) {
					GCESNN(i, ssh_keyset, "getting private key");
					goto NO_SSH;
				}
			}
			else {
				/* Couldn't do that... create a new context and use the key from there... */

				if (cryptStatusError(i = cryptCreateContext(&ssh_context, CRYPT_UNUSED, CRYPT_ALGO_RSA))) {
					GCESNN(i, CRYPT_UNUSED, "creating context");
					goto NO_SSH;
				}
				if (cryptStatusError(i = cryptSetAttributeString(ssh_context, CRYPT_CTXINFO_LABEL, "ssh_server", 10))) {
					GCESNN(i, ssh_context, "setting label");
					goto NO_SSH;
				}
				if (cryptStatusError(i = cryptGenerateKey(ssh_context))) {
					GCESNN(i, ssh_context, "generating key");
					goto NO_SSH;
				}

				/* Ok, now try saving this one... use the syspass to encrypt it. */
				if (cryptStatusOK(i = cryptKeysetOpen(&ssh_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, str, CRYPT_KEYOPT_CREATE))) {
					if (cryptStatusError(i = cryptAddPrivateKey(ssh_keyset, ssh_context, scfg.sys_pass)))
						GCESNN(i, ssh_keyset, "adding private key");
					if (cryptStatusError(i = cryptKeysetClose(ssh_keyset)))
						GCESNN(i, ssh_keyset, "closing keyset");
				}
				else
					GCESNN(i, CRYPT_UNUSED, "creating keyset");
			}

			/* open a socket and wait for a client */
			ssh_cb.protocol = "ssh";
			ssh_cb.startup = startup;
			xpms_add_list(ts_set, PF_UNSPEC, SOCK_STREAM, 0, startup->ssh_interfaces, startup->ssh_port, "SSH Server", &terminate_server, sock_cb, startup->seteuid, &ssh_cb);
		}
NO_SSH:
#endif

		sbbs_t* sbbs = new sbbs_t(0, &server_addr, sizeof(server_addr)
		                          , "Terminal Server", ts_set->socks[0].sock, &scfg, text, NULL);
		if (sbbs->init() == false) {
			lputs(LOG_CRIT, "!BBS initialization failed");
			cleanup(1);
			return;
		}
		sbbs->outcom_max_attempts = 10;
		sbbs->output_thread_running = true;
		_beginthread(output_thread, 0, sbbs);

		if (!(startup->options & BBS_OPT_NO_EVENTS)) {
			events = new sbbs_t(0, &server_addr, sizeof(server_addr)
			                    , "BBS Events", INVALID_SOCKET, &scfg, text, NULL, true);
			if (events->init() == false) {
				lputs(LOG_CRIT, "!Events initialization failed");
				cleanup(1);
				return;
			}
			_beginthread(event_thread, 0, events);
		}

		/* Save these values in case they're changed dynamically */
		first_node = startup->first_node;
		last_node = startup->last_node;

		for (int i = first_node; i <= last_node; i++) {
			if (sbbs->getnodedat(i, &node, true)) {
				node.status = NODE_WFC;
				node.misc &= NODE_EVENT; /* Note: Turns-off NODE_RRUN flag (and others) */
				node.action = 0;
				sbbs->putnodedat(i, &node);
			}
		}

		/* Setup recycle/shutdown semaphore file lists */
		shutdown_semfiles = semfile_list_init(scfg.ctrl_dir, "shutdown", server_abbrev);
		recycle_semfiles = semfile_list_init(scfg.ctrl_dir, "recycle", server_abbrev);
		pause_semfiles = semfile_list_init(scfg.ctrl_dir, "pause", server_abbrev);
		clear_attempts_semfiles = semfile_list_init(scfg.ctrl_dir, "clear", server_abbrev);
		semfile_list_add(&recycle_semfiles, startup->ini_fname);
		strListAppendFormat(&recycle_semfiles, "%stext.dat", scfg.ctrl_dir);
		strListAppendFormat(&recycle_semfiles, "%stext.ini", scfg.ctrl_dir);
		strListAppendFormat(&recycle_semfiles, "%sattr.cfg", scfg.ctrl_dir);
		if (!initialized)
			semfile_list_check(&initialized, shutdown_semfiles);
		semfile_list_check(&initialized, recycle_semfiles);
		semfile_list_check(&initialized, clear_attempts_semfiles);

		listInit(&current_logins, LINK_LIST_MUTEX);
		listInit(&current_connections, LINK_LIST_MUTEX);

#ifdef __unix__ //	unix-domain spy sockets
		for (int i = first_node; i <= last_node && !(startup->options & BBS_OPT_NO_SPY_SOCKETS); i++)  {
			if ((unsigned int)snprintf(str, sizeof(uspy_addr.un.sun_path),
			                           "%slocalspy%d.sock", startup->temp_dir, i)
			    >= sizeof(uspy_addr.un.sun_path)) {
				lprintf(LOG_ERR, "Node %d !ERROR local spy socket path \"%slocalspy%d.sock\" too long."
				        , i, startup->temp_dir, i);
				continue;
			}
			else  {
				if (xpms_add(ts_set, PF_UNIX, SOCK_STREAM, 0, str, 0, "Spy Socket", NULL, sock_cb, NULL, &uspy_cb[i - 1]))
					lprintf(LOG_INFO, "Node %d local spy using socket %s", i, str);
				else
					lprintf(LOG_ERR, "Node %d !ERROR %d (%s) creating local spy socket %s"
					        , i, errno, strerror(errno), str);
			}
		}
#endif // __unix__ (unix-domain spy sockets)

		lprintf(LOG_INFO, "Terminal Server thread started for nodes %d through %d", first_node, last_node);
		mqtt_client_max(&mqtt, (last_node - first_node) + 1);

		while (!terminate_server) {
			YIELD();
			/* check for re-run flags and recycle/shutdown sem files */
			if (!(startup->options & BBS_OPT_NO_RECYCLE)
			    && protected_uint32_value(node_threads_running) == 0) {
				if ((p = semfile_list_check(&initialized, recycle_semfiles)) != NULL) {
					lprintf(LOG_INFO, "Recycle semaphore file (%s) detected"
					        , p);
					break;
				}
				if (startup->recycle_now == true) {
					lprintf(LOG_INFO, "Recycle semaphore signaled");
					startup->recycle_now = false;
					break;
				}
			}
			if (((p = semfile_list_check(&initialized, shutdown_semfiles)) != NULL
			     && lprintf(LOG_INFO, "Shutdown semaphore file (%s) detected"
			                , p))
			    || (startup->shutdown_now == true
			        && lprintf(LOG_INFO, "Shutdown semaphore signaled"))) {
				startup->shutdown_now = false;
				terminate_server = true;
				break;
			}
			if (((p = semfile_list_check(NULL, pause_semfiles)) != NULL
			     && lprintf(LOG_INFO, "Pause semaphore file (%s) detected"
			                , p))
			    || (startup->paused
			        && lprintf(LOG_INFO, "Pause semaphore signaled"))) {
				set_state(SERVER_PAUSED);
				SLEEP(startup->sem_chk_freq * 1000);
				continue;
			}
			/* signal caller that we've started up successfully */
			set_state(SERVER_READY);

			sbbs->online = false;
#ifdef USE_CRYPTLIB
			sbbs->ssh_mode = false;
#endif

			if (events != nullptr && events->event_thread_running) {
				time_t now = time(NULL);
				t = event_thread_tick;
				if (now > t && now - t > 60 * 60 * 2) {
					if (!event_thread_blocked) {
						if (*events->event_code != '\0')
							snprintf(str, sizeof str, " running %s", events->event_code);
						else
							*str = '\0';
						lprintf(LOG_ERR, "Event thread appears to be blocked%s since %s", str, events->timestr(t));
						event_thread_blocked = true;
					}
				} else {
					if (event_thread_blocked) {
						lprintf(LOG_NOTICE, "Event thread unblocked");
						event_thread_blocked = false;
					}
				}
			}

			/* now wait for connection */
			client_addr_len = sizeof(client_addr);
			client_socket = xpms_accept(ts_set, &client_addr
			                            , &client_addr_len, startup->sem_chk_freq * 1000, (startup->options & BBS_OPT_HAPROXY_PROTO) ? XPMS_ACCEPT_FLAG_HAPROXY : XPMS_FLAGS_NONE, &ts_cb);

			if (terminate_server) /* terminated */
				break;

			if ((p = semfile_list_check(&initialized, clear_attempts_semfiles)) != NULL) {
				lprintf(LOG_INFO, "Clear Failed Login Attempts semaphore file (%s) detected", p);
				loginAttemptListClear(startup->login_attempt_list);
			}

			if (client_socket == INVALID_SOCKET)
				continue;

			bool rlogin = false;
#ifdef USE_CRYPTLIB
			bool ssh = false;
#endif

			is_client = false;
			if (ts_cb == &telnet_cb) {
				is_client = true;
			} else if (ts_cb == &rlogin_cb) {
				rlogin = true;
				is_client = true;
#ifdef USE_CRYPTLIB
			} else if (ts_cb == &ssh_cb) {
				ssh = true;
				is_client = true;
#endif
			} else {
#ifdef __unix__
				for (int i = first_node; i <= last_node; i++)  {
					if (&uspy_cb[i - 1] == ts_cb) {
						if (node_socket[i - 1] == INVALID_SOCKET)
							if (read(uspy_socket[i - 1], str, sizeof(str)) < 1)
								*str = '\0';
						if (!socket_check(uspy_socket[i - 1], NULL, NULL, 0)) {
							lprintf(LOG_NOTICE, "Spy socket for node %d disconnected", i);
							close_socket(uspy_socket[i - 1]);
							uspy_socket[i - 1] = INVALID_SOCKET;
						}
					}
					if (&uspy_cb[i - 1] == ts_cb) {
						BOOL already_connected = (uspy_socket[i - 1] != INVALID_SOCKET);
						SOCKET new_socket = client_socket;
						fcntl(new_socket, F_SETFL, fcntl(new_socket, F_GETFL) | O_NONBLOCK);
						if (already_connected)  {
							lprintf(LOG_ERR, "!ERROR Spy socket %s already in use", uspy_addr.un.sun_path);
							send(new_socket, "Spy socket already in use.\r\n", 27, 0);
							close_socket(new_socket);
						}
						else  {
							lprintf(LOG_INFO, "Spy socket %s (%d) connected", uspy_addr.un.sun_path, new_socket);
							uspy_socket[i - 1] = new_socket;
							SAFEPRINTF(str, "Spy connection established to node %d\r\n", i);
							send(uspy_socket[i - 1], str, strlen(str), 0);
						}
					}
				}
#else
				lprintf(LOG_ERR, "!NO SOCKETS set by select");
#endif
			}

			if (!is_client) {
				/* Do not need to close_socket(client_socket) here */
				continue;
			}

			// Count the socket:
			call_socket_open_callback(true);

			char host_ip[INET6_ADDRSTRLEN];

			inet_addrtop(&client_addr, host_ip, sizeof(host_ip));

			if (trashcan(&scfg, host_ip, "ip-silent")) {
				close_socket(client_socket);
				continue;
			}


#ifdef USE_CRYPTLIB
			SAFECOPY(client.protocol, rlogin ? "RLogin":(ssh ? "SSH" : "Telnet"));
#else
			SAFECOPY(client.protocol, rlogin ? "RLogin":"Telnet");
#endif
			union xp_sockaddr local_addr;
			memset(&local_addr, 0, sizeof(local_addr));
			socklen_t addr_len = sizeof(local_addr);
			if (getsockname(client_socket, (struct sockaddr *)&local_addr, &addr_len) != 0) {
				errprintf(LOG_CRIT, WHERE, "%04d %s [%s] !ERROR %d getting local address/port of socket"
				          , client_socket, client.protocol, host_ip, SOCKET_ERRNO);
				close_socket(client_socket);
				continue;
			}
			char local_ip[INET6_ADDRSTRLEN];
			inet_addrtop(&local_addr, local_ip, sizeof local_ip);

			lprintf(LOG_INFO, "%04d %s [%s] Connection accepted on %s port %u from port %u"
			        , client_socket, client.protocol, host_ip, local_ip, inet_addrport(&local_addr), inet_addrport(&client_addr));

			if (startup->max_concurrent_connections > 0) {
				int ip_len = strlen(host_ip) + 1;
				int connections = listCountMatches(&current_connections, host_ip, ip_len);
				int logins = listCountMatches(&current_logins, host_ip, ip_len);

				if (connections - logins >= (int)startup->max_concurrent_connections
				    && !is_host_exempt(&scfg, host_ip, /* host_name */ NULL)) {
					lprintf(LOG_NOTICE, "%04d %s !Maximum concurrent connections without login (%u) reached from host: %s"
					        , client_socket, client.protocol, startup->max_concurrent_connections, host_ip);
					close_socket(client_socket);
					continue;
				}
			}

			// Initialize term state
			sbbs->autoterm = 0;
			if (sbbs->term)
				delete sbbs->term;
			sbbs->term = new Terminal(sbbs);
			sbbs->term->cols = startup->default_term_width;

			sbbs->client_socket = client_socket; // required for output to the user
			if (!ssh)
				sbbs->online = ON_REMOTE;

			login_attempt_t attempted;
			uint banned = loginBanned(&scfg, startup->login_attempt_list, client_socket, /* host_name: */ NULL, startup->login_attempt, &attempted);
			if (banned) {
				char ban_duration[128];
				lprintf(LOG_NOTICE, "%04d %s [%s] !TEMPORARY BAN (%lu login attempts%s%s) - remaining: %s"
				        , client_socket, client.protocol, host_ip, attempted.count - attempted.dupes
				        , attempted.user[0] ? ", last: " : "", attempted.user, duration_estimate_to_vstr(banned, ban_duration, sizeof ban_duration, 1, 1));
				close_socket(client_socket);
				continue;
			}
			if (inet_addrport(&local_addr) == startup->pet40_port || inet_addrport(&local_addr) == startup->pet80_port) {
				sbbs->autoterm = PETSCII;
				sbbs->term->cols = inet_addrport(&local_addr) == startup->pet40_port ? 40 : 80;
				sbbs->term_out(PETSCII_UPPERLOWER);
			}
			update_terminal(sbbs);
			// TODO: Plain text output in SSH socket
			struct trash trash;
			if (sbbs->trashcan(host_ip, "ip", &trash)) {
				if (!trash.quiet) {
					char details[128];
					lprintf(LOG_NOTICE, "%04d %s [%s] !CLIENT BLOCKED in ip.can %s", client_socket, client.protocol, host_ip, trash_details(&trash, details, sizeof details));
				}
				close_socket(client_socket);
				continue;
			}

#ifdef _WIN32
			if (startup->sound.answer[0] && !sound_muted(&scfg))
				PlaySound(startup->sound.answer, NULL, SND_ASYNC | SND_FILENAME);
#endif

			/* Purge (flush) any pending input or output data */
			sbbs->rioctl(IOFB);

			/* Do SSH stuff here */
#ifdef USE_CRYPTLIB
			if (ssh) {
				BOOL nodelay = true;
				ulong nb = 0;
				int i;

				if (cryptStatusError(i = cryptCreateSession(&sbbs->ssh_session, CRYPT_UNUSED, CRYPT_SESSION_SSH_SERVER))) {
					GCESS(i, client_socket, CRYPT_UNUSED, "creating SSH session");
					close_socket(client_socket);
					continue;
				}
				lprintf(LOG_DEBUG, "%04d SSH Cryptlib Session: %d created", client_socket, sbbs->ssh_session);
				protected_uint32_adjust(&ssh_sessions, 1);
				sbbs->ssh_mode = true;

				if (cryptStatusError(i = cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_CONNECTTIMEOUT, startup->ssh_connect_timeout)))
					sbbs->log_crypt_error_status_sock(i, "setting connect timeout");

				if (startup->options & BBS_OPT_SSH_ANYAUTH) {
					if (cryptStatusError(i = cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_OPTIONS, CRYPT_SSHOPTION_NONE_AUTH)))
						sbbs->log_crypt_error_status_sock(i, "setting none auth");
				}

				if (cryptStatusError(i = cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_PRIVATEKEY, ssh_context))) {
					sbbs->log_crypt_error_status_sock(i, "setting private key");
					SSH_END(client_socket);
					close_socket(client_socket);
					continue;
				}
				setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
				ioctlsocket(client_socket, FIONBIO, &nb);
				if (cryptStatusError(i = cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_NETWORKSOCKET, client_socket))) {
					sbbs->log_crypt_error_status_sock(i, "setting network socket");
					SSH_END(client_socket);
					close_socket(client_socket);
					continue;
				}
				if (cryptStatusError(i = cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_READTIMEOUT, 0)))
					sbbs->log_crypt_error_status_sock(i, "setting read timeout");
				// READ = WRITE TIMEOUT HACK... REMOVE WHEN FIXED
				if (cryptStatusError(i = cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_WRITETIMEOUT, 0)))
					sbbs->log_crypt_error_status_sock(i, "setting write timeout");
#if 0
				if (cryptStatusError(err = crypt_pop_channel_data(sbbs, str, sizeof(str), &i))) {
					GCES(i, sbbs->cfg.node_num, sbbs->ssh_session, "popping data");
					i = 0;
				}
#endif
			}
#endif

			if (rlogin)
				sbbs->outcom(0); /* acknowledge RLogin per RFC 1282 */

			sbbs->bprintf("\r\n%s\r\n", VERSION_NOTICE);
			sbbs->bprintf("%s connection from: %s\r\n", client.protocol, host_ip);

			SAFECOPY(host_name, STR_NO_HOSTNAME);
			if (!(startup->options & BBS_OPT_NO_HOST_LOOKUP)) {
				sbbs->bprintf("Resolving hostname...");
				getnameinfo(&client_addr.addr, client_addr_len, host_name, sizeof(host_name), NULL, 0, NI_NAMEREQD);
				sbbs->cp437_out(crlf);
				lprintf(LOG_INFO, "%04d %s [%s] Hostname: %s", client_socket, client.protocol, host_ip, host_name);
			}

			if (sbbs->trashcan(host_name, "host", &trash)) {
				if (!trash.quiet) {
					char details[128];
					lprintf(LOG_NOTICE, "%04d %s [%s] !CLIENT BLOCKED in host.can: %s %s"
							, client_socket, client.protocol, host_ip, host_name, trash_details(&trash, details, sizeof details));
				}
				SSH_END(client_socket);
				close_socket(client_socket);
				continue;
			}

			identity = NULL;
			if (startup->options & BBS_OPT_GET_IDENT) {
				sbbs->bprintf("Resolving identity...");
				/* ToDo: Make ident timeout configurable */
				if (identify(&client_addr, inet_addrport(&client_addr), str, sizeof(str) - 1, /* timeout: */ 1)) {
					lprintf(LOG_DEBUG, "%04d %s [%s] Ident Response: %s", client_socket, client.protocol, host_ip, str);
					identity = strrchr(str, ':');
					if (identity != NULL) {
						identity++; /* skip colon */
						SKIP_WHITESPACE(identity);
						if (*identity)
							lprintf(LOG_INFO, "%04d %s [%s] Identity: %s", client_socket, client.protocol, host_ip, identity);
					}
				}
				sbbs->cp437_out(crlf);
			}
			/* Initialize client display */
			client.size = sizeof(client);
			client.time = time32(NULL);
			SAFECOPY(client.addr, host_ip);
			SAFECOPY(client.host, host_name);
			client.port = inet_addrport(&client_addr);
			SAFECOPY(client.user, STR_UNKNOWN_USER);
			client.usernum = 0;
			client_on(client_socket, &client, false /* update */);

			for (node_num = first_node; node_num <= last_node; node_num++) {
				if (node_socket[node_num - 1] != INVALID_SOCKET)
					continue;
				/* paranoia: make sure node.status!=NODE_WFC by default */
				node.status = NODE_INVALID_STATUS;
				if (!sbbs->getnodedat(node_num, &node, true))
					continue;
				switch (node.status) {
					case NODE_LOGON:
					case NODE_NEWUSER:
					case NODE_INUSE:
					case NODE_QUIET:
						if (node_socket[node_num - 1] == INVALID_SOCKET) {
							lprintf(LOG_CRIT, "%04d !Node %d status is %d, but the node socket is invalid, changing to WFC"
							        , client_socket, node_num, node.status);
							node.status = NODE_WFC;
						}
						break;
				}
				if (node.status == NODE_WFC) {
					if (node_socket[node_num - 1] != INVALID_SOCKET) {
						lprintf(LOG_CRIT, "%04d !Node %d status is WFC, but the node socket (%d) and thread are still in use!"
						        , client_socket, node_num, node_socket[node_num - 1]);
						sbbs->unlocknodedat(node_num);
						continue;
					}
					node.status = NODE_LOGON;
#ifdef USE_CRYPTLIB
					if (ssh)
						node.connection = NODE_CONNECTION_SSH;
					else
#endif
					if (rlogin)
						node.connection = NODE_CONNECTION_RLOGIN;
					else
						node.connection = NODE_CONNECTION_TELNET;

					node_socket[node_num - 1] = client_socket;
					sbbs->putnodedat(node_num, &node);
					break;
				}
				sbbs->unlocknodedat(node_num);
			}

			if (node_num > last_node) {
				lprintf(LOG_WARNING, "%04d %s [%s] !No nodes available for login.", client_socket, client.protocol, host_ip);
				SAFEPRINTF(str, "%snonodes.txt", scfg.text_dir);
				if (fexist(str))
					sbbs->printfile(str, P_NOABORT | P_MODS);
				else {
					sbbs->cp437_out("\r\nSorry, all terminal nodes are in use or otherwise unavailable.\r\n");
					sbbs->cp437_out("Please try again later.\r\n");
				}
				sbbs->flush_output(3000);
				client_off(client_socket);
				SSH_END(client_socket);
				close_socket(client_socket);
				continue;
			}

			lprintf(LOG_INFO, "%04d %s [%s] Attaching to Node %d", client_socket, client.protocol, host_ip, node_num);

			// Load the configuration files for this node, only if/when needed/updated
			scfg_t* cfg = &node_scfg[node_num - 1];
			if (cfg->size != sizeof(*cfg) || (node.misc & NODE_RRUN)) {
				sbbs->bprintf("Loading configuration...");
				cfg->size = sizeof(*cfg);
				cfg->node_num = node_num;
				SAFECOPY(cfg->ctrl_dir, startup->ctrl_dir);
				lprintf(LOG_INFO, "Node %d Loading configuration files from %s", cfg->node_num, cfg->ctrl_dir);
				SAFECOPY(logstr, UNKNOWN_LOAD_ERROR);
				if (!load_cfg(cfg, node_text[node_num - 1], TOTAL_TEXT, /* prep: */ true, /* node_req: */ true, logstr, sizeof(logstr))) {
					lprintf(LOG_WARNING, "Node %d LOAD ERROR: %s, falling back to Node %d", cfg->node_num, logstr, first_node);
					cfg->node_num = first_node;
					if (!load_cfg(cfg, node_text[node_num - 1], TOTAL_TEXT, /* prep: */ true, /* node: */ true, logstr, sizeof(logstr))) {
						lprintf(LOG_CRIT, "!ERROR loading configuration files: %s", logstr);
						sbbs->bprintf("\r\nFAILED: %s", logstr);
						client_off(client_socket);
						SSH_END(client_socket);
						close_socket(client_socket);
						if (sbbs->getnodedat(cfg->node_num, &node, true)) {
							node.status = NODE_WFC;
							node_socket[node_num - 1] = INVALID_SOCKET;
							sbbs->putnodedat(cfg->node_num, &node);
						} else
							node_socket[node_num - 1] = INVALID_SOCKET;
						continue;
					}
				}
				cfg->node_num = node_num; // correct the node number
				if (node.misc & NODE_RRUN) {
					if (sbbs->getnodedat(cfg->node_num, &node, true)) {
						node.misc &= ~NODE_RRUN;
						sbbs->putnodedat(cfg->node_num, &node);
					}
				}
				sbbs->bputs(crlf);
			}
			// Copy event last-run info from global config
			for (int e = 0; e < cfg->total_events && e < scfg.total_events; e++)
				cfg->event[e]->last = scfg.event[e]->last;

			sbbs_t* new_node = new sbbs_t(node_num, &client_addr, client_addr_len, host_name
			                              , client_socket
			                              , cfg, node_text[node_num - 1], &client);

			new_node->client = client;
#ifdef USE_CRYPTLIB
			if (ssh) {
				new_node->ssh_session = sbbs->ssh_session; // This is done again later, after NO_PASSTHRU: Why?
				new_node->ssh_mode = true;
				new_node->session_channel = sbbs->session_channel;
			}
#endif

			/* copy the IDENT response, if any */
			if (identity != NULL)
				SAFECOPY(new_node->client_ident, identity);

			if (new_node->init() == false) {
				lprintf(LOG_INFO, "%04d %s Node %d !Initialization failure"
				        , client_socket, client.protocol, new_node->cfg.node_num);
				SAFEPRINTF(str, "%snonodes.txt", scfg.text_dir);
				if (fexist(str))
					sbbs->printfile(str, P_NOABORT | P_MODS);
				else
					sbbs->cp437_out("\r\nSorry, initialization failed. Try again later.\r\n");
				sbbs->flush_output(3000);
				if (sbbs->getnodedat(new_node->cfg.node_num, &node, true)) {
					node.status = NODE_WFC;
					sbbs->putnodedat(new_node->cfg.node_num, &node);
				}
				delete new_node;
				node_socket[node_num - 1] = INVALID_SOCKET;
				client_off(client_socket);
				SSH_END(client_socket);
				close_socket(client_socket);
				continue;
			}

			if (rlogin == true) {
				SAFECOPY(new_node->connection, "RLogin");
				new_node->node_connection = NODE_CONNECTION_RLOGIN;
				new_node->sys_status |= SS_RLOGIN;
				new_node->telnet_mode |= TELNET_MODE_OFF; // RLogin does not use Telnet commands
			}

			// Passthru socket creation/connection
			if (true) {
				/* TODO: IPv6? */
				SOCKET tmp_sock;
				SOCKADDR_IN tmp_addr = {0};
				socklen_t tmp_addr_len;

				/* open a socket and connect to yourself */

				tmp_sock = open_socket(PF_INET, SOCK_STREAM, "passthru");

				if (tmp_sock == INVALID_SOCKET) {
					lprintf(LOG_ERR, "Node %d !ERROR %d creating passthru listen socket"
					        , new_node->cfg.node_num, SOCKET_ERRNO);
					goto NO_PASSTHRU;
				}

				lprintf(LOG_DEBUG, "Node %d passthru listen socket %d opened"
				        , new_node->cfg.node_num, tmp_sock);

				/*****************************/
				/* Listen for incoming calls */
				/*****************************/
				memset(&tmp_addr, 0, sizeof(tmp_addr));

				tmp_addr.sin_addr.s_addr = htonl(IPv4_LOCALHOST);
				tmp_addr.sin_family = AF_INET;
				tmp_addr.sin_port   = 0;

				result = bind(tmp_sock, (struct sockaddr *)&tmp_addr, sizeof(tmp_addr));
				if (result != 0) {
					lprintf(LOG_NOTICE, "%s", BIND_FAILURE_HELP);
					close_socket(tmp_sock);
					goto NO_PASSTHRU;
				}

				result = listen(tmp_sock, 1);

				if (result != 0) {
					lprintf(LOG_ERR, "Node %d !ERROR %d (%d) listening on passthru socket"
					        , new_node->cfg.node_num, result, SOCKET_ERRNO);
					close_socket(tmp_sock);
					goto NO_PASSTHRU;
				}

				tmp_addr_len = sizeof(tmp_addr);
				if (getsockname(tmp_sock, (struct sockaddr *)&tmp_addr, &tmp_addr_len)) {
					lprintf(LOG_CRIT, "Node %d !ERROR %d getting passthru listener address/port of socket"
					        , new_node->cfg.node_num, SOCKET_ERRNO);
					close_socket(tmp_sock);
					goto NO_PASSTHRU;
				}
				lprintf(LOG_DEBUG, "Node %d passthru socket listening on port %u"
				        , new_node->cfg.node_num, htons(tmp_addr.sin_port));

				new_node->passthru_socket = open_socket(PF_INET, SOCK_STREAM, "passthru");

				if (new_node->passthru_socket == INVALID_SOCKET) {
					lprintf(LOG_ERR, "Node %d !ERROR %d creating passthru connecting socket"
					        , new_node->cfg.node_num, SOCKET_ERRNO);
					close_socket(tmp_sock);
					goto NO_PASSTHRU;
				}

				lprintf(LOG_DEBUG, "Node %d passthru connect socket %d opened"
				        , new_node->cfg.node_num, new_node->passthru_socket.load());

				result = connect(new_node->passthru_socket, (struct sockaddr *)&tmp_addr, tmp_addr_len);

				if (result != 0) {
					char tmp[16];
					lprintf(LOG_ERR, "Node %d !ERROR %d (%d) connecting to passthru socket: %s port %u"
					        , new_node->cfg.node_num, result, SOCKET_ERRNO
					        , inet_ntop(AF_INET, &tmp_addr.sin_addr.s_addr, tmp, sizeof tmp), htons(tmp_addr.sin_port));
					close_socket(new_node->passthru_socket);
					new_node->passthru_socket = INVALID_SOCKET;
					close_socket(tmp_sock);
					goto NO_PASSTHRU;
				}

				new_node->client_socket_dup = accept(tmp_sock, (struct sockaddr *)&tmp_addr, &tmp_addr_len);

				if (new_node->client_socket_dup == INVALID_SOCKET) {
					lprintf(LOG_ERR, "Node %d !ERROR (%d) accepting on passthru socket"
					        , new_node->cfg.node_num, SOCKET_ERRNO);
					lprintf(LOG_WARNING, "Node %d !WARNING native doors which use sockets will not function"
					        , new_node->cfg.node_num);
					close_socket(new_node->passthru_socket);
					new_node->passthru_socket = INVALID_SOCKET;
					close_socket(tmp_sock);
					goto NO_PASSTHRU;
				}
				close_socket(tmp_sock);
				new_node->passthru_thread_running = true;
				_beginthread(passthru_thread, 0, new_node);

NO_PASSTHRU:
				if (ssh) {
					SAFECOPY(new_node->connection, "SSH");
					new_node->node_connection = NODE_CONNECTION_SSH;
					new_node->sys_status |= SS_SSH;
					new_node->telnet_mode |= TELNET_MODE_OFF; // SSH does not use Telnet commands
					new_node->ssh_session = sbbs->ssh_session;
					new_node->online = ON_REMOTE;
				}
				/* Wait for pending data to be sent then turn off ssh_mode for uber-output */
				while (sbbs->output_thread_running && RingBufFull(&sbbs->outbuf))
					SLEEP(1);
				pthread_mutex_lock(&sbbs->ssh_mutex);
				sbbs->ssh_mode = false;
				sbbs->ssh_session = 0; // Don't allow subsequent SSH connections to affect this one (!)
				pthread_mutex_unlock(&sbbs->ssh_mutex);
			}

			uint32_t client_count = protected_uint32_adjust_fetch(&node_threads_running, 1);
			new_node->input_thread_running = true;
			new_node->autoterm = sbbs->autoterm;
			update_terminal(new_node, sbbs->term);
			new_node->term->cols = sbbs->term->cols;
			new_node->input_thread = (HANDLE)_beginthread(input_thread, 0, new_node);
			new_node->output_thread_running = true;
			_beginthread(output_thread, 0, new_node);
			_beginthread(node_thread, 0, new_node);
			served++;
			if (client_count > client_highwater) {
				client_highwater = client_count;
				if (client_highwater > 1)
					lprintf(LOG_NOTICE, "Node %d New active client highwater mark: %u"
					        , node_num, client_highwater);
				mqtt_pub_uintval(&mqtt, TOPIC_SERVER, "highwater", mqtt.highwater = client_highwater);
			}
		}

		set_state(terminate_server ? SERVER_STOPPING : SERVER_RELOADING);

		// Close all open sockets
		for (int i = 0; i < MAX_NODES; i++)  {
			if (node_socket[i] != INVALID_SOCKET) {
				lprintf(LOG_INFO, "Closing node %d socket %d", i + 1, node_socket[i]);
				close_socket(node_socket[i]);
				node_socket[i] = INVALID_SOCKET;
			}
#ifdef __unix__
			if (uspy_socket[i] != INVALID_SOCKET) {
				close_socket(uspy_socket[i]);
				uspy_socket[i] = INVALID_SOCKET;
			}
			snprintf(str, sizeof(uspy_addr.un.sun_path), "%slocalspy%d.sock", startup->temp_dir, i + 1);
			if (fexist(str))
				unlink(str);
#endif
		}

		sbbs->client_socket = INVALID_SOCKET;
		if (events != NULL)
			events->terminated = true;
		// Wake-up BBS output thread so it can terminate
		SetEvent(sbbs->outbuf.data_event);
		SetEvent(sbbs->outbuf.highwater_event);

		// Wait for all node threads to terminate
		if (protected_uint32_value(node_threads_running)) {
			lprintf(LOG_INFO, "Waiting for %d node threads to terminate...", protected_uint32_value(node_threads_running));
			start = time(NULL);
			while (protected_uint32_value(node_threads_running)) {
				if (time(NULL) - start > TIMEOUT_THREAD_WAIT) {
					lprintf(LOG_ERR, "!TIMEOUT waiting for %d node thread(s) to "
					        "terminate", protected_uint32_value(node_threads_running));
					break;
				}
				mswait(100);
			}
			lprintf(LOG_INFO, "Done waiting for node threads to terminate");
		}

		// Wait for Events thread to terminate
		if (events != NULL && events->event_thread_running) {
			lprintf(LOG_INFO, "Waiting for events thread to terminate...");
			start = time(NULL);
			while (events->event_thread_running) {
#if 0 /* the events thread can/will segfault if it continues to run and dereference sbbs->cfg */
				if (time(NULL) - start > TIMEOUT_THREAD_WAIT) {
					lprintf(LOG_ERR, "!TIMEOUT waiting for BBS events thread to "
					        "terminate");
					break;
				}
#endif
				mswait(100);
			}
			lprintf(LOG_INFO, "Done waiting for events thread to terminate");
		}

		// Wait for BBS output thread to terminate
		if (sbbs->output_thread_running) {
			lprintf(LOG_INFO, "Waiting for system output thread to terminate...");
			start = time(NULL);
			while (sbbs->output_thread_running) {
				if (time(NULL) - start > TIMEOUT_THREAD_WAIT) {
					lprintf(LOG_ERR, "!TIMEOUT waiting for BBS output thread to "
					        "terminate");
					break;
				}
				mswait(100);
			}
			lprintf(LOG_INFO, "Done waiting for system output thread to terminate");
		}

		// Wait for BBS passthru thread to terminate
		if (sbbs->passthru_thread_running) {
			lprintf(LOG_INFO, "Waiting for passthru thread to terminate...");
			start = time(NULL);
			while (sbbs->passthru_thread_running) {
				if (time(NULL) - start > TIMEOUT_THREAD_WAIT) {
					lprintf(LOG_ERR, "!TIMEOUT waiting for passthru thread to terminate");
					break;
				}
				mswait(100);
			}
			lprintf(LOG_INFO, "Done waiting for passthru threads to terminate");
		}

		// Set all nodes' status to OFFLINE
		for (int i = first_node; i <= last_node; i++) {
			if (sbbs->getnodedat(i, &node, true)) {
				node.status = NODE_OFFLINE;
				sbbs->putnodedat(i, &node);
			}
		}

		if (events != NULL) {
			if (events->event_thread_running)
				lprintf(LOG_ERR, "!Events thread still running, can't delete");
			else
				delete events;
		}

		if (sbbs->passthru_thread_running || sbbs->output_thread_running)
			lprintf(LOG_ERR, "!System I/O thread still running, can't delete");
		else
			delete sbbs;

		cleanup(0);

		if (!terminate_server) {
			lprintf(LOG_INFO, "Recycling server...");
			mswait(2000);
			if (startup->recycle != NULL)
				startup->recycle(startup->cbdata);
		}

	} while (!terminate_server);

}
