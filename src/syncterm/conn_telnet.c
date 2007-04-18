/* $Id$ */

#include <stdlib.h>

#include "gen_defs.h"
#include "genwrap.h"
#include "sockwrap.h"
#include "threadwrap.h"

#include "bbslist.h"
#include "conn.h"
#include "uifcinit.h"

#include "telnet_io.h"

SOCKET telnet_sock=INVALID_SOCKET;

#ifdef __BORLANDC__
#pragma argsused
#endif
void telnet_input_thread(void *args)
{
	fd_set	rds;
	int		rd;
	int	buffered;
	size_t	buffer;
	char	rbuf[BUFFER_SIZE];
	char	*buf;

	conn_api.input_thread_running=1;
	while(telnet_sock != INVALID_SOCKET && !conn_api.terminate) {
		FD_ZERO(&rds);
		FD_SET(telnet_sock, &rds);
		rd=select(telnet_sock+1, &rds, NULL, NULL, NULL);
		if(rd==-1) {
			if(errno==EBADF)
				break;
			rd=0;
		}
		if(rd==1) {
			rd=recv(telnet_sock, conn_api.rd_buf, conn_api.rd_buf_size, 0);
			if(rd <= 0)
				break;
		}
		if(rd>0)
			buf=telnet_interpret(conn_api.rd_buf, rd, rbuf, &rd);
		buffered=0;
		while(buffered < rd) {
			pthread_mutex_lock(&(conn_inbuf.mutex));
			buffer=conn_buf_wait_free(&conn_inbuf, rd-buffered, 100);
			buffered+=conn_buf_put(&conn_inbuf, buf+buffered, buffer);
			pthread_mutex_unlock(&(conn_inbuf.mutex));
		}
	}
	conn_api.input_thread_running=0;
}

#ifdef __BORLANDC__
#pragma argsused
#endif
void telnet_output_thread(void *args)
{
	fd_set	wds;
	size_t		wr;
	int		ret;
	size_t	sent;
	char	ebuf[BUFFER_SIZE*2];
	char	*buf;

	conn_api.output_thread_running=1;
	while(telnet_sock != INVALID_SOCKET && !conn_api.terminate) {
		pthread_mutex_lock(&(conn_outbuf.mutex));
		wr=conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if(wr) {
			wr=conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			pthread_mutex_unlock(&(conn_outbuf.mutex));
			buf=telnet_expand(conn_api.wr_buf, wr, ebuf, &wr);
			sent=0;
			while(sent < wr) {
				FD_ZERO(&wds);
				FD_SET(telnet_sock, &wds);
				ret=select(telnet_sock+1, NULL, &wds, NULL, NULL);
				if(ret==-1) {
					if(errno==EBADF)
						break;
					ret=0;
				}
				if(ret==1) {
					ret=sendsocket(telnet_sock, buf+sent, wr-sent);
					if(ret==-1)
						break;
					sent+=ret;
				}
			}
		}
		else
			pthread_mutex_unlock(&(conn_outbuf.mutex));
		if(ret==-1)
			break;
	}
	conn_api.output_thread_running=0;
}

int telnet_connect(struct bbslist *bbs)
{
	init_uifc(TRUE, TRUE);

	telnet_sock=conn_socket_connect(bbs);
	if(telnet_sock==INVALID_SOCKET)
		return(-1);

	create_conn_buf(&conn_inbuf, BUFFER_SIZE);
	create_conn_buf(&conn_outbuf, BUFFER_SIZE);
	conn_api.rd_buf=(unsigned char *)malloc(BUFFER_SIZE);
	conn_api.rd_buf_size=BUFFER_SIZE;
	conn_api.wr_buf=(unsigned char *)malloc(BUFFER_SIZE);
	conn_api.wr_buf_size=BUFFER_SIZE;

	memset(telnet_local_option,0,sizeof(telnet_local_option));
	memset(telnet_remote_option,0,sizeof(telnet_remote_option));

	_beginthread(telnet_output_thread, 0, NULL);
	_beginthread(telnet_input_thread, 0, NULL);

	uifc.pop(NULL);

	return(0);
}

int telnet_close(void)
{
	conn_api.terminate=1;
	closesocket(telnet_sock);
	while(conn_api.input_thread_running || conn_api.output_thread_running)
		SLEEP(1);
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	return(0);
}

void telnet_binary_mode_on(void)
{
	request_telnet_opt(TELNET_DO,TELNET_BINARY_TX);
	request_telnet_opt(TELNET_WILL,TELNET_BINARY_TX);
}

void telnet_binary_mode_off(void)
{
	request_telnet_opt(TELNET_DONT,TELNET_BINARY_TX);
	request_telnet_opt(TELNET_WONT,TELNET_BINARY_TX);
}
