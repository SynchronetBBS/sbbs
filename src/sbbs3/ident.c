/* ident.c */

/* Synchronet Indentification (RFC1413) functions */

/* $Id: ident.c,v 1.16 2019/08/04 17:49:51 deuce Exp $ */

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

#include "sbbs.h"
#include "ident.h"

BOOL identify(union xp_sockaddr *client_addr, u_short local_port, char* buf
			   ,size_t maxlen, int timeout)
{
	char		req[128];
	int			i;
	int			result;
	int			rd;
	ulong		val;
	SOCKET		sock=INVALID_SOCKET;
	union xp_sockaddr	addr;
	struct timeval	tv;
	fd_set			socket_set;
	BOOL		success=FALSE;

	if(client_addr->addr.sa_family != AF_INET && client_addr->addr.sa_family != AF_INET6)
		return FALSE;
	if(timeout<=0)
		timeout=IDENT_DEFAULT_TIMEOUT;

	do {
		if((sock = open_socket(PF_INET, SOCK_STREAM, "ident")) == INVALID_SOCKET) {
			sprintf(buf,"ERROR %d creating socket",ERROR_VALUE);
			break;
		}

		val=1;
		ioctlsocket(sock,FIONBIO,&val);	

		memcpy(&addr, client_addr, xp_sockaddr_len(client_addr));
		inet_setaddrport(&addr, IPPORT_IDENT);

		result=connect(sock, &addr.addr, xp_sockaddr_len(&addr));

		if(result==SOCKET_ERROR
			&& (ERROR_VALUE==EWOULDBLOCK || ERROR_VALUE==EINPROGRESS)) {
			tv.tv_sec=timeout;
			tv.tv_usec=0;

			FD_ZERO(&socket_set);
			FD_SET(sock,&socket_set);
			if(select(sock+1,NULL,&socket_set,NULL,&tv)==1)
				result=0;	/* success */
		}
		if(result!=0) {
			sprintf(buf,"ERROR %d connecting to server",ERROR_VALUE);
			break;
		}

		val=0;
		ioctlsocket(sock,FIONBIO,&val);	

		tv.tv_sec=10;
		tv.tv_usec=0;

		FD_ZERO(&socket_set);
		FD_SET(sock,&socket_set);

		i=select(sock+1,NULL,&socket_set,NULL,&tv);
		if(i<1) {
			sprintf(buf,"ERROR %d selecting socket for send",ERROR_VALUE);
			break;
		}

		sprintf(req,"%u,%u\r\n", inet_addrport(client_addr), local_port);
		if(sendsocket(sock,req,strlen(req))!=(int)strlen(req)) {
			sprintf(buf,"ERROR %d sending request",ERROR_VALUE);
			break;
		}

		tv.tv_sec=10;
		tv.tv_usec=0;

		FD_ZERO(&socket_set);
		FD_SET(sock,&socket_set);

		i=select(sock+1,&socket_set,NULL,NULL,&tv);
		if(i<1) {
			sprintf(buf,"ERROR %d detecting response",ERROR_VALUE);
			break;
		}

		rd=recv(sock,buf,maxlen,0);
		if(rd<1) {
			sprintf(buf,"ERROR %d receiving response",ERROR_VALUE);
			break;
		}
		buf[rd]=0;
		truncsp(buf);
		success=TRUE;
	} while(0);

	close_socket(sock);

	return success;
}
