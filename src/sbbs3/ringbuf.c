/* ringbuf.c */

/* Synchronet ring buffer routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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
 * for malloc, free, and memcpy
 */

#ifdef VTOOLSD
#include <vtoolsd.h>
#include LOCKED_CODE_SEGMENT
#include LOCKED_DATA_SEGMENT
#endif

#include "genwrap.h" /* YIELD() */ 
#include "ringbuf.h"

#ifdef RINGBUF_USE_STD_RTL

	#ifndef VTOOLSD

	/* FreeBSD uses <stdlib.h> instead of <malloc.h> */
	#ifdef __unix__
		#include <stdlib.h>
	#else
		#include <malloc.h>		/* malloc prototype */
	#endif
	#include <string.h>			/* memcpy prototype */
    #endif	/* !VTOOLSD */

	#define os_malloc	malloc
	#define rb_malloc	malloc
	#define os_free 	free
	#define rb_free 	free
	#define os_memcpy	memcpy
	#define rb_memcpy	memcpy

#else

	void (*rb_free)(void *);
	void *(*rb_memcpy)(void *, const void *, size_t);

#endif
/****************************************************************************/
/* Returns 0 on success, non-zero on failure								*/
/****************************************************************************/
int RINGBUFCALL RingBufInit( RingBuf* rb, DWORD size
#ifndef RINGBUF_USE_STD_RTL
	,void *(os_malloc)(size_t)
	,void (os_free)(void *)
	,void *(os_memcpy)(void *, const void *, size_t)
#endif
	)
{
	if((rb->pStart=(BYTE *)os_malloc(size+1))==NULL)
		return(-1);
#ifndef RINGBUF_USE_STD_RTL
	rb_free=os_free;
	rb_memcpy=os_memcpy;
#endif
	rb->pHead=rb->pTail=rb->pStart;
	rb->pEnd=rb->pStart+size;
    rb->size=size;
#ifdef RINGBUF_MUTEX
	pthread_mutex_init(&rb->mutex,NULL);
#endif
	return(0);
}

void RINGBUFCALL RingBufDispose( RingBuf* rb)
{
    if(rb->pStart!=NULL)
		os_free(rb->pStart);
#ifdef RINGBUF_MUTEX
	pthread_mutex_destroy(&rb->mutex);
#endif
	memset(rb,0,sizeof(RingBuf));
}

DWORD RINGBUFCALL RingBufFull( RingBuf* rb )
{
	DWORD	head,tail,retval;

#ifdef RINGBUF_MUTEX
	pthread_mutex_lock(&rb->mutex);
#endif

	head = (DWORD) rb->pHead;
	tail = (DWORD) rb->pTail;

	if(head >= tail)
		retval = head - tail;
	else
		retval = rb->size - (tail - head);

#ifdef RINGBUF_MUTEX
	pthread_mutex_unlock(&rb->mutex);
#endif

	return(retval);
}

DWORD RINGBUFCALL RingBufFree( RingBuf* rb )
{
	DWORD retval;

	retval = (rb->size - RingBufFull( rb ));

	return(retval);
}

DWORD RINGBUFCALL RingBufWrite( RingBuf* rb, BYTE* src,  DWORD cnt )
{
	DWORD max, first, remain;

	if(rb->pStart==NULL)
		return(0);

#ifdef RINGBUF_MUTEX
	pthread_mutex_lock(&rb->mutex);
#endif

    /* allowed to write at pEnd */
	max = (((DWORD) rb->pEnd) - ((DWORD) rb->pHead)) + 1;

	/*
	 * we assume the caller has checked that there is enough room. For this reason
	 * we do not have to worry about head wrapping past the tail
	 */

	if( max >= cnt ) {
		first = cnt;
		remain = 0;
	} else {
		first = max;
		remain = cnt - first;
	}

	rb_memcpy( rb->pHead, src, first );
	rb->pHead += first;
    src += first;

	if(remain) {

		rb->pHead = rb->pStart;
		rb_memcpy(rb->pHead, src, remain);
		rb->pHead += remain;
	}

    if(rb->pHead > rb->pEnd)
    	rb->pHead = rb->pStart;

#ifdef RINGBUF_MUTEX
	pthread_mutex_unlock(&rb->mutex);
#endif

	YIELD();

	return(cnt);
}

/* Pass NULL dst to just foward pointer (after Peek) */
DWORD RINGBUFCALL RingBufRead( RingBuf* rb, BYTE* dst,  DWORD cnt )
{
	DWORD max, first, remain, len;

	YIELD();

	len = RingBufFull( rb );
	if( len == 0 )
		return(0);

#ifdef RINGBUF_MUTEX
	pthread_mutex_lock(&rb->mutex);
#endif

	if( len < cnt )
        cnt = len;

	/* allowed to read at pEnd */
	max = (((DWORD) rb->pEnd) - ((DWORD) rb->pTail)) + 1;

	if( max >= cnt ) {
		first = cnt;
		remain = 0;
	} else {
		first = max;
		remain = cnt - first;
	}

    if(dst!=NULL) {
		rb_memcpy( dst, rb->pTail, first );
		dst += first;
    }
	rb->pTail += first;

	if( remain ){

		rb->pTail = rb->pStart;
        if(dst!=NULL)
			rb_memcpy( dst, rb->pTail, remain );
		rb->pTail += remain;
	}

    if(rb->pTail > rb->pEnd)
		rb->pTail = rb->pStart;

#ifdef RINGBUF_MUTEX
	pthread_mutex_unlock(&rb->mutex);
#endif

	return(cnt);
}

DWORD RINGBUFCALL RingBufPeek( RingBuf* rb, BYTE* dst,  DWORD cnt)
{
	DWORD max, first, remain, len;

	YIELD();

	len = RingBufFull( rb );
	if( len == 0 )
		return(0);

#ifdef RINGBUF_MUTEX
	pthread_mutex_lock(&rb->mutex);
#endif

	if( len < cnt )
        cnt = len;

    /* allowed to read at pEnd */
	max = (((DWORD) rb->pEnd) - ((DWORD) rb->pTail)) + 1;

	if( max >= cnt ) {
		first = cnt;
		remain = 0;
	} else {
		first = max;
		remain = cnt - first;
	}

	rb_memcpy( dst, rb->pTail, first );
	dst += first;

	if(remain) {
		rb_memcpy( dst, rb->pStart, remain );
	}

#ifdef RINGBUF_MUTEX
	pthread_mutex_unlock(&rb->mutex);
#endif

	return(cnt);
}

/* Reset head and tail pointers */
void RINGBUFCALL RingBufReInit(RingBuf* rb)
{
#ifdef RINGBUF_MUTEX
	pthread_mutex_lock(&rb->mutex);
#endif
	rb->pHead = rb->pTail = rb->pStart;
#ifdef RINGBUF_MUTEX
	pthread_mutex_unlock(&rb->mutex);
#endif
}

/* End of RINGBUF.C */
