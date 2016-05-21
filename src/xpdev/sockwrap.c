/* sockwrap.c */

/* Berkley/WinSock socket API wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <ctype.h>		/* isdigit */
#include <stdlib.h>		/* alloca/free on FreeBSD */
#include <string.h>		/* bzero (for FD_ZERO) on FreeBSD */
#include <errno.h>		/* ENOMEM */
#include <stdio.h>		/* SEEK_SET */
#include <string.h>
#if defined(_WIN32)
 #include <malloc.h>	/* alloca() on Win32 */
#endif

#include "genwrap.h"	/* SLEEP */
#include "gen_defs.h"	/* BOOL/LOG_WARNING */
#include "sockwrap.h"	/* sendsocket */
#include "filewrap.h"	/* filelength */

static socket_option_t socket_options[] = {
	{ "TYPE",				0,				SOL_SOCKET,		SO_TYPE				},
	{ "ERROR",				0,				SOL_SOCKET,		SO_ERROR			},
	{ "DEBUG",				0,				SOL_SOCKET,		SO_DEBUG			},
	{ "LINGER",				SOCK_STREAM,	SOL_SOCKET,		SO_LINGER			},
	{ "SNDBUF",				0,				SOL_SOCKET,		SO_SNDBUF			},
	{ "RCVBUF",				0,				SOL_SOCKET,		SO_RCVBUF			},

#ifndef _WINSOCKAPI_	/* Defined, but not supported, by WinSock */
	{ "SNDLOWAT",			0,				SOL_SOCKET,		SO_SNDLOWAT			},
	{ "RCVLOWAT",			0,				SOL_SOCKET,		SO_RCVLOWAT			},
	{ "SNDTIMEO",			0,				SOL_SOCKET,		SO_SNDTIMEO			},
	{ "RCVTIMEO",			0,				SOL_SOCKET,		SO_RCVTIMEO			},
#ifdef SO_USELOOPBACK	/* SunOS */
	{ "USELOOPBACK",		0,				SOL_SOCKET,		SO_USELOOPBACK		},
#endif
#endif

	{ "REUSEADDR",			0,				SOL_SOCKET,		SO_REUSEADDR		},	
#ifdef SO_REUSEPORT	/* BSD */
	{ "REUSEPORT",			0,				SOL_SOCKET,		SO_REUSEPORT		},	
#endif
	{ "KEEPALIVE",			SOCK_STREAM,	SOL_SOCKET,		SO_KEEPALIVE		},
	{ "DONTROUTE",			0,				SOL_SOCKET,		SO_DONTROUTE		},
	{ "BROADCAST",			SOCK_DGRAM,		SOL_SOCKET,		SO_BROADCAST		},
	{ "OOBINLINE",			SOCK_STREAM,	SOL_SOCKET,		SO_OOBINLINE		},

#ifdef SO_ACCEPTCONN											
	{ "ACCEPTCONN",			SOCK_STREAM,	SOL_SOCKET,		SO_ACCEPTCONN		},
#endif
#ifdef SO_PRIORITY		/* Linux */
	{ "PRIORITY",			0,				SOL_SOCKET,		SO_PRIORITY			},
#endif
#ifdef SO_NO_CHECK		/* Linux */
	{ "NO_CHECK",			0,				SOL_SOCKET,		SO_NO_CHECK			},
#endif
#ifdef SO_PROTOTYPE		/* SunOS */
	{ "PROTOTYPE",			0,				SOL_SOCKET,		SO_PROTOTYPE		},
#endif
#ifdef SO_MAX_MSG_SIZE	/* WinSock2 */
	{ "MAX_MSG_SIZE",		SOCK_DGRAM,		SOL_SOCKET,		SO_MAX_MSG_SIZE		},
#endif
#ifdef SO_CONNECT_TIME	/* WinSock2 */
	{ "CONNECT_TIME",		SOCK_STREAM,	SOL_SOCKET,		SO_CONNECT_TIME		},
#endif

	/* IPPROTO-level socket options */
	{ "TCP_NODELAY",		SOCK_STREAM,	IPPROTO_TCP,	TCP_NODELAY			},
	/* The following are platform-specific */					
#ifdef TCP_MAXSEG											
	{ "TCP_MAXSEG",			SOCK_STREAM,	IPPROTO_TCP,	TCP_MAXSEG			},
#endif															
#ifdef TCP_CORK													
	{ "TCP_CORK",			SOCK_STREAM,	IPPROTO_TCP,	TCP_CORK			},
#endif															
#ifdef TCP_KEEPIDLE												
	{ "TCP_KEEPIDLE",		SOCK_STREAM,	IPPROTO_TCP,	TCP_KEEPIDLE		},
#endif															
#ifdef TCP_KEEPINTVL											
	{ "TCP_KEEPINTVL",		SOCK_STREAM,	IPPROTO_TCP,	TCP_KEEPINTVL		},
#endif															
#ifdef TCP_KEEPCNT												
	{ "TCP_KEEPCNT",		SOCK_STREAM,	IPPROTO_TCP,	TCP_KEEPCNT			},
#endif															
#ifdef TCP_KEEPALIVE	/* SunOS */
	{ "TCP_KEEPALIVE",		SOCK_STREAM,	IPPROTO_TCP,	TCP_KEEPALIVE		},
#endif															
#ifdef TCP_SYNCNT												
	{ "TCP_SYNCNT",			SOCK_STREAM,	IPPROTO_TCP,	TCP_SYNCNT			},
#endif															
#ifdef TCP_LINGER2												
	{ "TCP_LINGER2",		SOCK_STREAM,	IPPROTO_TCP,	TCP_LINGER2			},
#endif														
#ifdef TCP_DEFER_ACCEPT										
	{ "TCP_DEFER_ACCEPT",	SOCK_STREAM,	IPPROTO_TCP,	TCP_DEFER_ACCEPT	},
#endif															
#ifdef TCP_WINDOW_CLAMP											
	{ "TCP_WINDOW_CLAMP",	SOCK_STREAM,	IPPROTO_TCP,	TCP_WINDOW_CLAMP	},
#endif														
#ifdef TCP_QUICKACK											
	{ "TCP_QUICKACK",		SOCK_STREAM,	IPPROTO_TCP,	TCP_QUICKACK		},
#endif						
#ifdef TCP_NOPUSH			
	{ "TCP_NOPUSH",			SOCK_STREAM,	IPPROTO_TCP,	TCP_NOPUSH			},
#endif						
#ifdef TCP_NOOPT			
	{ "TCP_NOOPT",			SOCK_STREAM,	IPPROTO_TCP,	TCP_NOOPT			},
#endif
#if defined(IPV6_V6ONLY) && defined(IPPROTO_IPV6)
	{ "IPV6_V6ONLY",		0,				IPPROTO_IPV6,	IPV6_V6ONLY			},
#endif
	{ NULL }
};

