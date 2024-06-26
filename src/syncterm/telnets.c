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
#include "threadwrap.h"
#include "uifcinit.h"
#include "window.h"

int
telnets_connect(struct bbslist *bbs)
{
	int off = 1;
	int status;

	if (!bbs->hidepopups)
		init_uifc(true, true);
	pthread_mutex_init(&ssh_mutex, NULL);

	ssh_sock = conn_socket_connect(bbs);
	if (ssh_sock == INVALID_SOCKET)
		return -1;

	ssh_active = false;

	if (!bbs->hidepopups)
		uifc.pop("Creating Session");
	status = cryptCreateSession(&ssh_session, CRYPT_UNUSED, CRYPT_SESSION_SSL);
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
	if (setsockopt(ssh_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&off, sizeof(off)))
		fprintf(stderr, "%s:%d: Error %d calling setsockopt()\n", __FILE__, __LINE__, errno);

	if (!bbs->hidepopups)
		uifc.pop(NULL);

        /* Pass socket to cryptlib */
	status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_NETWORKSOCKET, ssh_sock);
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

	cryptSetAttribute(ssh_session, CRYPT_OPTION_NET_READTIMEOUT, 1);

        /* Activate the session */
	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Activating Session");
	}
	status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_ACTIVE, 1);
	if (cryptStatusError(status)) {
		if (!bbs->hidepopups)
			cryptlib_error_message(status, "activating session");
		conn_api.terminate = 1;
		if (!bbs->hidepopups)
			uifc.pop(NULL);
		return -1;
	}

	ssh_active = true;
	if (!bbs->hidepopups) {
                /* Clear ownership */
		uifc.pop(NULL); // TODO: Why is this called twice?
		uifc.pop(NULL);
		uifc.pop("Clearing Ownership");
	}
	status = cryptSetAttribute(ssh_session, CRYPT_PROPERTY_OWNER, CRYPT_UNUSED);
	if (cryptStatusError(status)) {
		if (!bbs->hidepopups)
			cryptlib_error_message(status, "clearing session ownership");
		conn_api.terminate = 1;
		if (!bbs->hidepopups)
			uifc.pop(NULL);
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

	_beginthread(ssh_output_thread, 0, NULL);
	_beginthread(ssh_input_thread, 0, NULL);

	if (!bbs->hidepopups)
		uifc.pop(NULL); // TODO: Why is this called twice?

	return 0;
}
