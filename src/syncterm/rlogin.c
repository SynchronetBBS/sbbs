/* Copyright (C), 2007 by Stephen Hurd */

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bbslist.h"
#include "conn.h"
#include "sockwrap.h"
#include "term.h"	/* get_cterm_size */
#include "uifcinit.h"

SOCKET rlogin_sock = INVALID_SOCKET;

/* Per RFC 1282, the server sends a TCP-urgent 0x80 byte to request
 * that the client begin reporting window sizes.  Latches true on
 * first receipt and stays true for the session; client may then send
 * updates at any time. */
static bool rlogin_winsize_enabled = false;

/* True only for CONN_TYPE_RLOGIN / CONN_TYPE_RLOGIN_REVERSED.  The
 * input+output threads are shared with telnet / raw / mbbs_ghost via
 * rlogin_input_thread / rlogin_output_thread; those conn types must
 * not run any RFC 1282 OOB dispatch or DC1/DC3 flow control. */
static _Atomic bool rlogin_active = false;

/* Set when rlogin_tx_parse_cb sees a DC3 (XOFF) in outbound user
 * input while in cooked mode.  The input thread halts recv while
 * true.  Cleared by DC1 (XON), by a server 0x10 transition to raw,
 * by rlogin_binary_mode_on, and at connect/close. */
static _Atomic bool rlogin_input_paused = false;

/* Forward decl so rlogin_handle_control() can call the public API
 * entry point in the same file. */
void rlogin_send_window_change(int text_cols, int text_rows,
    int pixel_cols, int pixel_rows);

/* conn_api.binary_mode_on hook.  The conn_binary_mode_on() wrapper
 * sets conn_api.binary_mode = true after calling this, so all we
 * need to do here is release any pause the DC1/DC3 path may have
 * imposed.  No binary_mode_off hook is needed — the wrapper clears
 * the flag on its own and tx_parse_cb will resume DC1/DC3 scanning. */
void
rlogin_binary_mode_on(void)
{
	atomic_store(&rlogin_input_paused, false);
}

/* Outbound byte filter for rlogin in cooked mode.  conn_send() calls
 * this per chunk; returns a malloc'd buffer of bytes to enqueue to
 * conn_outbuf.  In cooked mode, DC1 (XON) and DC3 (XOFF) are
 * consumed locally as TTY flow control against server→client data
 * (toggling rlogin_input_paused) and never reach the wire.  In raw
 * mode — i.e. conn_api.binary_mode — the buffer is passed through
 * verbatim so binary transfers stay intact. */
void *
rlogin_tx_parse_cb(const void *inbuf, size_t inlen, size_t *olen)
{
	void *ret = malloc(inlen > 0 ? inlen : 1);

	if (ret == NULL) {
		*olen = 0;
		return ret;
	}

	if (conn_api.binary_mode) {
		memcpy(ret, inbuf, inlen);
		*olen = inlen;
		return ret;
	}

	const unsigned char *src = (const unsigned char *)inbuf;
	unsigned char       *dst = (unsigned char *)ret;
	size_t               out = 0;
	for (size_t i = 0; i < inlen; i++) {
		unsigned char c = src[i];
		if (c == 0x13) {	/* DC3 / XOFF */
			atomic_store(&rlogin_input_paused, true);
			continue;
		}
		if (c == 0x11) {	/* DC1 / XON  */
			atomic_store(&rlogin_input_paused, false);
			continue;
		}
		dst[out++] = c;
	}
	*olen = out;
	return ret;
}

/* Returns 1 when the next byte to read from sock is the OOB mark
 * byte, 0 otherwise (or on error).  Uses SIOCATMARK for portability
 * — sockatmark() isn't universally available. */
static int
rlogin_at_oob_mark(SOCKET sock)
{
#ifdef _WIN32
	u_long atmark = 0;
	if (ioctlsocket(sock, SIOCATMARK, &atmark) != 0)
		return 0;
	return atmark ? 1 : 0;
#else
	int atmark = 0;
	if (ioctl(sock, SIOCATMARK, &atmark) != 0)
		return 0;
	return atmark ? 1 : 0;
#endif
}

/* Dispatch for RFC 1282 OOB control bytes.  Only 0x80 is implemented;
 * the others are reserved stubs so new control bytes can be added
 * without touching the detection path in rlogin_input_thread. */
