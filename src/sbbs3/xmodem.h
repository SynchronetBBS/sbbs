/* xmodem.h */

/* Synchronet X/YMODEM Functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifndef _XMODEM_H_
#define _XMODEM_H_

#include <stdio.h>		/* FILE */
#include <sockwrap.h>	/* SOCKET */
#include "crc16.h"

typedef struct {

	SOCKET	sock;											/* socket descriptor */
	long	mode;
	FILE*	statfp;
	FILE*	errfp;

} xmodem_t;


char*		xmodem_ver(char *buf);
const char* xmodem_source(void);
void		xmodem_cancel(xmodem_t* xm);
int			xmodem_get_ack(xmodem_t* xm, int tries);
void		xmodem_put_nak(xmodem_t* xm);
int			xmodem_get_block(xmodem_t* xm, uchar* block, uint block_size, BOOL hdrblock);
void		xmodem_put_block(xmodem_t* xm, uchar* block, uint block_size, ulong block_num);

#endif	/* Don't add anything after this line */