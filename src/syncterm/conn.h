#ifndef _CONN_H_
#define _CONN_H_

#include "sockwrap.h"

extern SOCKET conn_socket;
extern char *conn_types[];

enum {
	 CONN_TYPE_UNKNOWN
	,CONN_TYPE_RLOGIN
	,CONN_TYPE_TELNET
	,CONN_TYPE_RAW
	,CONN_TYPE_TERMINATOR
};

int conn_recv(char *buffer, size_t buflen);
int conn_send(char *buffer, size_t buflen, unsigned int timeout);
int conn_connect(char *addr, int port, char *ruser, char *passwd, int conn_type);
int conn_close(void);
void conn_settype(int type);

#endif
