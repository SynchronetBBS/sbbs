/* Copyright (C), 2007 by Stephen Hurd */

#include <stdlib.h>

#include "ciolib.h"
#include "gen_defs.h"
#include "genwrap.h"
#include "sockwrap.h"
#include "threadwrap.h"

#ifdef _WIN32
 #undef socklen_t

// Borland hack (broken header)
 #ifdef __BORLANDC__
  #define _MSC_VER 1
 #endif
 #include "ws2tcpip.h"
 #ifdef __BORLANDC__
  #undef _MSC_VER
 #endif
 #ifndef AI_ADDRCONFIG
  #define AI_ADDRCONFIG 0x0400 // Vista or later.
 #endif
 #ifndef AI_NUMERICSERV
  #define AI_NUMERICSERV 0     // No supported by Windows
 #endif
#endif /* ifdef _WIN32 */

#include "bbslist.h"
#include "conn.h"
#include "conn_mqtt.h"
#include "host_ui.h"
#include "raw.h"
#include "rlogin.h"
#include "syncterm.h"
#include "term.h"
#include "wren_host.h"
#ifndef WITHOUT_DEUCESSH
 #include "ssh.h"
#endif
#ifndef WITHOUT_CRYPTO
 #include "telnets.h"
#endif
#ifndef __HAIKU__
 #include "modem.h"
#endif
#ifdef __unix__
 #include "conn_pty.h"
#endif
#ifdef _WIN32
 #include "conn_conpty.h"
#endif
#include "conn_telnet.h"
#include "telnet_io.h"	/* telnet_send_window_change */

#ifdef _MSC_VER
#pragma warning(disable : 4244 4267 4018)
#endif

struct conn_api    conn_api;
char              *conn_types_enum[] = {
	"Unknown", "RLogin", "RLoginReversed", "Telnet", "Raw", "SSH", "SSHNA", "Modem", "Serial", "NoRTS", "Shell",
	"MBBSGhost", "TelnetS", "MQTT", NULL
};
char              *conn_types[] = {
	"Unknown", "RLogin", "RLogin Reversed", "Telnet", "Raw", "SSH", "SSH (no auth)", "Modem", "Serial",
	"3-wire (No RTS)", "Shell", "MBBS GHost", "TelnetS", "SBBS MQTT Spy", NULL
};
short unsigned int conn_ports[] = {0, 513, 513, 23, 0, 22, 22, 0, 0, 0, 0, 65535, 992, 8883, 0};

struct conn_buffer conn_inbuf;
struct conn_buffer conn_outbuf;

/*
 * Connection functions
 */
bool
conn_connected(void)
{
	if ((conn_api.input_thread_running == 1) && (conn_api.output_thread_running == 1))
		return true;
	return false;
}

int64_t
conn_connected_seconds(void)
{
	if (conn_api.connected_at <= 0)
		return 0;
	int64_t elapsed = xp_fast_timer64() - conn_api.connected_at;
	return elapsed > 0 ? elapsed : 0;
}

int
conn_recv_upto(void *vbuffer, size_t buflen, unsigned timeout)
{
	char  *buffer = (char *)vbuffer;
	size_t found = 0;
	size_t obuflen;
	void  *expanded;
	size_t max_rx = buflen;

	if (conn_api.rx_parse_cb != NULL) {
		if (max_rx > 1)
			max_rx /= 2;
	}
	assert_pthread_mutex_lock(&(conn_inbuf.read_mutex));
	if (conn_buf_wait_bytes(&conn_inbuf, 1, timeout))
		found = conn_buf_get(&conn_inbuf, buffer, max_rx);
	assert_pthread_mutex_unlock(&(conn_inbuf.read_mutex));

	if (found) {
		if (conn_api.rx_parse_cb != NULL) {
			expanded = conn_api.rx_parse_cb(buffer, found, &obuflen);
			memcpy(vbuffer, expanded, obuflen);
			free(expanded);
			found = obuflen;
		}
		else {
			expanded = buffer;
			obuflen = buflen;
		}
	}

	return found;
}

