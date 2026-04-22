/* Copyright (C), 2007 by Stephen Hurd */

#include <stdlib.h>

#include "bbslist.h"
#include "ciolib.h"
#include "conn.h"
#include "conn_telnet.h"
#include "gen_defs.h"
#include "genwrap.h"
#include "sockwrap.h"
#include "ssh.h"
#include "syncterm.h"
#include "telnet_io.h"
#include "threadwrap.h"
#include "uifcinit.h"
#include "window.h"
#include "xp_tls.h"

static SOCKET telnets_sock;
static xp_tls_t telnets_session;
static pthread_mutex_t telnets_mutex;

static void
xp_tls_error_message(xp_tls_t sess, const char *doing)
{
	char title[128];
	char body[512];
	const char *err = xp_tls_errstr(sess);
	snprintf(title, sizeof(title), "TLS error %s", doing);
	snprintf(body, sizeof(body), "Error %s\r\n\r\n%s\r\n\r\n", doing, err);
	uifcmsg(title, body);
}

/*
 * Inner-wrapper terminate-on-close logic: set conn_api.terminate when
 * the connection itself has gone away (clean close or reset). Other
 * errors (protocol, memory, etc.) propagate via return value so the
 * caller can surface an error dialog.
 */
static int
FlushData(xp_tls_t sess)
{
	int ret = xp_tls_flush(sess);
	if (ret == XP_TLS_ERR_CLOSED) {
		conn_api.terminate = true;
		shutdown(telnets_sock, SHUT_RDWR);
	}
	return ret;
}

static int
PopData(xp_tls_t sess, void *buf, size_t len, size_t *copied)
{
	int ret = xp_tls_pop(sess, buf, len, copied);
	if (ret == XP_TLS_ERR_CLOSED) {
		conn_api.terminate = true;
		shutdown(telnets_sock, SHUT_RDWR);
	}
	return ret;
}

static int
PushData(xp_tls_t sess, const void *buf, size_t len, size_t *copied)
{
	int ret = xp_tls_push(sess, buf, len, copied);
	if (ret == XP_TLS_ERR_CLOSED) {
		conn_api.terminate = true;
		shutdown(telnets_sock, SHUT_RDWR);
	}
	return ret;
}

void
telnets_input_thread(void *args)
{
	int    status;
	size_t rd;
	size_t buffered;
	size_t bufsz = 0;
	SetThreadName("TelnetS Input");
	conn_api.input_thread_running = 1;
	while (!conn_api.terminate) {
		bool data_avail;

		if (!socket_check(telnets_sock, &data_avail, NULL, bufsz ? 0 : 100))
			break;
		if (data_avail && bufsz < BUFFER_SIZE) {
			assert_pthread_mutex_lock(&telnets_mutex);
			FlushData(telnets_session);
			status = PopData(telnets_session, conn_api.rd_buf + bufsz, conn_api.rd_buf_size - bufsz, &rd);
			assert_pthread_mutex_unlock(&telnets_mutex);
			bufsz += rd;
			/* Batch: on a timeout, loop back and try another pop
			   before flushing to conn_inbuf — matches the original
			   Cryptlib-TIMEOUT continue pattern. */
			if (status == XP_TLS_TIMEOUT)
				continue;
			if (status < 0) {
				if (!conn_api.terminate) {
					if (status != XP_TLS_ERR_CLOSED)	/* not a clean close */
						xp_tls_error_message(telnets_session, "recieving data");
					conn_api.terminate = true;
				}
				break;
			}
		}
		if (bufsz) {
			while (bufsz > 0 && !conn_api.terminate) {
				assert_pthread_mutex_lock(&(conn_inbuf.mutex));
				conn_buf_wait_free(&conn_inbuf, 1, 1000);
				buffered = conn_buf_put(&conn_inbuf, conn_api.rd_buf, bufsz);
				memmove(conn_api.rd_buf, &conn_api.rd_buf[buffered], bufsz - buffered);
				bufsz -= buffered;
				assert_pthread_mutex_unlock(&(conn_inbuf.mutex));
			}
		}
	}
	shutdown(telnets_sock, SHUT_RDWR);
	conn_api.input_thread_running = 2;
}
void
telnets_output_thread(void *args)
{
	size_t wr;
	size_t ret;
	size_t sent;
	int    status;
	SetThreadName("TelnetS Output");
	conn_api.output_thread_running = 1;
	while (!conn_api.terminate) {
		assert_pthread_mutex_lock(&(conn_outbuf.mutex));
		wr = conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if (wr) {
			wr = conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			assert_pthread_mutex_unlock(&(conn_outbuf.mutex));
			sent = 0;
			while ((!conn_api.terminate) && sent < wr) {
				assert_pthread_mutex_lock(&telnets_mutex);
				status = PushData(telnets_session, conn_api.wr_buf + sent, wr - sent, &ret);
				assert_pthread_mutex_unlock(&telnets_mutex);
				if (status < 0) {
					if (!conn_api.terminate) {
						if (status != XP_TLS_ERR_CLOSED)
							xp_tls_error_message(telnets_session, "sending data");
						conn_api.terminate = true;
					}
					break;
				}
				sent += ret;
			}
			if (sent) {
				assert_pthread_mutex_lock(&telnets_mutex);
				FlushData(telnets_session);
				assert_pthread_mutex_unlock(&telnets_mutex);
			}
		}
		else {
			assert_pthread_mutex_unlock(&(conn_outbuf.mutex));
		}
	}
	shutdown(telnets_sock, SHUT_RDWR);
	conn_api.output_thread_running = 2;
}

