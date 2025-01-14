/* Berkley/WinSock socket API wrappers */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <stdlib.h>     /* alloca/free on FreeBSD */
#include <string.h>     /* bzero (for FD_ZERO) on FreeBSD */
#include <errno.h>      /* ENOMEM */
#include <stdio.h>      /* SEEK_SET */
#include <string.h>
#if defined(_WIN32)
 #include <malloc.h>    /* alloca() on Win32 */
#endif

#include "genwrap.h"    /* SLEEP */
#include "gen_defs.h"   /* bool/LOG_WARNING */
#include "sockwrap.h"   /* sendsocket */
#include "filewrap.h"   /* filelength */

static socket_option_t socket_options[] = {
	{ "TYPE",               0,              SOL_SOCKET,     SO_TYPE             },
	{ "ERROR",              0,              SOL_SOCKET,     SO_ERROR            },
	{ "DEBUG",              0,              SOL_SOCKET,     SO_DEBUG            },
	{ "LINGER",             SOCK_STREAM,    SOL_SOCKET,     SO_LINGER           },
	{ "SNDBUF",             0,              SOL_SOCKET,     SO_SNDBUF           },
	{ "RCVBUF",             0,              SOL_SOCKET,     SO_RCVBUF           },

#ifndef _WINSOCKAPI_    /* Defined, but not supported, by WinSock */
	{ "SNDLOWAT",           0,              SOL_SOCKET,     SO_SNDLOWAT         },
	{ "RCVLOWAT",           0,              SOL_SOCKET,     SO_RCVLOWAT         },
	{ "SNDTIMEO",           0,              SOL_SOCKET,     SO_SNDTIMEO         },
	{ "RCVTIMEO",           0,              SOL_SOCKET,     SO_RCVTIMEO         },
#ifdef SO_USELOOPBACK   /* SunOS */
	{ "USELOOPBACK",        0,              SOL_SOCKET,     SO_USELOOPBACK      },
#endif
#endif

	{ "REUSEADDR",          0,              SOL_SOCKET,     SO_REUSEADDR        },
#ifdef SO_REUSEPORT /* BSD */
	{ "REUSEPORT",          0,              SOL_SOCKET,     SO_REUSEPORT        },
#endif
#ifdef SO_EXCLUSIVEADDRUSE /* WinSock */
	{ "EXCLUSIVEADDRUSE",   0,              SOL_SOCKET,     SO_EXCLUSIVEADDRUSE },
#endif
	{ "KEEPALIVE",          SOCK_STREAM,    SOL_SOCKET,     SO_KEEPALIVE        },
	{ "DONTROUTE",          0,              SOL_SOCKET,     SO_DONTROUTE        },
	{ "BROADCAST",          SOCK_DGRAM,     SOL_SOCKET,     SO_BROADCAST        },
	{ "OOBINLINE",          SOCK_STREAM,    SOL_SOCKET,     SO_OOBINLINE        },

#ifdef SO_ACCEPTCONN
	{ "ACCEPTCONN",         SOCK_STREAM,    SOL_SOCKET,     SO_ACCEPTCONN       },
#endif
#ifdef SO_PRIORITY      /* Linux */
	{ "PRIORITY",           0,              SOL_SOCKET,     SO_PRIORITY         },
#endif
#ifdef SO_NO_CHECK      /* Linux */
	{ "NO_CHECK",           0,              SOL_SOCKET,     SO_NO_CHECK         },
#endif
#ifdef SO_PROTOTYPE     /* SunOS */
	{ "PROTOTYPE",          0,              SOL_SOCKET,     SO_PROTOTYPE        },
#endif
#ifdef SO_MAX_MSG_SIZE  /* WinSock2 */
	{ "MAX_MSG_SIZE",       SOCK_DGRAM,     SOL_SOCKET,     SO_MAX_MSG_SIZE     },
#endif
#ifdef SO_CONNECT_TIME  /* WinSock2 */
	{ "CONNECT_TIME",       SOCK_STREAM,    SOL_SOCKET,     SO_CONNECT_TIME     },
#endif

	/* IPPROTO-level socket options */
	{ "TCP_NODELAY",        SOCK_STREAM,    IPPROTO_TCP,    TCP_NODELAY         },
	/* The following are platform-specific */
#ifdef TCP_MAXSEG
	{ "TCP_MAXSEG",         SOCK_STREAM,    IPPROTO_TCP,    TCP_MAXSEG          },
#endif
#ifdef TCP_CORK
	{ "TCP_CORK",           SOCK_STREAM,    IPPROTO_TCP,    TCP_CORK            },
#endif
#ifdef TCP_KEEPIDLE
	{ "TCP_KEEPIDLE",       SOCK_STREAM,    IPPROTO_TCP,    TCP_KEEPIDLE        },
#endif
#ifdef TCP_KEEPINTVL
	{ "TCP_KEEPINTVL",      SOCK_STREAM,    IPPROTO_TCP,    TCP_KEEPINTVL       },
#endif
#ifdef TCP_KEEPCNT
	{ "TCP_KEEPCNT",        SOCK_STREAM,    IPPROTO_TCP,    TCP_KEEPCNT         },
#endif
#ifdef TCP_KEEPALIVE    /* SunOS */
	{ "TCP_KEEPALIVE",      SOCK_STREAM,    IPPROTO_TCP,    TCP_KEEPALIVE       },
#endif
#ifdef TCP_SYNCNT
	{ "TCP_SYNCNT",         SOCK_STREAM,    IPPROTO_TCP,    TCP_SYNCNT          },
#endif
#ifdef TCP_LINGER2
	{ "TCP_LINGER2",        SOCK_STREAM,    IPPROTO_TCP,    TCP_LINGER2         },
#endif
#ifdef TCP_DEFER_ACCEPT
	{ "TCP_DEFER_ACCEPT",   SOCK_STREAM,    IPPROTO_TCP,    TCP_DEFER_ACCEPT    },
#endif
#ifdef TCP_WINDOW_CLAMP
	{ "TCP_WINDOW_CLAMP",   SOCK_STREAM,    IPPROTO_TCP,    TCP_WINDOW_CLAMP    },
#endif
#ifdef TCP_QUICKACK
	{ "TCP_QUICKACK",       SOCK_STREAM,    IPPROTO_TCP,    TCP_QUICKACK        },
#endif
#ifdef TCP_NOPUSH
	{ "TCP_NOPUSH",         SOCK_STREAM,    IPPROTO_TCP,    TCP_NOPUSH          },
#endif
#ifdef TCP_NOOPT
	{ "TCP_NOOPT",          SOCK_STREAM,    IPPROTO_TCP,    TCP_NOOPT           },
#endif
#if defined(IPV6_V6ONLY) && defined(IPPROTO_IPV6)
	{ "IPV6_V6ONLY",        0,              IPPROTO_IPV6,   IPV6_V6ONLY         },
#endif
	{ NULL }
};

