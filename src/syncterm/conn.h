/* $Id$ */

#ifndef _CONN_H_
#define _CONN_H_

#include "sockwrap.h"
#include "threadwrap.h"

#include "bbslist.h"

extern char *conn_types[];
extern int conn_ports[];

enum {
	 CONN_TYPE_UNKNOWN
	,CONN_TYPE_RLOGIN
	,CONN_TYPE_TELNET
	,CONN_TYPE_RAW
	,CONN_TYPE_SSH
	,CONN_TYPE_TERMINATOR
};

struct conn_api {
	int (*connect)(struct bbslist *bbs);
	int (*close)(void);
	void (*binary_mode_on)(void);
	void (*binary_mode_off)(void);
	int log_level;
	int type;
	int input_thread_running;
	int output_thread_running;
	int terminate;
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
	pthread_mutex_t	mutex;
	sem_t			in_sem;
	sem_t			out_sem;
};

/*
 * Functions for stuff using connections
 */
int conn_recv(char *buffer, size_t buflen, unsigned int timeout);
int conn_peek(char *buffer, size_t buflen);
int conn_send(char *buffer, size_t buflen, unsigned int timeout);
int conn_connect(struct bbslist *bbs);
int conn_close(void);
BOOL conn_connected(void);
BOOL conn_data_waiting(void);
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
size_t conn_buf_peek(struct conn_buffer *buf, unsigned char *outbuf, size_t outlen);
size_t conn_buf_get(struct conn_buffer *buf, unsigned char *outbuf, size_t outlen);
size_t conn_buf_put(struct conn_buffer *buf, const unsigned char *outbuf, size_t outlen);
size_t conn_buf_wait_cond(struct conn_buffer *buf, size_t bcount, unsigned long timeout, int free);
#define conn_buf_wait_bytes(buf, count, timeout)	conn_buf_wait_cond(buf, count, timeout, 0)
#define conn_buf_wait_free(buf, count, timeout)	conn_buf_wait_cond(buf, count, timeout, 1)
int conn_socket_connect(struct bbslist *bbs);

#endif
