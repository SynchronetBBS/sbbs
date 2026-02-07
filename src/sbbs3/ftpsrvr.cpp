/* Synchronet FTP server */

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
#include <stdio.h>
#include <stdlib.h>         /* ltoa in GNU C lib */
#include <stdarg.h>         /* va_list, varargs */
#include <string.h>         /* strrchr */
#include <fcntl.h>          /* O_WRONLY, O_RDONLY, etc. */
#include <errno.h>          /* EACCES */
#include <ctype.h>          /* toupper */
#include <sys/types.h>
#include <sys/stat.h>

/* Synchronet-specific headers */
#undef SBBS /* this shouldn't be defined unless building sbbs.dll/libsbbs.so */
#include "sbbs.h"
#include "text.h"           /* TOTAL_TEXT */
#include "ftpsrvr.h"
#include "filedat.h"
#include "telnet.h"
#include "multisock.h"
#include "ssl.h"
#include "cryptlib.h"
#include "xpprintf.h"       // vasprintf
#include "md5.h"
#include "sauce.h"
#include "filterfile.hpp"
#include "ratelimit.hpp"
#include "git_branch.h"
#include "git_hash.h"

/* Constants */

#define FTP_SERVER              "Synchronet FTP Server"
static const char* server_abbrev = "ftp";

#define ANONYMOUS               "anonymous"

#define BBS_VIRTUAL_PATH        "bbs:/""/"  /* this is actually bbs:<slash><slash> */
#define LOCAL_FSYS_DIR          "local:"
#define BBS_FSYS_DIR            "bbs:"
#define BBS_HIDDEN_ALIAS        "hidden"

#define TIMEOUT_THREAD_WAIT     60      /* Seconds */

#define TIMEOUT_SOCKET_LISTEN   30      /* Seconds */

#define XFER_REPORT_INTERVAL    60      /* Seconds */

#define INDEX_FNAME_LEN         15

#define NAME_LEN                15      /* User name length for listings */

#define MLSX_TYPE   (1 << 0)
#define MLSX_PERM   (1 << 1)
#define MLSX_SIZE   (1 << 2)
#define MLSX_MODIFY (1 << 3)
#define MLSX_OWNER  (1 << 4)
#define MLSX_UNIQUE (1 << 5)
#define MLSX_CREATE (1 << 6)

static ftp_startup_t*     startup = NULL;
static scfg_t             scfg;
static struct mqtt        mqtt;
static struct xpms_set *  ftp_set = NULL;
static protected_uint32_t active_clients;
static protected_uint32_t thread_count;
static volatile uint32_t  client_highwater = 0;
static volatile time_t    uptime = 0;
static volatile ulong     served = 0;
static bool               terminate_server = false;
static bool               diskspace_error_reported = false;
static char *             text[TOTAL_TEXT];
static str_list_t         pause_semfiles;
static str_list_t         recycle_semfiles;
static str_list_t         shutdown_semfiles;
static link_list_t        current_connections;

static rateLimiter*       request_rate_limiter = nullptr;
static trashCan*          ip_can = nullptr;
static trashCan*          ip_silent_can = nullptr;
static trashCan*          host_can = nullptr;

#ifdef SOCKET_DEBUG
static BYTE               socket_debug[0x10000] = {0};

	#define SOCKET_DEBUG_CTRL       (1 << 0)  /* 0x01 */
	#define SOCKET_DEBUG_SEND       (1 << 1)  /* 0x02 */
	#define SOCKET_DEBUG_READLINE   (1 << 2)  /* 0x04 */
	#define SOCKET_DEBUG_ACCEPT     (1 << 3)  /* 0x08 */
	#define SOCKET_DEBUG_SENDTHREAD (1 << 4)  /* 0x10 */
	#define SOCKET_DEBUG_TERMINATE  (1 << 5)  /* 0x20 */
	#define SOCKET_DEBUG_RECV_CHAR  (1 << 6)  /* 0x40 */
	#define SOCKET_DEBUG_FILEXFER   (1 << 7)  /* 0x80 */
#endif

char* genvpath(int lib, int dir, char* str);

typedef struct {
	SOCKET socket;
	union xp_sockaddr client_addr;
	socklen_t client_addr_len;
} ftp_t;


static const char *ftp_mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun"
	                            , "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

BOOL direxist(char *dir)
{
	if (access(dir, 0) == 0)
		return TRUE;
	else
		return FALSE;
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

#ifdef _WINSOCKAPI_

static WSADATA WSAData;
#define SOCKLIB_DESC WSAData.szDescription

static BOOL    WSAInitialized = FALSE;

static BOOL winsock_startup(void)
{
	int status;                 /* Status Code */

	if ((status = WSAStartup(MAKEWORD(1, 1), &WSAData)) == 0) {
		lprintf(LOG_DEBUG, "%s %s", WSAData.szDescription, WSAData.szSystemStatus);
		WSAInitialized = TRUE;
		return TRUE;
	}

	lprintf(LOG_CRIT, "!WinSock startup ERROR %d", status);
	return FALSE;
}

#else /* No WINSOCK */

#define winsock_startup()   (TRUE)
#define SOCKLIB_DESC        NULL

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

static void client_on(SOCKET sock, client_t* client, BOOL update)
{
	if (!update)
		listAddNodeData(&current_connections, client->addr, strlen(client->addr) + 1, sock, LAST_NODE);
	if (startup != NULL && startup->client_on != NULL)
		startup->client_on(startup->cbdata, TRUE, sock, client, update);
	mqtt_client_on(&mqtt, TRUE, sock, client, update);
}

static void client_off(SOCKET sock)
{
	listRemoveTaggedNode(&current_connections, sock, /* free_data */ TRUE);
	if (startup != NULL && startup->client_on != NULL)
		startup->client_on(startup->cbdata, FALSE, sock, NULL, FALSE);
	mqtt_client_on(&mqtt, FALSE, sock, NULL, FALSE);
}

static void thread_up(BOOL setuid)
{
	if (startup != NULL) {
		if (startup->thread_up != NULL)
			startup->thread_up(startup->cbdata, TRUE, setuid);
	}
}

static int32_t thread_down(void)
{
	int32_t count = protected_uint32_adjust_fetch(&thread_count, -1);
	if (startup != NULL) {
		if (startup->thread_up != NULL)
			startup->thread_up(startup->cbdata, FALSE, FALSE);
	}
	return count;
}

static void ftp_open_socket_cb(SOCKET sock, void *cbdata)
{
	char error[256];

	if (startup != NULL && startup->socket_open != NULL)
		startup->socket_open(startup->cbdata, TRUE);
	if (set_socket_options(&scfg, sock, "FTP", error, sizeof(error)))
		lprintf(LOG_ERR, "%04d !ERROR %s", sock, error);
}

static void ftp_close_socket_cb(SOCKET sock, void *cbdata)
{
	if (startup != NULL && startup->socket_open != NULL)
		startup->socket_open(startup->cbdata, FALSE);
}

static SOCKET ftp_open_socket(int domain, int type)
{
	SOCKET sock;

	sock = socket(domain, type, IPPROTO_IP);
	if (sock != INVALID_SOCKET)
		ftp_open_socket_cb(sock, NULL);
	return sock;
}

#ifdef __BORLANDC__
#pragma argsused
#endif
static int ftp_close_socket(SOCKET* sock, CRYPT_SESSION *sess, int line)
{
	int result;

	if (*sess != -1) {
		destroy_session(lprintf, *sess);
		*sess = -1;
	}

	if ((*sock) == INVALID_SOCKET) {
		lprintf(LOG_WARNING, "0000 !INVALID_SOCKET in close_socket from line %u", line);
		return -1;
	}

	shutdown(*sock, SHUT_RDWR);  /* required on Unix */

	result = closesocket(*sock);
	if (startup != NULL && startup->socket_open != NULL)
		startup->socket_open(startup->cbdata, FALSE);

	if (result != 0) {
		if (SOCKET_ERRNO != ENOTSOCK)
			lprintf(LOG_WARNING, "%04d !ERROR %d closing socket from line %u", *sock, SOCKET_ERRNO, line);
	}
	*sock = INVALID_SOCKET;

	return result;
}

#define GCES(status, sock, session, estr, action) do {                  \
			int GCES_level;                                                     \
			get_crypt_error_string(status, session, &estr, action, &GCES_level); \
			if (estr != NULL) {                                                 \
				lprintf(GCES_level, "%04d TLS %s", sock, estr);                 \
				free_crypt_attrstr(estr);                                       \
				estr = NULL;                                                      \
			}                                                                   \
} while (0)


#if defined(__GNUC__)   // Catch printf-format errors with sockprintf
static int sockprintf(SOCKET sock, CRYPT_SESSION sess, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));
#endif
static int sockprintf(SOCKET sock, CRYPT_SESSION sess, const char *fmt, ...)
{
	int     len;
	int     maxlen;
	int     result;
	va_list argptr;
	char    sbuf[1024];
	char *  estr;

	va_start(argptr, fmt);
	len = vsnprintf(sbuf, maxlen = sizeof(sbuf) - 2, fmt, argptr);
	va_end(argptr);

	if (len < 0 || len > maxlen) /* format error or output truncated */
		len = maxlen;
	if (startup != NULL && startup->options & FTP_OPT_DEBUG_TX)
		lprintf(LOG_DEBUG, "%04d TX%s: %.*s", sock, sess != -1 ? "S" : "", len, sbuf);
	memcpy(sbuf + len, "\r\n", 2);
	len += 2;

	if (sock == INVALID_SOCKET) {
		lprintf(LOG_WARNING, "!INVALID SOCKET in call to sockprintf");
		return 0;
	}

	/* Check socket for writability */
	if (!socket_writable(sock, 300000)) {
		lprintf(LOG_WARNING, "%04d !WARNING socket not ready for write", sock);
		return 0;
	}

	if (sess != -1) {
		int tls_sent;
		int sent = 0;
		while (sent < len) {
			result = cryptPushData(sess, sbuf + sent, len - sent, &tls_sent);
			if (result == CRYPT_OK)
				sent += tls_sent;
			else {
				GCES(result, sock, sess, estr, "sending data");
				if (result != CRYPT_ERROR_TIMEOUT)
					return 0;
			}
			result = cryptFlushData(sess);
			if (result != CRYPT_OK) {
				GCES(result, sock, sess, estr, "flushing data");
				return 0;
			}
		}
	}
	else {
		while ((result = sendsocket(sock, sbuf, len)) != len) {
			if (result == SOCKET_ERROR) {
				if (SOCKET_ERRNO == EWOULDBLOCK) {
					YIELD();
					continue;
				}
				if (SOCKET_ERRNO == ECONNRESET)
					lprintf(LOG_WARNING, "%04d Connection reset by peer on send", sock);
				else if (SOCKET_ERRNO == ECONNABORTED)
					lprintf(LOG_WARNING, "%04d Connection aborted by peer on send", sock);
				else
					lprintf(LOG_WARNING, "%04d !ERROR %d sending", sock, SOCKET_ERRNO);
				return 0;
			}
			lprintf(LOG_WARNING, "%04d !ERROR: short send: %u instead of %u", sock, result, len);
		}
	}
	return len;
}

void recverror(SOCKET socket, int rd, int line)
{
	if (rd == 0)
		lprintf(LOG_NOTICE, "%04d Socket closed by peer on receive (line %u)"
		        , socket, line);
	else if (rd == SOCKET_ERROR) {
		if (SOCKET_ERRNO == ECONNRESET)
			lprintf(LOG_NOTICE, "%04d Connection reset by peer on receive (line %u)"
			        , socket, line);
		else if (SOCKET_ERRNO == ECONNABORTED)
			lprintf(LOG_NOTICE, "%04d Connection aborted by peer on receive (line %u)"
			        , socket, line);
		else
			lprintf(LOG_NOTICE, "%04d !ERROR %d receiving on socket (line %u)"
			        , socket, SOCKET_ERRNO, line);
	} else
		lprintf(LOG_WARNING, "%04d !ERROR: recv on socket returned unexpected value: %d (line %u)"
		        , socket, rd, line);
}

static int sock_recvbyte(SOCKET sock, CRYPT_SESSION sess, char *buf, time_t *lastactive)
{
	int   len = 0;
	int   ret;
	int   i;
	char *estr;
	BOOL  first = TRUE;

	if (ftp_set == NULL || terminate_server) {
		sockprintf(sock, sess, "421 Server downed, aborting.");
		lprintf(LOG_WARNING, "%04d Server downed, aborting", sock);
		return 0;
	}
	if (sess > -1) {
		/* Try a read with no timeout first. */
		if ((ret = cryptSetAttribute(sess, CRYPT_OPTION_NET_READTIMEOUT, 0)) != CRYPT_OK)
			GCES(ret, sock, sess, estr, "setting read timeout");
		while (1) {
			ret = cryptPopData(sess, buf, 1, &len);
			/* Successive reads will be with the full timeout after a socket_readable() */
			cryptSetAttribute(sess, CRYPT_OPTION_NET_READTIMEOUT, startup->max_inactivity);
			switch (ret) {
				case CRYPT_OK:
					break;
				case CRYPT_ERROR_TIMEOUT:
					if (!first) {
						GCES(ret, sock, sess, estr, "popping data");
						return -1;
					}
					break;
				case CRYPT_ERROR_COMPLETE:
					return 0;
				default:
					GCES(ret, sock, sess, estr, "popping data");
					if (ret < -1)
						return ret;
					return -2;
			}
			first = FALSE;
			if (len)
				return len;

			if ((time(NULL) - (*lastactive)) > startup->max_inactivity) {
				lprintf(LOG_WARNING, "%04d Disconnecting due to to inactivity", sock);
				sockprintf(sock, sess, "421 Disconnecting due to inactivity (%u seconds)."
				           , startup->max_inactivity);
				return 0;
			}

			if (!socket_readable(sock, startup->max_inactivity * 1000)) {
				if ((time(NULL) - (*lastactive)) > startup->max_inactivity) {
					lprintf(LOG_WARNING, "%04d Disconnecting due to to inactivity", sock);
					sockprintf(sock, sess, "421 Disconnecting due to inactivity (%u seconds)."
					           , startup->max_inactivity);
					return 0;
				}
			}
		}
	}
	else {
		while (1) {
			if (!socket_readable(sock, startup->max_inactivity * 1000)) {
				if ((time(NULL) - (*lastactive)) > startup->max_inactivity) {
					lprintf(LOG_WARNING, "%04d Disconnecting due to to inactivity", sock);
					sockprintf(sock, sess, "421 Disconnecting due to inactivity (%u seconds)."
					           , startup->max_inactivity);
					return 0;
				}
				continue;
			}
	#ifdef SOCKET_DEBUG_RECV_CHAR
			socket_debug[sock] |= SOCKET_DEBUG_RECV_CHAR;
	#endif
			i = recv(sock, buf, 1, 0);
	#ifdef SOCKET_DEBUG_RECV_CHAR
			socket_debug[sock] &= ~SOCKET_DEBUG_RECV_CHAR;
	#endif
			return i;
		}
	}
}

int sockreadline(SOCKET socket, CRYPT_SESSION sess, char* buf, int len, time_t* lastactive)
{
	char ch;
	int  i, rd = 0;

	buf[0] = 0;

	if (socket == INVALID_SOCKET) {
		lprintf(LOG_WARNING, "INVALID SOCKET in call to sockreadline");
		return 0;
	}

	while (rd < len - 1) {
		i = sock_recvbyte(socket, sess, &ch, lastactive);

		if (i < 1) {
			if (sess != -1)
				recverror(socket, i, __LINE__);
			return i;
		}
		if (ch == '\n' /* && rd>=1 */) { /* Mar-9-2003: terminate on sole LF */
			break;
		}
		buf[rd++] = ch;
	}
	if (rd > 0 && buf[rd - 1] == '\r')
		buf[rd - 1] = 0;
	else
		buf[rd] = 0;

	return rd;
}

void ftp_terminate(void)
{
	lprintf(LOG_INFO, "FTP Server terminate");
	terminate_server = TRUE;
}

bool ftp_remove(SOCKET sock, int line, const char* fname, const char* username, int err_level)
{
	int ret = 0;

	if (fexist(fname) && (ret = remove(fname)) != 0) {
		if (fexist(fname)) { // In case there was a race condition (other host deleted file first)
			char error[256];
			lprintf(err_level, "%04d <%s> !ERROR %d (%s) (line %d) removing file: %s"
			        , sock, username, errno, safe_strerror(errno, error, sizeof error), line, fname);
		}
	}
	return ret == 0;
}

typedef struct {
	SOCKET ctrl_sock;
	CRYPT_SESSION ctrl_sess;
	SOCKET* data_sock;
	CRYPT_SESSION* data_sess;
	volatile BOOL* inprogress;
	volatile BOOL* aborted;
	BOOL delfile;
	BOOL tmpfile;
	BOOL credits;
	BOOL append;
	off_t filepos;
	char filename[MAX_PATH + 1];
	time_t* lastactive;
	user_t* user;
	client_t* client;
	int dir;
	char* desc;
} xfer_t;

static void send_thread(void* arg)
{
	char              buf[8192];
	char              str[256];
	char              errstr[256];
	char              tmp[128];
	char              username[128];
	char              host_ip[INET6_ADDRSTRLEN];
	int               i;
	int               rd;
	int               wr;
	long              mod;
	uint64_t          l;
	off_t             total = 0;
	off_t             last_total = 0;
	ulong             dur;
	ulong             cps;
	off_t             length;
	BOOL              error = FALSE;
	FILE*             fp;
	file_t            f;
	xfer_t            xfer;
	time_t            now;
	time_t            start;
	time_t            last_report;
	user_t            uploader;
	union xp_sockaddr addr;
	socklen_t         addr_len;
	char *            estr;

	xfer = *(xfer_t*)arg;
	free(arg);

	SetThreadName("sbbs/ftpSend");
	thread_up(TRUE /* setuid */);

	length = flength(xfer.filename);

	if (length < 1) {
		if (xfer.tmpfile) {
			if (!(startup->options & FTP_OPT_KEEP_TEMP_FILES))
				ftp_remove(xfer.ctrl_sock, __LINE__, xfer.filename, xfer.user->alias, LOG_ERR);
			sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "450 No files");
		} else {
			lprintf(LOG_WARNING, "%04d <%s> !DATA cannot send file (%s) with size of %" PRIdOFF " bytes"
			        , xfer.ctrl_sock, xfer.user->alias, xfer.filename, length);
			sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "450 Invalid file size: %" PRIdOFF, length);
		}
		ftp_close_socket(xfer.data_sock, xfer.data_sess, __LINE__);
		*xfer.inprogress = FALSE;
		thread_down();
		return;
	}

	if ((fp = fnopen(NULL, xfer.filename, O_RDONLY | O_BINARY)) == NULL  /* non-shareable open failed */
	    && (fp = fopen(xfer.filename, "rb")) == NULL) {              /* shareable open failed */
		lprintf(LOG_ERR, "%04d <%s> !DATA ERROR %d (%s) line %d opening %s"
		        , xfer.ctrl_sock, xfer.user->alias, errno, safe_strerror(errno, errstr, sizeof errstr), __LINE__, xfer.filename);
		sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "450 ERROR %d (%s) opening %s", errno, safe_strerror(errno, errstr, sizeof errstr), xfer.filename);
		if (xfer.tmpfile && !(startup->options & FTP_OPT_KEEP_TEMP_FILES))
			ftp_remove(xfer.ctrl_sock, __LINE__, xfer.filename, xfer.user->alias, LOG_ERR);
		ftp_close_socket(xfer.data_sock, xfer.data_sess, __LINE__);
		*xfer.inprogress = FALSE;
		thread_down();
		return;
	}

#ifdef SOCKET_DEBUG_SENDTHREAD
	socket_debug[xfer.ctrl_sock] |= SOCKET_DEBUG_SENDTHREAD;
#endif

	*xfer.aborted = FALSE;
	if (xfer.filepos < 0)
		xfer.filepos = 0;
	if (startup->options & FTP_OPT_DEBUG_DATA || xfer.filepos)
		lprintf(LOG_DEBUG, "%04d <%s> DATA socket %d sending %s from offset %" PRIdOFF
		        , xfer.ctrl_sock, xfer.user->alias, *xfer.data_sock, xfer.filename, xfer.filepos);

	fseeko(fp, xfer.filepos, SEEK_SET);
	last_report = start = time(NULL);
	while ((xfer.filepos + total) < length) {

		now = time(NULL);

		/* Periodic progress report */
		if (total && now >= last_report + XFER_REPORT_INTERVAL) {
			if (xfer.filepos)
				sprintf(str, " from offset %" PRIdOFF, xfer.filepos);
			else
				str[0] = 0;
			lprintf(LOG_INFO, "%04d <%s> DATA Sent %" PRIdOFF " bytes (%" PRIdOFF " total) of %s (%lu cps)%s"
			        , xfer.ctrl_sock, xfer.user->alias, total, length, xfer.filename
			        , (ulong)((total - last_total) / (now - last_report))
			        , str);
			last_total = total;
			last_report = now;
		}

		if (*xfer.aborted == TRUE) {
			lprintf(LOG_WARNING, "%04d <%s> !DATA Transfer aborted", xfer.ctrl_sock, xfer.user->alias);
			sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "426 Transfer aborted.");
			error = TRUE;
			break;
		}
		if (ftp_set == NULL || terminate_server) {
			lprintf(LOG_WARNING, "%04d <%s> !DATA Transfer locally aborted", xfer.ctrl_sock, xfer.user->alias);
			sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "426 Transfer locally aborted.");
			error = TRUE;
			break;
		}

		/* Check socket for writability */
		if (!socket_writable(*xfer.data_sock, 1000))
			continue;

		fseeko(fp, xfer.filepos + total, SEEK_SET);
		rd = fread(buf, sizeof(char), sizeof(buf), fp);
		if (rd < 1) /* EOF or READ error */
			break;

#ifdef SOCKET_DEBUG_SEND
		socket_debug[xfer.ctrl_sock] |= SOCKET_DEBUG_SEND;
#endif
		if (*xfer.data_sess != -1) {
			int status = cryptPushData(*xfer.data_sess, buf, rd, &wr);
			if (status != CRYPT_OK) {
				GCES(status, *xfer.data_sock, *xfer.data_sess, estr, "pushing data");
				wr = -1;
			}
			else {
				status = cryptFlushData(*xfer.data_sess);
				if (status != CRYPT_OK) {
					GCES(status, *xfer.data_sock, *xfer.data_sess, estr, "flushing data");
					wr = -1;
				}
			}
		}
		else
			wr = sendsocket(*xfer.data_sock, buf, rd);
#ifdef SOCKET_DEBUG_SEND
		socket_debug[xfer.ctrl_sock] &= ~SOCKET_DEBUG_SEND;
#endif
		if (wr < 1) {
			if (wr == SOCKET_ERROR) {
				if (SOCKET_ERRNO == EWOULDBLOCK) {
					/*lprintf(LOG_WARNING,"%04d DATA send would block, retrying",xfer.ctrl_sock);*/
					YIELD();
					continue;
				}
				else if (SOCKET_ERRNO == ECONNRESET)
					lprintf(LOG_WARNING, "%04d <%s> DATA Connection reset by peer, sending on socket %d"
					        , xfer.ctrl_sock, xfer.user->alias, *xfer.data_sock);
				else if (SOCKET_ERRNO == ECONNABORTED)
					lprintf(LOG_WARNING, "%04d <%s> DATA Connection aborted by peer, sending on socket %d"
					        , xfer.ctrl_sock, xfer.user->alias, *xfer.data_sock);
				else
					lprintf(LOG_WARNING, "%04d <%s> !DATA ERROR %d sending on data socket %d"
					        , xfer.ctrl_sock, xfer.user->alias, SOCKET_ERRNO, *xfer.data_sock);
				/* Send NAK */
				sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "426 Error %d sending on DATA channel"
				           , SOCKET_ERRNO);
				error = TRUE;
				break;
			}
			if (wr == 0) {
				lprintf(LOG_WARNING, "%04d <%s> !DATA socket %d disconnected", xfer.ctrl_sock, xfer.user->alias, *xfer.data_sock);
				sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "426 DATA channel disconnected");
				error = TRUE;
				break;
			}
			lprintf(LOG_ERR, "%04d <%s> !DATA ERROR %d (%d) sending on socket %d"
			        , xfer.ctrl_sock, xfer.user->alias, wr, SOCKET_ERRNO, *xfer.data_sock);
			sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "451 DATA send error");
			error = TRUE;
			break;
		}
		total += wr;
		*xfer.lastactive = time(NULL);
		//YIELD();
	}

	if ((i = ferror(fp)) != 0)
		lprintf(LOG_ERR, "%04d <%s> !DATA FILE ERROR %d (errno %d %s)"
		        , xfer.ctrl_sock, xfer.user->alias, i, errno, safe_strerror(errno, errstr, sizeof errstr));

	ftp_close_socket(xfer.data_sock, xfer.data_sess, __LINE__);   /* Signal end of file */
	if (startup->options & FTP_OPT_DEBUG_DATA)
		lprintf(LOG_DEBUG, "%04d <%s> DATA socket closed", xfer.ctrl_sock, xfer.user->alias);

	if (!error) {
		dur = (long)(time(NULL) - start);
		cps = (ulong)(dur ? total / dur : total * 2);
		lprintf(LOG_INFO, "%04d <%s> DATA Transfer successful: %" PRIdOFF " bytes sent in %lu seconds (%lu cps)"
		        , xfer.ctrl_sock
		        , xfer.user->alias
		        , total, dur, cps);
		sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "226 Download complete (%lu cps).", cps);

		if (xfer.dir >= 0 && !xfer.tmpfile) {
			memset(&f, 0, sizeof(f));
			if (!loadfile(&scfg, xfer.dir, getfname(xfer.filename), &f, file_detail_normal, NULL)) {
				lprintf(LOG_ERR, "%04d <%s> DATA downloaded: %s (not found in filebase!)"
				        , xfer.ctrl_sock
				        , xfer.user->alias
				        , xfer.filename);
			} else {
				f.hdr.times_downloaded++;
				f.hdr.last_downloaded = time32(NULL);
				updatefile(&scfg, &f, NULL);

				lprintf(LOG_INFO, "%04d <%s> DATA downloaded: %s (%u times total, %u files in %s bytes today)"
				        , xfer.ctrl_sock
				        , xfer.user->alias
				        , xfer.filename
				        , f.hdr.times_downloaded
						, xfer.user->dtoday + 1
						, byte_estimate_to_str(xfer.user->btoday + total, tmp, sizeof tmp, 1, 1));
				/**************************/
				/* Update Uploader's Info */
				/**************************/
				uploader.number = 0;
				if (f.from_ext != NULL)
					uploader.number = atoi(f.from_ext);
				if (uploader.number == 0)
					uploader.number = matchuser(&scfg, f.from, TRUE /*sysop_alias*/);
				if (uploader.number
				    && uploader.number != xfer.user->number
				    && getuserdat(&scfg, &uploader) == 0
				    && uploader.firston < (time_t)f.hdr.when_imported.time) {
					l = f.cost;
					if (!(scfg.dir[f.dir]->misc & DIR_CDTDL))  /* Don't give credits on d/l */
						l = 0;
					if (scfg.dir[f.dir]->misc & DIR_CDTMIN && cps) { /* Give min instead of cdt */
						mod = ((ulong)(l * (scfg.dir[f.dir]->dn_pct / 100.0)) / cps) / 60;
						adjustuserval(&scfg, &uploader, USER_MIN, mod);
						sprintf(tmp, "%lu minute", mod);
					} else {
						mod = (ulong)(l * (scfg.dir[f.dir]->dn_pct / 100.0));
						adjustuserval(&scfg, &uploader, USER_CDT, mod);
						u32toac(mod, tmp, ',');
					}
					if (!(scfg.dir[f.dir]->misc & DIR_QUIET)) {
						const char* prefix = xfer.filepos ? "partially FTP-" : "FTP-";
						addr_len = sizeof(addr);
						if (user_is_sysop(&uploader)
						    && getpeername(xfer.ctrl_sock, &addr.addr, &addr_len) == 0
						    && inet_addrtop(&addr, host_ip, sizeof(host_ip)) != NULL)
							SAFEPRINTF2(username, "%s [%s]", xfer.user->alias, host_ip);
						else
							SAFECOPY(username, xfer.user->alias);
						/* Inform uploader of downloaded file */
						if (mod == 0)
							safe_snprintf(str, sizeof(str), text[FreeDownloadUserMsg]
							              , getfname(xfer.filename)
							              , prefix
							              , username);
						else
							safe_snprintf(str, sizeof(str), text[DownloadUserMsg]
							              , getfname(xfer.filename)
							              , prefix
							              , username, tmp);
						putsmsg(&scfg, uploader.number, str);
					}
				}
				mqtt_file_download(&mqtt, xfer.user, f.dir, f.name, total, xfer.client);
				smb_freefilemem(&f);
			}
			if (!xfer.tmpfile && !xfer.delfile && !(scfg.dir[f.dir]->misc & DIR_NOSTAT))
				inc_download_stats(&scfg, 1, (ulong)total);
		}

		if (xfer.credits) {
			user_downloaded(&scfg, xfer.user, 1, total);
			if (xfer.dir >= 0 && !download_is_free(&scfg, xfer.dir, xfer.user, xfer.client))
				subtract_cdt(&scfg, xfer.user, xfer.credits);
		}
	}

	fclose(fp);
	if (ftp_set != NULL)
		*xfer.inprogress = FALSE;
	if (xfer.tmpfile) {
		if (!(startup->options & FTP_OPT_KEEP_TEMP_FILES))
			ftp_remove(xfer.ctrl_sock, __LINE__, xfer.filename, xfer.user->alias, LOG_ERR);
	}
	else if (xfer.delfile && !error)
		ftp_remove(xfer.ctrl_sock, __LINE__, xfer.filename, xfer.user->alias, LOG_WARNING);

