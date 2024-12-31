/* Copyright (C), 2007 by Stephen Hurd */

#include <assert.h>

#if __STDC_NO_ATOMICS__
#error Support for stdatomic.h is required.
#endif

#include <stdatomic.h>
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
#include "threadwrap.h"
#include "uifcinit.h"
#include "window.h"

static SOCKET telnets_sock;
static CRYPT_SESSION telnets_session;
static atomic_bool telnets_active = true;
static pthread_once_t telnets_active_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t telnets_mutex;

void
telnets_input_thread(void *args)
{
	int    status;
	int    rd;
	size_t buffered;
	size_t buffer;
	SetThreadName("TelnetS Input");
	conn_api.input_thread_running = 1;
	while (atomic_load(&telnets_active) && !conn_api.terminate) {
		if (!socket_readable(telnets_sock, 100))
			continue;
		pthread_mutex_lock(&telnets_mutex);
		cryptFlushData(telnets_session);
		status = cryptPopData(telnets_session, conn_api.rd_buf, conn_api.rd_buf_size, &rd);
		pthread_mutex_unlock(&telnets_mutex);
                // Handle case where there was socket activity without readable data (ie: rekey)
		if (status == CRYPT_ERROR_TIMEOUT)
			continue;
		if (cryptStatusError(status)) {
			if ((status == CRYPT_ERROR_COMPLETE) || (status == CRYPT_ERROR_READ)) { /* connection closed */
				atomic_store(&telnets_active, false);
				break;
			}
			cryptlib_error_message(status, "recieving data");
			atomic_store(&telnets_active, false);
			break;
		}
		else {
			buffered = 0;
			while (buffered < rd) {
				pthread_mutex_lock(&(conn_inbuf.mutex));
				buffer = conn_buf_wait_free(&conn_inbuf, rd - buffered, 100);
				buffered += conn_buf_put(&conn_inbuf, conn_api.rd_buf + buffered, buffer);
				pthread_mutex_unlock(&(conn_inbuf.mutex));
			}
		}
	}
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
	while (atomic_load(&telnets_active) && !conn_api.terminate) {
		pthread_mutex_lock(&(conn_outbuf.mutex));
		wr = conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if (wr) {
			wr = conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			pthread_mutex_unlock(&(conn_outbuf.mutex));
			sent = 0;
			while (atomic_load(&telnets_active) && sent < wr) {
				pthread_mutex_lock(&telnets_mutex);
				status = cryptPushData(telnets_session, conn_api.wr_buf + sent, wr - sent, &ret);
				pthread_mutex_unlock(&telnets_mutex);
				if (cryptStatusError(status)) {
					if (status == CRYPT_ERROR_COMPLETE) { /* connection closed */
						atomic_store(&telnets_active, false);
						break;
					}
					cryptlib_error_message(status, "sending data");
					atomic_store(&telnets_active, false);
					break;
				}
				sent += ret;
			}
			if (sent) {
				pthread_mutex_lock(&telnets_mutex);
				cryptFlushData(telnets_session);
				pthread_mutex_unlock(&telnets_mutex);
			}
		}
		else {
			pthread_mutex_unlock(&(conn_outbuf.mutex));
		}
	}
	conn_api.output_thread_running = 2;
}

static void
init_once(void)
{
	// We do this with a once because there's no destroy function, and it may not be lock-free
	atomic_init(&telnets_active, false);
}


int
telnets_connect(struct bbslist *bbs)
{
	int off = 1;
	int status;

	if (!bbs->hidepopups)
		init_uifc(true, true);
	pthread_mutex_init(&telnets_mutex, NULL);
	pthread_once(&telnets_active_once, init_once);
	atomic_store(&telnets_active, false);

	telnets_sock = conn_socket_connect(bbs);
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

	atomic_store(&telnets_active, true);
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
		atomic_store(&telnets_active, false);
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

	return 0;
}

int
telnets_close(void)
{
	char garbage[1024];
	conn_api.terminate = 1;
	cryptSetAttribute(telnets_session, CRYPT_SESSINFO_ACTIVE, 0);
	atomic_store(&telnets_active, false);
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
