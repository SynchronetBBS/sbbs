/* Copyright (C), 2007 by Stephen Hurd */

/* $Id$ */

#include <stdlib.h>

#include "ciolib.h"

#include "gen_defs.h"
#include "genwrap.h"
#include "sockwrap.h"
#include "threadwrap.h"

#include "bbslist.h"
#include "uifcinit.h"
#include "conn.h"
#include "rlogin.h"
#include "raw.h"
#include "ssh.h"
#include "modem.h"
#ifdef __unix__
#include "conn_pty.h"
#endif
#include "conn_telnet.h"

struct conn_api conn_api;
char *conn_types[]={"Unknown","RLogin","RLogin Reversed","Telnet","Raw","SSH","Modem"
#ifdef __unix__
,"Shell"
#endif
,NULL};
short unsigned int conn_ports[]={0,513,513,23,0,22,0
#ifdef __unix__
,65535
#endif
,0};

struct conn_buffer conn_inbuf;
struct conn_buffer conn_outbuf;

/* Buffer functions */
struct conn_buffer *create_conn_buf(struct conn_buffer *buf, size_t size)
{
	buf->buf=(unsigned char *)malloc(size);
	if(buf->buf==NULL)
		return(NULL);
	buf->bufsize=size;
	buf->buftop=0;
	buf->bufbot=0;
	buf->isempty=1;
	if(pthread_mutex_init(&(buf->mutex), NULL)) {
		FREE_AND_NULL(buf->buf);
		return(NULL);
	}
	if(sem_init(&(buf->in_sem), 0, 0)) {
		FREE_AND_NULL(buf->buf);
		pthread_mutex_destroy(&(buf->mutex));
		return(NULL);
	}
	if(sem_init(&(buf->out_sem), 0, 0)) {
		FREE_AND_NULL(buf->buf);
		pthread_mutex_destroy(&(buf->mutex));
		sem_destroy(&(buf->in_sem));
		return(NULL);
	}
	return(buf);
}

void destroy_conn_buf(struct conn_buffer *buf)
{
	if(buf->buf != NULL) {
		FREE_AND_NULL(buf->buf);
		while(pthread_mutex_destroy(&(buf->mutex)));
		while(sem_destroy(&(buf->in_sem)));
		while(sem_destroy(&(buf->out_sem)));
	}
}

/*
 * The mutex should always be locked by the caller
 * for the rest of the buffer functions
 */
size_t conn_buf_bytes(struct conn_buffer *buf)
{
	if(buf->isempty)
		return(0);

	if(buf->buftop > buf->bufbot)
		return(buf->buftop - buf->bufbot);
	return(buf->bufsize - buf->bufbot + buf->buftop);
}

size_t conn_buf_free(struct conn_buffer *buf)
{
	return(buf->bufsize - conn_buf_bytes(buf));
}

/*
 * Copies up to outlen bytes from the buffer into outbuf,
 * leaving them in the buffer.  Returns the number of bytes
 * copied out of the buffer
 */
size_t conn_buf_peek(struct conn_buffer *buf, unsigned char *outbuf, size_t outlen)
{
	size_t copy_bytes;
	size_t chunk;

	copy_bytes=conn_buf_bytes(buf);
	if(copy_bytes > outlen)
		copy_bytes=outlen;
	chunk=buf->bufsize - buf->bufbot;
	if(chunk > copy_bytes)
		chunk=copy_bytes;

	if(chunk)
		memcpy(outbuf, buf->buf+buf->bufbot, chunk);
	if(chunk < copy_bytes)
		memcpy(outbuf+chunk, buf->buf, copy_bytes-chunk);

	return(copy_bytes);
}

/*
 * Copies up to outlen bytes from the buffer into outbuf,
 * removing them from the buffer.  Returns the number of
 * bytes removed from the buffer.
 */
size_t conn_buf_get(struct conn_buffer *buf, unsigned char *outbuf, size_t outlen)
{
	size_t ret;
	size_t atstart;

	atstart=conn_buf_bytes(buf);
	ret=conn_buf_peek(buf, outbuf, outlen);
	if(ret) {
		buf->bufbot+=ret;
		if(buf->bufbot >= buf->bufsize)
			buf->bufbot -= buf->bufsize;
		if(ret==atstart)
			buf->isempty=1;
		sem_post(&(buf->out_sem));
	}
	return(ret);
}

/*
 * Places up to outlen bytes from outbuf into the buffer
 * returns the number of bytes written into the buffer
 */
size_t conn_buf_put(struct conn_buffer *buf, const unsigned char *outbuf, size_t outlen)
{
	size_t write_bytes;
	size_t chunk;

	write_bytes=conn_buf_free(buf);
	if(write_bytes > outlen)
		write_bytes = outlen;
	if(write_bytes) {
		chunk=buf->bufsize - buf->buftop;
		if(chunk > write_bytes)
			chunk=write_bytes;
		if(chunk)
			memcpy(buf->buf+buf->buftop, outbuf, chunk);
		if(chunk < write_bytes)
			memcpy(buf->buf, outbuf+chunk, write_bytes-chunk);
		buf->buftop+=write_bytes;
		if(buf->buftop >= buf->bufsize)
			buf->buftop -= buf->bufsize;
		buf->isempty=0;
		sem_post(&(buf->in_sem));
	}
	return(write_bytes);
}

