#ifndef MULTISOCK_H
#define MULTISOCK_H

#include <limits.h>
#include "str_list.h"
#include "wrapdll.h"

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

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT struct xpms_set* DLLCALL xpms_create(unsigned int retries, unsigned int wait_secs,
	int (*lprintf)(int level, const char *fmt, ...));
DLLEXPORT void DLLCALL xpms_destroy(struct xpms_set *xpms_set, void (*sock_destroy)(SOCKET, void *), void *cbdata);
DLLEXPORT BOOL DLLCALL xpms_add(struct xpms_set *xpms_set, int domain, int type,
	int protocol, const char *addr, uint16_t port, const char *prot, 
	void (*sock_init)(SOCKET, void *), int(*bind_init)(BOOL), void *cbdata);
DLLEXPORT BOOL DLLCALL xpms_add_list(struct xpms_set *xpms_set, int domain, int type,
	int protocol, str_list_t list, uint16_t default_port, const char *prot, 
	void (*sock_init)(SOCKET, void *), int(*bind_init)(BOOL), void *cbdata);
DLLEXPORT BOOL DLLCALL xpms_add_chararray_list(struct xpms_set *xpms_set, int domain, int type,
	int protocol, const char *list, uint16_t default_port, const char *prot,
	void (*sock_init)(SOCKET, void *), int(*bind_init)(BOOL), void *cbdata);
DLLEXPORT SOCKET DLLCALL xpms_accept(struct xpms_set *, union xp_sockaddr * addr, 
	socklen_t * addrlen, unsigned int timeout, void **cb_data);

#ifdef __cplusplus
}
#endif

#endif
