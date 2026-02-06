/* Synchronet Mail (SMTP/POP3) server and sendmail threads */

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

/* ANSI C Library headers */
#include <limits.h>         /* UINT_MAX */
#include <stdio.h>
#include <stdlib.h>         /* ltoa in GNU C lib */
#include <stdarg.h>         /* va_list */
#include <string.h>         /* strrchr */
#include <fcntl.h>          /* Open flags */
#include <errno.h>          /* errno */

/* Synchronet-specific headers */
#undef SBBS /* this shouldn't be defined unless building sbbs.dll/libsbbs.so */
#include "sbbs.h"
#include "mailsrvr.h"
#include "utf8.h"
#include "mime.h"
#include "md5.h"
#include "crc32.h"
#include "base64.h"
#include "ini_file.h"
#include "netwrap.h"    /* getNameServerList() */
#include "xpendian.h"
#include "xpprintf.h"
#include "js_rtpool.h"
#include "js_request.h"
#include "multisock.h"
#include "mxlookup.h"
#include "ssl.h"
#include "cryptlib.h"
#include "trashcan.hpp"
#include "git_branch.h"
#include "git_hash.h"

/* Constants */
static const char* server_name = "Synchronet Mail Server";
static const char* server_abbrev = "mail";
#define FORWARD         "forward:"
#define NO_FORWARD      "local:"
#define NO_SPAM         "#nospam"

#define pop_error       "-ERR System Error: %s, try again later"
#define pop_auth_error  "-ERR Authentication failure"
#define ok_rsp          "250 OK"
#define auth_ok         "235 User Authenticated"
#define smtp_error      "421 System Error: %s, try again later"
#define insuf_stor      "452 Insufficient system storage"
#define badarg_rsp      "501 Bad argument"
#define badseq_rsp      "503 Bad sequence of commands"
#define badauth_rsp     "535 Authentication failure"
#define badrsp_err      "%s replied with:\r\n\"%s\"\r\ninstead of the expected reply:\r\n\"%s ...\""

#define TIMEOUT_THREAD_WAIT     60      /* Seconds */
#define DNSBL_THROTTLE_VALUE    1000    /* Milliseconds */
#define SMTP_MAX_BAD_CMDS       9
#define SMTP_MAX_CMD_LEN        510     /* Excluding CRLF */
#define RFC822_MAX_LINE_LEN     998     /* Excluding CRLF */

static mail_startup_t*    startup = NULL;
static scfg_t             scfg;
static char*              text[TOTAL_TEXT];
static struct xpms_set *  mail_set = NULL;
static bool               terminated = false;
static protected_uint32_t active_clients;
static protected_uint32_t thread_count;
static volatile uint32_t  client_highwater = 0;
static volatile int       active_sendmail = 0;
static volatile bool      sendmail_running = false;
static bool               terminate_server = false;
static volatile bool      terminate_sendmail = false;
static sem_t              sendmail_wakeup_sem;
static volatile time_t    uptime;
static str_list_t         pause_semfiles;
static str_list_t         recycle_semfiles;
static str_list_t         shutdown_semfiles;
static int                mailproc_count;
static js_server_props_t  js_server_props;
static link_list_t        current_logins;
static link_list_t        current_connections;
static bool               savemsg_mutex_created = false;
static pthread_mutex_t    savemsg_mutex;
static struct mqtt        mqtt;

static trashCan*          ip_can = nullptr;
static trashCan*          ip_silent_can = nullptr;
static trashCan*          host_can = nullptr;

static const char*        servprot_smtp = "SMTP";
static const char*        servprot_submission = "SMTP";
static const char*        servprot_submissions = "SMTPS";
static const char*        servprot_pop3 = "POP3";
static const char*        servprot_pop3s = "POP3S";

struct {
	volatile ulong sockets;
	volatile ulong errors;
	volatile ulong crit_errors;
	volatile ulong connections_exceeded;
	volatile ulong connections_ignored;
	volatile ulong connections_refused;
	volatile ulong connections_served;
	volatile ulong pop3_served;
	volatile ulong smtp_served;
	/* SMTP: */
	volatile ulong sessions_refused;
	volatile ulong msgs_ignored;
	volatile ulong msgs_refused;
	volatile ulong msgs_received;
} stats;

struct mailproc {
	char name[INI_MAX_VALUE_LEN];
	char cmdline[INI_MAX_VALUE_LEN];
	char eval[INI_MAX_VALUE_LEN];
	str_list_t to;
	str_list_t from;
	bool passthru;
	bool native;
	bool ignore_on_error;           /* Ignore mail message if cmdline fails */
	bool disabled;
	bool process_spam;
	bool process_dnsbl;
	uint8_t* ar;
	ulong handled;                  /* counter (for stats display) */
} *mailproc_list;

typedef struct {
	SOCKET socket;
	union xp_sockaddr client_addr;
	socklen_t client_addr_len;
	bool tls_port;
	CRYPT_SESSION session;
} smtp_t, pop3_t;

#define GCES(status, server, sock, sess, action) do {                             \
			char *GCES_estr;                                                               \
			int   GCES_level;                                                                 \
			get_crypt_error_string(status, sess, &GCES_estr, action, &GCES_level);  \
			if (GCES_estr) {                                                                  \
				if (GCES_level < startup->tls_error_level)                                     \
				GCES_level = startup->tls_error_level;                                     \
				if (GCES_level > LOG_INFO)                                                      \
				GCES_level = LOG_INFO;                                                      \
				lprintf(GCES_level, "%04d %-5s %s", sock, server, GCES_estr);                     \
				free_crypt_attrstr(GCES_estr);                                                  \
			}                                                                                    \
} while (0)

#define GCESH(status, server, sock, host, sess, action) do {                      \
			char *GCES_estr;                                                               \
			int   GCES_level;                                                                 \
			get_crypt_error_string(status, sess, &GCES_estr, action, &GCES_level);  \
			if (GCES_estr) {                                                                  \
				if (GCES_level < startup->tls_error_level)                                     \
				GCES_level = startup->tls_error_level;                                      \
				if (GCES_level > LOG_INFO)                                                      \
				GCES_level = LOG_INFO;                                                      \
				lprintf(GCES_level, "%04d %-5s [%s] %s", sock, server, host, GCES_estr);         \
				free_crypt_attrstr(GCES_estr);                                                  \
			}                                                                                    \
} while (0)

#define GCESHL(status, server, sock, host, log_level, sess, action) do {                        \
			char *GCES_estr;                                                                            \
			int   GCES_level;                                                                             \
			get_crypt_error_string(status, sess, &GCES_estr, action, &GCES_level);                      \
			if (GCES_estr) {                                                                            \
				lprintf((log_level < startup->tls_error_level) ? startup->tls_error_level : log_level   \
						, "%04d %-5s [%s] %s", sock, server, host, GCES_estr);                                \
				free_crypt_attrstr(GCES_estr);                                                          \
			}                                                                                           \
} while (0)

static int lputs(int level, const char* str)
{
	mqtt_lputs(&mqtt, TOPIC_SERVER, level, str);
	if (level <= LOG_ERR) {
		char errmsg[1024];
		stats.errors++;
		SAFEPRINTF2(errmsg, "%-4s %s", server_abbrev, str);
		errorlog(&scfg, &mqtt, level, startup == NULL ? NULL : startup->host_name, errmsg);
		if (startup != NULL && startup->errormsg != NULL)
			startup->errormsg(startup->cbdata, level, errmsg);
	}

	if (level <= LOG_CRIT)
		stats.crit_errors++;

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

#if defined(__GNUC__)   // Catch printf-format errors with lprintf
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

#ifdef _WINSOCKAPI_

static WSADATA WSAData;
#define SOCKLIB_DESC WSAData.szDescription
static bool WSAInitialized = false;

static bool winsock_startup(void)
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
		if (startup->clients != NULL)
			startup->clients(startup->cbdata, protected_uint32_value(active_clients) + active_sendmail);
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
	listRemoveTaggedNode(&current_connections, sock, /* free_data */ true);
	if (startup != NULL && startup->client_on != NULL)
		startup->client_on(startup->cbdata, false, sock, NULL, false);
	mqtt_client_on(&mqtt, false, sock, NULL, false);
}

static void thread_up(bool setuid)
{
	if (startup != NULL) {
		if (startup->thread_up != NULL)
			startup->thread_up(startup->cbdata, true, setuid);
	}
}

static int32_t thread_down(void)
{
	int32_t count = protected_uint32_adjust_fetch(&thread_count, -1);
	if (startup != NULL) {
		if (startup->thread_up != NULL)
			startup->thread_up(startup->cbdata, false, false);
	}
	return count;
}

void mail_open_socket(SOCKET sock, void* cb_protocol)
{
	char *protocol = (char *)cb_protocol;
	char  error[256];
	char  section[128];

	if (startup != NULL && startup->socket_open != NULL)
		startup->socket_open(startup->cbdata, true);
	SAFEPRINTF(section, "mail|%s", protocol);
	if (set_socket_options(&scfg, sock, section, error, sizeof(error)))
		lprintf(LOG_ERR, "%04d %-5s !ERROR %s", sock, protocol, error);

	stats.sockets++;
}

void mail_close_socket_cb(SOCKET sock, void* cb_protocol)
{
	if (startup != NULL && startup->socket_open != NULL)
		startup->socket_open(startup->cbdata, false);
	stats.sockets--;
}

int mail_close_socket(SOCKET *sock, int *sess)
{
	int result;

	if (*sess != -1) {
		destroy_session(lprintf, *sess);
		*sess = -1;
	}
	if (*sock == INVALID_SOCKET)
		return -1;

	shutdown(*sock, SHUT_RDWR);  /* required on Unix */
	result = closesocket(*sock);
	if (startup != NULL && startup->socket_open != NULL)
		startup->socket_open(startup->cbdata, false);
	stats.sockets--;
	if (result != 0) {
		if (SOCKET_ERRNO != ENOTSOCK)
			lprintf(LOG_WARNING, "%04d !ERROR %d closing socket", *sock, SOCKET_ERRNO);
	}
#if 0 /*def _DEBUG */
	else
		lprintf(LOG_DEBUG, "%04d Socket closed (%d sockets in use)", *sock, stats.sockets);
#endif

	*sock = -1;

	return result;
}

extern "C" int sockprintf(SOCKET sock, const char* prot, CRYPT_SESSION sess, const char *fmt, ...)
{
	int     len;
	int     result;
	va_list argptr;
	char*   sbuf = NULL;

	if (sock == INVALID_SOCKET) {
		lprintf(LOG_WARNING, "%s !INVALID SOCKET in call to sockprintf", prot);
		return 0;
	}

	/* Check socket for writability */
	if (!socket_writable(sock, 300000)) {
		lprintf(LOG_NOTICE, "%04d %-5s !NOTICE socket did not become writable"
		        , sock, prot);
		return 0;
	}

	va_start(argptr, fmt);
	len = vasprintf(&sbuf, fmt, argptr);
	va_end(argptr);

	if (len < 0 || sbuf == NULL) { /* format error or allocation error */
		errprintf(LOG_CRIT, WHERE, "%04d %-5s %s error (%d) formatting string: '%s'", sock, prot, __FUNCTION__, len, fmt);
		free(sbuf);
		return 0;
	}
	if (startup->options & MAIL_OPT_DEBUG_TX)
		lprintf(LOG_DEBUG, "%04d %-5s TX: %.*s", sock, prot, len, sbuf);
	char* newp = static_cast<char *>(realloc(sbuf, len + 2)); // "\r\n"
	if (newp == NULL) { /* format error or allocation error */
		errprintf(LOG_CRIT, WHERE, "%04d %-5s %s re-allocation failure of %d bytes", sock, prot, __FUNCTION__, len + 2);
		free(sbuf);
		return 0;
	}
	sbuf = newp;
	memcpy(sbuf + len, "\r\n", 2);
	len += 2;

	if (sess != -1) {
		int tls_sent;
		int sent = 0;
		while (sent < len) {
			result = cryptPushData(sess, sbuf + sent, len - sent, &tls_sent);
			if (result == CRYPT_OK)
				sent += tls_sent;
			else {
				GCES(result, prot, sock, sess, "pushing data");
				free(sbuf);
				return 0;
			}
		}
		if ((result = cryptFlushData(sess)) != CRYPT_OK) {
			GCES(result, prot, sock, sess, "flushing data");
			free(sbuf);
			return 0;
		}
	}
	else {
		// It looks like this could stutter on partial sends -- Deuce
		while ((result = sendsocket(sock, sbuf, len)) != len) {
			if (result == SOCKET_ERROR) {
				if (SOCKET_ERRNO == EWOULDBLOCK) {
					YIELD();
					continue;
				}
				if (SOCKET_ERRNO == ECONNRESET)
					lprintf(LOG_NOTICE, "%04d %-5s Connection reset by peer on send", sock, prot);
				else if (SOCKET_ERRNO == ECONNABORTED)
					lprintf(LOG_NOTICE, "%04d %-5s Connection aborted by peer on send", sock, prot);
				else
					lprintf(LOG_NOTICE, "%04d %-5s !ERROR %d sending on socket", sock, prot, SOCKET_ERRNO);
				free(sbuf);
				return 0;
			}
			lprintf(LOG_WARNING, "%04d %-5s !ERROR: short send on socket: %d instead of %d", sock, prot, result, len);
		}
	}
	free(sbuf);
	return len;
}

static void sockerror(SOCKET socket, const char* prot, int rd, const char* action)
{
	if (rd == 0)
		lprintf(LOG_NOTICE, "%04d %-5s Socket closed by peer on %s"
		        , socket, prot, action);
	else if (rd == SOCKET_ERROR) {
		if (SOCKET_ERRNO == ECONNRESET)
			lprintf(LOG_NOTICE, "%04d %-5s Connection reset by peer on %s"
			        , socket, prot, action);
		else if (SOCKET_ERRNO == ECONNABORTED)
			lprintf(LOG_NOTICE, "%04d %-5s Connection aborted by peer on %s"
			        , socket, prot, action);
		else
			lprintf(LOG_NOTICE, "%04d %-5s !SOCKET ERROR %d on %s"
			        , socket, prot, SOCKET_ERRNO, action);
	} else
		lprintf(LOG_WARNING, "%04d %-5s !SOCKET ERROR: unexpected return value %d from %s"
		        , socket, prot, rd, action);
}

static int sock_recvbyte(SOCKET sock, const char* prot, CRYPT_SESSION sess, char *buf, time_t start)
{
	int len = 0;
	int ret;
	int i;

	if (sess > -1) {
		while (1) {
			ret = cryptPopData(sess, buf, 1, &len);
			GCES(ret, prot, sock, sess, "popping data");
			switch (ret) {
				case CRYPT_OK:
					break;
				case CRYPT_ERROR_TIMEOUT:
					return -1;
				case CRYPT_ERROR_COMPLETE:
					return 0;
				default:
					if (ret < -1)
						return ret;
					return -2;
			}
			if (len)
				return len;
			if (startup->max_inactivity && (time(NULL) - start) > startup->max_inactivity) {
				lprintf(LOG_WARNING, "%04d %-5s !TIMEOUT in sock_recvbyte (%u seconds):  INACTIVE SOCKET"
				        , sock, prot, startup->max_inactivity);
				return -1;
			}
		}
	}
	else {
		if (!socket_readable(sock, startup->max_inactivity * 1000)) {
			if (startup->max_inactivity && (time(NULL) - start) > startup->max_inactivity) {
				lprintf(LOG_WARNING, "%04d %-5s !TIMEOUT in sock_recvbyte (%u seconds):  INACTIVE SOCKET"
				        , sock, prot, startup->max_inactivity);
				return -1;
			}
			return 0;
		}
		i = recv(sock, buf, 1, 0);
		if (i < 1)
			sockerror(sock, prot, i, "receive");
		return i;
	}
}

static int sockreadline(SOCKET socket, const char* prot, CRYPT_SESSION sess, char* buf, int len)
{
	char   ch;
	int    i, rd = 0;
	time_t start;

	buf[0] = 0;

	start = time(NULL);

	if (socket == INVALID_SOCKET) {
		lprintf(LOG_WARNING, "%s !INVALID SOCKET in call to sockreadline", prot);
		return -1;
	}

	while (rd < len - 1) {

		if (terminated || terminate_server) {
			lprintf(LOG_WARNING, "%04d %-5s !ABORTING sockreadline", socket, prot);
			return -1;
		}

		i = sock_recvbyte(socket, prot, sess, &ch, start);

		if (i < 1)
			return -1;

		if (ch == '\n' /* && rd>=1 */) { /* Mar-9-2003: terminate on sole LF */
			break;
		}
		buf[rd++] = ch;
	}
	if (rd > 0 && buf[rd - 1] == '\r')
		rd--;
	buf[rd] = 0;

	return rd;
}

static bool sockgetrsp(SOCKET socket, const char* prot, CRYPT_SESSION sess, const char* rsp, char *buf, int len)
{
	int rd;

	while (1) {
		rd = sockreadline(socket, prot, sess, buf, len);
		if (rd < 1) {
			if (rd == 0 && rsp != NULL)
				lprintf(LOG_NOTICE, "%04d %-5s !RECEIVED BLANK RESPONSE, Expected '%s'", socket, prot, rsp);
			return false;
		}
		if (buf[3] == '-') { /* Multi-line response */
			if (startup->options & MAIL_OPT_DEBUG_RX_RSP)
				lprintf(LOG_DEBUG, "%04d %-5s RX: %s", socket, prot, buf);
			continue;
		}
		if (rsp != NULL && strnicmp(buf, rsp, strlen(rsp))) {
			lprintf(LOG_NOTICE, "%04d %-5s !INVALID RESPONSE: '%s' Expected: '%s'", socket, prot, buf, rsp);
			return false;
		}
		break;
	}
	if (startup->options & MAIL_OPT_DEBUG_RX_RSP)
		lprintf(LOG_DEBUG, "%04d %-5s RX: %s", socket, prot, buf);
	return true;
}

static int sockgetrsp_opt(SOCKET socket, const char* prot, CRYPT_SESSION sess, const char* rsp, const char *opt, char *buf, int len)
{
	int    rd;
	int    ret = 0;
	size_t moptlen;
	char * mopt;

	moptlen = strlen(rsp) + strlen(opt) + 1;
	mopt = static_cast<char *>(malloc(moptlen + 1));
	if (mopt == NULL)
		return -1;
	sprintf(mopt, "%s-%s", rsp, opt);
	while (1) {
		rd = sockreadline(socket, prot, sess, buf, len);
		if (rd < 1) {
			if (rd == 0)
				lprintf(LOG_NOTICE, "%04d %-5s !RECEIVED BLANK RESPONSE, Expected '%s'", socket, prot, rsp);
			free(mopt);
			return -1;
		}
		if (buf[3] == '-') { /* Multi-line response */
			if (strncmp(buf, mopt, moptlen) == 0)
				ret = 1;
			if (startup->options & MAIL_OPT_DEBUG_RX_RSP)
				lprintf(LOG_DEBUG, "%04d %-5s RX: %s", socket, prot, buf);
			continue;
		}
		if (strnicmp(buf, rsp, strlen(rsp))) {
			lprintf(LOG_NOTICE, "%04d %-5s !INVALID RESPONSE: '%s' Expected: '%s'", socket, prot, buf, rsp);
			free(mopt);
			return -1;
		}
		break;
	}
	mopt[strlen(rsp)] = ' ';
	if (strncmp(buf, mopt, moptlen) == 0)
		ret = 1;
	free(mopt);
	if (startup->options & MAIL_OPT_DEBUG_RX_RSP)
		lprintf(LOG_DEBUG, "%04d %-5s RX: %s", socket, prot, buf);
	return ret;
}

/* non-standard, but documented (mostly) in draft-newman-msgheader-originfo-05 */
void originator_info(SOCKET socket, const char* protocol, CRYPT_SESSION sess, smbmsg_t* msg)
{
	char* user      = msg->from_ext;
	char* login     = static_cast<char *>(smb_get_hfield(msg, SENDERUSERID, NULL));
	char* server    = static_cast<char *>(smb_get_hfield(msg, SENDERSERVER, NULL));
	char* client    = static_cast<char *>(smb_get_hfield(msg, SENDERHOSTNAME, NULL));
	char* addr      = static_cast<char *>(smb_get_hfield(msg, SENDERIPADDR, NULL));
	char* prot      = static_cast<char *>(smb_get_hfield(msg, SENDERPROTOCOL, NULL));
	char* port      = static_cast<char *>(smb_get_hfield(msg, SENDERPORT, NULL));
	char* time      = static_cast<char *>(smb_get_hfield(msg, SENDERTIME, NULL));

	if (user || login || server || client || addr || prot || port || time)
		sockprintf(socket, protocol, sess
		           , "X-Originator-Info: account=%s; login-id=%s; server=%s; client=%s; addr=%s; prot=%s; port=%s; time=%s"
		           , user
		           , login
		           , server
		           , client
		           , addr
		           , prot
		           , port
		           , time
		           );
}

char* angle_bracket(char* buf, size_t max, const char* addr)
{
	if (*addr == '<')
		safe_snprintf(buf, max, "%s", addr);
	else
		safe_snprintf(buf, max, "<%s>", addr);
	return buf;
}

int compare_addrs(const char* addr1, const char* addr2)
{
	char tmp1[256];
	char tmp2[256];

	return strcmp(angle_bracket(tmp1, sizeof(tmp1), addr1), angle_bracket(tmp2, sizeof(tmp2), addr2));
}

// MIME-Encode a word (Q-encoded UTF-8, only)
size_t mime_encode_word(char* buf, size_t size, const char* src)
{
	size_t len = snprintf(buf, size - 1, "=?utf-8?q?");
	// RFC 2047: "An 'encoded-word' may not be more than 75 characters long"
	for (const char* p = src; *p != '\0' && len < (size - 1) && len < 71; ++p) {
		if (*p >= 0 && *p != '=' && *p != ' ') { // US-ASCII char, represented without Q-encoding
			buf[len++] = *p;
			continue;
		}
		len += snprintf(buf + len, size - (len + 1), "=%02X", (unsigned char)*p);
	}
	len += snprintf(buf + len, size - (len + 1), "?=");
	return len;
}

// MIME-Encode words in string, when necessary
static char* encode_header_field(const char* src, char* buf, size_t size, const smbmsg_t* msg)
{
	if (str_is_ascii(src))
		return (char*)src;
	char tmp[256];
	if (!(msg->hdr.auxattr & MSG_HFIELDS_UTF8))
		cp437_to_utf8_str(src, tmp, sizeof tmp, /* min-char-val: */ '\x80');
	else
		SAFECOPY(tmp, src);
	size_t len = 0;
	char*  state = NULL;
	for (char* p = strtok_r(tmp, " \t", &state); p != NULL && len < (size - 1); p = strtok_r(NULL, " \t", &state)) {
		if (len)
			buf[len++] = ' ';
		if (str_is_ascii(p))
			len += snprintf(buf + len, size - (len + 1), "%s", p);
		else
			len += mime_encode_word(buf + len, size - (len + 1), p);
	}
	buf[len] = '\0';
	return buf;
}

static ulong sockmimetext(SOCKET socket, const char* prot, CRYPT_SESSION sess, smbmsg_t* msg, char* msgtxt, ulong maxlines
                          , str_list_t file_list, char* mime_boundary)
{
	char  toaddr[256] = "";
	char  msgid[256];
	char  tmp[256];
	char  date[64];
	char  encoded_text[256];
	char* p;
	const char* np;
	int   i;
	int   s;
	ulong lines;
	int   len, tlen;

	/* HEADERS (in recommended order per RFC822 4.1) */

	if (msg->reverse_path != NULL)
		if (!sockprintf(socket, prot, sess, "Return-Path: %s", msg->reverse_path))
			return 0;

	for (i = 0; i < msg->total_hfields; i++)
		if (msg->hfield[i].type == SMTPRECEIVED && msg->hfield_dat[i] != NULL)
			if (!sockprintf(socket, prot, sess, "Received: %s", (char*)msg->hfield_dat[i]))
				return 0;

	if (!sockprintf(socket, prot, sess, "Date: %s", msgdate(msg->hdr.when_written, date)))
		return 0;

	if ((p = static_cast<char *>(smb_get_hfield(msg, RFC822FROM, NULL))) != NULL)
		s = sockprintf(socket, prot, sess, "From: %s", p); /* use original RFC822 header field */
	else {
		char fromname[256];
		char fromaddr[256];
		smtp_netmailaddr(&scfg, msg, fromname, sizeof(fromname), fromaddr, sizeof(fromaddr));
		s = sockprintf(socket, prot, sess, "From: %s %s"
		               , encode_header_field(fromname, encoded_text, sizeof encoded_text, msg)
		               , angle_bracket(tmp, sizeof(tmp), fromaddr));
	}
	if (!s)
		return 0;

	if ((p = static_cast<char *>(smb_get_hfield(msg, RFC822ORG, NULL))) != NULL) {
		if (!sockprintf(socket, prot, sess, "Organization: %s", p))
			return 0;
	} else {
		if (msg->from_org != NULL || msg->from_net.type == NET_NONE)
			if (!sockprintf(socket, prot, sess, "Organization: %s"
			                , encode_header_field(msg->from_org == NULL ? scfg.sys_name : msg->from_org, encoded_text, sizeof encoded_text, msg)))
				return 0;
	}

	p = static_cast<char *>(smb_get_hfield(msg, RFC822SUBJECT, NULL));
	if (!sockprintf(socket, prot, sess, "Subject: %s", p == NULL ? encode_header_field(msg->subj, encoded_text, sizeof encoded_text, msg) : p))
		return 0;

	if ((p = static_cast<char *>(smb_get_hfield(msg, RFC822TO, NULL))) != NULL)
		s = sockprintf(socket, prot, sess, "To: %s", p); /* use original RFC822 header field (MIME-Encoded) */
	else if ((p = msg->to_list) != NULL)
		s = sockprintf(socket, prot, sess, "To: %s", p); /* use original RFC822 header field */
	else {
		const char* to = encode_header_field(msg->to, encoded_text, sizeof encoded_text, msg);
		if (strchr(msg->to, '@') != NULL || msg->to_net.addr == NULL)
			s = sockprintf(socket, prot, sess, "To: %s", to); /* Avoid double-@ */
		else if (msg->to_net.type == NET_INTERNET || msg->to_net.type == NET_QWK) {
			if (strchr((char*)msg->to_net.addr, '<') != NULL)
				s = sockprintf(socket, prot, sess, "To: %s", (char*)msg->to_net.addr);
			else
				s = sockprintf(socket, prot, sess, "To: \"%s\" <%s>", to, (char*)msg->to_net.addr);
		} else if (msg->to_net.type == NET_FIDO) {
			char faddrbuf[64];
			s = sockprintf(socket, prot, sess, "To: \"%s\" (%s)", to, smb_faddrtoa((fidoaddr_t*)msg->to_net.addr, faddrbuf));
		} else {
			usermailaddr(&scfg, toaddr, msg->to);
			s = sockprintf(socket, prot, sess, "To: \"%s\" <%s>", to, toaddr);
		}
	}
	if (!s)
		return 0;
	if ((p = static_cast<char *>(smb_get_hfield(msg, RFC822CC, NULL))) != NULL || msg->cc_list != NULL)
		if (!sockprintf(socket, prot, sess, "Cc: %s", p == NULL ? msg->cc_list : p))
			return 0;
	np = NULL;
	p = static_cast<char *>(smb_get_hfield(msg, RFC822REPLYTO, NULL));
	if (p == NULL && (p = msg->replyto_list) == NULL) {
		np = msg->replyto;
		if (msg->replyto_net.type == NET_INTERNET)
			p = static_cast<char *>(msg->replyto_net.addr);
	}
	if (p != NULL && strchr((char*)p, '@') != NULL) {
		if (np != NULL)
			s = sockprintf(socket, prot, sess, "Reply-To: \"%s\" <%s>", np, p);
		else
			s = sockprintf(socket, prot, sess, "Reply-To: %s", p);
	}
	if (!s)
		return 0;
	if (!sockprintf(socket, prot, sess, "Message-ID: %s", get_msgid(&scfg, INVALID_SUB, msg, msgid, sizeof(msgid))))
		return 0;
	if (msg->reply_id != NULL)
		if (!sockprintf(socket, prot, sess, "In-Reply-To: %s", msg->reply_id))
			return 0;

	if (msg->hdr.priority != SMB_PRIORITY_UNSPECIFIED)
		if (!sockprintf(socket, prot, sess, "X-Priority: %u", (uint)msg->hdr.priority))
			return 0;

	originator_info(socket, prot, sess, msg);

	/* Include all possible FidoNet header fields here */
	for (i = 0; i < msg->total_hfields; i++) {
		switch (msg->hfield[i].type) {
			case FIDOCTRL:
			case FIDOAREA:
			case FIDOSEENBY:
			case FIDOPATH:
			case FIDOMSGID:
			case FIDOREPLYID:
			case FIDOPID:
			case FIDOFLAGS:
			case FIDOTID:
				if (!sockprintf(socket, prot, sess, "%s: %s", smb_hfieldtype(msg->hfield[i].type), (char*)msg->hfield_dat[i]))
					return 0;
				break;
		}
	}
	for (i = 0; i < msg->total_hfields; i++) {
		if (msg->hfield[i].type == RFC822HEADER) {
			if (!sockprintf(socket, prot, sess, "%s", (char*)msg->hfield_dat[i]))
				return 0;
		}
	}
	const char* charset = msg->text_charset;
	if (charset == NULL) {
		if (smb_msg_is_utf8(msg) || (msg->hdr.auxattr & MSG_HFIELDS_UTF8))
			charset = "UTF-8";
		else if (str_is_ascii(msgtxt))
			charset = "US-ASCII";
		else
			charset = "IBM437";
	}

	if (strListCount(file_list)) {   /* File attachments */
		mimeheaders(socket, prot, sess, mime_boundary);
		sockprintf(socket, prot, sess, "");
		mimeblurb(socket, prot, sess, mime_boundary);
		sockprintf(socket, prot, sess, "");
		mimetextpartheader(socket, prot, sess, mime_boundary, msg->text_subtype, charset);
	} else {
		/* Default MIME Content-Type for non-Internet messages */
		if (msg->from_net.type != NET_INTERNET && msg->content_type == NULL) {
			sockprintf(socket, prot, sess, "Content-Type: text/plain; charset=%s", charset);
			sockprintf(socket, prot, sess, "Content-Transfer-Encoding: 8bit");
		}
	}
	if (!sockprintf(socket, prot, sess, ""))    /* Header Terminator */
		return 0;

	/* MESSAGE BODY */
	lines = 0;
	if (*msgtxt == '\0')
		np = "\r\n"; // Send at least one line of message text (issue #822)
	else
		np = msgtxt;
	long bytes = 0;
	while (*np && lines < maxlines) {
		len = 0;
		while (len < RFC822_MAX_LINE_LEN && *(np + len) != 0 && *(np + len) != '\n')
			len++;

		tlen = len;
		while (tlen && *(np + (tlen - 1)) <= ' ') /* Takes care of '\r' or spaces */
			tlen--;

		if (!sockprintf(socket, prot, sess, "%s%.*s", *np == '.' ? ".":"", tlen, np))
			break;
		bytes += tlen;
		lines++;
		if (*(np + len) == '\r')
			len++;
		if (*(np + len) == '\n')
			len++;
		np += len;
		/* release time-slices every x lines */
		if (startup->lines_per_yield
		    && !(lines % startup->lines_per_yield))
			YIELD();
		if ((lines % 100) == 0)
			lprintf(LOG_DEBUG, "%04d %-5s sent %lu lines (%ld bytes) of body text"
			        , socket, prot, lines, bytes);
	}
	lprintf(LOG_DEBUG, "%04d %-5s sent %lu lines (%ld bytes) of body text"
	        , socket, prot, lines, bytes);
	if (file_list != NULL) {
		for (i = 0; file_list[i]; i++) {
			sockprintf(socket, prot, sess, "");
			lprintf(LOG_INFO, "%04u %s MIME Encoding and sending %s", socket, prot, file_list[i]);
			if (!mimeattach(socket, prot, sess, mime_boundary, file_list[i]))
				errprintf(LOG_ERR, WHERE, "%04u %s !ERROR opening/encoding/sending %s", socket, prot, file_list[i]);
			else {
				if (msg->hdr.auxattr & MSG_KILLFILE)
					if (remove(file_list[i]) != 0)
						lprintf(LOG_WARNING, "%04u %s !ERROR %d (%s) removing %s", socket, prot, errno, strerror(errno), file_list[i]);
			}
			endmime(socket, prot, sess, mime_boundary);
		}
	}
	sockprintf(socket, prot, sess, ".");   /* End of text */
	return lines;
}

static ulong sockmsgtxt(SOCKET socket, const char* prot, CRYPT_SESSION sess, smbmsg_t* msg, char* msgtxt, ulong maxlines)
{
	char       dirname[MAX_PATH + 1];
	char       filepath[MAX_PATH + 1];
	ulong      retval;
	char*      boundary = NULL;
	str_list_t file_list = NULL;

	if (msg->hdr.auxattr & MSG_FILEATTACH) {
		if (msg->idx.to != 0)
			SAFEPRINTF2(dirname, "%sfile/%04u.in", scfg.data_dir, msg->idx.to);
		else
			SAFEPRINTF2(dirname, "%sfile/%04u.out", scfg.data_dir, msg->idx.from);

		boundary = mimegetboundary();
		file_list = strListInit();

		/* filename(s) in subject */
		char* p = msg->subj;
		SKIP_WHITESPACE(p);
		while (p != NULL && *p != '\0') {
			char delim = ' ';
			if (*p == '"') {
				delim = '"';
				p++;
			}
			char* tp = strchr(p, delim);
			if (tp == NULL) {
				if (delim != ' ')
					break;
			} else
				*tp = '\0';
			char* fname = getfname(truncsp(p));
			if (strcspn(fname, ILLEGAL_FILENAME_CHARS) == strlen(fname)) {
				SAFEPRINTF2(filepath, "%s/%s", dirname, fname);
				strListPush(&file_list, filepath);
			}
			if (tp == NULL)
				break;
			p = tp + 1;
			SKIP_WHITESPACE(p);
		}
	}

	retval = sockmimetext(socket, prot, sess, msg, msgtxt, maxlines, file_list, boundary);

	strListFree(&file_list);

	if (boundary != NULL)
		free(boundary);

	return retval;
}

