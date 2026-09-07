/* *nix emulation of Win32 *Event API */

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

#ifndef _XPEVENT_H_
#define _XPEVENT_H_

#if !defined(__unix__) || !defined(_EVENTWRAP_H)
	#error Include eventwrap.h instead
#endif

#include "gen_defs.h"
#include "wrapdll.h"

/* The Unix implementation is private to the XPDev library. */
struct xpevent;
typedef struct xpevent *xpevent_t;

#if defined(__cplusplus)
extern "C" {
#endif
DLLEXPORT xpevent_t   CreateEvent(void *sec, BOOL bManualReset, BOOL bInitialState, const char *name);
DLLEXPORT BOOL        SetEvent(xpevent_t event);
DLLEXPORT BOOL        ResetEvent(xpevent_t event);
DLLEXPORT BOOL        CloseEvent(xpevent_t event);
DLLEXPORT DWORD       WaitForEvent(xpevent_t event, DWORD ms);
#if defined(__cplusplus)
}
#endif

#define INFINITE    ((DWORD)(-1))
enum {
	WAIT_OBJECT_0
	, WAIT_TIMEOUT
	, WAIT_FAILED
};

#endif
