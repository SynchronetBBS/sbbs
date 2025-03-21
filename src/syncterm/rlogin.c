/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: rlogin.c,v 1.38 2020/06/27 00:04:50 deuce Exp $ */

#include <stdlib.h>

#include "bbslist.h"
#include "conn.h"
#include "sockwrap.h"
#include "uifcinit.h"

SOCKET rlogin_sock = INVALID_SOCKET;

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
		bool data_avail;
		if (socket_check(rlogin_sock, &data_avail, NULL, bufsz ? 0 : 100)) {
			if (data_avail && bufsz < BUFFER_SIZE) {
				rd = recv(rlogin_sock, conn_api.rd_buf + bufsz, conn_api.rd_buf_size - bufsz, 0);
				if (rd <= 0)
					break;
				bufsz += rd;
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
		conn_send("", 1, 1000);
		conn_send(passwd, strlen(passwd) + 1, 1000);
		conn_send(ruser, strlen(ruser) + 1, 1000);
		if (bbs->bpsrate) {
			char sbuf[30];

			sprintf(sbuf, "%s/%d", get_emulation_str(get_emulation(bbs)), bbs->bpsrate);

			conn_send(sbuf, strlen(sbuf) + 1, 1000);
		}
		else {
			char sbuf[30];

			sprintf(sbuf, "%s/115200", get_emulation_str(get_emulation(bbs)));

			conn_send(sbuf, strlen(sbuf) + 1, 1000);
		}
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
			ret = recv(rlogin_sock, rbuf + idx, 1, 0);
			if (ret <= 0)
				break;
			rbuf[++idx] = 0;

                        /* It says ERROR, but this is a good response to PING. */
			if (strstr(rbuf, "ERROR\r\n"))
				break;

                        /* We didn't receive the desired response in time, so bail. */
			if (idx >= sizeof(rbuf))
				return -1;
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
			ret = recv(rlogin_sock, rbuf + idx, 1, 0);
			if (ret <= 0)
				break;
			rbuf[++idx] = 0;

                        /* GHost says it's launching the program, so pass terminal to user. */
			if (strstr(rbuf, "OK\r\n"))
				break;

                        /* We didn't receive the desired response in time, so bail. */
			if (idx >= sizeof(rbuf))
				return -1;
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
	return 0;
}
