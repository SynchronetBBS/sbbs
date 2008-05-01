/* sockwrap.c */

/* Berkley/WinSock socket API wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
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
	{ NULL }
};

int getSocketOptionByName(const char* name, int* level)
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

socket_option_t* getSocketOptionList(void)
{
	return(socket_options);
}

int sendfilesocket(int sock, int file, long *offset, long count)
{
	char	buf[1024*16];
	long	len;
	int		rd;
	int		wr=0;
	int		total=0;
	int		i;

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

int recvfilesocket(int sock, int file, long *offset, long count)
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
		
	if((buf=(char*)alloca(count))==NULL) {
		errno=ENOMEM;
		return(-1);
	}

	if(offset!=NULL)
		if(lseek(file,*offset,SEEK_SET)<0)
			return(-1);

	rd=read(sock,buf,count);
	if(rd!=count)
		return(-1);

	wr=write(file,buf,rd);

	if(offset!=NULL)
		(*offset)+=wr;

	return(wr);
}


/* Return true if connected, optionally sets *rd_p to true if read data available */
BOOL socket_check(SOCKET sock, BOOL* rd_p, BOOL* wr_p, DWORD timeout)
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

int retry_bind(SOCKET s, const struct sockaddr *addr, socklen_t addrlen
			   ,uint retries, uint wait_secs
			   ,const char* prot
			   ,int (*lprintf)(int level, char *fmt, ...))
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
			lprintf(i<retries ? LOG_WARNING:LOG_ERR
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
