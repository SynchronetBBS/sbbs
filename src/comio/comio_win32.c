/* comio_win32.h */

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

#include "comio.h"
#include "genwrap.h"

COM_HANDLE comOpen(const char* device)
{
	COM_HANDLE handle;
	COMMTIMEOUTS timeouts;

	if((handle=CreateFile(device
		,GENERIC_READ|GENERIC_WRITE 	/* Access */
		,0								/* Share mode */
		,NULL							/* Security attributes */
		,OPEN_EXISTING					/* Create access */
		,FILE_ATTRIBUTE_NORMAL			/* File attributes */
		,NULL							/* Template */
		))==INVALID_HANDLE_VALUE)
		return COM_HANDLE_INVALID;


	if(GetCommTimeouts(handle, &timeouts)) {
		timeouts.ReadIntervalTimeout=MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier=0;
		timeouts.ReadTotalTimeoutConstant=0;		// No-wait read timeout
		timeouts.WriteTotalTimeoutMultiplier=0;
		timeouts.WriteTotalTimeoutConstant=5000;	// 5 seconds
		SetCommTimeouts(handle,&timeouts);
	}

	return handle;
}

BOOL comClose(COM_HANDLE handle)
{
	return CloseHandle(handle);
}

int comGetModemStatus(COM_HANDLE handle)
{
	DWORD status=0;
	
	if(GetCommModemStatus(handle, &status))
		return status;
	else
		return COM_ERROR;
}

BOOL comRaiseDTR(COM_HANDLE handle)
{
	return EscapeCommFunction(handle, SETDTR);
}

BOOL comLowerDTR(COM_HANDLE handle)
{
	return EscapeCommFunction(handle, CLRDTR);
}

BOOL comWriteByte(COM_HANDLE handle, BYTE ch)
{
	DWORD wr=0;

	return WriteFile(handle, &ch, sizeof(ch), &wr, NULL) && wr==sizeof(BYTE);
}

int comWriteBuf(COM_HANDLE handle, const BYTE* buf, size_t buflen)
{
	DWORD wr=0;

	if(!WriteFile(handle, buf, buflen, &wr, NULL))
		return COM_ERROR;

	return wr;
}

int comWriteString(COM_HANDLE handle, const char* str)
{
	return comWriteBuf(handle, str, strlen(str));
}

BOOL comReadByte(COM_HANDLE handle, BYTE* ch)
{
	DWORD rd;

	return ReadFile(handle, ch, sizeof(BYTE), &rd, NULL) && rd==sizeof(BYTE);
}

size_t comReadBuf(COM_HANDLE handle, char* buf, size_t buflen, int timeout)
{
	BYTE		ch;
	size_t		len=0;
	msclock_t	start=msclock();

	while(len < buflen) {
		if(!comReadByte(handle, &ch)) {
			if(msclock()-start >= timeout)
				break;
			YIELD();
			continue;
		}
		buf[len++]=ch;
	}

	return len;
}

BOOL comPurgeInput(COM_HANDLE handle)
{
	return PurgeComm(handle, PURGE_RXCLEAR);
}

BOOL comPurgeOutput(COM_HANDLE handle)
{
	return PurgeComm(handle, PURGE_TXCLEAR);
}