int DLLCALL getSocketOptionByName(const char* name, int* level)
{
	int i;

	if(level!=NULL)
		*level=SOL_SOCKET;	/* default option level */
	for(i=0;socket_options[i].name;i++) {
		if(stricmp(name,socket_options[i].name)==0) {
			if(level!=NULL)
				*level = socket_options[i].level;
			return(socket_options[i].value);
		}
	}
	if(!isdigit(*name))	/* unknown option name */
		return(-1);
	return(strtol(name,NULL,0));
}

socket_option_t* DLLCALL getSocketOptionList(void)
{
	return(socket_options);
}

int DLLCALL sendfilesocket(int sock, int file, off_t *offset, off_t count)
{
	char		buf[1024*16];
	off_t		len;
	int			rd;
	int			wr=0;
	int			total=0;
	int			i;

/* sendfile() on Linux may or may not work with non-blocking sockets ToDo */
	len=filelength(file);

	if(offset!=NULL)
		if(lseek(file,*offset,SEEK_SET)<0)
			return(-1);

	if(count<1 || count>len) {
		count=len;
		count-=tell(file);		/* don't try to read beyond EOF */
	}
#if USE_SENDFILE
	while((i=sendfile(file,sock,(offset==NULL?0:*offset)+total,count-total,NULL,&wr,0))==-1 && errno==EAGAIN)  {
		total+=wr;
		SLEEP(1);
	}
	if(i==0)
		return((int)count);
#endif

	if(count<0) {
		errno=EINVAL;
		return(-1);
	}

	while(total<count) {
		rd=read(file,buf,sizeof(buf));
		if(rd==-1)
			return(-1);
		if(rd==0)
			break;
		for(i=wr=0;i<rd;i+=wr) {
			wr=sendsocket(sock,buf+i,rd-i);
			if(wr>0)
				continue;
			if(wr==SOCKET_ERROR && ERROR_VALUE==EWOULDBLOCK) {
				wr=0;
				SLEEP(1);
				continue;
			}
			return(wr);
		}
		if(i!=rd)
			return(-1);
		total+=rd;
	}

	if(offset!=NULL)
		(*offset)+=total;

	return(total);
}

