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
#include <errno.h>		/* ENOMEM */

#include "sockwrap.h"	/* sendsocket */
#include "filewrap.h"	/* filelength */

int sendfilesocket(int sock, int file, long *offset, long count)
{
	char*	buf;
	long	len;
	int		rd;
	int		wr;

	len=filelength(file);
	if((buf=malloc(len))==NULL) {
		errno=ENOMEM;
		return(-1);
	}

	if(offset!=NULL)
		if(lseek(file,*offset,SEEK_SET)<0)
			return(-1);

	if(count==0)
		count=len;

	rd=read(file,buf,count);
	close(file);
	if(rd!=count) {
		free(buf);
		return(-1);
	}

	wr=sendsocket(sock,buf,len);
	free(buf);

	if(offset!=NULL)
		(*offset)+=wr;

	return(wr);
}

