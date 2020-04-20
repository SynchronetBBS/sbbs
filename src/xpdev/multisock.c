// Multi-socket versions ofthe socket API...

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gen_defs.h"
#include "sockwrap.h"
#include "dirwrap.h"
#include "multisock.h"
#include <stdarg.h>

struct xpms_set* DLLCALL xpms_create(unsigned int retries, unsigned int wait_secs,
	int (*lprintf)(int level, const char *fmt, ...))
{
	struct xpms_set *ret=(struct xpms_set *)calloc(1, sizeof(struct xpms_set));

	if(ret==NULL)
		return ret;
	ret->retries = retries;
	ret->wait_secs = wait_secs;
	ret->lprintf=lprintf;
	return ret;
}

void DLLCALL xpms_destroy(struct xpms_set *xpms_set, void (*sock_destroy)(SOCKET, void *), void *cbdata)
{
	size_t		i;

	if(!xpms_set)
		return;
	for(i=0; i<xpms_set->sock_count; i++) {
		if(xpms_set->socks[i].sock != INVALID_SOCKET) {
			if(xpms_set->lprintf!=NULL)
				xpms_set->lprintf(LOG_INFO, "%04d %s closing socket %s port %d"
						, xpms_set->socks[i].sock, xpms_set->socks[i].prot?xpms_set->socks[i].prot:"unknown"
						, xpms_set->socks[i].address
						, xpms_set->socks[i].port);
			closesocket(xpms_set->socks[i].sock);
			if(sock_destroy)
				sock_destroy(xpms_set->socks[i].sock, cbdata);
		}
		xpms_set->socks[i].sock = INVALID_SOCKET;
		FREE_AND_NULL(xpms_set->socks[i].address);
		FREE_AND_NULL(xpms_set->socks[i].prot);
	}
	FREE_AND_NULL(xpms_set->socks);
	free(xpms_set);
}

BOOL DLLCALL xpms_add(struct xpms_set *xpms_set, int domain, int type,
	int protocol, const char *addr, uint16_t port, const char *prot, 
	void (*sock_init)(SOCKET, void *), int(*bind_init)(BOOL), void *cbdata)
{
	struct xpms_sockdef	*new_socks;
    struct addrinfo		hints;
    struct addrinfo		*res=NULL;
    struct addrinfo		*cur;
    unsigned int		added = 0;
    int					ret;
    char				port_str[6];

#ifndef _WIN32
	struct addrinfo		dummy;
	struct sockaddr_un	un_addr;

	if(domain == AF_UNIX) {
		memset(&dummy, 0, sizeof(dummy));
		memset(&un_addr, 0, sizeof(un_addr));
		dummy.ai_family = AF_UNIX;
		dummy.ai_socktype = type;
		dummy.ai_addr = (struct sockaddr *)&un_addr;
		un_addr.sun_family=AF_UNIX;

		if(strlen(addr) >= sizeof(un_addr.sun_path)) {
			if(xpms_set->lprintf)
				xpms_set->lprintf(LOG_ERR, "!%s ERROR %s is too long for a portable AF_UNIX socket", prot, addr);
			return FALSE;
		}
		strcpy(un_addr.sun_path,addr);
#ifdef SUN_LEN
		dummy.ai_addrlen = SUN_LEN(&un_addr);
#else
		dummy.ai_addrlen = offsetof(struct sockaddr_un, un_addr.sun_path) + strlen(addr) + 1;
#endif
		if(fexist(addr))
			unlink(addr);
		res = &dummy;
	}
#endif
	if(res == NULL) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_flags=AI_PASSIVE;
		hints.ai_family=domain;
		hints.ai_socktype=type;
		hints.ai_protocol=protocol;
		hints.ai_flags|=AI_NUMERICSERV;
#ifdef AI_ADDRCONFIG
		hints.ai_flags|=AI_ADDRCONFIG;
#endif
		sprintf(port_str, "%hu", port);
		if((ret=getaddrinfo(addr, port_str, &hints, &res))!=0) {
			if(xpms_set->lprintf)
				xpms_set->lprintf(LOG_CRIT, "!%s ERROR %d calling getaddrinfo() on %s", prot, ret, addr);
			return FALSE;
		}
	}

	for(cur=res; cur; cur=cur->ai_next) {
		new_socks=(struct xpms_sockdef *)realloc(xpms_set->socks, sizeof(struct xpms_sockdef)*(xpms_set->sock_count+1));
		if(new_socks==NULL) {
			/* This may be a partial failure */
			if(xpms_set->lprintf)
				xpms_set->lprintf(LOG_CRIT, "!%s ERROR out of memory adding to multisocket", prot);
			break;
		}
		xpms_set->socks=new_socks;
		xpms_set->socks[xpms_set->sock_count].address = strdup(addr);
		xpms_set->socks[xpms_set->sock_count].cb_data = cbdata;
		xpms_set->socks[xpms_set->sock_count].domain = cur->ai_family;	/* Address/Protocol Family */
		xpms_set->socks[xpms_set->sock_count].type = cur->ai_socktype;
		xpms_set->socks[xpms_set->sock_count].protocol = protocol;
		xpms_set->socks[xpms_set->sock_count].port = port;
		xpms_set->socks[xpms_set->sock_count].prot = strdup(prot);
		xpms_set->socks[xpms_set->sock_count].sock = socket(cur->ai_family, cur->ai_socktype, protocol);
		if(xpms_set->socks[xpms_set->sock_count].sock == INVALID_SOCKET) {
			FREE_AND_NULL(xpms_set->socks[xpms_set->sock_count].address);
			FREE_AND_NULL(xpms_set->socks[xpms_set->sock_count].prot);
			continue;
		}
		if(sock_init)
			sock_init(xpms_set->socks[xpms_set->sock_count].sock, cbdata);

		if(bind_init) {
			if(port < IPPORT_RESERVED && port > 0)
				bind_init(FALSE);
		}
		if(retry_bind(xpms_set->socks[xpms_set->sock_count].sock, cur->ai_addr, cur->ai_addrlen, xpms_set->retries, xpms_set->wait_secs, prot, xpms_set->lprintf)==-1) {
			closesocket(xpms_set->socks[xpms_set->sock_count].sock);
			FREE_AND_NULL(xpms_set->socks[xpms_set->sock_count].address);
			FREE_AND_NULL(xpms_set->socks[xpms_set->sock_count].prot);
			if(bind_init) {
				if(port < IPPORT_RESERVED)
					bind_init(TRUE);
			}
			continue;
		}
		if(bind_init) {
			if(port < IPPORT_RESERVED && port > 0)
				bind_init(TRUE);
		}

		if(type != SOCK_DGRAM) {
			if(listen(xpms_set->socks[xpms_set->sock_count].sock, SOMAXCONN)==-1) {
				if(xpms_set->lprintf)
					xpms_set->lprintf(LOG_WARNING, "%04d !%s ERROR %d (%s) listening on port %d"
						,xpms_set->socks[xpms_set->sock_count].sock, prot, ERROR_VALUE
						,socket_strerror(socket_errno), port);
				closesocket(xpms_set->socks[xpms_set->sock_count].sock);
				FREE_AND_NULL(xpms_set->socks[xpms_set->sock_count].address);
				FREE_AND_NULL(xpms_set->socks[xpms_set->sock_count].prot);
				continue;
			}
		}

		added++;
		xpms_set->sock_count++;
	}