#if defined(SOCKET_DEBUG_SENDTHREAD)
	socket_debug[xfer.ctrl_sock] &= ~SOCKET_DEBUG_SENDTHREAD;
#endif

	thread_down();
}

static void receive_thread(void* arg)
{
	char   str[128];
	char   errstr[256];
	char   buf[8192];
	char   extdesc[LEN_EXTDESC + 1] = "";
	char   tmp[MAX_PATH + 1];
	int    rd;
	off_t  total = 0;
	off_t  last_total = 0;
	ulong  dur;
	ulong  cps;
	BOOL   error = FALSE;
	BOOL   filedat;
	FILE*  fp;
	file_t f;
	xfer_t xfer;
	time_t now;
	time_t start;
	time_t last_report;
	char * estr;

	xfer = *(xfer_t*)arg;
	free(arg);

	SetThreadName("sbbs/ftpReceive");
	thread_up(TRUE /* setuid */);

	if ((fp = fopen(xfer.filename, xfer.append ? "ab" : "wb")) == NULL) {
		lprintf(LOG_ERR, "%04d <%s> !DATA ERROR %d (%s) line %d opening %s"
		        , xfer.ctrl_sock, xfer.user->alias, errno, safe_strerror(errno, errstr, sizeof errstr), __LINE__, xfer.filename);
		sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "450 ERROR %d (%s) opening %s", errno, safe_strerror(errno, errstr, sizeof errstr), xfer.filename);
		ftp_close_socket(xfer.data_sock, xfer.data_sess, __LINE__);
		*xfer.inprogress = FALSE;
		thread_down();
		return;
	}

	if (xfer.append)
		xfer.filepos = filelength(fileno(fp));

	if (xfer.filepos < 0)
		xfer.filepos = 0;

	*xfer.aborted = FALSE;
	if (xfer.filepos || startup->options & FTP_OPT_DEBUG_DATA)
		lprintf(LOG_DEBUG, "%04d <%s> DATA socket %d receiving %s from offset %" PRIdOFF
		        , xfer.ctrl_sock, xfer.user->alias, *xfer.data_sock, xfer.filename, xfer.filepos);

	fseeko(fp, xfer.filepos, SEEK_SET);

	// Determine the maximum file size to allow, accounting for minimum free space
	char    path[MAX_PATH + 1];
	SAFECOPY(path, xfer.filename);
	*getfname(path) = '\0';
	int64_t avail = getfreediskspace(path, 1);
	if (avail <= scfg.min_dspace)
		avail = 0;
	else
		avail -= scfg.min_dspace;
	int64_t max_fsize = xfer.filepos + avail;
	if (startup->max_fsize > 0 && startup->max_fsize < max_fsize)
		max_fsize = startup->max_fsize;
	if (startup->options & FTP_OPT_DEBUG_DATA)
		lprintf(LOG_DEBUG, "%04d <%s> DATA Limiting uploaded file size to %" PRIu64 " (%s) bytes"
		        , xfer.ctrl_sock, xfer.user->alias, max_fsize
		        , byte_estimate_to_str(max_fsize, tmp, sizeof(tmp), 1, 1));

	last_report = start = time(NULL);
	while (1) {

		now = time(NULL);

		/* Periodic progress report */
		if (total && now >= last_report + XFER_REPORT_INTERVAL) {
			if (xfer.filepos)
				sprintf(str, " from offset %" PRIdOFF, xfer.filepos);
			else
				str[0] = 0;
			lprintf(LOG_INFO, "%04d <%s> DATA Received %" PRIdOFF " bytes of %s (%lu cps)%s"
			        , xfer.ctrl_sock
			        , xfer.user->alias
			        , total, xfer.filename
			        , (ulong)((total - last_total) / (now - last_report))
			        , str);
			last_total = total;
			last_report = now;
		}
		if (xfer.filepos + total > max_fsize) {
			lprintf(LOG_WARNING, "%04d <%s> !DATA received %" PRIdOFF " bytes of %s exceeds maximum allowed (%" PRIu64 " bytes)"
			        , xfer.ctrl_sock, xfer.user->alias, xfer.filepos + total, xfer.filename, startup->max_fsize);
			sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "552 File size exceeds maximum allowed (%" PRIu64 " bytes)", startup->max_fsize);
			error = TRUE;
			break;
		}
		if (*xfer.aborted == TRUE) {
			lprintf(LOG_WARNING, "%04d <%s> !DATA Transfer aborted", xfer.ctrl_sock, xfer.user->alias);
			/* Send NAK */
			sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "426 Transfer aborted.");
			error = TRUE;
			break;
		}
		if (ftp_set == NULL || terminate_server) {
			lprintf(LOG_WARNING, "%04d <%s> !DATA Transfer locally aborted", xfer.ctrl_sock, xfer.user->alias);
			/* Send NAK */
			sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "426 Transfer locally aborted.");
			error = TRUE;
			break;
		}

		/* Check socket for readability */
		if (!socket_readable(*xfer.data_sock, 1000))
			continue;

#if defined(SOCKET_DEBUG_RECV_BUF)
		socket_debug[xfer.ctrl_sock] |= SOCKET_DEBUG_RECV_BUF;
#endif
		if (*xfer.data_sess != -1) {
			int status = cryptPopData(*xfer.data_sess, buf, sizeof(buf), &rd);
			if (status != CRYPT_OK) {
				GCES(status, *xfer.data_sock, *xfer.data_sess, estr, "popping data");
				if (status != CRYPT_ERROR_COMPLETE)
					rd = SOCKET_ERROR;
			}
		}
		else {
			rd = recv(*xfer.data_sock, buf, sizeof(buf), 0);
		}
#if defined(SOCKET_DEBUG_RECV_BUF)
		socket_debug[xfer.ctrl_sock] &= ~SOCKET_DEBUG_RECV_BUF;
#endif
		if (rd < 1) {
			if (rd == 0) { /* Socket closed */
				if (startup->options & FTP_OPT_DEBUG_DATA)
					lprintf(LOG_DEBUG, "%04d <%s> DATA socket %d closed by client"
					        , xfer.ctrl_sock, xfer.user->alias, *xfer.data_sock);
				break;
			}
			if (rd == SOCKET_ERROR) {
				if (SOCKET_ERRNO == EWOULDBLOCK) {
					/*lprintf(LOG_WARNING,"%04d DATA recv would block, retrying",xfer.ctrl_sock);*/
					YIELD();
					continue;
				}
				else if (SOCKET_ERRNO == ECONNRESET)
					lprintf(LOG_WARNING, "%04d <%s> DATA Connection reset by peer, receiving on socket %d"
					        , xfer.ctrl_sock, xfer.user->alias, *xfer.data_sock);
				else if (SOCKET_ERRNO == ECONNABORTED)
					lprintf(LOG_WARNING, "%04d <%s> DATA Connection aborted by peer, receiving on socket %d"
					        , xfer.ctrl_sock, xfer.user->alias, *xfer.data_sock);
				else
					lprintf(LOG_WARNING, "%04d <%s> !DATA ERROR %d receiving on data socket %d"
					        , xfer.ctrl_sock, xfer.user->alias, SOCKET_ERRNO, *xfer.data_sock);
				/* Send NAK */
				sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "426 Error %d receiving on DATA channel"
				           , SOCKET_ERRNO);
				error = TRUE;
				break;
			}
			lprintf(LOG_ERR, "%04d <%s> !DATA ERROR recv returned %d on socket %d"
			        , xfer.ctrl_sock, xfer.user->alias, rd, *xfer.data_sock);
			/* Send NAK */
			sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "451 Unexpected socket error: %d", rd);
			error = TRUE;
			break;
		}
		fwrite(buf, 1, rd, fp);
		total += rd;
		*xfer.lastactive = time(NULL);
		YIELD();
	}

	fclose(fp);

	ftp_close_socket(xfer.data_sock, xfer.data_sess, __LINE__);
	if (error && startup->options & FTP_OPT_DEBUG_DATA)
		lprintf(LOG_DEBUG, "%04d <%s> DATA socket %d closed", xfer.ctrl_sock, xfer.user->alias, *xfer.data_sock);

	if (xfer.filepos + total < startup->min_fsize) {
		lprintf(LOG_WARNING, "%04d <%s> DATA received %" PRIdOFF " bytes for %s, less than minimum required (%" PRIu64 " bytes)"
		        , xfer.ctrl_sock, xfer.user->alias, xfer.filepos + total, xfer.filename, startup->min_fsize);
		sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "550 File size less than minimum required (%" PRIu64 " bytes)"
		           , startup->min_fsize);
		error = TRUE;
	}
	if (error) {
		if (!xfer.append)
			ftp_remove(xfer.ctrl_sock, __LINE__, xfer.filename, xfer.user->alias, LOG_ERR);
	} else {
		dur = (long)(time(NULL) - start);
		cps = (ulong)(dur ? total / dur : total * 2);
		lprintf(LOG_INFO, "%04d <%s> DATA Transfer successful: %" PRIdOFF " bytes received in %lu seconds (%lu cps)"
		        , xfer.ctrl_sock
		        , xfer.user->alias
		        , total, dur, cps);

		if (xfer.dir >= 0) {
			memset(&f, 0, sizeof(f));
			f.dir = xfer.dir;
			smb_hfield_str(&f, SMB_FILENAME, getfname(xfer.filename));
			smb_hfield_str(&f, SENDER, xfer.user->alias);

			filedat = findfile(&scfg, xfer.dir, f.name, NULL);
			if (scfg.dir[f.dir]->misc & DIR_AONLY)  /* Forced anonymous */
				f.hdr.attr |= MSG_ANONYMOUS;
			off_t cdt = flength(xfer.filename);
			smb_hfield_bin(&f, SMB_COST, cdt);

			char  fdesc[LEN_FDESC + 1] = "";
			/* Description specified with DESC command? */
			if (xfer.desc != NULL)
				SAFECOPY(fdesc, xfer.desc);

			/* Necessary for DIR and LIB ARS keyword support in subsequent chk_ar()'s */
			SAFECOPY(xfer.user->curdir, scfg.dir[f.dir]->code);

			/* FILE_ID.DIZ support */
			if (scfg.dir[f.dir]->misc & DIR_DIZ) {
				lprintf(LOG_DEBUG, "%04d <%s> DATA Extracting DIZ from: %s", xfer.ctrl_sock, xfer.user->alias, xfer.filename);
				if (extract_diz(&scfg, &f, /* diz_fnames */ NULL, tmp, sizeof(tmp))) {
					struct sauce_charinfo sauce;
					lprintf(LOG_DEBUG, "%04d <%s> DATA Parsing DIZ: %s", xfer.ctrl_sock, xfer.user->alias, tmp);
					char*                 lines = read_diz(tmp, &sauce);
					format_diz(lines, extdesc, sizeof(extdesc), sauce.width, sauce.ice_color);
					free_diz(lines);
					if (!fdesc[0]) {                     /* use for normal description */
						prep_file_desc(extdesc, fdesc); /* strip control chars and dupe chars */
					}
					file_sauce_hfields(&f, &sauce);
					ftp_remove(xfer.ctrl_sock, __LINE__, tmp, xfer.user->alias, LOG_ERR);
				} else
					lprintf(LOG_DEBUG, "%04d <%s> DATA DIZ does not exist in: %s", xfer.ctrl_sock, xfer.user->alias, xfer.filename);
			} /* FILE_ID.DIZ support */

			if (f.desc == NULL)
				smb_new_hfield_str(&f, SMB_FILEDESC, fdesc);
			if (filedat) {
				int result;
				if (updatefile(&scfg, &f, &result))
					lprintf(LOG_INFO, "%04d <%s> DATA updated file: %s"
					        , xfer.ctrl_sock, xfer.user->alias, f.name);
				else
					lprintf(LOG_ERR, "%04d <%s> !DATA ERROR %d updating file (%s) in database"
					        , xfer.ctrl_sock, xfer.user->alias, result, f.name);
				/* need to update the index here */
			} else {
				int result;
				if (addfile(&scfg, &f, extdesc, /* metatdata: */ NULL, xfer.client, &result))
					lprintf(LOG_INFO, "%04d <%s> DATA uploaded file: %s"
					        , xfer.ctrl_sock, xfer.user->alias, f.name);
				else
					lprintf(LOG_ERR, "%04d <%s> !DATA ERROR %d adding file (%s) to database"
					        , xfer.ctrl_sock, xfer.user->alias, result, f.name);
			}

			if (scfg.dir[f.dir]->upload_sem[0])
				ftouch(scfg.dir[f.dir]->upload_sem);
			/**************************/
			/* Update Uploader's Info */
			/**************************/
			user_uploaded(&scfg, xfer.user, (!xfer.append && xfer.filepos == 0) ? 1:0, total);
			if (scfg.dir[f.dir]->up_pct && scfg.dir[f.dir]->misc & DIR_CDTUL) { /* credit for upload */
				if (scfg.dir[f.dir]->misc & DIR_CDTMIN && cps)    /* Give min instead of cdt */
					xfer.user->min = (uint32_t)adjustuserval(&scfg, xfer.user, USER_MIN
					                                         , ((ulong)(total * (scfg.dir[f.dir]->up_pct / 100.0)) / cps) / 60);
				else
					xfer.user->cdt = adjustuserval(&scfg, xfer.user, USER_CDT
					                               , cdt * (uint64_t)(scfg.dir[f.dir]->up_pct / 100.0));
			}
			if (!(scfg.dir[f.dir]->misc & DIR_NOSTAT))
				inc_upload_stats(&scfg, 1, (ulong)total);

			mqtt_file_upload(&mqtt, xfer.user, f.dir, f.name, total, xfer.client);
			smb_freefilemem(&f);
		}
		/* Send ACK */
		sockprintf(xfer.ctrl_sock, xfer.ctrl_sess, "226 Upload complete (%lu cps).", cps);
	}

	if (ftp_set != NULL)
		*xfer.inprogress = FALSE;

	thread_down();
}