static in_addr_t resolve_ip(const char *inaddr)
{
	char*    p;
	char*    addr;
	char     buf[128];
	struct addrinfo* res;
	struct addrinfo hints {};
	in_addr_t ipa = INADDR_NONE;

	SAFECOPY(buf, inaddr);
	addr = buf;
	if (*addr == '[' && *(p = lastchar(addr)) == ']') { /* Support [ip_address] notation */
		addr++;
		*p = 0;
	}

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

/****************************************************************************/
/* Consecutive failed login (possible password hack) attempt tracking		*/
/****************************************************************************/
/* Counter is global so it is tracked between multiple connections.			*/
/* Failed consecutive login attempts > 10 will generate a hacklog entry	and	*/
/* immediately disconnect (after the usual failed-login delay).				*/
/* A failed login from a different host resets the counter.					*/
/* A successful login from the same host resets the counter.				*/
/****************************************************************************/

static void badlogin(SOCKET sock, CRYPT_SESSION sess, const char* resp
                     , const char* user, char* passwd, client_t* client, union xp_sockaddr* addr)
{
	char            tmp[128];
	char            reason[128];
	ulong           count;
	login_attempt_t attempt;

	if (user == NULL)
		user = "<unspecified>";

	if (addr != NULL) {
		SAFEPRINTF(reason, "%s LOGIN", client->protocol);
		count = loginFailure(startup->login_attempt_list, addr, client->protocol, user, passwd, &attempt);
		if (count > 1)
			lprintf(LOG_NOTICE, "%04d %-5s [%s] !%lu " STR_FAILED_LOGIN_ATTEMPTS " in %s"
			        , sock, client->protocol, client->addr, count
			        , duration_estimate_to_vstr(attempt.time - attempt.first, tmp, sizeof tmp, 1, 1));
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
			filter_ip(&scfg, client->protocol, reason, client->host, client->addr, user, /* fname: */ NULL, startup->login_attempt.filter_duration);
		}
	}

	mswait(startup->login_attempt.delay);
	sockprintf(sock, client->protocol, sess, "%s", resp);
}

static bool pop3_client_thread(pop3_t* pop3)
{
	char*             p;
	char              str[128];
	char              buf[512];
	char              host_name[128];
	char              host_ip[INET6_ADDRSTRLEN];
	char              server_ip[INET6_ADDRSTRLEN];
	char              username[128];
	char              password[128];
	char              challenge[256];
	uchar             digest[MD5_DIGEST_SIZE];
	char*             response = (char*)"";
	char*             msgtxt;
	int               i;
	int               rd;
	bool              activity = true;
	bool              apop = false;
	uint32_t          l;
	long              lm_mode = 0;
	long              lines;
	ulong             lines_sent;
	ulong             login_attempts;
	uint32_t          msgs;
	uint32_t          msgnum;
	ulong             bytes;
	SOCKET            socket;
	smb_t             smb;
	smbmsg_t          msg;
	user_t            user;
	client_t          client;
	mail_t*           mail;
	login_attempt_t   attempted;
	CRYPT_SESSION     session = -1;
	bool              nodelay = true;
	ulong             nb = 0;
	int               stat;
	union xp_sockaddr server_addr;

	socket = pop3->socket;

	SAFECOPY(client.protocol, pop3->tls_port ? "POP3S" : "POP3");

	if (startup->options & MAIL_OPT_DEBUG_POP3)
		lprintf(LOG_DEBUG, "%04d %-5s session thread started", socket, client.protocol);

	socklen_t addr_len = sizeof(server_addr);
	if ((i = getsockname(socket, &server_addr.addr, &addr_len)) != 0) {
		errprintf(LOG_CRIT, WHERE, "%04d %-5s !ERROR %d (%d) getting local address/port of socket"
		          , socket, client.protocol, i, SOCKET_ERRNO);
		return false;
	}

	inet_addrtop(&pop3->client_addr, host_ip, sizeof(host_ip));
	inet_addrtop(&server_addr, server_ip, sizeof(server_ip));

	if (startup->options & MAIL_OPT_DEBUG_POP3)
		lprintf(LOG_INFO, "%04d %-5s [%s] Connection accepted on %s port %u from port %u"
		        , socket, client.protocol, host_ip, server_ip, inet_addrport(&server_addr), inet_addrport(&pop3->client_addr));

	SAFECOPY(host_name, STR_NO_HOSTNAME);
	if (!(startup->options & MAIL_OPT_NO_HOST_LOOKUP)) {
		getnameinfo(&pop3->client_addr.addr, pop3->client_addr_len, host_name, sizeof(host_name), NULL, 0, NI_NAMEREQD);
		if (startup->options & MAIL_OPT_DEBUG_POP3)
			lprintf(LOG_INFO, "%04d %-5s [%s] Hostname: %s", socket, client.protocol, host_ip, host_name);
	}
	if (pop3->tls_port) {
		if (!do_cryptInit(lprintf)) {
			return false;
		}
		if ((stat = cryptCreateSession(&session, CRYPT_UNUSED, CRYPT_SESSION_TLS_SERVER)) != CRYPT_OK) {
			GCESH(stat, client.protocol, socket, host_ip, CRYPT_UNUSED, "creating session");
			return false;
		}
		if ((stat = add_private_key(&scfg, lprintf, session)) != CRYPT_OK) {
			cryptDestroySession(session);
			GCESH(stat, client.protocol, socket, host_ip, session, "setting private key");
			return false;
		}
		pop3->session = session;
		nodelay = true;
		setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
		nb = 0;
		ioctlsocket(socket, FIONBIO, &nb);
		if ((stat = cryptSetAttribute(session, CRYPT_SESSINFO_NETWORKSOCKET, socket)) != CRYPT_OK) {
			GCESH(stat, client.protocol, socket, host_ip, session, "setting session socket");
			return false;
		}
		if ((stat = cryptSetAttribute(session, CRYPT_SESSINFO_ACTIVE, 1)) != CRYPT_OK) {
			GCESH(stat, client.protocol, socket, host_ip, session, "setting session active");
			return false;
		}
		if (startup->max_inactivity) {
			if (cryptSetAttribute(session, CRYPT_OPTION_NET_READTIMEOUT, startup->max_inactivity) != CRYPT_OK) {
				GCESH(stat, client.protocol, socket, host_ip, session, "setting read timeout");
				return false;
			}
		}
	}

	ulong banned = loginBanned(&scfg, startup->login_attempt_list, socket, host_name, startup->login_attempt, &attempted);
	if (banned) {
		char ban_duration[128];
		lprintf(LOG_NOTICE, "%04d %-5s [%s] !TEMPORARY BAN (%lu login attempts, last: %s) - remaining: %s"
		        , socket, client.protocol, host_ip, attempted.count - attempted.dupes, attempted.user
		        , duration_estimate_to_vstr(banned, ban_duration, sizeof ban_duration, 1, 1));
		sockprintf(socket, client.protocol, session, "-ERR Access denied.");
		return false;
	}
	struct trash trash;
	if (ip_can->listed(host_ip, nullptr, &trash)) {
		if (!trash.quiet) {
			char details[128];
			lprintf(LOG_NOTICE, "%04d %-5s [%s] !CLIENT BLOCKED in ip.can %s", socket, client.protocol, host_ip, trash_details(&trash, details, sizeof details));
		}
		sockprintf(socket, client.protocol, session, "-ERR Access denied.");
		return false;
	}
	if (host_can->listed(host_name, nullptr, &trash)) {
		if (!trash.quiet) {
			char details[128];
			lprintf(LOG_NOTICE, "%04d %-5s [%s] !CLIENT BLOCKED in host.can: %s %s"
					, socket, client.protocol, host_ip, host_name, trash_details(&trash, details, sizeof details));
		}
		sockprintf(socket, client.protocol, session, "-ERR Access denied.");
		return false;
	}

	/* Initialize client display */
	client.size = sizeof(client);
	client.time = time32(NULL);
	SAFECOPY(client.addr, host_ip);
	SAFECOPY(client.host, host_name);
	client.port = inet_addrport(&pop3->client_addr);
	SAFECOPY(client.user, STR_UNKNOWN_USER);
	client.usernum = 0;
	client_on(socket, &client, false /* update */);

	if (startup->login_attempt.throttle
	    && (login_attempts = loginAttempts(startup->login_attempt_list, &pop3->client_addr)) > 1) {
		lprintf(LOG_DEBUG, "%04d %-5s [%s] Throttling suspicious connection (%lu login attempts)"
		        , socket, client.protocol, host_ip, login_attempts);
		mswait(login_attempts * startup->login_attempt.throttle);
	}

	mail = NULL;

	do {
		memset(&smb, 0, sizeof(smb));
		memset(&msg, 0, sizeof(msg));
		memset(&user, 0, sizeof(user));
		password[0] = 0;

		srand((unsigned int)(time(NULL) ^ (time_t)GetCurrentThreadId()));   /* seed random number generator */
		(void)rand(); /* throw-away first result */
		safe_snprintf(challenge, sizeof(challenge), "<%x%x%lx%lx@%.128s>"
		              , rand(), socket, (ulong)time(NULL), (ulong)clock(), server_host_name());

		sockprintf(socket, client.protocol, session, "+OK Synchronet %s Server %s%c-%s Ready %s"
		           , client.protocol, VERSION, REVISION, PLATFORM_DESC, challenge);

		/* Requires USER or APOP command first */
		for (i = 5; i; i--) {
			if (!sockgetrsp(socket, client.protocol, session, NULL, buf, sizeof(buf)))
				break;
			if (!strnicmp(buf, "USER ", 5))
				break;
			else if (!strnicmp(buf, "APOP ", 5)) {
				apop = true;
				break;
			}
			else if (!stricmp(buf, "CAPA")) {
				// Capabilities
				sockprintf(socket, client.protocol, session, "+OK Capability list follows");
				sockprintf(socket, client.protocol, session, "TOP\r\nUSER\r\nPIPELINING\r\nUIDL\r\nIMPLEMENTATION Synchronet POP3 Server %s%c-%s\r\n%s.", VERSION, REVISION, PLATFORM_DESC, (session != -1 || !do_cryptInit(lprintf)) ? "" : "STLS\r\n");
				i++;
			}
			else if (!stricmp(buf, "STLS")) {
				if (!do_cryptInit(lprintf)) {
					sockprintf(socket, client.protocol, session, "-ERR STLS command not supported");
					continue;
				}
				sockprintf(socket, client.protocol, session, "+OK Begin TLS negotiation");
				if ((stat = cryptCreateSession(&session, CRYPT_UNUSED, CRYPT_SESSION_TLS_SERVER)) != CRYPT_OK) {
					GCESH(stat, client.protocol, socket, host_ip, CRYPT_UNUSED, "creating session");
					buf[0] = 0;
					break;
				}
				if ((stat = add_private_key(&scfg, lprintf, session)) != CRYPT_OK) {
					cryptDestroySession(session);
					GCESH(stat, client.protocol, socket, host_ip, session, "setting private key");
					buf[0] = 0;
					break;
				}
				pop3->session = session;
				nodelay = true;
				setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
				nb = 0;
				ioctlsocket(socket, FIONBIO, &nb);
				if ((stat = cryptSetAttribute(session, CRYPT_SESSINFO_NETWORKSOCKET, socket)) != CRYPT_OK) {
					GCESH(stat, client.protocol, socket, host_ip, session, "setting network socket");
					buf[0] = 0;
					break;
				}
				if ((stat = cryptSetAttribute(session, CRYPT_SESSINFO_ACTIVE, 1)) != CRYPT_OK) {
					GCESH(stat, client.protocol, socket, host_ip, session, "setting session active");
					buf[0] = 0;
					break;
				}
				if (startup->max_inactivity) {
					if ((stat = cryptSetAttribute(session, CRYPT_OPTION_NET_READTIMEOUT, startup->max_inactivity)) != CRYPT_OK) {
						GCESH(stat, client.protocol, socket, host_ip, session, "setting read timeout");
						buf[0] = 0;
						break;
					}
				}
				i++;
				SAFECOPY(client.protocol, "POP3S");
			}
			else
				sockprintf(socket, client.protocol, session, "-ERR USER, APOP, CAPA, or STLS command expected");
		}
		if (!i || buf[0] == 0) /* no USER or APOP command received */
			break;

		p = buf + 5;
		SKIP_WHITESPACE(p);
		if (apop) {
			if ((response = strrchr(p, ' ')) != NULL)
				*(response++) = 0;
			else
				response = p;
		}
		SAFECOPY(username, p);
		if ((p = strstr(username, NO_SPAM)) != NULL) {
			*p = 0;
			lm_mode = LM_NOSPAM;
		} else
			lm_mode = 0;
		if (!apop) {
			sockprintf(socket, client.protocol, session, "+OK");
			if (!sockgetrsp(socket, client.protocol, session, "PASS ", buf, sizeof(buf))) {
				sockprintf(socket, client.protocol, session, "-ERR PASS command expected");
				break;
			}
			p = buf + 5;
			SKIP_WHITESPACE(p);
			SAFECOPY(password, p);
		}
		user.number = find_login_id(&scfg, username);
		if (!user.number) {
			if (scfg.sys_misc & SM_ECHO_PW)
				lprintf(LOG_NOTICE, "%04d %-5s [%s] !UNKNOWN USER: '%s' (password: %s)"
				        , socket, client.protocol, host_ip, username, password);
			else
				lprintf(LOG_NOTICE, "%04d %-5s [%s] !UNKNOWN USER: '%s'"
				        , socket, client.protocol, host_ip, username);
			badlogin(socket, session, pop_auth_error, username, password, &client, &pop3->client_addr);
			break;
		}
		if ((i = getuserdat(&scfg, &user)) != 0) {
			errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] !ERROR %d getting data on user (%s)"
			          , socket, client.protocol, host_ip, i, username);
			break;
		}
		if (user.misc & (DELETED | INACTIVE)) {
			lprintf(LOG_NOTICE, "%04d %-5s [%s] !DELETED or INACTIVE user #%u (%s)"
			        , socket, client.protocol, host_ip, user.number, username);
			badlogin(socket, session, pop_auth_error, username, password, &client, NULL);
			break;
		}
		if (apop) {
			strlwr(user.pass);  /* this is case-sensitive, so convert to lowercase */
			strcat(challenge, user.pass);
			MD5_calc(digest, challenge, strlen(challenge));
			MD5_hex(str, digest);
			if (strcmp(str, response)) {
				lprintf(LOG_NOTICE, "%04d %-5s [%s] !FAILED APOP authentication: %s"
				        , socket, client.protocol, host_ip, username);
#if 0
				lprintf(LOG_DEBUG, "%04d !POP3 digest data: %s", socket, challenge);
				lprintf(LOG_DEBUG, "%04d !POP3 calc digest: %s", socket, str);
				lprintf(LOG_DEBUG, "%04d !POP3 resp digest: %s", socket, response);
#endif
				badlogin(socket, session, pop_auth_error, username, response, &client, &pop3->client_addr);
				break;
			}
		} else if (stricmp(password, user.pass)) {
			if (scfg.sys_misc & SM_ECHO_PW)
				lprintf(LOG_NOTICE, "%04d %-5s [%s] <%s> !FAILED Password attempt: '%s' expected '%s'"
				        , socket, client.protocol, host_ip, username, password, user.pass);
			else
				lprintf(LOG_NOTICE, "%04d %-5s [%s] <%s> !FAILED Password attempt"
				        , socket, client.protocol, host_ip, username);
			badlogin(socket, session, pop_auth_error, username, password, &client, &pop3->client_addr);
			break;
		}
		if (!chk_ars(&scfg, startup->login_ars, &user, &client)) {
			lprintf(LOG_NOTICE, "%04d %-5s [%s] <%s> !Insufficient server access: %s"
			        , socket, client.protocol, host_ip, username, startup->login_ars);
			badlogin(socket, session, pop_auth_error, username, NULL, &client, &pop3->client_addr);
			break;
		}

		if (user.pass[0]) {
			loginSuccess(startup->login_attempt_list, &pop3->client_addr);
			listAddNodeData(&current_logins, client.addr, strlen(client.addr) + 1, socket, LAST_NODE);
		}

		loginuserdat(&scfg, &user, &client, /* use_prot: */false, /* save_ars: */NULL);

		/* Update client display */
		SAFECOPY(client.user, user.alias);
		client.usernum = user.number;
		client_on(socket, &client, true /* update */);
		activity = false;

		if (startup->options & MAIL_OPT_DEBUG_POP3)
			lprintf(LOG_INFO, "%04d %-5s [%s] <%s> logged-in %s", socket, client.protocol, host_ip, user.alias, apop ? "via APOP":"");
#ifdef _WIN32
		if (startup->sound.login[0] && !sound_muted(&scfg))
			PlaySound(startup->sound.login, NULL, SND_ASYNC | SND_FILENAME);
#endif
		mqtt_user_login(&mqtt, &client);

		SAFEPRINTF(smb.file, "%smail", scfg.data_dir);
		if (smb_islocked(&smb)) {
			lprintf(LOG_WARNING, "%04d %-5s [%s] <%s> !MAIL BASE LOCKED: %s", socket, client.protocol, host_ip, user.alias, smb.last_error);
			sockprintf(socket, client.protocol, session, "-ERR database locked, try again later");
			break;
		}
		smb.retry_time = scfg.smb_retry_time;
		smb.subnum = INVALID_SUB;
		if ((i = smb_open(&smb)) != SMB_SUCCESS) {
			errprintf(LOG_ERR, WHERE, "%04d %-5s [%s] <%s> !ERROR %d (%s) opening %s", socket, client.protocol, host_ip, user.alias, i, smb.last_error, smb.file);
			sockprintf(socket, client.protocol, session, "-ERR %d opening %s", i, smb.file);
			break;
		}

		mail = loadmail(&smb, &msgs, user.number, MAIL_YOUR, lm_mode);
		lprintf(LOG_DEBUG, "%04d %-5s <%s> Loaded %lu messages", socket, client.protocol, user.alias, (ulong)msgs);

		for (l = bytes = 0; l < msgs && !smb_islocked(&smb); l++) {
			msg.hdr.number = mail[l].number;
			if ((i = smb_getmsgidx(&smb, &msg)) != SMB_SUCCESS) {
				errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) getting message index"
				          , socket, client.protocol, user.alias, i, smb.last_error);
				break;
			}
			if ((i = smb_lockmsghdr(&smb, &msg)) != SMB_SUCCESS) {
				lprintf(LOG_WARNING, "%04d %-5s <%s> !ERROR %d (%s) locking message header #%u"
				        , socket, client.protocol, user.alias, i, smb.last_error, msg.hdr.number);
				break;
			}
			i = smb_getmsghdr(&smb, &msg);
			smb_unlockmsghdr(&smb, &msg);
			if (i != 0) {
				errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) line %u, msg #%u"
				          , socket, client.protocol, user.alias, i, smb.last_error, __LINE__, msg.idx.number);
				break;
			}
			bytes += msg.hdr.length + smb_getmsgtxtlen(&msg);
			smb_freemsgmem(&msg);
		}

		if (l < msgs) {
			sockprintf(socket, client.protocol, session, "-ERR message #%d: %d (%s)"
			           , mail[l].number, i, smb.last_error);
			break;
		}

		sockprintf(socket, client.protocol, session, "+OK %u messages (%lu bytes)", msgs, bytes);

		while (1) {  /* TRANSACTION STATE */
			rd = sockreadline(socket, client.protocol, session, buf, sizeof(buf));
			if (rd < 0)
				break;
			truncsp(buf);
			if (startup->options & MAIL_OPT_DEBUG_POP3)
				lprintf(LOG_DEBUG, "%04d %-5s RX: %s", socket, client.protocol, buf);
			if (smb_islocked(&smb)) {
				lprintf(LOG_WARNING, "%04d %-5s [%s] <%s> !MAIL BASE LOCKED: %s", socket, client.protocol, host_ip, user.alias, smb.last_error);
				sockprintf(socket, client.protocol, session, "-ERR database locked, try again later");
				break;
			}
			if (!stricmp(buf, "NOOP")) {
				sockprintf(socket, client.protocol, session, "+OK");
				continue;
			}
			if (!stricmp(buf, "CAPA")) {
				// Capabilities
				sockprintf(socket, client.protocol, session, "+OK Capability list follows");
				sockprintf(socket, client.protocol, session, "TOP\r\nUSER\r\nPIPELINING\r\nUIDL\r\nIMPLEMENTATION Synchronet POP3 Server %s%c-%s\r\n.", VERSION, REVISION, PLATFORM_DESC);
				continue;
			}
			if (!stricmp(buf, "QUIT")) {
				sockprintf(socket, client.protocol, session, "+OK");
				break;
			}
			if (!stricmp(buf, "STAT")) {
				sockprintf(socket, client.protocol, session, "+OK %u %lu", msgs, bytes);
				continue;
			}
			if (!stricmp(buf, "RSET")) {
				if ((i = smb_locksmbhdr(&smb)) != SMB_SUCCESS) {
					errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) locking message base"
					          , socket, client.protocol, user.alias, i, smb.last_error);
					sockprintf(socket, client.protocol, session, "-ERR %d locking message base", i);
					continue;
				}
				for (l = 0; l < msgs && !smb_islocked(&smb); l++) {
					msg.hdr.number = mail[l].number;
					if ((i = smb_getmsgidx(&smb, &msg)) != SMB_SUCCESS) {
						errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) getting message index"
						          , socket, client.protocol, user.alias, i, smb.last_error);
						break;
					}
					if ((i = smb_lockmsghdr(&smb, &msg)) != SMB_SUCCESS) {
						lprintf(LOG_WARNING, "%04d %-5s <%s> !ERROR %d (%s) locking message header #%u"
						        , socket, client.protocol, user.alias, i, smb.last_error, msg.hdr.number);
						break;
					}
					if ((i = smb_getmsghdr(&smb, &msg)) != SMB_SUCCESS) {
						smb_unlockmsghdr(&smb, &msg);
						errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) line %u, msg #%u"
						          , socket, client.protocol, user.alias, i, smb.last_error, __LINE__, msg.idx.number);
						break;
					}
					msg.hdr.attr = mail[l].attr;
					if ((i = smb_putmsg(&smb, &msg)) != SMB_SUCCESS)
						errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) updating message index"
						          , socket, client.protocol, user.alias, i, smb.last_error);
					smb_unlockmsghdr(&smb, &msg);
					smb_freemsgmem(&msg);
				}
				smb_unlocksmbhdr(&smb);

				if (l < msgs)
					sockprintf(socket, client.protocol, session, "-ERR %d messages reset (ERROR: %d)", l, i);
				else
					sockprintf(socket, client.protocol, session, "+OK %u messages (%lu bytes)", msgs, bytes);
				continue;
			}
			if (!strnicmp(buf, "LIST", 4) || !strnicmp(buf, "UIDL", 4)) {
				p = buf + 4;
				SKIP_WHITESPACE(p);
				if (IS_DIGIT(*p)) {
					msgnum = strtoul(p, NULL, 10);
					if (msgnum < 1 || msgnum > msgs) {
						lprintf(LOG_NOTICE, "%04d %-5s <%s> !INVALID message #%" PRIu32
						        , socket, client.protocol, user.alias, msgnum);
						sockprintf(socket, client.protocol, session, "-ERR no such message");
						continue;
					}
					msg.hdr.number = mail[msgnum - 1].number;
					if ((i = smb_getmsgidx(&smb, &msg)) != SMB_SUCCESS) {
						errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) getting message index"
						          , socket, client.protocol, user.alias, i, smb.last_error);
						sockprintf(socket, client.protocol, session, "-ERR %d getting message index", i);
						break;
					}
					if (msg.idx.attr & MSG_DELETE) {
						lprintf(LOG_NOTICE, "%04d %-5s <%s> !ATTEMPT to list DELETED message"
						        , socket, client.protocol, user.alias);
						sockprintf(socket, client.protocol, session, "-ERR message deleted");
						continue;
					}
					if ((i = smb_lockmsghdr(&smb, &msg)) != SMB_SUCCESS) {
						lprintf(LOG_WARNING, "%04d %-5s <%s> !ERROR %d (%s) locking message header #%u"
						        , socket, client.protocol, user.alias, i, smb.last_error, msg.hdr.number);
						sockprintf(socket, client.protocol, session, "-ERR %d locking message header", i);
						continue;
					}
					i = smb_getmsghdr(&smb, &msg);
					smb_unlockmsghdr(&smb, &msg);
					if (i != 0) {
						smb_freemsgmem(&msg);
						errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) line %u, msg #%u"
						          , socket, client.protocol, user.alias, i, smb.last_error, __LINE__, msg.idx.number);
						sockprintf(socket, client.protocol, session, "-ERR %d getting message header", i);
						continue;
					}
					if (!strnicmp(buf, "LIST", 4)) {
						sockprintf(socket, client.protocol, session, "+OK %" PRIu32 " %u", msgnum, msg.hdr.length + smb_getmsgtxtlen(&msg));
					} else /* UIDL */
						sockprintf(socket, client.protocol, session, "+OK %" PRIu32 " %u", msgnum, msg.hdr.number);

					smb_freemsgmem(&msg);
					continue;
				}
				/* List ALL messages */
				sockprintf(socket, client.protocol, session, "+OK %u messages (%lu bytes)", msgs, bytes);
				for (l = 0; l < msgs && !smb_islocked(&smb); l++) {
					msg.hdr.number = mail[l].number;
					if ((i = smb_getmsgidx(&smb, &msg)) != SMB_SUCCESS) {
						errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) getting message index"
						          , socket, client.protocol, user.alias, i, smb.last_error);
						break;
					}
					if (msg.idx.attr & MSG_DELETE)
						continue;
					if ((i = smb_lockmsghdr(&smb, &msg)) != SMB_SUCCESS) {
						lprintf(LOG_WARNING, "%04d %-5s <%s> !ERROR %d (%s) locking message header #%u"
						        , socket, client.protocol, user.alias, i, smb.last_error, msg.hdr.number);
						break;
					}
					i = smb_getmsghdr(&smb, &msg);
					smb_unlockmsghdr(&smb, &msg);
					if (i != 0) {
						smb_freemsgmem(&msg);
						errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) line %u, msg #%u"
						          , socket, client.protocol, user.alias, i, smb.last_error, __LINE__, msg.idx.number);
						break;
					}
					if (!strnicmp(buf, "LIST", 4)) {
						i = sockprintf(socket, client.protocol, session, "%u %u", l + 1, msg.hdr.length + smb_getmsgtxtlen(&msg));
					} else /* UIDL */
						i = sockprintf(socket, client.protocol, session, "%u %u", l + 1, msg.hdr.number);

					smb_freemsgmem(&msg);
					if (i < 1)
						break;
				}
				sockprintf(socket, client.protocol, session, ".");
				continue;
			}
			activity = true;
			if (!strnicmp(buf, "RETR ", 5) || !strnicmp(buf, "TOP ", 4)) {

				lines = -1;
				p = buf + 4;
				SKIP_WHITESPACE(p);
				msgnum = strtoul(p, NULL, 10);

				if (!strnicmp(buf, "TOP ", 4)) {
					SKIP_DIGIT(p);
					SKIP_WHITESPACE(p);
					lines = atol(p);
				}
				if (msgnum < 1 || msgnum > msgs) {
					lprintf(LOG_NOTICE, "%04d %-5s <%s> !ATTEMPTED to retrieve an INVALID message #%" PRIu32
					        , socket, client.protocol, user.alias, msgnum);
					sockprintf(socket, client.protocol, session, "-ERR no such message");
					continue;
				}
				msg.hdr.number = mail[msgnum - 1].number;

				lprintf(LOG_INFO, "%04d %-5s <%s> retrieving message #%u with command: %s"
				        , socket, client.protocol, user.alias, msg.hdr.number, buf);

				if ((i = smb_getmsgidx(&smb, &msg)) != SMB_SUCCESS) {
					errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) getting message index"
					          , socket, client.protocol, user.alias, i, smb.last_error);
					sockprintf(socket, client.protocol, session, "-ERR %d getting message index", i);
					continue;
				}
				if (msg.idx.attr & MSG_DELETE) {
					lprintf(LOG_NOTICE, "%04d %-5s <%s> !ATTEMPT to retrieve DELETED message"
					        , socket, client.protocol, user.alias);
					sockprintf(socket, client.protocol, session, "-ERR message deleted");
					continue;
				}
				if ((i = smb_lockmsghdr(&smb, &msg)) != SMB_SUCCESS) {
					lprintf(LOG_WARNING, "%04d %-5s <%s> !ERROR %d (%s) locking message header #%u"
					        , socket, client.protocol, user.alias, i, smb.last_error, msg.hdr.number);
					sockprintf(socket, client.protocol, session, "-ERR %d locking message header", i);
					continue;
				}
				i = smb_getmsghdr(&smb, &msg);
				smb_unlockmsghdr(&smb, &msg);
				if (i != 0) {
					errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) line %u, msg #%u"
					          , socket, client.protocol, user.alias, i, smb.last_error, __LINE__, msg.idx.number);
					sockprintf(socket, client.protocol, session, "-ERR %d getting message header", i);
					continue;
				}

				if ((msgtxt = smb_getmsgtxt(&smb, &msg, GETMSGTXT_ALL)) == NULL) {
					smb_freemsgmem(&msg);
					errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR (%s) retrieving message #%u text"
					          , socket, client.protocol, user.alias, smb.last_error, msg.hdr.number);
					sockprintf(socket, client.protocol, session, "-ERR retrieving message text");
					continue;
				}

				remove_ctrl_a(msgtxt, msgtxt);

				if (lines > 0                    /* Works around BlackBerry mail server */
				    && lines >= (long)strlen(msgtxt)) /* which requests the number of bytes (instead of lines) using TOP */
					lines = -1;

				sockprintf(socket, client.protocol, session, "+OK message follows");
				lprintf(LOG_DEBUG, "%04d %-5s <%s> sending message text (%lu bytes) from %s"
				        , socket, client.protocol, user.alias, (ulong)strlen(msgtxt), msg.from);
				lines_sent = sockmsgtxt(socket, client.protocol, session, &msg, msgtxt, lines);
				/* if(startup->options&MAIL_OPT_DEBUG_POP3) */
				if (lines != -1 && lines_sent < (ulong)lines)   /* could send *more* lines */
					lprintf(LOG_WARNING, "%04d %-5s <%s> !ERROR sending message text (sent %ld of %ld lines) from %s"
					        , socket, client.protocol, user.alias, lines_sent, lines, msg.from);
				else {
					lprintf(LOG_DEBUG, "%04d %-5s <%s> message transfer complete (%lu lines) from %s"
					        , socket, client.protocol, user.alias, lines_sent, msg.from);

					if (!(startup->options & MAIL_OPT_NO_READ_POP3) && !smb_islocked(&smb)) {
						if ((i = smb_locksmbhdr(&smb)) != SMB_SUCCESS) {
							errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) locking message base"
							          , socket, client.protocol, user.alias, i, smb.last_error);
						} else {
							if ((i = smb_getmsgidx(&smb, &msg)) != SMB_SUCCESS) {
								errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) getting message index"
								          , socket, client.protocol, user.alias, i, smb.last_error);
							} else {
								msg.hdr.attr |= MSG_READ;

								if ((i = smb_lockmsghdr(&smb, &msg)) != SMB_SUCCESS)
									errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) locking message header #%u"
									          , socket, client.protocol, user.alias, i, smb.last_error, msg.hdr.number);
								if ((i = smb_putmsg(&smb, &msg)) != SMB_SUCCESS)
									errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) marking message #%u as read"
									          , socket, client.protocol, user.alias, i, smb.last_error, msg.hdr.number);
								smb_unlockmsghdr(&smb, &msg);
							}
							smb_unlocksmbhdr(&smb);
						}
					}
				}
				smb_freemsgmem(&msg);
				smb_freemsgtxt(msgtxt);
				continue;
			}
			if (!strnicmp(buf, "DELE ", 5)) {
				p = buf + 5;
				SKIP_WHITESPACE(p);
				msgnum = strtoul(p, NULL, 10);

				if (msgnum < 1 || msgnum > msgs) {
					lprintf(LOG_NOTICE, "%04d %-5s <%s> !ATTEMPTED to delete an INVALID message #%" PRIu32
					        , socket, client.protocol, user.alias, msgnum);
					sockprintf(socket, client.protocol, session, "-ERR no such message");
					continue;
				}
				msg.hdr.number = mail[msgnum - 1].number;

				lprintf(LOG_INFO, "%04d %-5s <%s> deleting message #%u"
				        , socket, client.protocol, user.alias, msg.hdr.number);

				if ((i = smb_locksmbhdr(&smb)) != SMB_SUCCESS) {
					errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) locking message base"
					          , socket, client.protocol, user.alias, i, smb.last_error);
					sockprintf(socket, client.protocol, session, "-ERR %d locking message base", i);
					continue;
				}
				if ((i = smb_getmsgidx(&smb, &msg)) != SMB_SUCCESS) {
					smb_unlocksmbhdr(&smb);
					errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) getting message index"
					          , socket, client.protocol, user.alias, i, smb.last_error);
					sockprintf(socket, client.protocol, session, "-ERR %d getting message index", i);
					continue;
				}
				if ((i = smb_lockmsghdr(&smb, &msg)) != SMB_SUCCESS) {
					smb_unlocksmbhdr(&smb);
					lprintf(LOG_WARNING, "%04d %-5s <%s> !ERROR %d (%s) locking message header #%u"
					        , socket, client.protocol, user.alias, i, smb.last_error, msg.hdr.number);
					sockprintf(socket, client.protocol, session, "-ERR %d locking message header", i);
					continue;
				}
				if ((i = smb_getmsghdr(&smb, &msg)) != SMB_SUCCESS) {
					smb_unlockmsghdr(&smb, &msg);
					smb_unlocksmbhdr(&smb);
					errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) line %u, msg #%u"
					          , socket, client.protocol, user.alias, i, smb.last_error, __LINE__, msg.idx.number);
					sockprintf(socket, client.protocol, session, "-ERR %d getting message header", i);
					continue;
				}
				msg.hdr.attr |= MSG_DELETE;

				if ((i = smb_putmsg(&smb, &msg)) == SMB_SUCCESS && msg.hdr.auxattr & MSG_FILEATTACH) {
					if (!delfattach(&scfg, &msg))
						errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d deleting attachment: %s"
						          , socket, client.protocol, user.alias, errno, msg.subj);
				}
				smb_unlockmsghdr(&smb, &msg);
				smb_unlocksmbhdr(&smb);
				smb_freemsgmem(&msg);
				if (i != SMB_SUCCESS) {
					errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d (%s) marking message for deletion"
					          , socket, client.protocol, user.alias, i, smb.last_error);
					sockprintf(socket, client.protocol, session, "-ERR %d marking message for deletion", i);
					continue;
				}
				sockprintf(socket, client.protocol, session, "+OK");
				if (startup->options & MAIL_OPT_DEBUG_POP3)
					lprintf(LOG_INFO, "%04d %-5s <%s> message deleted", socket, client.protocol, user.alias);
				continue;
			}
			lprintf(LOG_NOTICE, "%04d %-5s <%s> !UNSUPPORTED COMMAND: '%s'"
			        , socket, client.protocol, user.alias, buf);
			sockprintf(socket, client.protocol, session, "-ERR UNSUPPORTED COMMAND: %s", buf);
		}
		if (user.number) {
#ifdef _WIN32
			if (startup->sound.logout[0] && !sound_muted(&scfg))
				PlaySound(startup->sound.logout, NULL, SND_ASYNC | SND_FILENAME);
#endif
			if ((i = logoutuserdat(&scfg, &user, client.time)) != USER_SUCCESS)
				errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d in logoutuserdat", socket, client.protocol, user.alias, i);
			mqtt_user_logout(&mqtt, &client, client.time);
		}

	} while (0);

	if (activity) {
		if (user.number)
			lprintf(LOG_INFO, "%04d %-5s <%s> logged-out from port %u on %s [%s]"
			        , socket, client.protocol, user.alias, inet_addrport(&pop3->client_addr), host_name, host_ip);
		else
			lprintf(LOG_INFO, "%04d %-5s [%s] client disconnected from port %u on %s"
			        , socket, client.protocol, host_ip, inet_addrport(&pop3->client_addr), host_name);
	}

	/* Free up resources here */
	if (mail != NULL)
		freemail(mail);

	smb_freemsgmem(&msg);
	smb_close(&smb);

	listRemoveTaggedNode(&current_logins, socket, /* free_data */ true);
	client_off(socket);

	/* Must be last */
	{
		int32_t remain = thread_down();
		if (startup->options & MAIL_OPT_DEBUG_POP3)
			lprintf(LOG_DEBUG, "%04d %-5s [%s] Session thread terminated (%u threads remain, %lu clients served)"
			        , socket, client.protocol, host_ip, remain, ++stats.pop3_served);
	}
	return true;
}