int getSocketOptionByName(const char* name, int* level)
{
	int i;

	if (level != NULL)
		*level = SOL_SOCKET; /* default option level */
	for (i = 0; socket_options[i].name; i++) {
		if (stricmp(name, socket_options[i].name) == 0) {
			if (level != NULL)
				*level = socket_options[i].level;
			return socket_options[i].value;
		}
	}
	if (!IS_DIGIT(*name))    /* unknown option name */
		return -1;
	return strtol(name, NULL, 0);
}

socket_option_t* getSocketOptionList(void)
{
	return socket_options;
}

/* Return true if connected, optionally sets *rd_p to true if read data available */
/*
 * The exact conditions where rd_p is set to true and the return value
 * is true or false are complex, but the intent appears to be as follows:
 *
 * If the remote has half-closed the socket, rd_p should be false and
 * the function should return false, unless rd_p is NULL in which case
 * the function should return true.  wr_p will indicate that transmit
 * buffers are available.
 *
 * If we have half-closed the socket, wr_p should be true, the function
 * should return true, and rd_p will indicate if there is data available
 * to be received.
 *
 * If the socket is completely closed, wr_p should be true, rd_p should be
 * false, and the function should return false, unless rd_p is NULL in which
 * case, the function should return true.
 *
 * When the function is open in both directions, wr_p will indicate transmit
 * buffers are available, rd_p will indicate data is available to be recv()ed
 * and the return value should be true.
 *
 * If the socket is invalid, rd_p should be false, wr_p should be false, and
 * the function should return false.
 *
 * These rules have various exceptions when errors are returned by select(),
 * poll(), or recv(), which will generally cause a false return with rd_p
 * being false and wr_p being false if select/poll failed, or indicating
 * available write buffers otherwise.
 */
