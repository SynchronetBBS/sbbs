/* comio.h */

/* Synchronet Serial Communications (COM) I/O Library */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2007 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifndef _COMIO_H
#define _COMIO_H

#include <gen_defs.h>	/* BOOL */

#define COM_ERROR						-1

#ifdef _WIN32
	#include <windows.h>

    #define COM_HANDLE					HANDLE
	#define COM_HANDLE_INVALID			INVALID_HANDLE_VALUE
	#define COM_ERROR_VALUE				GetLastError()
/* Modem Status bits */
	#define COM_CTS						MS_CTS_ON
	#define COM_DSR						MS_DSR_ON
	#define COM_RING					MS_RING_ON
	#define	COM_DCD						MS_RLSD_ON
#else
	#include <termios.h>

    #define COM_HANDLE					int
	#define COM_HANDLE_INVALID			-1
	#define COM_ERROR_VALUE				errno

/* Stuff we apparently don't care about... 
 *
 *	TIOCM_LE   Line Enable.
 *	TIOCM_DTR  Data Terminal Ready.
 *	TIOCM_RTS  Request To Send.
 *	TIOCM_ST   Secondary Transmit.
 *	TIOCM_SR   Secondary Receive.
 */

	#define COM_CTS						TIOCM_CTS  /* Clear To Send. */
#ifdef TIOCM_CAR
	#define COM_DCD						TIOCM_CAR  /* Carrier Detect. */
#else
	#define COM_DCD						TIOCM_CD   /* Carrier Detect (synonym). */
#endif
#ifdef TIOCM_RNG
	#define COM_RING					TIOCM_RNG  /* Ring Indication. */
#else
	#define COM_RING					TIOCM_RI   /* Ring Indication (synonym). */
#endif
	#define COM_DSR						TIOCM_DSR  /* Data Set Ready. */

#endif

/**************/
/* Prototypes */
/**************/

#if defined(__cplusplus)
extern "C" {
#endif

char*		comVersion(char* str, size_t len);
COM_HANDLE	comOpen(const char* device);
BOOL		comClose(COM_HANDLE);
long		comGetBaudRate(COM_HANDLE);
BOOL		comSetBaudRate(COM_HANDLE, ulong rate);
int			comGetModemStatus(COM_HANDLE);
int			comRaiseDTR(COM_HANDLE);
int			comLowerDTR(COM_HANDLE);
BOOL		comWriteByte(COM_HANDLE, BYTE);
int			comWriteBuf(COM_HANDLE, const BYTE*, size_t buflen);
int			comWriteString(COM_HANDLE, const char*);
BOOL		comReadByte(COM_HANDLE, BYTE*);
size_t		comReadBuf(COM_HANDLE, char* buf, size_t buflen
					    ,const char* terminators, int timeout /* in milliseconds */);
size_t		comReadLine(COM_HANDLE, char* buf, size_t buflen
						,int timeout /* in milliseconds */);
BOOL		comPurgeInput(COM_HANDLE);
BOOL		comPurgeOutput(COM_HANDLE);

#if defined(__cplusplus)
}
#endif


#endif /* Don't add anything after this #endif statement */