int
conn_send_raw(const void *vbuffer, size_t buflen, unsigned int timeout)
{
	const char *buffer = vbuffer;
	size_t      found;

	assert_pthread_mutex_lock(&(conn_outbuf.write_mutex));
	found = conn_buf_wait_free(&conn_outbuf, buflen, timeout);
	if (found)
		found = conn_buf_put(&conn_outbuf, buffer, found);
	assert_pthread_mutex_unlock(&(conn_outbuf.write_mutex));
	return found;
}

int
conn_send(const void *vbuffer, size_t buflen, unsigned int timeout)
{
	const char *buffer = vbuffer;
	size_t      found;
	size_t      obuflen;
	void       *expanded;

	if (conn_api.tx_parse_cb != NULL) {
		expanded = conn_api.tx_parse_cb(buffer, buflen, &obuflen);
	}
	else {
		expanded = (void *)buffer;
		obuflen = buflen;
	}

	assert_pthread_mutex_lock(&(conn_outbuf.write_mutex));
	found = conn_buf_wait_free(&conn_outbuf, obuflen, timeout);
	if (found)
		found = conn_buf_put(&conn_outbuf, expanded, found);
	assert_pthread_mutex_unlock(&(conn_outbuf.write_mutex));

	if (conn_api.tx_parse_cb != NULL)
		free(expanded);

	return found;
}

bool
conn_connect(struct bbslist *bbs)
{
	char str[64];

	memset(&conn_api, 0, sizeof(conn_api));

	conn_api.nostatus = bbs->nostatus;
	conn_api.emulation = get_emulation(bbs);
	switch (bbs->conn_type) {
		case CONN_TYPE_RLOGIN:
		case CONN_TYPE_RLOGIN_REVERSED:
			conn_api.connect = rlogin_connect;
			conn_api.close = rlogin_close;
			conn_api.send_window_change = rlogin_send_window_change;
			break;
		case CONN_TYPE_TELNET:
			conn_api.connect = telnet_connect;
			conn_api.close = telnet_close;
			conn_api.binary_mode_on = telnet_binary_mode_on;
			conn_api.binary_mode_off = telnet_binary_mode_off;
			conn_api.send_window_change = telnet_send_window_change;
			break;
		case CONN_TYPE_RAW:
		case CONN_TYPE_MBBS_GHOST:
			conn_api.connect = raw_connect;
			conn_api.close = raw_close;
			break;
#ifndef WITHOUT_CRYPTO
		case CONN_TYPE_TELNETS:
			conn_api.connect = telnets_connect;
			conn_api.close = telnets_close;
			conn_api.binary_mode_on = telnet_binary_mode_on;
			conn_api.binary_mode_off = telnet_binary_mode_off;
			conn_api.send_window_change = telnet_send_window_change;
			break;
		case CONN_TYPE_MQTT:
			conn_api.connect = mqtt_connect;
			conn_api.close = mqtt_close;
			break;
#endif
#ifndef WITHOUT_DEUCESSH
		case CONN_TYPE_SSHNA:
		case CONN_TYPE_SSH:
			conn_api.connect = ssh_connect;
			conn_api.close = ssh_close;
			conn_api.send_window_change = ssh_send_window_change;
			break;
#endif
#ifndef __HAIKU__
		case CONN_TYPE_SERIAL:
		case CONN_TYPE_SERIAL_NORTS:
			conn_api.connect = modem_connect;
			conn_api.close = serial_close;
			break;
		case CONN_TYPE_MODEM:
			conn_api.connect = modem_connect;
			conn_api.close = modem_close;
			break;
#endif
#ifdef __unix__
		case CONN_TYPE_SHELL:
			conn_api.connect = pty_connect;
			conn_api.close = pty_close;
			conn_api.send_window_change = pty_send_window_change;
			break;
#endif
#ifdef HAS_CONPTY
		case CONN_TYPE_SHELL:
			conn_api.connect = conpty_connect;
			conn_api.close = conpty_close;
			conn_api.send_window_change = conpty_send_window_change;
			break;
#endif
		default:
			sprintf(str, "%s connections not supported.", conn_types[bbs->conn_type]);
			host_ui_alert(str,
			    "The connection type of this entry is not supported by this build.\n"
			    "Either the protocol was disabled at compile time, or is\n"
			    "unsupported on this platform.");
			conn_api.terminate = true;
	}
	if (conn_api.connect) {
		if (conn_api.connect(bbs)) {
			conn_api.terminate = true;
			while (conn_api.input_thread_running == 1 || conn_api.output_thread_running == 1)
				SLEEP(1);
		}
		else {
			while ((!conn_api.terminate)
			    && (conn_api.input_thread_running == 0 || conn_api.output_thread_running == 0))
				SLEEP(1);
			if (!conn_api.terminate)
				conn_api.connected_at = xp_fast_timer64();
		}
	}
	return conn_api.terminate;
}