/*
 * Waits up to timeout milliseconds for bcount bytes to be available/free
 * in the buffer.
 */
size_t conn_buf_wait_cond(struct conn_buffer *buf, size_t bcount, unsigned long timeout, int do_free)
{
	long double now;
	long double end;
	size_t found;
	unsigned long timeleft;
	int retnow=0;
	sem_t	*sem;
	size_t (*cond)(struct conn_buffer *buf);

	if(do_free) {
		sem=&(buf->out_sem);
		cond=conn_buf_free;
	}
	else {
		sem=&(buf->in_sem);
		cond=conn_buf_bytes;
	}

	found=cond(buf);
	if(found > bcount)
		found=bcount;

	if(found == bcount || timeout==0)
		return(found);

	pthread_mutex_unlock(&(buf->mutex));

	end=timeout;
	end/=1000;
	now=xp_timer();
	end+=now;

	for(;;) {
		now=xp_timer();
		if(end <= now)
			timeleft=0;
		else {
			timeleft=(end-now)*1000;
			if(timeleft<1 || timeleft > timeout)
				timeleft=1;
		}
		if(sem_trywait_block(sem, timeleft))
			retnow=1;
		pthread_mutex_lock(&(buf->mutex));
		found=cond(buf);
		if(found > bcount)
			found=bcount;

		if(found == bcount || retnow)
			return(found);

		pthread_mutex_unlock(&(buf->mutex));
	}
}

/*
 * Connection functions
 */

BOOL conn_connected(void)
{
	if(conn_api.input_thread_running && conn_api.output_thread_running)
		return(TRUE);
	return(FALSE);
}

int conn_recv(char *buffer, size_t buflen, unsigned timeout)
{
	size_t found;

	pthread_mutex_lock(&(conn_inbuf.mutex));
	found=conn_buf_wait_bytes(&conn_inbuf, buflen, timeout);
	if(found)
		found=conn_buf_get(&conn_inbuf, buffer, found);
	pthread_mutex_unlock(&(conn_inbuf.mutex));
	return(found);
}

int conn_peek(char *buffer, size_t buflen)
{
	size_t found;

	pthread_mutex_lock(&(conn_inbuf.mutex));
	found=conn_buf_wait_bytes(&conn_inbuf, buflen, 0);
	if(found)
		found=conn_buf_peek(&conn_inbuf, buffer, found);
	pthread_mutex_unlock(&(conn_inbuf.mutex));
	return(found);
}

int conn_send(char *buffer, size_t buflen, unsigned int timeout)
{
	size_t found;

	pthread_mutex_lock(&(conn_outbuf.mutex));
	found=conn_buf_wait_free(&conn_outbuf, buflen, timeout);
	if(found)
		found=conn_buf_put(&conn_outbuf, buffer, found);
	pthread_mutex_unlock(&(conn_outbuf.mutex));
	return(found);
}

int conn_connect(struct bbslist *bbs)
{
	memset(&conn_api, 0, sizeof(conn_api));

	switch(bbs->conn_type) {
		case CONN_TYPE_RLOGIN:
		case CONN_TYPE_RLOGIN_REVERSED:
			conn_api.connect=rlogin_connect;
			conn_api.close=rlogin_close;
			break;
		case CONN_TYPE_TELNET:
			conn_api.connect=telnet_connect;
			conn_api.close=telnet_close;
			conn_api.binary_mode_on=telnet_binary_mode_on;
			conn_api.binary_mode_off=telnet_binary_mode_off;
			break;
		case CONN_TYPE_RAW:
			conn_api.connect=raw_connect;
			conn_api.close=raw_close;
			break;
		case CONN_TYPE_SSH:
			conn_api.connect=ssh_connect;
			conn_api.close=ssh_close;
			break;
		case CONN_TYPE_MODEM:
			conn_api.connect=modem_connect;
			conn_api.close=modem_close;
			break;
#ifdef __unix__
		case CONN_TYPE_SHELL:
			conn_api.connect=pty_connect;
			conn_api.close=pty_close;
			break;
#endif
	}
	if(conn_api.connect) {
		if(conn_api.connect(bbs)) {
			conn_api.terminate = 1;
			while(conn_api.input_thread_running || conn_api.output_thread_running)
				SLEEP(1);
		}
		else {
			while(conn_api.terminate == 0 && (conn_api.input_thread_running == 0 || conn_api.output_thread_running == 0))
				SLEEP(1);
		}
	}
	return(conn_api.terminate);
}

size_t conn_data_waiting(void)
{
	size_t found;

	pthread_mutex_lock(&(conn_inbuf.mutex));
	found=conn_buf_bytes(&conn_inbuf);
	pthread_mutex_unlock(&(conn_inbuf.mutex));
	return(found);
}