static void
rlogin_handle_control(uint8_t c)
{
	switch (c) {
		case 0x02:	/* TIOCFLUSH — server tells client to discard
				 * any output it has queued but not yet sent.
				 * RFC 1282: typically follows an interrupt at
				 * the server side, so the client's in-flight
				 * typeahead is stale.  Drop the outbound ring. */
			conn_buf_reset(&conn_outbuf);
			break;
		case 0x10:	/* server-initiated: enter raw mode.
				 * Usually a no-op for us (we default to raw
				 * at connect for SSH parity), but a server
				 * that previously asked for cooked via 0x20
				 * can flip us back.  Drop any pause imposed
				 * by a prior DC3 since raw does no local
				 * flow control. */
			conn_api.binary_mode = true;
			atomic_store(&rlogin_input_paused, false);
			break;
		case 0x20:	/* server-initiated: switch to cooked mode and
				 * have the client interpret DC1/DC3 locally. */
			conn_api.binary_mode = false;
			break;
		case 0x80: {	/* server requests window-size reports */
			bool was_enabled = rlogin_winsize_enabled;
			rlogin_winsize_enabled = true;
			if (!was_enabled) {
				int cols, rows;
				get_cterm_size(&cols, &rows, conn_api.nostatus);
				rlogin_send_window_change(cols, rows, -1, -1);
			}
			break;
		}
		default:
			break;
	}
}

/* Send a window-size message per RFC 1282:
 *   0xFF 0xFF 's' 's'  rows cols xpx ypx   (each 16-bit big-endian)
 * No-op until the server has requested reports with an OOB 0x80 byte. */
void
rlogin_send_window_change(int text_cols, int text_rows,
    int pixel_cols, int pixel_rows)
{
	if (!rlogin_winsize_enabled)
		return;
	if (text_cols <= 0 || text_rows <= 0)
		return;

	uint16_t cols = (uint16_t)text_cols;
	uint16_t rows = (uint16_t)text_rows;
	uint16_t xpx = (pixel_cols < 0) ? 0 : (uint16_t)pixel_cols;
	uint16_t ypx = (pixel_rows < 0) ? 0 : (uint16_t)pixel_rows;

	uint8_t buf[12];
	buf[0]  = 0xff;
	buf[1]  = 0xff;
	buf[2]  = 's';
	buf[3]  = 's';
	buf[4]  = (rows >> 8) & 0xff;
	buf[5]  = rows & 0xff;
	buf[6]  = (cols >> 8) & 0xff;
	buf[7]  = cols & 0xff;
	buf[8]  = (xpx >> 8) & 0xff;
	buf[9]  = xpx & 0xff;
	buf[10] = (ypx >> 8) & 0xff;
	buf[11] = ypx & 0xff;

	conn_send(buf, sizeof(buf), 1000);
}

#ifdef __BORLANDC__
 #pragma argsused
#endif

void
rlogin_input_thread(void *args)
{
	int    rd = 0;
	int    buffered;
	size_t bufsz = 0;

	SetThreadName("RLogin Input");
	conn_api.input_thread_running = 1;
	while (rlogin_sock != INVALID_SOCKET && !conn_api.terminate) {
		/* Cooked-mode TTY flow control: DC3 sets the pause flag,
		 * DC1 clears it.  Only stalls recv for actual rlogin
		 * sessions, and only while binary_mode (raw) is off. */
		if (atomic_load(&rlogin_active)
		    && !conn_api.binary_mode
		    && atomic_load(&rlogin_input_paused)) {
			SLEEP(10);
			continue;
		}
		bool data_avail;
		if (socket_check(rlogin_sock, &data_avail, NULL, bufsz ? 0 : 100)) {
			if (data_avail && bufsz < BUFFER_SIZE) {
				/* RFC 1282 OOB control bytes: if the socket's
				 * urgent-data mark is at the next byte, consume
				 * that single byte out-of-band and dispatch it
				 * rather than passing it to the terminal.  Only
				 * relevant for true rlogin sessions. */
				if (atomic_load(&rlogin_active)
				    && rlogin_at_oob_mark(rlogin_sock)) {
					uint8_t ctl;
					rd = recv(rlogin_sock, (char *)&ctl, 1, 0);
					if (rd <= 0)
						break;
					rlogin_handle_control(ctl);
				}
				else {
					rd = recv(rlogin_sock, conn_api.rd_buf + bufsz, conn_api.rd_buf_size - bufsz, 0);
					if (rd <= 0)
						break;
					bufsz += rd;
				}
			}
			if (bufsz) {
				assert_pthread_mutex_lock(&(conn_inbuf.mutex));
				conn_buf_wait_free(&conn_inbuf, 1, 1000);
				buffered = conn_buf_put(&conn_inbuf, conn_api.rd_buf, bufsz);
				memmove(conn_api.rd_buf, &conn_api.rd_buf[buffered], bufsz - buffered);
				bufsz -= buffered;
				assert_pthread_mutex_unlock(&(conn_inbuf.mutex));
			}
		}
		else
			break;
	}
	conn_api.terminate = true;
	shutdown(rlogin_sock, SHUT_RDWR);
	conn_api.input_thread_running = 2;
}