// Returns TRUE upon error?!?
static BOOL start_tls(SOCKET *sock, CRYPT_SESSION *sess, BOOL resp)
{
	BOOL  nodelay;
	ulong nb;
	int   status;
	char *estr = NULL;

	if (!ssl_sync(&scfg, lprintf)) {
		lprintf(LOG_CRIT, "!ssl_sync() failure trying to enable TLS support");
		if (resp)
			sockprintf(*sock, *sess, "431 TLS not available");
		return FALSE;
	}
	if ((status = cryptCreateSession(sess, CRYPT_UNUSED, CRYPT_SESSION_TLS_SERVER)) != CRYPT_OK) {
		GCES(status, *sock, CRYPT_UNUSED, estr, "creating session");
		if (resp)
			sockprintf(*sock, *sess, "431 TLS not available");
		return FALSE;
	}
	if ((status = cryptSetAttribute(*sess, CRYPT_SESSINFO_TLS_OPTIONS, CRYPT_TLSOPTION_MINVER_TLS12)) != CRYPT_OK) {
		GCES(status, *sock, *sess, estr, "setting TLS minver");
		cryptDestroySession(*sess);
		*sess = -1;
		if (resp)
			sockprintf(*sock, *sess, "431 TLS not available");
		return FALSE;
	}
	if ((status = add_private_key(&scfg, lprintf, *sess)) != CRYPT_OK) {
		GCES(status, *sock, *sess, estr, "setting private key");
		destroy_session(lprintf, *sess);
		*sess = -1;
		if (resp)
			sockprintf(*sock, *sess, "431 TLS not available");
		return FALSE;
	}
	nodelay = TRUE;
	(void)setsockopt(*sock, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
	nb = 0;
	ioctlsocket(*sock, FIONBIO, &nb);
	if ((status = cryptSetAttribute(*sess, CRYPT_SESSINFO_NETWORKSOCKET, *sock)) != CRYPT_OK) {
		GCES(status, *sock, *sess, estr, "setting network socket");
		destroy_session(lprintf, *sess);
		*sess = -1;
		if (resp)
			sockprintf(*sock, *sess, "431 TLS not available");
		return TRUE;
	}
	if (resp)
		sockprintf(*sock, -1, "234 Ready to start TLS");
	if ((status = cryptSetAttribute(*sess, CRYPT_SESSINFO_ACTIVE, 1)) != CRYPT_OK) {
		GCES(status, *sock, *sess, estr, "setting session active");
		return TRUE;
	}
	if (startup->max_inactivity) {
		if ((status = cryptSetAttribute(*sess, CRYPT_OPTION_NET_READTIMEOUT, startup->max_inactivity)) != CRYPT_OK) {
			GCES(status, *sock, *sess, estr, "setting read timeout");
			return TRUE;
		}
	}
	return FALSE;
}

static void filexfer(union xp_sockaddr* addr, SOCKET ctrl_sock, CRYPT_SESSION ctrl_sess, SOCKET pasv_sock, CRYPT_SESSION pasv_sess, SOCKET* data_sock
                     , CRYPT_SESSION *data_sess, char* filename, off_t filepos, volatile BOOL* inprogress, volatile BOOL* aborted
                     , BOOL delfile, BOOL tmpfile
                     , time_t* lastactive
                     , user_t* user
                     , client_t* client
                     , int dir
                     , BOOL receiving
                     , BOOL credits
                     , BOOL append
                     , char* desc, BOOL protect)
{
	int               result;
	ulong             l;
	socklen_t         addr_len;
	union xp_sockaddr server_addr;
	BOOL              reuseaddr;
	xfer_t*           xfer;
	char              host_ip[INET6_ADDRSTRLEN];

	if ((*inprogress) == TRUE) {
		lprintf(LOG_WARNING, "%04d <%s> !DATA TRANSFER already in progress", ctrl_sock, user->alias);
		sockprintf(ctrl_sock, ctrl_sess, "425 Transfer already in progress.");
		if (tmpfile && !(startup->options & FTP_OPT_KEEP_TEMP_FILES))
			ftp_remove(ctrl_sock, __LINE__, filename, user->alias, LOG_ERR);
		return;
	}
	*inprogress = TRUE;

	if (*data_sock != INVALID_SOCKET)
		ftp_close_socket(data_sock, data_sess, __LINE__);

	inet_addrtop(addr, host_ip, sizeof(host_ip));
	if (pasv_sock == INVALID_SOCKET) { /* !PASV */

		if ((*data_sock = socket(addr->addr.sa_family, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET) {
			lprintf(LOG_ERR, "%04d <%s> !DATA ERROR %d opening socket", ctrl_sock, user->alias, SOCKET_ERRNO);
			sockprintf(ctrl_sock, ctrl_sess, "425 Error %d opening socket", SOCKET_ERRNO);
			if (tmpfile && !(startup->options & FTP_OPT_KEEP_TEMP_FILES))
				ftp_remove(ctrl_sock, __LINE__, filename, user->alias, LOG_ERR);
			*inprogress = FALSE;
			return;
		}
		if (startup->socket_open != NULL)
			startup->socket_open(startup->cbdata, TRUE);
		if (startup->options & FTP_OPT_DEBUG_DATA)
			lprintf(LOG_DEBUG, "%04d <%s> DATA socket %d opened", ctrl_sock, user->alias, *data_sock);

		/* Use port-1 for all data connections */
		reuseaddr = TRUE;
		(void)setsockopt(*data_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseaddr, sizeof(reuseaddr));

		addr_len = sizeof(server_addr);
		if ((result = getsockname(ctrl_sock, &server_addr.addr, &addr_len)) != 0) {
			lprintf(LOG_CRIT, "%04d <%s> !DATA ERROR %d (%d) getting address/port of command socket (%u)"
			        , ctrl_sock, user->alias, result, SOCKET_ERRNO, pasv_sock);
			return;
		}

		inet_setaddrport(&server_addr, inet_addrport(&server_addr) - 1);  /* 20? */

		result = bind(*data_sock, &server_addr.addr, addr_len);
		if (result != 0) {
			inet_setaddrport(&server_addr, 0);  /* any user port */
			result = bind(*data_sock, &server_addr.addr, addr_len);
		}
		if (result != 0) {
			lprintf(LOG_ERR, "%04d <%s> DATA ERROR %d (%d) binding socket %d"
			        , ctrl_sock, user->alias, result, SOCKET_ERRNO, *data_sock);
			sockprintf(ctrl_sock, ctrl_sess, "425 Error %d binding socket", SOCKET_ERRNO);
			if (tmpfile && !(startup->options & FTP_OPT_KEEP_TEMP_FILES))
				ftp_remove(ctrl_sock, __LINE__, filename, user->alias, LOG_ERR);
			*inprogress = FALSE;
			ftp_close_socket(data_sock, data_sess, __LINE__);
			return;
		}

		result = connect(*data_sock, &addr->addr, xp_sockaddr_len(addr));
		if (result != 0) {
			lprintf(LOG_WARNING, "%04d <%s> !DATA ERROR %d (%d) connecting to client %s port %u on socket %d"
			        , ctrl_sock, user->alias, result, SOCKET_ERRNO
			        , host_ip, inet_addrport(addr), *data_sock);
			sockprintf(ctrl_sock, ctrl_sess, "425 Error %d connecting to socket", SOCKET_ERRNO);
			if (tmpfile && !(startup->options & FTP_OPT_KEEP_TEMP_FILES))
				ftp_remove(ctrl_sock, __LINE__, filename, user->alias, LOG_ERR);
			*inprogress = FALSE;
			ftp_close_socket(data_sock, data_sess, __LINE__);
			return;
		}
		if (startup->options & FTP_OPT_DEBUG_DATA)
			lprintf(LOG_DEBUG, "%04d <%s> DATA socket %d connected to %s port %u"
			        , ctrl_sock, user->alias, *data_sock, host_ip, inet_addrport(addr));

		if (protect) {
			if (start_tls(data_sock, data_sess, FALSE) || *data_sess == -1) {
				lprintf(LOG_DEBUG, "%04d <%s> !DATA ERROR activating TLS"
				        , ctrl_sock, user->alias);
				sockprintf(ctrl_sock, ctrl_sess, "425 Error activating TLS");
				if (tmpfile && !(startup->options & FTP_OPT_KEEP_TEMP_FILES))
					ftp_remove(ctrl_sock, __LINE__, filename, user->alias, LOG_ERR);
				*inprogress = FALSE;
				ftp_close_socket(data_sock, data_sess, __LINE__);
				return;
			}
		}
	} else {    /* PASV */

		if (startup->options & FTP_OPT_DEBUG_DATA) {
			addr_len = sizeof(*addr);
			if ((result = getsockname(pasv_sock, &addr->addr, &addr_len)) != 0)
				lprintf(LOG_CRIT, "%04d <%s> PASV !DATA ERROR %d (%d) getting address/port of passive socket (%u)"
				        , ctrl_sock, user->alias, result, SOCKET_ERRNO, pasv_sock);
			else
				lprintf(LOG_DEBUG, "%04d <%s> PASV DATA socket %d listening on %s port %u"
				        , ctrl_sock, user->alias, pasv_sock, host_ip, inet_addrport(addr));
		}

		if (!socket_readable(pasv_sock, TIMEOUT_SOCKET_LISTEN * 1000)) {
			lprintf(LOG_WARNING, "%04d <%s> PASV !WARNING socket not readable"
			        , ctrl_sock, user->alias);
			sockprintf(ctrl_sock, ctrl_sess, "425 Error %d selecting socket for connection", SOCKET_ERRNO);
			if (tmpfile && !(startup->options & FTP_OPT_KEEP_TEMP_FILES))
				ftp_remove(ctrl_sock, __LINE__, filename, user->alias, LOG_ERR);
			*inprogress = FALSE;
			return;
		}

		addr_len = sizeof(*addr);
#ifdef SOCKET_DEBUG_ACCEPT
		socket_debug[ctrl_sock] |= SOCKET_DEBUG_ACCEPT;
#endif
		*data_sock = accept(pasv_sock, &addr->addr, &addr_len);
#ifdef SOCKET_DEBUG_ACCEPT
		socket_debug[ctrl_sock] &= ~SOCKET_DEBUG_ACCEPT;
#endif
		if (*data_sock == INVALID_SOCKET) {
			lprintf(LOG_WARNING, "%04d <%s> PASV !DATA ERROR %d accepting connection on socket %d"
			        , ctrl_sock, user->alias, SOCKET_ERRNO, pasv_sock);
			sockprintf(ctrl_sock, ctrl_sess, "425 Error %d accepting connection", SOCKET_ERRNO);
			if (tmpfile && !(startup->options & FTP_OPT_KEEP_TEMP_FILES))
				ftp_remove(ctrl_sock, __LINE__, filename, user->alias, LOG_ERR);
			*inprogress = FALSE;
			return;
		}
		if (startup->socket_open != NULL)
			startup->socket_open(startup->cbdata, TRUE);
		if (startup->options & FTP_OPT_DEBUG_DATA)
			lprintf(LOG_DEBUG, "%04d <%s> PASV DATA socket %d connected to %s port %u"
			        , ctrl_sock, user->alias, *data_sock, host_ip, inet_addrport(addr));
		if (protect) {
			if (start_tls(data_sock, data_sess, FALSE) || *data_sess == -1) {
				lprintf(LOG_WARNING, "%04d <%s> PASV !DATA ERROR starting TLS", pasv_sock, user->alias);
				sockprintf(ctrl_sock, ctrl_sess, "425 Error negotiating TLS");
				if (tmpfile && !(startup->options & FTP_OPT_KEEP_TEMP_FILES))
					ftp_remove(ctrl_sock, __LINE__, filename, user->alias, LOG_ERR);
				*inprogress = FALSE;
				return;
			}
		}
	}

	do {

		l = 1;

		if (ioctlsocket(*data_sock, FIONBIO, &l) != 0) {
			lprintf(LOG_ERR, "%04d <%s> !DATA ERROR %d disabling socket blocking"
			        , ctrl_sock, user->alias, SOCKET_ERRNO);
			sockprintf(ctrl_sock, ctrl_sess, "425 Error %d disabling socket blocking"
			           , SOCKET_ERRNO);
			break;
		}

		if ((xfer = static_cast<xfer_t *>(malloc(sizeof(xfer_t)))) == NULL) {
			lprintf(LOG_CRIT, "%04d <%s> !DATA MALLOC FAILURE LINE %d", ctrl_sock, user->alias, __LINE__);
			sockprintf(ctrl_sock, ctrl_sess, "425 MALLOC FAILURE");
			break;
		}
		memset(xfer, 0, sizeof(xfer_t));
		xfer->ctrl_sock = ctrl_sock;
		xfer->ctrl_sess = ctrl_sess;
		xfer->data_sock = data_sock;
		xfer->data_sess = data_sess;
		xfer->inprogress = inprogress;
		xfer->aborted = aborted;
		xfer->delfile = delfile;
		xfer->tmpfile = tmpfile;
		xfer->append = append;
		xfer->filepos = filepos;
		xfer->credits = credits;
		xfer->lastactive = lastactive;
		xfer->user = user;
		xfer->client = client;
		xfer->dir = dir;
		xfer->desc = desc;
		SAFECOPY(xfer->filename, filename);
		(void)protected_uint32_adjust(&thread_count, 1);
		if (receiving)
			result = _beginthread(receive_thread, 0, (void*)xfer);
		else
			result = _beginthread(send_thread, 0, (void*)xfer);

		if (result != -1)
			return; /* success */

	} while (0);

	/* failure */
	if (tmpfile && !(startup->options & FTP_OPT_KEEP_TEMP_FILES))
		ftp_remove(ctrl_sock, __LINE__, filename, user->alias, LOG_ERR);
	*inprogress = FALSE;
}

/* convert "user name" to "user.name" or "mr. user" to "mr._user" */
char* dotname(char* in, char* out)
{
	char ch;
	int  i;

	if (in == NULL) {
		strcpy(out, "(null)");
		return out;
	}
	if (strchr(in, '.') == NULL)
		ch = '.';
	else
		ch = '_';
	for (i = 0; in[i]; i++)
		if (in[i] <= ' ')
			out[i] = ch;
		else
			out[i] = in[i];
	out[i] = 0;
	return out;
}

static BOOL can_list(lib_t *lib, dir_t *dir, user_t *user, client_t *client)
{
	if (!chk_ar(&scfg, lib->ar, user, client))
		return FALSE;
	if (dir->dirnum == scfg.sysop_dir)
		return TRUE;
	if (dir->dirnum == scfg.upload_dir)
		return TRUE;
	if (chk_ar(&scfg, dir->ar, user, client))
		return TRUE;
	return FALSE;
}

static int getdir_from_vpath(scfg_t* cfg, const char* vpath, user_t* user, client_t* client, BOOL include_upload_only)
{
	int               dir = -1;
	int               lib = -1;
	char*             filename = NULL;

	enum parsed_vpath result = parse_vpath(cfg, vpath, &lib, &dir, &filename);

	if (result == PARSED_VPATH_DIR || result == PARSED_VPATH_FULL) {
		if ((include_upload_only && (dir == cfg->sysop_dir || dir == cfg->upload_dir))
		    || user_can_access_dir(cfg, dir, user, client))
			return dir;
	}
	return -1;
}

static BOOL ftpalias(char* fullalias, char* filename, user_t* user, client_t* client, int* curdir)
{
	char* p;
	char* tp;
	const char* fname = "";
	char  line[512];
	char  alias[512];
	char  aliasfile[MAX_PATH + 1];
	int   dir = -1;
	FILE* fp;
	BOOL  result = FALSE;

	SAFECOPY(alias, fullalias);
	p = getfname(alias);
	if (p) {
		if (p != alias)
			*(p - 1) = 0;
		if (*p) {
			if (filename == NULL && p != alias)  // CWD command and a filename specified
				return FALSE;
			fname = p;
		}
	}

	for (int i = 0; i < scfg.total_dirs; ++i) {
		if (scfg.dir[i]->vshortcut[0] == '\0')
			continue;
		if (!user_can_access_dir(&scfg, i, user, client))
			continue;
		if (stricmp(scfg.dir[i]->vshortcut, alias) == 0) {
			if (curdir != NULL)
				*curdir = i;
			if (p != NULL && filename != NULL) {
				if (*p)
					sprintf(filename, "%s%s", scfg.dir[i]->path, p);
				else
					sprintf(filename, "%s%s", scfg.dir[i]->path, fname);
			}
			return TRUE;
		}
	}

	SAFEPRINTF(aliasfile, "%sftpalias.cfg", scfg.ctrl_dir);
	if ((fp = fopen(aliasfile, "r")) == NULL)
		return FALSE;

	while (!feof(fp)) {
		if (!fgets(line, sizeof(line), fp))
			break;

		p = line; /* alias */
		SKIP_WHITESPACE(p);
		if (*p == ';') /* comment */
			continue;

		tp = p;       /* terminator */
		FIND_WHITESPACE(tp);
		if (*tp)
			*tp = 0;

		if (stricmp(p, alias))   /* Not a match */
			continue;

		p = tp + 1;     /* filename */
		SKIP_WHITESPACE(p);

		tp = p;       /* terminator */
		FIND_WHITESPACE(tp);
		if (*tp)
			*tp = 0;

		if (filename == NULL /* CWD? */ && (*lastchar(p) != '/' || (*fname != 0 && strcmp(fname, alias)))) {
			fclose(fp);
			return FALSE;
		}

		if (!strnicmp(p, BBS_VIRTUAL_PATH, strlen(BBS_VIRTUAL_PATH))) {
			if ((dir = getdir_from_vpath(&scfg, p + strlen(BBS_VIRTUAL_PATH), user, client, true)) < 0)    {
				lprintf(LOG_WARNING, "0000 <%s> !Invalid virtual path: %s", user->alias, p);
				/* invalid or no access */
				continue;
			}
			p = strrchr(p, '/');
			if (p != NULL)
				p++;
			if (p != NULL && filename != NULL) {
				if (*p)
					sprintf(filename, "%s%s", scfg.dir[dir]->path, p);
				else
					sprintf(filename, "%s%s", scfg.dir[dir]->path, fname);
			}
		} else if (filename != NULL)
			strcpy(filename, p);

		result = TRUE;    /* success */
		break;
	}
	fclose(fp);
	if (curdir != NULL)
		*curdir = dir;
	return result;
}

/*
 * Parses a path into *curlib, *curdir, and sets *pp to point to the filename
 */
static int parsepath(char** pp, user_t* user, client_t* client, int* curlib, int* curdir)
{
	char   filename[MAX_PATH + 1];
	int    lib = *curlib;
	int    dir = *curdir;
	char * p = *pp;
	char * tmp;
	char * fname = strchr(p, 0);
	int    ret = 0;
	size_t len;

	if (*p == '/') {
		lib = -1;
		dir = -1;
		p++;
	}

	while (*p) {
		/* Relative path stuff */
		if (strcmp(p, "..") == 0) {
			if (dir >= 0)
				dir = -1;
			else if (lib >= 0)
				lib = -1;
			else
				ret = -1;
			p += 2;
		}
		else if (strncmp(p, "../", 3) == 0) {
			if (dir >= 0)
				dir = -1;
			else if (lib >= 0)
				lib = -1;
			else
				ret = -1;
			p += 3;
		}
		else if (strcmp(p, ".") == 0)
			p++;
		else if (strncmp(p, "./", 2) == 0)
			p += 2;
		/* Path component */
		else if (lib < 0) {
			for (lib = 0; lib < scfg.total_libs; lib++) {
				if (!chk_ar(&scfg, scfg.lib[lib]->ar, user, client))
					continue;
				len = strlen(scfg.lib[lib]->vdir);
				if (strlen(p) < len)
					continue;
				if (p[len] != 0 && p[len] != '/')
					continue;
				if (!strnicmp(scfg.lib[lib]->vdir, p, len)) {
					p += len;
					if (*p)
						p++;
					break;
				}
			}
			if (lib == scfg.total_libs) {
				SAFECOPY(filename, p);
				tmp = strchr(filename, '/');
				if (tmp != NULL)
					*tmp = 0;
				if (ftpalias(filename, filename, user, client, &dir) == TRUE && dir >= 0) {
					lib = scfg.dir[dir]->lib;
					if (strchr(p, '/') != NULL) {
						p = strchr(p, '/');
						p++;
					}
					else
						p = strchr(p, 0);
				}
				else {
					ret = -1;
					lib = -1;
					if (strchr(p, '/') != NULL) {
						p = strchr(p, '/');
						p++;
					}
					else
						p = strchr(p, 0);
				}
			}
		}
		else if (dir < 0) {
			for (dir = 0; dir < scfg.total_dirs; dir++) {
				if (scfg.dir[dir]->lib != lib)
					continue;
				if (!can_list(scfg.lib[lib], scfg.dir[dir], user, client))
					continue;
				len = strlen(scfg.dir[dir]->vdir);
				if (strlen(p) < len)
					continue;
				if (p[len] != 0 && p[len] != '/')
					continue;
				if (!strnicmp(scfg.dir[dir]->vdir, p, len)) {
					p += len;
					if (*p)
						p++;
					break;
				}
			}
			if (dir == scfg.total_dirs) {
				ret = -1;
				dir = -1;
				if (strchr(p, '/') != NULL) {
					p = strchr(p, '/');
					p++;
				}
				else
					p = strchr(p, 0);
			}
		}
		else {  // Filename
			if (strchr(p, '/') != NULL) {
				ret = -1;
				p = strchr(p, '/');
				p++;
			}
			else {
				fname = p;
				p += strlen(fname);
			}
		}
	}
	*curdir = dir;
	*curlib = lib;
	*pp = fname;
	return ret;
}

char* root_dir(char* path)
{
	char*       p;
	static char root[MAX_PATH + 1];

	SAFECOPY(root, path);

	if (!strncmp(root, "\\\\", 2)) {   /* network path */
		p = strchr(root + 2, '\\');
		if (p)
			p = strchr(p + 1, '\\');
		if (p)
			*(p + 1) = 0;           /* truncate at \\computer\sharename\ */
	}
	else if (!strncmp(root + 1, ":/", 2) || !strncmp(root + 1, ":\\", 2))
		root[3] = 0;
	else if (*root == '/' || *root == '\\')
		root[1] = 0;

	return root;
}

char* genvpath(int lib, int dir, char* str)
{
	strcpy(str, "/");
	if (lib < 0)
		return str;
	strcat(str, scfg.lib[lib]->vdir);
	strcat(str, "/");
	if (dir < 0)
		return str;
	strcat(str, scfg.dir[dir]->vdir);
	strcat(str, "/");
	return str;
}

void ftp_printfile(SOCKET sock, CRYPT_SESSION sess, const char* name, unsigned code)
{
	char     path[MAX_PATH + 1];
	char     buf[512];
	FILE*    fp;
	unsigned i;

	SAFEPRINTF2(path, "%sftp%s.txt", scfg.text_dir, name);
	if ((fp = fopen(path, "rb")) != NULL) {
		i = 0;
		while (!feof(fp)) {
			if (!fgets(buf, sizeof(buf), fp))
				break;
			truncsp(buf);
			if (!i)
				sockprintf(sock, sess, "%u-%s", code, buf);
			else
				sockprintf(sock, sess, " %s", buf);
			i++;
		}
		fclose(fp);
	}
}

static BOOL ftp_hacklog(const char* prot, char* user, char* text, char* host, union xp_sockaddr* addr)
{
#ifdef _WIN32
	if (startup->sound.hack[0] && !sound_muted(&scfg))
		PlaySound(startup->sound.hack, NULL, SND_ASYNC | SND_FILENAME);
#endif

	return hacklog(&scfg, &mqtt, prot, user, text, host, addr);
}

/****************************************************************************/
/* Consecutive failed login (possible password hack) attempt tracking		*/
/****************************************************************************/

static BOOL badlogin(SOCKET sock, CRYPT_SESSION sess, ulong* login_attempts
                     , char* user, char* passwd, client_t* client, union xp_sockaddr* addr)
{
	char            tmp[128];
	ulong           count;
	login_attempt_t attempt;

	if (addr != NULL) {
		count = loginFailure(startup->login_attempt_list, addr, client->protocol, user, passwd, &attempt);
		if (count > 1)
			lprintf(LOG_NOTICE, "%04d [%s] !%lu " STR_FAILED_LOGIN_ATTEMPTS " in %s"
			        , sock, client->addr, count, duration_estimate_to_vstr(attempt.time - attempt.first, tmp, sizeof tmp, 1, 1));
		mqtt_user_login_fail(&mqtt, client, user);
		if (startup->login_attempt.hack_threshold && count >= startup->login_attempt.hack_threshold)
			ftp_hacklog("FTP LOGIN", user, passwd, client->host, addr);
		if (startup->login_attempt.filter_threshold && count >= startup->login_attempt.filter_threshold) {
			char reason[128];
			snprintf(reason, sizeof reason, "%lu " STR_FAILED_LOGIN_ATTEMPTS " in %s"
			         , count, duration_estimate_to_str(attempt.time - attempt.first, tmp, sizeof tmp, 1, 1));
			filter_ip(&scfg, client->protocol, reason, client->host, client->addr, user, /* fname: */ NULL, startup->login_attempt.filter_duration);
		}
		if (count > *login_attempts)
			*login_attempts = count;
	} else
		(*login_attempts)++;

	mswait(startup->login_attempt.delay);   /* As recommended by RFC2577 */

	if ((*login_attempts) >= 3) {
		sockprintf(sock, sess, "421 Too many failed login attempts.");
		return TRUE;
	}
	ftp_printfile(sock, sess, "badlogin", 530);
	sockprintf(sock, sess, "530 Invalid login.");
	return FALSE;
}

static char* ftp_tmpfname(char* fname, const char* ext, SOCKET sock)
{
	safe_snprintf(fname, MAX_PATH, "%sSBBS_FTP.%x%x%x%lx.%s"
	              , scfg.temp_dir, getpid(), sock, rand(), (ulong)clock(), ext);
	return fname;
}

#if defined(__GNUC__)   // Catch printf-format errors
static BOOL send_mlsx(FILE *fp, SOCKET sock, CRYPT_SESSION sess, const char *format, ...) __attribute__ ((format (printf, 4, 5)));
#endif
static BOOL send_mlsx(FILE *fp, SOCKET sock, CRYPT_SESSION sess, const char *format, ...)
{
	va_list va;
	char *  str;
	int     rval;

	if (fp == NULL && sock == INVALID_SOCKET)
		return FALSE;
	va_start(va, format);
	rval = vasprintf(&str, format, va);
	va_end(va);
	if (rval == -1)
		return FALSE;
	if (fp != NULL)
		fprintf(fp, "%s\r\n", str);
	else
		sockprintf(sock, sess, " %s", str);
	free(str);
	return TRUE;
}

static char *get_unique(const char *path, char *uniq)
{
	BYTE digest[MD5_DIGEST_SIZE];

	if (path == NULL)
		return NULL;

	MD5_calc(digest, path, strlen(path));
	MD5_hex(uniq, digest);
	return uniq;
}

static BOOL send_mlsx_entry(FILE *fp, SOCKET sock, CRYPT_SESSION sess, unsigned feats, const char *type, const char *perm, uint64_t size, time_t modify, const char *owner, const char *unique, time_t ul, const char *fname)
{
	char      line[1024];
	char *    end;
	BOOL      need_owner = FALSE;
	struct tm t;

	end = line;
	*end = 0;
	if (type != NULL && (feats & MLSX_TYPE))
		end += sprintf(end, "Type=%s;", type);
	if (perm != NULL && (feats & MLSX_PERM))
		end += sprintf(end, "Perm=%s;", perm);
	if (size != UINT64_MAX && (feats & MLSX_SIZE))
		end += sprintf(end, "Size=%" PRIu64 ";", size);
	if (modify > 0 && (feats & MLSX_MODIFY)) {
		t = *gmtime(&modify);
		end += sprintf(end, "Modify=%04d%02d%02d%02d%02d%02d;",
		               t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
		               t.tm_hour, t.tm_min, t.tm_sec);
	}
	if (unique != NULL && (feats & MLSX_UNIQUE))
		end += sprintf(end, "Unique=%s;", unique);
	if (ul != 0 && (feats & MLSX_CREATE)) {
		t = *gmtime(&ul);
		end += sprintf(end, "Create=%04d%02d%02d%02d%02d%02d;",
		               t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
		               t.tm_hour, t.tm_min, t.tm_sec);
	}
	// Owner can contain percents, so let send_mlsx() deal with it
	if (owner != NULL && (feats & MLSX_OWNER)) {
		strcat(end, "UNIX.ownername=%s;");
		need_owner = TRUE;
	}
	strcat(end, " %s");
	if (need_owner)
		return send_mlsx(fp, sock, sess, line, owner, fname == NULL ? "" : fname);
	return send_mlsx(fp, sock, sess, line, fname == NULL ? "" : fname);
}

static BOOL write_local_mlsx(FILE *fp, SOCKET sock, CRYPT_SESSION sess, unsigned feats, const char *path, BOOL full_path)
{
	const char *type;
	char        permstr[11];
	char *      p;
	BOOL        is_file = FALSE;
	struct stat st;

	if (stat(path, &st) != 0)
		return FALSE;
	if (!strcmp(path, "."))
		type = "cdir";
	else if (!strcmp(path, ".."))
		type = "pdir";
	else if (*lastchar(path) == '/')    /* is directory */
		type = "dir";
	else {
		is_file = TRUE;
		type = "file";
	}
	// TODO: Check for deletability 'd'
	// TODO: Check for renamability 'f'
	p = permstr;
	if (is_file) {
		if (access(path, W_OK) == 0) {
			// Can append ('a') and write ('w')
			*(p++) = 'a';
			*(p++) = 'w';
		}
		if (access(path, R_OK) == 0) {
			// Can read ('r')
			*(p++) = 'r';
		}
	}
	else {
		// TODO: Check these on Windows...
		if (access(path, W_OK) == 0) {
			// Can create files ('c'), directories ('m') and delete files ('p')
			*(p++) = 'c';
			*(p++) = 'm';
			*(p++) = 'p';
		}
		if (access(path, R_OK) == 0) {
			// Can change to the directory ('e'), and list files ('l')
			*(p++) = 'e';
			*(p++) = 'l';
		}
	}
	*p = 0;
	if (is_file)
		full_path = FALSE;
	return send_mlsx_entry(fp, sock, sess, feats, type, permstr, (uint64_t)st.st_size, st.st_mtime, NULL, NULL, st.st_ctime, full_path ? path : getfname(path));
}

/*
 * Nobody can do anything but list files and change to dirs.
 */
static void get_libperm(lib_t *lib, user_t *user, client_t *client, char *permstr)
{
	char *p = permstr;

	if (chk_ar(&scfg, lib->ar, user, client)) {
		//*(p++) = 'a';	// File may be appended to
		//*(p++) = 'c';	// Files may be created in dir
		//*(p++) = 'd';	// Item may be depeted (dir or file)
		*(p++) = 'e';   // Can change to the dir
		//*(p++) = 'f';	// Item may be renamed
		*(p++) = 'l';   // Directory contents can be listed
		//*(p++) = 'm';	// New subdirectories may be created
		//*(p++) = 'p';	// Files/Dirs in directory may be deleted
		//*(p++) = 'r';	// File may be retrieved
		//*(p++) = 'w';	// File may be overwritten
	}
	*p = 0;
}

static BOOL can_upload(lib_t *lib, dir_t *dir, user_t *user, client_t *client)
{
	if (!chk_ar(&scfg, lib->ar, user, client))
		return FALSE;
	if (user->rest & FLAG('U'))
		return FALSE;
	if (user_is_dirop(&scfg, dir->dirnum, user, client))
		return TRUE;
	// The rest can only upload if there's room
	if (dir->maxfiles && getfiles(&scfg, dir->dirnum) >= dir->maxfiles)
		return FALSE;
	if (dir->dirnum == scfg.sysop_dir)
		return TRUE;
	if (dir->dirnum == scfg.upload_dir)
		return TRUE;
	if (!chk_ar(&scfg, lib->ul_ar, user, client))
		return FALSE;
	if (chk_ar(&scfg, dir->ul_ar, user, client))
		return TRUE;
	if ((user->exempt & FLAG('U')))
		return TRUE;
	return FALSE;
}

static BOOL can_delete_files(lib_t *lib, dir_t *dir, user_t *user, client_t *client)
{
	if (!chk_ar(&scfg, lib->ar, user, client))
		return FALSE;
	if (user->rest & FLAG('D'))
		return FALSE;
	if (!chk_ar(&scfg, dir->ar, user, client))
		return FALSE;
	if (user_is_dirop(&scfg, dir->dirnum, user, client))
		return TRUE;
	if (user->exempt & FLAG('R'))
		return TRUE;
	return FALSE;
}

static void get_dirperm(lib_t *lib, dir_t *dir, user_t *user, client_t *client, char *permstr)
{
	char *p = permstr;

	//*(p++) = 'a';	// File may be appended to
	if (can_upload(lib, dir, user, client))
		*(p++) = 'c';   // Files may be created in dir
	//*(p++) = 'd';	// Item may be depeted (dir or file)
	if (can_list(lib, dir, user, client)) {
		*(p++) = 'e';   // Can change to the dir
		//*(p++) = 'f';	// Item may be renamed
		*(p++) = 'l';   // Directory contents can be listed
	}
	//*(p++) = 'm';	// New subdirectories may be created
	if (can_delete_files(lib, dir, user, client))
		*(p++) = 'p';   // Files/Dirs in directory may be deleted
	//*(p++) = 'r';	// File may be retrieved
	//*(p++) = 'w';	// File may be overwritten
	*p = 0;
}

static BOOL can_append(lib_t *lib, dir_t *dir, user_t *user, client_t *client, file_t *file)
{
	if (!chk_ar(&scfg, lib->ar, user, client))
		return FALSE;
	if (user->rest & FLAG('U'))
		return FALSE;
	if (dir->dirnum != scfg.sysop_dir && dir->dirnum != scfg.upload_dir && !chk_ar(&scfg, dir->ar, user, client))
		return FALSE;
	if (!user_is_dirop(&scfg, dir->dirnum, user, client) && !(user->exempt & FLAG('U'))) {
		if (!chk_ar(&scfg, dir->ul_ar, user, client) || !chk_ar(&scfg, lib->ul_ar, user, client))
			return FALSE;
	}
	if (file->from == NULL || stricmp(file->from, user->alias) != 0)
		return FALSE;
	return TRUE;
}

static BOOL can_delete(lib_t *lib, dir_t *dir, user_t *user, client_t *client, file_t *file)
{
	if (user->rest & FLAG('D'))
		return FALSE;
	if (!chk_ar(&scfg, lib->ar, user, client))
		return FALSE;
	if (!chk_ar(&scfg, dir->ar, user, client))
		return FALSE;
	if (!user_is_dirop(&scfg, dir->dirnum, user, client))
		return FALSE;
	if (!(user->exempt & FLAG('R')))
		return FALSE;
	return TRUE;
}

static BOOL can_download(lib_t *lib, dir_t *dir, user_t *user, client_t *client, file_t *file)
{
	return user_can_download(&scfg, dir->dirnum, user, client,  /* reason */ NULL);
}

static void get_fileperm(lib_t *lib, dir_t *dir, user_t *user, client_t *client, file_t *file, char *permstr)
{
	char *p = permstr;

	if (can_append(lib, dir, user, client, file))
		*(p++) = 'a';   // File may be appended to
	//*(p++) = 'c';	// Files may be created in dir
	if (can_delete(lib, dir, user, client, file))
		*(p++) = 'd';   // Item may be depeted (dir or file)
	//*(p++) = 'e';	// Can change to the dir
	//*(p++) = 'f';	// Item may be renamed
	//*(p++) = 'l';	// Directory contents can be listed
	//*(p++) = 'm';	// New subdirectories may be created
	//*(p++) = 'p';	// Files/Dirs in directory may be deleted
	if (can_download(lib, dir, user, client, file))
		*(p++) = 'r';   // File may be retrieved
	//*(p++) = 'w';	// File may be overwritten
	*p = 0;
}

static char* get_owner_name(file_t *file, char *namestr, size_t size)
{
	char *p;

	if (file) {
		if (file->hdr.attr & MSG_ANONYMOUS)
			strlcpy(namestr, ANONYMOUS, size);
		else
			strlcpy(namestr, file->from, size);
	}
	else
		strlcpy(namestr, scfg.sys_id, size);

	// Now ensure it's an RCHAR string.
	for (p = namestr; *p; p++) {
		if (*p >= '!' && *p <= ')')
			continue;
		else if (*p >= '+' && *p <= ':')
			continue;
		else if (*p >= '?' && *p <= 'Z')
			continue;
		else if (*p == '\\')
			continue;
		else if (*p == '^')
			continue;
		else if (*p == '_')
			continue;
		else if (*p >= 'a' && *p <= 'z')
			continue;
		else if (*p == ' ')
			*p = '.';
		else
			*p = '_';
	}
	return namestr;
}

static void ctrl_thread(void* arg)
{
	unsigned          mlsx_feats = (MLSX_TYPE | MLSX_PERM | MLSX_SIZE | MLSX_MODIFY | MLSX_OWNER | MLSX_UNIQUE | MLSX_CREATE);
	char              buf[512];
	char              str[128];
	char              tmp[128];
	char              uniq[33];
	char              owner[33];
	char              error[256];
	char*             cmd;
	char*             p;
	char*             np;
	char*             tp;
	char*             dp;
	char*             ap;
	const char*       filespec;
	const char*       mode = "active";
	char              old_char;
	char              password[64];
	char              fname[MAX_PATH + 1];
	char              qwkfile[MAX_PATH + 1];
	char              aliasfile[MAX_PATH + 1];
	char              aliaspath[MAX_PATH + 1];
	char              mls_path[MAX_PATH + 1];
	char *            mls_fname;
	char              permstr[11];
	char              aliasline[512];
	char              desc[501] = "";
	char              sys_pass[128];
	char              host_name[256];
	char              host_ip[INET6_ADDRSTRLEN];
	char              data_ip[INET6_ADDRSTRLEN];
	uint16_t          data_port;
	char              path[MAX_PATH + 1];
	char              local_dir[MAX_PATH + 1];
	char              ren_from[MAX_PATH + 1] = "";
	WORD              port;
	uint32_t          ip_addr;
	socklen_t         addr_len;
	unsigned          h1, h2, h3, h4;
	u_short           p1, p2; /* For PORT command */
	int               i;
	int               rd;
	int               result;
	int               lib;
	int               dir;
	int               curlib = -1;
	int               curdir = -1;
	int               orglib;
	int               orgdir;
	fmutex_t          mutex_file = fmutex_init();
	long              filepos = 0L;
	long              timeleft;
	ulong             l;
	ulong             login_attempts = 0;
	uint64_t          avail; /* disk space */
	uint              count;
	BOOL              detail;
	BOOL              success;
	BOOL              getdate;
	BOOL              getsize;
	BOOL              delecmd;
	BOOL              delfile;
	BOOL              tmpfile;
	BOOL              credits;
	BOOL              filedat = FALSE;
	volatile BOOL     transfer_inprogress;
	volatile BOOL     transfer_aborted;
	BOOL              sysop = FALSE;
	BOOL              local_fsys = FALSE;
	BOOL              alias_dir;
	BOOL              append;
	BOOL              reuseaddr;
	FILE*             fp;
	FILE*             alias_fp;
	SOCKET            sock;
	SOCKET            tmp_sock;
	SOCKET            pasv_sock = INVALID_SOCKET;
	CRYPT_SESSION     pasv_sess = -1;
	SOCKET            data_sock = INVALID_SOCKET;
	CRYPT_SESSION     data_sess = -1;
	HOSTENT*          host;
	union xp_sockaddr addr;
	union xp_sockaddr data_addr;
	union xp_sockaddr pasv_addr;
	ftp_t             ftp = *(ftp_t*)arg;
	user_t            user;
	time_t            t;
	time_t            now;
	time_t            logintime = 0;
	time_t            lastactive;
	time_t            file_date;
	off_t             file_size;
	node_t            node;
	client_t          client;
	struct tm         tm;
	struct tm         cur_tm;
	login_attempt_t   attempted;
	CRYPT_SESSION     sess = -1;
	BOOL              got_pbsz = FALSE;
	BOOL              protection = FALSE;

	SetThreadName("sbbs/ftpControl");
	thread_up(TRUE /* setuid */);

	lastactive = time(NULL);

	sock = ftp.socket;
	memcpy(&data_addr, &ftp.client_addr, ftp.client_addr_len);
	/* Default data port is ctrl port-1 */
	data_port = inet_addrport(&data_addr) - 1;

	lprintf(LOG_DEBUG, "%04d Session thread started", sock);

	free(arg);

#ifdef _WIN32
	if (startup->sound.answer[0] && !sound_muted(&scfg))
		PlaySound(startup->sound.answer, NULL, SND_ASYNC | SND_FILENAME);
#endif

	transfer_inprogress = FALSE;
	transfer_aborted = FALSE;

	l = 1;

	if ((i = ioctlsocket(sock, FIONBIO, &l)) != 0) {
		lprintf(LOG_ERR, "%04d !ERROR %d (%d) disabling socket blocking"
		        , sock, i, SOCKET_ERRNO);
		sockprintf(sock, sess, "425 Error %d disabling socket blocking"
		           , SOCKET_ERRNO);
		ftp_close_socket(&sock, &sess, __LINE__);
		thread_down();
		return;
	}

	memset(&user, 0, sizeof(user));

	inet_addrtop(&ftp.client_addr, host_ip, sizeof(host_ip));

	union xp_sockaddr local_addr;
	memset(&local_addr, 0, sizeof(local_addr));
	addr_len = sizeof(local_addr);
	if (getsockname(sock, (struct sockaddr *)&local_addr, &addr_len) != 0) {
		lprintf(LOG_CRIT, "%04d [%s] !ERROR %d getting local address/port of socket"
		        , sock, host_ip, SOCKET_ERRNO);
		ftp_close_socket(&sock, &sess, __LINE__);
		thread_down();
		return;
	}
	char local_ip[INET6_ADDRSTRLEN];
	inet_addrtop(&local_addr, local_ip, sizeof local_ip);

	lprintf(LOG_INFO, "%04d [%s] Connection accepted on %s port %u from port %u"
	        , sock, host_ip, local_ip, inet_addrport(&local_addr), inet_addrport(&ftp.client_addr));

	SAFECOPY(host_name, STR_NO_HOSTNAME);
	if (!(startup->options & FTP_OPT_NO_HOST_LOOKUP)) {
		getnameinfo(&ftp.client_addr.addr, sizeof(ftp.client_addr), host_name, sizeof(host_name), NULL, 0, NI_NAMEREQD);
		lprintf(LOG_INFO, "%04d [%s] Hostname: %s", sock, host_ip, host_name);
	}

	ulong banned = loginBanned(&scfg, startup->login_attempt_list, sock, host_name, startup->login_attempt, &attempted);
	if (banned) {
		char ban_duration[128];
		lprintf(LOG_NOTICE, "%04d [%s] !TEMPORARY BAN (%lu login attempts, last: %s) - remaining: %s"
		        , sock, host_ip, attempted.count - attempted.dupes, attempted.user
		        , duration_estimate_to_vstr(banned, ban_duration, sizeof ban_duration, 1, 1));
		sockprintf(sock, sess, "550 Access denied.");
		ftp_close_socket(&sock, &sess, __LINE__);
		thread_down();
		return;
	}

	struct trash trash;
	if (ip_can->listed(host_ip, nullptr, &trash)) {
		if (!trash.quiet) {
			char details[128];
			lprintf(LOG_NOTICE, "%04d [%s] !CLIENT BLOCKED in %s %s", sock, host_ip, ip_can->fname, trash_details(&trash, details, sizeof details));
		}
		sockprintf(sock, sess, "550 Access denied.");
		ftp_close_socket(&sock, &sess, __LINE__);
		thread_down();
		return;
	}
	if (host_can->listed(host_name, nullptr, &trash)) {
		if (!trash.quiet) {
			char details[128];
			lprintf(LOG_NOTICE, "%04d [%s] !CLIENT BLOCKED in %s: %s %s", sock, host_ip, host_can->fname, host_name, trash_details(&trash, details, sizeof details));
		}
		sockprintf(sock, sess, "550 Access denied.");
		ftp_close_socket(&sock, &sess, __LINE__);
		thread_down();
		return;
	}

	/* For PASV mode */
	addr_len = sizeof(pasv_addr);
	if ((result = getsockname(sock, &pasv_addr.addr, &addr_len)) != 0) {
		lprintf(LOG_CRIT, "%04d !ERROR %d (%d) getting address/port of socket", sock, result, SOCKET_ERRNO);
		sockprintf(sock, sess, "425 Error %d getting address/port", SOCKET_ERRNO);
		ftp_close_socket(&sock, &sess, __LINE__);
		thread_down();
		return;
	}

	uint32_t client_count = protected_uint32_adjust_fetch(&active_clients, 1);
	if (client_count > client_highwater) {
		client_highwater = client_count;
		if (client_highwater > 1)
			lprintf(LOG_NOTICE, "%04d New active client highwater mark: %u"
			        , sock, client_highwater);
		mqtt_pub_uintval(&mqtt, TOPIC_SERVER, "highwater", mqtt.highwater = client_highwater);
	}
	update_clients();

	/* Initialize client display */
	client.size = sizeof(client);
	client.time = time32(NULL);
	SAFECOPY(client.addr, host_ip);
	SAFECOPY(client.host, host_name);
	client.port = inet_addrport(&ftp.client_addr);
	SAFECOPY(client.protocol, "FTP");
	SAFECOPY(client.user, STR_UNKNOWN_USER);
	client.usernum = 0;
	client_on(sock, &client, FALSE /* update */);

	if (startup->login_attempt.throttle
	    && (login_attempts = loginAttempts(startup->login_attempt_list, &ftp.client_addr)) > 1) {
		lprintf(LOG_DEBUG, "%04d Throttling suspicious connection from: %s (%lu login attempts)"
		        , sock, host_ip, login_attempts);
		mswait(login_attempts * startup->login_attempt.throttle);
	}

	sockprintf(sock, sess, "220-%s (%s)", scfg.sys_name, server_host_name());
	sockprintf(sock, sess, " Synchronet FTP Server %s%c-%s Ready"
	           , VERSION, REVISION, PLATFORM_DESC);
	sprintf(str, "%sftplogin.txt", scfg.text_dir);
	if ((fp = fopen(str, "rb")) != NULL) {
		while (!feof(fp)) {
			if (!fgets(buf, sizeof(buf), fp))
				break;
			truncsp(buf);
			sockprintf(sock, sess, " %s", buf);
		}
		fclose(fp);
	}
	sockprintf(sock, sess, "220 Please enter your user name.");

#ifdef SOCKET_DEBUG_CTRL
	socket_debug[sock] |= SOCKET_DEBUG_CTRL;
#endif
	while (1) {

#ifdef SOCKET_DEBUG_READLINE
		socket_debug[sock] |= SOCKET_DEBUG_READLINE;
#endif
		rd = sockreadline(sock, sess, buf, sizeof(buf), &lastactive);
#ifdef SOCKET_DEBUG_READLINE
		socket_debug[sock] &= ~SOCKET_DEBUG_READLINE;
#endif
		if (rd < 1) {
			if (transfer_inprogress == TRUE) {
				lprintf(LOG_WARNING, "%04d <%s> !Aborting transfer due to CTRL socket receive error", sock, user.number ? user.alias : host_ip);
				transfer_aborted = TRUE;
			}
			break;
		}
		truncsp(buf);
		lastactive = time(NULL);
		cmd = buf;
		while (((BYTE)*cmd) == TELNET_IAC) {
			cmd++;
			lprintf(LOG_DEBUG, "%04d <%s> RX%s: Telnet cmd: %s", sock, user.number ? user.alias : host_ip, sess == -1 ? "" : "S", telnet_cmd_desc(*cmd));
			cmd++;
		}
		while (*cmd && *cmd < ' ') {
			lprintf(LOG_DEBUG, "%04d <%s> RX%s: %d (0x%02X)", sock, user.number ? user.alias : host_ip, sess == -1 ? "" : "S", (BYTE)*cmd, (BYTE)*cmd);
			cmd++;
		}
		if (!(*cmd))
			continue;
		if (request_rate_limiter->allowRequest(host_ip) == false) {
			lprintf(LOG_NOTICE, "%04d <%s> Too many requests per rate limit (%u over %us)"
				, sock, user.number ? user.alias : host_ip, request_rate_limiter->maxRequests, request_rate_limiter->timeWindowSeconds);
			sockprintf(sock, sess, "421 Too many requests, try again later.");
			break;
		}
		if (startup->options & FTP_OPT_DEBUG_RX)
			lprintf(LOG_DEBUG, "%04d <%s> RX%s: %s", sock, user.number ? user.alias : host_ip, sess == -1 ? "" : "S", cmd);
		if (!stricmp(cmd, "NOOP")) {
			sockprintf(sock, sess, "200 NOOP command successful.");
			continue;
		}
		if (!stricmp(cmd, "HELP SITE") || !stricmp(cmd, "SITE HELP")) {
			sockprintf(sock, sess, "214-The following SITE commands are recognized (* => unimplemented):");
			sockprintf(sock, sess, " HELP    VER     WHO     UPTIME");
			if (user_is_sysop(&user))
				sockprintf(sock, sess,
				           " RECYCLE [ALL]");
			if (sysop)
				sockprintf(sock, sess,
				           " EXEC <cmd>");
			sockprintf(sock, sess, "214 Direct comments to sysop@%s.", scfg.sys_inetaddr);
			continue;
		}
		if (!strnicmp(cmd, "HELP", 4)) {
			sockprintf(sock, sess, "214-The following commands are recognized (* => unimplemented, # => extension):");
			sockprintf(sock, sess, " USER    PASS    CWD     XCWD    CDUP    XCUP    PWD     XPWD");
			sockprintf(sock, sess, " QUIT    REIN    PORT    PASV    LIST    NLST    NOOP    HELP");
			sockprintf(sock, sess, " SIZE    MDTM    RETR    STOR    REST    ALLO    ABOR    SYST");
			sockprintf(sock, sess, " TYPE    STRU    MODE    SITE    RNFR*   RNTO*   DELE*   DESC#");
			sockprintf(sock, sess, " FEAT#   OPTS#   EPRT    EPSV    AUTH#   PBSZ#   PROT#   CCC#");
			sockprintf(sock, sess, " MLSD#");
			sockprintf(sock, sess, "214 Direct comments to sysop@%s.", scfg.sys_inetaddr);
			continue;
		}
		if (!stricmp(cmd, "FEAT")) {
			sockprintf(sock, sess, "211-The following additional (post-RFC949) features are supported:");
			sockprintf(sock, sess, " DESC");
			sockprintf(sock, sess, " MDTM");
			sockprintf(sock, sess, " SIZE");
			sockprintf(sock, sess, " REST STREAM");
			sockprintf(sock, sess, " AUTH TLS");
			sockprintf(sock, sess, " PBSZ");
			sockprintf(sock, sess, " PROT");
			sockprintf(sock, sess, " MLST Type%s;Perm%s;Size%s;Modify%s;UNIX.ownername%s;Unique%s;Create%s",
			           (mlsx_feats & MLSX_TYPE) ? "*" : "",
			           (mlsx_feats & MLSX_PERM) ? "*" : "",
			           (mlsx_feats & MLSX_SIZE) ? "*" : "",
			           (mlsx_feats & MLSX_MODIFY) ? "*" : "",
			           (mlsx_feats & MLSX_OWNER) ? "*" : "",
			           (mlsx_feats & MLSX_UNIQUE) ? "*" : "",
			           (mlsx_feats & MLSX_CREATE) ? "*" : ""
			           );
			sockprintf(sock, sess, " TVFS");
			sockprintf(sock, sess, "211 End");
			continue;
		}
		if (!strnicmp(cmd, "OPTS MLST", 9)) {
			if (cmd[9] == 0) {
				mlsx_feats = 0;
				continue;
			}
			if (cmd[9] != ' ') {
				sockprintf(sock, sess, "501 Option not supported.");
				continue;
			}
			mlsx_feats = 0;
			for (p = cmd; *p; p++)
				*p = toupper(*p);
			if (strstr(cmd, "TYPE;"))
				mlsx_feats |= MLSX_TYPE;
			if (strstr(cmd, "PERM;"))
				mlsx_feats |= MLSX_PERM;
			if (strstr(cmd, "SIZE;"))
				mlsx_feats |= MLSX_SIZE;
			if (strstr(cmd, "MODIFY;"))
				mlsx_feats |= MLSX_MODIFY;
			if (strstr(cmd, "UNIX.OWNERNAME;"))
				mlsx_feats |= MLSX_OWNER;
			if (strstr(cmd, "UNIQUE;"))
				mlsx_feats |= MLSX_UNIQUE;
			if (strstr(cmd, "CREATE;"))
				mlsx_feats |= MLSX_CREATE;
			sockprintf(sock, sess, "200 %s%s%s%s%s%s%s",
			           (mlsx_feats & MLSX_TYPE) ? "Type;" : "",
			           (mlsx_feats & MLSX_PERM) ? "Perm;" : "",
			           (mlsx_feats & MLSX_SIZE) ? "Size;" : "",
			           (mlsx_feats & MLSX_MODIFY) ? "Modify;" : "",
			           (mlsx_feats & MLSX_OWNER) ? "UNIX.ownername;" : "",
			           (mlsx_feats & MLSX_UNIQUE) ? "Unique;" : "",
			           (mlsx_feats & MLSX_CREATE) ? "Create;" : ""
			           );
			continue;
		}
		if (!strnicmp(cmd, "OPTS", 4)) {
			sockprintf(sock, sess, "501 Option not supported.");
			continue;
		}
		if (!stricmp(cmd, "QUIT")) {
			ftp_printfile(sock, sess, "bye", 221);
			sockprintf(sock, sess, "221 Goodbye. Closing control connection.");
			break;
		}
		if (!strnicmp(cmd, "USER ", 5)) {
			sysop = FALSE;
			user.number = 0;
			fmutex_close(&mutex_file);
			p = cmd + 5;
			SKIP_WHITESPACE(p);
			truncsp(p);
			SAFECOPY(user.alias, p);
			user.number = find_login_id(&scfg, user.alias);
			if (!user.number && (stricmp(user.alias, "anonymous") == 0 || stricmp(user.alias, "ftp") == 0))
				user.number = matchuser(&scfg, "guest", FALSE);
			if (user.number && getuserdat(&scfg, &user) == 0 && user.pass[0] == 0)
				sockprintf(sock, sess, "331 User name okay, give your full e-mail address as password.");
			else
				sockprintf(sock, sess, "331 User name okay, need password.");
			user.number = 0;
			continue;
		}
		if (!strnicmp(cmd, "PASS ", 5) && user.alias[0]) {
			user.number = 0;
			fmutex_close(&mutex_file);
			p = cmd + 5;
			SKIP_WHITESPACE(p);

			SAFECOPY(password, p);
			uint usernum = find_login_id(&scfg, user.alias);
			if (usernum == 0) {
				if (scfg.sys_misc & SM_ECHO_PW)
					lprintf(LOG_NOTICE, "%04d !UNKNOWN USER: '%s' (password: %s)", sock, user.alias, p);
				else
					lprintf(LOG_NOTICE, "%04d !UNKNOWN USER: '%s'", sock, user.alias);
				if (badlogin(sock, sess, &login_attempts, user.alias, p, &client, &ftp.client_addr))
					break;
				continue;
			}
			user.number = usernum;
			if ((i = getuserdat(&scfg, &user)) != 0) {
				lprintf(LOG_ERR, "%04d <%s> !ERROR %d (errno %d %s) getting data for user #%d"
				        , sock, user.alias, i, errno, safe_strerror(errno, error, sizeof error), usernum);
				sockprintf(sock, sess, "530 Database error %d", i);
				continue;
			}
			if (user.misc & (DELETED | INACTIVE)) {
				lprintf(LOG_NOTICE, "%04d <%s> !DELETED or INACTIVE user #%d"
				        , sock, user.alias, user.number);
				user.number = 0;
				if (badlogin(sock, sess, &login_attempts, NULL, NULL, NULL, NULL))
					break;
				continue;
			}
			if (user.rest & FLAG('T')) {
				lprintf(LOG_NOTICE, "%04d <%s> !T RESTRICTED user #%d"
				        , sock, user.alias, user.number);
				user.number = 0;
				if (badlogin(sock, sess, &login_attempts, NULL, NULL, NULL, NULL))
					break;
				continue;
			}
			if (user.ltoday >= scfg.level_callsperday[user.level]
			    && !(user.exempt & FLAG('L'))) {
				lprintf(LOG_NOTICE, "%04d <%s> !MAXIMUM LOGONS (%d) reached for level %u"
				        , sock, user.alias, scfg.level_callsperday[user.level], user.level);
				sockprintf(sock, sess, "530 Maximum logons per day reached.");
				user.number = 0;
				continue;
			}
			if (user.rest & FLAG('L') && user.ltoday >= 1) {
				lprintf(LOG_NOTICE, "%04d <%s> !L RESTRICTED user already on today"
				        , sock, user.alias);
				sockprintf(sock, sess, "530 Maximum logons per day reached.");
				user.number = 0;
				continue;
			}
			if (!chk_ars(&scfg, startup->login_ars, &user, &client)) {
				lprintf(LOG_NOTICE, "%04d <%s> !Insufficient server access: %s"
				        , sock, user.alias, startup->login_ars);
				sockprintf(sock, sess, "530 Insufficient server access.");
				user.number = 0;
				continue;
			}

			SAFEPRINTF2(sys_pass, "%s:%s", user.pass, scfg.sys_pass);
			if (!user.pass[0]) { /* Guest/Anonymous */
				if (trashcan(&scfg, password, "email")) {
					lprintf(LOG_NOTICE, "%04d <%s> !BLOCKED e-mail address: %s", sock, user.alias, password);
					user.number = 0;
					if (badlogin(sock, sess, &login_attempts, NULL, NULL, NULL, NULL))
						break;
					continue;
				}
				lprintf(LOG_INFO, "%04d <%s> identity: %s", sock, user.alias, password);
				putuserstr(&scfg, user.number, USER_NETMAIL, password);
			}
			else if (user_is_sysop(&user) && !stricmp(password, sys_pass)) {
				if (scfg.sys_misc & SM_R_SYSOP) {
					lprintf(LOG_INFO, "%04d <%s> Sysop access granted", sock, user.alias);
					sysop = TRUE;
				} else
					lprintf(LOG_NOTICE, "%04d <%s> Remote sysop access disabled", sock, user.alias);
			}
			else if (stricmp(password, user.pass)) {
				if (scfg.sys_misc & SM_ECHO_PW)
					lprintf(LOG_NOTICE, "%04d <%s> !FAILED Password attempt: '%s' expected '%s'"
					        , sock, user.alias, password, user.pass);
				else
					lprintf(LOG_NOTICE, "%04d <%s> !FAILED Password attempt"
					        , sock, user.alias);
				user.number = 0;
				if (badlogin(sock, sess, &login_attempts, user.alias, password, &client, &ftp.client_addr))
					break;
				continue;
			}

			if (user.rest & FLAG('Q')) { // QWKnet accont
				snprintf(mutex_file.name, sizeof mutex_file.name, "%suser/%04u.ftp", scfg.data_dir, user.number);
				if (!fmutex_open(&mutex_file, startup->host_name, /* max_age: */ 60 * 60)) {
					lprintf(LOG_NOTICE, "%04d <%s> QWKnet account already logged-in to FTP server: %s (since %s)"
					        , sock, user.alias, mutex_file.name, time_as_hhmm(&scfg, mutex_file.time, str));
					sockprintf(sock, sess, "421 QWKnet accounts are limited to one concurrent FTP session");
					user.number = 0;
					break;
				}
			}

			/* Update client display */
			if (user.pass[0]) {
				SAFECOPY(client.user, user.alias);
				loginSuccess(startup->login_attempt_list, &ftp.client_addr);
			} else {    /* anonymous */
				SAFEPRINTF2(client.user, "%s <%.32s>", user.alias, password);
			}
			client.usernum = user.number;
			client_on(sock, &client, TRUE /* update */);

			lprintf(LOG_INFO, "%04d [%s] <%s> logged-in (%u today, %u total)"
			        , sock, host_ip, user.alias, user.ltoday + 1, user.logons + 1);
			logintime = time(NULL);
			timeleft = (long)gettimeleft(&scfg, &user, logintime);
			ftp_printfile(sock, sess, "hello", 230);

			if (sysop)
				sockprintf(sock, sess, "230-Sysop access granted.");
			sockprintf(sock, sess, "230-%s logged in.", user.alias);
			if (!(user.exempt & FLAG('D')) && user_available_credits(&user) > 0)
				sockprintf(sock, sess, "230-You have %" PRIu64 " download credits."
				           , user_available_credits(&user));
			sockprintf(sock, sess, "230 You are allowed %lu minutes of use for this session."
			           , timeleft / 60);
			sprintf(qwkfile, "%sfile/%04d.qwk", scfg.data_dir, user.number);

			/* Adjust User Total Logons/Logons Today */
			user.logons++;
			user.ltoday++;
			if ((result = loginuserdat(&scfg, &user, &client, /* use_protocol: */true, startup->login_info_save)) != 0)
				lprintf(LOG_ERR, "%04d [%s] <%s> !Error %d (errno %d %s) writing user data for user #%d"
				        , sock, host_ip, user.alias, result, errno, safe_strerror(errno, error, sizeof error), user.number);
			mqtt_user_login(&mqtt, &client);

#ifdef _WIN32
			if (startup->sound.login[0] && !sound_muted(&scfg))
				PlaySound(startup->sound.login, NULL, SND_ASYNC | SND_FILENAME);
#endif
			continue;
		}
		if (!strnicmp(cmd, "AUTH ", 5)) {
			if (!stricmp(cmd, "AUTH TLS")) {
				if (sess != -1) {
					sockprintf(sock, sess, "534 Already in TLS mode");
					continue;
				}
				if (startup->options & FTP_OPT_NO_FTPS) {
					lprintf(LOG_NOTICE, "%04d [%s] AUTH TLS rejected because FTPS support is disabled", sock, host_ip);
					sockprintf(sock, sess, "431 TLS not available");
					continue;
				}
				if (start_tls(&sock, &sess, TRUE) || sess == -1) {
					lprintf(LOG_WARNING, "%04d [%s] failed to initialize TLS successfully", sock, host_ip);
					break;
				}
				user.number = 0;
				sysop = FALSE;
				filepos = 0;
				got_pbsz = FALSE;
				protection = FALSE;
				lprintf(LOG_INFO, "%04d [%s] initialized TLS successfully", sock, host_ip);
				SAFECOPY(client.protocol, "FTPS");
				client_on(sock, &client, /* update: */ TRUE);
				continue;
			}
			sockprintf(sock, sess, "504 TLS is the only AUTH supported");
			continue;
		}
		if (!strnicmp(cmd, "PBSZ ", 5)) {
			if (!stricmp(cmd, "PBSZ 0") && sess != -1) {
				got_pbsz = TRUE;
				sockprintf(sock, sess, "200 OK");
				continue;
			}
			if (sess == -1) {
				sockprintf(sock, sess, "503 Need AUTH TLS first");
				continue;
			}
			if (strspn(cmd + 5, "0123456789") == strlen(cmd + 5)) {
				sockprintf(sock, sess, "200 PBSZ=0");
				continue;
			}
			sockprintf(sock, sess, "501 Unable to parse buffer size");
			continue;
		}
		if (!strnicmp(cmd, "PROT ", 5)) {
			if (sess == -1) {
				sockprintf(sock, sess, "503 No AUTH yet");
				continue;
			}
			if (!strnicmp(cmd, "PROT P", 6) && sess != -1 && got_pbsz) {
				protection = TRUE;
				sockprintf(sock, sess, "200 Accepted");
				continue;
			}
			if (!strnicmp(cmd, "PROT C", 6) && sess != -1 && got_pbsz) {
				protection = FALSE;
				sockprintf(sock, sess, "200 Accepted");
				continue;
			}
			sockprintf(sock, sess, "536 Only C and P are supported in TLS mode");
			continue;
		}
		if (!stricmp(cmd, "CCC")) {
			if (sess == -1) {
				sockprintf(sock, sess, "533 Not in TLS mode");
				continue;
			}
			sockprintf(sock, sess, "200 Accepted");
			destroy_session(lprintf, sess);
			sess = -1;
			continue;
		}

		if (!user.number) {
			sockprintf(sock, sess, "530 Please login with USER and PASS.");
			continue;
		}

		if (!user_is_guest(&user))
			getuserdat(&scfg, &user);   /* get current user data */

		if ((timeleft = (long)gettimeleft(&scfg, &user, logintime)) < 1L) {
			sockprintf(sock, sess, "421 Sorry, you've run out of time.");
			lprintf(LOG_WARNING, "%04d <%s> Out of time, disconnecting", sock, user.alias);
			break;
		}

		/********************************/
		/* These commands require login */
		/********************************/

		if (!stricmp(cmd, "REIN")) {
			lprintf(LOG_INFO, "%04d <%s> reinitialized control session", sock, user.alias);
			user.number = 0;
			sysop = FALSE;
			filepos = 0;
			sockprintf(sock, sess, "220 Control session re-initialized. Ready for re-login.");
			if (sess != -1) {
				destroy_session(lprintf, sess);
				sess = -1;
			}
			got_pbsz = FALSE;
			protection = FALSE;
			continue;
		}

		if (!stricmp(cmd, "SITE WHO")) {
			sockprintf(sock, sess, "211-Active Telnet Nodes:");
			for (i = 0; i < scfg.sys_nodes && i < scfg.sys_lastnode; i++) {
				if ((result = getnodedat(&scfg, i + 1, &node, FALSE, NULL)) != 0) {
					sockprintf(sock, sess, " Error %d getting data for Telnet Node %d", result, i + 1);
					continue;
				}
				if (node.status == NODE_INUSE)
					sockprintf(sock, sess, " Node %3d: %s", i + 1, username(&scfg, node.useron, str));
			}
			sockprintf(sock, sess, "211 End (%d active FTP clients)", protected_uint32_value(active_clients));
			continue;
		}
		if (!stricmp(cmd, "SITE VER")) {
			sockprintf(sock, sess, "211 %s", ftp_ver());
			continue;
		}
		if (!stricmp(cmd, "SITE UPTIME")) {
			sockprintf(sock, sess, "211 %s (%lu served)", sectostr((uint)(time(NULL) - uptime), str), served);
			continue;
		}
		if (!stricmp(cmd, "SITE RECYCLE") && user_is_sysop(&user)) {
			startup->recycle_now = TRUE;
			sockprintf(sock, sess, "211 server will recycle when not in-use");
			continue;
		}
		if (!stricmp(cmd, "SITE RECYCLE ALL") && user_is_sysop(&user)) {
			refresh_cfg(&scfg);
			sockprintf(sock, sess, "211 ALL servers/nodes will recycle when not in-use");
			continue;
		}
		if (!strnicmp(cmd, "SITE EXEC ", 10) && sysop) {
			p = cmd + 10;
			SKIP_WHITESPACE(p);
#ifdef __unix__
			fp = popen(p, "r");
			if (fp == NULL)
				sockprintf(sock, sess, "500 Error %d opening pipe to: %s", errno, p);
			else {
				while (!feof(fp)) {
					if (fgets(str, sizeof(str), fp) == NULL)
						break;
					sockprintf(sock, sess, "200-%s", str);
				}
				sockprintf(sock, sess, "200 %s returned %d", p, pclose(fp));
			}
#else
			sockprintf(sock, sess, "200 system(%s) returned %d", p, system(p));
#endif
			continue;
		}


#ifdef SOCKET_DEBUG_CTRL
		if (!stricmp(cmd, "SITE DEBUG")) {
			sockprintf(sock, sess, "211-Debug");
			for (i = 0; i < sizeof(socket_debug); i++)
				if (socket_debug[i] != 0)
					sockprintf(sock, sess, "211-socket %d = 0x%X", i, socket_debug[i]);
			sockprintf(sock, sess, "211 End");
			continue;
		}
#endif

		if (strnicmp(cmd, "PORT ", 5) == 0 || strnicmp(cmd, "EPRT ", 5) == 0 || strnicmp(cmd, "LPRT ", 5) == 0) {

			if (pasv_sock != INVALID_SOCKET)  {
				ftp_close_socket(&pasv_sock, &pasv_sess, __LINE__);
			}
			memcpy(&data_addr, &ftp.client_addr, ftp.client_addr_len);
			p = cmd + 5;
			SKIP_WHITESPACE(p);
			if (strnicmp(cmd, "PORT ", 5) == 0 && sscanf(p, "%u,%u,%u,%u,%hd,%hd", &h1, &h2, &h3, &h4, &p1, &p2) == 6) {
				data_addr.in.sin_family = AF_INET;
				data_addr.in.sin_addr.s_addr = htonl((h1 << 24) | (h2 << 16) | (h3 << 8) | h4);
				data_port = (p1 << 8) | p2;
			} else if (strnicmp(cmd, "EPRT ", 5) == 0) { /* EPRT */
				char delim = *p;
				int  prot;
				char addr_str[INET6_ADDRSTRLEN];

				memset(&data_addr, 0, sizeof(data_addr));
				if (*p)
					p++;
				prot = strtol(p, NULL, /* base: */ 10);
				switch (prot) {
					case 1:
						FIND_CHAR(p, delim);
						if (*p)
							p++;
						ap = p;
						FIND_CHAR(p, delim);
						old_char = *p;
						*p = 0;
						data_addr.in.sin_addr.s_addr = parseIPv4Address(ap);
						*p = old_char;
						if (*p)
							p++;
						data_port = atoi(p);
						data_addr.in.sin_family = AF_INET;
						break;
					case 2:
						FIND_CHAR(p, delim);
						if (*p)
							p++;
						strncpy(addr_str, p, sizeof(addr_str));
						addr_str[sizeof(addr_str) - 1] = 0;
						tp = addr_str;
						FIND_CHAR(tp, delim);
						*tp = 0;
						if (inet_ptoaddr(addr_str, &data_addr, sizeof(data_addr)) == NULL) {
							lprintf(LOG_WARNING, "%04d <%s> !Unable to parse IPv6 address: %s", sock, user.alias, addr_str);
							sockprintf(sock, sess, "522 Unable to parse IPv6 address (1)");
							continue;
						}
						FIND_CHAR(p, delim);
						if (*p)
							p++;
						data_port = atoi(p);
						data_addr.in6.sin6_family = AF_INET6;
						break;
					default:
						lprintf(LOG_WARNING, "%04d <%s> !UNSUPPORTED protocol: %d", sock, user.alias, prot);
						sockprintf(sock, sess, "522 Network protocol not supported, use (1)");
						continue;
				}
			}
			else {  /* LPRT */
				if (sscanf(p, "%u,%u", &h1, &h2) != 2) {
					lprintf(LOG_ERR, "%04d <%s> !Unable to parse LPRT: %s", sock, user.alias, p);
					sockprintf(sock, sess, "521 Address family not supported");
					continue;
				}
				FIND_CHAR(p, ',');
				if (*p)
					p++;
				FIND_CHAR(p, ',');
				if (*p)
					p++;
				switch (h1) {
					case 4: /* IPv4 */
						if (h2 != 4) {
							lprintf(LOG_ERR, "%04d <%s> !Unable to parse LPRT: %s", sock, user.alias, p);
							sockprintf(sock, sess, "501 IPv4 Address is the wrong length");
							continue;
						}
						for (h1 = 0; h1 < h2; h1++) {
							((unsigned char *)(&data_addr.in.sin_addr))[h1] = atoi(p);
							FIND_CHAR(p, ',');
							if (*p)
								p++;
						}
						if (atoi(p) != 2) {
							lprintf(LOG_ERR, "%04d <%s> !Unable to parse LPRT %s", sock, user.alias, p);
							sockprintf(sock, sess, "501 IPv4 Port is the wrong length");
							continue;
						}
						FIND_CHAR(p, ',');
						if (*p)
							p++;
						for (h1 = 0; h1 < 2; h1++) {
							((unsigned char *)(&data_port))[1 - h1] = atoi(p);
							FIND_CHAR(p, ',');
							if (*p)
								p++;
						}
						data_addr.in.sin_family = AF_INET;
						break;
					case 6: /* IPv6 */
						if (h2 != 16) {
							lprintf(LOG_ERR, "%04d <%s> !Unable to parse LPRT: %s", sock, user.alias, p);
							sockprintf(sock, sess, "501 IPv6 Address is the wrong length");
							continue;
						}
						for (h1 = 0; h1 < h2; h1++) {
							((unsigned char *)(&data_addr.in6.sin6_addr))[h1] = atoi(p);
							FIND_CHAR(p, ',');
							if (*p)
								p++;
						}
						if (atoi(p) != 2) {
							lprintf(LOG_ERR, "%04d <%s> !Unable to parse LPRT: %s", sock, user.alias, p);
							sockprintf(sock, sess, "501 IPv6 Port is the wrong length");
							continue;
						}
						FIND_CHAR(p, ',');
						if (*p)
							p++;
						for (h1 = 0; h1 < 2; h1++) {
							((unsigned char *)(&data_port))[1 - h1] = atoi(p);
							FIND_CHAR(p, ',');
							if (*p)
								p++;
						}
						data_addr.in6.sin6_family = AF_INET6;
						break;
					default:
						lprintf(LOG_ERR, "%04d <%s> !Unable to parse LPRT: %s", sock, user.alias, p);
						sockprintf(sock, sess, "521 Address family not supported");
						continue;
				}
			}

			inet_addrtop(&data_addr, data_ip, sizeof(data_ip));
			bool bounce_allowed = (startup->options & FTP_OPT_ALLOW_BOUNCE) && !user_is_guest(&user);
			if (data_port < IPPORT_RESERVED
			    || (strcmp(data_ip, host_ip) != 0 && !bounce_allowed)) {
				lprintf(LOG_WARNING, "%04d <%s> !SUSPECTED BOUNCE ATTACK ATTEMPT to %s port %u"
				        , sock, user.alias
				        , data_ip, data_port);
				ftp_hacklog("FTP BOUNCE", user.alias, cmd, host_name, &ftp.client_addr);
				sockprintf(sock, sess, "504 Bad port number.");
				continue; /* As recommended by RFC2577 */
			}
			inet_setaddrport(&data_addr, data_port);
			sockprintf(sock, sess, "200 PORT Command successful.");
			mode = "active";
			continue;
		}

		if (stricmp(cmd, "PASV") == 0 || stricmp(cmd, "P@SW") == 0   /* Kludge required for SMC Barricade V1.2 */
		    || stricmp(cmd, "EPSV") == 0 || strnicmp(cmd, "EPSV ", 5) == 0 || stricmp(cmd, "LPSV") == 0) {

			if (pasv_sock != INVALID_SOCKET)
				ftp_close_socket(&pasv_sock, &pasv_sess, __LINE__);

			if ((pasv_sock = ftp_open_socket(pasv_addr.addr.sa_family, SOCK_STREAM)) == INVALID_SOCKET) {
				lprintf(LOG_WARNING, "%04d <%s> !PASV ERROR %d opening socket", sock, user.alias, SOCKET_ERRNO);
				sockprintf(sock, sess, "425 Error %d opening PASV data socket", SOCKET_ERRNO);
				continue;
			}

			reuseaddr = FALSE;
			if ((result = setsockopt(pasv_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseaddr, sizeof(reuseaddr))) != 0) {
				lprintf(LOG_WARNING, "%04d <%s> !PASV ERROR %d disabling REUSEADDR socket option"
				        , sock, user.alias, SOCKET_ERRNO);
				sockprintf(sock, sess, "425 Error %d disabling REUSEADDR socket option", SOCKET_ERRNO);
				continue;
			}

			if (startup->options & FTP_OPT_DEBUG_DATA)
				lprintf(LOG_DEBUG, "%04d <%s> PASV DATA socket %d opened", sock, user.alias, pasv_sock);

			for (port = startup->pasv_port_low; port <= startup->pasv_port_high; port++) {

				if (startup->options & FTP_OPT_DEBUG_DATA)
					lprintf(LOG_DEBUG, "%04d <%s> PASV DATA trying to bind socket to port %u"
					        , sock, user.alias, port);

				inet_setaddrport(&pasv_addr, port);

				if ((result = bind(pasv_sock, &pasv_addr.addr, xp_sockaddr_len(&pasv_addr))) == 0)
					break;
				if (port == startup->pasv_port_high)
					break;
			}
			if (result != 0) {
				lprintf(LOG_ERR, "%04d <%s> !PASV ERROR %d (%d) binding socket to port %u"
				        , sock, user.alias, result, SOCKET_ERRNO, port);
				sockprintf(sock, sess, "425 Error %d binding data socket", SOCKET_ERRNO);
				ftp_close_socket(&pasv_sock, &pasv_sess, __LINE__);
				continue;
			}
			if (startup->options & FTP_OPT_DEBUG_DATA)
				lprintf(LOG_DEBUG, "%04d <%s> PASV DATA socket %d bound to port %u", sock, user.alias, pasv_sock, port);

			addr_len = sizeof(addr);
			if ((result = getsockname(pasv_sock, &addr.addr, &addr_len)) != 0) {
				lprintf(LOG_CRIT, "%04d <%s> !PASV ERROR %d (%d) getting address/port of socket"
				        , sock, user.alias, result, SOCKET_ERRNO);
				sockprintf(sock, sess, "425 Error %d getting address/port", SOCKET_ERRNO);
				ftp_close_socket(&pasv_sock, &pasv_sess, __LINE__);
				continue;
			}

			if ((result = listen(pasv_sock, 1)) != 0) {
				lprintf(LOG_ERR, "%04d <%s> !PASV ERROR %d (%d) listening on port %u"
				        , sock, user.alias, result, SOCKET_ERRNO, port);
				sockprintf(sock, sess, "425 Error %d listening on data socket", SOCKET_ERRNO);
				ftp_close_socket(&pasv_sock, &pasv_sess, __LINE__);
				continue;
			}

			port = inet_addrport(&addr);
			if (strnicmp(cmd, "EPSV", 4) == 0)
				sockprintf(sock, sess, "229 Entering Extended Passive Mode (|||%hu|)", port);
			else if (stricmp(cmd, "LPSV") == 0) {
				switch (addr.addr.sa_family) {
					case AF_INET:
						sockprintf(sock, sess, "228 Entering Long Passive Mode (4, 4, %d, %d, %d, %d, 2, %d, %d)"
						           , ((unsigned char *)&(addr.in.sin_addr))[0]
						           , ((unsigned char *)&(addr.in.sin_addr))[1]
						           , ((unsigned char *)&(addr.in.sin_addr))[2]
						           , ((unsigned char *)&(addr.in.sin_addr))[3]
						           , ((unsigned char *)&(addr.in.sin_port))[0]
						           , ((unsigned char *)&(addr.in.sin_port))[1]);
						break;
					case AF_INET6:
						sockprintf(sock, sess, "228 Entering Long Passive Mode (6, 16, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, 2, %d, %d)"
						           , ((unsigned char *)&(addr.in6.sin6_addr))[0]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[1]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[2]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[3]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[4]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[5]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[6]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[7]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[8]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[9]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[10]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[11]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[12]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[13]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[14]
						           , ((unsigned char *)&(addr.in6.sin6_addr))[15]
						           , ((unsigned char *)&(addr.in6.sin6_port))[0]
						           , ((unsigned char *)&(addr.in6.sin6_port))[1]);
						break;
				}
			}
			else {
				/* Choose IP address to use in passive response */
				ip_addr = 0;
				/* TODO: IPv6 this here lookup */
				if (startup->options & FTP_OPT_LOOKUP_PASV_IP
				    && (host = gethostbyname(server_host_name())) != NULL
				    && host->h_addr_list[0] != NULL)
					ip_addr = ntohl(*((in_addr_t*)host->h_addr_list[0]));
				if (ip_addr == 0 && (ip_addr = startup->pasv_ip_addr.s_addr) == 0)
					ip_addr = ntohl(pasv_addr.in.sin_addr.s_addr);

				if (startup->options & FTP_OPT_DEBUG_DATA)
					lprintf(LOG_INFO, "%04d <%s> PASV DATA IP address in response: %u.%u.%u.%u (subject to NAT)"
					        , sock
					        , user.alias
					        , (ip_addr >> 24) & 0xff
					        , (ip_addr >> 16) & 0xff
					        , (ip_addr >> 8) & 0xff
					        , ip_addr & 0xff
					        );
				sockprintf(sock, sess, "227 Entering Passive Mode (%u,%u,%u,%u,%hu,%hu)"
				           , (ip_addr >> 24) & 0xff
				           , (ip_addr >> 16) & 0xff
				           , (ip_addr >> 8) & 0xff
				           , ip_addr & 0xff
				           , (ushort)((port >> 8) & 0xff)
				           , (ushort)(port & 0xff)
				           );
			}
			mode = "passive";
			continue;
		}

		if (!strnicmp(cmd, "TYPE ", 5)) {
			sockprintf(sock, sess, "200 All files sent in BINARY mode.");
			continue;
		}

		if (!strnicmp(cmd, "ALLO", 4)) {
			p = cmd + 5;
			SKIP_WHITESPACE(p);
			if (*p)
				l = atol(p);
			else
				l = 0;
			if (local_fsys)
				avail = getfreediskspace(local_dir, 0);
			else
				avail = getfreediskspace(scfg.data_dir, 0); /* Change to temp_dir? */
			if (l && l > avail)
				sockprintf(sock, sess, "504 Only %" PRIu64 " bytes available.", avail);
			else
				sockprintf(sock, sess, "200 %" PRIu64 " bytes available.", avail);
			continue;
		}

		if (!strnicmp(cmd, "REST", 4)) {
			p = cmd + 4;
			SKIP_WHITESPACE(p);
			if (*p)
				filepos = atol(p);
			else
				filepos = 0;
			sockprintf(sock, sess, "350 Restarting at %ld. Send STORE or RETRIEVE to initiate transfer."
			           , filepos);
			continue;
		}

		if (!strnicmp(cmd, "MODE ", 5)) {
			p = cmd + 5;
			SKIP_WHITESPACE(p);
			if (toupper(*p) != 'S')
				sockprintf(sock, sess, "504 Only STREAM mode supported.");
			else
				sockprintf(sock, sess, "200 STREAM mode.");
			continue;
		}

		if (!strnicmp(cmd, "STRU ", 5)) {
			p = cmd + 5;
			SKIP_WHITESPACE(p);
			if (toupper(*p) != 'F')
				sockprintf(sock, sess, "504 Only FILE structure supported.");
			else
				sockprintf(sock, sess, "200 FILE structure.");
			continue;
		}

		if (!stricmp(cmd, "SYST")) {
			sockprintf(sock, sess, "215 UNIX Type: L8");
			continue;
		}

		if (!stricmp(cmd, "ABOR")) {
			if (!transfer_inprogress)
				sockprintf(sock, sess, "226 No transfer in progress.");
			else {
				lprintf(LOG_WARNING, "%04d <%s> aborting transfer"
				        , sock, user.alias);
				transfer_aborted = TRUE;
				YIELD(); /* give send thread time to abort */
				sockprintf(sock, sess, "226 Transfer aborted.");
			}
			continue;
		}

		if (!strnicmp(cmd, "SMNT ", 5) && sysop && !(startup->options & FTP_OPT_NO_LOCAL_FSYS)) {
			p = cmd + 5;
			SKIP_WHITESPACE(p);
			if (!stricmp(p, BBS_FSYS_DIR))
				local_fsys = FALSE;
			else {
				if (!direxist(p)) {
					sockprintf(sock, sess, "550 Directory does not exist.");
					lprintf(LOG_WARNING, "%04d <%s> !attempted to mount invalid directory: '%s'"
					        , sock, user.alias, p);
					continue;
				}
				local_fsys = TRUE;
				SAFECOPY(local_dir, p);
			}
			sockprintf(sock, sess, "250 %s file system mounted."
			           , local_fsys ? "Local" : "BBS");
			lprintf(LOG_INFO, "%04d <%s> mounted %s file system"
			        , sock, user.alias, local_fsys ? "local" : "BBS");
			continue;
		}

		/****************************/
		/* Local File System Access */
		/****************************/
		if (sysop && local_fsys && !(startup->options & FTP_OPT_NO_LOCAL_FSYS)) {
			if (local_dir[0]
			    && local_dir[strlen(local_dir) - 1] != '\\'
			    && local_dir[strlen(local_dir) - 1] != '/')
				strcat(local_dir, "/");

			if (!strnicmp(cmd, "MLS", 3)) {
				if (cmd[3] == 'T' || cmd[3] == 'D') {
					if (cmd[3] == 'D') {
						if ((fp = fopen(ftp_tmpfname(fname, "lst", sock), "w+b")) == NULL) {
							lprintf(LOG_ERR, "%04d <%s> !ERROR %d (%s) line %d opening %s"
							        , sock, user.alias, errno, safe_strerror(errno, error, sizeof error), __LINE__, fname);
							sockprintf(sock, sess, "451 Insufficient system storage");
							continue;
						}
					}

					p = cmd + 4;
					SKIP_WHITESPACE(p);

					filespec = p;
					if (!local_dir[0])
						strcpy(local_dir, "/");
					SAFEPRINTF2(path, "%s%s", local_dir, filespec);
					p = FULLPATH(NULL, path, 0);
					strcpy(path, p);
					free(p);
					if (cmd[3] == 'T') {
						if (access(path, 0) == -1) {
							sockprintf(sock, sess, "550 No such path %s", path);
							continue;
						}
						sockprintf(sock, sess, "250- Listing %s", path);
					}
					else {
						if (access(path, 0) == -1) {
							sockprintf(sock, sess, "550 No such path %s", path);
							continue;
						}
						if (!isdir(path)) {
							sockprintf(sock, sess, "501 Not a directory");
							continue;
						}
						sockprintf(sock, sess, "150 Directory of %s", path);
						backslash(path);
						strcat(path, "*");
					}

					lprintf(LOG_INFO, "%04d <%s> MLSx listing: local %s in %s mode", sock, user.alias, path, mode);

					now = time(NULL);
					if (localtime_r(&now, &cur_tm) == NULL)
						memset(&cur_tm, 0, sizeof(cur_tm));

					if (cmd[3] == 'T') {
						write_local_mlsx(NULL, sock, sess, mlsx_feats, path, TRUE);
						sockprintf(sock, sess, "250 End");
					}
					else {
						time_t start = time(NULL);
						glob_t g;
						glob(path, GLOB_MARK, NULL, &g);
						for (i = 0; i < (int)g.gl_pathc; i++) {
							char fpath[MAX_PATH + 1];
							SAFECOPY(fpath, g.gl_pathv[i]);
							if (*lastchar(fpath) == '/')
								*lastchar(fpath) = 0;
							write_local_mlsx(fp, INVALID_SOCKET, -1, mlsx_feats, fpath, FALSE);
						}
						lprintf(LOG_INFO, "%04d <%s> %s-listing (%ld bytes) of local %s (%lu files) created in %" PRId64 " seconds"
						        , sock, user.alias, cmd, ftell(fp), path
						        , (ulong)g.gl_pathc, (int64_t)(time(NULL) - start));
						globfree(&g);
						fclose(fp);
						filexfer(&data_addr, sock, sess, pasv_sock, pasv_sess, &data_sock, &data_sess, fname, 0L
						         , &transfer_inprogress, &transfer_aborted
						         , TRUE /* delfile */
						         , TRUE /* tmpfile */
						         , &lastactive, &user, &client, -1, FALSE, FALSE, FALSE, NULL, protection);
					}
					continue;
				}
			}

			if (!strnicmp(cmd, "LIST", 4) || !strnicmp(cmd, "NLST", 4)) {
				if (!strnicmp(cmd, "LIST", 4))
					detail = TRUE;
				else
					detail = FALSE;

				if ((fp = fopen(ftp_tmpfname(fname, "lst", sock), "w+b")) == NULL) {
					lprintf(LOG_ERR, "%04d <%s> !ERROR %d (%s) line %d opening %s"
					        , sock, user.alias, errno, safe_strerror(errno, error, sizeof error), __LINE__, fname);
					sockprintf(sock, sess, "451 Insufficient system storage");
					continue;
				}

				p = cmd + 4;
				SKIP_WHITESPACE(p);

				if (*p == '-') {   /* -Letc */
					FIND_WHITESPACE(p);
					SKIP_WHITESPACE(p);
				}

				filespec = p;
				if (*filespec == 0)
					filespec = "*";

				SAFEPRINTF2(path, "%s%s", local_dir, filespec);
				lprintf(LOG_INFO, "%04d <%s> %slisting: local %s in %s mode"
				        , sock, user.alias, detail ? "detailed ":"", path, mode);
				sockprintf(sock, sess, "150 Directory of %s%s", local_dir, filespec);

				now = time(NULL);
				if (localtime_r(&now, &cur_tm) == NULL)
					memset(&cur_tm, 0, sizeof(cur_tm));

				time_t start = time(NULL);
				glob_t g;
				glob(path, GLOB_MARK, NULL, &g);
				for (i = 0; i < (int)g.gl_pathc; i++) {
					char fpath[MAX_PATH + 1];
					SAFECOPY(fpath, g.gl_pathv[i]);
					if (*lastchar(fpath) == '/')
						*lastchar(fpath) = 0;
					if (detail) {
						struct stat st;
						if (stat(fpath, &st) != 0)
							continue;
						if (localtime_r(&st.st_mtime, &tm) == NULL)
							memset(&tm, 0, sizeof(tm));
						fprintf(fp, "%crw-r--r--   1 %-8s local %9" PRId64 " %s %2d "
						        , *lastchar(g.gl_pathv[i]) == '/' ? 'd':'-'
						        , scfg.sys_id
						        , (int64_t)st.st_size
						        , ftp_mon[tm.tm_mon], tm.tm_mday);
						if (tm.tm_year == cur_tm.tm_year)
							fprintf(fp, "%02d:%02d %s\r\n"
							        , tm.tm_hour, tm.tm_min
							        , getfname(fpath));
						else
							fprintf(fp, "%5d %s\r\n"
							        , 1900 + tm.tm_year
							        , getfname(fpath));
					} else
						fprintf(fp, "%s\r\n", getfname(fpath));
				}
				lprintf(LOG_INFO, "%04d <%s> %slisting (%ld bytes) of local %s (%lu files) created in %" PRId64 " seconds"
				        , sock, user.alias, detail ? "detailed ":"", ftell(fp), path
				        , (ulong)g.gl_pathc, (int64_t)(time(NULL) - start));
				globfree(&g);
				fclose(fp);
				filexfer(&data_addr, sock, sess, pasv_sock, pasv_sess, &data_sock, &data_sess, fname, 0L
				         , &transfer_inprogress, &transfer_aborted
				         , TRUE /* delfile */
				         , TRUE /* tmpfile */
				         , &lastactive, &user, &client, -1, FALSE, FALSE, FALSE, NULL, protection);
				continue;
			} /* Local LIST/NLST */

			if (!strnicmp(cmd, "CWD ", 4) || !strnicmp(cmd, "XCWD ", 5)) {
				if (!strnicmp(cmd, "CWD ", 4))
					p = cmd + 4;
				else
					p = cmd + 5;
				SKIP_WHITESPACE(p);
				tp = p;
				if (*tp == '/' || *tp == '\\') /* /local: and /bbs: are valid */
					tp++;
				if (!strnicmp(tp, BBS_FSYS_DIR, strlen(BBS_FSYS_DIR))) {
					local_fsys = FALSE;
					sockprintf(sock, sess, "250 CWD command successful (BBS file system mounted).");
					lprintf(LOG_INFO, "%04d <%s> mounted BBS file system", sock, user.alias);
					continue;
				}
				if (!strnicmp(tp, LOCAL_FSYS_DIR, strlen(LOCAL_FSYS_DIR))) {
					tp += strlen(LOCAL_FSYS_DIR); /* already mounted */
					p = tp;
				}

				if (p[1] == ':' || !strncmp(p, "\\\\", 2))
					SAFECOPY(path, p);
				else if (*p == '/' || *p == '\\') {
					SAFEPRINTF2(path, "%s%s", root_dir(local_dir), p + 1);
					p = FULLPATH(NULL, path, 0);
					SAFECOPY(path, p);
					free(p);
				}
				else {
					SAFEPRINTF2(fname, "%s%s", local_dir, p);
					FULLPATH(path, fname, sizeof(path));
				}

				if (!direxist(path)) {
					sockprintf(sock, sess, "550 Directory does not exist (%s).", path);
					lprintf(LOG_WARNING, "%04d <%s> !attempted to change to an invalid directory: '%s'"
					        , sock, user.alias, path);
				} else {
					SAFECOPY(local_dir, path);
					sockprintf(sock, sess, "250 CWD command successful (%s).", local_dir);
				}
				continue;
			} /* Local CWD */

			if (!stricmp(cmd, "CDUP") || !stricmp(cmd, "XCUP")) {
				SAFEPRINTF(path, "%s..", local_dir);
				if (FULLPATH(local_dir, path, sizeof(local_dir)) == NULL)
					sockprintf(sock, sess, "550 Directory does not exist.");
				else
					sockprintf(sock, sess, "200 CDUP command successful.");
				continue;
			}

			if (!stricmp(cmd, "PWD") || !stricmp(cmd, "XPWD")) {
				if (strlen(local_dir) > 3)
					local_dir[strlen(local_dir) - 1] = 0; /* truncate '/' */

				sockprintf(sock, sess, "257 \"%s\" is current directory."
				           , local_dir);
				continue;
			} /* Local PWD */

			if (!strnicmp(cmd, "MKD ", 4) || !strnicmp(cmd, "XMKD", 4)) {
				p = cmd + 4;
				SKIP_WHITESPACE(p);
				if (*p == '/') /* absolute */
					SAFEPRINTF2(fname, "%s%s", root_dir(local_dir), p + 1);
				else        /* relative */
					SAFEPRINTF2(fname, "%s%s", local_dir, p);

				if (MKDIR(fname) == 0) {
					sockprintf(sock, sess, "257 \"%s\" directory created", fname);
					lprintf(LOG_NOTICE, "%04d <%s> created directory: %s", sock, user.alias, fname);
				} else {
					sockprintf(sock, sess, "521 Error %d creating directory: %s", errno, fname);
					lprintf(LOG_WARNING, "%04d <%s> !ERROR %d (%s) attempting to create directory: %s"
					        , sock, user.alias, errno, safe_strerror(errno, error, sizeof error), fname);
				}
				continue;
			}

			if (!strnicmp(cmd, "RMD ", 4) || !strnicmp(cmd, "XRMD", 4)) {
				p = cmd + 4;
				SKIP_WHITESPACE(p);
				if (*p == '/') /* absolute */
					SAFEPRINTF2(fname, "%s%s", root_dir(local_dir), p + 1);
				else        /* relative */
					SAFEPRINTF2(fname, "%s%s", local_dir, p);

				if (rmdir(fname) == 0) {
					sockprintf(sock, sess, "250 \"%s\" directory removed", fname);
					lprintf(LOG_NOTICE, "%04d <%s> removed directory: %s", sock, user.alias, fname);
				} else {
					sockprintf(sock, sess, "450 Error %d removing directory: %s", errno, fname);
					lprintf(LOG_WARNING, "%04d <%s> !ERROR %d (%s) removing directory: %s"
					        , sock, user.alias, errno, safe_strerror(errno, error, sizeof error), fname);
				}
				continue;
			}

			if (!strnicmp(cmd, "RNFR ", 5)) {
				p = cmd + 5;
				SKIP_WHITESPACE(p);
				if (*p == '/') /* absolute */
					SAFEPRINTF2(ren_from, "%s%s", root_dir(local_dir), p + 1);
				else        /* relative */
					SAFEPRINTF2(ren_from, "%s%s", local_dir, p);
				if (!fexist(ren_from)) {
					sockprintf(sock, sess, "550 File not found: %s", ren_from);
					lprintf(LOG_WARNING, "%04d <%s> !ERROR renaming %s (not found)"
					        , sock, user.alias, ren_from);
				} else
					sockprintf(sock, sess, "350 File exists, ready for destination name");
				continue;
			}

			if (!strnicmp(cmd, "RNTO ", 5)) {
				p = cmd + 5;
				SKIP_WHITESPACE(p);
				if (*p == '/') /* absolute */
					SAFEPRINTF2(fname, "%s%s", root_dir(local_dir), p + 1);
				else        /* relative */
					SAFEPRINTF2(fname, "%s%s", local_dir, p);

				if (rename(ren_from, fname) == 0) {
					sockprintf(sock, sess, "250 \"%s\" renamed to \"%s\"", ren_from, fname);
					lprintf(LOG_NOTICE, "%04d <%s> renamed %s to %s", sock, user.alias, ren_from, fname);
				} else {
					sockprintf(sock, sess, "450 Error %d renaming file: %s", errno, ren_from);
					lprintf(LOG_WARNING, "%04d <%s> !ERRROR %d (%s) renaming file: %s"
					        , sock, user.alias, errno, safe_strerror(errno, error, sizeof error), ren_from);
				}
				continue;
			}


			if (!strnicmp(cmd, "RETR ", 5) || !strnicmp(cmd, "SIZE ", 5)
			    || !strnicmp(cmd, "MDTM ", 5) || !strnicmp(cmd, "DELE ", 5)) {
				p = cmd + 5;
				SKIP_WHITESPACE(p);

				if (!strnicmp(p, LOCAL_FSYS_DIR, strlen(LOCAL_FSYS_DIR)))
					p += strlen(LOCAL_FSYS_DIR); /* already mounted */

				if (p[1] == ':')       /* drive specified */
					SAFECOPY(fname, p);
				else if (*p == '/')    /* absolute, current drive */
					SAFEPRINTF2(fname, "%s%s", root_dir(local_dir), p + 1);
				else        /* relative */
					SAFEPRINTF2(fname, "%s%s", local_dir, p);
				if (!fexist(fname)) {
					lprintf(LOG_WARNING, "%04d <%s> !File not found: %s", sock, user.alias, fname);
					sockprintf(sock, sess, "550 File not found: %s", fname);
					continue;
				}
				if (!strnicmp(cmd, "SIZE ", 5)) {
					sockprintf(sock, sess, "213 %" PRIdOFF, flength(fname));
					continue;
				}
				if (!strnicmp(cmd, "MDTM ", 5)) {
					t = fdate(fname);
					if (gmtime_r(&t, &tm) == NULL) /* specifically use GMT/UTC representation */
						memset(&tm, 0, sizeof(tm));
					sockprintf(sock, sess, "213 %u%02u%02u%02u%02u%02u"
					           , 1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday
					           , tm.tm_hour, tm.tm_min, tm.tm_sec);
					continue;
				}
				if (!strnicmp(cmd, "DELE ", 5)) {
					if (ftp_remove(sock, __LINE__, fname, user.alias, LOG_ERR) == true) {
						sockprintf(sock, sess, "250 \"%s\" removed successfully.", fname);
						lprintf(LOG_NOTICE, "%04d <%s> deleted file: %s", sock, user.alias, fname);
					} else {
						sockprintf(sock, sess, "450 Error %d removing file: %s", errno, fname);
						lprintf(LOG_WARNING, "%04d <%s> !ERROR %d (%s) deleting file: %s"
						        , sock, user.alias, errno, safe_strerror(errno, error, sizeof error), fname);
					}
					continue;
				}
				/* RETR */
				lprintf(LOG_INFO, "%04d <%s> downloading: %s (%s bytes) in %s mode"
				        , sock, user.alias, fname, byte_estimate_to_str(flength(fname), tmp, sizeof tmp, 1, 1)
				        , mode);
				sockprintf(sock, sess, "150 Opening BINARY mode data connection for file transfer.");
				filexfer(&data_addr, sock, sess, pasv_sock, pasv_sess, &data_sock, &data_sess, fname, filepos
				         , &transfer_inprogress, &transfer_aborted, FALSE, FALSE
				         , &lastactive, &user, &client, -1, FALSE, FALSE, FALSE, NULL, protection);
				continue;
			} /* Local RETR/SIZE/MDTM */

			if (!strnicmp(cmd, "STOR ", 5) || !strnicmp(cmd, "APPE ", 5)) {
				p = cmd + 5;
				SKIP_WHITESPACE(p);

				if (!strnicmp(p, LOCAL_FSYS_DIR, strlen(LOCAL_FSYS_DIR)))
					p += strlen(LOCAL_FSYS_DIR); /* already mounted */

				if (p[1] == ':')       /* drive specified */
					SAFECOPY(fname, p);
				else if (*p == '/')    /* absolute, current drive */
					SAFEPRINTF2(fname, "%s%s", root_dir(local_dir), p + 1);
				else                /* relative */
					SAFEPRINTF2(fname, "%s%s", local_dir, p);

				lprintf(LOG_INFO, "%04d <%s> uploading: %s in %s mode", sock, user.alias, fname
				        , mode);
				sockprintf(sock, sess, "150 Opening BINARY mode data connection for file transfer.");
				filexfer(&data_addr, sock, sess, pasv_sock, pasv_sess, &data_sock, &data_sess, fname, filepos
				         , &transfer_inprogress, &transfer_aborted, FALSE, FALSE
				         , &lastactive
				         , &user
				         , &client
				         , -1 /* dir */
				         , TRUE /* uploading */
				         , FALSE /* credits */
				         , !strnicmp(cmd, "APPE", 4) ? TRUE : FALSE /* append */
				         , NULL /* desc */
				         , protection
				         );
				filepos = 0;
				continue;
			} /* Local STOR */
		}

		if (!strnicmp(cmd, "MLS", 3)) {
			if (cmd[3] == 'D' || cmd[3] == 'T') {
				dir = curdir;
				lib = curlib;
				l = 0;

				if (cmd[4] != 0)
					lprintf(LOG_DEBUG, "%04d <%s> MLSx: %s", sock, user.alias, cmd);

				/* path specified? */
				p = cmd + 4;
				if (*p == ' ')
					p++;

				if (parsepath(&p, &user, &client, &lib, &dir) == -1) {
					sockprintf(sock, sess, "550 No such path");
					continue;
				}

				if (strchr(p, '/')) {
					sockprintf(sock, sess, "550 No such path");
					continue;
				}
				if (cmd[3] == 'T') {
					if (cmd[4])
						strcpy(mls_path, cmd + 5);
					else
						strcpy(mls_path, p);
				}
				else {
					if (*p) {
						sockprintf(sock, sess, "501 Not a directory");
						continue;
					}
					strcpy(mls_path, p);
				}
				mls_fname = p;

				fp = NULL;
				if (cmd[3] == 'D') {
					if ((fp = fopen(ftp_tmpfname(fname, "lst", sock), "w+b")) == NULL) {
						lprintf(LOG_ERR, "%04d <%s> !ERROR %d (%s) line %d opening %s"
						        , sock, user.alias, errno, safe_strerror(errno, error, sizeof error), __LINE__, fname);
						sockprintf(sock, sess, "451 Insufficient system storage");
						continue;
					}
					sockprintf(sock, sess, "150 Opening ASCII mode data connection for MLSD.");
				}
				now = time(NULL);
				if (localtime_r(&now, &cur_tm) == NULL)
					memset(&cur_tm, 0, sizeof(cur_tm));

				/* ASCII Index File */
				if (startup->options & FTP_OPT_INDEX_FILE && startup->index_file_name[0]
				    && (cmd[3] == 'D' || strcmp(startup->index_file_name, mls_fname) == 0)) {
					if (cmd[3] == 'T')
						sockprintf(sock, sess, "250- Listing %s", startup->index_file_name);
					get_owner_name(NULL, str, sizeof str);
					send_mlsx_entry(fp, sock, sess, mlsx_feats, "file", "r", UINT64_MAX, 0, str, NULL, 0, cmd[3] == 'T' ? mls_path : startup->index_file_name);
					l++;
				}
				if (lib < 0) { /* Root dir */
					if (cmd[3] == 'T' && !*mls_fname) {
						sockprintf(sock, sess, "250- Listing root");
						get_owner_name(NULL, str, sizeof str);
						strcpy(aliaspath, "/");
						send_mlsx_entry(fp, sock, sess, mlsx_feats, "dir", (startup->options & FTP_OPT_ALLOW_QWK) ? "elc" : "el", UINT64_MAX, 0, str, NULL, 0, aliaspath);
						l++;
						/* RFC 3659: "if no object is named ... MLST to send a one-line response, describing the current directory itself" */
					}
					else {
						send_mlsx_entry(fp, sock, sess, mlsx_feats, "cdir", (startup->options & FTP_OPT_ALLOW_QWK) ? "elc" : "el", UINT64_MAX, 0, str, NULL, 0, "/");
						lprintf(LOG_INFO, "%04d <%s> %s listing: root in %s mode", sock, user.alias, cmd, mode);

						/* QWK Packet */
						if (startup->options & FTP_OPT_ALLOW_QWK) {
							SAFEPRINTF(str, "%s.qwk", scfg.sys_id);
							if (cmd[3] == 'D' || strcmp(str, mls_fname) == 0) {
								if (cmd[3] == 'T')
									sockprintf(sock, sess, "250- Listing %s", str);
								get_owner_name(NULL, owner, sizeof owner);
								send_mlsx_entry(fp, sock, sess, mlsx_feats, "file", "r", flength(qwkfile), fdate(qwkfile), owner, NULL, 0, cmd[3] == 'T' ? mls_path : str);
								l++;
							}
						}

						/* File Aliases */
						for (int i = 0; i < scfg.total_dirs; ++i) {
							if (scfg.dir[i]->vshortcut[0] == '\0')
								continue;
							if (!user_can_access_dir(&scfg, i, &user, &client))
								continue;
							SAFEPRINTF2(aliaspath, "/%s/%s", scfg.lib[scfg.dir[i]->lib]->vdir, scfg.dir[i]->vdir);
							get_unique(aliaspath, uniq);
							if (cmd[3] == 'D')
								send_mlsx_entry(fp, sock, sess, mlsx_feats, "dir", "el", UINT64_MAX, /* modify_date: */ 0, /* owner: */ scfg.lib[scfg.dir[i]->lib]->vdir, uniq, 0, scfg.dir[i]->vshortcut);
							else {
								if (strcmp(mls_fname, scfg.dir[i]->vshortcut) != 0)
									continue;
								send_mlsx_entry(fp, sock, sess, mlsx_feats, "dir", "el", UINT64_MAX, /* modify_date: */ 0, /* owner: */ scfg.lib[scfg.dir[i]->lib]->vdir, uniq, 0, aliaspath[0] ? aliaspath : mls_path);
							}
						}

						sprintf(aliasfile, "%sftpalias.cfg", scfg.ctrl_dir);
						if ((alias_fp = fopen(aliasfile, "r")) != NULL) {

							while (!feof(alias_fp)) {
								if (!fgets(aliasline, sizeof(aliasline), alias_fp))
									break;

								alias_dir = FALSE;

								p = aliasline;        /* alias pointer */
								SKIP_WHITESPACE(p);

								if (*p == ';') /* comment */
									continue;

								tp = p;       /* terminator pointer */
								FIND_WHITESPACE(tp);
								if (*tp)
									*tp = 0;

								np = tp + 1;    /* filename pointer */
								SKIP_WHITESPACE(np);

								tp = np;      /* terminator pointer */
								FIND_WHITESPACE(tp);
								if (*tp)
									*tp = 0;

								dp = tp + 1;    /* description pointer */
								SKIP_WHITESPACE(dp);
								truncsp(dp);

								if (stricmp(dp, BBS_HIDDEN_ALIAS) == 0)
									continue;

								/* Virtual Path? */
								aliaspath[0] = 0;
								if (!strnicmp(np, BBS_VIRTUAL_PATH, strlen(BBS_VIRTUAL_PATH))) {
									if ((dir = getdir_from_vpath(&scfg, np + strlen(BBS_VIRTUAL_PATH), &user, &client, true)) < 0) {
										lprintf(LOG_WARNING, "%04d <%s> !Invalid virtual path:%s", sock, user.alias, np);
										continue; /* No access or invalid virtual path */
									}
									tp = strrchr(np, '/');
									if (tp == NULL)
										continue;
									tp++;
									if (*tp) {
										SAFEPRINTF2(aliasfile, "%s%s", scfg.dir[dir]->path, tp);
										np = aliasfile;
										SAFEPRINTF3(aliaspath, "/%s/%s/%s", scfg.lib[scfg.dir[dir]->lib]->vdir, scfg.dir[dir]->vdir, tp);
									}
									else {
										alias_dir = TRUE;
										SAFEPRINTF2(aliaspath, "/%s/%s", scfg.lib[scfg.dir[dir]->lib]->vdir, scfg.dir[dir]->vdir);
									}
								}

								if (!alias_dir && !fexist(np)) {
									lprintf(LOG_WARNING, "%04d <%s> !Missing aliased file: %s", sock, user.alias, np);
									continue;
								}

								get_unique(aliaspath, uniq);
								if (cmd[3] == 'D') {
									if (alias_dir == TRUE)
										send_mlsx_entry(fp, sock, sess, mlsx_feats, "dir", "el", UINT64_MAX, /* modify_date: */ 0, /* owner: */ scfg.lib[scfg.dir[dir]->lib]->vdir, uniq, 0, p);
									else
										send_mlsx_entry(fp, sock, sess, mlsx_feats, "file", "r", (uint64_t)flength(np), fdate(np), get_owner_name(NULL, owner, sizeof owner), uniq, 0, p);
								}
								else {
									if (strcmp(mls_fname, p) != 0)
										continue;
									if (alias_dir == TRUE)
										send_mlsx_entry(fp, sock, sess, mlsx_feats, "dir", "el", UINT64_MAX, /* modify_date: */ 0, /* owner: */ scfg.lib[scfg.dir[dir]->lib]->vdir, uniq, 0, aliaspath[0] ? aliaspath : mls_path);
									else
										send_mlsx_entry(fp, sock, sess, mlsx_feats, "file", "r", (uint64_t)flength(np), fdate(np), get_owner_name(NULL, owner, sizeof owner), uniq, 0, mls_path);
								}
								l++;
							}

							fclose(alias_fp);
						}
					}

					/* Library folders */
					for (i = 0; i < scfg.total_libs; i++) {
						if (!chk_ar(&scfg, scfg.lib[i]->ar, &user, &client))
							continue;
						if (cmd[3] != 'D' && strcmp(scfg.lib[i]->vdir, mls_fname) != 0)
							continue;
						if (cmd[3] == 'T')
							sockprintf(sock, sess, "250- Listing %s", scfg.lib[i]->vdir);
						get_libperm(scfg.lib[i], &user, &client, permstr);
						get_owner_name(NULL, str, sizeof str);
						send_mlsx_entry(fp, sock, sess, mlsx_feats, "dir", permstr, UINT64_MAX, 0, str, NULL, 0, cmd[3] == 'T' ? mls_path : scfg.lib[i]->vdir);
						l++;
					}
				} else if (dir < 0) {
					if (cmd[3] == 'T' && !*mls_fname) {
						sockprintf(sock, sess, "250- Listing %s", scfg.lib[lib]->vdir);
						get_owner_name(NULL, str, sizeof str);
						SAFEPRINTF(aliaspath, "/%s", scfg.lib[lib]->vdir);
						send_mlsx_entry(fp, sock, sess, mlsx_feats, "dir", "el", UINT64_MAX, 0, str, NULL, 0, aliaspath);
						l++;
					}
					if (cmd[3] == 'D') {
						get_owner_name(NULL, str, sizeof str);
						send_mlsx_entry(fp, sock, sess, mlsx_feats, "pdir", (startup->options & FTP_OPT_ALLOW_QWK) ? "elc" : "el", UINT64_MAX, 0, str, NULL, 0, "/");
						SAFEPRINTF(aliaspath, "/%s", scfg.lib[lib]->vdir);
						send_mlsx_entry(fp, sock, sess, mlsx_feats, "cdir", (startup->options & FTP_OPT_ALLOW_QWK) ? "elc" : "el", UINT64_MAX, 0, str, NULL, 0, aliaspath);
					}
					lprintf(LOG_INFO, "%04d <%s> %s listing: %s library in %s mode"
					        , sock, user.alias, cmd, scfg.lib[lib]->vdir, mode);
					for (i = 0; i < scfg.total_dirs; i++) {
						if (scfg.dir[i]->lib != lib)
							continue;
						if (i != (int)scfg.sysop_dir && i != (int)scfg.upload_dir
						    && !chk_ar(&scfg, scfg.dir[i]->ar, &user, &client))
							continue;
						if (cmd[3] != 'D' && strcmp(scfg.dir[i]->vdir, mls_fname) != 0)
							continue;
						if (cmd[3] == 'T')
							sockprintf(sock, sess, "250- Listing %s", scfg.dir[i]->vdir);
						get_dirperm(scfg.lib[lib], scfg.dir[i], &user, &client, permstr);
						get_owner_name(NULL, str, sizeof str);
						SAFEPRINTF2(aliaspath, "/%s/%s", scfg.lib[lib]->vdir, scfg.dir[i]->vdir);
						get_unique(aliaspath, uniq);
						send_mlsx_entry(fp, sock, sess, mlsx_feats, "dir", permstr, UINT64_MAX, 0, str, uniq, 0, cmd[3] == 'T' ? mls_path : scfg.dir[i]->vdir);
						l++;
					}
				} else if (chk_ar(&scfg, scfg.dir[dir]->ar, &user, &client)) {
					lprintf(LOG_INFO, "%04d <%s> %s listing: /%s/%s directory in %s mode"
					        , sock, user.alias, cmd, scfg.lib[lib]->vdir, scfg.dir[dir]->vdir, mode);

					if (cmd[3] == 'T' && !*mls_fname) {
						sockprintf(sock, sess, "250- Listing %s/%s", scfg.lib[lib]->vdir, scfg.dir[dir]->vdir);
						get_owner_name(NULL, str, sizeof str);
						SAFEPRINTF2(aliaspath, "/%s/%s", scfg.lib[lib]->vdir, scfg.dir[dir]->vdir);
						get_unique(aliaspath, uniq);
						send_mlsx_entry(fp, sock, sess, mlsx_feats, "dir", (startup->options & FTP_OPT_ALLOW_QWK) ? "elc" : "el", UINT64_MAX, 0, str, uniq, 0, aliaspath);
						l++;
					}
					if (cmd[3] == 'D') {
						get_libperm(scfg.lib[lib], &user, &client, permstr);
						get_owner_name(NULL, str, sizeof str);
						SAFEPRINTF(aliaspath, "/%s", scfg.lib[lib]->vdir);
						send_mlsx_entry(fp, sock, sess, mlsx_feats, "pdir", permstr, UINT64_MAX, 0, str, NULL, 0, aliaspath);
						SAFEPRINTF2(aliaspath, "/%s/%s", scfg.lib[lib]->vdir, scfg.dir[dir]->vdir);
						get_unique(aliaspath, uniq);
						send_mlsx_entry(fp, sock, sess, mlsx_feats, "cdir", permstr, UINT64_MAX, 0, str, NULL, 0, aliaspath);
					}
					smb_t smb;
					if ((result = smb_open_dir(&scfg, &smb, dir)) != SMB_SUCCESS) {
						lprintf(LOG_ERR, "ERROR %d (%s) opening %s", result, smb.last_error, smb.file);
						continue;
					}
					time_t  start = time(NULL);
					size_t  file_count = 0;
					file_t* file_list = loadfiles(&smb
					                              , /* filespec */ NULL, /* time: */ 0, file_detail_normal, static_cast<file_sort>(scfg.dir[dir]->sort), &file_count);
					for (size_t i = 0; i < file_count; i++) {
						file_t* f = &file_list[i];
						if (cmd[3] != 'D' && strcmp(f->name, mls_fname) != 0)
							continue;
						if (cmd[3] == 'T')
							sockprintf(sock, sess, "250- Listing %s", p);
						get_fileperm(scfg.lib[lib], scfg.dir[dir], &user, &client, f, permstr);
						get_owner_name(f, str, sizeof str);
						SAFEPRINTF3(aliaspath, "/%s/%s/%s", scfg.lib[lib]->vdir, scfg.dir[dir]->vdir, f->name);
						get_unique(aliaspath, uniq);
						f->size = f->cost;
						f->time = f->hdr.when_imported.time;
						if (scfg.dir[dir]->misc & DIR_FCHK) {
							struct stat st;
							if (stat(getfilepath(&scfg, f, path), &st) != 0)
								continue;
							f->size = st.st_size;
							f->time = st.st_mtime;
							f->hdr.when_imported.time = (uint32_t)st.st_ctime;
						}
						send_mlsx_entry(fp, sock, sess, mlsx_feats, "file", permstr, f->size, f->time, str, uniq, f->hdr.when_imported.time, cmd[3] == 'T' ? mls_path : f->name);
						l++;
					}
					if (cmd[3] == 'D') {
						lprintf(LOG_INFO, "%04d <%s> %s listing (%ld bytes) of /%s/%s (%lu files) created in %" PRId64 " seconds"
						        , sock, user.alias, cmd, ftell(fp), scfg.lib[lib]->vdir, scfg.dir[dir]->vdir
						        , (ulong)file_count, (int64_t)(time(NULL) - start));
					}
					freefiles(file_list, file_count);
					smb_close(&smb);
				} else
					lprintf(LOG_INFO, "%04d <%s> %s listing: /%s/%s directory in %s mode (empty - no access)"
					        , sock, user.alias, cmd, scfg.lib[lib]->vdir, scfg.dir[dir]->vdir, mode);

				if (cmd[3] == 'D') {
					fclose(fp);
					filexfer(&data_addr, sock, sess, pasv_sock, pasv_sess, &data_sock, &data_sess, fname, 0L
					         , &transfer_inprogress, &transfer_aborted
					         , TRUE /* delfile */
					         , TRUE /* tmpfile */
					         , &lastactive, &user, &client, dir, FALSE, FALSE, FALSE, NULL, protection);
				}
				else {
					if (l == 0)
						sockprintf(sock, sess, "550 No such path");
					else
						sockprintf(sock, sess, "250 End");
				}
				continue;
			}
		}

		if (!strnicmp(cmd, "LIST", 4) || !strnicmp(cmd, "NLST", 4)) {
			dir = curdir;
			lib = curlib;

			if (cmd[4] != 0)
				lprintf(LOG_DEBUG, "%04d <%s> LIST/NLST: %s", sock, user.alias, cmd);

			/* path specified? */
			p = cmd + 4;
			SKIP_WHITESPACE(p);

			if (*p == '-') {   /* -Letc */
				FIND_WHITESPACE(p);
				SKIP_WHITESPACE(p);
			}

			if ((fp = fopen(ftp_tmpfname(fname, "lst", sock), "w+b")) == NULL) {
				lprintf(LOG_ERR, "%04d <%s> !ERROR %d (%s) line %d opening %s"
				        , sock, user.alias, errno, safe_strerror(errno, error, sizeof error), __LINE__, fname);
				sockprintf(sock, sess, "451 Insufficient system storage");
				continue;
			}
			sockprintf(sock, sess, "150 Opening ASCII mode data connection for /bin/ls.");

			if (parsepath(&p, &user, &client, &lib, &dir) == -1) {
				/* Empty list */
				fclose(fp);
				filexfer(&data_addr, sock, sess, pasv_sock, pasv_sess, &data_sock, &data_sess, fname, 0L
				         , &transfer_inprogress, &transfer_aborted
				         , TRUE /* delfile */
				         , TRUE /* tmpfile */
				         , &lastactive, &user, &client, dir, FALSE, FALSE, FALSE, NULL, protection);
				continue;
			}
			filespec = p;
			if (*filespec == 0)
				filespec = "*";

			if (!strnicmp(cmd, "LIST", 4))
				detail = TRUE;
			else
				detail = FALSE;
			now = time(NULL);
			if (localtime_r(&now, &cur_tm) == NULL)
				memset(&cur_tm, 0, sizeof(cur_tm));

			/* ASCII Index File */
			if (startup->options & FTP_OPT_INDEX_FILE && startup->index_file_name[0]
			    && wildmatchi(startup->index_file_name, filespec, FALSE)) {
				if (detail)
					fprintf(fp, "-r--r--r--   1 %-*s %-8s %9ld %s %2d %02d:%02d %s\r\n"
					        , NAME_LEN
					        , scfg.sys_id
					        , lib < 0 ? scfg.sys_id : dir < 0
					        ? scfg.lib[lib]->vdir : scfg.dir[dir]->vdir
					        , 512L
					        , ftp_mon[cur_tm.tm_mon], cur_tm.tm_mday, cur_tm.tm_hour, cur_tm.tm_min
					        , startup->index_file_name);
				else
					fprintf(fp, "%s\r\n", startup->index_file_name);
			}

			if (lib < 0) { /* Root dir */
				lprintf(LOG_INFO, "%04d <%s> %slisting: root in %s mode", sock, user.alias, detail ? "detailed ":"", mode);

				/* QWK Packet */
				if (startup->options & FTP_OPT_ALLOW_QWK) {
					SAFEPRINTF(str, "%s.qwk", scfg.sys_id);
					if (wildmatchi(str, filespec, FALSE)) {
						if (detail) {
							if (fexistcase(qwkfile)) {
								t = fdate(qwkfile);
								l = (ulong)flength(qwkfile);
							} else {
								t = time(NULL);
								l = 10240;
							};
							if (localtime_r(&t, &tm) == NULL)
								memset(&tm, 0, sizeof(tm));
							fprintf(fp, "-r--r--r--   1 %-*s %-8s %9ld %s %2d %02d:%02d %s\r\n"
							        , NAME_LEN
							        , scfg.sys_id
							        , scfg.sys_id
							        , l
							        , ftp_mon[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min
							        , str);
						} else
							fprintf(fp, "%s\r\n", str);
					}
				}

				/* File Aliases */
				for (int i = 0; i < scfg.total_dirs; ++i) {
					if (scfg.dir[i]->vshortcut[0] == '\0')
						continue;
					if (!wildmatchi(scfg.dir[i]->vshortcut, filespec, FALSE))
						continue;
					if (!user_can_access_dir(&scfg, i, &user, &client))
						continue;
					if (detail) {
						fprintf(fp, "drwxrwxrwx   1 %-*s %-8s %9ld %s %2d %02d:%02d %s\r\n"
						        , NAME_LEN
						        , scfg.sys_id
						        , scfg.lib[scfg.dir[i]->lib]->vdir
						        , 512L
						        , ftp_mon[cur_tm.tm_mon], cur_tm.tm_mday, cur_tm.tm_hour, cur_tm.tm_min
						        , scfg.dir[i]->vshortcut);
					} else
						fprintf(fp, "%s\r\n", scfg.dir[i]->vshortcut);
				}

				sprintf(aliasfile, "%sftpalias.cfg", scfg.ctrl_dir);
				if ((alias_fp = fopen(aliasfile, "r")) != NULL) {

					while (!feof(alias_fp)) {
						if (!fgets(aliasline, sizeof(aliasline), alias_fp))
							break;

						alias_dir = FALSE;

						p = aliasline;        /* alias pointer */
						SKIP_WHITESPACE(p);

						if (*p == ';') /* comment */
							continue;

						tp = p;       /* terminator pointer */
						FIND_WHITESPACE(tp);
						if (*tp)
							*tp = 0;

						np = tp + 1;    /* filename pointer */
						SKIP_WHITESPACE(np);

						tp = np;      /* terminator pointer */
						FIND_WHITESPACE(tp);
						if (*tp)
							*tp = 0;

						dp = tp + 1;    /* description pointer */
						SKIP_WHITESPACE(dp);
						truncsp(dp);

						if (stricmp(dp, BBS_HIDDEN_ALIAS) == 0)
							continue;

						if (!wildmatchi(p, filespec, FALSE))
							continue;

						/* Virtual Path? */
						if (!strnicmp(np, BBS_VIRTUAL_PATH, strlen(BBS_VIRTUAL_PATH))) {
							if ((dir = getdir_from_vpath(&scfg, np + strlen(BBS_VIRTUAL_PATH), &user, &client, true)) < 0) {
								lprintf(LOG_WARNING, "%04d <%s> !Invalid virtual path: %s", sock, user.alias, np);
								continue; /* No access or invalid virtual path */
							}
							tp = strrchr(np, '/');
							if (tp == NULL)
								continue;
							tp++;
							if (*tp) {
								SAFEPRINTF2(aliasfile, "%s%s", scfg.dir[dir]->path, tp);
								np = aliasfile;
							}
							else
								alias_dir = TRUE;
						}

						if (!alias_dir && !fexist(np)) {
							lprintf(LOG_WARNING, "%04d <%s> !Missing aliased file: %s", sock, user.alias, np);
							continue;
						}

						if (detail) {

							if (alias_dir == TRUE) {
								fprintf(fp, "drwxrwxrwx   1 %-*s %-8s %9ld %s %2d %02d:%02d %s\r\n"
								        , NAME_LEN
								        , scfg.sys_id
								        , scfg.lib[scfg.dir[dir]->lib]->vdir
								        , 512L
								        , ftp_mon[cur_tm.tm_mon], cur_tm.tm_mday, cur_tm.tm_hour, cur_tm.tm_min
								        , p);
							}
							else {
								t = fdate(np);
								if (localtime_r(&t, &tm) == NULL)
									memset(&tm, 0, sizeof(tm));
								fprintf(fp, "-r--r--r--   1 %-*s %-8s %9" PRIdOFF " %s %2d %02d:%02d %s\r\n"
								        , NAME_LEN
								        , scfg.sys_id
								        , scfg.sys_id
								        , flength(np)
								        , ftp_mon[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min
								        , p);
							}
						} else
							fprintf(fp, "%s\r\n", p);

					}

					fclose(alias_fp);
				}

				/* Library folders */
				for (i = 0; i < scfg.total_libs; i++) {
					if (!chk_ar(&scfg, scfg.lib[i]->ar, &user, &client))
						continue;
					if (!wildmatchi(scfg.lib[i]->vdir, filespec, FALSE))
						continue;
					if (detail)
						fprintf(fp, "dr-xr-xr-x   1 %-*s %-8s %9ld %s %2d %02d:%02d %s\r\n"
						        , NAME_LEN
						        , scfg.sys_id
						        , scfg.sys_id
						        , 512L
						        , ftp_mon[cur_tm.tm_mon], cur_tm.tm_mday, cur_tm.tm_hour, cur_tm.tm_min
						        , scfg.lib[i]->vdir);
					else
						fprintf(fp, "%s\r\n", scfg.lib[i]->vdir);
				}
			} else if (dir < 0) {
				lprintf(LOG_INFO, "%04d <%s> %slisting: %s library in %s mode"
				        , sock, user.alias, detail ? "detailed ":"", scfg.lib[lib]->vdir, mode);
				for (i = 0; i < scfg.total_dirs; i++) {
					if (scfg.dir[i]->lib != lib)
						continue;
					if (i != (int)scfg.sysop_dir && i != (int)scfg.upload_dir
					    && !chk_ar(&scfg, scfg.dir[i]->ar, &user, &client))
						continue;
					if (!wildmatchi(scfg.dir[i]->vdir, filespec, FALSE))
						continue;
					if (detail)
						fprintf(fp, "drwxrwxrwx   1 %-*s %-8s %9ld %s %2d %02d:%02d %s\r\n"
						        , NAME_LEN
						        , scfg.sys_id
						        , scfg.lib[lib]->vdir
						        , 512L
						        , ftp_mon[cur_tm.tm_mon], cur_tm.tm_mday, cur_tm.tm_hour, cur_tm.tm_min
						        , scfg.dir[i]->vdir);
					else
						fprintf(fp, "%s\r\n", scfg.dir[i]->vdir);
				}
			} else if (chk_ar(&scfg, scfg.dir[dir]->ar, &user, &client)) {
				lprintf(LOG_INFO, "%04d <%s> %slisting: /%s/%s directory in %s mode"
				        , sock, user.alias, detail ? "detailed ":""
				        , scfg.lib[lib]->vdir, scfg.dir[dir]->vdir, mode);

				smb_t smb;
				if ((result = smb_open_dir(&scfg, &smb, dir)) != SMB_SUCCESS) {
					lprintf(LOG_ERR, "ERROR %d (%s) opening %s", result, smb.last_error, smb.file);
					continue;
				}
				time_t  start = time(NULL);
				size_t  file_count = 0;
				file_t* file_list = loadfiles(&smb
				                              , filespec, /* time: */ 0, file_detail_normal, static_cast<file_sort>(scfg.dir[dir]->sort), &file_count);
				for (size_t i = 0; i < file_count; i++) {
					file_t* f = &file_list[i];
					if (detail) {
						f->size = f->cost;
						t = f->hdr.when_imported.time;
						if (scfg.dir[dir]->misc & DIR_FCHK) {
							struct stat st;
							if (stat(getfilepath(&scfg, f, path), &st) != 0)
								continue;
							f->size = st.st_size;
							t = st.st_mtime;
						}
						if (localtime_r(&t, &tm) == NULL)
							memset(&tm, 0, sizeof(tm));
						if (f->hdr.attr & MSG_ANONYMOUS)
							SAFECOPY(str, ANONYMOUS);
						else
							dotname(f->from, str);
						fprintf(fp, "-r--r--r--   1 %-*s %-8s %9" PRId64 " %s %2d "
						        , NAME_LEN
						        , str
						        , scfg.dir[dir]->vdir
						        , (int64_t)f->size
						        , ftp_mon[tm.tm_mon], tm.tm_mday);
						if (tm.tm_year == cur_tm.tm_year)
							fprintf(fp, "%02d:%02d %s\r\n"
							        , tm.tm_hour, tm.tm_min
							        , f->name);
						else
							fprintf(fp, "%5d %s\r\n"
							        , 1900 + tm.tm_year
							        , f->name);
					} else
						fprintf(fp, "%s\r\n", f->name);
				}
				lprintf(LOG_INFO, "%04d <%s> %slisting (%ld bytes) of /%s/%s (%lu files) created in %" PRId64 " seconds"
				        , sock, user.alias, detail ? "detailed ":"", ftell(fp), scfg.lib[lib]->vdir, scfg.dir[dir]->vdir
				        , (ulong)file_count, (int64_t)(time(NULL) - start));
				freefiles(file_list, file_count);
				smb_close(&smb);
			} else
				lprintf(LOG_INFO, "%04d <%s> %slisting: /%s/%s directory in %s mode (empty - no access)"
				        , sock, user.alias, detail ? "detailed ":"", scfg.lib[lib]->vdir, scfg.dir[dir]->vdir, mode);

			fclose(fp);
			filexfer(&data_addr, sock, sess, pasv_sock, pasv_sess, &data_sock, &data_sess, fname, 0L
			         , &transfer_inprogress, &transfer_aborted
			         , TRUE /* delfile */
			         , TRUE /* tmpfile */
			         , &lastactive, &user, &client, dir, FALSE, FALSE, FALSE, NULL, protection);
			continue;
		}

		if (!strnicmp(cmd, "RETR ", 5)
		    || !strnicmp(cmd, "SIZE ", 5)
		    || !strnicmp(cmd, "MDTM ", 5)
		    || !strnicmp(cmd, "DELE ", 5)) {
			getdate = FALSE;
			getsize = FALSE;
			delecmd = FALSE;
			file_date = 0;
			file_size = -1;
			if (!strnicmp(cmd, "SIZE ", 5))
				getsize = TRUE;
			else if (!strnicmp(cmd, "MDTM ", 5))
				getdate = TRUE;
			else if (!strnicmp(cmd, "DELE ", 5))
				delecmd = TRUE;

			if (!getsize && !getdate && user.rest & FLAG('D')) {
				sockprintf(sock, sess, "550 Insufficient access.");
				filepos = 0;
				continue;
			}
			credits = TRUE;
			success = FALSE;
			delfile = FALSE;
			tmpfile = FALSE;
			lib = curlib;
			dir = curdir;

			p = cmd + 5;
			SKIP_WHITESPACE(p);

			if (!strnicmp(p, BBS_FSYS_DIR, strlen(BBS_FSYS_DIR)))
				p += strlen(BBS_FSYS_DIR);  /* already mounted */

			if (*p == '/') {
				lib = -1;
				p++;
			}
			if (!strncmp(p, "./", 2))
				p += 2;

			if (lib < 0 && ftpalias(p, fname, &user, &client, &dir) == TRUE) {
				success = TRUE;
				credits = TRUE;   /* include in d/l stats */
				tmpfile = FALSE;
				delfile = FALSE;
				lprintf(LOG_INFO, "%04d <%s> %.4s by alias: %s"
				        , sock, user.alias, cmd, p);
				p = getfname(fname);
				if (dir >= 0)
					lib = scfg.dir[dir]->lib;
			}
			if (!success && lib < 0 && (tp = strchr(p, '/')) != NULL) {
				dir = -1;
				*tp = 0;
				for (i = 0; i < scfg.total_libs; i++) {
					if (!chk_ar(&scfg, scfg.lib[i]->ar, &user, &client))
						continue;
					if (!stricmp(scfg.lib[i]->vdir, p))
						break;
				}
				if (i < scfg.total_libs)
					lib = i;
				p = tp + 1;
			}
			if (!success && dir < 0 && (tp = strchr(p, '/')) != NULL) {
				*tp = 0;
				for (i = 0; i < scfg.total_dirs; i++) {
					if (scfg.dir[i]->lib != lib)
						continue;
					if (!chk_ar(&scfg, scfg.dir[i]->ar, &user, &client))
						continue;
					if (!stricmp(scfg.dir[i]->vdir, p))
						break;
				}
				if (i < scfg.total_dirs)
					dir = i;
				p = tp + 1;
			}

			sprintf(str, "%s.qwk", scfg.sys_id);
			if (lib < 0 && startup->options & FTP_OPT_ALLOW_QWK
			    && !stricmp(p, str) && !delecmd) {
				if (!fexistcase(qwkfile)) {
					lprintf(LOG_INFO, "%04d <%s> creating QWK packet...", sock, user.alias);
					char hostname[128];
					if (gethostname(hostname, sizeof hostname) != 0)
						SAFECOPY(hostname, server_host_name());
					snprintf(str, sizeof str, "%spack%04u.%s.now", scfg.data_dir, user.number, hostname);
					if (!fmutex(str, startup->host_name, /* max_age: */ 60 * 60, /* time: */ NULL)) {
						lprintf(LOG_WARNING, "%04d <%s> !ERROR %d (%s) creating mutex-semaphore file: %s"
						        , sock, user.alias, errno, safe_strerror(errno, error, sizeof error), str);
						sockprintf(sock, sess, "451 Packet creation already in progress (are you logged-in concurrently?)");
						filepos = 0;
						continue;
					}

					t = time(NULL);
					while (fexist(str) && !terminate_server) {
						if (!socket_check(sock, NULL, NULL, 0))
							break;
						if (time(NULL) - t > startup->qwk_timeout)
							break;
						mswait(1000);
					}
					if (!socket_check(sock, NULL, NULL, 0)) {
						lprintf(LOG_NOTICE, "%04d <%s> disconnected while waiting for QWK packet creation"
						        , sock, user.alias);
						ftp_remove(sock, __LINE__, str, user.alias, LOG_ERR);
						continue;
					}
					if (fexist(str)) {
						lprintf(LOG_WARNING, "%04d <%s> !TIMEOUT waiting for QWK packet creation", sock, user.alias);
						sockprintf(sock, sess, "451 Time-out waiting for packet creation.");
						ftp_remove(sock, __LINE__, str, user.alias, LOG_ERR);
						filepos = 0;
						continue;
					}
					if (!fexistcase(qwkfile)) {
						lprintf(LOG_INFO, "%04d <%s> No QWK Packet created (no new messages)", sock, user.alias);
						sockprintf(sock, sess, "550 No QWK packet created (no new messages)");
						filepos = 0;
						continue;
					}
				}
				SAFECOPY(fname, qwkfile);
				file_size = flength(fname);
				if (file_size < 1) {
					lprintf(LOG_WARNING, "%04d <%s> Invalid QWK packet file size (%" PRIdOFF " bytes): %s"
					        , sock, user.alias, file_size, fname);
					sockprintf(sock, sess, "550 Invalid QWK packet file size: %" PRIdOFF " bytes", file_size);
					filepos = 0;
					continue;
				}
				success = TRUE;
				delfile = TRUE;
				credits = FALSE;
				if (!getsize && !getdate)
					lprintf(LOG_INFO, "%04d <%s> downloading QWK packet (%s bytes) in %s mode"
					        , sock, user.alias, byte_estimate_to_str(file_size, tmp, sizeof tmp, 1, 1)
					        , mode);
				/* ASCII Index File */
			} else if (startup->options & FTP_OPT_INDEX_FILE
			           && !stricmp(p, startup->index_file_name)
			           && !delecmd) {
				if (getsize) {
					sockprintf(sock, sess, "550 Size not available for dynamically generated files");
					continue;
				}
				if ((fp = fopen(ftp_tmpfname(fname, "ndx", sock), "wb")) == NULL) {
					lprintf(LOG_ERR, "%04d <%s> !ERROR %d (%s) line %d opening %s"
					        , sock, user.alias, errno, safe_strerror(errno, error, sizeof error), __LINE__, fname);
					sockprintf(sock, sess, "451 Insufficient system storage");
					filepos = 0;
					continue;
				}
				success = TRUE;
				if (getdate)
					file_date = time(NULL);
				else {
					lprintf(LOG_INFO, "%04d <%s> downloading %s for %s in %s mode"
					        , sock, user.alias, startup->index_file_name, genvpath(lib, dir, str)
					        , mode);
					credits = FALSE;
					tmpfile = TRUE;
					delfile = TRUE;
					fprintf(fp, "%-*s File/Folder Descriptions\r\n"
					        , INDEX_FNAME_LEN, startup->index_file_name);

					if (lib < 0) {

						/* File Aliases */
						for (int i = 0; i < scfg.total_dirs; ++i) {
							if (scfg.dir[i]->vshortcut[0] == '\0')
								continue;
							if (!user_can_access_dir(&scfg, i, &user, &client))
								continue;
							fprintf(fp, "%-*s %s\r\n", INDEX_FNAME_LEN, scfg.dir[i]->vshortcut, scfg.dir[i]->lname);
						}

						sprintf(aliasfile, "%sftpalias.cfg", scfg.ctrl_dir);
						if ((alias_fp = fopen(aliasfile, "r")) != NULL) {

							while (!feof(alias_fp)) {
								if (!fgets(aliasline, sizeof(aliasline), alias_fp))
									break;

								p = aliasline;    /* alias pointer */
								SKIP_WHITESPACE(p);

								if (*p == ';') /* comment */
									continue;

								tp = p;       /* terminator pointer */
								FIND_WHITESPACE(tp);
								if (*tp)
									*tp = 0;

								np = tp + 1;    /* filename pointer */
								SKIP_WHITESPACE(np);

								np++;       /* description pointer */
								FIND_WHITESPACE(np);

								while (*np && *np < ' ') np++;

								truncsp(np);

								fprintf(fp, "%-*s %s\r\n", INDEX_FNAME_LEN, p, np);
							}

							fclose(alias_fp);
						}

						/* QWK Packet */
						if (startup->options & FTP_OPT_ALLOW_QWK /* && fexist(qwkfile) */) {
							sprintf(str, "%s.qwk", scfg.sys_id);
							fprintf(fp, "%-*s QWK Message Packet\r\n"
							        , INDEX_FNAME_LEN, str);
						}

						/* Library Folders */
						for (i = 0; i < scfg.total_libs; i++) {
							if (!chk_ar(&scfg, scfg.lib[i]->ar, &user, &client))
								continue;
							fprintf(fp, "%-*s %s\r\n"
							        , INDEX_FNAME_LEN, scfg.lib[i]->vdir, scfg.lib[i]->lname);
						}
					} else if (dir < 0) {
						for (i = 0; i < scfg.total_dirs; i++) {
							if (scfg.dir[i]->lib != lib)
								continue;
							if (i != (int)scfg.sysop_dir && i != (int)scfg.upload_dir
							    && !chk_ar(&scfg, scfg.dir[i]->ar, &user, &client))
								continue;
							fprintf(fp, "%-*s %s\r\n"
							        , INDEX_FNAME_LEN, scfg.dir[i]->vdir, scfg.dir[i]->lname);
						}
					} else if (chk_ar(&scfg, scfg.dir[dir]->ar, &user, &client)) {
						smb_t smb;
						if ((result = smb_open_dir(&scfg, &smb, dir)) != SMB_SUCCESS) {
							lprintf(LOG_ERR, "ERROR %d (%s) opening %s", result, smb.last_error, smb.file);
							continue;
						}
						time_t  start = time(NULL);
						size_t  file_count = 0;
						file_t* file_list = loadfiles(&smb
						                              , /* filespec */ NULL, /* time: */ 0, file_detail_normal, static_cast<file_sort>(scfg.dir[dir]->sort), &file_count);
						for (size_t i = 0; i < file_count; i++) {
							file_t* f = &file_list[i];
							fprintf(fp, "%-*s %s\r\n", INDEX_FNAME_LEN
							        , f->name, f->desc);
						}
						lprintf(LOG_INFO, "%04d <%s> index (%ld bytes) of /%s/%s (%lu files) created in %" PRId64 " seconds"
						        , sock, user.alias, ftell(fp), scfg.lib[lib]->vdir, scfg.dir[dir]->vdir
						        , (ulong)file_count, (int64_t)(time(NULL) - start));
						freefiles(file_list, file_count);
						smb_close(&smb);
					}
					fclose(fp);
				}
			} else if (dir >= 0) {

				if (!chk_ar(&scfg, scfg.dir[dir]->ar, &user, &client)) {
					lprintf(LOG_WARNING, "%04d <%s> has insufficient access to /%s/%s"
					        , sock, user.alias
					        , scfg.lib[scfg.dir[dir]->lib]->vdir
					        , scfg.dir[dir]->vdir);
					sockprintf(sock, sess, "550 Insufficient access.");
					filepos = 0;
					continue;
				}

				uint reason = R_Download;
				if (!getsize && !getdate && !delecmd
				    && !user_can_download(&scfg, dir, &user, &client, &reason)) {
					lprintf(LOG_WARNING, "%04d <%s> has insufficient access (reason: %u) to download from /%s/%s"
					        , sock, user.alias
					        , reason + 1
					        , scfg.lib[scfg.dir[dir]->lib]->vdir
					        , scfg.dir[dir]->vdir);
					sockprintf(sock, sess, "550 Insufficient access.");
					filepos = 0;
					continue;
				}

				filedat = findfile(&scfg, dir, p, NULL);
				if (!filedat) {
					sockprintf(sock, sess, "550 File not found: %s", p);
					lprintf(LOG_WARNING, "%04d <%s> file (%s%s) not in database for %.4s command"
					        , sock, user.alias, genvpath(lib, dir, str), p, cmd);
					filepos = 0;
					continue;
				}
				if (delecmd && !user_is_dirop(&scfg, dir, &user, &client) && !(user.exempt & FLAG('R'))) {
					file_t f = {0};
					if (filedat)
						loadfile(&scfg, dir, p, &f, file_detail_normal, NULL);
					if (stricmp(f.from, user.alias)) {
						lprintf(LOG_WARNING, "%04d <%s> has insufficient access to delete file (%s) uploaded by %s in /%s/%s"
								, sock, user.alias
								, p
								, f.from[0] == 0 ? STR_UNKNOWN_USER : f.from
								, scfg.lib[scfg.dir[dir]->lib]->vdir
								, scfg.dir[dir]->vdir);
						sockprintf(sock, sess, "550 Insufficient access.");
						filepos = 0;
						smb_freefilemem(&f);
						continue;
					}
					smb_freefilemem(&f);
				}
				SAFEPRINTF2(fname, "%s%s", scfg.dir[dir]->path, p);

				/* Verify credits */
				if (!getsize && !getdate && !delecmd
				    && !download_is_free(&scfg, dir, &user, &client)) {
					file_t f;
					if (filedat)
						loadfile(&scfg, dir, p, &f, file_detail_normal, NULL);
					else
						f.cost = (uint32_t)flength(fname);
					if (f.cost > user_available_credits(&user)) {
						lprintf(LOG_WARNING, "%04d <%s> has insufficient credit to download /%s/%s/%s (%lu credits)"
						        , sock, user.alias, scfg.lib[scfg.dir[dir]->lib]->vdir
						        , scfg.dir[dir]->vdir
						        , p
						        , (ulong)f.cost);
						sockprintf(sock, sess, "550 Insufficient credit (%lu required).", (ulong)f.cost);
						filepos = 0;
						smb_freefilemem(&f);
						continue;
					}
					smb_freefilemem(&f);
				}

				if (strcspn(p, ILLEGAL_FILENAME_CHARS) != strlen(p)) {
					success = FALSE;
					lprintf(LOG_WARNING, "%04d <%s> !ILLEGAL FILENAME ATTEMPT by %s [%s]: '%s'"
					        , sock, user.alias, host_name, host_ip, p);
					ftp_hacklog("FTP FILENAME", user.alias, cmd, host_name, &ftp.client_addr);
				} else {
					if (fexistcase(fname)) {
						success = TRUE;
						if (!getsize && !getdate && !delecmd)
							lprintf(LOG_INFO, "%04d <%s> downloading: %s (%s bytes) in %s mode"
							        , sock, user.alias, fname, byte_estimate_to_str(flength(fname), tmp, sizeof tmp, 1, 1)
							        , mode);
					}
				}
			}
#if defined(SOCKET_DEBUG_DOWNLOAD)
			socket_debug[sock] |= SOCKET_DEBUG_DOWNLOAD;
#endif

			if (getsize && success)
				sockprintf(sock, sess, "213 %" PRIdOFF, flength(fname));
			else if (getdate && success) {
				if (file_date == 0)
					file_date = fdate(fname);
				if (gmtime_r(&file_date, &tm) == NULL)  /* specifically use GMT/UTC representation */
					memset(&tm, 0, sizeof(tm));
				sockprintf(sock, sess, "213 %u%02u%02u%02u%02u%02u"
				           , 1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday
				           , tm.tm_hour, tm.tm_min, tm.tm_sec);
			} else if (delecmd && success) {
				if (removecase(fname) != 0) {
					lprintf(LOG_ERR, "%04d <%s> !ERROR %d (%s) deleting %s"
					        , sock, user.alias, errno, safe_strerror(errno, error, sizeof error), fname);
					sockprintf(sock, sess, "450 %s could not be deleted (error: %d)"
					           , fname, errno);
				} else {
					lprintf(LOG_NOTICE, "%04d <%s> deleted %s", sock, user.alias, fname);
					if (filedat)
						removefile(&scfg, dir, getfname(fname), NULL);
					sockprintf(sock, sess, "250 %s deleted.", fname);
				}
			} else if (success) {
				sockprintf(sock, sess, "150 Opening BINARY mode data connection for file transfer.");
				filexfer(&data_addr, sock, sess, pasv_sock, pasv_sess, &data_sock, &data_sess, fname, filepos
				         , &transfer_inprogress, &transfer_aborted, delfile, tmpfile
				         , &lastactive, &user, &client, dir, FALSE, credits, FALSE, NULL, protection);
			}
			else {
				sockprintf(sock, sess, "550 File not found: %s", p);
				lprintf(LOG_WARNING, "%04d <%s> file (%s%s) not found for %.4s command"
				        , sock, user.alias, genvpath(lib, dir, str), p, cmd);
			}
			filepos = 0;
#if defined(SOCKET_DEBUG_DOWNLOAD)
			socket_debug[sock] &= ~SOCKET_DEBUG_DOWNLOAD;
#endif
			continue;
		}

		if (!strnicmp(cmd, "DESC", 4)) {

			if (user.rest & FLAG('U')) {
				sockprintf(sock, sess, "553 Insufficient access.");
				continue;
			}

			p = cmd + 4;
			SKIP_WHITESPACE(p);

			if (*p == 0)
				sockprintf(sock, sess, "501 No file description given.");
			else {
				SAFECOPY(desc, p);
				sockprintf(sock, sess, "200 File description set. Ready to STOR file.");
			}
			continue;
		}

		if (!strnicmp(cmd, "STOR ", 5) || !strnicmp(cmd, "APPE ", 5)) {

			if (user.rest & FLAG('U')) {
				sockprintf(sock, sess, "553 Insufficient access.");
				continue;
			}

			if (transfer_inprogress == TRUE) {
				lprintf(LOG_WARNING, "%04d <%s> !TRANSFER already in progress (%s)", sock, user.alias, cmd);
				sockprintf(sock, sess, "425 Transfer already in progress.");
				continue;
			}

			append = FALSE;
			lib = curlib;
			dir = curdir;
			p = cmd + 5;

			SKIP_WHITESPACE(p);

			if (!strnicmp(p, BBS_FSYS_DIR, strlen(BBS_FSYS_DIR)))
				p += strlen(BBS_FSYS_DIR);  /* already mounted */

			if (*p == '/') {
				lib = -1;
				p++;
			}
			if (!strncmp(p, "./", 2))
				p += 2;
			/* Need to add support for uploading to aliased directories */
			if (lib < 0 && (tp = strchr(p, '/')) != NULL) {
				dir = -1;
				*tp = 0;
				for (i = 0; i < scfg.total_libs; i++) {
					if (!chk_ar(&scfg, scfg.lib[i]->ar, &user, &client))
						continue;
					if (!stricmp(scfg.lib[i]->vdir, p))
						break;
				}
				if (i < scfg.total_libs)
					lib = i;
				p = tp + 1;
			}
			if (dir < 0 && (tp = strchr(p, '/')) != NULL) {
				*tp = 0;
				for (i = 0; i < scfg.total_dirs; i++) {
					if (scfg.dir[i]->lib != lib)
						continue;
					if (i != (int)scfg.sysop_dir && i != (int)scfg.upload_dir
					    && !chk_ar(&scfg, scfg.dir[i]->ar, &user, &client))
						continue;
					if (!stricmp(scfg.dir[i]->vdir, p))
						break;
				}
				if (i < scfg.total_dirs)
					dir = i;
				p = tp + 1;
			}
			int64_t freespace;
			if (dir < 0) {
				sprintf(str, "%s.rep", scfg.sys_id);
				if (!(startup->options & FTP_OPT_ALLOW_QWK)
				    || stricmp(p, str)) {
					lprintf(LOG_WARNING, "%04d <%s> !attempted to upload invalid path or filename: '%s'"
					        , sock, user.alias, p);
					sockprintf(sock, sess, "553 Invalid path or filename.");
					continue;
				}
				sprintf(fname, "%sfile/%04d.rep", scfg.data_dir, user.number);
				lprintf(LOG_INFO, "%04d <%s> uploading: %s in %s mode"
				        , sock, user.alias, fname
				        , mode);
				freespace = getfreediskspace(scfg.data_dir, 1);
			} else {

				append = (strnicmp(cmd, "APPE", 4) == 0);

				if (!user_is_dirop(&scfg, dir, &user, &client) && !(user.exempt & FLAG('U'))) {
					if (!chk_ar(&scfg, scfg.dir[dir]->ul_ar, &user, &client)
					    || !chk_ar(&scfg, scfg.lib[scfg.dir[dir]->lib]->ul_ar, &user, &client)) {
						lprintf(LOG_WARNING, "%04d <%s> cannot upload to /%s/%s (insufficient access)"
						        , sock, user.alias
						        , scfg.lib[scfg.dir[dir]->lib]->vdir
						        , scfg.dir[dir]->vdir);
						sockprintf(sock, sess, "553 Insufficient access.");
						continue;
					}

					if (!append && scfg.dir[dir]->maxfiles && getfiles(&scfg, dir) >= scfg.dir[dir]->maxfiles) {
						lprintf(LOG_WARNING, "%04d <%s> cannot upload to /%s/%s (directory full: %d files)"
						        , sock, user.alias
						        , scfg.lib[scfg.dir[dir]->lib]->vdir
						        , scfg.dir[dir]->vdir
						        , getfiles(&scfg, dir));
						sockprintf(sock, sess, "553 Directory full.");
						continue;
					}
				}
				if (illegal_filename(p)
				    || trashcan(&scfg, p, "file")) {
					lprintf(LOG_WARNING, "%04d <%s> !ILLEGAL FILENAME ATTEMPT by %s [%s]: '%s'"
					        , sock, user.alias, host_name, host_ip, p);
					sockprintf(sock, sess, "553 Illegal filename attempt");
					ftp_hacklog("FTP FILENAME", user.alias, cmd, host_name, &ftp.client_addr);
					continue;
				}
				if (!allowed_filename(&scfg, p)) {
					lprintf(LOG_WARNING, "%04d <%s> !UNALLOWED FILENAME ATTEMPT by %s [%s]: '%s'"
					        , sock, user.alias, host_name, host_ip, p);
					sockprintf(sock, sess, "553 Unallowed filename attempt");
					continue;
				}
				SAFEPRINTF2(fname, "%s%s", scfg.dir[dir]->path, p);
				if ((!append && filepos == 0 && fexist(fname))
				    || (startup->options & FTP_OPT_INDEX_FILE
				        && !stricmp(p, startup->index_file_name))
				    ) {
					lprintf(LOG_WARNING, "%04d <%s> attempted to overwrite existing file: '%s'"
					        , sock, user.alias, fname);
					sockprintf(sock, sess, "553 File already exists.");
					continue;
				}
				if (append || filepos) { /* RESUME */
					file_t f;
					if (!loadfile(&scfg, dir, p, &f, file_detail_normal, NULL)) {
						if (filepos) {
							lprintf(LOG_WARNING, "%04d <%s> file (%s) not in database for %.4s command"
							        , sock, user.alias, fname, cmd);
							sockprintf(sock, sess, "550 File not found: %s", p);
							continue;
						}
						append = FALSE;
					}
					/* Verify user is original uploader */
					if ((append || filepos) && stricmp(f.from, user.alias)) {
						lprintf(LOG_WARNING, "%04d <%s> !cannot resume upload of %s, uploaded by %s"
						        , sock, user.alias, fname, f.from);
						sockprintf(sock, sess, "553 Insufficient access (can't resume upload from different user).");
						smb_freefilemem(&f);
						continue;
					}
					smb_freefilemem(&f);
				}
				lprintf(LOG_INFO, "%04d <%s> uploading: %s to %s (%s) in %s mode"
				        , sock, user.alias
				        , p                 /* filename */
				        , genvpath(lib, dir, str) /* virtual path */
				        , scfg.dir[dir]->path /* actual path */
				        , mode);
				freespace = getfreediskspace(scfg.dir[dir]->path, 1);
			}
			if (freespace < scfg.min_dspace) {
				lprintf(diskspace_error_reported ? LOG_WARNING : LOG_ERR, "%04d <%s> !Insufficient free disk space (%s bytes) to allow upload"
				        , sock, user.alias, byte_estimate_to_str(freespace, str, sizeof(str), 1, 1));
				sockprintf(sock, sess, "452 Insufficient free disk space, try again later");
				diskspace_error_reported = true;
				continue;
			}
			diskspace_error_reported = false;
			sockprintf(sock, sess, "150 Opening BINARY mode data connection for file transfer.");
			filexfer(&data_addr, sock, sess, pasv_sock, pasv_sess, &data_sock, &data_sess, fname, filepos
			         , &transfer_inprogress, &transfer_aborted, FALSE, FALSE
			         , &lastactive
			         , &user
			         , &client
			         , dir
			         , TRUE /* uploading */
			         , TRUE /* credits */
			         , append
			         , desc
			         , protection
			         );
			filepos = 0;
			continue;
		}

		if (!stricmp(cmd, "CDUP") || !stricmp(cmd, "XCUP")) {
			if (curdir < 0)
				curlib = -1;
			else
				curdir = -1;
			sockprintf(sock, sess, "200 CDUP command successful.");
			continue;
		}

		if (!strnicmp(cmd, "CWD ", 4) || !strnicmp(cmd, "XCWD ", 5)) {
			p = cmd + 4;
			SKIP_WHITESPACE(p);

			if (!strnicmp(p, BBS_FSYS_DIR, strlen(BBS_FSYS_DIR)))
				p += strlen(BBS_FSYS_DIR);  /* already mounted */

			if (*p == '/') {
				curlib = -1;
				curdir = -1;
				p++;
			}
			/* Local File System? */
			if (sysop && !(startup->options & FTP_OPT_NO_LOCAL_FSYS)
			    && !strnicmp(p, LOCAL_FSYS_DIR, strlen(LOCAL_FSYS_DIR))) {
				p += strlen(LOCAL_FSYS_DIR);
				if (!direxist(p)) {
					sockprintf(sock, sess, "550 Directory does not exist.");
					lprintf(LOG_WARNING, "%04d <%s> attempted to mount invalid directory: '%s'"
					        , sock, user.alias, p);
					continue;
				}
				SAFECOPY(local_dir, p);
				local_fsys = TRUE;
				sockprintf(sock, sess, "250 CWD command successful (local file system mounted).");
				lprintf(LOG_INFO, "%04d <%s> mounted local file system", sock, user.alias);
				continue;
			}
			success = FALSE;

			/* Directory Alias? */
			if (curlib < 0 && ftpalias(p, NULL, &user, &client, &curdir) == TRUE) {
				if (curdir >= 0)
					curlib = scfg.dir[curdir]->lib;
				success = TRUE;
			}

			orglib = curlib;
			orgdir = curdir;
			tp = 0;
			if (!strncmp(p, "...", 3)) {
				curlib = -1;
				curdir = -1;
				p += 3;
			}
			if (!strncmp(p, "./", 2))
				p += 2;
			else if (!strncmp(p, "..", 2)) {
				if (curdir < 0)
					curlib = -1;
				else
					curdir = -1;
				p += 2;
			}
			if (*p == 0)
				success = TRUE;
			else if (!strcmp(p, "."))
				success = TRUE;
			if (!success  && (curlib < 0 || *p == '/')) { /* Root dir */
				if (*p == '/')
					p++;
				tp = strchr(p, '/');
				if (tp)
					*tp = 0;
				for (i = 0; i < scfg.total_libs; i++) {
					if (!chk_ar(&scfg, scfg.lib[i]->ar, &user, &client))
						continue;
					if (!stricmp(scfg.lib[i]->vdir, p))
						break;
				}
				if (i < scfg.total_libs) {
					curlib = i;
					success = TRUE;
				}
			}
			if ((!success && curdir < 0) || (success && tp && *(tp + 1))) {
				if (tp)
					p = tp + 1;
				tp = lastchar(p);
				if (tp && *tp == '/')
					*tp = 0;
				for (i = 0; i < scfg.total_dirs; i++) {
					if (scfg.dir[i]->lib != curlib)
						continue;
					if (i != (int)scfg.sysop_dir && i != (int)scfg.upload_dir
					    && !chk_ar(&scfg, scfg.dir[i]->ar, &user, &client))
						continue;
					if (!stricmp(scfg.dir[i]->vdir, p))
						break;
				}
				if (i < scfg.total_dirs) {
					curdir = i;
					success = TRUE;
				} else
					success = FALSE;
			}

			if (success)
				sockprintf(sock, sess, "250 CWD command successful.");
			else {
				sockprintf(sock, sess, "550 %s: No such directory.", p);
				curlib = orglib;
				curdir = orgdir;
			}
			continue;
		}

		if (!stricmp(cmd, "PWD") || !stricmp(cmd, "XPWD")) {
			if (curlib < 0)
				sockprintf(sock, sess, "257 \"/\" is current directory.");
			else if (curdir < 0)
				sockprintf(sock, sess, "257 \"/%s\" is current directory."
				           , scfg.lib[curlib]->vdir);
			else
				sockprintf(sock, sess, "257 \"/%s/%s\" is current directory."
				           , scfg.lib[curlib]->vdir
				           , scfg.dir[curdir]->vdir);
			continue;
		}
		bool mkdir_cmd = (strnicmp(cmd, "MKD ", 4) == 0 || strnicmp(cmd, "XMKD", 4) == 0);
		if (mkdir_cmd && !(user.rest & (FLAG('G') | FLAG('U')))) {
			p = cmd + 4;
			SKIP_WHITESPACE(p);
			sockprintf(sock, sess, "257 \"%s\" directory created (not really)", p);
			lprintf(LOG_DEBUG, "%04d <%s> requested to create directory: %s", sock, user.alias, p);
			continue;
		}
		if (mkdir_cmd ||
		    !strnicmp(cmd, "SITE EXEC", 9)) {
			lprintf(LOG_WARNING, "%04d <%s> !SUSPECTED HACK ATTEMPT: %s"
			        , sock, user.alias, cmd);
			ftp_hacklog("FTP", user.alias, cmd, host_name, &ftp.client_addr);
		}
		// TODO: STAT is mandatory
		sockprintf(sock, sess, "500 Syntax error: '%s'", cmd);
		lprintf(LOG_WARNING, "%04d <%s> !UNSUPPORTED COMMAND: %s"
		        , sock, user.alias, cmd);
	} /* while(1) */

#if defined(SOCKET_DEBUG_TERMINATE)
	socket_debug[sock] |= SOCKET_DEBUG_TERMINATE;
#endif

	if (transfer_inprogress == TRUE) {
		lprintf(LOG_DEBUG, "%04d Waiting for transfer to complete...", sock);
		count = 0;
		while (transfer_inprogress == TRUE) {
			if (ftp_set == NULL) {
				mswait(2000);   /* allow xfer threads to terminate */
				break;
			}
			if (!transfer_aborted) {
				if (gettimeleft(&scfg, &user, logintime) < 1) {
					lprintf(LOG_WARNING, "%04d Out of time, disconnecting", sock);
					sockprintf(sock, sess, "421 Sorry, you've run out of time.");
					ftp_close_socket(&data_sock, &data_sess, __LINE__);
					transfer_aborted = TRUE;
				}
				if ((time(NULL) - lastactive) > startup->max_inactivity) {
					lprintf(LOG_WARNING, "%04d Disconnecting due to to inactivity", sock);
					sockprintf(sock, sess, "421 Disconnecting due to inactivity (%u seconds)."
					           , startup->max_inactivity);
					ftp_close_socket(&data_sock, &data_sess, __LINE__);
					transfer_aborted = TRUE;
				}
			}
			if (count && (count % 60) == 0)
				lprintf(LOG_WARNING, "%04d Still waiting (%us) for transfer to complete "
				        "(aborted=%d, lastactive=%" PRId64 "s, max_inactivity=%us) ..."
				        , sock, count, transfer_aborted, (uint64_t)(time(NULL)-lastactive)
						, startup->max_inactivity);
			count++;
			mswait(1000);
		}
		lprintf(LOG_DEBUG, "%04d Done waiting for transfer to complete", sock);
	}

	fmutex_close(&mutex_file);
	if (user.number) {
		/* Update User Statistics */
		if (chk_ars(&scfg, startup->login_info_save, &user, &client)) {
			if ((i = logoutuserdat(&scfg, &user, logintime)) != USER_SUCCESS)
				lprintf(LOG_ERR, "%04d <%s> !ERROR %d in logoutuserdat", sock, user.alias, i);
		}
		mqtt_user_logout(&mqtt, &client, logintime);
		lprintf(LOG_INFO, "%04d <%s> logged-out", sock, user.alias);
#ifdef _WIN32
		if (startup->sound.logout[0] && !sound_muted(&scfg))
			PlaySound(startup->sound.logout, NULL, SND_ASYNC | SND_FILENAME);
#endif

	}

#ifdef _WIN32
	if (startup->sound.hangup[0] && !sound_muted(&scfg))
		PlaySound(startup->sound.hangup, NULL, SND_ASYNC | SND_FILENAME);
#endif

	if (pasv_sock != INVALID_SOCKET)
		ftp_close_socket(&pasv_sock, &pasv_sess, __LINE__);
	if (data_sock != INVALID_SOCKET)
		ftp_close_socket(&data_sock, &data_sess, __LINE__);

	client_off(sock);

#ifdef SOCKET_DEBUG_CTRL
	socket_debug[sock] &= ~SOCKET_DEBUG_CTRL;
#endif

#if defined(SOCKET_DEBUG_TERMINATE)
	socket_debug[sock] &= ~SOCKET_DEBUG_TERMINATE;
#endif

	tmp_sock = sock;
	ftp_close_socket(&tmp_sock, &sess, __LINE__);

	{
		int32_t clients = protected_uint32_adjust_fetch(&active_clients, -1);
		int32_t threads = thread_down();
		update_clients();

		lprintf(LOG_INFO, "%04d [%s] Session thread terminated (%d clients and %d threads remain, %lu served)"
		        , sock, host_ip, clients, threads, served);
	}
}

static void cleanup(int code, int line)
{
#ifdef _DEBUG
	lprintf(LOG_DEBUG, "0000 cleanup called from line %d", line);
#endif

	if (protected_uint32_value(thread_count) > 1) {
		lprintf(LOG_INFO, "0000 Waiting for %d child threads to terminate", protected_uint32_value(thread_count) - 1);
		while (protected_uint32_value(thread_count) > 1) {
			mswait(100);
		}
		lprintf(LOG_INFO, "0000 Done waiting for child threads to terminate");
	}

	free_cfg(&scfg);
	free_text(text);

	semfile_list_free(&pause_semfiles);
	semfile_list_free(&recycle_semfiles);
	semfile_list_free(&shutdown_semfiles);

	if (ftp_set != NULL) {
		xpms_destroy(ftp_set, ftp_close_socket_cb, NULL);
		ftp_set = NULL;
	}

	update_clients();   /* active_clients is destroyed below */

	listFree(&current_connections);

	if (protected_uint32_value(active_clients))
		lprintf(LOG_WARNING, "!!!! Terminating with %d active clients", protected_uint32_value(active_clients));
	else
		protected_uint32_destroy(active_clients);

#ifdef _WINSOCKAPI_
	if (WSAInitialized && WSACleanup() != 0)
		lprintf(LOG_ERR, "0000 !WSACleanup ERROR %d", SOCKET_ERRNO);
#endif

	thread_down();
	if (terminate_server || code) {
		lprintf(LOG_INFO, "#### FTP Server thread terminated (%lu clients served, %u concurrently, denied: %u due to rate limit, %u due to IP address, %u due to hostname)"
		        , served, client_highwater, request_rate_limiter->disallowed.load(), ip_can->total_found.load() + ip_silent_can->total_found.load(), host_can->total_found.load());
		set_state(SERVER_STOPPED);
		if (startup != NULL && startup->terminated != NULL)
			startup->terminated(startup->cbdata, code);
	}

	delete request_rate_limiter, request_rate_limiter = nullptr;
	delete ip_can, ip_can = nullptr;
	delete ip_silent_can, ip_silent_can = nullptr;
	delete host_can, host_can = nullptr;

	mqtt_shutdown(&mqtt);
}

const char* ftp_ver(void)
{
	static char ver[256];
	char        compiler[32];

	DESCRIBE_COMPILER(compiler);

	safe_snprintf(ver, sizeof(ver), "%s %s%c%s  "
	              "Compiled %s/%s %s with %s"
	              , FTP_SERVER
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

void ftp_server(void* arg)
{
	char*             p;
	char              path[MAX_PATH + 1];
	char              error[256];
	char              compiler[32];
	char              str[256];
	union xp_sockaddr client_addr;
	socklen_t         client_addr_len;
	SOCKET            client_socket;
	int               i;
	time_t            t;
	time_t            start;
	time_t            initialized = 0;
	ftp_t*            ftp;
	char              client_ip[INET6_ADDRSTRLEN];
	CRYPT_SESSION     none = -1;

	startup = (ftp_startup_t*)arg;
	SetThreadName("sbbs/ftpServer");

#ifdef _THREAD_SUID_BROKEN
	if (thread_suid_broken)
		startup->seteuid(TRUE);
#endif

	if (startup == NULL) {
		sbbs_beep(100, 500);
		fprintf(stderr, "No startup structure passed!\n");
		return;
	}

	if (startup->size != sizeof(ftp_startup_t)) {  /* verify size */
		sbbs_beep(100, 500);
		sbbs_beep(300, 500);
		sbbs_beep(100, 500);
		fprintf(stderr, "Invalid startup structure!\n");
		return;
	}
	set_state(SERVER_INIT);

	uptime = 0;
	served = 0;
	startup->recycle_now = FALSE;
	startup->shutdown_now = FALSE;
	terminate_server = FALSE;
	protected_uint32_init(&thread_count, 0);

	do {
		listInit(&current_connections, LINK_LIST_MUTEX);
		protected_uint32_init(&active_clients, 0);

		/* Setup intelligent defaults */
		if (startup->port == 0)
			startup->port = IPPORT_FTP;
		if (startup->qwk_timeout == 0)
			startup->qwk_timeout = FTP_DEFAULT_QWK_TIMEOUT;                                         /* seconds */
		if (startup->max_inactivity == 0)
			startup->max_inactivity = FTP_DEFAULT_MAX_INACTIVITY;                                        /* seconds */
		if (startup->sem_chk_freq == 0)
			startup->sem_chk_freq = DEFAULT_SEM_CHK_FREQ;                                        /* seconds */
		if (startup->index_file_name[0] == 0)
			SAFECOPY(startup->index_file_name, "00index");

		(void)protected_uint32_adjust(&thread_count, 1);
		thread_up(FALSE /* setuid */);

		memset(&scfg, 0, sizeof(scfg));

		lprintf(LOG_INFO, "Synchronet FTP Server Version %s%c%s"
		        , VERSION, REVISION
#ifdef _DEBUG
		        , " Debug"
#else
		        , ""
#endif
		        );

		DESCRIBE_COMPILER(compiler);

		lprintf(LOG_INFO, "Compiled %s/%s %s with %s", GIT_BRANCH, GIT_HASH, GIT_DATE, compiler);

		sbbs_srand();   /* Seed random number generator */

		if (!winsock_startup()) {
			cleanup(1, __LINE__);
			break;
		}

		t = time(NULL);
		lprintf(LOG_INFO, "Initializing on %.24s with options: %x"
		        , ctime_r(&t, str), startup->options);

		if (chdir(startup->ctrl_dir) != 0)
			lprintf(LOG_ERR, "!ERROR %d (%s) changing directory to: %s"
			        , errno, safe_strerror(errno, error, sizeof error), startup->ctrl_dir);

		/* Initial configuration and load from CNF files */
		SAFECOPY(scfg.ctrl_dir, startup->ctrl_dir);
		lprintf(LOG_INFO, "Loading configuration files from %s", scfg.ctrl_dir);
		scfg.size = sizeof(scfg);
		SAFECOPY(error, UNKNOWN_LOAD_ERROR);
		if (!load_cfg(&scfg, text, TOTAL_TEXT, /* prep: */ TRUE, /* node: */ FALSE, error, sizeof(error))) {
			lprintf(LOG_CRIT, "!ERROR loading configuration files: %s", error);
			cleanup(1, __LINE__);
			break;
		}
		if (error[0] != '\0')
			lprintf(LOG_WARNING, "!WARNING loading configuration files: %s", error);

		mqtt_startup(&mqtt, &scfg, (struct startup*)startup, ftp_ver(), lputs);

		if ((t = checktime()) != 0) {   /* Check binary time */
			lprintf(LOG_ERR, "!TIME PROBLEM (%" PRId64 ")", (int64_t)t);
		}

		if (uptime == 0)
			uptime = time(NULL); /* this must be done *after* setting the timezone */

		if (startup->temp_dir[0])
			SAFECOPY(scfg.temp_dir, startup->temp_dir);
		else
			SAFECOPY(scfg.temp_dir, "../temp");
		prep_dir(scfg.ctrl_dir, scfg.temp_dir, sizeof(scfg.temp_dir));
		if ((i = md(scfg.temp_dir)) != 0) {
			lprintf(LOG_CRIT, "!ERROR %d (%s) creating directory: %s", i, strerror(i), scfg.temp_dir);
			cleanup(1, __LINE__);
			break;
		}
		lprintf(LOG_DEBUG, "Temporary file directory: %s", scfg.temp_dir);

		if (!startup->max_clients) {
			startup->max_clients = scfg.sys_nodes;
			if (startup->max_clients < 10)
				startup->max_clients = 10;
		}
		lprintf(LOG_DEBUG, "Maximum clients: %d", startup->max_clients);

		/* Sanity-check the passive port range */
		if (startup->pasv_port_low || startup->pasv_port_high) {
			if (startup->pasv_port_low > startup->pasv_port_high
			    || startup->pasv_port_high - startup->pasv_port_low < (startup->max_clients - 1)) {
				lprintf(LOG_WARNING, "!Correcting Passive Port Range (Low: %u, High: %u)"
				        , startup->pasv_port_low, startup->pasv_port_high);
				if (startup->pasv_port_low)
					startup->pasv_port_high = startup->pasv_port_low + (startup->max_clients - 1);
				else
					startup->pasv_port_low = startup->pasv_port_high - (startup->max_clients - 1);
			}
			lprintf(LOG_DEBUG, "Passive Port Low: %u", startup->pasv_port_low);
			lprintf(LOG_DEBUG, "Passive Port High: %u", startup->pasv_port_high);
		}

		lprintf(LOG_DEBUG, "Maximum inactivity: %u seconds", startup->max_inactivity);

		update_clients();

		/* open a socket and wait for a client */
		ftp_set = xpms_create(startup->bind_retry_count, startup->bind_retry_delay, lprintf);

		if (ftp_set == NULL) {
			lprintf(LOG_CRIT, "!ERROR %d creating FTP socket set", SOCKET_ERRNO);
			cleanup(1, __LINE__);
			return;
		}
		lprintf(LOG_DEBUG, "FTP Server socket set created");

		/*
		 * Add interfaces
		 */
		xpms_add_list(ftp_set, PF_UNSPEC, SOCK_STREAM, 0, startup->interfaces, startup->port, "FTP Server", &terminate_server, ftp_open_socket_cb, startup->seteuid, NULL);

		request_rate_limiter = new rateLimiter(startup->max_requests_per_period, startup->request_rate_limit_period);
		ip_can = new trashCan(&scfg, "ip", startup->sem_chk_freq);
		ip_silent_can = new trashCan(&scfg, "ip-silent", startup->sem_chk_freq);
		host_can = new trashCan(&scfg, "host", startup->sem_chk_freq);

		/* Setup recycle/shutdown semaphore file lists */
		shutdown_semfiles = semfile_list_init(scfg.ctrl_dir, "shutdown", server_abbrev);
		pause_semfiles = semfile_list_init(scfg.ctrl_dir, "pause", server_abbrev);
		recycle_semfiles = semfile_list_init(scfg.ctrl_dir, "recycle", server_abbrev);
		semfile_list_add(&recycle_semfiles, startup->ini_fname);
		SAFEPRINTF(path, "%sftpsrvr.rec", scfg.ctrl_dir); /* legacy */
		semfile_list_add(&recycle_semfiles, path);
		if (!initialized) {
			semfile_list_check(&initialized, recycle_semfiles);
			semfile_list_check(&initialized, shutdown_semfiles);
		}

		lprintf(LOG_INFO, "FTP Server thread started");
		mqtt_client_max(&mqtt, startup->max_clients);

		while (ftp_set != NULL && !terminate_server) {
			YIELD();
			if (!(startup->options & FTP_OPT_NO_RECYCLE) && protected_uint32_value(thread_count) <= 1) {
				if ((p = semfile_list_check(&initialized, recycle_semfiles)) != NULL) {
					lprintf(LOG_INFO, "0000 Recycle semaphore file (%s) detected", p);
					break;
				}
				if (startup->recycle_now == TRUE) {
					lprintf(LOG_NOTICE, "0000 Recycle semaphore signaled");
					startup->recycle_now = FALSE;
					break;
				}
			}
			if (((p = semfile_list_check(&initialized, shutdown_semfiles)) != NULL
			     && lprintf(LOG_INFO, "0000 Shutdown semaphore file (%s) detected", p))
			    || (startup->shutdown_now == TRUE
			        && lprintf(LOG_INFO, "0000 Shutdown semaphore signaled"))) {
				startup->shutdown_now = FALSE;
				terminate_server = TRUE;
				break;
			}
			if (((p = semfile_list_check(NULL, pause_semfiles)) != NULL
			     && lprintf(LOG_INFO, "0000 Pause semaphore file (%s) detected", p))
			    || (startup->paused
			        && lprintf(LOG_INFO, "0000 Pause semaphore signaled"))) {
				set_state(SERVER_PAUSED);
				SLEEP(startup->sem_chk_freq * 1000);
				continue;
			}
			/* signal caller that we've started up successfully */
			set_state(SERVER_READY);


			if (ftp_set == NULL || terminate_server)   /* terminated */
				break;

			/* now wait for connection */
			client_addr_len = sizeof(client_addr);
			client_socket = xpms_accept(ftp_set, &client_addr, &client_addr_len, startup->sem_chk_freq * 1000, XPMS_FLAGS_NONE, NULL);

			if (client_socket == INVALID_SOCKET)
				continue;

			if (startup->socket_open != NULL)
				startup->socket_open(startup->cbdata, TRUE);

			inet_addrtop(&client_addr, client_ip, sizeof(client_ip));

			if (startup->max_concurrent_connections > 0) {
				int  ip_len = strlen(client_ip) + 1;
				uint connections = listCountMatches(&current_connections, client_ip, ip_len);
				if (connections >= startup->max_concurrent_connections
				    && !is_host_exempt(&scfg, client_ip, /* host_name */ NULL)) {
					lprintf(LOG_NOTICE, "%04d [%s] !Maximum concurrent connections (%u) exceeded"
					        , client_socket, client_ip, startup->max_concurrent_connections);
					sockprintf(client_socket, -1, "421 Maximum connections (%u) exceeded", startup->max_concurrent_connections);
					ftp_close_socket(&client_socket, &none, __LINE__);
					continue;
				}
			}

			if (ip_silent_can->listed(client_ip)) {
				ftp_close_socket(&client_socket, &none, __LINE__);
				continue;
			}

			if (protected_uint32_value(active_clients) >= startup->max_clients) {
				lprintf(LOG_WARNING, "%04d [%s] !MAXIMUM CLIENTS (%d) reached, access denied"
				        , client_socket, client_ip, startup->max_clients);
				sockprintf(client_socket, -1, "421 Maximum active clients reached, please try again later.");
				ftp_close_socket(&client_socket, &none, __LINE__);
				continue;
			}

			if ((ftp = static_cast<ftp_t*>(malloc(sizeof(ftp_t)))) == NULL) {
				lprintf(LOG_CRIT, "%04d !ERROR allocating %d bytes of memory for ftp_t"
				        , client_socket, (int)sizeof(ftp_t));
				sockprintf(client_socket, -1, "421 System error, please try again later.");
				ftp_close_socket(&client_socket, &none, __LINE__);
				continue;
			}

			ftp->socket = client_socket;
			memcpy(&ftp->client_addr, &client_addr, client_addr_len);
			ftp->client_addr_len = client_addr_len;

			(void)protected_uint32_adjust(&thread_count, 1);
			_beginthread(ctrl_thread, 0, ftp);
			served++;
		}

		set_state(terminate_server ? SERVER_STOPPING : SERVER_RELOADING);

#if 0 /* def _DEBUG */
		lprintf(LOG_DEBUG, "0000 terminate_server: %d", terminate_server);
#endif
		if (protected_uint32_value(active_clients)) {
			lprintf(LOG_INFO, "0000 Waiting for %d active clients to disconnect..."
			        , protected_uint32_value(active_clients));
			start = time(NULL);
			while (protected_uint32_value(active_clients)) {
				if (time(NULL) - start > startup->max_inactivity * 2) {
					lprintf(LOG_WARNING, "0000 !TIMEOUT waiting for %d active clients"
					        , protected_uint32_value(active_clients));
					break;
				}
				mswait(100);
			}
			lprintf(LOG_INFO, "0000 Done waiting for active clients to disconnect");
		}

		if (!terminate_server) {
			lprintf(LOG_INFO, "Recycling server...");
			if (startup->recycle != NULL)
				startup->recycle(startup->cbdata);
		}
		cleanup(0, __LINE__);

	} while (!terminate_server);

	protected_uint32_destroy(thread_count);
}
