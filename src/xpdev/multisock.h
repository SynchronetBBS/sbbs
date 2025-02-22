#ifndef MULTISOCK_H
#define MULTISOCK_H

#include <limits.h>
#include "str_list.h"
#include "wrapdll.h"
#include "gen_defs.h" // bool
#include "sockwrap.h" // SOCKET

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
#define XPMS_FLAGS_NONE 0
#define XPMS_ACCEPT_FLAG_HAPROXY	(1<<0)

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT struct xpms_set* xpms_create(unsigned int retries, unsigned int wait_secs,
	int (*lprintf)(int level, const char *fmt, ...));
DLLEXPORT void xpms_destroy(struct xpms_set *xpms_set, void (*sock_destroy)(SOCKET, void *), void *cbdata);
DLLEXPORT bool xpms_add(struct xpms_set *xpms_set, int domain, int type,
	int protocol, const char *addr, uint16_t port, const char *prot, bool* terminated,
	void (*sock_init)(SOCKET, void *), bool(*bind_init)(bool), void *cbdata);
DLLEXPORT bool xpms_add_list(struct xpms_set *xpms_set, int domain, int type,
	int protocol, str_list_t list, uint16_t default_port, const char *prot, bool* terminated,
	void (*sock_init)(SOCKET, void *), bool(*bind_init)(bool), void *cbdata);
DLLEXPORT bool xpms_add_chararray_list(struct xpms_set *xpms_set, int domain, int type,
	int protocol, const char *list, uint16_t default_port, const char *prot, bool* terminated,
	void (*sock_init)(SOCKET, void *), bool(*bind_init)(bool), void *cbdata);
DLLEXPORT SOCKET xpms_accept(struct xpms_set *, union xp_sockaddr * addr,
	socklen_t * addrlen, unsigned int timeout, uint32_t flags, void **cb_data);

#ifdef __cplusplus
}
#endif

#endif
