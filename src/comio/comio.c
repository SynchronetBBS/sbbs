/* Synchronet Serial Communications I/O Library Common Functions */

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

#include "comio.h"
#include "genwrap.h"	/* msclock */

size_t comReadBuf(COM_HANDLE handle, char* buf, size_t buflen, const char* terminators, int timeout)
{
	BYTE		ch;
	size_t		len=0;
	msclock_t	start=msclock();

	while(len < buflen) {
		if(!comReadByte(handle, &ch)) {
			if(timeout != COM_INFINITE_TIMEOUT && msclock()-start >= timeout)
				break;
			YIELD();
			continue;
		}
		if(len && terminators!=NULL && strchr(terminators, ch)!=NULL)
			break;
		buf[len++]=ch;
	}

	return len;
}

size_t comReadLine(COM_HANDLE handle, char* buf, size_t buflen, int timeout)
{
	size_t	len;

	len=comReadBuf(handle, buf, buflen-1, "\n", timeout);

	buf[len]=0;

	return len;
}

