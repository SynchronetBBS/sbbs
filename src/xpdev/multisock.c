// Multi-socket versions ofthe socket API...

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gen_defs.h"
#include "sockwrap.h"
#include "dirwrap.h"
#include "multisock.h"
#include "haproxy.h"
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
	char				err[128];

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
					xpms_set->lprintf(LOG_WARNING, "%04d !%s ERROR %d listening on port %d: %s"
						,xpms_set->socks[xpms_set->sock_count].sock, prot, ERROR_VALUE
						,port, socket_strerror(socket_errno,err,sizeof(err)));
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

/* Convert a binary variable into a hex string - used for printing in the debug log */
static void btox(char *hexstr, const char *srcbuf, size_t srcbuflen, size_t hexstrlen, int (*lprintf)(int level, const char *fmt, ...)) 
{
	size_t         i;

	if (hexstrlen < srcbuflen*2+1) {
		lprintf(LOG_WARNING,"btox hexstr buffer too small [%d] - not all data will be processed",hexstrlen);
		srcbuflen = hexstrlen/2-1;
	}

	*hexstr = '\0';
	for (i=0;i<srcbuflen;i++) {
		sprintf(hexstr+strlen(hexstr),"%02x",(unsigned char)srcbuf[i]);
	}
}

static BOOL read_socket(SOCKET sock, char *buffer, size_t len, int (*lprintf)(int level, const char *fmt, ...))
{
	size_t            i;
	int            rd;
	unsigned char           ch;
	char           err[128];

	for (i=0;i<len;i++) {
		if (socket_readable(sock, 1000)) {
			rd = recv(sock,&ch,1,0);
			if (rd == 0) {
				lprintf(LOG_WARNING,"%04d multisock read_socket() - remote closed the connection",sock);
				return FALSE;

			} else if (rd == 1) {
				buffer[i] = ch;

			} else {
				lprintf(LOG_WARNING,"%04d multisock read_socket() - failed to read from socket. Got [%d] with error [%s]",sock,rd,socket_strerror(socket_errno,err,sizeof(err)));
				return FALSE;
			}

		} else {
			lprintf(LOG_WARNING,"%04d multisock read_socket() - No data?",sock);
			return FALSE;

		}
	}

	return TRUE;
}

static BOOL read_socket_line(SOCKET sock, char *buffer, size_t buflen, int (*lprintf)(int level, const char *fmt, ...))
{
	size_t         i;

	for (i = 0; i < buflen - 1; i++) {
		if (read_socket(sock, &buffer[i], 1, lprintf)) {
			switch(buffer[i]) {
				case 0:
					return FALSE;
				case '\n':
					buffer[i+1] = 0;
					return TRUE;
			}

		} else {
			buffer[i] = 0;
			return FALSE;
		}
	}

	buffer[i] = 0;
	return FALSE;
}