// Wrapper for pop3_client_thread() that does some basic resource management
static void pop3_thread(void* arg)
{
	pop3_t pop3 = *(pop3_t*)arg;

	SetThreadName("sbbs/pop3");
	thread_up(true /* setuid */);

	free(arg);

	uint32_t client_count = protected_uint32_adjust_fetch(&active_clients, 1);
	if (client_count > client_highwater) {
		client_highwater = client_count;
		if (client_highwater > 1)
			lprintf(LOG_NOTICE, "%04d POP3 New active client highwater mark: %u"
			        , pop3.socket, client_highwater);
		mqtt_pub_uintval(&mqtt, TOPIC_SERVER, "highwater", mqtt.highwater = client_highwater);
	}
	update_clients();

	if (!pop3_client_thread(&pop3))
		thread_down();

	(void)protected_uint32_adjust(&active_clients, -1);
	update_clients();

	mail_close_socket(&pop3.socket, &pop3.session);
}

static in_addr_t rblchk(SOCKET sock, const char* prot, union xp_sockaddr *addr, const char* rbl_addr)
{
	char           name[256];
	in_addr_t      mail_addr;
	in_addr_t      result = 0;
	unsigned char *addr6;

	switch (addr->addr.sa_family) {
		case AF_INET:
			mail_addr = ntohl(addr->in.sin_addr.s_addr);
			safe_snprintf(name, sizeof(name), "%lu.%lu.%lu.%lu.%.128s"
			              , (ulong)(mail_addr & 0xff)
			              , (ulong)(mail_addr >> 8) & 0xff
			              , (ulong)(mail_addr >> 16) & 0xff
			              , (ulong)(mail_addr >> 24) & 0xff
			              , rbl_addr
			              );
			break;
		case AF_INET6:
			addr6 = (unsigned char *)&addr->in6.sin6_addr;
			safe_snprintf(name, sizeof(name), "%1x.%1x.%1x.%1x.%1x.%1x.%1x.%1x."
			              "%1x.%1x.%1x.%1x.%1x.%1x.%1x.%1x."
			              "%1x.%1x.%1x.%1x.%1x.%1x.%1x.%1x."
			              "%1x.%1x.%1x.%1x.%1x.%1x.%1x.%1x.%.128s"
			              , addr6[15] & 0x0f
			              , addr6[15] >> 4
			              , addr6[14] & 0x0f
			              , addr6[14] >> 4
			              , addr6[13] & 0x0f
			              , addr6[13] >> 4
			              , addr6[12] & 0x0f
			              , addr6[12] >> 4
			              , addr6[11] & 0x0f
			              , addr6[11] >> 4
			              , addr6[10] & 0x0f
			              , addr6[10] >> 4
			              , addr6[9] & 0x0f
			              , addr6[9] >> 4
			              , addr6[8] & 0x0f
			              , addr6[8] >> 4
			              , addr6[7] & 0x0f
			              , addr6[7] >> 4
			              , addr6[6] & 0x0f
			              , addr6[6] >> 4
			              , addr6[5] & 0x0f
			              , addr6[5] >> 4
			              , addr6[4] & 0x0f
			              , addr6[4] >> 4
			              , addr6[3] & 0x0f
			              , addr6[3] >> 4
			              , addr6[2] & 0x0f
			              , addr6[2] >> 4
			              , addr6[1] & 0x0f
			              , addr6[1] >> 4
			              , addr6[0] & 0x0f
			              , addr6[0] >> 4
			              , rbl_addr
			              );
			break;
		default:
			return 0;
	}
	lprintf(LOG_DEBUG, "%04d %-5s DNSBL Query: %s", sock, prot, name);

	struct addrinfo* res;
	struct addrinfo hints {};
	hints.ai_family = AF_INET;
	if (getaddrinfo(name, NULL, &hints, &res) != 0)
		return 0;

	if (res->ai_family == AF_INET) {
		char addr[128];
		inet_ntop(AF_INET, &((struct sockaddr_in*)res->ai_addr)->sin_addr, addr, sizeof addr);
		if (strncmp(addr, "127.0.0.", 8) == 0)
			result = ((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
		lprintf(result ? LOG_INFO : LOG_ERR, "%04d %-5s DNSBL Query: %s resolved to: %s"
		    , sock, prot, name, addr);
	}
	freeaddrinfo(res);
	return result;
}

static ulong dns_blacklisted(SOCKET sock, const char* prot, union xp_sockaddr *addr, char* host_name, char* list, char* dnsbl_ip)
{
	char  fname[MAX_PATH + 1];
	char  str[256];
	char* p;
	char* tp;
	FILE* fp;
	ulong found = 0;
	char  ip[INET6_ADDRSTRLEN];

	SAFEPRINTF(fname, "%sdnsbl_exempt.cfg", scfg.ctrl_dir);
	inet_addrtop(addr, ip, sizeof(ip));
	if (find2strs(ip, host_name, fname, NULL))
		return false;

	SAFEPRINTF(fname, "%sdns_blacklist.cfg", scfg.ctrl_dir);
	if ((fp = fopen(fname, "r")) == NULL)
		return false;

	while (!feof(fp) && !found) {
		if (fgets(str, sizeof(str), fp) == NULL)
			break;
		truncsp(str);

		p = str;
		SKIP_WHITESPACE(p);
		if (*p == ';' || *p == 0) /* comment or blank line */
			continue;

		sprintf(list, "%.100s", p);

		/* terminate */
		tp = p;
		FIND_WHITESPACE(tp);
		*tp = 0;

		found = rblchk(sock, prot, addr, p);
	}
	fclose(fp);
	if (found)
		strcpy(dnsbl_ip, ip);

	return found;
}

static void parse_mail_address(const char* p
                               , char* name, size_t name_len
                               , char* addr, size_t addr_len)
{
	char* tp;
	char  tmp[256];

	if (p == NULL || addr == NULL)
		return;

	SKIP_WHITESPACE(p);

	/* Get the address */
	if ((tp = (char*)strchr(p, '<')) != NULL)
		tp++;
	else
		tp = (char*)p;
	SKIP_WHITESPACE(tp);
	sprintf(addr, "%.*s", (int)addr_len, tp);
	truncstr(addr, ">( ");

	if (name != NULL) {
		SAFECOPY(tmp, p);
		p = tmp;
		/* Get the "name" (if possible) */
		if ((tp = (char*)strchr(p, '"')) != NULL) {  /* name in quotes? */
			p = tp + 1;
			tp = (char*)strchr(p, '"');
		} else if ((tp = (char*)strchr(p, '(')) != NULL) {   /* name in parenthesis? */
			p = tp + 1;
			tp = (char*)strchr(p, ')');
		} else if (*p == '<') {                    /* address in brackets? */
			p++;
			tp = (char*)strchr(p, '>');
		} else                                  /* name, then address in brackets */
			tp = (char*)strchr(p, '<');
		if (tp)
			*tp = 0;
		sprintf(name, "%.*s", (int)name_len, p);
		truncsp(name);
		strip_char(name, name, '\\');
	}
}

static bool chk_email_addr(SOCKET socket, const char* prot, const char* p, const char* host_name, const char* host_ip
                           , const char* to, const char* from, const char* source)
{
	char         name[128];
	char         addr[128];
	char         tmp[128];

	parse_mail_address(p, name, sizeof name - 1, addr, sizeof addr - 1);

	struct trash trash;
	if (!trashcan2(&scfg, name, addr, "email", &trash))
		return true;
	if (!trash.quiet) {
		char         details[128];
		lprintf(LOG_NOTICE, "%04d %-5s [%s] !BLOCKED %s e-mail address: %s %s"
				, socket, prot, host_ip, source, p, trash_details(&trash, details, sizeof details));
		SAFEPRINTF2(tmp, "Blocked %s e-mail address: %s", source, p);
		spamlog(&scfg, &mqtt, prot, "REFUSED", tmp, host_name, host_ip, to, from);
	}
	return false;
}

static bool email_addr_is_exempt(const char* addr)
{
	char  fname[MAX_PATH + 1];
	char  netmail[128];
	char* p;

	if (*addr == 0 || strcmp(addr, "<>") == 0)
		return false;
	angle_bracket(netmail, sizeof(netmail), addr);
	SAFEPRINTF(fname, "%sdnsbl_exempt.cfg", scfg.ctrl_dir);
	if (findstr(netmail, fname))
		return true;
	p = netmail + 1;
	*lastchar(p) = '\0';
	return finduserstr(&scfg, 0, USER_NETMAIL, p, /* del */ false, /* next */ false, NULL, NULL);
}

static void exempt_email_addr(const char* comment
                              , const char* sender_info
                              , const char* toaddr)
{
	char  fname[MAX_PATH + 1];
	char  to[256];
	char  tmp[128];
	FILE* fp;

	angle_bracket(to, sizeof(to), toaddr);
	if (!email_addr_is_exempt(to)) {
		SAFEPRINTF(fname, "%sdnsbl_exempt.cfg", scfg.ctrl_dir);
		if ((fp = fnopen(NULL, fname, O_CREAT | O_WRONLY | O_APPEND)) == NULL)
			errprintf(LOG_ERR, WHERE, "0000 !Error %d opening file: %s", errno, fname);
		else {
			lprintf(LOG_INFO, "0000 %s: %s", comment, to);
			fprintf(fp, "\n;%s from %s on %s\n%s\n"
			        , comment, sender_info
			        , timestr(&scfg, time32(NULL), tmp), to);
			fclose(fp);
		}
	}
}

static void signal_smtp_sem(void)
{
	int file;

	if (scfg.smtpmail_sem[0] == 0)
		return; /* do nothing */

	if ((file = open(scfg.smtpmail_sem, O_WRONLY | O_CREAT | O_TRUNC, DEFFILEMODE)) != -1)
		close(file);
}

/*****************************************************************************/
/* Returns command line generated from instr with %c replacements            */
/*****************************************************************************/
static char* mailcmdstr(char* instr, char* msgpath, char* newpath, char* logpath
                        , char* lstpath, char* errpath
                        , char* host, char* ip, int usernum
                        , char* rcpt_addr
                        , char* sender, char* sender_addr, char* reverse_path, char* cmd)
{
	char str[1024];
	int  i, j, len;

	len = strlen(instr);
	for (i = j = 0; i < len; i++) {
		if (instr[i] == '%') {
			i++;
			cmd[j] = 0;
			switch (toupper(instr[i])) {
				case 'D':
					strcat(cmd, logpath);
					break;
				case 'E':
					strcat(cmd, errpath);
					break;
				case 'H':
					strcat(cmd, host);
					break;
				case 'I':
					strcat(cmd, ip);
					break;
				case 'G':   /* Temp directory */
					strcat(cmd, scfg.temp_dir);
					break;
				case 'J':
					strcat(cmd, scfg.data_dir);
					break;
				case 'K':
					strcat(cmd, scfg.ctrl_dir);
					break;
				case 'L':
					strcat(cmd, lstpath);
					break;
				case 'F':
				case 'M':
					strcat(cmd, msgpath);
					break;
				case 'N':
					strcat(cmd, newpath);
					break;
				case 'O':   /* SysOp */
					strcat(cmd, scfg.sys_op);
					break;
				case 'Q':   /* QWK ID */
					strcat(cmd, scfg.sys_id);
					break;
				case 'R':   /* reverse path */
					strcat(cmd, reverse_path);
					break;
				case 'S':   /* sender name */
					strcat(cmd, sender);
					break;
				case 'T':   /* recipient */
					strcat(cmd, rcpt_addr);
					break;
				case 'A':   /* sender address */
					strcat(cmd, sender_addr);
					break;
				case 'V':   /* Synchronet Version */
					SAFEPRINTF2(str, "%s%c", VERSION, REVISION);
					strcat(cmd, str);
					break;
				case 'Z':
					strcat(cmd, scfg.text_dir);
					break;
				case '!':   /* EXEC Directory */
					strcat(cmd, scfg.exec_dir);
					break;
				case '@':   /* EXEC Directory for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
					strcat(cmd, scfg.exec_dir);
#endif
					break;
				case '%':   /* %% for percent sign */
					strcat(cmd, "%");
					break;
				case '?':   /* Platform */
#ifdef __OS2__
					SAFECOPY(str, "OS2");
#else
					SAFECOPY(str, PLATFORM_DESC);
#endif
					strlwr(str);
					strcat(cmd, str);
					break;
				case 'U':   /* User number */
					SAFEPRINTF(str, "%d", usernum);
					strcat(cmd, str);
					break;
				default:    /* unknown specification */
					break;
			}
			j = strlen(cmd);
		}
		else
			cmd[j++] = instr[i];
	}
	cmd[j] = 0;

	return cmd;
}
#ifdef JAVASCRIPT

struct private_t {
	SOCKET sock;
	const char* log_prefix;
	const char* proc_name;
};

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	char       line[64];
	char       file[MAX_PATH + 1];
	const char* warning;
	private_t* p;
	jsrefcount rc;
	int        log_level;

	if ((p = (private_t*)JS_GetContextPrivate(cx)) == NULL)
		return;

	if (report == NULL) {
		lprintf(LOG_ERR, "%04d %-5s %s !JavaScript: %s"
		        , p->sock, p->log_prefix, p->proc_name, message);
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
	} else {
		log_level = LOG_ERR;
		warning = "";
	}

	rc = JS_SUSPENDREQUEST(cx);
	lprintf(log_level, "%04d %-5s %s !JavaScript %s%s%s: %s"
	        , p->sock, p->log_prefix, p->proc_name
	        , warning, file, line, message);
	JS_RESUMEREQUEST(cx, rc);
}

static JSBool
js_log(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	uintN      i = 0;
	int32      level = LOG_INFO;
	private_t* p;
	jsrefcount rc;
	char *     lstr = NULL;
	size_t     lstr_sz = 0;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((p = (private_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	if (JSVAL_IS_NUMBER(argv[i])) {
		if (!JS_ValueToInt32(cx, argv[i++], &level))
			return JS_FALSE;
	}

	for (; i < argc; i++) {
		JSVALUE_TO_RASTRING(cx, argv[i], lstr, &lstr_sz, NULL);
		HANDLE_PENDING(cx, lstr);
		if (lstr == NULL)
			return JS_TRUE;
		rc = JS_SUSPENDREQUEST(cx);
		lprintf(level, "%04d %-5s %s %s"
		        , p->sock, p->log_prefix, p->proc_name, lstr);
		JS_SET_RVAL(cx, arglist, argv[i]);
		JS_RESUMEREQUEST(cx, rc);
	}

	if (lstr)
		free(lstr);

	return JS_TRUE;
}

static JSBool
js_alert(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	private_t* p;
	jsrefcount rc;
	char *     line;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((p = (private_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], line, NULL);
	if (line == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	lprintf(LOG_ERR, "%04d %-5s %s %s"
	        , p->sock, p->log_prefix, p->proc_name, line);
	free(line);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}


static JSFunctionSpec js_global_functions[] = {
	{"write",           js_log,             0},
	{"writeln",         js_log,             0},
	{"print",           js_log,             0},
	{"log",             js_log,             0},
	{"alert",           js_alert,           1},
	{0}
};

static bool
js_mailproc(SOCKET sock, client_t* client, user_t* user, struct mailproc* mailproc
            , char* cmdline
            , char* msgtxt_fname, char* newtxt_fname, char* logtxt_fname
            , char* rcpt_addr
            , char* rcptlst_fname, char* proc_err_fname
            , char* sender, char* sender_addr, char* reverse_path, char* hello_name
            , int32* result
            , JSRuntime**    js_runtime
            , JSContext**    js_cx
            , JSObject**     js_glob
            , const char*    log_prefix
            )
{
	char*         p;
	char          fname[MAX_PATH + 1];
	char          path[MAX_PATH + 1];
	char          arg[MAX_PATH + 1];
	bool          success = false;
	JSObject*     js_scope = NULL;
	JSObject*     argv;
	jsuint        argc;
	JSObject*     js_script;
	js_callback_t js_callback;
	jsval         val;
	jsval         rval = JSVAL_VOID;
	private_t     priv;

	ZERO_VAR(js_callback);

	SAFECOPY(fname, cmdline);
	truncstr(fname, " \t");
	if (getfext(fname) == NULL) /* No extension specified, assume '.js' */
		strcat(fname, ".js");

	SAFECOPY(path, fname);
	if (getfname(path) == path) { /* No path specified, assume mods or exec dir */
		SAFEPRINTF2(path, "%s%s", scfg.mods_dir, fname);
		if (scfg.mods_dir[0] == 0 || !fexist(path))
			SAFEPRINTF2(path, "%s%s", scfg.exec_dir, fname);
	}

	*result = 0;
	do {
		if (*js_runtime == NULL) {
			lprintf(LOG_DEBUG, "%04d %-5s JavaScript: Creating runtime: %u bytes"
			        , sock, log_prefix, startup->js.max_bytes);

			if ((*js_runtime = jsrt_GetNew(startup->js.max_bytes, 1000, __FILE__, __LINE__)) == NULL)
				return false;
		}

		if (*js_cx == NULL) {
			if ((*js_cx = JS_NewContext(*js_runtime, JAVASCRIPT_CONTEXT_STACK)) == NULL)
				return false;
			JS_SetOptions(*js_cx, startup->js.options);
		}
		JS_BEGINREQUEST(*js_cx);

		JS_SetErrorReporter(*js_cx, js_ErrorReporter);

		priv.sock = sock;
		priv.log_prefix = log_prefix;
		priv.proc_name = mailproc->name;
		JS_SetContextPrivate(*js_cx, &priv);

		if (*js_glob == NULL) {
			/* Global Objects (including system, js, client, Socket, MsgBase, File, User, etc. */
			if (!js_CreateCommonObjects(*js_cx, &scfg, &scfg, NULL
			                            , uptime, server_host_name(), SOCKLIB_DESC /* system */
			                            , &js_callback              /* js */
			                            , &startup->js
				                        , NULL                      /* user */
			                            , client, sock, -1          /* client */
			                            , &js_server_props          /* server */
			                            , js_glob
			                            , &mqtt
			                            ))
				break;

			if (!JS_DefineFunctions(*js_cx, *js_glob, js_global_functions))
				break;

			/* Area and "user" Objects */
			if (!js_CreateUserObjects(*js_cx, *js_glob, &scfg, user, client, NULL, NULL, &mqtt))
				break;

			/* Mailproc "API" filenames */
			JS_DefineProperty(*js_cx, *js_glob, "message_text_filename"
			                  , STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx, msgtxt_fname))
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "new_message_text_filename"
			                  , STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx, newtxt_fname))
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "log_text_filename"
			                  , STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx, logtxt_fname))
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "recipient_address"
			                  , STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx, rcpt_addr))
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "recipient_list_filename"
			                  , STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx, rcptlst_fname))
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "processing_error_filename"
			                  , STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx, proc_err_fname))
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "sender_name"
			                  , STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx, sender))
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "sender_address"
			                  , STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx, sender_addr))
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "reverse_path"
			                  , STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx, reverse_path))
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

			JS_DefineProperty(*js_cx, *js_glob, "hello_name"
			                  , STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx, hello_name))
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

		}

		if ((js_scope = JS_NewObject(*js_cx, NULL, NULL, *js_glob)) == NULL)
			break;

		/* Convert command-line to argv/argc */
		argv = JS_NewArrayObject(*js_cx, 0, NULL);
		JS_DefineProperty(*js_cx, js_scope, "argv", OBJECT_TO_JSVAL(argv)
		                  , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);

		p = cmdline;
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		for (argc = 0; p != NULL && *p != '\0'; argc++) {
			SAFECOPY(arg, p);
			truncstr(arg, " \t");
			val = STRING_TO_JSVAL(JS_NewStringCopyZ(*js_cx, arg));
			if (!JS_SetElement(*js_cx, argv, argc, &val))
				break;
			FIND_WHITESPACE(p);
			SKIP_WHITESPACE(p);
		}
		JS_DefineProperty(*js_cx, js_scope, "argc", INT_TO_JSVAL(argc)
		                  , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);

		if (*mailproc->eval != 0) {
			lprintf(LOG_DEBUG, "%04d %-5s Evaluating: %s"
			        , sock, log_prefix, mailproc->eval);
			js_script = JS_CompileScript(*js_cx, js_scope, mailproc->eval, strlen(mailproc->eval), NULL, 1);
		} else {
			lprintf(LOG_DEBUG, "%04d %-5s Executing: %s"
			        , sock, log_prefix, cmdline);
			if ((js_script = JS_CompileFile(*js_cx, js_scope, path)) != NULL)
				js_PrepareToExecute(*js_cx, js_scope, path, /* startup_dir: */ NULL, js_scope);
		}
		if (js_script == NULL)
			break;

		/* ToDo: Set operational callback */
		success = JS_ExecuteScript(*js_cx, js_scope, js_script, &rval);

		if (JS_GetProperty(*js_cx, js_scope, "exit_code", &rval)
		    && rval != JSVAL_VOID && JSVAL_IS_NUMBER(rval))
			JS_ValueToInt32(*js_cx, rval, result);

		js_EvalOnExit(*js_cx, js_scope, &js_callback);

		JS_ReportPendingException(*js_cx);

		JS_ClearScope(*js_cx, js_scope);

		JS_GC(*js_cx);

	} while (0);

	JS_ENDREQUEST(*js_cx);

	return success;
}

void js_cleanup(JSRuntime* js_runtime, JSContext* js_cx, JSObject** js_glob)
{
	if (js_cx != NULL) {
		JS_BEGINREQUEST(js_cx);
		JS_RemoveObjectRoot(js_cx, js_glob);
		JS_ENDREQUEST(js_cx);
		JS_DestroyContext(js_cx);
	}
	if (js_runtime != NULL)
		jsrt_Release(js_runtime);
}
#endif

static size_t strStartsWith_i(const char* buf, const char* match)
{
	size_t len = strlen(match);
	if (strnicmp(buf, match, len) == 0)
		return len;
	return 0;
}

/* Decode RFC2047 'Q' encoded-word (in-place), "similar to" Quoted-printable */
char* mimehdr_q_decode(char* buf)
{
	uchar* p = (uchar*)buf;
	uchar* dest = p;

	for (; *p != 0; p++) {
		if (*p == '=') {
			p++;
			if (IS_HEXDIGIT(*p) && IS_HEXDIGIT(*(p + 1))) {
				uchar ch = HEX_CHAR_TO_INT(*p) << 4;
				p++;
				ch |= HEX_CHAR_TO_INT(*p);
				if (ch >= ' ')
					*dest++ = ch;
			} else {    /* bad encoding */
				*dest++ = '=';
				*dest++ = *p;
			}
		}
		else if (*p == '_')
			*dest++ = ' ';
		else if (*p >= '!' && *p <= '~')
			*dest++ = *p;
	}
	*dest = 0;
	return buf;
}

enum mimehdr_charset {
	MIMEHDR_CHARSET_ASCII,
	MIMEHDR_CHARSET_UTF8,
	MIMEHDR_CHARSET_CP437,
	MIMEHDR_CHARSET_OTHER
};

static enum mimehdr_charset mimehdr_charset_decode(const char* str)
{
	if (strStartsWith_i(str, "ascii?") || strStartsWith_i(str, "us-ascii?"))
		return MIMEHDR_CHARSET_ASCII;
	if (strStartsWith_i(str, "utf-8?"))
		return MIMEHDR_CHARSET_UTF8;
	if (strStartsWith_i(str, "cp437?") || strStartsWith_i(str, "ibm437?"))
		return MIMEHDR_CHARSET_CP437;
	return MIMEHDR_CHARSET_OTHER;
}

// Replace MIME (RFC 2047) "encoded-words" with their decoded-values
// Returns true if the value was MIME-encoded
bool mimehdr_value_decode(char* str, smbmsg_t* msg)
{
	bool  encoded = false;
	bool  encoded_word = false;
	if (str == NULL)
		return false;
	char* buf = strdup(str);
	if (buf == NULL)
		return false;
	char* state = NULL;
	*str = 0;
	char  tmp[256]; // "An 'encoded-word' may not be more than 75 characters long"
	for (char* p = strtok_r(buf, " \t", &state); p != NULL; p = strtok_r(NULL, " \t", &state)) {
		char* end = lastchar(p);
		if (*p == '=' && *(p + 1) == '?' && *(end - 1) == '?' && *end == '=' && end - p < (int)sizeof(tmp)) {
			if (*str && !encoded_word)
				strcat(str, " ");
			encoded = true;
			encoded_word = true;
			char*                cp = p + 2;
			enum mimehdr_charset charset = mimehdr_charset_decode(cp);
			FIND_CHAR(cp, '?');
			if (*cp == '?' && *(cp + 1) != 0 && *(cp + 2) == '?') {
				cp++;
				char  encoding = toupper(*cp);
				cp += 2;
				SAFECOPY(tmp, cp);
				char* tp = lastchar(tmp);
				*(tp - 1) = 0;  // remove the terminating "?="
				if (encoding == 'Q') {
					mimehdr_q_decode(tmp);
					if (charset == MIMEHDR_CHARSET_UTF8 && !str_is_ascii(tmp) && utf8_str_is_valid(tmp))
						msg->hdr.auxattr |= MSG_HFIELDS_UTF8;
					// TODO consider converting other 8-bit charsets (e.g. CP437, ISO-8859-1) to UTF-8 here
					p = tmp;
				}
				else if (encoding == 'B'
				         && b64_decode(tmp, sizeof(tmp), tmp, strlen(tmp)) > 0) { // base64
					strip_ctrl(tmp, tmp);
					if (charset == MIMEHDR_CHARSET_UTF8 && !str_is_ascii(tmp) && utf8_str_is_valid(tmp))
						msg->hdr.auxattr |= MSG_HFIELDS_UTF8;
					p = tmp;
				}
			}
		} else {
			if (*str)
				strcat(str, " ");
			encoded_word = false;
		}
		strcat(str, p);
	}
	free(buf);
	return encoded;
}

static char* get_header_field(char* buf, char* name, size_t maxlen)
{
	char*  p;
	size_t len;

	if (buf[0] <= ' ') /* folded header */
		return NULL;

	if ((p = strchr(buf, ':')) == NULL)
		return NULL;

	len = p - buf;
	if (len >= maxlen)
		len = maxlen - 1;
	sprintf(name, "%.*s", (int)len, buf);
	truncsp(name);

	p++;    /* skip colon */
	SKIP_WHITESPACE(p);
	return p;
}

static int parse_header_field(char* buf, smbmsg_t* msg, ushort* type)
{
	char*  p;
	char*  tp;
	char   field[128];
	int    len;
	ushort nettype;

	if (buf[0] <= ' ' && *type != UNKNOWN) { /* folded header, append to previous */
		p = buf;
		truncsp(p);
		if (*type == RFC822HEADER || *type == SMTPRECEIVED)
			smb_hfield_append_str(msg, *type, "\r\n");
		else { /* Unfold other common header field types (e.g. Subject, From, To) */
			smb_hfield_append_str(msg, *type, " ");
			SKIP_WHITESPACE(p);
		}
		return smb_hfield_append_str(msg, *type, p);
	}

	if ((p = strchr(buf, ':')) == NULL)
		return smb_hfield_str(msg, *type = RFC822HEADER, buf);

	len = (ulong)p - (ulong)buf;
	if (len > (int)sizeof(field) - 1)
		len = sizeof(field) - 1;
	sprintf(field, "%.*s", len, buf);
	truncsp(field);

	p++;    /* skip colon */
	SKIP_WHITESPACE(p);
	truncsp(p);

	if (!stricmp(field, "TO"))
		return smb_hfield_str(msg, *type = RFC822TO, p);

	if (!stricmp(field, "REPLY-TO")) {
		smb_hfield_str(msg, *type = RFC822REPLYTO, p);
		if ((tp = strrchr(p, '<')) != NULL)  {
			tp++;
			truncstr(tp, ">");
			p = tp;
		}
		nettype = NET_INTERNET;
		smb_hfield(msg, REPLYTONETTYPE, sizeof(nettype), &nettype);
		return smb_hfield_str(msg, *type = REPLYTONETADDR, p);
	}
	if (!stricmp(field, "FROM"))
		return smb_hfield_str(msg, *type = RFC822FROM, p);

	if (!stricmp(field, "ORGANIZATION"))
		return smb_hfield_str(msg, *type = RFC822ORG, p);

	if (!stricmp(field, "DATE")) {
		msg->hdr.when_written = rfc822date(p);
		*type = UNKNOWN;
		return SMB_SUCCESS;
	}
	if (!stricmp(field, "MESSAGE-ID"))
		return smb_hfield_str(msg, *type = RFC822MSGID, p);

	if (!stricmp(field, "IN-REPLY-TO"))
		return smb_hfield_str(msg, *type = RFC822REPLYID, p);

	if (!stricmp(field, "CC"))
		return smb_hfield_str(msg, *type = RFC822CC, p);

	if (!stricmp(field, "RECEIVED"))
		return smb_hfield_str(msg, *type = SMTPRECEIVED, p);

	if (!stricmp(field, "RETURN-PATH")) {
		*type = UNKNOWN;
		return SMB_SUCCESS; /* Ignore existing "Return-Path" header fields */
	}

	if (!stricmp(field, "X-PRIORITY")) {
		msg->hdr.priority = atoi(p);
		if (msg->hdr.priority > SMB_PRIORITY_LOWEST)
			msg->hdr.priority = SMB_PRIORITY_UNSPECIFIED;
		*type = UNKNOWN;
		return SMB_SUCCESS;
	}

	/* Fall-through */
	return smb_hfield_str(msg, *type = RFC822HEADER, buf);
}

static int chk_received_hdr(SOCKET socket, const char* prot, const char *buf, IN_ADDR *dnsbl_result, char *dnsbl, char *dnsbl_ip)
{
	char              host_name[128];
	char              tmp[128];
	char *            fromstr;
	char              ip[16] = "ipv6-addr";
	char *            p;
	char *            p2;
	char *            last;
	union xp_sockaddr addr;
	struct addrinfo   ai, *res;

	fromstr = strdup(buf);
	if (fromstr == NULL)
		return 0;
	strlwr(fromstr);
	do {
		p = strstr(fromstr, "from ");
		if (p == NULL)
			break;
		p += 4;
		SKIP_WHITESPACE(p);
		if (*p == 0)
			break;
		p2 = host_name;
		for (; *p && !IS_WHITESPACE(*p) && p2 < host_name + 126; p++)  {
			*p2++ = *p;
		}
		*p2 = 0;
		p = strtok_r(fromstr, "[", &last);
		if (p == NULL)
			break;
		p = strtok_r(NULL, "]", &last);
		if (p == NULL)
			break;
		if (strnicmp("IPv6:", p, 5) == 0) {
			p += 5;
			SKIP_WHITESPACE(p);
			memset(&ai, 0, sizeof(ai));
			ai.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_PASSIVE;
			if (getaddrinfo(p, NULL, &ai, &res) != 0)
				break;
			if (res->ai_family == AF_INET6) {
				memcpy(&addr, res->ai_addr, res->ai_addrlen);
				freeaddrinfo(res);
			} else {
				freeaddrinfo(res);
				break;
			}
		}
		else {
			strncpy(ip, p, 16);
			ip[15] = 0;
			addr.in.sin_family = AF_INET;
			addr.in.sin_addr.s_addr = parseIPv4Address(ip);
			lprintf(LOG_DEBUG, "%04d %-5s DNSBL checking received header address %s [%s]", socket, prot, host_name, ip);
		}

		if ((dnsbl_result->s_addr = dns_blacklisted(socket, prot, &addr, host_name, dnsbl, dnsbl_ip)) != 0)
			lprintf(LOG_NOTICE, "%04d %-5s [%s] BLACKLISTED SERVER on %s: %s = %s"
			        , socket, prot, ip, dnsbl, host_name, inet_ntop(AF_INET, dnsbl_result, tmp, sizeof tmp));
	} while (0);
	free(fromstr);
	return dnsbl_result->s_addr;
}

static bool checktag(scfg_t *scfg, char *tag, int usernum)
{
	char fname[MAX_PATH + 1];

	if (tag == NULL)
		return false;
	SAFEPRINTF2(fname, "%suser/%04d.smtpblock", scfg->data_dir, usernum);
	return findstr(tag, fname);
}

