/* ident.c */

/* Synchronet Indentification (RFC1413) functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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

char *identify(SOCKADDR_IN* client_addr, u_short local_port, char* buf, size_t maxlen)
{
	char		req[128];
	char*		identity=NULL;
	SOCKET		sock=INVALID_SOCKET;
	SOCKADDR_IN	addr;

	do {
		if((sock = open_socket(SOCK_STREAM)) == INVALID_SOCKET) {
			sprintf(buf,"ERROR %d creating socket",ERROR_VALUE);
			break;
		}

		addr=*client_addr;
		addr.sin_port=113;	/* per RFC1413 */

		if(connect(sock, &addr, sizeof(addr)!=0) {
			sprintf(buf,"ERROR %d connecting to server",ERROR_VALUE);
			break;
		}

		sprintf(req,"%u, %u\r\n", client_adr.sin_port, local_port);
		if(send(sock,req,strlen(req),0)!=strlen(req)) {
			sprintf(buf,"ERROR %d sending request",ERROR_VALUE);
			break;
		}

		if(recv(sock,buf,maxlen,0)<1) {
			sprintf(buf,"ERROR %d receving response",ERROR_VALUE);
			break;
		}
		identity=buf;
	} while(0);

	close_socket(sock);

	return(identity);
}