SOCKET DLLCALL xpms_accept(struct xpms_set *xpms_set, union xp_sockaddr * addr, 
	socklen_t * addrlen, unsigned int timeout, uint32_t flags, void **cb_data)
{
#ifdef _WIN32	// Use select()
	fd_set         read_fs;
	struct timeval tv;
	struct timeval *tvp;
	SOCKET         max_sock=0;
#else	// Use poll()
	struct pollfd *fds;
	int poll_timeout;
	nfds_t scnt = 0;
#endif
	size_t         i;
	SOCKET         ret;
	char           hapstr[128];
	char           haphex[256];
	char           *p, *tok;
	long           l;
	void           *vp;

	if (xpms_set->sock_count < 1)
		return INVALID_SOCKET;

#ifdef _WIN32
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
#else
	fds = calloc(xpms_set->sock_count, sizeof(*fds));
	if (fds == NULL)
		return INVALID_SOCKET;
	for (i = 0; i < xpms_set->sock_count; i++) {
		if (xpms_set->socks[i].sock == INVALID_SOCKET)
			continue;
		fds[scnt].fd = xpms_set->socks[i].sock;
		fds[scnt].events = POLLIN;
		scnt++;
	}

	if (timeout == XPMS_FOREVER)
		poll_timeout = -1;
	else if (timeout > INT_MAX)
		poll_timeout = INT_MAX;
	else
		poll_timeout = timeout;

	switch (poll(fds, scnt, poll_timeout)) {
		case 0:
			free(fds);
			return INVALID_SOCKET;
		case -1:
			free(fds);
			return SOCKET_ERROR;
		default:
			scnt = 0;
			for(i=0; i<xpms_set->sock_count; i++) {
				if(xpms_set->socks[i].sock == INVALID_SOCKET)
					continue;
				if (fds[scnt].revents & (POLLERR | POLLNVAL)) {
					scnt++;
					closesocket(xpms_set->socks[i].sock);
					xpms_set->lprintf(LOG_ERR, "%04d * Listening socket went bad", xpms_set->socks[i].sock);
					xpms_set->socks[i].sock = INVALID_SOCKET;
					continue;
				}
				if (fds[scnt++].revents & POLLIN) {
#endif
					if(cb_data)
						*cb_data=xpms_set->socks[i].cb_data;
					ret = accept(xpms_set->socks[i].sock, &addr->addr, addrlen);
					if (ret == INVALID_SOCKET) {
						goto error_return;
					}

					// Set host_ip from haproxy protocol, if its used
					// http://www.haproxy.org/download/1.8/doc/proxy-protocol.txt
					if (flags & XPMS_ACCEPT_FLAG_HAPROXY) {
						memset(addr, 0, sizeof(*addr));
						xpms_set->lprintf(LOG_DEBUG,"%04d Working out client address from HAProxy PROTO",ret);

						// Read the first line
						// In normal proxy usage, we shouldnt fail here - but if there is a badly implemented HAPROXY PROTO
						// or the user attempts to connect direct to the BBS (not via the proxy) there could be anything 
						// received (IAC sequences, SSH setup, or just badness)
						if (! read_socket_line(ret, hapstr, 108, xpms_set->lprintf)) {
							btox(haphex,hapstr,strlen(hapstr),sizeof(haphex), xpms_set->lprintf);
							xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY looking for version - failed [%s]",ret,haphex);
							closesocket(ret);
							goto error_return;
						}

						btox(haphex,hapstr,strlen(hapstr)>16 ? 16 : strlen(hapstr),sizeof(haphex), xpms_set->lprintf);
						xpms_set->lprintf(LOG_DEBUG,"%04d * HAPROXY looking for version - 1st %d bytes received [%s] of (%d)",ret,strlen(hapstr)>16 ? 16 : strlen(hapstr),haphex,strlen(hapstr));

						// v1 of the protocol uses plain text, starting with "PROXY "
						// eg: "PROXY TCP4 255.255.255.255 255.255.255.255 65535 65535\r\n"
						if (strncmp((char *)hapstr,"PROXY ",6) == 0) {
							xpms_set->lprintf(LOG_DEBUG,"%04d * HAPROXY PROTO v1",ret);

							tok = &hapstr[6];
							// IPV4
							if (strncmp(tok, "TCP4 ", 5) == 0) {
								tok += 5;
								xpms_set->lprintf(LOG_DEBUG,"%04d * HAPROXY Proto TCP4",ret,hapstr);
								addr->addr.sa_family = AF_INET;
								if (addrlen)
									*addrlen = sizeof(struct sockaddr_in);
								vp = &addr->in.sin_addr;
							// IPV6
							} else if (strncmp(tok,"TCP6 ",5) == 0) {
								tok += 5;
								xpms_set->lprintf(LOG_DEBUG,"%04d * HAPROXY Proto TCP6",ret,hapstr);
								addr->addr.sa_family = AF_INET6;
								if (addrlen)
									*addrlen = sizeof(struct sockaddr_in6);
								vp = &addr->in6.sin6_addr;
							// Unknown?
							} else {
								xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY Unknown Protocol",ret);
								closesocket(ret);
								goto error_return;
							}

							// Look for the space between the next IP
							p = strchr(tok, ' ');
							if (p == NULL) {
								xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY Couldnt find IP address",ret);
								closesocket(ret);
								goto error_return;
							}
							*p = 0;
							if (inet_pton(addr->addr.sa_family, tok, vp) != 1) {
								xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY Unable to parse %s address [%s]",addr->addr.sa_family == AF_INET ? "IPv4" : "IPv6", tok);
								closesocket(ret);
								goto error_return;
							}
							tok = p + 1;
							// Look for the space before the port number
							p = strchr(tok, ' ');
							if (p == NULL) {
								xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY Couldnt find port",ret);
								closesocket(ret);
								goto error_return;
							}
							tok = p + 1;
							l = strtol(tok, NULL, 10);
							if (l <= 0 || l > UINT16_MAX) {
								xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY Source port out of range",ret);
								closesocket(ret);
								goto error_return;
							}
							switch(addr->addr.sa_family) {
								case AF_INET:
									addr->in.sin_port = htons((unsigned short)l);
									break;
								case AF_INET6:
									addr->in6.sin6_port = htons((unsigned short)l);
									break;
							}

						// v2 is in binary with the first 12 bytes "\x0D\x0A\x0D\x0A\x00\x0D\x0A\x51\x55\x49\x54\x0A"
						// we'll compare the 1st 6 bytes here, since we are determining if we are v1 or v2.
						} else if (strcmp((char *)hapstr,"\r\n") == 0) {
							xpms_set->lprintf(LOG_DEBUG,"%04d * HAPROXY PROTO v2",ret);

							// OK, just for sanity, our next 10 chars should be v2...
							memset(hapstr, 0, 10);
							if (read_socket(ret,hapstr,10,xpms_set->lprintf)==FALSE || memcmp(hapstr, "\x0D\x0A\x00\x0D\x0A\x51\x55\x49\x54\x0A", 10) != 0) {
								btox(haphex,hapstr,10,sizeof(haphex), xpms_set->lprintf);
								xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY Something went wrong - incomplete v2 setup [%s]",ret,haphex);
								closesocket(ret);
								goto error_return;
							}

							// Command and Version
							if (read_socket(ret,hapstr,1,xpms_set->lprintf)==FALSE) {
								btox(haphex,hapstr,1,sizeof(haphex), xpms_set->lprintf);
								xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY looking for Verson/Command - failed [%s]",ret,haphex);
								closesocket(ret);
								goto error_return;
							}
							xpms_set->lprintf(LOG_DEBUG,"%04d * HAPROXY Version [%x]",ret,(hapstr[0]>>4)&0x0f); //Should be 2

							// Check the version
							switch((hapstr[0]>>4)&0x0f) {
								case 0x02:
									break;
								default:
									xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY invalid version [%x]",ret,(hapstr[0]>>4)&0x0f);
									closesocket(ret);
									goto error_return;
							}

							xpms_set->lprintf(LOG_DEBUG,"%04d * HAPROXY Command [%x]",ret,hapstr[0]&0x0f); //0=Local/1=Proxy

							// Check the command
							switch(hapstr[0]&0x0f) {
								case HAPROXY_LOCAL:
									xpms_set->lprintf(LOG_INFO,"%04d * HAPROXY health check - we are alive!",ret);
									closesocket(ret);
									goto error_return;
								case HAPROXY_PROXY:
									break;
								default:
									xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY invalid command [%x]",ret,hapstr[0]&0x0f);
									closesocket(ret);
									goto error_return;
							}

							// Protocol and Family
							if (read_socket(ret,hapstr,1,xpms_set->lprintf)==FALSE) {
								btox(haphex,hapstr,1,sizeof(haphex), xpms_set->lprintf);
								xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY looking for Protocol/Family - failed [%s]",ret,haphex);
								closesocket(ret);
								goto error_return;
							}
							xpms_set->lprintf(LOG_DEBUG,"%04d * HAPROXY Protocol [%x]",ret,hapstr[0]&0x0f); //0=Unspec/1=AF_INET/2=AF_INET6/3=AF_UNIX
							l = (hapstr[0]>>4)&0x0f;
							xpms_set->lprintf(LOG_DEBUG,"%04d * HAPROXY Family [%x]",ret,l); //0=UNSPEC/1=STREAM/2=DGRAM

							// Address Length - 2 bytes
							if (read_socket(ret,hapstr,2,xpms_set->lprintf)==FALSE) {
								btox(haphex,hapstr,2,sizeof(haphex), xpms_set->lprintf);
								xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY looking for address length - failed [%s]",ret,haphex);
								closesocket(ret);
								goto error_return;
							}
							i = ntohs(*(uint16_t*)hapstr);
							xpms_set->lprintf(LOG_DEBUG,"%04d * HAPROXY Address Length [%d]",ret,i);

							switch (l) {
								// IPv4 - AF_INET
								case HAPROXY_AFINET:
									if (i != 12) {
										xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY Something went wrong - IPv4 address length is incorrect",ret);
										closesocket(ret);
										goto error_return;
									}
									addr->in.sin_family = AF_INET;
									if (read_socket(ret, hapstr, i, xpms_set->lprintf)==FALSE) {
										xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY looking for IPv4 address - failed",ret);
										closesocket(ret);
										goto error_return;
									}
									memcpy(&addr->in.sin_addr.s_addr, hapstr, 4);
									memcpy(&addr->in.sin_port, &hapstr[8], 2);
									if (addrlen)
										*addrlen = sizeof(struct sockaddr_in);

									break;

								// IPv6 - AF_INET6
								case HAPROXY_AFINET6:
									if (i != 36) {
										xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY Something went wrong - IPv6 address length is incorrect.",ret);
										closesocket(ret);
										goto error_return;
									}
									addr->in6.sin6_family = AF_INET6;
									if (read_socket(ret,hapstr,i,xpms_set->lprintf)==FALSE) {
										xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY looking for IPv6 address - failed",ret);
										closesocket(ret);
										goto error_return;
									}
									memcpy(&addr->in6.sin6_addr.s6_addr, hapstr, 16);
									memcpy(&addr->in6.sin6_port, &hapstr[32], 2);
									if (addrlen)
										*addrlen = sizeof(struct sockaddr_in6);

									break;

								default:
									xpms_set->lprintf(LOG_ERR,"%04d * HAPROXY Unknown Family [%x]",ret,l);
									closesocket(ret);
									goto error_return;
							}

						} else {
							xpms_set->lprintf(LOG_ERR,"%04d Unknown HAProxy Initialisation - is HAProxy used?",ret);
							closesocket(ret);
							goto error_return;
						}

						hapstr[0] = 0;
						inet_addrtop(addr, hapstr, sizeof(hapstr));
						xpms_set->lprintf(LOG_INFO,"%04d * HAPROXY Source [%s]",ret,hapstr);

#ifndef _WIN32
						free(fds);
#endif
						return ret;
					} else {
#ifndef _WIN32
						free(fds);
#endif
						return ret;
					}
				}
			}
	}

error_return:
#ifndef _WIN32
	free(fds);
#endif
	return INVALID_SOCKET;
}