static bool smtp_splittag(char *in, char **name, char **tag)
{
	char *last;

	if (in == NULL)
		return false;

	*name = strtok_r(in, "#", &last);
	if (*name) {
		*tag = strtok_r(NULL, "", &last);
		return true;
	}
	return false;
}

static int smtp_matchuser(scfg_t *scfg, char *str, bool aliases, bool datdupe)
{
	char *user = strdup(str);
	char *name;
	char *tag = NULL;
	int  usernum = 0;

	if (!user)
		return 0;

	if (!smtp_splittag(user, &name, &tag))
		goto end;

	if (datdupe)
		usernum = finduserstr(scfg, 0, USER_NAME, name, /* del */ false, /* next */ false, NULL, NULL);
	else
		usernum = matchuser(scfg, name, aliases);

	if (!usernum)
		goto end;

	if (checktag(scfg, tag, usernum))
		usernum = INT_MAX;

end:
	free(user);
	return usernum;
}

// Save a verbatim-copy of the SMTP-received mail message to a file in .eml format
bool archive_mail(const char* fname, int usernumber, const char* subdir, const char* session_id)
{
	char tmp[128];
	char path[MAX_PATH + 1];

	snprintf(path, sizeof path, "%suser/%04u/%s/", scfg.data_dir, usernumber, subdir);
	if (!isdir(path) && !mkpath(path))
		return false;
	SAFECAT(path, gmtime_to_isoDateTimeStr(time(NULL), tmp, sizeof tmp));
	SAFECAT(path, "-");
	SAFECAT(path, session_id);
	SAFECAT(path, ".eml");
	return CopyFile(fname, path, /* fail-if-exists: */true);
}

#define WITH_ESMTP  (1 << 0)
#define WITH_AUTH   (1 << 1)
#define WITH_TLS    (1 << 2)

const char *with_clauses[] = {
	"SMTP",         // No WITH_*
	"ESMTP",        // WITH_ESMTP
	"SMTP",         // WITH_AUTH
	"ESMTPA",       // WITH_ESMTP | WITH_AUTH
	"SMTP",         // WITH_TLS
	"ESMTPS",       // WITH_ESMTP | WITH_TLS
	"SMTP",         // WITH_TLS | WITH_AUTH
	"ESMTPSA"       // WITH_TLS | WITH_AUTH | WITH_ESMTP
};

static bool smtp_client_thread(smtp_t* smtp)
{
	int               i, j;
	int               rd;
	char              str[512];
	char              tmp[128];
	char              path[MAX_PATH + 1];
	char              value[INI_MAX_VALUE_LEN];
	str_list_t        sec_list;
	char*             section;
	char              buf[1024], *p, *tp, *cp;
	char              hdrfield[512];
	char              alias_buf[128];
	char              name_alias_buf[128];
	char              reverse_path[128];
	char              forward_path[128];
	char              date[64];
	char              qwkid[32];
	char              rcpt_to[128];
	char              rcpt_name[128];
	char              rcpt_addr[128];
	char              sender[128];
	char              sender_addr[128];
	char              hello_name[128];
	char              user_name[128];
	char              relay_list[MAX_PATH + 1];
	char              domain_list[MAX_PATH + 1];
	char              spam_bait[MAX_PATH + 1];
	bool              spam_bait_result = false;
	char              spam_block[MAX_PATH + 1];
	char              spam_block_exemptions[MAX_PATH + 1];
	bool              spam_block_exempt = false;
	char              host_name[128];
	char              host_ip[INET6_ADDRSTRLEN];
	char              server_ip[INET6_ADDRSTRLEN];
	char              dnsbl[256];
	char              dnsbl_ip[INET6_ADDRSTRLEN];
	char*             telegram_buf;
	char*             msgbuf;
	char              challenge[256];
	char              response[128];
	char              secret[64];
	char              md5_data[384];
	uchar             digest[MD5_DIGEST_SIZE];
	char              dest_host[128];
	const char*       errmsg;
	ushort            dest_port;
	socklen_t         addr_len;
	ushort            hfield_type;
	ushort            nettype;
	ushort            agent;
	int               usernum;
	ulong             lines = 0;
	ulong             hdr_lines = 0;
	ulong             hdr_len = 0;
	off_t             length;
	ulong             badcmds = 0;
	ulong             login_attempts;
	ulong             waiting;
	bool              esmtp = false;
	bool              telegram = false;
	bool              forward = false;
	bool              no_forward = false;
	bool              auth_login = false;
	bool              routed = false;
	bool              dnsbl_recvhdr;
	bool              msg_handled;
	int               subnum = INVALID_SUB;
	FILE*             msgtxt = NULL;
	char              msgtxt_fname[MAX_PATH + 1];
	char              newtxt_fname[MAX_PATH + 1];
	char              logtxt_fname[MAX_PATH + 1];
	FILE*             rcptlst;
	char              rcptlst_fname[MAX_PATH + 1];
	ushort            rcpt_count = 0;
	FILE*             proc_out;
	char              proc_err_fname[MAX_PATH + 1];
	char              session_id[MAX_PATH + 1];
	FILE*             spy = NULL;
	SOCKET            socket;
	int               smb_error;
	smb_t             smb;
	smb_t             spam;
	smbmsg_t          msg;
	smbmsg_t          newmsg;
	user_t            user;
	user_t            relay_user;
	node_t            node;
	client_t          client;
	char              client_id[128];
	union xp_sockaddr server_addr;
	IN_ADDR           dnsbl_result;
	int               mailproc_match;
	JSRuntime*        js_runtime = NULL;
	JSContext*        js_cx = NULL;
	JSObject*         js_glob = NULL;
	int32             js_result;
	struct mailproc*  mailproc;
	login_attempt_t   attempted;
	int               session = -1;
	bool              nodelay = true;
	ulong             nb = 0;
	unsigned          with_val;
	int               cstat;

	enum {
		SMTP_STATE_INITIAL
		, SMTP_STATE_HELO
		, SMTP_STATE_MAIL_FROM
		, SMTP_STATE_RCPT_TO
		, SMTP_STATE_DATA_HEADER
		, SMTP_STATE_DATA_BODY

	} state = SMTP_STATE_INITIAL;

	enum {
		SMTP_CMD_NONE
		, SMTP_CMD_MAIL
		, SMTP_CMD_SEND
		, SMTP_CMD_SOML
		, SMTP_CMD_SAML

	} cmd = SMTP_CMD_NONE;

	socket = smtp->socket;

	SAFECOPY(client.protocol, smtp->tls_port ? "SMTPS" : "SMTP");

	lprintf(LOG_DEBUG, "%04d %-5s Session thread started", socket, client.protocol);

#ifdef _WIN32
	if (startup->inbound_sound[0] && !sound_muted(&scfg))
		PlaySound(startup->inbound_sound, NULL, SND_ASYNC | SND_FILENAME);
#endif
	SAFEPRINTF(domain_list, "%sdomains.cfg", scfg.ctrl_dir);

	addr_len = sizeof(server_addr);

	if ((i = getsockname(socket, &server_addr.addr, &addr_len)) != 0) {
		errprintf(LOG_CRIT, WHERE, "%04d %-5s !ERROR %d (%d) getting address/port of socket"
		          , socket, client.protocol, i, SOCKET_ERRNO);
		return false;
	}

	inet_addrtop(&smtp->client_addr, host_ip, sizeof(host_ip));
	inet_addrtop(&server_addr, server_ip, sizeof(server_ip));

	lprintf(LOG_INFO, "%04d %-5s [%s] Connection accepted on %s port %u from port %u"
	        , socket, client.protocol, host_ip, server_ip, inet_addrport(&server_addr), inet_addrport(&smtp->client_addr));
	SAFEPRINTF(client_id, "[%s]", host_ip);

	SAFECOPY(host_name, STR_NO_HOSTNAME);
	if (!(startup->options & MAIL_OPT_NO_HOST_LOOKUP)) {
		getnameinfo(&smtp->client_addr.addr, smtp->client_addr_len, host_name, sizeof(host_name), NULL, 0, NI_NAMEREQD);
		lprintf(LOG_INFO, "%04d %-5s %s Hostname: %s", socket, client.protocol, client_id, host_name);
	}

	if (smtp->tls_port) {
		if (!do_cryptInit(lprintf)) {
			return false;
		}
		if ((cstat = cryptCreateSession(&session, CRYPT_UNUSED, CRYPT_SESSION_TLS_SERVER)) != CRYPT_OK) {
			GCESH(cstat, client.protocol, socket, host_ip, CRYPT_UNUSED, "creating session");
			return false;
		}
		if ((cstat = add_private_key(&scfg, lprintf, session)) != CRYPT_OK) {
			cryptDestroySession(session);
			GCESH(cstat, client.protocol, socket, host_ip, session, "setting private key");
			return false;
		}
		smtp->session = session;
		nodelay = true;
		setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
		nb = 0;
		ioctlsocket(socket, FIONBIO, &nb);
		if ((cstat = cryptSetAttribute(session, CRYPT_SESSINFO_NETWORKSOCKET, socket)) != CRYPT_OK) {
			GCESH(cstat, client.protocol, socket, host_ip, session, "setting network socket");
			return false;
		}
		if ((cstat = cryptSetAttribute(session, CRYPT_SESSINFO_ACTIVE, 1)) != CRYPT_OK) {
			GCESH(cstat, client.protocol, socket, host_ip, session, "setting session active");
			return false;
		}
		if (startup->max_inactivity) {
			if ((cstat = cryptSetAttribute(session, CRYPT_OPTION_NET_READTIMEOUT, startup->max_inactivity)) != CRYPT_OK) {
				GCESH(cstat, client.protocol, socket, host_ip, session, "setting read timeout");
				return false;
			}
		}
	}

	memset(&smb, 0, sizeof(smb));
	memset(&msg, 0, sizeof(msg));
	memset(&spam, 0, sizeof(spam));
	memset(&user, 0, sizeof(user));
	memset(&relay_user, 0, sizeof(relay_user));

	SAFECOPY(hello_name, host_name);

	SAFEPRINTF(spam_bait, "%sspambait.cfg", scfg.ctrl_dir);
	SAFEPRINTF(spam_block, "%sspamblock.cfg", scfg.ctrl_dir);
	SAFEPRINTF(spam_block_exemptions, "%sspamblock_exempt.cfg", scfg.ctrl_dir);

	if (strcmp(server_ip, host_ip) == 0) {
		/* local connection */
		dnsbl_result.s_addr = 0;
	} else {
		ulong banned = loginBanned(&scfg, startup->login_attempt_list, socket, host_name, startup->login_attempt, &attempted);
		if (banned) {
			char ban_duration[128];
			lprintf(LOG_NOTICE, "%04d %-5s [%s] !TEMPORARY BAN (%lu login attempts, last: %s) - remaining: %s"
			        , socket, client.protocol, host_ip, attempted.count - attempted.dupes, attempted.user
			        , duration_estimate_to_vstr(banned, ban_duration, sizeof ban_duration, 1, 1));
			return false;
		}

		spam_block_exempt = find2strs(host_ip, host_name, spam_block_exemptions, NULL);
		if (!spam_block_exempt && findstr(host_ip, spam_block)) {
			lprintf(LOG_NOTICE, "%04d %-5s [%s] !CLIENT BLOCKED in %s (%lu total)"
			        , socket, client.protocol, host_ip, spam_block, ++stats.sessions_refused);
			sockprintf(socket, client.protocol, session, "550 CLIENT IP ADDRESS BLOCKED: %s", host_ip);
			return false;
		}
		struct trash trash;
		if (ip_can->listed(host_ip, nullptr, &trash)) {
			++stats.sessions_refused;
			if (!trash.quiet) {
				char details[128];
				lprintf(LOG_NOTICE, "%04d %-5s [%s] !CLIENT BLOCKED in ip.can %s (%lu total)"
						, socket, client.protocol, host_ip, trash_details(&trash, details, sizeof details), stats.sessions_refused);
			}
			sockprintf(socket, client.protocol, session, "550 CLIENT IP ADDRESS BLOCKED: %s", host_ip);
			return false;
		}
		if (host_can->listed(host_name, nullptr, &trash)) {
			++stats.sessions_refused;
			if (!trash.quiet) {
				char details[128];
				lprintf(LOG_NOTICE, "%04d %-5s [%s] !CLIENT BLOCKED in host.can: %s %s (%lu total)"
						, socket, client.protocol, host_ip, host_name, trash_details(&trash, details, sizeof details), stats.sessions_refused);
			}
			sockprintf(socket, client.protocol, session, "550 CLIENT HOSTNAME BLOCKED: %s", host_name);
			return false;
		}

		/*  SPAM Filters (mail-abuse.org) */
		dnsbl_result.s_addr = dns_blacklisted(socket, client.protocol, &smtp->client_addr, host_name, dnsbl, dnsbl_ip);
		if (dnsbl_result.s_addr) {
			lprintf(LOG_NOTICE, "%04d %-5s [%s] BLACKLISTED SERVER on %s: %s = %s"
			        , socket, client.protocol, dnsbl_ip, dnsbl, host_name, inet_ntop(AF_INET, &dnsbl_result, tmp, sizeof tmp));
			if (startup->options & MAIL_OPT_DNSBL_REFUSE) {
				SAFEPRINTF2(str, "Listed on %s as %s", dnsbl, inet_ntop(AF_INET, &dnsbl_result, tmp, sizeof tmp));
				spamlog(&scfg, &mqtt, client.protocol, "SESSION REFUSED", str, host_name, dnsbl_ip, NULL, NULL);
				sockprintf(socket, client.protocol, session
				           , "550 Mail from %s refused due to listing at %s"
				           , dnsbl_ip, dnsbl);
				lprintf(LOG_NOTICE, "%04d %-5s !REFUSED SESSION from blacklisted server (%lu total)"
				        , socket, client.protocol, ++stats.sessions_refused);
				return false;
			}
		}
	}

	SAFEPRINTF(smb.file, "%smail", scfg.data_dir);
	SAFEPRINTF(spam.file, "%sspam", scfg.data_dir);
	spam.retry_time = scfg.smb_retry_time;
	spam.subnum = INVALID_SUB;

	srand((unsigned int)(time(NULL) ^ (time_t)GetCurrentThreadId()));   /* seed random number generator */
	(void)rand(); /* throw-away first result */
	SAFEPRINTF4(session_id, "%x%x%x%lx", getpid(), socket, rand(), (long)clock());
	lprintf(LOG_DEBUG, "%04d %-5s [%s] Session ID=%s", socket, client.protocol, host_ip, session_id);
	SAFEPRINTF3(msgtxt_fname, "%sSBBS_%s.%s.msg", scfg.temp_dir, client.protocol, session_id);
	SAFEPRINTF3(newtxt_fname, "%sSBBS_%s.%s.new", scfg.temp_dir, client.protocol, session_id);
	SAFEPRINTF3(logtxt_fname, "%sSBBS_%s.%s.log", scfg.temp_dir, client.protocol, session_id);
	SAFEPRINTF3(rcptlst_fname, "%sSBBS_%s.%s.lst", scfg.temp_dir, client.protocol, session_id);
	rcptlst = fopen(rcptlst_fname, "w+");
	if (rcptlst == NULL) {
		errprintf(LOG_CRIT, WHERE, "%04d %-5s [%s] !ERROR %d creating recipient list: %s"
		          , socket, client.protocol, host_ip, errno, rcptlst_fname);
		sockprintf(socket, client.protocol, session, smtp_error, "fopen error");
		return false;
	}

	/* Initialize client display */
	client.size = sizeof(client);
	client.time = time32(NULL);
	SAFECOPY(client.addr, host_ip);
	SAFECOPY(client.host, host_name);
	client.port = inet_addrport(&smtp->client_addr);
	SAFECOPY(client.user, STR_UNKNOWN_USER);
	client.usernum = 0;
	client_on(socket, &client, false /* update */);

	if (startup->login_attempt.throttle
	    && (login_attempts = loginAttempts(startup->login_attempt_list, &smtp->client_addr)) > 1) {
		lprintf(LOG_DEBUG, "%04d %-5s Throttling suspicious connection from: %s (%lu login attempts)"
		        , socket, client.protocol, host_ip, login_attempts);
		mswait(login_attempts * startup->login_attempt.throttle);
	}

	bool* mailproc_to_match = static_cast<bool*>(calloc(sizeof(*mailproc_to_match), mailproc_count));
	if (mailproc_to_match == NULL) {
		fclose(rcptlst);
		errprintf(LOG_CRIT, WHERE, "%04d %-5s !ERROR allocating memory for mailproc_to_match", socket, client.protocol);
		sockprintf(socket, client.protocol, session, smtp_error, "malloc failure");
		return false;
	}

	if (trashcan2(&scfg, host_ip, host_name, "smtpspy", NULL)) {
		SAFECOPY(str, client.protocol);
		strlwr(str);
		SAFEPRINTF2(path, "%s%sspy.txt", scfg.logs_dir, str);
		spy = fopen(path, "a");
	}

	/* SMTP session active: */

	sockprintf(socket, client.protocol, session, "220 %s Synchronet %s Server %s%c-%s Ready"
	           , server_host_name(), client.protocol, VERSION, REVISION, PLATFORM_DESC);
	while (1) {
		rd = sockreadline(socket, client.protocol, session, buf, sizeof(buf));
		if (rd < 0)
			break;
		truncsp(buf);
		if (spy != NULL)
			fprintf(spy, "%s\n", buf);
		if (relay_user.number == 0 && dnsbl_result.s_addr && startup->options & MAIL_OPT_DNSBL_THROTTLE)
			mswait(DNSBL_THROTTLE_VALUE);
		if (smb_islocked(&smb)) {
			lprintf(LOG_WARNING, "%04d %-5s %s !MAIL BASE LOCKED: %s"
					, socket, client.protocol, client_id, smb.last_error);
			sockprintf(socket, client.protocol, session, smtp_error, "mail base locked");
			break;
		}
		if (state >= SMTP_STATE_DATA_HEADER) {
			if (!strcmp(buf, ".")) {

				state = SMTP_STATE_HELO;  /* RESET state machine here in case of error */
				cmd = SMTP_CMD_NONE;

				if (msgtxt == NULL) {
					errprintf(LOG_ERR, WHERE, "%04d %-5s %s !NO MESSAGE TEXT FILE POINTER?", socket, client.protocol, client_id);
					sockprintf(socket, client.protocol, session, "554 No message text");
					continue;
				}

				if (ftell(msgtxt) < 1) {
					errprintf(LOG_ERR, WHERE, "%04d %-5s %s !INVALID MESSAGE LENGTH: %ld (%lu lines)"
					          , socket, client.protocol, client_id, ftell(msgtxt), lines);
					sockprintf(socket, client.protocol, session, "554 No message text");
					continue;
				}

				lprintf(LOG_INFO, "%04d %-5s %s End of message (body: %lu lines, %lu bytes, header: %lu lines, %lu bytes)"
				        , socket, client.protocol, client_id, lines, ftell(msgtxt) - hdr_len, hdr_lines, hdr_len);

				if (!socket_check(socket, NULL, NULL, 0)) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !Sender disconnected (premature evacuation)", socket, client.protocol, client_id);
					continue;
				}

				stats.msgs_received++;

				/* Twit-listing (sender's name and e-mail addresses) here */
				twitlist_fname(&scfg, path, sizeof path);
				if (fexist(path) && find2strs(sender, sender_addr, path, NULL)) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !FILTERING TWIT-LISTED SENDER: '%s' <%s> (%lu total)"
					        , socket, client.protocol, client_id, sender, sender_addr, ++stats.msgs_refused);
					SAFEPRINTF2(tmp, "Twit-listed sender: '%s' <%s>", sender, sender_addr);
					spamlog(&scfg, &mqtt, client.protocol, "REFUSED", tmp, host_name, host_ip, rcpt_addr, reverse_path);
					sockprintf(socket, client.protocol, session, "554 Sender not allowed.");
					continue;
				}

				if (telegram == true) {        /* Telegram */
					const char*      head = "\1n\1h\1cInstant Message\1n from \1h\1y";
					const char*      tail = "\1n:\r\n\1h";
					struct addrinfo  ai;
					struct addrinfo *res, *cur;
					bool             matched = false;

					rewind(msgtxt);
					length = filelength(fileno(msgtxt));

					p = strchr(sender_addr, '@');
					memset(&ai, 0, sizeof(ai));
					ai.ai_flags = AI_PASSIVE;
					ai.ai_family = smtp->client_addr.addr.sa_family;
					if (getaddrinfo(p + 1, NULL, &ai, &res) != 0)
						p = NULL;
					else {
						for (cur = res; cur; cur = cur->ai_next) {
							char cur_ip[INET6_ADDRSTRLEN];

							if (inet_addrtop((xp_sockaddr *)cur->ai_addr, cur_ip, sizeof(cur_ip))) {
								if (strcmp(host_ip, cur_ip) == 0)
									matched = true;
							}
						}
						freeaddrinfo(res);
						if (!matched)
							p = NULL;
					}
					if (p == NULL)
						/* Append real IP and hostname if different */
						safe_snprintf(str, sizeof(str), "%s%s\r\n\1w[\1n%s\1h] (\1n%s\1h)%s"
						              , head, sender_addr, host_ip, host_name, tail);
					else
						safe_snprintf(str, sizeof(str), "%s%s%s", head, sender_addr, tail);

					if ((telegram_buf = (char*)malloc((size_t)(length + strlen(str) + 1))) == NULL) {
						errprintf(LOG_CRIT, WHERE, "%04d %-5s %s !ERROR allocating %" PRIu64 " bytes of memory for telegram from %s"
						          , socket, client.protocol, client_id, (uint64_t)(length + strlen(str) + 1), sender_addr);
						sockprintf(socket, client.protocol, session, insuf_stor);
						continue;
					}
					strcpy(telegram_buf, str);   /* can't use SAFECOPY here */
					if (fread(telegram_buf + strlen(str), 1, (size_t)length, msgtxt) != (size_t)length) {
						errprintf(LOG_ERR, WHERE, "%04d %-5s %s !ERROR reading %" PRIdOFF " bytes from telegram file"
						          , socket, client.protocol, client_id, length);
						sockprintf(socket, client.protocol, session, insuf_stor);
						free(telegram_buf);
						continue;
					}
					telegram_buf[length + strlen(str)] = 0; /* Need ASCIIZ */

					/* Send telegram to users */
					sec_list = iniReadSectionList(rcptlst, NULL);  /* Each section is a recipient */
					for (rcpt_count = 0; sec_list != NULL
					     && sec_list[rcpt_count] != NULL
					     && (startup->max_recipients == 0 || rcpt_count < startup->max_recipients); rcpt_count++) {

						section = sec_list[rcpt_count];

						SAFECOPY(rcpt_to, iniReadString(rcptlst, section, smb_hfieldtype(RECIPIENT), "unknown", value));
						usernum = iniReadInteger(rcptlst, section, smb_hfieldtype(RECIPIENTEXT), 0);
						SAFECOPY(rcpt_addr, iniReadString(rcptlst, section, smb_hfieldtype(RECIPIENTNETADDR), rcpt_to, value));

						if ((i = putsmsg(&scfg, usernum, telegram_buf)) == 0)
							lprintf(LOG_INFO, "%04d %-5s %s Created telegram (%" PRIdOFF "/%lu bytes) from '%s' to '%s' <%s>"
							        , socket, client.protocol, client_id, length, (ulong)strlen(telegram_buf), sender_addr, rcpt_to, rcpt_addr);
						else
							errprintf(LOG_ERR, WHERE, "%04d %-5s %s !ERROR %d creating telegram from '%s' to '%s' <%s>"
							          , socket, client.protocol, client_id, i, sender_addr, rcpt_to, rcpt_addr);
					}
					iniFreeStringList(sec_list);
					free(telegram_buf);
					sockprintf(socket, client.protocol, session, ok_rsp);
					telegram = false;
					continue;
				}

				FCLOSE_OPEN_FILE(msgtxt);
				FCLOSE_OPEN_FILE(rcptlst);

				/* External Mail Processing here */
				mailproc = NULL;
				msg_handled = false;
				if (mailproc_count) {
					SAFEPRINTF3(proc_err_fname, "%sSBBS_%s.%s.err", scfg.temp_dir, client.protocol, session_id);
					remove(proc_err_fname);

					for (i = 0; i < mailproc_count && !msg_handled; i++) {
						struct mailproc* mp = &mailproc_list[i];
						if (mp->disabled)
							continue;

						if (!mp->process_dnsbl && dnsbl_result.s_addr)
							continue;

						if (!mp->process_spam && spam_bait_result)
							continue;

						if (!chk_ar(&scfg, mp->ar, &relay_user, &client))
							continue;

						if (mp->to != NULL && !mailproc_to_match[i])
							continue;

						if (mp->from != NULL
						    && !findstr_in_list(sender_addr, mp->from, NULL))
							continue;

						mailcmdstr(mp->cmdline
						           , msgtxt_fname, newtxt_fname, logtxt_fname
						           , rcptlst_fname, proc_err_fname
						           , host_name, host_ip, relay_user.number
						           , rcpt_addr
						           , sender, sender_addr, reverse_path, str);
						lprintf(LOG_INFO, "%04d %-5s %s Executing external mail processor: %s"
						        , socket, client.protocol, client_id, mp->name);

						if (mp->native) {
							lprintf(LOG_DEBUG, "%04d %-5s %s Executing external command: %s"
							        , socket, client.protocol, client_id, str);
							if ((j = system(str)) != 0) {
								lprintf(LOG_NOTICE, "%04d %-5s %s system(%s) returned %d (errno: %d)"
								        , socket, client.protocol, client_id, str, j, errno);
								if (mp->ignore_on_error) {
									lprintf(LOG_WARNING, "%04d %-5s %s !IGNORED MAIL due to mail processor (%s) error: %d"
									        , socket, client.protocol, client_id, mp->name, j);
									msg_handled = true;
								}
							}
						} else {  /* JavaScript */
							if (!js_mailproc(socket, &client, &relay_user
							                 , mp
							                 , str /* cmdline */
							                 , msgtxt_fname, newtxt_fname, logtxt_fname
							                 , rcpt_addr
							                 , rcptlst_fname, proc_err_fname
							                 , sender, sender_addr, reverse_path, hello_name, &js_result
							                 , &js_runtime, &js_cx, &js_glob
							                 , client.protocol) || js_result != 0) {
#if 0 /* calling exit() in a script causes js_mailproc to return false */
								lprintf(LOG_NOTICE, "%04d !SMTP JavaScript mailproc command (%s) failed (returned: %d)"
								        , socket, str, js_result);
								if (mailproc->ignore_on_error) {
									lprintf(LOG_WARNING, "%04d !SMTP IGNORED MAIL due to mail processor (%s) failure"
									        , socket, mailproc->name);
									msg_handled = true;
								}
#endif
							}
						}
						/* Log debug output (file) from mailproc: */
						if (flength(logtxt_fname) > 0 && (proc_out = fopen(logtxt_fname, "r")) != NULL) {
							while (!feof(proc_out)) {
								if (!fgets(str, sizeof(str), proc_out))
									break;
								truncsp(str);
								lprintf(LOG_DEBUG, "%04d %-5s %s External mail processor (%s) debug: %s"
								        , socket, client.protocol, client_id, mp->name, str);
							}
							fclose(proc_out);
						}
						remove(logtxt_fname);
						if (!mp->passthru || flength(proc_err_fname) > 0 || !fexist(msgtxt_fname) || !fexist(rcptlst_fname)) {
							mailproc = mp;
							msg_handled = true;
							break;
						}
					}
					if (flength(proc_err_fname) > 0
					    && (proc_out = fopen(proc_err_fname, "r")) != NULL) {
						lprintf(LOG_NOTICE, "%04d %-5s %s !External mail processor (%s) created: %s"
						        , socket, client.protocol, client_id, mailproc->name, proc_err_fname);
						while (!feof(proc_out)) {
							int n;
							if (!fgets(str, sizeof(str), proc_out))
								break;
							truncsp(str);
							lprintf(LOG_WARNING, "%04d %-5s %s !External mail processor (%s) error: %s"
							        , socket, client.protocol, client_id, mailproc->name, str);
							n = atoi(str);
							if (n >= 100 && n < 1000)
								sockprintf(socket, client.protocol, session, "%s", str);
							else
								sockprintf(socket, client.protocol, session, "554%c%s"
								           , ftell(proc_out) < filelength(fileno(proc_out)) ? '-' : ' '
								           , str);
						}
						fclose(proc_out);
						msg_handled = true;
					}
					else if (!fexist(msgtxt_fname) || !fexist(rcptlst_fname)) {
						lprintf(LOG_NOTICE, "%04d %-5s %s External mail processor (%s) removed %s file"
						        , socket, client.protocol, client_id
						        , mailproc->name
						        , fexist(msgtxt_fname) == false ? "message text" : "recipient list");
						sockprintf(socket, client.protocol, session, ok_rsp);
						msg_handled = true;
					}
					else if (msg_handled)
						sockprintf(socket, client.protocol, session, ok_rsp);
					remove(proc_err_fname); /* Remove error file here */
				}

				/* Re-open files */
				/* We must do this before continuing for handled msgs */
				/* to prevent freopen(NULL) and orphaned temp files */
				if ((rcptlst = fopen(rcptlst_fname, fexist(rcptlst_fname) ? "r":"w+")) == NULL) {
					errprintf(LOG_ERR, WHERE, "%04d %-5s %s !ERROR %d re-opening recipient list: %s"
					          , socket, client.protocol, client_id, errno, rcptlst_fname);
					if (!msg_handled)
						sockprintf(socket, client.protocol, session, smtp_error, "fopen error");
					continue;
				}

				if (!msg_handled && subnum == INVALID_SUB && iniReadSectionCount(rcptlst, NULL) < 1) {
					lprintf(LOG_DEBUG, "%04d %-5s %s No recipients in recipient list file (message handled by external mail processor?)"
					        , socket, client.protocol, client_id);
					sockprintf(socket, client.protocol, session, ok_rsp);
					msg_handled = true;
				}
				if (msg_handled) {
					if (mailproc != NULL)
						lprintf(LOG_NOTICE, "%04d %-5s %s Message handled by external mail processor (%s, %lu total)"
						        , socket, client.protocol, client_id, mailproc->name, ++mailproc->handled);
					continue;
				}

				/* If mailproc has written new message text to .new file, use that instead of .msg */
				if (flength(newtxt_fname) > 0) {
					remove(msgtxt_fname);
					SAFECOPY(msgtxt_fname, newtxt_fname);
				} else
					remove(newtxt_fname);

				if ((msgtxt = fopen(msgtxt_fname, "rb")) == NULL) {
					errprintf(LOG_ERR, WHERE, "%04d %-5s %s !ERROR %d re-opening message file: %s"
					          , socket, client.protocol, client_id, errno, msgtxt_fname);
					sockprintf(socket, client.protocol, session, smtp_error, "fopen error");
					continue;
				}

				/* Initialize message header */
				smb_freemsgmem(&msg);
				memset(&msg, 0, sizeof(smbmsg_t));

				/* Parse message header here */
				hfield_type = UNKNOWN;
				smb_error = SMB_SUCCESS; /* no SMB error */
				errmsg = insuf_stor;
				while (!feof(msgtxt)) {
					char field[32];

					if (!fgets(buf, sizeof(buf), msgtxt))
						break;
					truncsp(buf);
					if (buf[0] == 0)   /* blank line marks end of header */
						break;

					if ((p = get_header_field(buf, field, sizeof(field))) != NULL) {
						//normalize_hfield_value(p);
						if (stricmp(field, "SUBJECT") == 0) {
							/* SPAM Filtering/Logging */
							if (relay_user.number == 0) {
								if (trashcan(&scfg, p, "subject")) {
									lprintf(LOG_NOTICE, "%04d %-5s %s !BLOCKED SUBJECT (%s) from: %s (%lu total)"
									        , socket, client.protocol, client_id, p, reverse_path, ++stats.msgs_refused);
									SAFEPRINTF2(tmp, "Blocked subject (%s) from: %s"
									            , p, reverse_path);
									spamlog(&scfg, &mqtt, client.protocol, "REFUSED"
									        , tmp, host_name, host_ip, rcpt_addr, reverse_path);
									errmsg = "554 Subject not allowed.";
									smb_error = SMB_FAILURE;
									break;
								}
								if (dnsbl_result.s_addr && startup->dnsbl_tag[0] && !(startup->options & MAIL_OPT_DNSBL_IGNORE)) {
									safe_snprintf(str, sizeof(str), "%.*s: %.*s"
									              , (int)sizeof(str) / 2, startup->dnsbl_tag
									              , (int)sizeof(str) / 2, p);
									p = str;
									lprintf(LOG_NOTICE, "%04d %-5s %s TAGGED MAIL SUBJECT from blacklisted server with: %s"
									        , socket, client.protocol, client_id, startup->dnsbl_tag);
									msg.hdr.attr |= MSG_SPAM;
								}
							}
							smb_hfield_str(&msg, hfield_type = RFC822SUBJECT, p);
							continue;
						}
						if (stricmp(field, "FROM") == 0) {
							if (relay_user.number == 0
							    && !chk_email_addr(socket, client.protocol, p, host_name, host_ip, rcpt_addr, reverse_path, "FROM")) {
								errmsg = "554 Sender not allowed.";
								smb_error = SMB_FAILURE;
								break;
							}
							char from_addr[128];
							parse_mail_address(p, /* name: */ NULL, 0, from_addr, sizeof(from_addr) - 1);
							if (dnsbl_result.s_addr && email_addr_is_exempt(from_addr)) {
								lprintf(LOG_INFO, "%04d %-5s %s Ignoring DNSBL results for exempt sender (from): %s"
								        , socket, client.protocol, client_id, from_addr);
								dnsbl_result.s_addr = 0;
							}
						}
						if (relay_user.number == 0 && stricmp(field, "TO") == 0 && !spam_bait_result
						    && !chk_email_addr(socket, client.protocol, p, host_name, host_ip, rcpt_addr, reverse_path, "TO")) {
							errmsg = "550 Unknown user.";
							smb_error = SMB_FAILURE;
							break;
						}
						if (stricmp(field, "X-Spam-Flag") == 0 && stricmp(p, "Yes") == 0)
							msg.hdr.attr |= MSG_SPAM;   /* e.g. flagged by SpamAssasin */
					}
					if ((smb_error = parse_header_field((char*)buf, &msg, &hfield_type)) != SMB_SUCCESS) {
						if (smb_error == SMB_ERR_HDR_LEN)
							lprintf(LOG_WARNING, "%04d %-5s %s !MESSAGE HEADER EXCEEDS %u BYTES"
							        , socket, client.protocol, client_id, SMB_MAX_HDR_LEN);
						else
							errprintf(LOG_ERR, WHERE, "%04d %-5s %s !ERROR %d adding header field: %s"
							          , socket, client.protocol, client_id, smb_error, buf);
						break;
					}
				}
				if (smb_error != SMB_SUCCESS) {    /* SMB Error */
					sockprintf(socket, client.protocol, session, "%s", errmsg);
					stats.msgs_refused++;
					continue;
				}
				hfield_t* hfield;
				if ((p = static_cast<char *>(smb_get_hfield(&msg, RFC822TO, &hfield))) != NULL) {
					char* np = strdup(p);
					if (np != NULL) {
						if (mimehdr_value_decode(np, &msg))
							smb_hfield_str(&msg, RECIPIENTLIST, np);
						else
							hfield->type = RECIPIENTLIST;
						parse_mail_address(np
						                   , rcpt_name, sizeof(rcpt_name) - 1
						                   , rcpt_addr, sizeof(rcpt_addr) - 1);
						free(np);
					}
				}
				if ((p = static_cast<char *>(smb_get_hfield(&msg, RFC822FROM, NULL))) != NULL) {
					parse_mail_address(p
					                   , sender, sizeof(sender) - 1
					                   , sender_addr, sizeof(sender_addr) - 1);
					// We only support MIME-encoded name portion of 'name <user@addr>'
					mimehdr_value_decode(sender, &msg);
				}
				dnsbl_recvhdr = false;
				if (startup->options & MAIL_OPT_DNSBL_CHKRECVHDRS)  {
					for (i = 0; !dnsbl_result.s_addr && i < msg.total_hfields; i++)  {
						if (msg.hfield[i].type == SMTPRECEIVED)  {
							if (chk_received_hdr(socket, client.protocol, static_cast<const char *>(msg.hfield_dat[i]), &dnsbl_result, dnsbl, dnsbl_ip)) {
								dnsbl_recvhdr = true;
								break;
							}
						}
					}
				}
				if (relay_user.number == 0 && dnsbl_result.s_addr && !(startup->options & MAIL_OPT_DNSBL_IGNORE)) {
					msg.hdr.attr |= MSG_SPAM;
					/* tag message as spam */
					if (startup->dnsbl_hdr[0]) {
						safe_snprintf(str, sizeof(str), "%s: %s is listed on %s as %s"
						              , startup->dnsbl_hdr, dnsbl_ip
						              , dnsbl, inet_ntop(AF_INET, &dnsbl_result, tmp, sizeof tmp));
						smb_hfield_str(&msg, RFC822HEADER, str);
						lprintf(LOG_NOTICE, "%04d %-5s %s TAGGED MAIL HEADER from blacklisted server with: %s"
						        , socket, client.protocol, client_id, startup->dnsbl_hdr);
					}
					if (startup->dnsbl_hdr[0] || startup->dnsbl_tag[0]) {
						SAFEPRINTF2(str, "Listed on %s as %s", dnsbl, inet_ntop(AF_INET, &dnsbl_result, tmp, sizeof tmp));
						spamlog(&scfg, &mqtt, client.protocol, "TAGGED", str, host_name, dnsbl_ip, rcpt_addr, reverse_path);
					}
				}
				if (dnsbl_recvhdr)           /* DNSBL-listed IP found in Received header? */
					dnsbl_result.s_addr = 0; /* Reset DNSBL look-up result between messages */

				if ((scfg.sys_misc & SM_DELREADM)
				    || ((startup->options & MAIL_OPT_KILL_READ_SPAM) && (msg.hdr.attr & MSG_SPAM)))
					msg.hdr.attr |= MSG_KILLREAD;

				if (sender[0] == 0) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !MISSING mail header 'FROM' field (%lu total)"
					        , socket, client.protocol, client_id, ++stats.msgs_refused);
					sockprintf(socket, client.protocol, session, "554 Mail header missing 'FROM' field");
					subnum = INVALID_SUB;
					continue;
				}
				if (relay_user.number == 0
				    && smb_netaddr_type(sender) == NET_INTERNET
				    && compare_addrs(sender, sender_addr) != 0) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !FORGED mail header 'FROM' field ('%s' vs '%s', %lu total)"
					        , socket, client.protocol, client_id, sender, sender_addr, ++stats.msgs_refused);
					sockprintf(socket, client.protocol, session, "554 Mail header contains mismatched 'FROM' field");
					subnum = INVALID_SUB;
					continue;
				}
				char sender_info[512];
				if (relay_user.number) {
					SAFEPRINTF(str, "%u", relay_user.number);
					smb_hfield_str(&msg, SENDEREXT, str);
					SAFEPRINTF2(sender_info, "'%s' #%u", sender, relay_user.number);
				} else if (compare_addrs(sender, sender_addr) == 0) {
					angle_bracket(sender_info, sizeof(sender_info), sender_addr);
				} else {
					safe_snprintf(sender_info, sizeof(sender_info), "'%s' %s", sender, angle_bracket(tmp, sizeof(tmp), sender_addr));
				}
				if (relay_user.number && subnum != INVALID_SUB) {
					nettype = NET_NONE;
					smb_hfield_str(&msg, SENDER, relay_user.alias);
				} else {
					nettype = NET_INTERNET;
					smb_hfield_str(&msg, SENDER, sender);
					smb_hfield(&msg, SENDERNETTYPE, sizeof(nettype), &nettype);
					smb_hfield_str(&msg, SENDERNETADDR, sender_addr);
				}
				smb_hfield_str(&msg, SMTPREVERSEPATH, reverse_path);
				if ((p = static_cast<char *>(smb_get_hfield(&msg, RFC822ORG, &hfield))) != NULL) {
					char* np = strdup(p);
					if (np != NULL) {
						if (mimehdr_value_decode(np, &msg))
							smb_hfield_str(&msg, SENDERORG, np);
						else
							hfield->type = SENDERORG;
						free(np);
					}
				}
				if ((p = static_cast<char *>(smb_get_hfield(&msg, RFC822CC, &hfield))) != NULL) {
					char* np = strdup(p);
					if (np != NULL) {
						if (mimehdr_value_decode(np, &msg))
							smb_hfield_str(&msg, SMB_CARBONCOPY, np);
						else
							hfield->type = SMB_CARBONCOPY;
						free(np);
					}
				}
				if ((p = static_cast<char *>(smb_get_hfield(&msg, RFC822REPLYTO, &hfield))) != NULL) {
					char* np = strdup(p);
					if (np != NULL) {
						if (mimehdr_value_decode(np, &msg))
							smb_hfield_str(&msg, REPLYTOLIST, np);
						else
							hfield->type = REPLYTOLIST;
						free(np);
					}
				}
				if ((p = static_cast<char *>(smb_get_hfield(&msg, RFC822SUBJECT, &hfield))) != NULL) {
					char* np = strdup(p);
					if (np != NULL) {
						if (mimehdr_value_decode(np, &msg))
							smb_hfield_str(&msg, SUBJECT, np);
						else
							hfield->type = SUBJECT;
						free(np);
					}
				}
				else
					smb_hfield(&msg, SUBJECT, 0, NULL);

				length = filelength(fileno(msgtxt)) - ftell(msgtxt);

				if (startup->max_msg_size && length > startup->max_msg_size) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !Message size (%" PRIdOFF ") from %s to <%s> exceeds maximum: %u bytes"
					        , socket, client.protocol, client_id, length, sender_info, rcpt_addr, startup->max_msg_size);
					sockprintf(socket, client.protocol, session, "552 Message size (%" PRIdOFF ") exceeds maximum: %u bytes"
					           , length, startup->max_msg_size);
					subnum = INVALID_SUB;
					stats.msgs_refused++;
					continue;
				}

				if ((msgbuf = (char*)malloc((size_t)(length + 1))) == NULL) {
					errprintf(LOG_CRIT, WHERE, "%04d %-5s %s !ERROR allocating %" PRIdOFF " bytes of memory"
					          , socket, client.protocol, client_id, length + 1);
					sockprintf(socket, client.protocol, session, insuf_stor);
					subnum = INVALID_SUB;
					continue;
				}
				if (fread(msgbuf, (size_t)length, 1, msgtxt) != 1)
					*msgbuf = '\0';
				else
					msgbuf[length] = 0; /* ASCIIZ */

				/* Do external JavaScript processing here? */

				if (subnum != INVALID_SUB) {   /* Message Base */
					uint reason;
					if (relay_user.number == 0) {
						memset(&relay_user, 0, sizeof(relay_user));
						if (dnsbl_recvhdr || dnsbl_result.s_addr) {
							lprintf(LOG_NOTICE, "%04d %-5s %s !refusing to post message (on %s) from DNS-Blacklisted client: %s"
							        , socket, client.protocol, client_id, scfg.sub[subnum]->sname, sender_addr);
							sockprintf(socket, client.protocol, session, "550 Insufficient access: DNS-blacklisted");
							subnum = INVALID_SUB;
							stats.msgs_refused++;
							continue;
						}
					}

					if (!user_can_post(&scfg, subnum, &relay_user, &client, &reason)) {
						strip_ctrl(text[reason], tmp);
						lprintf(LOG_NOTICE, "%04d %-5s %s !%s (user #%u) cannot post on %s (reason[%u]: %s)"
						        , socket, client.protocol, client_id, sender_addr, relay_user.number
						        , scfg.sub[subnum]->sname, reason + 1, tmp);
						sockprintf(socket, client.protocol, session, "550 Insufficient access: %s", tmp);
						subnum = INVALID_SUB;
						stats.msgs_refused++;
						continue;
					}

					if (startup->post_to[0] != '\0')
						SAFECOPY(rcpt_name, startup->post_to);
					else if (rcpt_name[0] == 0)
						SAFECOPY(rcpt_name, "All");
					smb_hfield_str(&msg, RECIPIENT, rcpt_name);

					smb.subnum = subnum;
					if ((i = savemsg(&scfg, &smb, &msg, &client, server_host_name(), msgbuf, /* remsg: */ NULL)) != SMB_SUCCESS) {
						lprintf(LOG_WARNING, "%04d %-5s %s !ERROR %d (%s) %s posting message to %s (%s)"
						        , socket, client.protocol, client_id, i, smb.last_error, sender_info, scfg.sub[subnum]->sname, smb.file);
						sockprintf(socket, client.protocol, session, "452 ERROR %d (%s) posting message"
						           , i, smb.last_error);
					} else {
						lprintf(LOG_INFO, "%04d %-5s %s %s posted a message on %s (%s)"
						        , socket, client.protocol, client_id, sender_info, scfg.sub[subnum]->sname, smb.file);
						sockprintf(socket, client.protocol, session, ok_rsp);
						if (relay_user.number != 0)
							user_posted_msg(&scfg, &relay_user, 1);
						signal_smtp_sem();
					}
					free(msgbuf);
					smb_close(&smb);
					subnum = INVALID_SUB;
					continue;
				}

				/* Create/check hashes of known SPAM */
				{
					hash_t** hashes;
					bool     is_spam = spam_bait_result;
					long     sources = SMB_HASH_SOURCE_SPAM;

					if ((dnsbl_recvhdr || dnsbl_result.s_addr) && startup->options & MAIL_OPT_DNSBL_SPAMHASH)
						is_spam = true;

					lprintf(LOG_DEBUG, "%04d %-5s %s Calculating message hashes (sources=%lx, msglen=%lu)"
					        , socket, client.protocol, client_id, sources, (ulong)strlen(msgbuf));
					if ((hashes = smb_msghashes(&msg, (uchar*)msgbuf, sources)) != NULL) {
						hash_t found;

						for (i = 0; hashes[i]; i++)
							lprintf(LOG_DEBUG, "%04d %-5s %s Message %s crc32=%x flags=%x length=%u"
							        , socket, client.protocol, client_id, smb_hashsourcetype(hashes[i]->source)
							        , hashes[i]->data.crc32, hashes[i]->flags, hashes[i]->length);

						lprintf(LOG_DEBUG, "%04d %-5s %s Searching SPAM database for a match", socket, client.protocol, client_id);
						if ((i = smb_findhash(&spam, hashes, &found, sources, /* Mark: */ true)) == SMB_SUCCESS) {
							SAFEPRINTF3(str, "%s (%s) found in SPAM database (added on %s)"
							            , smb_hashsourcetype(found.source)
							            , smb_hashsource(&msg, found.source)
							            , timestr(&scfg, found.time, tmp)
							            );
							lprintf(LOG_NOTICE, "%04d %-5s %s Message from %s %s", socket, client.protocol, client_id, sender_info, str);
							if (!is_spam) {
								spamlog(&scfg, &mqtt, client.protocol, "IGNORED"
								        , str, host_name, host_ip, rcpt_addr, reverse_path);
								is_spam = true;
							}
						} else {
							lprintf(LOG_DEBUG, "%04d %-5s %s Done searching SPAM database", socket, client.protocol, client_id);
							if (i != SMB_ERR_NOT_FOUND)
								errprintf(LOG_ERR, WHERE, "%04d %-5s %s !ERROR %d (%s) opening SPAM database"
								          , socket, client.protocol, client_id, i, spam.last_error);
						}

						if (is_spam) {
							size_t n, total = 0;
							for (n = 0; hashes[n] != NULL; n++)
								if (!(hashes[n]->flags & SMB_HASH_MARKED)) {
									lprintf(LOG_INFO, "%04d %-5s %s Adding message %s (%s) from %s to SPAM database"
									        , socket, client.protocol, client_id
									        , smb_hashsourcetype(hashes[n]->source)
									        , smb_hashsource(&msg, hashes[n]->source)
									        , sender_info
									        );
									total++;
								}
							if (total) {
								lprintf(LOG_DEBUG, "%04d %-5s %s Adding %lu message hashes to SPAM database", socket, client.protocol, client_id, (ulong)total);
								smb_addhashes(&spam, hashes, /* skip_marked: */ true);
							}
							if (i != SMB_SUCCESS && !spam_bait_result && (dnsbl_recvhdr || dnsbl_result.s_addr))
								is_spam = false;
						}
						smb_close_hash(&spam);

						smb_freehashes(hashes);
					} else
						errprintf(LOG_ERR, WHERE, "%04d %-5s %s !smb_msghashes returned NULL", socket, client.protocol, client_id);

					if (is_spam || ((startup->options & MAIL_OPT_DNSBL_IGNORE) && (dnsbl_recvhdr || dnsbl_result.s_addr))) {
						free(msgbuf);
						if (is_spam)
							lprintf(LOG_NOTICE, "%04d %-5s %s !IGNORED SPAM MESSAGE from %s to <%s> (%lu total)"
							        , socket, client.protocol, client_id, sender_info, rcpt_addr, ++stats.msgs_ignored);
						else {
							SAFEPRINTF2(str, "Listed on %s as %s", dnsbl, inet_ntop(AF_INET, &dnsbl_result, tmp, sizeof tmp));
							lprintf(LOG_NOTICE, "%04d %-5s %s !IGNORED MAIL from %s to <%s> from server: %s (%lu total)"
							        , socket, client.protocol, client_id, sender_info, rcpt_addr, str, ++stats.msgs_ignored);
							spamlog(&scfg, &mqtt, client.protocol, "IGNORED"
							        , str, host_name, dnsbl_ip, rcpt_addr, reverse_path);
						}
						/* pretend we received it */
						sockprintf(socket, client.protocol, session, ok_rsp);
						subnum = INVALID_SUB;
						continue;
					}
				}

				char rcpt_info[512];
				if (compare_addrs(rcpt_name, rcpt_addr) == 0)
					angle_bracket(rcpt_info, sizeof(rcpt_info), rcpt_addr);
				else
					safe_snprintf(rcpt_info, sizeof(rcpt_info), "'%s' %s", rcpt_name, angle_bracket(tmp, sizeof(tmp), rcpt_addr));
				lprintf(LOG_DEBUG, "%04d %-5s %s Saving message data from %s to %s"
				        , socket, client.protocol, client_id, sender_info, rcpt_info);
				pthread_mutex_lock(&savemsg_mutex);
				/* E-mail */
				smb.subnum = INVALID_SUB;
				/* creates message data, but no header or index records (since msg.to==NULL) */
				i = savemsg(&scfg, &smb, &msg, &client, server_host_name(), msgbuf, /* remsg: */ NULL);
				if (smb_countattachments(&smb, &msg, msgbuf) > 0)
					msg.hdr.auxattr |= MSG_MIMEATTACH;
				msg.hdr.netattr |= NETMSG_KILLSENT;
				free(msgbuf);
				if (i != SMB_SUCCESS) {
					smb_close(&smb);
					pthread_mutex_unlock(&savemsg_mutex);
					errprintf(LOG_CRIT, WHERE, "%04d %-5s %s !ERROR %d (%s) saving message from %s to %s"
					          , socket, client.protocol, client_id, i, smb.last_error, sender_info, rcpt_info);
					sockprintf(socket, client.protocol, session, "452 ERROR %d (%s) saving message"
					           , i, smb.last_error);
					continue;
				}

				lprintf(LOG_DEBUG, "%04d %-5s %s Saved message data from %s to %s"
				        , socket, client.protocol, client_id, sender_info, rcpt_info);

				sec_list = iniReadSectionList(rcptlst, NULL);  /* Each section is a recipient */
				for (rcpt_count = 0; sec_list != NULL
				     && sec_list[rcpt_count] != NULL
				     && (startup->max_recipients == 0 || rcpt_count < startup->max_recipients); rcpt_count++) {

					section = sec_list[rcpt_count];

					SAFECOPY(rcpt_to, iniReadString(rcptlst, section, smb_hfieldtype(RECIPIENT), "unknown", value));
					usernum = iniReadInteger(rcptlst, section, smb_hfieldtype(RECIPIENTEXT), 0);
					agent = iniReadShortInt(rcptlst, section, smb_hfieldtype(RECIPIENTAGENT), AGENT_PERSON);
					nettype = iniReadShortInt(rcptlst, section, smb_hfieldtype(RECIPIENTNETTYPE), NET_NONE);
					SAFEPRINTF(str, "#%u", usernum);
					SAFECOPY(rcpt_addr, iniReadString(rcptlst, section, smb_hfieldtype(RECIPIENTNETADDR), str, value));
					SAFECOPY(forward_path, iniReadString(rcptlst, section, smb_hfieldtype(SMTPFORWARDPATH), "", value));
					if (compare_addrs(rcpt_to, rcpt_addr) == 0)
						angle_bracket(rcpt_info, sizeof(rcpt_info), rcpt_addr);
					else
						safe_snprintf(rcpt_info, sizeof(rcpt_info), "'%s' %s", rcpt_to, angle_bracket(tmp, sizeof(tmp), rcpt_addr));

					if (nettype == NET_NONE /* Local destination */ && usernum == 0) {
						errprintf(LOG_ERR, WHERE, "%04d %-5s %s !Can't deliver mail from %s to user #0"
						          , socket, client.protocol, client_id, sender_info);
						break;
					}

					if ((i = smb_copymsgmem(&smb, &newmsg, &msg)) != SMB_SUCCESS) {
						errprintf(LOG_ERR, WHERE, "%04d %-5s %s !ERROR %d (%s) copying message from %s"
						          , socket, client.protocol, client_id, i, smb.last_error, sender_info);
						break;
					}

					with_val = 0;
					if (esmtp)
						with_val |= WITH_ESMTP;
					if (auth_login)
						with_val |= WITH_AUTH;
					if (session != -1)
						with_val |= WITH_TLS;

					when_t when {};
					when.time = msg.hdr.when_imported.time;
					when.zone = msg.hdr.when_imported.zone;
					snprintf(hdrfield, sizeof(hdrfield),
					         "from %s (%s [%s%s])\r\n"
					         "          by %s [%s%s] (%s %s%c-%s) with %s\r\n"
					         "          for %s; %s\r\n"
					         "          (envelope-from %s)"
					         , host_name, hello_name
					         , smtp->client_addr.addr.sa_family == AF_INET6?"IPv6: ":""
					         , host_ip
					         , server_host_name()
					         , server_addr.addr.sa_family == AF_INET6?"IPv6: ":""
					         , server_ip
					         , server_name
					         , VERSION, REVISION, PLATFORM_DESC
					         , with_clauses[with_val]
					         , forward_path, msgdate(when, date)
					         , reverse_path);
					smb_hfield_add_str(&newmsg, SMTPRECEIVED, hdrfield, /* insert: */ true);

					if (nettype == NET_FIDO) {
						newmsg.hdr.netattr |= NETMSG_LOCAL | NETMSG_KILLSENT;
						char* tp = strchr(rcpt_name, '@');
						if (tp != NULL)
							*tp = 0;
						// Remove "(ftn_addr)" portion of to name
						SAFEPRINTF(str, "(%s)", rcpt_addr);
						if ((tp = strstr(rcpt_name, str)) != NULL && tp != rcpt_name) {
							*tp = 0;
							truncsp(rcpt_name);
						}
						if (!matchusername(&scfg, rcpt_name, rcpt_to)) {
							SAFECOPY(rcpt_name, rcpt_to);
							truncstr(rcpt_name, "@");
						}
					}
					smb_hfield_str(&newmsg, RECIPIENT, rcpt_name);
					if (forward_path[0] != 0)
						smb_hfield_str(&newmsg, SMTPFORWARDPATH, forward_path);

					if (usernum && nettype != NET_INTERNET) {  /* Local destination or QWKnet routed */
						/* This is required for fixsmb to be able to rebuild the index */
						SAFEPRINTF(str, "%u", usernum);
						smb_hfield_str(&newmsg, RECIPIENTEXT, str);
					}
					if (nettype != NET_NONE) {
						smb_hfield(&newmsg, RECIPIENTNETTYPE, sizeof(nettype), &nettype);
						smb_hfield_netaddr(&newmsg, RECIPIENTNETADDR, rcpt_addr, &nettype);
					}
					if (agent != newmsg.to_agent)
						smb_hfield(&newmsg, RECIPIENTAGENT, sizeof(agent), &agent);

					add_msg_ids(&scfg, &smb, &newmsg, /* remsg: */ NULL);
					lprintf(LOG_DEBUG, "%04d %-5s %s Adding message header from %s to %s"
					        , socket, client.protocol, client_id, sender_info, rcpt_info);
					i = smb_addmsghdr(&smb, &newmsg, smb_storage_mode(&scfg, &smb));
					smb_freemsgmem(&newmsg);
					if (i != SMB_SUCCESS) {
						errprintf(LOG_ERR, WHERE, "%04d %-5s %s !ERROR %d (%s) adding message header from %s to %s"
						          , socket, client.protocol, client_id, i, smb.last_error, sender_info, rcpt_info);
						break;
					}
					lprintf(LOG_INFO, "%04d %-5s %s Added message header #%u from %s to %s"
					        , socket, client.protocol, client_id, newmsg.hdr.number, sender_info, rcpt_info);
					if (relay_user.number != 0) {
						user_sent_email(&scfg, &relay_user, 1, usernum == 1);
						lprintf(LOG_DEBUG, "%04d %-5s %s #%u sent %u email messages today (of %u max), %u total"
						        , socket, client.protocol, client_id, relay_user.number
						        , relay_user.etoday, scfg.level_emailperday[relay_user.level], relay_user.emails);
					}

					if (nettype == NET_FIDO && scfg.netmail_sem[0])
						ftouch(mailcmdstr(scfg.netmail_sem
						                  , msgtxt_fname, newtxt_fname, logtxt_fname
						                  , rcptlst_fname, proc_err_fname
						                  , host_name, host_ip, relay_user.number
						                  , rcpt_addr
						                  , sender, sender_addr, reverse_path, str));

					if (!(startup->options & MAIL_OPT_NO_NOTIFY) && usernum) {
						if (!newmsg.idx.to || startup->notify_offline_users || user_is_online(&scfg, usernum)) {
							p = sender_addr;
							if (stricmp(sender, sender_addr) == 0) {
								if ((p = strchr(sender_addr, '@')) == NULL)
									p = sender_addr;
								else
									p++;
							}
							safe_snprintf(str, sizeof(str)
							              , text[InternetMailReceived]
							              , timestr(&scfg, newmsg.hdr.when_imported.time, tmp)
							              , sender, p);
							if (newmsg.hdr.auxattr & MSG_HFIELDS_UTF8)
								utf8_to_cp437_inplace(str);
							if (!newmsg.idx.to)  /* Forwarding */
								sprintf(str + strlen(str), text[InternetMailForwarded], rcpt_addr);
							putsmsg(&scfg, usernum, str);
						}
					}
					if (!(msg.hdr.attr & MSG_SPAM) && startup->archive_ars[0] != 0 && usernum != 0 && relay_user.number != usernum) {
						user_t user{};
						user.number = usernum;
						if (getuserdat(&scfg, &user) == USER_SUCCESS) {
							if (chk_ars(&scfg, startup->archive_ars, &user, &client))
								archive_mail(msgtxt_fname, usernum, "smtp_in", session_id);
						}
					}
				}
				iniFreeStringList(sec_list);
				if (rcpt_count < 1) {
					smb_freemsg_dfields(&smb, &msg, SMB_ALL_REFS);
					sockprintf(socket, client.protocol, session, insuf_stor);
				}
				else {
					if (rcpt_count > 1)
						smb_incmsg_dfields(&smb, &msg, (ushort)(rcpt_count - 1));
					sockprintf(socket, client.protocol, session, ok_rsp);
					signal_smtp_sem();
				}
