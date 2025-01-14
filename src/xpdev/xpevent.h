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

#include <pthread.h>
#include "gen_defs.h"

#if defined(__solaris__)
#include <xpsem.h>  /* u_int32_t */
#endif

/* Opaque type definition. */
struct xpevent;
typedef struct xpevent *xpevent_t;

#if defined(__cplusplus)
extern "C" {
#endif
xpevent_t   CreateEvent(void *sec, BOOL bManualReset, BOOL bInitialState, const char *name);
BOOL        SetEvent(xpevent_t event);
BOOL        ResetEvent(xpevent_t event);
BOOL        CloseEvent(xpevent_t event);
DWORD       WaitForEvent(xpevent_t event, DWORD ms);
#if defined(__cplusplus)
}
#endif

struct xpevent {
#define EVENT_MAGIC       ((u_int32_t) 0x09fa4014)
	u_int32_t magic;
	pthread_mutex_t lock;
	pthread_cond_t gtzero;
	BOOL value;
	BOOL mreset;
	DWORD nwaiters;
	void *cbdata;
	BOOL (*verify)(void *);
};

#define INFINITE    ((DWORD)(-1))
enum {
	WAIT_OBJECT_0
	, WAIT_TIMEOUT
	, WAIT_FAILED
};

#endif