int conn_close(void)
{
	if(conn_api.close)
		return(conn_api.close());
	return(0);
}

enum failure_reason {
	 FAILURE_RESOLVE
	,FAILURE_CANT_CREATE
	,FAILURE_CONNECT_ERROR
	,FAILURE_ABORTED
	,FAILURE_GENERAL
};

int conn_socket_connect(struct bbslist *bbs)
{
	SOCKET	sock;
	HOSTENT *ent;
	SOCKADDR_IN	saddr;
	char	*p;
	unsigned int	neta;
	int		nonblock;
	struct timeval tv;
	fd_set	wfd;
	int		failcode;

	for(p=bbs->addr;*p;p++)
		if(*p!='.' && !isdigit(*p))
			break;

	if(!(*p))
		neta=inet_addr(bbs->addr);
	else {
		uifc.pop("Looking up host");
		if((ent=gethostbyname(bbs->addr))==NULL) {
			failcode=FAILURE_RESOLVE;
			goto connect_failed;
		}
		neta=*((unsigned int*)ent->h_addr_list[0]);
		uifc.pop(NULL);
	}
	uifc.pop("Connecting...");

	sock=socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
	if(sock==INVALID_SOCKET) {
		failcode=FAILURE_CANT_CREATE;
		goto connect_failed;
	}
	memset(&saddr,0,sizeof(saddr));
	saddr.sin_addr.s_addr = neta;
	saddr.sin_family = AF_INET;
	saddr.sin_port   = htons((WORD)bbs->port);

	/* Set to non-blocking for the connect */
	nonblock=-1;
	ioctlsocket(sock, FIONBIO, &nonblock);
	/* Drain the input buffer to avoid accidental cancel */
	while(kbhit())
		getch();
	if(connect(sock, (struct sockaddr *)&saddr, sizeof(saddr))) {
		switch(ERROR_VALUE) {
			case EINPROGRESS:
			case EINTR:
			case EAGAIN:

#if (!defined(EAGAIN) && defined(EWOULDBLOCK)) || (EAGAIN!=EWOULDBLOCK)
			case EWOULDBLOCK:
#endif
				break;
			default:
				failcode=FAILURE_CONNECT_ERROR;
				goto connect_failed;
		}
	}
	else
		goto connected;
	for(;;) {
		tv.tv_sec=1;
		tv.tv_usec=0;

		FD_ZERO(&wfd);
		FD_SET(sock, &wfd);
		switch(select(sock+1, NULL, &wfd, NULL, &tv)) {
			case 0:
				if(kbhit()) {
					failcode=FAILURE_ABORTED;
					goto connect_failed;
				}
				break;
			case -1:
				failcode=FAILURE_GENERAL;
				goto connect_failed;
			case 1:
				goto connected;
			default:
				break;
		}
	}
connected:
	nonblock=0;
	ioctlsocket(sock, FIONBIO, &nonblock);
	if(!socket_check(sock, NULL, NULL, 0))
		goto connect_failed;

	uifc.pop(NULL);
	return(sock);

connect_failed:
	{
		char str[LIST_ADDR_MAX+40];

		uifc.pop(NULL);
		conn_api.terminate=-1;
		switch(failcode) {
			case FAILURE_RESOLVE:
				sprintf(str,"Cannot resolve %s!",bbs->addr);
				uifcmsg(str,	"`Cannot Resolve Host`\n\n"
								"The system is unable to resolve the hostname... double check the spelling.\n"
								"If it's not an issue with your DNS settings, the issue is probobly\n"
								"with the DNS settings of the system you are trying to contact.");
				break;
			case FAILURE_CANT_CREATE:
				sprintf(str,"Cannot create socket (%d)!",ERROR_VALUE);
				uifcmsg(str,
								"`Unable to create socket`\n\n"
								"Your system is either dangerously low on resources, or there\n"
								"is a problem with your TCP/IP stack.");
				break;
			case FAILURE_CONNECT_ERROR:
				sprintf(str,"Connect error (%d)!",ERROR_VALUE);
				uifcmsg(str
								,"`The connect call returned an error`\n\n"
								 "The call to connect() returned an unexpected error code.");
				break;
			case FAILURE_ABORTED:
				uifcmsg("Connection Aborted.",	"`Connection Aborted`\n\n"
								"Connection to the remote system aborted by keystroke.");
				break;
			case FAILURE_GENERAL:
				sprintf(str,"Connect error (%d)!",ERROR_VALUE);
				uifcmsg(str
								,"`SyncTERM failed to connect`\n\n"
								 "The call to select() returned an unexpected error code.");
				break;
		}
		conn_close();
		closesocket(sock);
		return(INVALID_SOCKET);
	}
}

void conn_binary_mode_on(void)
{
	if(conn_api.binary_mode_on)
		conn_api.binary_mode_on();
}

void conn_binary_mode_off(void)
{
	if(conn_api.binary_mode_off)
		conn_api.binary_mode_off();
}