#ifdef __BORLANDC__
 #pragma argsused
#endif

void
rlogin_output_thread(void *args)
{
	size_t sent;
	size_t wr;
	ssize_t ret;

	SetThreadName("RLogin Output");
	conn_api.output_thread_running = 1;
	while (rlogin_sock != INVALID_SOCKET && !conn_api.terminate) {
		assert_pthread_mutex_lock(&(conn_outbuf.mutex));
		ret = 0;
		wr = conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if (wr) {
			wr = conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			assert_pthread_mutex_unlock(&(conn_outbuf.mutex));
			sent = 0;
			while (rlogin_sock != INVALID_SOCKET && sent < wr && !conn_api.terminate) {
				if (socket_writable(rlogin_sock, 100)) {
					// coverity[overflow:SUPPRESS]
					ret = sendsocket(rlogin_sock, conn_api.wr_buf + sent, wr - sent);
					if (ret > 0)
						sent += ret;
					else
						break;
				}
			}
		}
		else {
			assert_pthread_mutex_unlock(&(conn_outbuf.mutex));
		}
		if (ret < 0)
			break;
	}
	conn_api.terminate = true;
	shutdown(rlogin_sock, SHUT_RDWR);
	conn_api.output_thread_running = 2;
}

int
rlogin_connect(struct bbslist *bbs)
{
	char *ruser;
	char *passwd;

	if (!bbs->hidepopups)
		init_uifc(true, true);

	ruser = bbs->user;
	passwd = bbs->password;
	if (bbs->conn_type == CONN_TYPE_RLOGIN_REVERSED) {
		passwd = bbs->user;
		ruser = bbs->password;
	}

	/* Explicitly reset state from any prior session — conn_connect
	 * memsets conn_api but that's not a valid init for an _Atomic
	 * field, and stale flag values here would be catastrophic.
	 *
	 * binary_mode defaults to true (raw): SyncTERM is a glass terminal
	 * with no local flow control, matching what we advertise to SSH
	 * peers (IXON=0).  A server that wants local DC1/DC3 flow control
	 * can still request it with an OOB 0x20 byte. */
	rlogin_winsize_enabled = false;
	atomic_store(&rlogin_active,        false);
	atomic_store(&rlogin_input_paused,  false);
	atomic_store(&conn_api.binary_mode, true);

	rlogin_sock = conn_socket_connect(bbs, true);
	if (rlogin_sock == INVALID_SOCKET)
		return -1;

	if (!create_conn_buf(&conn_inbuf, BUFFER_SIZE))
		return -1;
	if (!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		destroy_conn_buf(&conn_inbuf);
		return -1;
	}
	if (!(conn_api.rd_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		return -1;
	}
	conn_api.rd_buf_size = BUFFER_SIZE;
	if (!(conn_api.wr_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		FREE_AND_NULL(conn_api.rd_buf);
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		return -1;
	}
	conn_api.wr_buf_size = BUFFER_SIZE;

	if ((bbs->conn_type == CONN_TYPE_RLOGIN) || (bbs->conn_type == CONN_TYPE_RLOGIN_REVERSED)) {
		/* rlogin-only: enable SO_OOBINLINE so RFC 1282 urgent-mode
		 * control bytes arrive inline and can be detected via
		 * SIOCATMARK.  Failure is not fatal — we just won't see
		 * control-byte requests. */
		{
			int on = 1;
			setsockopt(rlogin_sock, SOL_SOCKET, SO_OOBINLINE,
			    (const char *)&on, sizeof(on));
		}
		/* Send the rlogin handshake BEFORE installing tx_parse_cb so
		 * that a password or username byte of 0x11/0x13 is passed
		 * through verbatim rather than being stripped as XON/XOFF. */
		conn_send("", 1, 1000);
		conn_send(passwd, strlen(passwd) + 1, 1000);
		conn_send(ruser, strlen(ruser) + 1, 1000);
		if (bbs->bpsrate) {
			char sbuf[64];

			sprintf(sbuf, "%s/%d", get_emulation_str(bbs), bbs->bpsrate);

			conn_send(sbuf, strlen(sbuf) + 1, 1000);
		}
		else {
			char sbuf[64];

			sprintf(sbuf, "%s/115200", get_emulation_str(bbs));

			conn_send(sbuf, strlen(sbuf) + 1, 1000);
		}

		/* Now wire rlogin-only hooks: DC1/DC3 scanning on outbound,
		 * and a binary-mode-on side-effect that clears any pending
		 * pause.  These must NOT be wired for RAW / MBBS_GHOST,
		 * which share rlogin_connect but are transparent pipes. */
		conn_api.tx_parse_cb    = rlogin_tx_parse_cb;
		conn_api.binary_mode_on = rlogin_binary_mode_on;
		atomic_store(&rlogin_active, true);
	}

        /* Negotiate with GHost and bail if there's apparently no GHost listening. */
	if (bbs->conn_type == CONN_TYPE_MBBS_GHOST) {
		char sbuf[180];
		char rbuf[10];
		int  idx, ret = -1;

                /* Check to make sure GHost is actually listening */
		sendsocket(rlogin_sock, "\r\nMBBS: PING\r\n", 14);

		idx = 0;
		while (socket_readable(rlogin_sock, 1000)) {
			if (idx >= sizeof(rbuf) - 1)
				return -1;
			ret = recv(rlogin_sock, rbuf + idx, 1, 0);
			if (ret <= 0)
				break;
			rbuf[++idx] = 0;

                        /* It says ERROR, but this is a good response to PING. */
			if (strstr(rbuf, "ERROR\r\n"))
				break;
		}

		if (ret < 1)
			return -1;

		sprintf(sbuf, "MBBS: %s %d '%s' %d %s\r\n",
		    (bbs->ghost_program[0]) ? bbs->ghost_program : bbs->password,  /* Program name */
		    2,                                                             /* GHost protocol version */
		    bbs->user,                                                     /* User's full name */
		    999,                                                           /* Time remaining */
		    "GR"                                                           /* GR = ANSI, NG = ASCII */
		    );
		sendsocket(rlogin_sock, sbuf, strlen(sbuf));

		idx = 0;
		while (socket_readable(rlogin_sock, 1000)) {
			if (idx >= sizeof(rbuf) - 1)
				return -1;
			ret = recv(rlogin_sock, rbuf + idx, 1, 0);
			if (ret <= 0)
				break;
			rbuf[++idx] = 0;

                        /* GHost says it's launching the program, so pass terminal to user. */
			if (strstr(rbuf, "OK\r\n"))
				break;
		}

		if (ret < 1)
			return -1;
	}

	_beginthread(rlogin_output_thread, 0, NULL);
	_beginthread(rlogin_input_thread, 0, NULL);

	if (!bbs->hidepopups)
		uifc.pop(NULL);

	return 0;
}

int
rlogin_close(void)
{
	char garbage[1024];
	SOCKET oldsock;

	conn_api.terminate = true;
	oldsock = rlogin_sock;
	rlogin_sock = INVALID_SOCKET;
	while (conn_api.input_thread_running == 1 || conn_api.output_thread_running == 1) {
		conn_recv_upto(garbage, sizeof(garbage), 0);
		SLEEP(1);
	}
	closesocket(oldsock);
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	/* Clear rlogin-local state so a subsequent session starts clean. */
	atomic_store(&rlogin_active,       false);
	atomic_store(&rlogin_input_paused, false);
	rlogin_winsize_enabled = false;
	return 0;
}
