#ifndef _TELNET_IO_H_
#define _TELNET_IO_H_

int telnet_recv(char *buffer, size_t buflen);
int telnet_send(char *buffer, size_t buflen, unsigned int timeout);
int telnet_close(void);
int telnet_connect(char *addr, int port, char *ruser, char *passwd);

#endif