int
conn_close(void)
{
	if (conn_api.close)
		return conn_api.close();
	return 0;
}

enum failure_reason {
	FAILURE_WHAT_FAILURE
	,
	FAILURE_RESOLVE
	,
	FAILURE_CANT_CREATE
	,
	FAILURE_CONNECT_ERROR
	,
	FAILURE_ABORTED
	,
	FAILURE_DISCONNECTED
};

SOCKET
conn_socket_connect(struct bbslist *bbs, bool can_cancel)
{
	SOCKET           sock = INVALID_SOCKET;
#ifdef _WIN32
	u_long           nonblock;
#else
	int              nonblock;
#endif
	int              failcode = FAILURE_WHAT_FAILURE;
	struct addrinfo  hints;
	struct addrinfo *res = NULL;
	struct addrinfo *cur;
	char             portnum[6];
	char             str[LIST_ADDR_MAX + 40];

	if (!bbs->hidepopups)
		host_ui_status("Looking up host");
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = PF_UNSPEC;
	switch (bbs->address_family) {
		case ADDRESS_FAMILY_INET:
			hints.ai_family = PF_INET;
			break;
		case ADDRESS_FAMILY_INET6:
			hints.ai_family = PF_INET6;
			break;
		case ADDRESS_FAMILY_UNSPEC:
		default:
			hints.ai_family = PF_UNSPEC;
			break;
	}
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_NUMERICSERV;
#ifdef AI_ADDRCONFIG
	hints.ai_flags |= AI_ADDRCONFIG;
#endif
	sprintf(portnum, "%hu", bbs->port);
	if (getaddrinfo(bbs->addr, portnum, &hints, &res) != 0) {
		failcode = FAILURE_RESOLVE;
		res = NULL;
	}
	if (!bbs->hidepopups) {
		host_ui_status(NULL);
		host_ui_status("Connecting...");
	}

	if (can_cancel) {
		/* Drain the input buffer to avoid accidental cancel */
		while (kbhit()) {
			int ch = syncterm_getkey();
			if (ch == CIO_KEY_MOUSE)
				getmouse(NULL);
			if (quitting)
				break;
		}
		if (quitting)
			failcode = FAILURE_ABORTED;
	}

	for (cur = res; cur && sock == INVALID_SOCKET && failcode == FAILURE_WHAT_FAILURE; cur = cur->ai_next) {
		if (sock == INVALID_SOCKET) {
			sock = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
			if (sock == INVALID_SOCKET) {
				failcode = FAILURE_CANT_CREATE;
				break;
			}

                        /* Set to non-blocking for the connect */
			nonblock = -1;
			ioctlsocket(sock, FIONBIO, &nonblock);
		}

		if (connect(sock, cur->ai_addr, cur->ai_addrlen)) {
			int tries = 0;
			switch (ERROR_VALUE) {
				case EINPROGRESS:
				case EINTR:
				case EAGAIN:
#if (EAGAIN != EWOULDBLOCK)
				case EWOULDBLOCK:
#endif
					for (; sock != INVALID_SOCKET;) {
						if (socket_writable(sock, 1000)) {
							tries++;
							if (tries >= 5 && !can_cancel)  {
								closesocket(sock);
								sock = INVALID_SOCKET;
								continue;
							}
							else if (socket_recvdone(sock, 0)) {
								closesocket(sock);
								sock = INVALID_SOCKET;
								continue;
							}
							else {
								goto connected;
							}
						}
						else {
							if (can_cancel) {
								if (kbhit()) {
									int ch = syncterm_getkey();
									if (ch == CIO_KEY_MOUSE)
										getmouse(NULL);
									failcode = FAILURE_ABORTED;
									closesocket(sock);
									sock = INVALID_SOCKET;
								}
							}
						}
					}

connected:
					break;
				default:
					closesocket(sock);
					sock = INVALID_SOCKET;
					continue;
			}
		}
	}
	if (sock != INVALID_SOCKET) {
		freeaddrinfo(res);
		res = NULL;
		nonblock = 0;
		ioctlsocket(sock, FIONBIO, &nonblock);
		if (!socket_recvdone(sock, 0)) {
			int keepalives = true;
			if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalives, sizeof(keepalives)))
				fprintf(stderr, "%s:%d: Error %d calling setsockopt()\n", __FILE__, __LINE__, errno);

			/* Pin both kernel socket buffers to 1 MiB for consistent
			 * throughput across platforms.  Windows defaults to ~8 KiB
			 * for both directions and does not auto-tune the receive
			 * buffer the way Linux does, so a Windows-side telnet (or
			 * any TCP) download caps at roughly RCVBUF/RTT — about
			 * 400 KiB/s on a 20 ms link with the stock 8 KiB receive
			 * window — regardless of how fast the server streams.  1
			 * MiB covers the BDP of a 500 Mbps × 16 ms link with
			 * headroom and is small enough that the per-connection
			 * memory cost is negligible.
			 *
			 * SSH overrides SO_SNDBUF dynamically via
			 * ssh_set_sftp_buffer_mode() (drops to 64 KiB while
			 * CTerm.sftpActive is set so a saturated SFTP transfer
			 * can't queue keystroke-blocking bulk data), so the value
			 * set here is the baseline that the toggle restores to. */
			int sockbuf = 1024 * 1024;
			if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&sockbuf, sizeof(sockbuf)))
				fprintf(stderr, "%s:%d: Error %d calling setsockopt(SO_SNDBUF)\n",
				    __FILE__, __LINE__, errno);
			if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&sockbuf, sizeof(sockbuf)))
				fprintf(stderr, "%s:%d: Error %d calling setsockopt(SO_RCVBUF)\n",
				    __FILE__, __LINE__, errno);

			if (!bbs->hidepopups)
				host_ui_status(NULL);
			return sock;
		}
		failcode = FAILURE_DISCONNECTED;
	}
	if (failcode == FAILURE_WHAT_FAILURE)
		failcode = FAILURE_CONNECT_ERROR;

	if (res)
		freeaddrinfo(res);
	if (!bbs->hidepopups)
		host_ui_status(NULL);
	conn_api.terminate = true;
	if (!bbs->hidepopups && !quitting) {
		switch (failcode) {
			case FAILURE_RESOLVE:
				sprintf(str, "Cannot resolve %s!", bbs->addr);
				host_ui_alert(str,
				    "The system is unable to resolve the hostname. Check its spelling\n"
				    "and the DNS configuration for this system and the remote host.");
				break;
			case FAILURE_CANT_CREATE:
				sprintf(str, "Cannot create socket (%d)!", ERROR_VALUE);
				host_ui_alert(str,
				    "Your system is either dangerously low on resources, or there\n"
				    "is a problem with your TCP/IP stack.");
				break;
			case FAILURE_CONNECT_ERROR:
				sprintf(str, "Connect error (%d)!", ERROR_VALUE);
				host_ui_alert(str,
				    "The call to connect() returned an unexpected error code.");
				break;
			case FAILURE_ABORTED:
				host_ui_alert("Connection Aborted",
				    "Connection to the remote system aborted by keystroke.");
				break;
			case FAILURE_DISCONNECTED:
				sprintf(str, "Connect error (%d)!", ERROR_VALUE);
				host_ui_alert(str,
				    "After connect() succeeded, the socket was in a disconnected state.");
				break;
		}
	}
	conn_close();
	if (sock != INVALID_SOCKET)
		closesocket(sock);
	return INVALID_SOCKET;
}

void
conn_binary_mode_on(void)
{
	if (conn_api.binary_mode_on)
		conn_api.binary_mode_on();
	conn_api.binary_mode = true;
}

void
conn_binary_mode_off(void)
{
	if (conn_api.binary_mode_off)
		conn_api.binary_mode_off();
	conn_api.binary_mode = false;
}

void
conn_send_window_change(int text_cols, int text_rows,
    int pixel_cols, int pixel_rows)
{
	if (conn_api.send_window_change)
		conn_api.send_window_change(text_cols, text_rows,
		    pixel_cols, pixel_rows);
}
