#ifndef MULTISOCK_H
#define MULTISOCK_H

#include <sys/limits.h>

struct xpms_sockdef
{
	void			*cb_data;
	int				domain;
	int				type;
	int				protocol;
	SOCKET			sock;
	char			*address;
	uint16_t		port;
	char			*prot;
};

struct xpms_set {
	struct xpms_sockdef	*socks;
	int					(*lprintf)(int level, const char *fmt, ...);
	size_t				sock_count;
	unsigned int		retries;
	unsigned int		wait_secs;
};

#define XPMS_FOREVER	UINT_MAX

struct xpms_set *xpms_create(unsigned int retries, unsigned int wait_secs,
	int (*lprintf)(int level, const char *fmt, ...));
void xpms_destroy(struct xpms_set *xpms_set);
BOOL xpms_add(struct xpms_set *xpms_set, int domain, int type,
	int protocol, const char *addr, uint16_t port, const char *prot, 
	void (*sock_init)(SOCKET, void *), int(*bind_init)(BOOL), void *cbdata);
SOCKET xpms_accept(struct xpms_set *, struct sockaddr * addr, 
	socklen_t * addrlen, unsigned int timeout, void **cb_data);

#endif