#ifndef _WIN32
	if(res != &dummy)
#endif
		freeaddrinfo(res);
	if(added)
		return TRUE;
	return FALSE;
}

BOOL DLLCALL xpms_add_list(struct xpms_set *xpms_set, int domain, int type,
	int protocol, str_list_t list, uint16_t default_port, const char *prot, 
	void (*sock_init)(SOCKET, void *), int(*bind_init)(BOOL), void *cbdata)
{
	char	**iface;
	char	*host;
	char	*host_str;
	char	*p, *p2;
	BOOL	one_good=FALSE;
	
	for(iface=list; iface && *iface; iface++) {
		WORD	port=default_port;

		host=strdup(*iface);

		host_str=host;
		p = strrchr(host, ':');
		/*
		 * If there isn't a [, and the first and last colons aren't the same
		 * it's assumed to be an IPv6 address
		 */
		if(strchr(host,'[')==NULL && p != NULL && strchr(host, ':') != p)
			p=NULL;
		if(host[0]=='[') {
			host_str++;
			p2=strrchr(host,']');
			if(p2)
				*p2=0;
			if(p2 > p)
				p=NULL;
		}
		if(p!=NULL) {
			*(p++)=0;
			sscanf(p, "%hu", &port);
		}
		if(xpms_set->lprintf)
			xpms_set->lprintf(LOG_INFO, "%s listening on socket %s port %hu", prot, host_str, port);
		if(xpms_add(xpms_set, domain, type, protocol, host_str, port, prot, sock_init, bind_init, cbdata))
			one_good=TRUE;
		free(host);
	}
	return one_good;
}

BOOL DLLCALL xpms_add_chararray_list(struct xpms_set *xpms_set, int domain, int type,
	int protocol, const char *list, uint16_t default_port, const char *prot,
	void (*sock_init)(SOCKET, void *), int(*bind_init)(BOOL), void *cbdata)
{
	str_list_t slist;
	BOOL ret;

	slist = strListSplitCopy(NULL, list, ", \t\r\n");
	if (slist == NULL)
		return FALSE;
	ret = xpms_add_list(xpms_set, domain, type, protocol, slist, default_port, prot,
			sock_init, bind_init, cbdata);
	strListFree(&slist);
	return ret;
}

SOCKET DLLCALL xpms_accept(struct xpms_set *xpms_set, union xp_sockaddr * addr, 
	socklen_t * addrlen, unsigned int timeout, void **cb_data)
{
	fd_set			read_fs;
	size_t			i;
	struct timeval	tv;
	struct timeval	*tvp;
	SOCKET			max_sock=0;
	
	FD_ZERO(&read_fs);
	for(i=0; i<xpms_set->sock_count; i++) {
		if(xpms_set->socks[i].sock == INVALID_SOCKET)
			continue;
		FD_SET(xpms_set->socks[i].sock, &read_fs);
		if(xpms_set->socks[i].sock >= max_sock)
			max_sock=xpms_set->socks[i].sock+1;
	}

	if(timeout==XPMS_FOREVER)
		tvp=NULL;
	else {
		tv.tv_sec=timeout/1000;
		tv.tv_usec=(timeout%1000)*1000;
		tvp=&tv;
	}
	switch(select(max_sock, &read_fs, NULL, NULL, tvp)) {
		case 0:
			return INVALID_SOCKET;
		case -1:
			return SOCKET_ERROR;
		default:
			for(i=0; i<xpms_set->sock_count; i++) {
				if(xpms_set->socks[i].sock == INVALID_SOCKET)
					continue;
				if(FD_ISSET(xpms_set->socks[i].sock, &read_fs)) {
					if(cb_data)
						*cb_data=xpms_set->socks[i].cb_data;
					return accept(xpms_set->socks[i].sock, &addr->addr, addrlen);
				}
			}
	}

	return INVALID_SOCKET;
}
