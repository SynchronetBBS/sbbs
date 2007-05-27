/* $Id$ */

#include <stdlib.h>

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
#include "conn_telnet.h"

struct conn_api conn_api;
char *conn_types[]={"Unknown","RLogin","Telnet","Raw","SSH","Modem",NULL};
int conn_ports[]={0,513,23,0,22,0};

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
		free(buf->buf);
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
	if(buf->buftop >= buf->bufbot)
		return(buf->buftop-buf->bufbot);
	return(buf->bufsize-buf->bufbot + buf->buftop);
}

size_t conn_buf_free(struct conn_buffer *buf)
{
	if(buf->buftop >= buf->bufbot)
		return(buf->bufsize-buf->buftop-buf->bufbot);
	return(buf->bufbot - buf->buftop);
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
	chunk=buf->bufsize-buf->bufbot;
	if(chunk > copy_bytes)
		chunk=copy_bytes;

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
	size_t loop;

	ret=conn_buf_peek(buf, outbuf, outlen);
	buf->bufbot+=ret;
	if(buf->bufbot >= buf->bufsize)
		buf->bufbot -= buf->bufsize;
	sem_post(&(buf->out_sem));
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
	size_t loop;

	write_bytes=buf->bufsize-conn_buf_bytes(buf);
	if(write_bytes > outlen)
		write_bytes = outlen;
	chunk=buf->bufsize-buf->buftop;
	if(chunk > write_bytes)
		chunk=write_bytes;
	memcpy(buf->buf+buf->buftop, outbuf, chunk);
	if(chunk < write_bytes)
		memcpy(buf->buf, outbuf+chunk, write_bytes-chunk);
	buf->buftop+=write_bytes;
	if(buf->buftop >= buf->bufsize)
		buf->buftop -= buf->bufsize;
	sem_post(&(buf->in_sem));
	return(write_bytes);
}

/*
 * Waits up to timeout milliseconds for bcount bytes to be available/free
 * in the buffer.
 */
size_t conn_buf_wait_cond(struct conn_buffer *buf, size_t bcount, unsigned long timeout, int free)
{
	long double now;
	long double end;
	size_t found;
	size_t loop;
	unsigned long timeleft;
	int retnow=0;
	sem_t	*sem;
	size_t (*cond)(struct conn_buffer *buf);
	
	if(free) {
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
	}
	if(conn_api.connect) {
		conn_api.connect(bbs);
		while(conn_api.terminate == 0 && (conn_api.input_thread_running == 0 || conn_api.output_thread_running == 0))
			SLEEP(1);
	}
	return(conn_api.terminate);
}

BOOL conn_data_waiting(void)
{
	size_t found;

	pthread_mutex_lock(&(conn_inbuf.mutex));
	found=conn_buf_bytes(&conn_inbuf);
	pthread_mutex_unlock(&(conn_inbuf.mutex));
	if(found)
		return(TRUE);
	return(FALSE);
}

int conn_close(void)
{
	if(conn_api.close)
		return(conn_api.close());
	return(0);
}

int conn_socket_connect(struct bbslist *bbs)
{
	SOCKET	sock;
	HOSTENT *ent;
	SOCKADDR_IN	saddr;
	char	*p;
	unsigned int	neta;

	for(p=bbs->addr;*p;p++)
		if(*p!='.' && !isdigit(*p))
			break;

	if(!(*p))
		neta=inet_addr(bbs->addr);
	else {
		uifc.pop("Lookup up host");
		if((ent=gethostbyname(bbs->addr))==NULL) {
			char str[LIST_ADDR_MAX+17];

			uifc.pop(NULL);
			sprintf(str,"Cannot resolve %s!",bbs->addr);
			uifcmsg(str,	"`Cannot Resolve Host`\n\n"
							"The system is unable to resolve the hostname... double check the spelling.\n"
							"If it's not an issue with your DNS settings, the issue is probobly\n"
							"with the DNS settings of the system you are trying to contact.");
			conn_api.terminate=-1;
			return(INVALID_SOCKET);
		}
		neta=*((unsigned int*)ent->h_addr_list[0]);
		uifc.pop(NULL);
	}
	uifc.pop("Connecting...");

	sock=socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
	if(sock==INVALID_SOCKET) {
		uifc.pop(NULL);
		uifcmsg("Cannot create socket!",	"`Unable to create socket`\n\n"
											"Your system is either dangerously low on resources, or there"
											"is a problem with your TCP/IP stack.");
		conn_api.terminate=-1;
		return(INVALID_SOCKET);
	}
	memset(&saddr,0,sizeof(saddr));
	saddr.sin_addr.s_addr = neta;
	saddr.sin_family = AF_INET;
	saddr.sin_port   = htons((WORD)bbs->port);

	if(connect(sock, (struct sockaddr *)&saddr, sizeof(saddr))) {
		char str[LIST_ADDR_MAX+20];

		conn_close();
		uifc.pop(NULL);
		sprintf(str,"Cannot connect to %s!",bbs->addr);
		uifcmsg(str,	"`Unable to connect`\n\n"
						"Cannot connect to the remote system... it is down or unreachable.");
		conn_api.terminate=-1;
		return(INVALID_SOCKET);
	}

	return(sock);
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
