/* sockwrap.c */

/* Berkley/WinSock socket API wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include <stdlib.h>		/* malloc/free on FreeBSD */
#include <string.h>		/* bzero (for FD_ZERO) on FreeBSD */
#include <errno.h>		/* ENOMEM */
#include <stdio.h>		/* SEEK_SET */
#include <string.h>

#include "genwrap.h"	/* SLEEP */
#include "gen_defs.h"	/* BOOL */
#include "sockwrap.h"	/* sendsocket */
#include "filewrap.h"	/* filelength */

int sendfilesocket(int sock, int file, long *offset, long count)
{
/* sendfile() on Linux may or may not work with non-blocking sockets ToDo */
#if defined(__FreeBSD__)
	off_t	total=0;
	off_t	wr=0;
	int		i;
	long	len;

	len=filelength(file);
	if(count<1 || count>len) {
		count=len;
		count-=offset==NULL?0:*offset;
	}
	while((i=sendfile(file,sock,(offset==NULL?0:*offset)+total,count-total,NULL,&wr,0))==-1 && errno==EAGAIN)  {
		total+=wr;
		SLEEP(1);
	}
	if(i==0)
		return((int)count);
	return(i);
#else
	char*	buf;
	long	len;
	int		rd;
	int		wr;
	int		total;

	if(offset!=NULL)
		if(lseek(file,*offset,SEEK_SET)<0)
			return(-1);

	len=filelength(file);

	if(count<1 || count>len) {
		count=len;
		count-=tell(file);		/* don't try to read beyond EOF */
	}

	if((buf=malloc(count))==NULL) {
		errno=ENOMEM;
		return(-1);
	}

	rd=read(file,buf,count);
	if(rd!=count) {
		free(buf);
		return(-1);
	}

	for(total=wr=0;total<count;total+=wr) {
		wr=sendsocket(sock,buf+total,count-total);
		if(wr>0)
			continue;
		if(wr==SOCKET_ERROR && ERROR_VALUE==EWOULDBLOCK) {
			wr=0;
			SLEEP(1);
			continue;
		}
		break;
	}

	free(buf);

	if(offset!=NULL)
		(*offset)+=total;

	if(wr<1)
		return(wr);

	return(total);
#endif
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
		
	if((buf=malloc(count))==NULL) {
		errno=ENOMEM;
		return(-1);
	}

	if(offset!=NULL)
		if(lseek(file,*offset,SEEK_SET)<0)
			return(-1);

	rd=read(sock,buf,count);
	if(rd!=count) {
		free(buf);
		return(-1);
	}

	wr=write(file,buf,rd);
	free(buf);

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
			FD_CLR(sock,&rd_set);
	}

	/* Convert timeout from ms to sec/usec */
	tv.tv_sec=timeout/1000;
	tv.tv_usec=(timeout%1000)*1000;

	i=select(sock+1,&rd_set,wr_set_p,NULL,&tv);
	if(i==SOCKET_ERROR)
		return(FALSE);

	if(i==0) 
		return(TRUE);

	if(wr_p!=NULL && FD_ISSET(sock,wr_set_p)) {
		*wr_p=TRUE;
		if(i==1)
			return(TRUE);
	}

	rd=recv(sock,&ch,1,MSG_PEEK);
	if(rd==1 
		|| (rd==SOCKET_ERROR && ERROR_VALUE==EMSGSIZE)) {
		if(rd_p!=NULL)
			*rd_p=TRUE;
		return(TRUE);
	}

	return(FALSE);
}


