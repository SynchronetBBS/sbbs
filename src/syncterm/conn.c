/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: conn.c,v 1.81 2020/06/27 00:04:49 deuce Exp $ */

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
  #define AI_ADDRCONFIG 0x0400	// Vista or later.
 #endif
 #ifndef AI_NUMERICSERV
  #define AI_NUMERICSERV 0		// No supported by Windows
 #endif
#endif

#include "bbslist.h"
#include "uifcinit.h"
#include "conn.h"
#include "rlogin.h"
#include "raw.h"
#ifndef WITHOUT_CRYPTLIB
#include "ssh.h"
#include "telnets.h"
#endif
#ifndef __HAIKU__
#include "modem.h"
#endif
#ifdef __unix__
#include "conn_pty.h"
#endif
#include "conn_telnet.h"

struct conn_api conn_api;
char *conn_types_enum[]={"Unknown","RLogin","RLoginReversed","Telnet","Raw","SSH","SSHNA","Modem","Serial","Shell","MBBSGhost","TelnetS", NULL};
char *conn_types[]={"Unknown","RLogin","RLogin Reversed","Telnet","Raw","SSH","SSH (no auth)","Modem","Serial","Shell","MBBS GHost","TelnetS",NULL};
short unsigned int conn_ports[]={0,513,513,23,0,22,22,0,0,0,65535,992,0};

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
size_t conn_buf_peek(struct conn_buffer *buf, void *voutbuf, size_t outlen)
{
	unsigned char *outbuf = (unsigned char *)voutbuf;
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
size_t conn_buf_get(struct conn_buffer *buf, void *voutbuf, size_t outlen)
{
	unsigned char *outbuf = (unsigned char *)voutbuf;
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
size_t conn_buf_put(struct conn_buffer *buf, const void *voutbuf, size_t outlen)
{
	const unsigned char *outbuf = (unsigned char *)voutbuf;
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
		pthread_mutex_lock(&(buf->mutex));	/* term.c data_waiting() blocks here, seemingly forever */
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
	if(conn_api.input_thread_running == 1 && conn_api.output_thread_running == 1)
		return(TRUE);
	return(FALSE);
}

int conn_recv_upto(void *vbuffer, size_t buflen, unsigned timeout)
{
	char *buffer = (char *)vbuffer;
	size_t	found=0;
	size_t obuflen;
	void *expanded;
	size_t max_rx = buflen;

	if (conn_api.rx_parse_cb != NULL) {
		if (max_rx > 1)
			max_rx /= 2;
	}
	pthread_mutex_lock(&(conn_inbuf.mutex));
	if(conn_buf_wait_bytes(&conn_inbuf, 1, timeout))
		found=conn_buf_get(&conn_inbuf, buffer, max_rx);
	pthread_mutex_unlock(&(conn_inbuf.mutex));

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

	return(found);
}


int conn_send_raw(const void *vbuffer, size_t buflen, unsigned int timeout)
{
	const char *buffer = vbuffer;
	size_t found;

	pthread_mutex_lock(&(conn_outbuf.mutex));
	found=conn_buf_wait_free(&conn_outbuf, buflen, timeout);
	if(found)
		found=conn_buf_put(&conn_outbuf, buffer, found);
	pthread_mutex_unlock(&(conn_outbuf.mutex));
	return(found);
}

int conn_send(const void *vbuffer, size_t buflen, unsigned int timeout)
{
	const char *buffer = vbuffer;
	size_t found;
	size_t obuflen;
	void *expanded;

	if (conn_api.tx_parse_cb != NULL) {
		expanded = conn_api.tx_parse_cb(buffer, buflen, &obuflen);
	}
	else {
		expanded = (void *)buffer;
		obuflen = buflen;
	}

	pthread_mutex_lock(&(conn_outbuf.mutex));
	found=conn_buf_wait_free(&conn_outbuf, obuflen, timeout);
	if(found)
		found=conn_buf_put(&conn_outbuf, buffer, found);
	pthread_mutex_unlock(&(conn_outbuf.mutex));

	if (conn_api.tx_parse_cb != NULL) {
		free(expanded);
	}

	return(found);
}

int conn_connect(struct bbslist *bbs)
{
	char	str[64];

	memset(&conn_api, 0, sizeof(conn_api));

	conn_api.nostatus = bbs->nostatus;
	conn_api.emulation = get_emulation(bbs);
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
		case CONN_TYPE_MBBS_GHOST:
			conn_api.connect=raw_connect;
			conn_api.close=raw_close;
			break;
#ifndef WITHOUT_CRYPTLIB
		case CONN_TYPE_TELNETS:
			conn_api.connect=telnets_connect;
			conn_api.close=telnets_close;
			conn_api.binary_mode_on=telnet_binary_mode_on;
			conn_api.binary_mode_off=telnet_binary_mode_off;
			break;
		case CONN_TYPE_SSHNA:
		case CONN_TYPE_SSH:
			conn_api.connect=ssh_connect;
			conn_api.close=ssh_close;
			break;
#endif
#ifndef __HAIKU__
		case CONN_TYPE_SERIAL:
			conn_api.connect=modem_connect;
			conn_api.close=serial_close;
			break;
		case CONN_TYPE_MODEM:
			conn_api.connect=modem_connect;
			conn_api.close=modem_close;
			break;
#endif
#ifdef __unix__
		case CONN_TYPE_SHELL:
			conn_api.connect=pty_connect;
			conn_api.close=pty_close;
			break;
#endif
		default:
			sprintf(str,"%s connections not supported.",conn_types[bbs->conn_type]);
			uifcmsg(str,	"`Connection type not supported`\n\n"
							"The connection type of this entry is not supported by this build.\n"
							"Either the protocol was disabled at compile time, or is\n"
							"unsupported on this plattform.");
			conn_api.terminate=1;
	}
	if(conn_api.connect) {
		if(conn_api.connect(bbs)) {
			conn_api.terminate = 1;
			while(conn_api.input_thread_running == 1 || conn_api.output_thread_running == 1)
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
	 FAILURE_WHAT_FAILURE
	,FAILURE_RESOLVE
	,FAILURE_CANT_CREATE
	,FAILURE_CONNECT_ERROR
	,FAILURE_ABORTED
	,FAILURE_DISCONNECTED
};

int conn_socket_connect(struct bbslist *bbs)
{
	SOCKET			sock=INVALID_SOCKET;
	int				nonblock;
	int				failcode=FAILURE_WHAT_FAILURE;
	struct addrinfo	hints;
	struct addrinfo	*res=NULL;
	struct addrinfo	*cur;
	char			portnum[6];
	char str[LIST_ADDR_MAX+40];

	if (!bbs->hidepopups) {
		uifc.pop("Looking up host");
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags=PF_UNSPEC;
	switch(bbs->address_family) {
		case ADDRESS_FAMILY_INET:
			hints.ai_family=PF_INET;
			break;
		case ADDRESS_FAMILY_INET6:
			hints.ai_family=PF_INET6;
			break;
		case ADDRESS_FAMILY_UNSPEC:
		default:
			hints.ai_family=PF_UNSPEC;
			break;
	}
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_protocol=IPPROTO_TCP;
	hints.ai_flags=AI_NUMERICSERV;
#ifdef AI_ADDRCONFIG
	hints.ai_flags|=AI_ADDRCONFIG;
#endif
	sprintf(portnum, "%hu", bbs->port);
	if(getaddrinfo(bbs->addr, portnum, &hints, &res)!=0) {
		failcode=FAILURE_RESOLVE;
		res=NULL;
	}
	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Connecting...");
	}

	/* Drain the input buffer to avoid accidental cancel */
	while(kbhit())
		getch();

	for(cur=res; cur && sock == INVALID_SOCKET && failcode == FAILURE_WHAT_FAILURE; cur=cur->ai_next) {
		if(sock==INVALID_SOCKET) {
			sock=socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
			if(sock==INVALID_SOCKET) {
				failcode=FAILURE_CANT_CREATE;
				break;
			}
			/* Set to non-blocking for the connect */
			nonblock=-1;
			ioctlsocket(sock, FIONBIO, &nonblock);
		}

		if(connect(sock, cur->ai_addr, cur->ai_addrlen)) {
			switch(ERROR_VALUE) {
				case EINPROGRESS:
				case EINTR:
				case EAGAIN:
#if (EAGAIN!=EWOULDBLOCK)
				case EWOULDBLOCK:
#endif
					for(;sock!=INVALID_SOCKET;) {
						if (socket_writable(sock, 1000)) {
							if (socket_recvdone(sock, 0)) {
								closesocket(sock);
								sock=INVALID_SOCKET;
								continue;
							}
							else {
								goto connected;
							}
						}
						else {
							if (kbhit()) {
								failcode = FAILURE_ABORTED;
								closesocket(sock);
								sock = INVALID_SOCKET;
							}
						}
					}

connected:
					break;
				default:
					closesocket(sock);
					sock=INVALID_SOCKET;
					continue;
			}
		}
	}
	if (sock != INVALID_SOCKET) {
		freeaddrinfo(res);
		res=NULL;
		nonblock=0;
		ioctlsocket(sock, FIONBIO, &nonblock);
		if (!socket_recvdone(sock, 0)) {
			int keepalives = TRUE;
			setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepalives, sizeof(keepalives));

			if (!bbs->hidepopups) {
				uifc.pop(NULL);
			}
			return(sock);
		}
		failcode=FAILURE_DISCONNECTED;
	}
	if (failcode == FAILURE_WHAT_FAILURE)
		failcode=FAILURE_CONNECT_ERROR;

	if(res)
		freeaddrinfo(res);
	if (!bbs->hidepopups) {
		uifc.pop(NULL);
	}
	conn_api.terminate=-1;
	if (!bbs->hidepopups) {
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
			case FAILURE_DISCONNECTED:
				sprintf(str,"Connect error (%d)!",ERROR_VALUE);
				uifcmsg(str
								,"`SyncTERM failed to connect`\n\n"
								 "After connect() succeeded, the socket was in a disconnected state.");
				break;
		}
	}
	conn_close();
	if (sock != INVALID_SOCKET)
		closesocket(sock);
	return(INVALID_SOCKET);
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
