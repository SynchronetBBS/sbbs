/* sexyz.h */

/* Synchronet External X/Y/ZMODEM Transfer Program */

/* $Id: sexyz.h,v 1.13 2018/07/24 01:11:08 rswindell Exp $ */

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

#include "xmodem.h"
#include "zmodem.h"

#define MAXERRORS	10

#define SEND			(1<<0)	/* Sending file(s)							*/
#define RECV			(1<<1)	/* Receiving file(s)						*/
#define XMODEM			(1<<2)	/* Use Xmodem								*/
#define YMODEM			(1<<3)	/* Use Ymodem								*/
#define ZMODEM			(1<<4)	/* Use Zmodem								*/
#define CRC 			(1<<5)	/* Use CRC error correction 				*/
#define GMODE			(1<<6)	/* For Xmodem-G and Ymodem-G				*/
#define RECVDIR 		(1<<7)	/* Directory specified to download to		*/
#define OVERWRITE		(1<<9)	/* Overwrite receiving files				*/

							/* Zmodem mode bits 						*/
#define CTRL_ESC	(1<<0)	/* Escape all control chars 				*/
#define VAR_HDRS	(1<<1)	/* Use variable headers 					*/

#define SUCCESS		0
#define NOINP		-1			/* input buffer empty (incom only) */
#define FAILURE		-2			/* xmodem_get_block() failure */
#define NOT_YMODEM	-3			/* Expected block zero, got block one */
#define NOT_XMODEM	-4			/* Expected block one, got block zero */