#if 0 /* This shouldn't be necessary here */
				smb_close_da(&smb);
#endif
				smb_close(&smb);
				if (startup->archive_ars[0] != 0 && relay_user.number != 0
					&& chk_ars(&scfg, startup->archive_ars, &relay_user, &client))
					archive_mail(msgtxt_fname, relay_user.number, "smtp_relay", session_id);
				pthread_mutex_unlock(&savemsg_mutex);
				continue;
			}
			if (buf[0] == 0 && state == SMTP_STATE_DATA_HEADER) {
				state = SMTP_STATE_DATA_BODY; /* Null line separates header and body */
				lines = 0;
				if (msgtxt != NULL) {
					fputs("\r\n", msgtxt);
					hdr_len = ftell(msgtxt);
				}
				continue;
			}
			if (state == SMTP_STATE_DATA_BODY) {
				p = buf;
				if (*p == '.')
					p++;            /* Transparency (RFC821 4.5.2) */
				if (strlen(p) > RFC822_MAX_LINE_LEN) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !%s sent an ILLEGALLY-LONG body line (%d chars > %d): '%s'"
					        , socket, client.protocol, client_id, reverse_path, (int)strlen(p), RFC822_MAX_LINE_LEN, p);
					sockprintf(socket, client.protocol, session, "500 Line too long (body)");
					break;
				}
				if (msgtxt != NULL) {
					fputs(p, msgtxt);
					fputs("\r\n", msgtxt);
				}
				lines++;
				if ((lines % 100) == 0 && (msgtxt != NULL))
					lprintf(LOG_DEBUG, "%04d %-5s %s received %lu lines (%lu bytes) of body text"
					        , socket, client.protocol, client_id, lines, ftell(msgtxt) - hdr_len);
				continue;
			}
			/* RFC822 Header parsing */
			if (strlen(buf) > RFC822_MAX_LINE_LEN) {
				lprintf(LOG_NOTICE, "%04d %-5s %s !%s sent an ILLEGALLY-LONG header line (%d chars > %d): '%s'"
				        , socket, client.protocol, client_id, reverse_path, (int)strlen(buf), RFC822_MAX_LINE_LEN, buf);
				sockprintf(socket, client.protocol, session, "500 Line too long (header)");
				break;
			}
			strip_char(buf, buf, '\r'); /* There should be no bare carriage returns in header fields */
			if (startup->options & MAIL_OPT_DEBUG_RX_HEADER)
				lprintf(LOG_DEBUG, "%04d %-5s %s %s", socket, client.protocol, client_id, buf);

			{
				char field[32];

				if ((p = get_header_field(buf, field, sizeof(field))) != NULL) {
					if (stricmp(field, "FROM") == 0) {
						parse_mail_address(p
						                   , sender,        sizeof(sender) - 1
						                   , sender_addr,   sizeof(sender_addr) - 1);
					}
				}
			}

			if (msgtxt != NULL) {
				fputs(buf, msgtxt);
				fputs("\r\n", msgtxt);
			}
			hdr_lines++;
			continue;
		}
		if (strlen(buf) > SMTP_MAX_CMD_LEN) {
			lprintf(LOG_NOTICE, "%04d %-5s %s sent an ILLEGALLY-LONG command line (%d chars > %d): '%s'"
			        , socket, client.protocol, client_id, (int)strlen(buf), SMTP_MAX_CMD_LEN, buf);
			sockprintf(socket, client.protocol, session, "500 Line too long (command)");
			break;
		}
		strip_ctrl(buf, buf);
		lprintf(LOG_DEBUG, "%04d %-5s %s RX: %s", socket, client.protocol, client_id, buf);
		if (!strnicmp(buf, "HELO", 4)) {
			p = buf + 4;
			SKIP_WHITESPACE(p);
			SAFECOPY(hello_name, p);
			sockprintf(socket, client.protocol, session, "250 %s", server_host_name());
			esmtp = false;
			state = SMTP_STATE_HELO;
			cmd = SMTP_CMD_NONE;
			telegram = false;
			subnum = INVALID_SUB;
			continue;
		}
		if (!strnicmp(buf, "EHLO", 4)) {
			p = buf + 4;
			SKIP_WHITESPACE(p);
			SAFECOPY(hello_name, p);
			sockprintf(socket, client.protocol, session, "250-%s", server_host_name());
			sockprintf(socket, client.protocol, session, "250-AUTH PLAIN LOGIN CRAM-MD5");
			sockprintf(socket, client.protocol, session, "250-SEND");
			sockprintf(socket, client.protocol, session, "250-SOML");
			sockprintf(socket, client.protocol, session, "250-SAML");
			sockprintf(socket, client.protocol, session, "250-8BITMIME");
			if (session == -1)
				sockprintf(socket, client.protocol, session, "250-STARTTLS");
			if (startup->max_msg_size)
				sockprintf(socket, client.protocol, session, "250-SIZE %u", startup->max_msg_size);
			sockprintf(socket, client.protocol, session, ok_rsp);
			esmtp = true;
			state = SMTP_STATE_HELO;
			cmd = SMTP_CMD_NONE;
			telegram = false;
			subnum = INVALID_SUB;
			continue;
		}
		if (relay_user.number != 0 && strnicmp(buf, "AUTH", 4) == 0) {
			sockprintf(socket, client.protocol, session, badseq_rsp);
			continue;
		}
		if ((auth_login = (stricmp(buf, "AUTH LOGIN") == 0)) == true
		    || strnicmp(buf, "AUTH PLAIN ", 11) == 0
		    || stricmp(buf, "AUTH PLAIN") == 0) {
			char user_pass[128] = "";
			ZERO_VAR(relay_user);
			listRemoveTaggedNode(&current_logins, socket, /* free_data */ true);
			if (auth_login) {
				sockprintf(socket, client.protocol, session, "334 VXNlcm5hbWU6");  /* Base64-encoded "Username:" */
				if ((rd = sockreadline(socket, client.protocol, session, buf, sizeof(buf))) < 1) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !Missing AUTH LOGIN username argument", socket, client.protocol, client_id);
					badlogin(socket, session, badarg_rsp, NULL, NULL, &client, &smtp->client_addr);
					continue;
				}
				if (startup->options & MAIL_OPT_DEBUG_RX_RSP)
					lprintf(LOG_DEBUG, "%04d %-5s %s RX: %s", socket, client.protocol, client_id, buf);
				if (b64_decode(user_name, sizeof(user_name), buf, rd) < 1 || str_has_ctrl(user_name)) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !Bad AUTH LOGIN username argument: %s", socket, client.protocol, client_id, buf);
					badlogin(socket, session, badarg_rsp, NULL, NULL, &client, &smtp->client_addr);
					continue;
				}
				sockprintf(socket, client.protocol, session, "334 UGFzc3dvcmQ6");  /* Base64-encoded "Password:" */
				if ((rd = sockreadline(socket, client.protocol, session, buf, sizeof(buf))) < 1) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !Missing AUTH LOGIN password argument", socket, client.protocol, client_id);
					badlogin(socket, session, badarg_rsp, user_name, NULL, &client, &smtp->client_addr);
					continue;
				}
				if (startup->options & MAIL_OPT_DEBUG_RX_RSP)
					lprintf(LOG_DEBUG, "%04d %-5s %s RX: %s", socket, client.protocol, client_id, buf);
				if (b64_decode(user_pass, sizeof(user_pass), buf, rd) < 1 || str_has_ctrl(user_pass)) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !Bad AUTH LOGIN password argument: %s", socket, client.protocol, client_id, buf);
					badlogin(socket, session, badarg_rsp, user_name, NULL, &client, &smtp->client_addr);
					continue;
				}
			} else {    /* AUTH PLAIN b64(<username>\0<user-id>\0<password>) */
				p = buf + 10;
				SKIP_WHITESPACE(p);
				if (*p == 0) {
					lprintf(LOG_DEBUG, "%04d %-5s %s AUTH PLAIN argument not provided, sending server challenge", socket, client.protocol, client_id);
					// RFC 4954: Note that there is still a space following the reply code, so the complete response line is "334 "
					sockprintf(socket, client.protocol, session, "334 ");
					if ((rd = sockreadline(socket, client.protocol, session, buf, sizeof buf)) < 1) {
						lprintf(LOG_NOTICE, "%04d %-5s %s !No AUTH PLAIN response received", socket, client.protocol, client_id);
						badlogin(socket, session, badarg_rsp, NULL, NULL, &client, &smtp->client_addr);
						continue;
					}
					if (startup->options & MAIL_OPT_DEBUG_RX_RSP)
						lprintf(LOG_DEBUG, "%04d %-5s %s RX: %s", socket, client.protocol, client_id, buf);
					p = buf;
				}
				ZERO_VAR(tmp);
				if (b64_decode(tmp, sizeof(tmp), p, strlen(p)) < 1 || str_has_ctrl(tmp)) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !Bad AUTH PLAIN argument: %s", socket, client.protocol, client_id, p);
					badlogin(socket, session, badarg_rsp, NULL, NULL, &client, &smtp->client_addr);
					continue;
				}
				p = tmp;
				while (*p) p++; /* skip username */
				p++;            /* skip NULL */
				if (*p == 0) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !Missing AUTH PLAIN user-id argument", socket, client.protocol, client_id);
					badlogin(socket, session, badarg_rsp, NULL, NULL, &client, &smtp->client_addr);
					continue;
				}
				SAFECOPY(user_name, p);
				while (*p) p++; /* skip user-id */
				p++;            /* skip NULL */
				if (*p == 0) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !Missing AUTH PLAIN password argument", socket, client.protocol, client_id);
					badlogin(socket, session, badarg_rsp, user_name, NULL, &client, &smtp->client_addr);
					continue;
				}
				SAFECOPY(user_pass, p);
			}

			if ((relay_user.number = find_login_id(&scfg, user_name)) == 0) {
				if (scfg.sys_misc & SM_ECHO_PW)
					lprintf(LOG_NOTICE, "%04d %-5s %s !UNKNOWN USER: '%s' (password: %s)"
					        , socket, client.protocol, client_id, user_name, user_pass);
				else
					lprintf(LOG_NOTICE, "%04d %-5s %s !UNKNOWN USER: '%s'"
					        , socket, client.protocol, client_id, user_name);
				badlogin(socket, session, badauth_rsp, user_name, user_pass, &client, &smtp->client_addr);
				break;
			}
			if ((i = getuserdat(&scfg, &relay_user)) != 0) {
				errprintf(LOG_ERR, WHERE, "%04d %-5s %s <%s> !ERROR %d getting user data"
				          , socket, client.protocol, client_id, user_name, i);
				badlogin(socket, session, badauth_rsp, NULL, NULL, &client, NULL);
				break;
			}
			if (relay_user.misc & (DELETED | INACTIVE)) {
				lprintf(LOG_NOTICE, "%04d %-5s %s <%s> !DELETED or INACTIVE user #%u"
				        , socket, client.protocol, client_id, user_name, relay_user.number);
				badlogin(socket, session, badauth_rsp, NULL, NULL, &client, NULL);
				break;
			}
			if (stricmp(user_pass, relay_user.pass)) {
				if (scfg.sys_misc & SM_ECHO_PW)
					lprintf(LOG_NOTICE, "%04d %-5s %s <%s> !FAILED Password attempt: '%s' expected '%s'"
					        , socket, client.protocol, client_id, user_name, user_pass, relay_user.pass);
				else
					lprintf(LOG_NOTICE, "%04d %-5s %s <%s> !FAILED Password attempt"
					        , socket, client.protocol, client_id, user_name);
				badlogin(socket, session, badauth_rsp, user_name, user_pass, &client, &smtp->client_addr);
				break;
			}
			if (!chk_ars(&scfg, startup->login_ars, &relay_user, &client)) {
				lprintf(LOG_NOTICE, "%04d %-5s [%s] <%s> !Insufficient server access: %s"
				        , socket, client.protocol, client_id, user_name, startup->login_ars);
				badlogin(socket, session, badauth_rsp, user_name, NULL, &client, &smtp->client_addr);
				break;
			}

			if (relay_user.pass[0]) {
				loginSuccess(startup->login_attempt_list, &smtp->client_addr);
				listAddNodeData(&current_logins, client.addr, strlen(client.addr) + 1, socket, LAST_NODE);
			}

			/* Update client display */
			SAFECOPY(client.user, relay_user.alias);
			client.usernum = relay_user.number;
			client_on(socket, &client, true /* update */);

			loginuserdat(&scfg, &relay_user, &client, /* use_prot: */true, /* save_ars: */NULL);

			lprintf(LOG_INFO, "%04d %-5s %s <%s> logged-in using %s authentication (%u emails sent today)"
			        , socket, client.protocol, client_id, relay_user.alias, auth_login ? "LOGIN" : "PLAIN"
					, relay_user.etoday);
			SAFEPRINTF(client_id, "<%s>", relay_user.alias);
			sockprintf(socket, client.protocol, session, auth_ok);