bool socket_check(SOCKET sock, bool* rd_p, bool* wr_p, DWORD timeout)
{
#ifdef PREFER_POLL
	struct pollfd pfd = {0};
	int           j, rd;
	char          ch;

	if (rd_p != NULL)
		*rd_p = false;

	if (wr_p != NULL)
		*wr_p = false;

	if (sock == INVALID_SOCKET)
		return false;

	pfd.fd = sock;
	pfd.events = POLLIN | POLLHUP;
	if (wr_p != NULL)
		pfd.events |= POLLOUT;

	j = poll(&pfd, 1, timeout);

	if (j == 0)
		return true;

	if (j == 1) {
		if (wr_p != NULL && (pfd.revents & POLLOUT)) {
			*wr_p = true;
			if (rd_p == NULL)
				return true;
		}

		if (pfd.revents & (POLLERR | POLLNVAL | POLLHUP))
			return false;

		if (pfd.revents & ~(POLLOUT) && (rd_p != NULL || wr_p == NULL))  {
			rd = recv(sock, &ch, 1, MSG_PEEK);
			if (rd == 1 || (rd == SOCKET_ERROR && SOCKET_ERRNO == EMSGSIZE)) {
				if (rd_p != NULL)
					*rd_p = true;
				return true;
			}
		}
		return false;
	}

	if (j == -1) {
		if (errno == EINTR || errno == ENOMEM)
			return true;
	}

	return false;
#else
	char            ch;
	int             i, rd;
	fd_set          rd_set;
	fd_set*         rd_set_p = &rd_set;
	fd_set          wr_set;
	fd_set*         wr_set_p = NULL;
	struct  timeval tv;

	if (rd_p != NULL)
		*rd_p = false;

	if (wr_p != NULL)
		*wr_p = false;

	if (sock == INVALID_SOCKET)
		return false;

	FD_ZERO(&rd_set);
	FD_SET(sock, &rd_set);
	if (wr_p != NULL) {
		wr_set_p = &wr_set;
		FD_ZERO(wr_set_p);
		FD_SET(sock, wr_set_p);
		if (rd_p == NULL)
			rd_set_p = NULL;
	}

	/* Convert timeout from ms to sec/usec */
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	i = select(sock + 1, rd_set_p, wr_set_p, NULL, &tv);
	if (i == SOCKET_ERROR)
		return false;

	if (i == 0)
		return true;

	if (wr_p != NULL && FD_ISSET(sock, wr_set_p)) {
		*wr_p = true;
		if (i == 1)
			return true;
	}

	if (rd_p != NULL || wr_p == NULL)  {
		rd = recv(sock, &ch, 1, MSG_PEEK);
		if (rd == 1
		    || (rd == SOCKET_ERROR && SOCKET_ERRNO == EMSGSIZE)) {
			if (rd_p != NULL)
				*rd_p = true;
			return true;
		}
	}

	return false;
#endif
}

/*
 * Return true if recv() will not block on socket
 * Will block for timeout ms or forever if timeout is negative
 *
 * This means it will return true if recv() will return an error
 * as well as if the socket is closed (and recv() will return 0)
 */
bool socket_readable(SOCKET sock, int timeout)
{
#ifdef PREFER_POLL
	struct pollfd pfd = {0};
	pfd.fd = sock;
	pfd.events = POLLIN;

	if (poll(&pfd, 1, timeout) == 1)
		return true;
	return false;
#else
	fd_set          rd_set;
	struct timeval  tv = {0};
	struct timeval *tvp = &tv;

	FD_ZERO(&rd_set);
	FD_SET(sock, &rd_set);
	if (timeout < 0)
		tvp = NULL;
	else {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
	}

	switch (select(sock + 1, &rd_set, NULL, NULL, tvp)) {
		case 0:     // Nothing to read
			return false;
		case 1:
			return true;
	}
	// Errors and unexpected cases
	return true;
#endif
}

