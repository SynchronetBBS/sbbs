#include "sockwrap.h"

#include "rlogin.h"
#include "telnet_io.h"
#include "raw_io.h"
#include "conn.h"

static int	con_type=CONN_TYPE_UNKNOWN;
SOCKET conn_socket=INVALID_SOCKET;
char *conn_types[]={"Unknown","RLogin","Telnet","Raw",""};

int conn_recv(char *buffer, size_t buflen)
{
	int retval=-1;

	switch(con_type) {
		case CONN_TYPE_RLOGIN:
			retval=rlogin_recv(buffer, buflen);
			break;
		case CONN_TYPE_TELNET:
			retval=telnet_recv(buffer, buflen);
			break;
		case CONN_TYPE_RAW:
			retval=raw_recv(buffer, buflen);
			break;
	}
	return(retval);
}

int conn_send(char *buffer, size_t buflen, unsigned int timeout)
{
	int retval=-1;

	switch(con_type) {
		case CONN_TYPE_RLOGIN:
			retval=rlogin_send(buffer, buflen, timeout);
			break;
		case CONN_TYPE_TELNET:
			retval=telnet_send(buffer, buflen, timeout);
			break;
		case CONN_TYPE_RAW:
			retval=raw_send(buffer, buflen, timeout);
			break;
	}
	return(retval);
}

int conn_connect(char *addr, int port, char *ruser, char *passwd, int conn_type)
{
	int retval=-1;

	con_type=conn_type;
	switch(con_type) {
		case CONN_TYPE_RLOGIN:
			retval=rlogin_connect(addr, port, ruser, passwd);
			break;
		case CONN_TYPE_TELNET:
			retval=telnet_connect(addr, port, ruser, passwd);
			break;
		case CONN_TYPE_RAW:
			retval=raw_connect(addr, port, ruser, passwd);
			break;
	}
	return(retval);
}

int conn_close(void)
{
	int retval=-1;

	switch(con_type) {
		case CONN_TYPE_RLOGIN:
			retval=rlogin_close();
			break;
		case CONN_TYPE_TELNET:
			retval=telnet_close();
			break;
		case CONN_TYPE_RAW:
			retval=raw_close();
			break;
	}
	con_type=CONN_TYPE_UNKNOWN;
	return(retval);
}