int DLLCALL recvfilesocket(int sock, int file, off_t *offset, off_t count)
{
	/* Writes a file from a socket -
	 *
	 * sock		- Socket to read from
	 * file		- File descriptior to write to
	 *				MUST be open and writeable
	 * offset	- pointer to file offset to start writing at
	 *				is set to offset writing STOPPED
	 *				on return
	 * count	- number of bytes to read/write
	 *
	 * returns -1 if an error occurse, otherwise
	 * returns number ob bytes written and sets offset
	 * to the new offset
	 */
	 
	char*	buf;
	int		rd;
	int		wr;

	if(count<1) {
		errno=ERANGE;
		return(-1);
	}
		
	if((buf=(char*)malloc((size_t)count))==NULL) {
		errno=ENOMEM;
		return(-1);
	}

	if(offset!=NULL) {
		if(lseek(file,*offset,SEEK_SET)<0) {
			free(buf);
			return(-1);
		}
	}

	rd=read(sock,buf,(size_t)count);
	if(rd!=count) {
		free(buf);
		return(-1);
	}

	wr=write(file,buf,rd);

	if(offset!=NULL)
		(*offset)+=wr;

	free(buf);
	return(wr);
}


/* Return true if connected, optionally sets *rd_p to true if read data available */
BOOL DLLCALL socket_check(SOCKET sock, BOOL* rd_p, BOOL* wr_p, DWORD timeout)
{
	char	ch;
	int		i,rd;
	fd_set	rd_set;
	fd_set*	rd_set_p=&rd_set;
	fd_set	wr_set;
	fd_set*	wr_set_p=NULL;
	struct	timeval tv;

	if(rd_p!=NULL)
		*rd_p=FALSE;

	if(wr_p!=NULL)
		*wr_p=FALSE;

	if(sock==INVALID_SOCKET)
		return(FALSE);

	FD_ZERO(&rd_set);
	FD_SET(sock,&rd_set);
	if(wr_p!=NULL) {
		wr_set_p=&wr_set;
		FD_ZERO(wr_set_p);
		FD_SET(sock,wr_set_p);
		if(rd_p==NULL)
			rd_set_p=NULL;
	}

	/* Convert timeout from ms to sec/usec */
	tv.tv_sec=timeout/1000;
	tv.tv_usec=(timeout%1000)*1000;

	i=select(sock+1,rd_set_p,wr_set_p,NULL,&tv);
	if(i==SOCKET_ERROR)
		return(FALSE);

	if(i==0) 
		return(TRUE);

	if(wr_p!=NULL && FD_ISSET(sock,wr_set_p)) {
		*wr_p=TRUE;
		if(i==1)
			return(TRUE);
	}

	if(rd_p !=NULL || wr_p==NULL)  {
		rd=recv(sock,&ch,1,MSG_PEEK);
		if(rd==1 
			|| (rd==SOCKET_ERROR && ERROR_VALUE==EMSGSIZE)) {
			if(rd_p!=NULL)
				*rd_p=TRUE;
			return(TRUE);
		}
	}

	return(FALSE);
}

int DLLCALL retry_bind(SOCKET s, const struct sockaddr *addr, socklen_t addrlen
			   ,uint retries, uint wait_secs
			   ,const char* prot
			   ,int (*lprintf)(int level, const char *fmt, ...))
{
	char	port_str[128];
	int		result=-1;
	uint	i;

	if(addr->sa_family==AF_INET)
		SAFEPRINTF(port_str," to port %u",ntohs(((SOCKADDR_IN *)(addr))->sin_port));
	else
		port_str[0]=0;
	for(i=0;i<=retries;i++) {
		if((result=bind(s,addr,addrlen))==0)
			break;
		if(lprintf!=NULL)
			lprintf(i<retries ? LOG_WARNING:LOG_CRIT
				,"%04d !ERROR %d binding %s socket%s", s, ERROR_VALUE, prot, port_str);
		if(i<retries) {
			if(lprintf!=NULL)
				lprintf(LOG_WARNING,"%04d Will retry in %u seconds (%u of %u)"
					,s, wait_secs, i+1, retries);
			SLEEP(wait_secs*1000);
		}
	}
	return(result);
}

