/* Synchronet Serial Communications (COM) I/O Library */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _COMIO_H
#define _COMIO_H

#include <limits.h>	/* INT_MAX */
#include <gen_defs.h>	/* bool */

#define COM_ERROR						-1
#define COM_INFINITE_TIMEOUT	INT_MIN

#ifdef _WIN32
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

#define COM_FLOW_CONTROL_NONE			0
#define COM_FLOW_CONTROL_RTS_CTS		(1<<0)
#define COM_FLOW_CONTROL_DTR_DSR		(1<<1)
#define COM_FLOW_CONTROL_XON_OFF		(1<<2)

/**************/
/* Prototypes */
/**************/

#if defined(__cplusplus)
extern "C" {
#endif

COMIOEXPORT char*		comVersion(char* str, size_t len);
COMIOEXPORT COM_HANDLE	comOpen(const char* device);
COMIOEXPORT bool		comClose(COM_HANDLE);
COMIOEXPORT long		comGetBaudRate(COM_HANDLE);
COMIOEXPORT bool		comSetBaudRate(COM_HANDLE, ulong rate);
COMIOEXPORT int			comGetFlowControl(COM_HANDLE);
COMIOEXPORT bool		comSetFlowControl(COM_HANDLE, int);
COMIOEXPORT int			comGetModemStatus(COM_HANDLE);
COMIOEXPORT bool		comRaiseDTR(COM_HANDLE);
COMIOEXPORT bool		comLowerDTR(COM_HANDLE);
COMIOEXPORT bool		comRaiseRTS(COM_HANDLE);
COMIOEXPORT bool		comLowerRTS(COM_HANDLE);
COMIOEXPORT bool		comWriteByte(COM_HANDLE, BYTE);
COMIOEXPORT int			comWriteBuf(COM_HANDLE, const BYTE*, size_t buflen);
COMIOEXPORT int			comWriteString(COM_HANDLE, const char*);
COMIOEXPORT bool		comReadByte(COM_HANDLE, BYTE*);
COMIOEXPORT size_t		comReadBuf(COM_HANDLE, char* buf, size_t buflen
							,const char* terminators, int timeout /* in milliseconds */);
COMIOEXPORT size_t		comReadLine(COM_HANDLE, char* buf, size_t buflen
							,int timeout /* in milliseconds */);
COMIOEXPORT bool		comPurgeInput(COM_HANDLE);
COMIOEXPORT bool		comPurgeOutput(COM_HANDLE);
COMIOEXPORT bool		comDrainOutput(COM_HANDLE);
COMIOEXPORT bool		comSetParity(COM_HANDLE, bool enable, bool odd);
COMIOEXPORT bool		comSetBits(COM_HANDLE, size_t byteSize, size_t stopBits);

#if defined(__cplusplus)
}
#endif


#endif /* Don't add anything after this #endif statement */
