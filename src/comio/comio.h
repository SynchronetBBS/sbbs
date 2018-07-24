/* comio.h */

/* Synchronet Serial Communications (COM) I/O Library */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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
	#ifdef __BORLANDC__
		#define COMIOCALL __stdcall
	#else
		#define COMIOCALL
	#endif
	#if defined(COMIO_IMPORTS) || defined(COMIO_EXPORTS)
		#if defined(COMIO_IMPORTS)
			#define COMIOEXPORT __declspec( dllimport )
		#else
			#define COMIOEXPORT __declspec( dllexport )
		#endif
	#else	/* self-contained executable */
		#define COMIOEXPORT
	#endif
#else
	#define COMIOCALL
	#define COMIOEXPORT
#endif

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

COMIOEXPORT char*		COMIOCALL comVersion(char* str, size_t len);
COMIOEXPORT COM_HANDLE	COMIOCALL comOpen(const char* device);
COMIOEXPORT BOOL		COMIOCALL comClose(COM_HANDLE);
COMIOEXPORT long		COMIOCALL comGetBaudRate(COM_HANDLE);
COMIOEXPORT BOOL		COMIOCALL comSetBaudRate(COM_HANDLE, ulong rate);
COMIOEXPORT int			COMIOCALL comGetModemStatus(COM_HANDLE);
COMIOEXPORT int			COMIOCALL comRaiseDTR(COM_HANDLE);
COMIOEXPORT int			COMIOCALL comLowerDTR(COM_HANDLE);
COMIOEXPORT BOOL		COMIOCALL comWriteByte(COM_HANDLE, BYTE);
COMIOEXPORT int			COMIOCALL comWriteBuf(COM_HANDLE, const BYTE*, size_t buflen);
COMIOEXPORT int			COMIOCALL comWriteString(COM_HANDLE, const char*);
COMIOEXPORT BOOL		COMIOCALL comReadByte(COM_HANDLE, BYTE*);
COMIOEXPORT size_t		COMIOCALL comReadBuf(COM_HANDLE, char* buf, size_t buflen
					    ,const char* terminators, int timeout /* in milliseconds */);
COMIOEXPORT size_t		COMIOCALL comReadLine(COM_HANDLE, char* buf, size_t buflen
						,int timeout /* in milliseconds */);
COMIOEXPORT BOOL		COMIOCALL comPurgeInput(COM_HANDLE);
COMIOEXPORT BOOL		COMIOCALL comPurgeOutput(COM_HANDLE);

#if defined(__cplusplus)
}
#endif


#endif /* Don't add anything after this #endif statement */
