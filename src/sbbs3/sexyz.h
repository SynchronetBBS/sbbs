/* sexyz.h */

/* Synchronet External X/Y/ZMODEM Transfer Program */

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

#include "xmodem.h"
#include "zmodem.h"

#define MAXERRORS	10

#if 0
#define DLE 		CTRL_P	/* Data link escape 						*/
#define XON 		CTRL_Q	/* Resume transmission 						*/
#define XOFF		CTRL_S	/* Pause transmission						*/

#define SOH 		CTRL_A	/* Start of header							*/
#define EOT 		CTRL_D	/* End of transmission						*/

#define ACK 		CTRL_F	/* Acknowledge								*/
#define NAK 		CTRL_U	/* Negative Acknowledge 					*/
#define CAN 		CTRL_X	/* Cancel									*/
#endif

#define CPMEOF		CTRL_Z	/* CP/M End of file (^Z)					*/
#define LOC_ABORT	0x2e03	/* Local abort key	(^C)					*/

#define SEND		(1<<0)	/* Sending file(s)							*/
#define RECV		(1<<1)	/* Receiving file(s)						*/
#define XMODEM		(1<<2)	/* Use Xmodem								*/
#define YMODEM		(1<<3)	/* Use Ymodem								*/
#define ZMODEM		(1<<4)	/* Use Zmodem								*/
#define CRC 		(1<<5)	/* Use CRC error correction 				*/
#define GMODE		(1<<6)	/* For Qmodem-G and Ymodem-G				*/
#define DIR 		(1<<7)	/* Directory specified to download to		*/
#define DEBUG		(1<<8)	/* Debug output 							*/
#define OVERWRITE	(1<<9)	/* Overwrite receiving files				*/
#define IGNORE_DCD	(1<<10) /* Ignore DCD								*/
#define ALARM		(1<<11) /* Alarm when starting and stopping xfer	*/
#define NO_LOCAL	(1<<12) /* Don't check local keyboard               */
#define PAUSE_ABEND	(1<<13)	/* Pause on abnormal exit					*/
#define TELNET		(1<<14)	/* Telnet IAC escaping						*/

							/* Zmodem mode bits 						*/
#define CTRL_ESC	(1<<0)	/* Escape all control chars 				*/
#define VAR_HDRS	(1<<1)	/* Use variable headers 					*/

#define NOINP 0x0100     	/* input buffer empty (incom only) */

uint	recv_byte(SOCKET sock, int timeout, long mode);
int		send_byte(SOCKET sock, uchar ch, int timeout, long mode);
char*	chr(uchar ch);
void	bail(int code);