int DLLCALL nonblocking_connect(SOCKET sock, struct sockaddr* addr, size_t size, unsigned timeout)
{
	int result;

	result=connect(sock, addr, size);

	if(result==SOCKET_ERROR) {
		result=ERROR_VALUE;
		if(result==EWOULDBLOCK || result==EINPROGRESS) {
			fd_set		wsocket_set;
			fd_set		esocket_set;
			struct		timeval tv;
			socklen_t	optlen=sizeof(result);
			tv.tv_sec = timeout;
			tv.tv_usec = 0;
			FD_ZERO(&wsocket_set);
			FD_SET(sock,&wsocket_set);
			FD_ZERO(&esocket_set);
			FD_SET(sock,&esocket_set);
			switch(select(sock+1,NULL,&wsocket_set,&esocket_set,&tv)) {
				case 1:
					if(getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)&result, &optlen)==SOCKET_ERROR)
						result=ERROR_VALUE;
					break;
				case 0:
					break;
				case SOCKET_ERROR:
					result=ERROR_VALUE;
					break;
			}
		}
	}
	return result;
}


union xp_sockaddr* DLLCALL inet_ptoaddr(char *addr_str, union xp_sockaddr *addr, size_t size)
{
    struct addrinfo hints = {0};
    struct addrinfo *res, *cur;

    hints.ai_flags = AI_NUMERICHOST|AI_PASSIVE;
    if(getaddrinfo(addr_str, NULL, &hints, &res))
        return NULL;
    
    for(cur = res; cur; cur++) {
        if(cur->ai_addr->sa_family == AF_INET6)
            break;
    }
    if(!cur) {
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

const char* DLLCALL inet_addrtop(union xp_sockaddr *addr, char *dest, size_t size)
{
#ifdef _WIN32
	if(getnameinfo(&addr->addr, xp_sockaddr_len(addr), dest, size, NULL, 0, NI_NUMERICHOST))
		safe_snprintf(dest, size, "<Error %u converting address, family=%u>", WSAGetLastError(), addr->addr.sa_family);
	return dest;
#else
	switch(addr->addr.sa_family) {
		case AF_INET:
			return inet_ntop(addr->in.sin_family, &addr->in.sin_addr, dest, size);
		case AF_INET6:
			return inet_ntop(addr->in6.sin6_family, &addr->in6.sin6_addr, dest, size);
		case AF_UNIX:
			strncpy(dest, addr->un.sun_path, size);
			dest[size-1]=0;
			return dest;
		default:
			safe_snprintf(dest, size, "<unknown address family: %u>", addr->addr.sa_family);
			return NULL;
	}
#endif
}

uint16_t DLLCALL inet_addrport(union xp_sockaddr *addr)
{
	switch(addr->addr.sa_family) {
		case AF_INET:
			return ntohs(addr->in.sin_port);
		case AF_INET6:
			return ntohs(addr->in6.sin6_port);
		default:
			return 0;
	}
}

void DLLCALL inet_setaddrport(union xp_sockaddr *addr, uint16_t port)
{
	switch(addr->addr.sa_family) {
		case AF_INET:
			addr->in.sin_port = htons(port);
			break;
		case AF_INET6:
			addr->in6.sin6_port = htons(port);
			break;
	}
}

/* Return TRUE if the 2 addresses are the same host (type and address) */
BOOL DLLCALL inet_addrmatch(union xp_sockaddr* addr1, union xp_sockaddr* addr2)
{
	if(addr1->addr.sa_family != addr2->addr.sa_family)
		return FALSE;

	switch(addr1->addr.sa_family) {
		case AF_INET:
			return memcmp(&addr1->in.sin_addr, &addr2->in.sin_addr, sizeof(addr1->in.sin_addr)) == 0;
		case AF_INET6:
			return memcmp(&addr1->in6.sin6_addr, &addr2->in6.sin6_addr, sizeof(addr1->in6.sin6_addr)) == 0;
	}
	return FALSE;
}