/*
 * Return true if send() will not block on socket
 * Will block for timeout ms or forever if timeout is negative
 *
 * This means it will return true if send() will return an error
 * as well as if the socket is closed (and send() will return 0)
 */
bool socket_writable(SOCKET sock, int timeout)
{
#ifdef PREFER_POLL
	struct pollfd pfd = {0};
	pfd.fd = sock;
	pfd.events = POLLOUT;

	if (poll(&pfd, 1, timeout) == 1)
		return true;
	return false;
#else
	fd_set          wr_set;
	struct timeval  tv = {0};
	struct timeval *tvp = &tv;

	FD_ZERO(&wr_set);
	FD_SET(sock, &wr_set);
	if (timeout < 0)
		tvp = NULL;
	else {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
	}

	switch (select(sock + 1, NULL, &wr_set, NULL, tvp)) {
		case 0:     // Nothing to read
			return false;
		case 1:
			return true;
	}
	// Errors and unexpected cases
	return true;
#endif
}

/*
 * Return true if recv() will not block and will return zero
 * or an error. This is *not* a test if a socket is
 * disconnected, but rather that it is disconnected *AND* all
 * data has been recv()ed.
 */
bool socket_recvdone(SOCKET sock, int timeout)
{
#ifdef PREFER_POLL
	struct pollfd pfd = {0};
	pfd.fd = sock;
	pfd.events = POLLIN;
	char          ch;
	int           rd;

	switch (poll(&pfd, 1, timeout)) {
		case 1:
			if (pfd.revents) {
				rd = recv(sock, &ch, 1, MSG_PEEK);
				if (rd == 1 || (rd == SOCKET_ERROR && SOCKET_ERRNO == EMSGSIZE))
					return false;
				return true;
			}
			return false;
		case -1:
			if (errno == EINTR || errno == ENOMEM)
				return false;
			return true;
	}
	return false;
#else
	fd_set          rd_set;
	struct timeval  tv = {0};
	struct timeval *tvp = &tv;
	char            ch;
	int             rd;

	FD_ZERO(&rd_set);
	FD_SET(sock, &rd_set);
	if (timeout < 0)
		tvp = NULL;
	else {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
	}

	switch (select(sock + 1, &rd_set, NULL, NULL, tvp)) {
		case -1:    // Error, call this disconnected
			return true;
		case 0:     // Nothing to read
			return false;
	}
	rd = recv(sock, &ch, 1, MSG_PEEK);
	if (rd == 1 || (rd == SOCKET_ERROR && SOCKET_ERRNO == EMSGSIZE))
		return false;
	return true;
#endif
}

int retry_bind(SOCKET s, const struct sockaddr *addr, socklen_t addrlen
               , uint retries, uint wait_secs
               , const char* prot
               , int (*lprintf)(int level, const char *fmt, ...))
{
	char port_str[128];
	char err[256];
	int  result = -1;
	uint i;

	if (addr->sa_family == AF_INET)
		SAFEPRINTF(port_str, " to port %u", ntohs(((SOCKADDR_IN *)(addr))->sin_port));
	else
		port_str[0] = 0;
	for (i = 0; i <= retries; i++) {
		if ((result = bind(s, addr, addrlen)) == 0)
			break;
		if (lprintf != NULL)
			lprintf(i < retries ? LOG_WARNING:LOG_CRIT
			        , "%04d !ERROR %d binding %s socket%s: %s", s, SOCKET_ERRNO, prot, port_str, SOCKET_STRERROR(err, sizeof(err)));
		if (i < retries) {
			if (lprintf != NULL)
				lprintf(LOG_WARNING, "%04d Will retry in %u seconds (%u of %u)"
				        , s, wait_secs, i + 1, retries);
			SLEEP(wait_secs * 1000);
		}
	}
	return result;
}

