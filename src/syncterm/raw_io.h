#ifndef _RAW_IO_H_
#define _RAW_IO_H_

int raw_recv(char *buffer, size_t buflen, unsigned int timeout);
int raw_send(char *buffer, size_t buflen, unsigned int timeout);
int raw_connect(char *addr, int port, char *ruser, char *passwd);
int raw_close(void);

#endif