#ifdef _WIN32
			if (startup->sound.login[0] && !sound_muted(&scfg))
				PlaySound(startup->sound.login, NULL, SND_ASYNC | SND_FILENAME);
#endif
			mqtt_user_login(&mqtt, &client);
			continue;
		}
		if (!stricmp(buf, "AUTH CRAM-MD5")) {
			ZERO_VAR(relay_user);
			listRemoveTaggedNode(&current_logins, socket, /* free_data */ true);

			safe_snprintf(challenge, sizeof(challenge), "<%x%x%lx%lx@%s>"
			              , rand(), socket, (ulong)time(NULL), (ulong)clock(), server_host_name());
#if 0
			lprintf(LOG_DEBUG, "%04d SMTP CRAM-MD5 challenge: %s"
			        , socket, challenge);
#endif
			b64_encode(str, sizeof(str), challenge, strlen(challenge));
			sockprintf(socket, client.protocol, session, "334 %s", str);
			if ((rd = sockreadline(socket, client.protocol, session, buf, sizeof(buf))) < 1) {
				lprintf(LOG_NOTICE, "%04d %-5s %s !Missing AUTH CRAM-MD5 response", socket, client.protocol, client_id);
				sockprintf(socket, client.protocol, session, badarg_rsp);
				continue;
			}
			if (startup->options & MAIL_OPT_DEBUG_RX_RSP)
				lprintf(LOG_DEBUG, "%04d %-5s %s RX: %s", socket, client.protocol, client_id, buf);

			if (b64_decode(response, sizeof(response), buf, rd) < 1 || str_has_ctrl(response)) {
				lprintf(LOG_NOTICE, "%04d %-5s %s !Bad AUTH CRAM-MD5 response: %s", socket, client.protocol, client_id, buf);
				sockprintf(socket, client.protocol, session, badarg_rsp);
				continue;
			}
#if 0
			lprintf(LOG_DEBUG, "%04d SMTP CRAM-MD5 response: %s"
			        , socket, response);
#endif
			if ((p = strrchr(response, ' ')) != NULL)
				*(p++) = 0;
			else
				p = response;
			SAFECOPY(user_name, response);
			if ((relay_user.number = find_login_id(&scfg, user_name)) == 0) {
				lprintf(LOG_NOTICE, "%04d %-5s %s !UNKNOWN USER: '%s'"
				        , socket, client.protocol, client_id, user_name);
				badlogin(socket, session, badauth_rsp, user_name, NULL, &client, &smtp->client_addr);
				break;
			}
			if ((i = getuserdat(&scfg, &relay_user)) != 0) {
				errprintf(LOG_ERR, WHERE, "%04d %-5s %s <%s> !ERROR %d getting user data"
				          , socket, client.protocol, client_id, user_name, i);
				badlogin(socket, session, badauth_rsp, NULL, NULL, &client, NULL);
				break;
			}
			if (relay_user.misc & (DELETED | INACTIVE)) {
				lprintf(LOG_NOTICE, "%04d %-5s %s <%s> !DELETED or INACTIVE user #%u"
				        , socket, client.protocol, client_id, user_name, relay_user.number);
				badlogin(socket, session, badauth_rsp, NULL, NULL, &client, NULL);
				break;
			}
			/* Calculate correct response */
			memset(secret, 0, sizeof(secret));
			SAFECOPY(secret, relay_user.pass);
			strlwr(secret); /* this is case sensitive, so convert to lowercase first */
			for (i = 0; i < (int)sizeof(secret); i++)
				md5_data[i] = secret[i] ^ 0x36; /* ipad */
			strcpy(md5_data + i, challenge);
			MD5_calc(digest, md5_data, sizeof(secret) + strlen(challenge));
			for (i = 0; i < (int)sizeof(secret); i++)
				md5_data[i] = secret[i] ^ 0x5c; /* opad */
			memcpy(md5_data + i, digest, sizeof(digest));
			MD5_calc(digest, md5_data, sizeof(secret) + sizeof(digest));
			MD5_hex(str, digest);
			if (strcmp(p, str)) {
				lprintf(LOG_NOTICE, "%04d %-5s %s <%s> !FAILED CRAM-MD5 authentication"
				        , socket, client.protocol, client_id, relay_user.alias);
#if 0
				lprintf(LOG_DEBUG, "%04d !SMTP calc digest: %s"
				        , socket, str);
				lprintf(LOG_DEBUG, "%04d !SMTP resp digest: %s"
				        , socket, p);
#endif
				badlogin(socket, session, badauth_rsp, user_name, p, &client, &smtp->client_addr);
				break;
			}
			if (!chk_ars(&scfg, startup->login_ars, &relay_user, &client)) {
				lprintf(LOG_NOTICE, "%04d %-5s [%s] <%s> !Insufficient server access: %s"
				        , socket, client.protocol, client_id, user_name, startup->login_ars);
				badlogin(socket, session, badauth_rsp, user_name, NULL, &client, &smtp->client_addr);
				break;
			}

			if (relay_user.pass[0]) {
				loginSuccess(startup->login_attempt_list, &smtp->client_addr);
				listAddNodeData(&current_logins, client.addr, strlen(client.addr) + 1, socket, LAST_NODE);
			}

			/* Update client display */
			SAFECOPY(client.user, relay_user.alias);
			client.usernum = relay_user.number;
			client_on(socket, &client, true /* update */);

			loginuserdat(&scfg, &relay_user, &client, /* use_prot: */true, /* save_ars: */NULL);

			lprintf(LOG_INFO, "%04d %-5s %s <%s> logged-in using CRAM-MD5 authentication (%u emails sent today)"
			        , socket, client.protocol, client_id, relay_user.alias, relay_user.etoday);
			SAFEPRINTF(client_id, "<%s>", relay_user.alias);
			sockprintf(socket, client.protocol, session, auth_ok);
#ifdef _WIN32
			if (startup->sound.login[0] && !sound_muted(&scfg))
				PlaySound(startup->sound.login, NULL, SND_ASYNC | SND_FILENAME);
#endif
			mqtt_user_login(&mqtt, &client);
			continue;
		}
		if (!strnicmp(buf, "AUTH", 4)) {
			sockprintf(socket, client.protocol, session, "504 Unrecognized authentication type.");
			continue;
		}
		if (!stricmp(buf, "QUIT")) {
			sockprintf(socket, client.protocol, session, "221 %s Service closing transmission channel", server_host_name());
			break;
		}
		if (!stricmp(buf, "NOOP")) {
			sockprintf(socket, client.protocol, session, ok_rsp);
			badcmds = 0;
			continue;
		}
		if (state < SMTP_STATE_HELO) {
			/* RFC 821 4.1.1 "The first command in a session must be the HELO command." */
			lprintf(LOG_NOTICE, "%04d %-5s %s !MISSING 'HELO' command (Received: '%s')", socket, client.protocol, client_id, buf);
			sockprintf(socket, client.protocol, session, badseq_rsp);
			continue;
		}
		if (!stricmp(buf, "TURN")) {
			sockprintf(socket, client.protocol, session, "502 command not supported");
			badcmds = 0;
			continue;
		}
		if (!stricmp(buf, "RSET")) {
			smb_freemsgmem(&msg);
			memset(&msg, 0, sizeof(smbmsg_t));        /* Initialize message header */
			reverse_path[0] = 0;
			state = SMTP_STATE_HELO;
			cmd = SMTP_CMD_NONE;
			telegram = false;
			subnum = INVALID_SUB;
			spam_bait_result = false;

			/* reset recipient list */
			if ((rcptlst = freopen(rcptlst_fname, "w+", rcptlst)) == NULL) {
				errprintf(LOG_ERR, WHERE, "%04d %-5s %s !ERROR %d re-opening %s"
				          , socket, client.protocol, client_id, errno, rcptlst_fname);
				sockprintf(socket, client.protocol, session, smtp_error, "fopen error");
				break;
			}
			rcpt_count = 0;

			memset(mailproc_to_match, false, sizeof(bool) * mailproc_count);

			sockprintf(socket, client.protocol, session, ok_rsp);
			badcmds = 0;
			lprintf(LOG_INFO, "%04d %-5s %s Session reset", socket, client.protocol, client_id);
			continue;
		}
		if (!strnicmp(buf, "MAIL FROM:", 10)
		    || !strnicmp(buf, "SEND FROM:", 10)   /* Send a Message (Telegram) to a local ONLINE user */
		    || !strnicmp(buf, "SOML FROM:", 10)   /* Send OR Mail a Message to a local user */
		    || !strnicmp(buf, "SAML FROM:", 10)   /* Send AND Mail a Message to a local user */
		    ) {
			p = buf + 10;
			if (relay_user.number == 0
			    && !chk_email_addr(socket, client.protocol, p, host_name, host_ip, NULL, NULL, "REVERSE PATH")) {
				sockprintf(socket, client.protocol, session, "554 Sender not allowed.");
				stats.msgs_refused++;
				break;
			}
			SKIP_WHITESPACE(p);
			SAFECOPY(reverse_path, p);
			if ((p = strchr(reverse_path, ' ')) != NULL)  /* Truncate "<user@domain> KEYWORD=VALUE" to just "<user@domain>" per RFC 1869 */
				*p = 0;

			/* If MAIL FROM address is in dnsbl_exempt.cfg, clear DNSBL results */
			if (dnsbl_result.s_addr && email_addr_is_exempt(reverse_path)) {
				lprintf(LOG_INFO, "%04d %-5s %s Ignoring DNSBL results for exempt sender: %s"
				        , socket, client.protocol, client_id, reverse_path);
				dnsbl_result.s_addr = 0;
			}

			/* Update client display */
			if (relay_user.number == 0) {
				SAFECOPY(client.user, reverse_path);
				client_on(socket, &client, true /* update */);
			}

			/* Setup state */
			state = SMTP_STATE_MAIL_FROM;
			if (!strnicmp(buf, "MAIL FROM:", 10))
				cmd = SMTP_CMD_MAIL;
			else if (!strnicmp(buf, "SEND FROM:", 10))
				cmd = SMTP_CMD_SEND;
			else if (!strnicmp(buf, "SOML FROM:", 10))
				cmd = SMTP_CMD_SOML;
			else if (!strnicmp(buf, "SAML FROM:", 10))
				cmd = SMTP_CMD_SAML;

			/* reset recipient list */
			if ((rcptlst = freopen(rcptlst_fname, "w+", rcptlst)) == NULL) {
				errprintf(LOG_ERR, WHERE, "%04d %-5s %s !ERROR %d re-opening %s"
				          , socket, client.protocol, client_id, errno, rcptlst_fname);
				sockprintf(socket, client.protocol, session, smtp_error, "fopen error");
				break;
			}
			rcpt_count = 0;
			memset(mailproc_to_match, false, sizeof(bool) * mailproc_count);
			sockprintf(socket, client.protocol, session, ok_rsp);
			badcmds = 0;
			continue;
		}

#if 0   /* No one uses this command */
		if (!strnicmp(buf, "VRFY", 4)) {
			p = buf + 4;
			SKIP_WHITESPACE(p);
			if (*p == 0) {
				sockprintf(socket, client.protocol, session, "550 No user specified.");
				continue;
			}
#endif

		/* Add to Recipient list */
		if (!strnicmp(buf, "RCPT TO:", 8)) {

			if (state < SMTP_STATE_MAIL_FROM) {
				lprintf(LOG_NOTICE, "%04d %-5s %s !MISSING 'MAIL' command", socket, client.protocol, client_id);
				sockprintf(socket, client.protocol, session, badseq_rsp);
				continue;
			}

			p = buf + 8;
			SKIP_WHITESPACE(p);
			SAFECOPY(rcpt_to, p);
			SAFECOPY(str, p);
			p = strrchr(str, '<');
			if (p == NULL)
				p = str;
			else
				p++;

			truncstr(str, ">");  /* was truncating at space too */

			routed = false;
			forward = false;
			no_forward = false;
			if (!strnicmp(p, FORWARD, strlen(FORWARD))) {
				forward = true;       /* force forward to user's netmail address */
				p += strlen(FORWARD);
			}
			if (!strnicmp(p, NO_FORWARD, strlen(NO_FORWARD))) {
				no_forward = true;    /* do not forward to user's netmail address */
				p += strlen(NO_FORWARD);
			}

			if (*p == 0) {
				lprintf(LOG_NOTICE, "%04d %-5s %s !NO RECIPIENT SPECIFIED"
				        , socket, client.protocol, client_id);
				sockprintf(socket, client.protocol, session, "500 No recipient specified");
				continue;
			}

			rcpt_name[0] = 0;
			SAFECOPY(rcpt_addr, p);

			/* Check recipient counter */
			if (startup->max_recipients) {
				if (rcpt_count >= startup->max_recipients) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !MAXIMUM RECIPIENTS (%d) REACHED"
					        , socket, client.protocol, client_id, startup->max_recipients);
					SAFEPRINTF(tmp, "Maximum recipient count (%d)", startup->max_recipients);
					spamlog(&scfg, &mqtt, client.protocol, "REFUSED", tmp
					        , host_name, host_ip, rcpt_addr, reverse_path);
					sockprintf(socket, client.protocol, session, "452 Too many recipients");
					stats.msgs_refused++;
					continue;
				}
				if (relay_user.number != 0 && !(relay_user.exempt & FLAG('M'))
				    && rcpt_count + (waiting = getmail(&scfg, relay_user.number, /* sent: */ true, /* SPAM: */ false)) > startup->max_recipients) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !MAXIMUM PENDING SENT EMAILS (%lu) REACHED for User #%u (%s)"
					        , socket, client.protocol, client_id, waiting, relay_user.number, relay_user.alias);
					sockprintf(socket, client.protocol, session, "452 Too many pending emails sent");
					stats.msgs_refused++;
					continue;
				}
			}

			if (relay_user.number && getuserdat(&scfg, &relay_user) == USER_SUCCESS
				&& (relay_user.etoday + rcpt_count) >= scfg.level_emailperday[relay_user.level]
			    && !(relay_user.exempt & FLAG('M'))) {
				lprintf(LOG_NOTICE, "%04d %-5s %s !EMAILS PER DAY LIMIT (%u) REACHED FOR USER #%u (%s)"
				        , socket, client.protocol, client_id, scfg.level_emailperday[relay_user.level], relay_user.number, relay_user.alias);
				SAFEPRINTF2(tmp, "Maximum emails per day (%u) for %s"
				            , scfg.level_emailperday[relay_user.level], relay_user.alias);
				spamlog(&scfg, &mqtt, client.protocol, "REFUSED", tmp
				        , host_name, host_ip, rcpt_addr, reverse_path);
				sockprintf(socket, client.protocol, session, "452 Too many emails today");
				stats.msgs_refused++;
				continue;
			}

			/* Check for SPAM bait recipient */
			if ((spam_bait_result = findstr(rcpt_addr, spam_bait)) == true) {
				char reason[256];
				SAFEPRINTF(reason, "SPAM BAIT (%s) taken", rcpt_addr);
				lprintf(LOG_NOTICE, "%04d %-5s %s %s by: %s"
				        , socket, client.protocol, client_id, reason, reverse_path);
				if (relay_user.number == 0) {
					strcpy(tmp, "IGNORED");
					if (dnsbl_result.s_addr == 0                       /* Don't double-filter */
					    && !spam_block_exempt)  {
						lprintf(LOG_NOTICE, "%04d %-5s !BLOCKING IP ADDRESS: %s in %s", socket, client.protocol, client_id, spam_block);
						filter_ip(&scfg, client.protocol, reason, host_name, host_ip, reverse_path, spam_block, startup->spam_block_duration);
						strcat(tmp, " and BLOCKED");
					}
					spamlog(&scfg, &mqtt, client.protocol, tmp, "Attempted recipient in SPAM BAIT list"
					        , host_name, host_ip, rcpt_addr, reverse_path);
					dnsbl_result.s_addr = 0;
				}
				sockprintf(socket, client.protocol, session, ok_rsp);
				state = SMTP_STATE_RCPT_TO;
				continue;
			}

			/* Check for blocked recipients */
			if (relay_user.number == 0
			    && !chk_email_addr(socket, client.protocol, rcpt_addr, host_name, host_ip, rcpt_addr, reverse_path, "RECIPIENT")) {
				sockprintf(socket, client.protocol, session, "550 Unknown User: %s", rcpt_to);
				stats.msgs_refused++;
				continue;
			}

			if (relay_user.number == 0 && dnsbl_result.s_addr && startup->options & MAIL_OPT_DNSBL_BADUSER) {
				lprintf(LOG_NOTICE, "%04d %-5s %s !REFUSED MAIL from blacklisted server (%lu total)"
				        , socket, client.protocol, client_id, ++stats.sessions_refused);
				SAFEPRINTF2(str, "Listed on %s as %s", dnsbl, inet_ntop(AF_INET, &dnsbl_result, tmp, sizeof tmp));
				spamlog(&scfg, &mqtt, client.protocol, "REFUSED", str, host_name, host_ip, rcpt_addr, reverse_path);
				sockprintf(socket, client.protocol, session
				           , "550 Mail from %s refused due to listing at %s"
				           , host_ip, dnsbl);
				break;
			}

			if (spy == NULL
			    && trashcan2(&scfg, reverse_path, rcpt_addr, "smtpspy", NULL)) {
				SAFECOPY(tmp, client.protocol);
				strlwr(tmp);
				SAFEPRINTF2(path, "%s%sspy.txt", scfg.logs_dir, tmp);
				spy = fopen(path, "a");
			}

			/* Check for full address aliases */
			p = alias(&scfg, p, alias_buf);
			if (p == alias_buf)
				lprintf(LOG_DEBUG, "%04d %-5s %s ADDRESS ALIAS: %s (for %s)"
				        , socket, client.protocol, client_id, p, rcpt_addr);

			tp = strrchr(p, '@');
			if (cmd == SMTP_CMD_MAIL && tp != NULL) {
				/* RELAY */
				dest_port = inet_addrport(&server_addr);
				SAFECOPY(dest_host, tp + 1);
				if (relay_user.number && scfg.total_faddrs) {
					char* ftn_tld = strstr(dest_host, FIDO_TLD);
					if (ftn_tld != NULL && ftn_tld[strlen(FIDO_TLD)] == 0) {
						short      point, node, net, zone;

						fidoaddr_t faddr = scfg.faddr[0];
						point = 0;
						if ((sscanf(dest_host, "p%hu.f%hu.n%hu.z%hu" FIDO_TLD
						            , &point
						            , &node
						            , &net
						            , &zone) == 4
						     ||
						     sscanf(dest_host, "f%hu.n%hu.z%hu" FIDO_TLD
						            , &node
						            , &net
						            , &zone) == 3
						     ) && zone) {
							faddr.point = point;
							faddr.node = node;
							faddr.net = net;
							faddr.zone = zone;

							char faddrstr[64];
							smb_faddrtoa(&faddr, faddrstr);
							lprintf(LOG_INFO, "%04d %-5s %s %s relaying to FidoNet address: %s (%s)"
							        , socket, client.protocol, client_id, relay_user.alias, p, faddrstr);
							fprintf(rcptlst, "[%u]\n", rcpt_count++);
							fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(RECIPIENT), p);
							fprintf(rcptlst, "%s=%u\n", smb_hfieldtype(RECIPIENTNETTYPE), NET_FIDO);
							fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(RECIPIENTNETADDR), faddrstr);
							fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(SMTPFORWARDPATH), rcpt_to);

							sockprintf(socket, client.protocol, session, ok_rsp);
							state = SMTP_STATE_RCPT_TO;
							continue;
						}
					}
				}
				cp = strrchr(dest_host, ':');
				if (cp != NULL) {
					*cp = 0;
					dest_port = atoi(cp + 1);
				}
				if ((stricmp(dest_host, scfg.sys_inetaddr) != 0
				     && stricmp(dest_host, startup->host_name) != 0
				     && findstr(dest_host, domain_list) == false)
				    || dest_port != inet_addrport(&server_addr)) {

					SAFEPRINTF(relay_list, "%srelay.cfg", scfg.ctrl_dir);
					if (relay_user.number == 0 /* not authenticated, search for IP */
					    && startup->options & MAIL_OPT_SMTP_AUTH_VIA_IP) {
						relay_user.number = finduserstr(&scfg, 0, USER_IPADDR, host_ip, /* del */ false, /* next */ false, NULL, NULL);
						if (relay_user.number) {
							getuserdat(&scfg, &relay_user);
							if (relay_user.laston < time(NULL) - (60 * 60))  /* logon in past hour? */
								relay_user.number = 0;
						}
					} else
						getuserdat(&scfg, &relay_user);
					if (p != alias_buf /* forced relay by alias */ &&
					    (!(startup->options & MAIL_OPT_ALLOW_RELAY)
					     || relay_user.number == 0
					     || relay_user.rest & (FLAG('G') | FLAG('M'))) &&
					    !find2strs(host_name, host_ip, relay_list, NULL)) {
						lprintf(LOG_NOTICE, "%04d %-5s %s !ILLEGAL RELAY ATTEMPT from %s [%s] to %s"
						        , socket, client.protocol, client_id, reverse_path, host_ip, p);
						SAFEPRINTF(tmp, "Relay attempt to: %s", p);
						spamlog(&scfg, &mqtt, client.protocol, "REFUSED", tmp, host_name, host_ip, rcpt_addr, reverse_path);
						if (startup->options & MAIL_OPT_ALLOW_RELAY)
							sockprintf(socket, client.protocol, session, "553 Relaying through this server "
							           "requires authentication.  "
							           "Please authenticate before sending.");
						else {
							sockprintf(socket, client.protocol, session, "550 Relay not allowed.");
							stats.msgs_refused++;
						}
						break;
					}

					if (relay_user.number == 0)
						SAFECOPY(relay_user.alias, "Unknown User");

					lprintf(LOG_INFO, "%04d %-5s %s %s relaying to external mail service: %s"
					        , socket, client.protocol, client_id, relay_user.alias, tp + 1);

					fprintf(rcptlst, "[%u]\n", rcpt_count++);
					fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(RECIPIENT), rcpt_addr);
					fprintf(rcptlst, "%s=%u\n", smb_hfieldtype(RECIPIENTNETTYPE), NET_INTERNET);
					fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(RECIPIENTNETADDR), p);
					fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(SMTPFORWARDPATH), rcpt_to);

					sockprintf(socket, client.protocol, session, ok_rsp);
					state = SMTP_STATE_RCPT_TO;
					continue;
				}
			}
			if (tp != NULL)
				*tp = 0; /* truncate at '@' */

			tp = strchr(p, '!');   /* Routed QWKnet mail in <qwkid!user@host> format */
			if (tp != NULL) {
				*(tp++) = 0;
				SKIP_CHAR(tp, '"');              /* Skip '"' */
				truncstr(tp, "\"");              /* Strip '"' */
				SAFECOPY(rcpt_addr, tp);
				routed = true;
			}

			FIND_ALPHANUMERIC(p);               /* Skip '<' or '"' */
			truncstr(p, "\"");

			p = alias(&scfg, p, name_alias_buf);
			if (p == name_alias_buf)
				lprintf(LOG_DEBUG, "%04d %-5s %s NAME ALIAS: %s (for %s)"
				        , socket, client.protocol, client_id, p, rcpt_addr);

			/* Check if message is to be processed by one or more external mail processors */
			mailproc_match = INT_MAX;   // no match, by default
			for (i = 0; i < mailproc_count; i++) {

				if (!mailproc_list[i].process_dnsbl && dnsbl_result.s_addr)
					continue;

				if (!mailproc_list[i].process_spam && spam_bait_result)
					continue;

				if (!chk_ar(&scfg, mailproc_list[i].ar, &relay_user, &client))
					continue;

				if (find2strs_in_list(p, rcpt_addr, mailproc_list[i].to, NULL)) {
					mailproc_to_match[i] = true;
					if (!mailproc_list[i].passthru)
						mailproc_match = i;
				}
			}

			if (!strnicmp(p, "sub:", 4)) {     /* Post on a sub-board */
				p += 4;
				for (i = 0; i < scfg.total_subs; i++)
					if (!stricmp(p, scfg.sub[i]->code))
						break;
				if (i >= scfg.total_subs) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !UNKNOWN SUB-BOARD: %s", socket, client.protocol, client_id, p);
					sockprintf(socket, client.protocol, session, "550 Unknown sub-board: %s", p);
					continue;
				}
				subnum = i;
				sockprintf(socket, client.protocol, session, ok_rsp);
				state = SMTP_STATE_RCPT_TO;
				rcpt_count++;
				continue;
			}

			/* destined for a (non-passthru) external mail processor */
			if (mailproc_match < mailproc_count) {
				fprintf(rcptlst, "[%u]\n", rcpt_count++);
				fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(RECIPIENT), rcpt_addr);
				fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(SMTPFORWARDPATH), rcpt_to);

#if 0   /* should we fall-through to the sysop account? */
				fprintf(rcptlst, "%s=%u\n", smb_hfieldtype(RECIPIENTEXT), 1);