int
telnets_connect(struct bbslist *bbs)
{
	int off = 1;

	if (!bbs->hidepopups)
		init_uifc(true, true);
	assert_pthread_mutex_init(&telnets_mutex, NULL);

	telnets_sock = conn_socket_connect(bbs, true);
	if (telnets_sock == INVALID_SOCKET)
		return -1;

        /* we need to disable Nagle on the socket. */
	if (setsockopt(telnets_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&off, sizeof(off)))
		fprintf(stderr, "%s:%d: Error %d calling setsockopt()\n", __FILE__, __LINE__, errno);

	if (!bbs->hidepopups)
		uifc.pop("Activating Session");
	/* 1-second read timeout mirrors the Cryptlib-era behaviour that
	   the input thread loops around for rekey-detection. */
	telnets_session = xp_tls_client_open(telnets_sock, bbs->addr, 1);
	if (telnets_session == NULL) {
		char str[512];
		snprintf(str, sizeof(str), "Error activating session: %s", xp_tls_last_err());
		if (!bbs->hidepopups)
			uifcmsg("Error activating session", str);
		conn_api.terminate = 1;
		if (!bbs->hidepopups)
			uifc.pop(NULL);
		return -1;
	}
	if (!bbs->hidepopups)
		uifc.pop(NULL);

	if (!create_conn_buf(&conn_inbuf, BUFFER_SIZE))
		return -1;
	if (!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		destroy_conn_buf(&conn_inbuf);
		return -1;
	}
	if (!(conn_api.rd_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		conn_api.terminate = true;
		return -1;
	}
	conn_api.rd_buf_size = BUFFER_SIZE;
	if (!(conn_api.wr_buf = (unsigned char *)malloc(BUFFER_SIZE))) {
		FREE_AND_NULL(conn_api.rd_buf);
		conn_api.terminate = true;
		return -1;
	}
	conn_api.wr_buf_size = BUFFER_SIZE;
	conn_api.rx_parse_cb = telnet_rx_parse_cb;
	conn_api.tx_parse_cb = telnet_tx_parse_cb;

	telnet_deferred =  bbs->defer_telnet_negotiation;
	_beginthread(telnets_output_thread, 0, NULL);
	_beginthread(telnets_input_thread, 0, NULL);

	if (!telnet_deferred)
		send_initial_state();

	return 0;
}

int
telnets_close(void)
{
	char garbage[1024];
	conn_api.terminate = 1;
	/* Unblock the I/O threads by shutting the socket. xp_tls_close()
	   then runs its graceful-close best-effort on the dead fd. */
	shutdown(telnets_sock, SHUT_RDWR);
	while (conn_api.input_thread_running == 1 || conn_api.output_thread_running == 1) {
		conn_recv_upto(garbage, sizeof(garbage), 0);
		SLEEP(1);
	}
	xp_tls_close(telnets_session, /*close_socket=*/false);
	telnets_session = NULL;
	closesocket(telnets_sock);
	telnets_sock = INVALID_SOCKET;
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	pthread_mutex_destroy(&telnets_mutex);
	return 0;
}
