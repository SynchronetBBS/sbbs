/* xsdkinet.h */

/* Synchronet platform-specific Internet stuff */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout xtrn	*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _XSDKINET_H
#define _XSDKINET_H

/***************/
/* OS-specific */
/***************/
#if defined _WIN32	|| defined __OS2__	/* Use WinSock */

#include <winsock.h>	/* socket/bind/etc. */

/* Let's agree on a standard WinSock symbol here, people */
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_	
#endif

#elif defined __unix__	/* Unix-variant */

#ifdef __BSD
#include <sys/types.h>	/* u_intXX_t */
#endif

#include <netdb.h>		/* gethostbyname */
#include <netinet/in.h>	/* IPPROTO_IP */
#include <sys/socket.h>	/* socket/bind/etc. */
#include <sys/ioctl.h>	/* FIONBIO */
#include <sys/time.h>	/* struct timeval */
#include <arpa/inet.h>	/* inet_ntoa */
#include <unistd.h>		/* close */
#include <errno.h>		/* errno */

#endif

/**********************************/
/* Socket Implementation-specific */
/**********************************/
#ifdef _WINSOCKAPI_

#undef  EINTR
#define EINTR			WSAEINTR
#undef  ENOTSOCK
#define ENOTSOCK		WSAENOTSOCK
#undef  EWOULDBLOCK
#define EWOULDBLOCK		WSAEWOULDBLOCK
#undef  ECONNRESET
#define ECONNRESET		WSAECONNRESET
#undef  ECONNABORTED
#define ECONNABORTED	WSAECONNABORTED

#define s_addr			S_un.S_addr

#define socklen_t		int

#define ERROR_VALUE		WSAGetLastError()

#else	/* BSD sockets */

/* WinSock-isms */
#define HOSTENT			struct hostent
#define SOCKADDR_IN		struct sockaddr_in
#define LINGER			struct linger
#define SOCKET			int
#define SOCKET_ERROR	-1
#define INVALID_SOCKET  (SOCKET)(~0)
#define closesocket		close
#define ioctlsocket		ioctl
#define ERROR_VALUE		errno

#endif	/* __unix__ */

#ifndef SHUT_RDWR
#define SHUT_RDWR		2	/* for shutdown() */
#endif

#endif	/* Don't add anything after this line */
