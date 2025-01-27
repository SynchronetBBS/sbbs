/* Copyright (C), 2007 by Stephen Hurd */

#include <stdlib.h>

#include "bbslist.h"
#include "ciolib.h"
#include "conn.h"
#include "conn_telnet.h"
#include "cryptlib.h"
#include "gen_defs.h"
#include "genwrap.h"
#include "sockwrap.h"
#include "ssh.h"
#include "syncterm.h"
#include "telnet_io.h"
#include "threadwrap.h"
#include "uifcinit.h"
#include "window.h"

static SOCKET telnets_sock;
static CRYPT_SESSION telnets_session;
static pthread_mutex_t telnets_mutex;

static int
FlushData(CRYPT_SESSION sess)
{
	int ret = cryptFlushData(sess);
	if (ret == CRYPT_ERROR_COMPLETE || ret == CRYPT_ERROR_READ) {
		conn_api.terminate = true;
		shutdown(telnets_sock, SHUT_RDWR);
	}
	return ret;
}

static int
PopData(CRYPT_HANDLE e, void *buf, int len, int *copied)
{
	int ret = cryptPopData(e, buf, len, copied);
	if (ret == CRYPT_ERROR_COMPLETE || ret == CRYPT_ERROR_READ) {
		conn_api.terminate = true;
		shutdown(telnets_sock, SHUT_RDWR);
	}
	return ret;
}

static int
PushData(CRYPT_HANDLE e, void *buf, int len, int *copied)
{
	int ret = cryptPushData(e, buf, len, copied);
	if (ret == CRYPT_ERROR_COMPLETE || ret == CRYPT_ERROR_WRITE) {
		conn_api.terminate = true;
		shutdown(telnets_sock, SHUT_RDWR);
	}
	return ret;
}

void
telnets_input_thread(void *args)
{
	int    status;
	int    rd;
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
			// Handle case where there was socket activity without readable data (ie: rekey)
			if (status == CRYPT_ERROR_TIMEOUT)
				continue;
			if (cryptStatusError(status)) {
				if (!conn_api.terminate) {
					if ((status != CRYPT_ERROR_COMPLETE) && (status != CRYPT_ERROR_READ)) /* connection closed */
						cryptlib_error_message(status, "recieving data");
					conn_api.terminate = true;
				}
				break;
			}
		}
		if (bufsz) {
			buffered = 0;
			while (buffered < rd) {
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
	int    wr;
	int    ret;
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
				if (cryptStatusError(status)) {
					if (!conn_api.terminate) {
						if (status != CRYPT_ERROR_COMPLETE && status != CRYPT_ERROR_READ) { /* connection closed */
							cryptlib_error_message(status, "sending data");
						}
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
	int status;

	if (!bbs->hidepopups)
		init_uifc(true, true);
	assert_pthread_mutex_init(&telnets_mutex, NULL);

	telnets_sock = conn_socket_connect(bbs, true);
	if (telnets_sock == INVALID_SOCKET)
		return -1;

	if (!bbs->hidepopups)
		uifc.pop("Creating Session");
	status = cryptCreateSession(&telnets_session, CRYPT_UNUSED, CRYPT_SESSION_SSL);
	if (cryptStatusError(status)) {
		char str[1024];
		sprintf(str, "Error %d creating session", status);
		if (!bbs->hidepopups)
			uifcmsg("Error creating session", str);
		conn_api.terminate = 1;
		if (!bbs->hidepopups)
			uifc.pop(NULL);
		return -1;
	}

        /* we need to disable Nagle on the socket. */
	if (setsockopt(telnets_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&off, sizeof(off)))
		fprintf(stderr, "%s:%d: Error %d calling setsockopt()\n", __FILE__, __LINE__, errno);

	if (!bbs->hidepopups)
		uifc.pop(NULL);

        /* Pass socket to cryptlib */
	status = cryptSetAttribute(telnets_session, CRYPT_SESSINFO_NETWORKSOCKET, telnets_sock);
	if (cryptStatusError(status)) {
		char str[1024];
		sprintf(str, "Error %d passing socket", status);
		if (!bbs->hidepopups)
			uifcmsg("Error passing socket", str);
		conn_api.terminate = 1;
		if (!bbs->hidepopups)
			uifc.pop(NULL);
		return -1;
	}

	cryptSetAttribute(telnets_session, CRYPT_OPTION_NET_READTIMEOUT, 1);
	cryptSetAttributeString(telnets_session, CRYPT_SESSINFO_SERVER_NAME, bbs->addr, strlen(bbs->addr));

        /* Activate the session */
	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Activating Session");
	}
	status = cryptSetAttribute(telnets_session, CRYPT_SESSINFO_ACTIVE, 1);
	if (cryptStatusError(status)) {
		if (!bbs->hidepopups)
			cryptlib_error_message(status, "activating session");
		conn_api.terminate = 1;
		if (!bbs->hidepopups)
			uifc.pop(NULL);
		return -1;
	}

	if (!bbs->hidepopups) {
                /* Clear ownership */
		uifc.pop(NULL); // TODO: Why is this called twice?
		uifc.pop(NULL);
		uifc.pop("Clearing Ownership");
	}
	status = cryptSetAttribute(telnets_session, CRYPT_PROPERTY_OWNER, CRYPT_UNUSED);
	if (cryptStatusError(status)) {
		if (!bbs->hidepopups)
			cryptlib_error_message(status, "clearing session ownership");
		conn_api.terminate = 1;
		if (!bbs->hidepopups)
			uifc.pop(NULL);
		conn_api.terminate = true;
		return -1;
	}
	if (!bbs->hidepopups)
		uifc.pop(NULL);

	create_conn_buf(&conn_inbuf, BUFFER_SIZE);
	create_conn_buf(&conn_outbuf, BUFFER_SIZE);
	conn_api.rd_buf = (unsigned char *)malloc(BUFFER_SIZE);
	conn_api.rd_buf_size = BUFFER_SIZE;
	conn_api.wr_buf = (unsigned char *)malloc(BUFFER_SIZE);
	conn_api.wr_buf_size = BUFFER_SIZE;
	conn_api.rx_parse_cb = telnet_rx_parse_cb;
	conn_api.tx_parse_cb = telnet_tx_parse_cb;

	_beginthread(telnets_output_thread, 0, NULL);
	_beginthread(telnets_input_thread, 0, NULL);

	if (!bbs->hidepopups)
		uifc.pop(NULL); // TODO: Why is this called twice?

	// Suppress Go Aheads (both directions)
	request_telnet_opt(TELNET_WILL, TELNET_SUP_GA);
	request_telnet_opt(TELNET_DO, TELNET_SUP_GA);
	if (!bbs->telnet_no_binary) {
		// Enable binary mode (both directions)
		request_telnet_opt(TELNET_WILL, TELNET_BINARY_TX);
		request_telnet_opt(TELNET_DO, TELNET_BINARY_TX);
	}
	// Request that the server echos
	request_telnet_opt(TELNET_DO, TELNET_ECHO);

	return 0;
}

int
telnets_close(void)
{
	char garbage[1024];
	conn_api.terminate = 1;
	cryptSetAttribute(telnets_session, CRYPT_SESSINFO_ACTIVE, 0);
	while (conn_api.input_thread_running == 1 || conn_api.output_thread_running == 1) {
		conn_recv_upto(garbage, sizeof(garbage), 0);
		SLEEP(1);
	}
	cryptDestroySession(telnets_session);
	closesocket(telnets_sock);
	telnets_sock = INVALID_SOCKET;
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	pthread_mutex_destroy(&telnets_mutex);
	return 0;
}