int nonblocking_connect(SOCKET sock, struct sockaddr* addr, size_t size, unsigned timeout)
{
	int       result;
	socklen_t optlen;

	result = connect(sock, addr, size);

	if (result == SOCKET_ERROR) {
		result = SOCKET_ERRNO;
		if (result == EWOULDBLOCK || result == EINPROGRESS) {
			if (socket_writable(sock, timeout * 1000)) {
				result = 0;
			}
			else {
				optlen = sizeof(result);
				if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)&result, &optlen) == SOCKET_ERROR)
					result = SOCKET_ERRNO;
			}
		}
	}
	return result;
}


union xp_sockaddr* inet_ptoaddr(const char *addr_str, union xp_sockaddr *addr, size_t size)
{
	struct addrinfo  hints = {0};
	struct addrinfo *res, *cur;

	hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;
	if (getaddrinfo(addr_str, NULL, &hints, &res))
		return NULL;

	for (cur = res; cur; cur = cur->ai_next) {
		if (cur->ai_addr->sa_family == AF_INET6)
			break;
		if (cur->ai_addr->sa_family == AF_INET)
			break;
	}
	if (!cur) {
		freeaddrinfo(res);
		return NULL;
	}
	if (size < sizeof(struct sockaddr_in6)) {
		freeaddrinfo(res);
		return NULL;
	}
	size = sizeof(struct sockaddr_in6);
	memcpy(addr, ((struct sockaddr_in6 *)(cur->ai_addr)), size);
	freeaddrinfo(res);
	return addr;
}

const char* inet_addrtop(union xp_sockaddr *addr, char *dest, size_t size)
{
#ifdef _WIN32
	if (getnameinfo(&addr->addr, xp_sockaddr_len(addr), dest, size, NULL, 0, NI_NUMERICHOST))
		safe_snprintf(dest, size, "<Error %u converting address, family=%u>", WSAGetLastError(), addr->addr.sa_family);
	return dest;
#else
	switch (addr->addr.sa_family) {
		case AF_INET:
			return inet_ntop(addr->in.sin_family, &addr->in.sin_addr, dest, size);
		case AF_INET6:
			return inet_ntop(addr->in6.sin6_family, &addr->in6.sin6_addr, dest, size);
		case AF_UNIX:
			strncpy(dest, addr->un.sun_path, size);
			dest[size - 1] = 0;
			return dest;
		default:
			safe_snprintf(dest, size, "<unknown address family: %u>", addr->addr.sa_family);
			return NULL;
	}
#endif
}

uint16_t inet_addrport(union xp_sockaddr *addr)
{
	switch (addr->addr.sa_family) {
		case AF_INET:
			return ntohs(addr->in.sin_port);
		case AF_INET6:
			return ntohs(addr->in6.sin6_port);
		default:
			return 0;
	}
}

void inet_setaddrport(union xp_sockaddr *addr, uint16_t port)
{
	switch (addr->addr.sa_family) {
		case AF_INET:
			addr->in.sin_port = htons(port);
			break;
		case AF_INET6:
			addr->in6.sin6_port = htons(port);
			break;
	}
}

/* Return true if the 2 addresses are the same host (type and address) */
bool inet_addrmatch(union xp_sockaddr* addr1, union xp_sockaddr* addr2)
{
	if (addr1->addr.sa_family != addr2->addr.sa_family)
		return false;

	switch (addr1->addr.sa_family) {
		case AF_INET:
			return memcmp(&addr1->in.sin_addr, &addr2->in.sin_addr, sizeof(addr1->in.sin_addr)) == 0;
		case AF_INET6:
			return memcmp(&addr1->in6.sin6_addr, &addr2->in6.sin6_addr, sizeof(addr1->in6.sin6_addr)) == 0;
	}
	return false;
}

/* Return the current socket error description (for Windows), like strerror() does for errno */
DLLEXPORT char* socket_strerror(int error_number, char* buf, size_t buflen)
{
#if defined(_WINSOCKAPI_)
	strncpy(buf, "Unknown error", buflen);
	buf[buflen - 1] = 0;
	if (error_number > 0 && error_number < WSABASEERR)
		error_number += WSABASEERR;
	if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,  // dwFlags
	                    NULL, // lpSource
	                    error_number, // dwMessageId
	                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // dwLanguageId
	                    buf,
	                    buflen,
	                    NULL))
		safe_snprintf(buf, buflen, "Error %d getting error description", GetLastError());
	truncsp(buf);
	return buf;