#endif
				lprintf(LOG_INFO, "%04d %-5s %s Routing mail for %s to External Mail Processor: %s"
				        , socket, client.protocol, client_id, rcpt_addr, mailproc_list[mailproc_match].name);
				sockprintf(socket, client.protocol, session, ok_rsp);
				state = SMTP_STATE_RCPT_TO;
				continue;
			}

			usernum = 0;  /* unknown user at this point */

			if (routed) {
				SAFECOPY(qwkid, p);
				truncstr(qwkid, "/");
				/* Search QWKnet hub-IDs for route destination */
				for (i = 0; i < scfg.total_qhubs; i++) {
					if (!stricmp(qwkid, scfg.qhub[i]->id))
						break;
				}
				if (i < scfg.total_qhubs) {    /* found matching QWKnet Hub */

					lprintf(LOG_INFO, "%04d %-5s %s Routing mail for '%s' <%s> to QWKnet Hub: %s"
					        , socket, client.protocol, client_id, rcpt_addr, p, scfg.qhub[i]->id);

					fprintf(rcptlst, "[%u]\n", rcpt_count++);
					fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(RECIPIENT), rcpt_addr);
					fprintf(rcptlst, "%s=%u\n", smb_hfieldtype(RECIPIENTNETTYPE), NET_QWK);
					fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(RECIPIENTNETADDR), p);
					fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(SMTPFORWARDPATH), rcpt_to);

					sockprintf(socket, client.protocol, session, ok_rsp);
					state = SMTP_STATE_RCPT_TO;
					continue;
				}
			}

			if ((p == alias_buf || p == name_alias_buf)
			    && IS_DIGIT(*p)) {
				usernum = atoi(p);            /* RX by user number */
				/* verify usernum */
				username(&scfg, usernum, str);
				if (!str[0] || !stricmp(str, "DELETED USER"))
					usernum = 0;
				p = str;
			} else {
				/* RX by "user alias", "user.alias" or "user_alias" */
				usernum = smtp_matchuser(&scfg, p, startup->options & MAIL_OPT_ALLOW_SYSOP_ALIASES, false);

				if (!usernum) { /* RX by "real name", "real.name", or "sysop.alias" */

					/* convert "user.name" to "user name" */
					SAFECOPY(rcpt_name, p);
					for (tp = rcpt_name; *tp; tp++)
						if (*tp == '.')
							*tp = ' ';

					if (!stricmp(p, scfg.sys_op) || !stricmp(rcpt_name, scfg.sys_op))
						usernum = 1;        /* RX by "sysop.alias" */

					if (!usernum && scfg.msg_misc & MM_REALNAME)   /* RX by "real name" */
						usernum = smtp_matchuser(&scfg, p, false, true);

					if (!usernum && scfg.msg_misc & MM_REALNAME)   /* RX by "real.name" */
						usernum = smtp_matchuser(&scfg, rcpt_name, false, true);
				}
			}
			if (!usernum && startup->default_user[0]) {
				usernum = matchuser(&scfg, startup->default_user, true /* sysop_alias */);
				if (usernum)
					lprintf(LOG_INFO, "%04d %-5s %s Forwarding mail for UNKNOWN USER to default user-recipient: '%s' #%u"
					        , socket, client.protocol, client_id, startup->default_user, usernum);
				else
					lprintf(LOG_WARNING, "%04d %-5s %s !UNKNOWN DEFAULT USER-RECIPIENT: '%s'"
					        , socket, client.protocol, client_id, startup->default_user);
			}

			if (usernum == INT_MAX) {
				lprintf(LOG_INFO, "%04d %-5s %s Blocked tag: %s", socket, client.protocol, client_id, rcpt_to);
				sockprintf(socket, client.protocol, session, "550 Unknown User: %s", rcpt_to);
				continue;
			}
			if (!usernum) {
				lprintf(LOG_NOTICE, "%04d %-5s %s !UNKNOWN USER-RECIPIENT: '%s'", socket, client.protocol, client_id, rcpt_to);
				sockprintf(socket, client.protocol, session, "550 Unknown User: %s", rcpt_to);
				continue;
			}
			user.number = usernum;
			if ((i = getuserdat(&scfg, &user)) != 0) {
				errprintf(LOG_ERR, WHERE, "%04d %-5s %s !ERROR %d getting data on user-recipient #%u (%s)"
				          , socket, client.protocol, client_id, i, usernum, p);
				sockprintf(socket, client.protocol, session, "550 Unknown User: %s", rcpt_to);
				continue;
			}
			if (user.misc & (DELETED | INACTIVE)) {
				lprintf(LOG_WARNING, "%04d %-5s %s !DELETED or INACTIVE user-recipient #%u (%s)"
				        , socket, client.protocol, client_id, usernum, p);
				sockprintf(socket, client.protocol, session, "550 Unknown User: %s", rcpt_to);
				continue;
			}
			if (cmd == SMTP_CMD_MAIL) {
				if ((user.rest & FLAG('I')) && relay_user.number == 0) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !I-restricted user-recipient #%u (%s) cannot receive unauthenticated SMTP mail"
					        , socket, client.protocol, client_id, user.number, user.alias);
					sockprintf(socket, client.protocol, session, "550 Closed mailbox: %s", rcpt_to);
					stats.msgs_refused++;
					continue;
				}
				if (startup->max_msgs_waiting && !(user.exempt & FLAG('W'))
				    && (waiting = getmail(&scfg, user.number, /* sent: */ false, /* spam: */ false)) > startup->max_msgs_waiting) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !User-recipient #%u (%s) mailbox (%lu msgs) exceeds the maximum (%u) msgs waiting"
					        , socket, client.protocol, client_id, user.number, user.alias, waiting, startup->max_msgs_waiting);
					sockprintf(socket, client.protocol, session, "450 Mailbox full: %s", rcpt_to);
					stats.msgs_refused++;
					continue;
				}
			}
			else if (cmd == SMTP_CMD_SEND) { /* Check if user online */
				for (i = 0; i < scfg.sys_nodes; i++) {
					getnodedat(&scfg, i + 1, &node, false, NULL);
					if (node.status == NODE_INUSE && node.useron == user.number
					    && !(node.misc & NODE_POFF))
						break;
				}
				if (i >= scfg.sys_nodes) {
					lprintf(LOG_NOTICE, "%04d %-5s %s !Attempt to send telegram to unavailable user-recipient #%u (%s)"
					        , socket, client.protocol, client_id, user.number, user.alias);
					sockprintf(socket, client.protocol, session, "450 User unavailable");
					continue;
				}
			}
			if (cmd != SMTP_CMD_MAIL)
				telegram = true;

			fprintf(rcptlst, "[%u]\n", rcpt_count++);
			fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(RECIPIENT), rcpt_addr);
			fprintf(rcptlst, "%s=%u\n", smb_hfieldtype(RECIPIENTEXT), user.number);

			/* Forward to Internet */
			tp = strrchr(user.netmail, '@');
			if (!telegram
			    && !routed
			    && !no_forward
			    && scfg.sys_misc & SM_FWDTONET
			    && (user.misc & NETMAIL || forward)
			    && tp != NULL && smb_netaddr_type(user.netmail) == NET_INTERNET
			    && stricmp(tp + 1, scfg.sys_inetaddr) != 0
			    && stricmp(tp + 1, startup->host_name) != 0
			    && findstr(tp + 1, domain_list) == false
			    ) {
				lprintf(LOG_INFO, "%04d %-5s %s Forwarding to: %s"
				        , socket, client.protocol, client_id, user.netmail);
				fprintf(rcptlst, "%s=%u\n", smb_hfieldtype(RECIPIENTNETTYPE), NET_INTERNET);
				fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(RECIPIENTNETADDR), user.netmail);
				sockprintf(socket, client.protocol, session, ok_rsp);  // used to be a 251 response, changed per RFC2821
			} else { /* Local (no-forward) */
				if (routed) { /* QWKnet */
					fprintf(rcptlst, "%s=%u\n", smb_hfieldtype(RECIPIENTNETTYPE), NET_QWK);
					fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(RECIPIENTNETADDR), user.alias);
				}
				fprintf(rcptlst, "%s=%s\n", smb_hfieldtype(SMTPFORWARDPATH), rcpt_to);
				sockprintf(socket, client.protocol, session, ok_rsp);
			}
			state = SMTP_STATE_RCPT_TO;
			continue;
		}
		/* Message Data (header and body) */
		if (!strnicmp(buf, "DATA", 4)) {
			if (state < SMTP_STATE_RCPT_TO) {
				lprintf(LOG_NOTICE, "%04d %-5s %s !MISSING 'RCPT TO' command", socket, client.protocol, client_id);
				sockprintf(socket, client.protocol, session, badseq_rsp);
				continue;
			}
			FCLOSE_OPEN_FILE(msgtxt);
			remove(msgtxt_fname);
			if ((msgtxt = fopen(msgtxt_fname, "w+b")) == NULL) {
				errprintf(LOG_ERR, WHERE, "%04d %-5s %s !ERROR %d opening %s"
				          , socket, client.protocol, client_id, errno, msgtxt_fname);
				sockprintf(socket, client.protocol, session, insuf_stor);
				continue;
			}
			/* These vars are potentially over-written by parsing an RFC822 header */
			/* get sender_addr */
			p = strrchr(reverse_path, '<');
			if (p == NULL)
				p = reverse_path;
			else
				p++;
			SAFECOPY(sender_addr, p);
			truncstr(sender_addr, ">");
			/* get sender */
			SAFECOPY(sender, sender_addr);
			if (truncstr(sender, "@") == NULL)
				sender[0] = 0;

			sockprintf(socket, client.protocol, session, "354 send the mail data, end with <CRLF>.<CRLF>");
			if (telegram)
				state = SMTP_STATE_DATA_BODY; /* No RFC headers in Telegrams */
			else
				state = SMTP_STATE_DATA_HEADER;
			lprintf(LOG_INFO, "%04d %-5s %s Receiving %s message from %s to <%s>"
			        , socket, client.protocol, client_id, telegram ? "telegram":"mail", reverse_path, rcpt_addr);
			hdr_lines = 0;
			continue;
		}
		if (session == -1 && !stricmp(buf, "STARTTLS")) {
			if (!do_cryptInit(lprintf)) {
				sockprintf(socket, client.protocol, session, "454 TLS not available");
				continue;
			}
			if ((cstat = cryptCreateSession(&session, CRYPT_UNUSED, CRYPT_SESSION_TLS_SERVER)) != CRYPT_OK) {
				GCESH(cstat, "SMTPS", socket, host_ip, CRYPT_UNUSED, "creating TLS session");
				sockprintf(socket, client.protocol, session, "454 TLS not available");
				continue;
			}
			if ((cstat = add_private_key(&scfg, lprintf, session)) != CRYPT_OK) {
				GCESH(cstat, "SMTPS", socket, host_ip, session, "setting private key");
				errprintf(LOG_ERR, WHERE,  "%04d SMTPS %s !Unable to set private key", socket, client_id);
				cryptDestroySession(session);
				session = -1;
				sockprintf(socket, client.protocol, session, "454 TLS not available");
				continue;
			}
			smtp->session = session;
			nodelay = true;
			setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
			nb = 0;
			ioctlsocket(socket, FIONBIO, &nb);
			if ((cstat = cryptSetAttribute(session, CRYPT_SESSINFO_NETWORKSOCKET, socket)) != CRYPT_OK) {
				GCESH(cstat, "SMTPS", socket, host_ip, session, "setting network socket");
				destroy_session(lprintf, session);
				session = -1;
				sockprintf(socket, client.protocol, session, "454 TLS not available");
				continue;
			}
			sockprintf(socket, client.protocol, -1, "220 Ready to start TLS");
			if ((cstat = cryptSetAttribute(session, CRYPT_SESSINFO_ACTIVE, 1)) != CRYPT_OK) {
				GCESH(cstat, "SMTPS", socket, host_ip, session, "setting session active");
				break;
			}
			if (startup->max_inactivity) {
				if ((cstat = cryptSetAttribute(session, CRYPT_OPTION_NET_READTIMEOUT, startup->max_inactivity)) != CRYPT_OK) {
					GCESH(cstat, "SMTPS", socket, host_ip, session, "setting read timeout");
					break;
				}
			}
			SAFECOPY(client.protocol, "SMTPS");
			continue;
		}
		sockprintf(socket, client.protocol, session, "500 Syntax error");
		lprintf(LOG_NOTICE, "%04d %-5s %s !UNSUPPORTED COMMAND: '%s'", socket, client.protocol, client_id, buf);
		if (++badcmds > SMTP_MAX_BAD_CMDS) {
			lprintf(LOG_NOTICE, "%04d %-5s %s !TOO MANY INVALID COMMANDS (%lu)", socket, client.protocol, client_id, badcmds);
			break;
		}
	}

	/* Free up resources here */
	smb_freemsgmem(&msg);

	FCLOSE_OPEN_FILE(msgtxt);
	if (!(startup->options & MAIL_OPT_DEBUG_RX_BODY))
		remove(msgtxt_fname);
	FCLOSE_OPEN_FILE(rcptlst);
	remove(rcptlst_fname);
	FCLOSE_OPEN_FILE(spy);
	js_cleanup(js_runtime, js_cx, &js_glob);

	listRemoveTaggedNode(&current_logins, socket, /* free_data */ true);
	client_off(socket);

	if (relay_user.number) {
#ifdef _WIN32
		if (startup->sound.logout[0] && !sound_muted(&scfg))
			PlaySound(startup->sound.logout, NULL, SND_ASYNC | SND_FILENAME);
#endif
		if ((i = logoutuserdat(&scfg, &relay_user, client.time)) != USER_SUCCESS)
			errprintf(LOG_ERR, WHERE, "%04d %-5s <%s> !ERROR %d in logoutuserdat", socket, client.protocol, relay_user.alias, i);
		mqtt_user_logout(&mqtt, &client, client.time);
	}
	/* Must be last */
	{
		int32_t remain = thread_down();
		lprintf(LOG_INFO, "%04d %-5s %s Session thread terminated (%u threads remain, %lu clients served)"
		        , socket, client.protocol, client_id, remain, ++stats.smtp_served);
	}
	free(mailproc_to_match);
	return true;
}

// Wrapper for smtp_client_thread() that does some basic resource management
static void smtp_thread(void* arg)
{
	smtp_t smtp = *(smtp_t*)arg;

	SetThreadName("sbbs/smtp");
	thread_up(true /* setuid */);

	free(arg);

	uint32_t client_count = protected_uint32_adjust_fetch(&active_clients, 1);
	if (client_count > client_highwater) {
		client_highwater = client_count;
		if (client_highwater > 1)
			lprintf(LOG_NOTICE, "%04d SMTP New active client highwater mark: %u"
			        , smtp.socket, client_highwater);
		mqtt_pub_uintval(&mqtt, TOPIC_SERVER, "highwater", mqtt.highwater = client_highwater);
	}
	update_clients();

	if (!smtp_client_thread(&smtp))
		thread_down();

	(void)protected_uint32_adjust(&active_clients, -1);
	update_clients();

	mail_close_socket(&smtp.socket, &smtp.session);
}

bool bounce(SOCKET sock, smb_t* smb, smbmsg_t* msg, char* err, bool immediate)
{
	char     str[128];
	char     attempts[64];
	char     msgid[256];
	int      i;
	ushort   agent = AGENT_SMTPSYSMSG;
	smbmsg_t newmsg;

	msg->hdr.delivery_attempts++;
	lprintf(LOG_NOTICE, "%04d SEND !Delivery attempt #%u FAILED (%s) for message #%u from %s to %s"
	        , sock
	        , msg->hdr.delivery_attempts
	        , err
	        , msg->hdr.number
	        , msg->from
	        , (char*)msg->to_net.addr);

	if ((i = smb_updatemsg(smb, msg)) != SMB_SUCCESS) {
		errprintf(LOG_ERR, WHERE, "%04d SEND !BOUNCE ERROR %d (%s) incrementing delivery attempt counter of message #%u"
		          , sock, i, smb->last_error, msg->hdr.number);
		return false;
	}

	if (!immediate && msg->hdr.delivery_attempts < startup->max_delivery_attempts)
		return true;

	newmsg = *msg;
	/* Mark original message as deleted */
	msg->hdr.attr |= MSG_DELETE;

	i = smb_updatemsg(smb, msg);
	if (msg->hdr.auxattr & MSG_FILEATTACH) {
		if (!delfattach(&scfg, msg))
			errprintf(LOG_ERR, WHERE,  "%04d SEND !ERROR %d deleting attachment: %s"
			          , sock, errno, msg->subj);
	}
	if (i != SMB_SUCCESS) {
		errprintf(LOG_ERR, WHERE, "%04d SEND !BOUNCE ERROR %d (%s) deleting message"
		          , sock, i, smb->last_error);
		return false;
	}

	if (msg->from_agent == AGENT_SMTPSYSMSG    /* don't bounce 'bounce messages' */
	    || (msg->hdr.attr & MSG_NOREPLY)
	    || (msg->idx.from == 0 && msg->from_net.type == NET_NONE)
	    || (msg->reverse_path != NULL && *msg->reverse_path == 0)) {
		lprintf(LOG_NOTICE, "%04d SEND !Deleted undeliverable message from %s", sock, msg->from);
		return true;
	}

	newmsg.hfield = NULL;
	newmsg.hfield_dat = NULL;
	newmsg.total_hfields = 0;
	newmsg.hdr.delivery_attempts = 0;
	msg->text_subtype = NULL;
	msg->text_charset = NULL;
	char* reverse_path = msg->reverse_path == NULL ? msg->from : msg->reverse_path;

	lprintf(LOG_NOTICE, "%04d SEND !Bouncing message back to %s", sock, reverse_path);

	SAFEPRINTF(str, "Delivery failure: %s", newmsg.subj);
	smb_hfield_str(&newmsg, SUBJECT, str);
	smb_hfield_str(&newmsg, RECIPIENT, reverse_path);
	if (msg->from_agent == AGENT_PERSON) {

		if (newmsg.from_ext != NULL) { /* Back to sender */
			smb_hfield_str(&newmsg, RECIPIENTEXT, newmsg.from_ext);
			newmsg.from_ext = NULL;   /* Clear the sender extension */
		}

		if ((newmsg.from_net.type == NET_QWK || newmsg.from_net.type == NET_INTERNET)
		    && newmsg.reverse_path != NULL) {
			smb_hfield(&newmsg, RECIPIENTNETTYPE, sizeof(newmsg.from_net.type), &newmsg.from_net.type);
			smb_hfield_str(&newmsg, RECIPIENTNETADDR, newmsg.reverse_path);
		}
	} else {
		smb_hfield(&newmsg, RECIPIENTAGENT, sizeof(msg->from_agent), &msg->from_agent);
	}
	newmsg.hdr.attr |= MSG_NOREPLY;
	newmsg.hdr.attr &= ~MSG_READ;
	if (scfg.sys_misc & SM_DELREADM)
		newmsg.hdr.attr |= MSG_KILLREAD;

	strcpy(str, "Mail Delivery Subsystem");
	smb_hfield_str(&newmsg, SENDER, str);
	smb_hfield(&newmsg, SENDERAGENT, sizeof(agent), &agent);
	smb_hfield_str(&newmsg, RFC822MSGID, get_msgid(&scfg, INVALID_SUB, &newmsg, msgid, sizeof(msgid)));

	/* Put error message in subject for now */
	if (msg->hdr.delivery_attempts > 1)
		SAFEPRINTF(attempts, "after %u attempts", msg->hdr.delivery_attempts);
	else
		attempts[0] = 0;
	SAFEPRINTF2(str, "%s reporting delivery failure of message %s"
	            , server_host_name(), attempts);
	smb_hfield_str(&newmsg, SMB_COMMENT, str);
	SAFEPRINTF2(str, "from %s to %s\r\n"
	            , msg->from
	            , (char*)msg->to_net.addr);
	smb_hfield_str(&newmsg, SMB_COMMENT, str);
	strcpy(str, "Reason:");
	smb_hfield_str(&newmsg, SMB_COMMENT, str);
	smb_hfield_str(&newmsg, SMB_COMMENT, err);
	smb_hfield_str(&newmsg, SMB_COMMENT, "\r\nOriginal message text follows:");

	if ((i = smb_addmsghdr(smb, &newmsg, smb_storage_mode(&scfg, smb))) != SMB_SUCCESS)
		errprintf(LOG_ERR, WHERE, "%04d SEND !BOUNCE ERROR %d (%s) adding message header"
		          , sock, i, smb->last_error);
	else {
		lprintf(LOG_NOTICE, "%04d SEND !Delivery failure notification (message #%u) created for %s"
		        , sock, newmsg.hdr.number, reverse_path);
		if ((i = smb_incmsg_dfields(smb, &newmsg, 1)) != SMB_SUCCESS)
			errprintf(LOG_ERR, WHERE, "%04d SEND !BOUNCE ERROR %d (%s) incrementing data allocation units"
			          , sock, i, smb->last_error);
	}

	newmsg.dfield = NULL;             /* Don't double-free the data fields */
	newmsg.hdr.total_dfields = 0;
	smb_freemsgmem(&newmsg);

	return true;
}

static int remove_msg_intransit(smb_t* smb, smbmsg_t* msg)
{
	int i;

	if ((i = smb_lockmsghdr(smb, msg)) != SMB_SUCCESS) {
		lprintf(LOG_WARNING, "0000 SEND !ERROR %d (%s) locking message header #%u"
		        , i, smb->last_error, msg->idx.number);
		return i;
	}
	msg->hdr.netattr &= ~NETMSG_INTRANSIT;
	i = smb_putmsghdr(smb, msg);
	smb_unlockmsghdr(smb, msg);

	if (i != 0)
		errprintf(LOG_ERR, WHERE, "0000 SEND !ERROR %d (%s) writing message header #%u"
		          , i, smb->last_error, msg->idx.number);

	return i;
}

void get_dns_server(char* dns_server, size_t len)
{
	str_list_t list;
	size_t     count;

	sprintf(dns_server, "%.*s", (int)len - 1, startup->dns_server);
	if (!IS_ALPHANUMERIC(dns_server[0])) {
		if ((list = getNameServerList()) != NULL) {
			if ((count = strListCount(list)) > 0) {
				sprintf(dns_server, "%.*s", (int)len, list[xp_random(count)]);
				lprintf(LOG_DEBUG, "0000 SEND using auto-detected DNS server address: %s"
				        , dns_server);
			}
			freeNameServerList(list);
		}
	}
}

static bool sendmail_open_socket(SOCKET *sock, CRYPT_SESSION *session)
{
	int         i;
	SOCKADDR_IN addr;

	if (*sock != INVALID_SOCKET)
		mail_close_socket(sock, session);

	if ((*sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET) {
		errprintf(LOG_ERR, WHERE, "0000 SEND !ERROR %d opening socket", SOCKET_ERRNO);
		return false;
	}
	mail_open_socket(*sock, (char*)"smtp|sendmail");

	if (startup->connect_timeout) {  /* Use non-blocking socket */
		long nbio = 1;
		if ((i = ioctlsocket(*sock, FIONBIO, (u_long *)&nbio)) != 0) {
			errprintf(LOG_ERR, WHERE, "%04d SEND !ERROR %d (%d) disabling blocking on socket"
			          , *sock, i, SOCKET_ERRNO);
			return false;
		}
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = startup->outgoing4.s_addr;
	addr.sin_family = AF_INET;

	i = bind(*sock, (struct sockaddr *)&addr, sizeof(addr));
	if (i != 0) {
		errprintf(LOG_ERR, WHERE, "%04d SEND !ERROR %d (%d) binding socket", *sock, i, SOCKET_ERRNO);
		return false;
	}
	return true;
}

static SOCKET sendmail_negotiate(CRYPT_SESSION *session, smb_t *smb, smbmsg_t *msg, const char *mx, const char *mx2, const char *server, link_list_t *failed_server_list, ushort port)
{
	int               i;
	int               tls_retry;
	SOCKET            sock = INVALID_SOCKET;
	list_node_t*      node;
	ulong             ip_addr;
	union xp_sockaddr server_addr;
	char              server_ip[INET6_ADDRSTRLEN];
	bool              success;
	bool              nodelay = true;
	ulong             nb = 0;
	int               status;
	char              buf[512];
	char              err[1024] = "UNKNOWN ERROR";

	for (tls_retry = 0; tls_retry < 2; tls_retry++) {
		if (!sendmail_open_socket(&sock, session))
			continue;

		success = false;
		for (i = 0; i < 2 && !success; i++) {
			if (i) {
				if (startup->options & MAIL_OPT_RELAY_TX || !mx2[0])
					break;
				lprintf(LOG_DEBUG, "%04d SEND reverting to second MX: %s", sock, mx2);
				server = mx2; /* Give second mx record a try */
			}

			lprintf(LOG_DEBUG, "%04d SEND resolving SMTP hostname: '%s'", sock, server);
			ip_addr = resolve_ip(server);
			if (ip_addr == INADDR_NONE) {
				SAFEPRINTF(err, "Error resolving hostname '%s'", server);
				lprintf(LOG_WARNING, "%04d SEND !Failure resolving hostname: '%s'", sock, server);
				continue;
			}

			memset(&server_addr, 0, sizeof(server_addr));
			server_addr.in.sin_addr.s_addr = ip_addr;
			server_addr.in.sin_family = AF_INET;
			server_addr.in.sin_port = htons(port);
			inet_addrtop(&server_addr, server_ip, sizeof(server_ip));

			if ((node = listFindNode(failed_server_list, &server_addr, sizeof(server_addr))) != NULL) {
				SAFEPRINTF4(err, "Error %d connecting to port %u on %s [%s]", node->tag, inet_addrport(&server_addr), server, server_ip);
				lprintf(LOG_INFO, "%04d SEND skipping failed SMTP server: %s", sock, err);
				continue;
			}

			if ((server == mx || server == mx2)
			    && ((ip_addr & 0xff) == 127 || ip_addr == 0)) {
				SAFEPRINTF2(err, "Bad IP address (%s) for MX server: %s"
				            , server_ip, server);
				lprintf(LOG_INFO, "%04d SEND %s", sock, err);
				continue;
			}

			lprintf(LOG_INFO, "%04d SEND connecting to port %u on %s [%s]"
			        , sock
			        , inet_addrport(&server_addr)
			        , server, server_ip);
			if ((i = nonblocking_connect(sock, (struct sockaddr *)&server_addr, xp_sockaddr_len(&server_addr), startup->connect_timeout)) != 0) {
				SAFEPRINTF2(err, "ERROR %d connecting to SMTP server: %s"
				            , i, server);
				lprintf(LOG_INFO, "%04d SEND !%s", sock, err);
				listAddNodeData(failed_server_list, &server_addr, sizeof(server_addr), i, NULL);
				continue;
			}

			lprintf(LOG_DEBUG, "%04d SEND connected to %s", sock, server);

			/* HELO */
			if (!sockgetrsp(sock, "SEND", *session, "220", buf, sizeof(buf))) {
				SAFEPRINTF3(err, badrsp_err, server, buf, "220");
				lprintf(LOG_INFO, "%04d SEND %s", sock, err);
				continue;
			}
			success = true;
		}
		if (!success) {  /* Failed to connect to an MX, so bounce */
			remove_msg_intransit(smb, msg);
			bounce(sock /* Should be zero? */, smb, msg, err, /* immediate: */ false);
			mail_close_socket(&sock, session);
			return INVALID_SOCKET;
		}

		sockprintf(sock, "SEND", *session, "EHLO %s", server_host_name());
		switch (sockgetrsp_opt(sock, "SEND", *session, "250", "STARTTLS", buf, sizeof(buf))) {
			case -1:
				if (startup->options & MAIL_OPT_RELAY_TX
				    && (startup->options & MAIL_OPT_RELAY_AUTH_MASK) != 0) {    /* Requires ESMTP */
					SAFEPRINTF3(err, badrsp_err, server, buf, "250");
					remove_msg_intransit(smb, msg);
					bounce(sock, smb, msg, err, /* immediate: */ buf[0] == '5');
					mail_close_socket(&sock, session);
					return INVALID_SOCKET;
				}
				sockprintf(sock, "SEND", *session, "HELO %s", server_host_name());
				if (!sockgetrsp(sock, "SEND", *session, "250", buf, sizeof(buf))) {
					SAFEPRINTF3(err, badrsp_err, server, buf, "250");
					remove_msg_intransit(smb, msg);
					bounce(sock, smb, msg, err, /* immediate: */ buf[0] == '5');
					mail_close_socket(&sock, session);
					return INVALID_SOCKET;
				}
				return sock;
			case 0:
				return sock;
			case 1:
				/* We NEVER bounce() because of TLS errors, so we don't need to set err */
				if (!tls_retry) {
					lprintf(LOG_DEBUG, "%04d SEND Starting TLS session", sock);
					if (!do_cryptInit(lprintf)) {
						continue;
					}
					const char* prot = "SEND/TLS";
					sockprintf(sock, prot, *session, "STARTTLS");
					if (sockgetrsp(sock, prot, *session, "220", buf, sizeof(buf))) {
						if ((status = cryptCreateSession(session, CRYPT_UNUSED, CRYPT_SESSION_TLS)) != CRYPT_OK) {
							GCESH(status, prot, sock, server, CRYPT_UNUSED, "creating TLS session");
							continue;
						}
						if ((status = cryptSetAttribute(*session, CRYPT_SESSINFO_TLS_OPTIONS, CRYPT_TLSOPTION_DISABLE_CERTVERIFY)) != CRYPT_OK) {
							cryptDestroySession(*session);
							*session = -1;
							GCESH(status, prot, sock, server, *session, "disabling certificate verification");
							continue;
						}
						if ((status = cryptSetAttribute(*session, CRYPT_OPTION_CERT_COMPLIANCELEVEL, CRYPT_COMPLIANCELEVEL_OBLIVIOUS)) != CRYPT_OK) {
							cryptDestroySession(*session);
							*session = -1;
							GCESH(status, prot, sock, server, *session, "setting certificate compliance level");
							continue;
						}
						if ((status = add_private_key(&scfg, lprintf, *session)) != CRYPT_OK) {
							cryptDestroySession(*session);
							*session = -1;
							GCESH(status, prot, sock, server, *session, "setting private key");
							continue;
						}
						nodelay = true;
						setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
						nb = 0;
						ioctlsocket(sock, FIONBIO, &nb);
						if ((status = cryptSetAttribute(*session, CRYPT_SESSINFO_NETWORKSOCKET, sock)) != CRYPT_OK) {
							GCESH(status, prot, sock, server, *session, "setting network socket");
							continue;
						}
						if ((status = cryptSetAttribute(*session, CRYPT_SESSINFO_ACTIVE, 1)) != CRYPT_OK) {
							GCESHL(status, prot, sock, server, LOG_WARNING, *session, "setting session active");
							continue;
						}
						if (startup->max_inactivity) {
							if ((status = cryptSetAttribute(*session, CRYPT_OPTION_NET_READTIMEOUT, startup->max_inactivity)) != CRYPT_OK) {
								GCESH(status, prot, sock, server, *session, "setting read timeout");
								continue;
							}
						}
						sockprintf(sock, prot, *session, "EHLO %s", server_host_name());
						if (!sockgetrsp(sock, prot, *session, "250", buf, sizeof(buf))) {
							SAFEPRINTF3(err, badrsp_err, server, buf, "220");
							lprintf(LOG_INFO, "%04d SEND/TLS %s", sock, err);
							continue;
						}
						lprintf(LOG_INFO, "%04d SEND TLS Session started successfully", sock);
					}
				}
				return sock;
		}
	}
	remove_msg_intransit(smb, msg);
	bounce(sock, smb, msg, err, /* immediate: */ false);
	mail_close_socket(&sock, session);
	return INVALID_SOCKET;
}

/* TODO: IPv6 etc. */
#ifdef __BORLANDC__
#pragma argsused
#endif
static void sendmail_thread(void* arg)
{
	int           i;
	char          to[128];
	char          mx[128];
	char          mx2[128];
	char          err[1024];
	char          buf[512];
	char          str[128];
	char          tmp[256];
	char          resp[512];
	char          toaddr[256];
	char          fromaddr[256];
	char          challenge[256];
	char          secret[64];
	char          md5_data[384];
	uchar         digest[MD5_DIGEST_SIZE];
	char          numeric_ip[16];
	char          domain_list[MAX_PATH + 1];
	char          dns_server[128];
	const char*   server;
	char*         msgtxt = NULL;
	char*         p;
	char*         tp;
	ushort        port;
	ulong         last_msg = 0;
	ulong         dns;
	ulong         lines;
	ulong         bytes;
	bool          first_cycle = true;
	SOCKET        sock = INVALID_SOCKET;
	time_t        last_scan = 0;
	smb_t         smb;
	smbmsg_t      msg;
	mail_t*       mail;
	uint32_t      msgs;
	uint32_t      u;
	size_t        len;
	bool          sending_locally = false;
	link_list_t   failed_server_list;
	CRYPT_SESSION session = -1;

	SetThreadName("sbbs/sendMail");
	thread_up(true /* setuid */);

	terminate_sendmail = false;

	lprintf(LOG_INFO, "SendMail thread started");

	memset(&msg, 0, sizeof(msg));
	memset(&smb, 0, sizeof(smb));

	listInit(&failed_server_list, /* flags: */ 0);

	while ((!terminated) && !terminate_sendmail) {
		YIELD();
		if (startup->options & MAIL_OPT_NO_SENDMAIL) {
			sem_trywait_block(&sendmail_wakeup_sem, 1000);
			continue;
		}

		if (active_sendmail != 0)
			active_sendmail = 0, update_clients();

		listFreeNodes(&failed_server_list);

		smb_close(&smb);

		if (sock != INVALID_SOCKET)
			mail_close_socket(&sock, &session);

		if (msgtxt != NULL) {
			smb_freemsgtxt(msgtxt);
			msgtxt = NULL;
		}

		smb_freemsgmem(&msg);

		/* Don't delay on first loop */
		if (first_cycle)
			first_cycle = false;
		else
			sem_trywait_block(&sendmail_wakeup_sem, startup->sem_chk_freq * 1000);

		SAFEPRINTF(smb.file, "%smail", scfg.data_dir);
		smb.retry_time = scfg.smb_retry_time;
		smb.subnum = INVALID_SUB;
		if ((i = smb_open(&smb)) != SMB_SUCCESS) {
			lprintf(LOG_DEBUG, "0000 SEND ERROR %d (%s) opening: %s", i, smb.last_error, smb.file);
			continue;
		}
		if ((i = smb_locksmbhdr(&smb)) != SMB_SUCCESS)
			continue;
		i = smb_getstatus(&smb);
		smb_unlocksmbhdr(&smb);
		if (i != 0)
			continue;
		if (smb.status.last_msg == last_msg && time(NULL) - last_scan < startup->rescan_frequency)
			continue;
		lprintf(LOG_DEBUG, "0000 SEND last_msg=%lu, smb.status.last_msg=%u, elapsed=%ld"
		        , last_msg, smb.status.last_msg, (long)(time(NULL) - last_scan));
		last_msg = smb.status.last_msg;
		last_scan = time(NULL);
		mail = loadmail(&smb, &msgs, /* to network */ 0, MAIL_YOUR, 0);
		for (u = 0; u < msgs && !smb_islocked(&smb); u++) {
			if (active_sendmail != 0)
				active_sendmail = 0, update_clients();

			if (terminated || terminate_sendmail)    /* server stopped */
				break;

			if (sock != INVALID_SOCKET)
				mail_close_socket(&sock, &session);

			if (msgtxt != NULL) {
				smb_freemsgtxt(msgtxt);
				msgtxt = NULL;
			}

			smb_freemsgmem(&msg);

			msg.hdr.number = mail[u].number;
			if ((i = smb_getmsgidx(&smb, &msg)) != SMB_SUCCESS) {
				errprintf(LOG_ERR, WHERE, "0000 !SEND ERROR %d (%s) getting message index #%u"
				          , i, smb.last_error, mail[u].number);
				break;
			}
			if ((i = smb_lockmsghdr(&smb, &msg)) != SMB_SUCCESS) {
				lprintf(LOG_WARNING, "0000 !SEND ERROR %d (%s) locking message header #%u"
				        , i, smb.last_error, msg.idx.number);
				continue;
			}
			if ((i = smb_getmsghdr(&smb, &msg)) != SMB_SUCCESS) {
				smb_unlockmsghdr(&smb, &msg);
				errprintf(LOG_ERR, WHERE, "0000 !SEND ERROR %d (%s) line %u, msg #%u"
				          , i, smb.last_error, __LINE__, msg.idx.number);
				continue;
			}
			if (msg.hdr.attr & MSG_DELETE || msg.to_net.type != NET_INTERNET || msg.to_net.addr == NULL
			    || (msg.hdr.netattr & NETMSG_SENT)) {
				smb_unlockmsghdr(&smb, &msg);
				continue;
			}
			if (msg.from_net.type == NET_INTERNET && msg.reverse_path != NULL)
				angle_bracket(fromaddr, sizeof(fromaddr), msg.reverse_path);
			else
				angle_bracket(fromaddr, sizeof(fromaddr), usermailaddr(&scfg, str, msg.from));

			char sender_info[512];
			if (msg.from_ext != NULL)
				SAFEPRINTF2(sender_info, "'%s' #%s", msg.from, msg.from_ext);
			else if (strstr(fromaddr, msg.from) == fromaddr + 1)
				SAFECOPY(sender_info, fromaddr);
			else
				SAFEPRINTF2(sender_info, "'%s' %s", msg.from, fromaddr);

			char rcpt_info[512];
			if (compare_addrs(msg.to, (char*)msg.to_net.addr) == 0)
				angle_bracket(rcpt_info, sizeof(rcpt_info), msg.to);
			else
				safe_snprintf(rcpt_info, sizeof(rcpt_info), "'%s' %s", msg.to, angle_bracket(tmp, sizeof(tmp), (char*)msg.to_net.addr));

			if (!(startup->options & MAIL_OPT_SEND_INTRANSIT) && msg.hdr.netattr & NETMSG_INTRANSIT) {
				smb_unlockmsghdr(&smb, &msg);
				lprintf(LOG_NOTICE, "0000 SEND Message #%u from %s to %s - in transit"
				        , msg.hdr.number, sender_info, rcpt_info);
				continue;
			}
			msg.hdr.netattr |= NETMSG_INTRANSIT;  /* Prevent another sendmail thread from sending this msg */
			smb_putmsghdr(&smb, &msg);
			smb_unlockmsghdr(&smb, &msg);

			active_sendmail = 1, update_clients();

			lprintf(LOG_INFO, "0000 SEND Message #%u (%u of %u) from %s to %s"
			        , msg.hdr.number, u + 1, msgs, sender_info, rcpt_info);
#ifdef _WIN32
			if (startup->outbound_sound[0] && !sound_muted(&scfg))
				PlaySound(startup->outbound_sound, NULL, SND_ASYNC | SND_FILENAME);
#endif

			lprintf(LOG_DEBUG, "0000 SEND getting message text");
			if ((msgtxt = smb_getmsgtxt(&smb, &msg, GETMSGTXT_ALL)) == NULL) {
				remove_msg_intransit(&smb, &msg);
				errprintf(LOG_ERR, WHERE, "0000 !SEND ERROR (%s) retrieving message text", smb.last_error);
				continue;
			}

			remove_ctrl_a(msgtxt, msgtxt);

			port = 0;
			mx2[0] = 0;

			sending_locally = false;
			/* Check if this is a local email ToDo */
			SAFECOPY(to, (char*)msg.to_net.addr);
			truncstr(to, "> ");

			p = strrchr(to, '@');
			if (p == NULL) {
				remove_msg_intransit(&smb, &msg);
				lprintf(LOG_WARNING, "0000 !SEND INVALID destination address: %s", to);
				SAFEPRINTF(err, "Invalid destination address: %s", to);
				bounce(0, &smb, &msg, err, /* immediate: */ true);
				continue;
			}
			p++;
			SAFEPRINTF(domain_list, "%sdomains.cfg", scfg.ctrl_dir);
			if (stricmp(p, scfg.sys_inetaddr) == 0
			    || stricmp(p, startup->host_name) == 0
			    || findstr(p, domain_list)) {
				/* This is a local message... no need to send to remote */
				port = startup->smtp_port;
				/* TODO: IPv6 */
				if (startup->outgoing4.s_addr == 0)
					server = "127.0.0.1";
				else {
					inet_ntop(AF_INET, &startup->outgoing4, numeric_ip, sizeof numeric_ip);
					server = numeric_ip;
				}
				sending_locally = true;
			}
			else {
				if (startup->options & MAIL_OPT_RELAY_TX) {
					server = startup->relay_server;
					port = startup->relay_port;
				} else {
					server = p;
					tp = strrchr(p, ':');  /* non-standard SMTP port */
					if (tp != NULL) {
						*tp = 0;
						port = atoi(tp + 1);
					}
					if (port == 0) {   /* No port specified, use MX look-up */
						get_dns_server(dns_server, sizeof(dns_server));
						if ((dns = resolve_ip(dns_server)) == INADDR_NONE) {
							remove_msg_intransit(&smb, &msg);
							lprintf(LOG_WARNING, "0000 !SEND INVALID DNS server address: %s"
							        , dns_server);
							continue;
						}
						lprintf(LOG_DEBUG, "0000 SEND getting MX records for %s from %s", p, dns_server);
						if ((i = dns_getmx(p, mx, mx2, INADDR_ANY, dns
						                   , startup->options & MAIL_OPT_USE_TCP_DNS ? true : false
						                   , TIMEOUT_THREAD_WAIT / 2)) != 0) {
							remove_msg_intransit(&smb, &msg);
							lprintf(LOG_WARNING, "0000 !SEND ERROR %d obtaining MX records for %s from %s"
							        , i, p, dns_server);
							SAFEPRINTF2(err, "Error %d obtaining MX record for %s", i, p);
							bounce(0, &smb, &msg, err, /* immediate: */ false);
							continue;
						}
						server = mx;
					}
				}
			}
			if (!port)
				port = IPPORT_SMTP;

			sock = sendmail_negotiate(&session, &smb, &msg, mx, mx2, server, &failed_server_list, port);
			if (sock == INVALID_SOCKET)
				continue;
			const char* prot = session >= 0 ? "SEND/TLS" : "SEND";

			/* AUTH */
			if (startup->options & MAIL_OPT_RELAY_TX
			    && (startup->options & MAIL_OPT_RELAY_AUTH_MASK) != 0 && !sending_locally) {

				if ((startup->options & MAIL_OPT_RELAY_AUTH_MASK) == MAIL_OPT_RELAY_AUTH_PLAIN) {
					/* Build the buffer: <username>\0<user-id>\0<password */
					len = safe_snprintf(buf, sizeof(buf), "%s%c%s%c%s"
					                    , startup->relay_user
					                    , 0
					                    , startup->relay_user
					                    , 0
					                    , startup->relay_pass);
					b64_encode(resp, sizeof(resp), buf, len);
					sockprintf(sock, prot, session, "AUTH PLAIN %s", resp);
				} else {
					switch (startup->options & MAIL_OPT_RELAY_AUTH_MASK) {
						case MAIL_OPT_RELAY_AUTH_LOGIN:
							p = (char *)"LOGIN";
							break;
						case MAIL_OPT_RELAY_AUTH_CRAM_MD5:
							p = (char *)"CRAM-MD5";
							break;
						default:
							p = (char *)"<unknown>";
							break;
					}
					sockprintf(sock, prot, session, "AUTH %s", p);
					if (!sockgetrsp(sock, prot, session, "334", buf, sizeof(buf))) {
						SAFEPRINTF3(err, badrsp_err, server, buf, "334 Username/Challenge");
						bounce(sock, &smb, &msg, err, /* immediate: */ buf[0] == '5');
						continue;
					}
					switch (startup->options & MAIL_OPT_RELAY_AUTH_MASK) {
						case MAIL_OPT_RELAY_AUTH_LOGIN:
							b64_encode(p = resp, sizeof(resp), startup->relay_user, strlen(startup->relay_user));
							break;
						case MAIL_OPT_RELAY_AUTH_CRAM_MD5:
							p = buf;
							FIND_WHITESPACE(p);
							SKIP_WHITESPACE(p);
							b64_decode(challenge, sizeof(challenge), p, 0);

							/* Calculate response */
							memset(secret, 0, sizeof(secret));
							SAFECOPY(secret, startup->relay_pass);
							for (i = 0; i < (int)sizeof(secret); i++)
								md5_data[i] = secret[i] ^ 0x36; /* ipad */
							strcpy(md5_data + i, challenge);
							MD5_calc(digest, md5_data, sizeof(secret) + strlen(challenge));
							for (i = 0; i < (int)sizeof(secret); i++)
								md5_data[i] = secret[i] ^ 0x5c; /* opad */
							memcpy(md5_data + i, digest, sizeof(digest));
							MD5_calc(digest, md5_data, sizeof(secret) + sizeof(digest));
							safe_snprintf(buf, sizeof(buf), "%s %s", startup->relay_user, MD5_hex(str, digest));
							b64_encode(p = resp, sizeof(resp), buf, strlen(buf));
							break;
						default:
							p = (char *)"<unknown>";
							break;
					}
					sockprintf(sock, prot, session, "%s", p);
					if ((startup->options & MAIL_OPT_RELAY_AUTH_MASK) != MAIL_OPT_RELAY_AUTH_CRAM_MD5) {
						if (!sockgetrsp(sock, prot, session, "334", buf, sizeof(buf))) {
							SAFEPRINTF3(err, badrsp_err, server, buf, "334 Password");
							bounce(sock, &smb, &msg, err, /* immediate: */ buf[0] == '5');
							continue;
						}
						switch (startup->options & MAIL_OPT_RELAY_AUTH_MASK) {
							case MAIL_OPT_RELAY_AUTH_LOGIN:
								b64_encode(p = buf, sizeof(buf), startup->relay_pass, strlen(startup->relay_pass));
								break;
							default:
								p = (char *)"<unknown>";
								break;
						}
						sockprintf(sock, prot, session, "%s", p);
					}
				}
				if (!sockgetrsp(sock, prot, session, "235", buf, sizeof(buf))) {
					SAFEPRINTF3(err, badrsp_err, server, buf, "235");
					bounce(sock, &smb, &msg, err, /* immediate: */ buf[0] == '5');
					continue;
				}
			}

			/* MAIL */
			sockprintf(sock, prot, session, "MAIL FROM:%s", fromaddr);
			if (!sockgetrsp(sock, prot, session, "250", buf, sizeof(buf))) {
				remove_msg_intransit(&smb, &msg);
				SAFEPRINTF3(err, badrsp_err, server, buf, "250");
				bounce(sock, &smb, &msg, err, /* immediate: */ buf[0] == '5');
				continue;
			}
			/* RCPT */
			if (msg.forward_path != NULL) {
				SAFECOPY(toaddr, msg.forward_path);
			} else {
				if ((p = strrchr((char*)msg.to_net.addr, '<')) != NULL)
					p++;
				else
					p = (char*)msg.to_net.addr;
				SAFECOPY(toaddr, p);
				truncstr(toaddr, "> ");
				if ((p = strrchr(toaddr, '@')) != NULL && (tp = strrchr(toaddr, ':')) != NULL
				    && tp > p)
					*tp = 0; /* Remove ":port" designation from envelope */
			}
			sockprintf(sock, prot, session, "RCPT TO:%s", angle_bracket(tmp, sizeof(tmp), toaddr));
			if (!sockgetrsp(sock, prot, session, "25", buf, sizeof(buf))) {
				remove_msg_intransit(&smb, &msg);
				SAFEPRINTF3(err, badrsp_err, server, buf, "25*");
				bounce(sock, &smb, &msg, err, /* immediate: */ buf[0] == '5');
				continue;
			}
			/* DATA */
			sockprintf(sock, prot, session, "DATA");
			if (!sockgetrsp(sock, prot, session, "354", buf, sizeof(buf))) {
				remove_msg_intransit(&smb, &msg);
				SAFEPRINTF3(err, badrsp_err, server, buf, "354");
				bounce(sock, &smb, &msg, err, /* immediate: */ buf[0] == '5');
				continue;
			}
			bytes = strlen(msgtxt);
			lprintf(LOG_DEBUG, "%04d %-5s sending message text (%lu bytes) begin"
			        , sock, prot, bytes);
			lines = sockmsgtxt(sock, prot, session, &msg, msgtxt, /* max_lines: */ -1);
			lprintf(LOG_DEBUG, "%04d %-5s send of message text (%lu bytes, %lu lines) complete, waiting for acknowledgment (250)"
			        , sock, prot, bytes, lines);
			if (!sockgetrsp(sock, prot, session, "250", buf, sizeof(buf))) {
				/* Wait doublely-long for the acknowledgment */
				if (buf[0] || !sockgetrsp(sock, prot, session, "250", buf, sizeof(buf))) {
					remove_msg_intransit(&smb, &msg);
					SAFEPRINTF3(err, badrsp_err, server, buf, "250");
					bounce(sock, &smb, &msg, err, /* immediate: */ buf[0] == '5');
					continue;
				}
			}
			lprintf(LOG_INFO, "%04d %-5s Successfully sent message #%u (%lu bytes, %lu lines) from %s to '%s' %s"
			        , sock, prot, msg.hdr.number, bytes, lines, sender_info, msg.to, toaddr);

			/* Now lets mark this message for deletion without corrupting the index */
			if ((msg.hdr.netattr & NETMSG_KILLSENT) || msg.from_ext == NULL)
				msg.hdr.attr |= MSG_DELETE;
			msg.hdr.netattr |= NETMSG_SENT;
			msg.hdr.netattr &= ~NETMSG_INTRANSIT;
			if ((i = smb_updatemsg(&smb, &msg)) != SMB_SUCCESS)
				errprintf(LOG_ERR, WHERE, "%04d %-5s !ERROR %d (%s) deleting message #%u"
				          , sock, prot, i, smb.last_error, msg.hdr.number);
			if (msg.hdr.auxattr & MSG_FILEATTACH) {
				if (!delfattach(&scfg, &msg))
					errprintf(LOG_ERR, WHERE,  "%04d %-5s !ERROR %d deleting attachment: %s"
					          , sock, prot, errno, msg.subj);
			}

			/* QUIT */
			sockprintf(sock, prot, session, "QUIT");
			sockgetrsp(sock, prot, session, "221", buf, sizeof(buf));
			mail_close_socket(&sock, &session);

			if (msg.from_agent == AGENT_PERSON && !(startup->options & MAIL_OPT_NO_AUTO_EXEMPT))
				exempt_email_addr("SEND Auto-exempting", sender_info, toaddr);
		}
		/* Free up resources here */
		if (mail != NULL)
			freemail(mail);
	}
	if (sock != INVALID_SOCKET)
		mail_close_socket(&sock, &session);

	listFree(&failed_server_list);

	smb_freemsgtxt(msgtxt);
	smb_freemsgmem(&msg);
	smb_close(&smb);

	if (active_sendmail != 0)
		active_sendmail = 0, update_clients();

	{
		int32_t remain = thread_down();
		lprintf(LOG_DEBUG, "0000 SendMail thread terminated (%u threads remain)", remain);
	}

	sendmail_running = false;
}

void mail_terminate(void)
{
	lprintf(LOG_INFO, "Mail Server terminate");
	terminate_server = true;
}

static void cleanup(int code)
{
	int i;

	if (protected_uint32_value(thread_count) > 1) {
		lprintf(LOG_INFO, "0000 Waiting for %d child threads to terminate", protected_uint32_value(thread_count) - 1);
		while (protected_uint32_value(thread_count) > 1) {
			mswait(100);
		}
		lprintf(LOG_INFO, "0000 Done waiting for child threads to terminate");
	}
	free_cfg(&scfg);

	semfile_list_free(&pause_semfiles);
	semfile_list_free(&recycle_semfiles);
	semfile_list_free(&shutdown_semfiles);

	if (mailproc_list != NULL) {
		for (i = 0; i < mailproc_count; i++) {
			if (mailproc_list[i].ar != NULL)
				free(mailproc_list[i].ar);
			strListFree(&mailproc_list[i].to);
			strListFree(&mailproc_list[i].from);
		}
		FREE_AND_NULL(mailproc_list);
	}

	/* Check a if(mail_set!=NULL) check be performed here? */
	xpms_destroy(mail_set, mail_close_socket_cb, NULL);
	mail_set = NULL;
	terminated = true;

	if (savemsg_mutex_created && pthread_mutex_destroy(&savemsg_mutex))
		lprintf(LOG_CRIT, "!!!! Failed to destroy savemsg_mutex, system is unstable");
	savemsg_mutex_created = false;

	update_clients();   /* active_clients is destroyed below */

	listFree(&current_logins);
	listFree(&current_connections);

	if (protected_uint32_value(active_clients))
		lprintf(LOG_WARNING, "!!!! Terminating with %d active clients", protected_uint32_value(active_clients));
	else
		protected_uint32_destroy(active_clients);

	delete ip_can, ip_can = nullptr;
	delete ip_silent_can, ip_silent_can = nullptr;
	delete host_can, host_can = nullptr;

#ifdef _WINSOCKAPI_
	if (WSAInitialized && WSACleanup() != 0)
		lprintf(LOG_ERR, "0000 !WSACleanup ERROR %d", SOCKET_ERRNO);
#endif
	thread_down();
	if (terminate_server || code) {
		char str[1024];
		sprintf(str, "%lu connections served", stats.connections_served);
		if (stats.connections_exceeded)
			sprintf(str + strlen(str), ", %lu exceeded max", stats.connections_exceeded);
		if (stats.connections_refused)
			sprintf(str + strlen(str), ", %lu refused", stats.connections_refused);
		if (stats.connections_ignored)
			sprintf(str + strlen(str), ", %lu ignored", stats.connections_refused);
		if (stats.sessions_refused)
			sprintf(str + strlen(str), ", %lu sessions refused", stats.sessions_refused);
		sprintf(str + strlen(str), ", %lu messages received", stats.msgs_received);
		if (stats.msgs_refused)
			sprintf(str + strlen(str), ", %lu refused", stats.msgs_refused);
		if (stats.msgs_ignored)
			sprintf(str + strlen(str), ", %lu ignored", stats.msgs_ignored);
		if (stats.errors)
			sprintf(str + strlen(str), ", %lu errors", stats.errors);
		if (stats.crit_errors)
			sprintf(str + strlen(str), ", %lu critical", stats.crit_errors);
		if (client_highwater > 1)
			sprintf(str + strlen(str), ", %u concurrent clients", client_highwater);

		lprintf(LOG_INFO, "#### Mail Server thread terminated (%s)", str);
		set_state(SERVER_STOPPED);
		if (startup != NULL && startup->terminated != NULL)
			startup->terminated(startup->cbdata, code);
	}
	mqtt_shutdown(&mqtt);
}

const char* mail_ver(void)
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
	        , GIT_DATE, compiler
	        );

	return ver;
}

