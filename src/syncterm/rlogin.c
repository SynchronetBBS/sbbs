/* $Id$ */

#include <stdlib.h>

#include "sockwrap.h"

#include "bbslist.h"
#include "conn.h"
#include "uifcinit.h"

static SOCKET sock=INVALID_SOCKET;

#ifdef __BORLANDC__
#pragma argsused
#endif
void rlogin_input_thread(void *args)
{
	fd_set	rds;
	int		rd;
	int	buffered;
	size_t	buffer;

	conn_api.input_thread_running=1;
	while(sock != INVALID_SOCKET && !conn_api.terminate) {
		FD_ZERO(&rds);
		FD_SET(sock, &rds);
		rd=select(sock+1, &rds, NULL, NULL, NULL);
		if(rd==-1) {
			if(errno==EBADF)
				break;
			rd=0;
		}
		if(rd==1) {
			rd=recv(sock, conn_api.rd_buf, conn_api.rd_buf_size, 0);
			if(rd <= 0)
				break;
		}
		buffered=0;
		while(buffered < rd) {
			pthread_mutex_lock(&(conn_inbuf.mutex));
			buffer=conn_buf_wait_free(&conn_inbuf, rd-buffered, 100);
			buffered+=conn_buf_put(&conn_inbuf, conn_api.rd_buf+buffered, buffer);
			pthread_mutex_unlock(&(conn_inbuf.mutex));
		}
	}
	conn_api.input_thread_running=0;
}

#ifdef __BORLANDC__
#pragma argsused
#endif
void rlogin_output_thread(void *args)
{
	fd_set	wds;
	int		wr;
	int		ret;
	int	sent;

	conn_api.output_thread_running=1;
	while(sock != INVALID_SOCKET && !conn_api.terminate) {
		pthread_mutex_lock(&(conn_outbuf.mutex));
		wr=conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if(wr) {
			wr=conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			pthread_mutex_unlock(&(conn_outbuf.mutex));
			sent=0;
			while(sent < wr) {
				FD_ZERO(&wds);
				FD_SET(sock, &wds);
				ret=select(sock+1, NULL, &wds, NULL, NULL);
				if(ret==-1) {
					if(errno==EBADF)
						break;
					ret=0;
				}
				if(ret==1) {
					ret=sendsocket(sock, conn_api.wr_buf+sent, wr-sent);
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

int rlogin_connect(struct bbslist *bbs)
{
	char	*ruser;
	char	*passwd;

	init_uifc(TRUE, TRUE);

	ruser=bbs->user;
	passwd=bbs->password;
	if(bbs->conn_type==CONN_TYPE_RLOGIN && bbs->reversed) {
		passwd=bbs->user;
		ruser=bbs->password;
	}

	sock=conn_socket_connect(bbs);
	if(sock==INVALID_SOCKET)
		return(-1);

	if(!create_conn_buf(&conn_inbuf, BUFFER_SIZE))
		return(-1);
	if(!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		destroy_conn_buf(&conn_inbuf);
		return(-1);
	}
	if(!(conn_api.rd_buf=(unsigned char *)malloc(BUFFER_SIZE))) {
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		return(-1);
	}
	conn_api.rd_buf_size=BUFFER_SIZE;
	if(!(conn_api.wr_buf=(unsigned char *)malloc(BUFFER_SIZE))) {
		free(conn_api.rd_buf);
		destroy_conn_buf(&conn_inbuf);
		destroy_conn_buf(&conn_outbuf);
		return(-1);
	}
	conn_api.wr_buf_size=BUFFER_SIZE;

	if(bbs->conn_type == CONN_TYPE_RLOGIN) {
		conn_send("",1,1000);
		conn_send(passwd,strlen(passwd)+1,1000);
		conn_send(ruser,strlen(ruser)+1,1000);
		if(bbs->bpsrate) {
			char	sbuf[30];
			sprintf(sbuf, "ansi-bbs/%d", bbs->bpsrate);

			conn_send(sbuf, strlen(sbuf)+1,1000);
		}
		else
			conn_send("ansi-bbs/115200",15,1000);
	}

	_beginthread(rlogin_output_thread, 0, NULL);
	_beginthread(rlogin_input_thread, 0, NULL);

	uifc.pop(NULL);

	return(0);
}

int rlogin_close(void)
{
	conn_api.terminate=1;
	closesocket(sock);
	while(conn_api.input_thread_running || conn_api.output_thread_running)
		SLEEP(1);
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	return(0);
}
