/* Synchronet ring buffer routines */

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


/* Pre-define RINGBUF_USE_STD_RTL to use standard C runtime library symbols
   for malloc, free, and memcpy
 */

#ifndef _RINGBUF_H_
#define _RINGBUF_H_

#include "gen_defs.h"

#ifdef RINGBUF_SEM
	#include "semwrap.h"	/* sem_t */
#endif
#ifdef RINGBUF_EVENT
	#include "eventwrap.h"	/* xpevent_t */
#endif
#ifdef RINGBUF_MUTEX
	#include "threadwrap.h"	/* pthread_mutex_t */
#endif

#ifndef NULL
#define NULL	0
#endif

#define RINGBUF_USE_STD_RTL

/************/
/* Typedefs */
/************/

typedef struct {

	BYTE* 	pStart;
	BYTE* 	pHead;			/* next space available for data */
	BYTE* 	pTail;			/* next byte to be consumed */
	BYTE* 	pEnd; 			/* end of the buffer, used for wrap around */
    DWORD	size;
#ifdef RINGBUF_SEM
	sem_t	sem;			/* semaphore used to signal data waiting */
	sem_t	highwater_sem;	/* semaphore used to signal highwater mark reached */
	DWORD	highwater_mark;
#endif
#ifdef RINGBUF_EVENT
	xpevent_t empty_event;
#endif
#ifdef RINGBUF_MUTEX
	pthread_mutex_t mutex;	/* mutex used to protect ring buffer pointers */
#endif

} RingBuf;

#ifdef __cplusplus
extern "C" {
#endif

/***********************/
/* Function Prototypes */
/***********************/

int 	RingBufInit( RingBuf* rb, DWORD size
#ifndef RINGBUF_USE_STD_RTL
			,void *(os_malloc)(size_t)
			,void (os_free)(void *)
			,void *(os_memcpy)(void *, const void *, size_t)
#endif
			);
void	RingBufDispose( RingBuf* rb );
DWORD	RingBufFull( RingBuf* rb );
DWORD	RingBufFree( RingBuf* rb );
DWORD	RingBufWrite( RingBuf* rb, const BYTE *src,	DWORD cnt );
DWORD	RingBufRead( RingBuf* rb, BYTE *dst,  DWORD cnt );
DWORD	RingBufPeek( RingBuf* rb, BYTE *dst,  DWORD cnt );
void	RingBufReInit( RingBuf* rb );

#ifdef	__cplusplus
}
#endif

#endif	/* Don't add anything after this line */