void mail_server(void* arg)
{
	char*             p;
	char              path[MAX_PATH + 1];
	char              mailproc_ini[MAX_PATH + 1];
	char              str[256];
	char              error[256];
	char              compiler[32];
	union xp_sockaddr client_addr;
	socklen_t         client_addr_len;
	char              host_ip[INET6_ADDRSTRLEN];
	SOCKET            client_socket;
	int               i;
	ulong             l;
	time_t            t;
	time_t            start;
	time_t            initialized = 0;
	pop3_t*           pop3;
	smtp_t*           smtp;
	FILE*             fp;
	str_list_t        sec_list;
	const char*       servprot = "N/A";
	CRYPT_SESSION     session = -1;

	startup = (mail_startup_t*)arg;

#ifdef _THREAD_SUID_BROKEN
	if (thread_suid_broken)
		startup->seteuid(true);
#endif

	if (startup == NULL) {
		sbbs_beep(100, 500);
		fprintf(stderr, "No startup structure passed!\n");
		return;
	}

	if (startup->size != sizeof(mail_startup_t)) { /* verify size */
		sbbs_beep(100, 500);
		sbbs_beep(300, 500);
		sbbs_beep(100, 500);
		fprintf(stderr, "Invalid startup structure!\n");
		return;
	}

	set_state(SERVER_INIT);

	ZERO_VAR(js_server_props);
	SAFEPRINTF3(js_server_props.version, "%s %s%c", server_name, VERSION, REVISION);
	js_server_props.version_detail = mail_ver();
	js_server_props.clients = &active_clients;
	js_server_props.options = &startup->options;
	js_server_props.interfaces = &startup->interfaces;

	uptime = 0;
	memset(&stats, 0, sizeof(stats));
	startup->recycle_now = false;
	startup->shutdown_now = false;
	terminate_server = false;

	SetThreadName("sbbs/mailServer");
	protected_uint32_init(&thread_count, 0);

	do {
		listInit(&current_logins, LINK_LIST_MUTEX);
		listInit(&current_connections, LINK_LIST_MUTEX);
		protected_uint32_init(&active_clients, 0);

		/* Setup intelligent defaults */
		if (startup->relay_port == 0)
			startup->relay_port = IPPORT_SMTP;
		if (startup->submission_port == 0)
			startup->submission_port = IPPORT_SUBMISSION;
		if (startup->submissions_port == 0)
			startup->submissions_port = IPPORT_SUBMISSIONS;
		if (startup->smtp_port == 0)
			startup->smtp_port = IPPORT_SMTP;
		if (startup->pop3_port == 0)
			startup->pop3_port = IPPORT_POP3;
		if (startup->pop3s_port == 0)
			startup->pop3s_port = IPPORT_POP3S;
		if (startup->rescan_frequency == 0)
			startup->rescan_frequency = MAIL_DEFAULT_RESCAN_FREQUENCY;
		if (startup->max_delivery_attempts == 0)
			startup->max_delivery_attempts = MAIL_DEFAULT_MAX_DELIVERY_ATTEMPTS;
		if (startup->max_inactivity == 0)
			startup->max_inactivity = MAIL_DEFAULT_MAX_INACTIVITY;                                        /* seconds */
		if (startup->sem_chk_freq == 0)
			startup->sem_chk_freq = DEFAULT_SEM_CHK_FREQ;
		if (startup->js.max_bytes == 0)
			startup->js.max_bytes = JAVASCRIPT_MAX_BYTES;

		(void)protected_uint32_adjust(&thread_count, 1);
		thread_up(false /* setuid */);

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

		sbbs_srand();

		if (!winsock_startup()) {
			cleanup(1);
			return;
		}

		t = time(NULL);
		lprintf(LOG_INFO, "Initializing on %.24s with options: %x"
		        , ctime_r(&t, str), startup->options);

		if (chdir(startup->ctrl_dir) != 0)
			lprintf(LOG_ERR, "!ERROR %d (%s) changing directory to: %s", errno, strerror(errno), startup->ctrl_dir);

		if ((startup->options & MAIL_OPT_RELAY_TX) && startup->relay_server[0] == '\0') {
			lprintf(LOG_ERR, "SMTP-Transmit-Relay enabled, but no relay server (hostname or IP address) configured. Disabling.");
			startup->options &= ~MAIL_OPT_RELAY_TX;
		}

		/* Initial configuration and load from CNF files */
		SAFECOPY(scfg.ctrl_dir, startup->ctrl_dir);
		lprintf(LOG_INFO, "Loading configuration files from %s", scfg.ctrl_dir);
		scfg.size = sizeof(scfg);
		SAFECOPY(error, UNKNOWN_LOAD_ERROR);
		if (!load_cfg(&scfg, text, TOTAL_TEXT, /* prep: */ true, /* node: */ false, error, sizeof(error))) {
			lprintf(LOG_CRIT, "!ERROR loading configuration files: %s", error);
			cleanup(1);
			return;
		}
		if (error[0] != '\0')
			lprintf(LOG_WARNING, "!WARNING loading configuration files: %s", error);


		mqtt_startup(&mqtt, &scfg, (struct startup*)startup, mail_ver(), lputs);

		if (startup->temp_dir[0])
			SAFECOPY(scfg.temp_dir, startup->temp_dir);
		else
			SAFECOPY(scfg.temp_dir, "../temp");
		prep_dir(scfg.ctrl_dir, scfg.temp_dir, sizeof(scfg.temp_dir));
		if ((i = md(scfg.temp_dir)) != 0) {
			lprintf(LOG_CRIT, "!ERROR %d (%s) creating directory: %s", i, strerror(i), scfg.temp_dir);
			cleanup(1);
			return;
		}
		lprintf(LOG_DEBUG, "Temporary file directory: %s", scfg.temp_dir);

		/* Parse the mailproc[.host].ini */
		mailproc_list = NULL;
		mailproc_count = 0;
		iniFileName(mailproc_ini, sizeof(mailproc_ini), scfg.ctrl_dir, "mailproc.ini");
		if ((fp = iniOpenFile(mailproc_ini, /* for_modify: */ false)) != NULL) {
			lprintf(LOG_DEBUG, "Reading %s", mailproc_ini);
			sec_list = iniReadSectionList(fp, /* prefix */ NULL);
			if ((mailproc_count = strListCount(sec_list)) != 0
			    && (mailproc_list = static_cast<mailproc *>(malloc(mailproc_count * sizeof(struct mailproc)))) != NULL) {
				char buf[INI_MAX_VALUE_LEN + 1];
				for (i = 0; i < mailproc_count; i++) {
					memset(&mailproc_list[i], 0, sizeof(struct mailproc));
					SAFECOPY(mailproc_list[i].name, sec_list[i]);
					SAFECOPY(mailproc_list[i].cmdline,
					         iniReadString(fp, sec_list[i], "Command", sec_list[i], buf));
					SAFECOPY(mailproc_list[i].eval,
					         iniReadString(fp, sec_list[i], "Eval", "", buf));
					mailproc_list[i].to =
						iniReadStringList(fp, sec_list[i], "To", ",", NULL);
					mailproc_list[i].from =
						iniReadStringList(fp, sec_list[i], "From", ",", NULL);
					mailproc_list[i].passthru =
						iniReadBool(fp, sec_list[i], "PassThru", true);
					mailproc_list[i].native =
						iniReadBool(fp, sec_list[i], "Native", false);
					mailproc_list[i].disabled =
						iniReadBool(fp, sec_list[i], "Disabled", false);
					mailproc_list[i].ignore_on_error =
						iniReadBool(fp, sec_list[i], "IgnoreOnError", false);
					mailproc_list[i].process_spam =
						iniReadBool(fp, sec_list[i], "ProcessSPAM", true);
					mailproc_list[i].process_dnsbl =
						iniReadBool(fp, sec_list[i], "ProcessDNSBL", true);
					mailproc_list[i].ar =
						arstr(NULL, iniReadString(fp, sec_list[i], "AccessRequirements", "", buf), &scfg, NULL);
				}
			}
			iniFreeStringList(sec_list);
			iniCloseFile(fp);
		}

		if ((t = checktime()) != 0) {   /* Check binary time */
			errprintf(LOG_ERR, WHERE, "!TIME PROBLEM (%" PRId64 ")", (int64_t)t);
		}

		if (uptime == 0)
			uptime = time(NULL); /* this must be done *after* setting the timezone */

		if (startup->max_clients == 0) {
			startup->max_clients = scfg.sys_nodes;
			if (startup->max_clients < 10)
				startup->max_clients = 10;
		}

		lprintf(LOG_DEBUG, "Maximum clients: %u", startup->max_clients);

		lprintf(LOG_DEBUG, "Maximum inactivity: %u seconds", startup->max_inactivity);

		update_clients();
		if (!ssl_sync(&scfg, lprintf)) {
			lprintf(LOG_CRIT, "!ssl_sync() failure trying to enable TLS support");
			startup->options &= ~(MAIL_OPT_TLS_POP3 | MAIL_OPT_TLS_SUBMISSION);
		}

		/* open a socket and wait for a client */
		mail_set = xpms_create(startup->bind_retry_count, startup->bind_retry_delay, lprintf);
		if (mail_set == NULL) {
			lprintf(LOG_CRIT, "!ERROR creating mail server socket set");
			cleanup(1);
			return;
		}
		terminated = false;
		if (!xpms_add_list(mail_set, PF_UNSPEC, SOCK_STREAM, 0, startup->interfaces
		                   , startup->smtp_port, "SMTP Transfer Agent", &terminate_server, mail_open_socket, startup->seteuid, (void*)servprot_smtp))
			lprintf(LOG_INFO, "SMTP No extra interfaces listening");

		if (startup->options & MAIL_OPT_USE_SUBMISSION_PORT) {
			xpms_add_list(mail_set, PF_UNSPEC, SOCK_STREAM, 0, startup->interfaces
			              , startup->submission_port, "SMTP Submission Agent", &terminate_server, mail_open_socket, startup->seteuid, (void*)servprot_submission);
		}

		if (startup->options & MAIL_OPT_TLS_SUBMISSION) {
			xpms_add_list(mail_set, PF_UNSPEC, SOCK_STREAM, 0, startup->interfaces, startup->submissions_port
			              , "SMTPS Submission Agent", &terminate_server, mail_open_socket, startup->seteuid, (void*)servprot_submissions);
		}

		if (startup->options & MAIL_OPT_ALLOW_POP3) {
			/* open a socket and wait for a client */
			if (!xpms_add_list(mail_set, PF_UNSPEC, SOCK_STREAM, 0, startup->pop3_interfaces, startup->pop3_port
			                   , "POP3 Server", &terminate_server, mail_open_socket, startup->seteuid, (void*)servprot_pop3))
				lprintf(LOG_INFO, "POP3 No extra interfaces listening");
		}

		if (startup->options & MAIL_OPT_TLS_POP3) {
			/* open a socket and wait for a client */
			if (!xpms_add_list(mail_set, PF_UNSPEC, SOCK_STREAM, 0, startup->pop3_interfaces
			                   , startup->pop3s_port, "POP3S Server", &terminate_server, mail_open_socket, startup->seteuid, (void*)servprot_pop3s))
				lprintf(LOG_INFO, "POP3S No extra interfaces listening");
		}

		ip_can = new trashCan(&scfg, "ip");
		ip_silent_can = new trashCan(&scfg, "ip-silent");
		host_can = new trashCan(&scfg, "host");

		sem_init(&sendmail_wakeup_sem, 0, 0);

		if (!(startup->options & MAIL_OPT_NO_SENDMAIL)) {
			sendmail_running = true;
			(void)protected_uint32_adjust(&thread_count, 1);
			_beginthread(sendmail_thread, 0, NULL);
		}

		/* Setup recycle/shutdown semaphore file lists */
		shutdown_semfiles = semfile_list_init(scfg.ctrl_dir, "shutdown", server_abbrev);
		pause_semfiles = semfile_list_init(scfg.ctrl_dir, "pause", server_abbrev);
		recycle_semfiles = semfile_list_init(scfg.ctrl_dir, "recycle", server_abbrev);
		semfile_list_add(&recycle_semfiles, startup->ini_fname);
		SAFEPRINTF(path, "%smailsrvr.rec", scfg.ctrl_dir);    /* legacy */
		semfile_list_add(&recycle_semfiles, path);
		semfile_list_add(&recycle_semfiles, mailproc_ini);
		if (!initialized) {
			semfile_list_check(&initialized, recycle_semfiles);
			semfile_list_check(&initialized, shutdown_semfiles);
		}

		pthread_mutex_init(&savemsg_mutex, NULL);
		savemsg_mutex_created = true;

		lprintf(LOG_INFO, "Mail Server thread started");
		mqtt_client_max(&mqtt, startup->max_clients);

		while (!terminated && !terminate_server) {
			YIELD();

			if (!(startup->options & MAIL_OPT_NO_RECYCLE)
			    && protected_uint32_value(thread_count) <= (unsigned int)(1 + (sendmail_running?1:0))) {
				if ((p = semfile_list_check(&initialized, recycle_semfiles)) != NULL) {
					lprintf(LOG_INFO, "Recycle semaphore file (%s) detected", p);
					break;
				}
				if (startup->recycle_now == true) {
					lprintf(LOG_NOTICE, "Recycle semaphore signaled");
					startup->recycle_now = false;
					break;
				}
			}
			if (((p = semfile_list_check(&initialized, shutdown_semfiles)) != NULL
			     && lprintf(LOG_INFO, "Shutdown semaphore file (%s) detected", p))
			    || (startup->shutdown_now == true
			        && lprintf(LOG_INFO, "Shutdown semaphore signaled"))) {
				startup->shutdown_now = false;
				terminate_server = true;
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
			/* signal caller that we've started up successfully */
			set_state(SERVER_READY);


			/* now wait for connection */
			client_addr_len = sizeof(client_addr);
			client_socket = xpms_accept(mail_set, &client_addr, &client_addr_len, startup->sem_chk_freq * 1000, XPMS_FLAGS_NONE, (void**)&servprot);
			if (client_socket != INVALID_SOCKET) {
				bool is_smtp = (servprot != servprot_pop3 && servprot != servprot_pop3s);
				if (startup->socket_open != NULL)
					startup->socket_open(startup->cbdata, true);
				stats.sockets++;

				inet_addrtop(&client_addr, host_ip, sizeof(host_ip));

				if (startup->max_concurrent_connections > 0) {
					int ip_len = strlen(host_ip) + 1;
					int connections = listCountMatches(&current_connections, host_ip, ip_len);
					int logins = listCountMatches(&current_logins, host_ip, ip_len);

					if (connections - logins >= (int)startup->max_concurrent_connections
					    && !is_host_exempt(&scfg, host_ip, /* host_name */ NULL)) {
						lprintf(LOG_NOTICE, "%04d %-5s [%s] !Maximum concurrent connections without login (%u) exceeded (%lu total)"
						        , client_socket, servprot, host_ip, startup->max_concurrent_connections, ++stats.connections_exceeded);
						sockprintf(client_socket, servprot, session, is_smtp ? smtp_error : pop_error, "Maximum connections exceeded");
						mail_close_socket(&client_socket, &session);
						continue;
					}
				}

				if (ip_silent_can->listed(host_ip)) {
					mail_close_socket(&client_socket, &session);
					stats.connections_ignored++;
					continue;
				}

				if (protected_uint32_value(active_clients) >= startup->max_clients) {
					lprintf(LOG_WARNING, "%04d %-5s [%s] !MAXIMUM CLIENTS (%u) reached, access denied (%lu total)"
					        , client_socket, servprot, host_ip, startup->max_clients, ++stats.connections_refused);
					sockprintf(client_socket, servprot, session, is_smtp ? smtp_error : pop_error, "Maximum active clients reached");
					mail_close_socket(&client_socket, &session);
					continue;
				}

				l = 1;

				if ((i = ioctlsocket(client_socket, FIONBIO, &l)) != 0) {
					errprintf(LOG_CRIT, WHERE, "%04d %-5s [%s] !ERROR %d (%d) disabling blocking on socket"
					          , client_socket, servprot, host_ip, i, SOCKET_ERRNO);
					sockprintf(client_socket, servprot, session, is_smtp ? smtp_error : pop_error, "ioctlsocket error");
					mswait(3000);
					mail_close_socket(&client_socket, &session);
					continue;
				}

				if (is_smtp) {
					if ((smtp = static_cast<smtp_t *>(malloc(sizeof(smtp_t)))) == NULL) {
						errprintf(LOG_CRIT, WHERE, "%04d %-5s !ERROR allocating %lu bytes of memory for smtp_t"
						          , client_socket, servprot, (ulong)sizeof(smtp_t));
						sockprintf(client_socket, servprot, session, smtp_error, "malloc failure");
						mail_close_socket(&client_socket, &session);
						continue;
					}

					smtp->session = -1;
					smtp->socket = client_socket;
					memcpy(&smtp->client_addr, &client_addr, client_addr_len);
					smtp->client_addr_len = client_addr_len;
					smtp->tls_port = (servprot == servprot_submissions);
					(void)protected_uint32_adjust(&thread_count, 1);
					_beginthread(smtp_thread, 0, smtp);
					stats.connections_served++;
				}
				else {
					if ((pop3 = static_cast<pop3_t *>(malloc(sizeof(pop3_t)))) == NULL) {
						errprintf(LOG_CRIT, WHERE, "%04d %-5s !ERROR allocating %lu bytes of memory for pop3_t"
						          , client_socket, servprot, (ulong)sizeof(pop3_t));
						sockprintf(client_socket, "POP3", session, "-ERR System error, please try again later.");
						mswait(3000);
						mail_close_socket(&client_socket, &session);
						continue;
					}

					pop3->session = -1;
					pop3->socket = client_socket;
					memcpy(&pop3->client_addr, &client_addr, client_addr_len);
					pop3->client_addr_len = client_addr_len;
					pop3->tls_port = (servprot == servprot_pop3s);
					(void)protected_uint32_adjust(&thread_count, 1);
					_beginthread(pop3_thread, 0, pop3);
					stats.connections_served++;
				}
			}
		}

		set_state(terminate_server ? SERVER_STOPPING : SERVER_RELOADING);

		if (protected_uint32_value(active_clients)) {
			lprintf(LOG_INFO, "Waiting for %d active clients to disconnect..."
			        , protected_uint32_value(active_clients));
			start = time(NULL);
			while (protected_uint32_value(active_clients)) {
				if (startup->max_inactivity && time(NULL) - start > startup->max_inactivity) {
					lprintf(LOG_WARNING, "!TIMEOUT (%u seconds) waiting for %d active clients"
					        , startup->max_inactivity, protected_uint32_value(active_clients));
					break;
				}
				mswait(100);
			}
			lprintf(LOG_INFO, "Done waiting for active clients to disconnect");
		}

		if (sendmail_running) {
			terminate_sendmail = true;
			sem_post(&sendmail_wakeup_sem);
			mswait(100);
		}
		if (sendmail_running) {
			lprintf(LOG_INFO, "Waiting for SendMail thread to terminate...");
			start = time(NULL);
			while (sendmail_running) {
				if (time(NULL) - start > TIMEOUT_THREAD_WAIT) {
					lprintf(LOG_WARNING, "!TIMEOUT waiting for SendMail thread to terminate");
					break;
				}
				mswait(500);
			}
			lprintf(LOG_INFO, "Done waiting for SendMail thread to terminate");
		}
		if (!sendmail_running) {
			if (sem_destroy(&sendmail_wakeup_sem) == -1)
				lprintf(LOG_CRIT, "!!!! Failed to destroy sendmail_wakeup_sem (%d), system is unstable", errno);
		}

		if (!terminate_server) {
			lprintf(LOG_INFO, "Recycling server...");
			if (startup->recycle != NULL)
				startup->recycle(startup->cbdata);
		}
		cleanup(0);

	} while (!terminate_server);

	protected_uint32_destroy(thread_count);
}