#else
	return safe_strerror(error_number, buf, buflen);
#endif
}

DLLEXPORT void set_socket_errno(int err)
{
#if defined(_WINSOCKAPI_)
	WSASetLastError(err);
#else
	errno = err;
#endif
}

DLLEXPORT int xp_inet_pton(int af, const char *src, void *dst)
{
	struct addrinfo  hints = {0};
	struct addrinfo *res, *cur;

	if (af != AF_INET && af != AF_INET6) {
		set_socket_errno(EAFNOSUPPORT);
		return -1;
	}

	hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;
	if (getaddrinfo(src, NULL, &hints, &res))
		return -1;

	for (cur = res; cur; cur++) {
		if (cur->ai_addr->sa_family == af)
			break;
	}
	if (!cur) {
		freeaddrinfo(res);
		return 0;
	}
	switch (af) {
		case AF_INET:
			memcpy(dst, &(((struct sockaddr_in *)cur->ai_addr)->sin_addr), sizeof(((struct sockaddr_in *)cur->ai_addr)->sin_addr));
			break;
		case AF_INET6:
			memcpy(dst, &(((struct sockaddr_in6 *)cur->ai_addr)->sin6_addr), sizeof(((struct sockaddr_in6 *)cur->ai_addr)->sin6_addr));
			break;
	}
	freeaddrinfo(res);
	return 1;
}

#ifdef _WIN32
DLLEXPORT int
socketpair(int domain, int type, int protocol, SOCKET *sv)
{
	union xp_sockaddr la = {0};
	const int         ra = 1;
	SOCKET            ls;
	SOCKET *          check;
	fd_set            rfd;
	struct timeval    tv;
	socklen_t         sa_len;

	sv[0] = sv[1] = INVALID_SOCKET;
	ls = socket(domain, type, protocol);
	if (ls == INVALID_SOCKET)
		goto fail;
	switch (domain) {
		case PF_INET:
			if (inet_ptoaddr("127.0.0.1", &la, sizeof(la)) == NULL)
				goto fail;
			sa_len = sizeof(la.in);
			break;
		case PF_INET6:
			if (inet_ptoaddr("::1", &la, sizeof(la)) == NULL)
				goto fail;
			sa_len = sizeof(la.in6);
			break;
		default:
			goto fail;
	}
	inet_setaddrport(&la, 0);
	if (setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&ra, sizeof(ra)) == -1)
		goto fail;
	if (bind(ls, &la.addr, sa_len) == -1)
		goto fail;
	if (getsockname(ls, &la.addr, &sa_len) == -1)
		goto fail;
	if (listen(ls, 1) == -1)
		goto fail;
	sv[0] = socket(la.addr.sa_family, type, protocol);
	if (sv[0] == INVALID_SOCKET)
		goto fail;
	if (connect(sv[0], &la.addr, sa_len) == -1)
		goto fail;
	sv[1] = accept(ls, NULL, NULL);
	if (sv[1] == INVALID_SOCKET)
		goto fail;
	closesocket(ls);
	ls = INVALID_SOCKET;

	if (send(sv[1], (const char *)&sv, sizeof(sv), 0) != sizeof(sv))
		goto fail;
	tv.tv_sec = 0;
	tv.tv_usec = 50000;
	FD_ZERO(&rfd);
	FD_SET(sv[0], &rfd);
	if (select(sv[0] + 1, &rfd, NULL, NULL, &tv) != 1)
		goto fail;
	if (recv(sv[0], (char *)&check, sizeof(check), 0) != sizeof(check))
		goto fail;
	if (check != sv)
		goto fail;
	return 0;

fail:
	if (ls != INVALID_SOCKET)
		closesocket(ls);
	if (sv[0] != INVALID_SOCKET)
		closesocket(sv[0]);
	sv[0] = INVALID_SOCKET;
	if (sv[1] != INVALID_SOCKET)
		closesocket(sv[1]);
	sv[1] = INVALID_SOCKET;
	return -1;
}
#endif

