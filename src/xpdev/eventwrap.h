/* Event-related cross-platform development wrappers (Win32 API emulation) */

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

#ifndef _EVENTWRAP_H
#define _EVENTWRAP_H

#include "gen_defs.h"

#if defined(__unix__)

	#include "xpevent.h"

#elif defined(_WIN32)	

	typedef HANDLE xpevent_t;
	#define WaitForEvent(event,ms)		WaitForSingleObject(event,ms)
	#define CloseEvent(event)			CloseHandle(event)

#else

	#error "Need event wrappers."

#endif

#endif	/* Don't add anything after this line */
