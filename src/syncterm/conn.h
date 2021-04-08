/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: conn.h,v 1.27 2020/06/27 00:04:49 deuce Exp $ */

#ifndef _CONN_H_
#define _CONN_H_

#include "sockwrap.h"
#include "threadwrap.h"

#include "bbslist.h"

extern char *conn_types[];
extern char *conn_types_enum[];
extern short unsigned int conn_ports[];

enum {
	 CONN_TYPE_UNKNOWN
	,CONN_TYPE_RLOGIN
	,CONN_TYPE_RLOGIN_REVERSED
	,CONN_TYPE_TELNET
	,CONN_TYPE_RAW
	,CONN_TYPE_SSH
	,CONN_TYPE_MODEM
	,CONN_TYPE_SERIAL
	,CONN_TYPE_SHELL
	,CONN_TYPE_MBBS_GHOST
	,CONN_TYPE_TERMINATOR
};

struct conn_api {
	int (*connect)(struct bbslist *bbs);
	int (*close)(void);
	void (*binary_mode_on)(void);
	void (*binary_mode_off)(void);
	void *(*rx_parse_cb)(const void* inbuf, size_t inlen, size_t *olen);
	void *(*tx_parse_cb)(const void* inbuf, size_t inlen, size_t *olen);
	int log_level;
	int type;
	int nostatus;
	cterm_emulation_t emulation;
	volatile int input_thread_running;
	volatile int output_thread_running;
	volatile int terminate;
	unsigned char *rd_buf;
	size_t rd_buf_size;
	unsigned char *wr_buf;
	size_t wr_buf_size;
};

struct conn_buffer {
	unsigned char	*buf;
	size_t			bufsize;
	size_t			buftop;
	size_t			bufbot;
	int				isempty;
	pthread_mutex_t	mutex;
	sem_t			in_sem;
	sem_t			out_sem;
};

/*
 * Functions for stuff using connections
 */
int conn_recv_upto(void *buffer, size_t buflen, unsigned int timeout);
int conn_send(const void *buffer, size_t buflen, unsigned int timeout);
int conn_send_raw(const void *buffer, size_t buflen, unsigned int timeout);
int conn_connect(struct bbslist *bbs);
int conn_close(void);
BOOL conn_connected(void);
size_t conn_data_waiting(void);
void conn_binary_mode_on(void);
void conn_binary_mode_off(void);

/*
 * For connection providers
 */

#define BUFFER_SIZE	16384

extern struct conn_buffer conn_inbuf;
extern struct conn_buffer conn_outbuf;
extern struct conn_api conn_api;

struct conn_buffer *create_conn_buf(struct conn_buffer *buf, size_t size);
void destroy_conn_buf(struct conn_buffer *buf);
size_t conn_buf_bytes(struct conn_buffer *buf);
size_t conn_buf_peek(struct conn_buffer *buf, void *voutbuf, size_t outlen);
size_t conn_buf_get(struct conn_buffer *buf, void *outbuf, size_t outlen);
size_t conn_buf_put(struct conn_buffer *buf, const void *outbuf, size_t outlen);
size_t conn_buf_wait_cond(struct conn_buffer *buf, size_t bcount, unsigned long timeout, int do_free);
#define conn_buf_wait_bytes(buf, count, timeout)	conn_buf_wait_cond(buf, count, timeout, 0)
#define conn_buf_wait_free(buf, count, timeout)	conn_buf_wait_cond(buf, count, timeout, 1)
int conn_socket_connect(struct bbslist *bbs);

#endif
