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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
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

#include "ringbuf.h"
#include "genwrap.h"    /* SLEEP() */

#ifdef RINGBUF_USE_STD_RTL

	#ifndef VTOOLSD

/* FreeBSD uses <stdlib.h> instead of <malloc.h> */
	#ifdef __unix__
		#include <stdlib.h>
	#else
		#include <malloc.h>     /* malloc prototype */
	#endif
	#include <string.h>         /* memcpy prototype */
	#endif  /* !VTOOLSD */

	#define os_malloc   malloc
	#define rb_malloc   malloc
	#define os_free     free
	#define rb_free     free
	#define os_memcpy   memcpy
	#define rb_memcpy   memcpy

#else

void  (*rb_free)(void *);
void *(*rb_memcpy)(void *, const void *, size_t);

#endif
/****************************************************************************/
/* Returns 0 on success, non-zero on failure								*/
/****************************************************************************/
int RingBufInit( RingBuf* rb, DWORD size
#ifndef RINGBUF_USE_STD_RTL
                 , void *(os_malloc)(size_t)
                 , void (os_free)(void *)
                 , void *(os_memcpy)(void *, const void *, size_t)
#endif
                 )
{
	memset(rb, 0, sizeof(RingBuf));
	if ((rb->pStart = (BYTE *)os_malloc(size + 1)) == NULL)
		return -1;
#ifndef RINGBUF_USE_STD_RTL
	rb_free = os_free;
	rb_memcpy = os_memcpy;
#endif
	rb->pHead = rb->pTail = rb->pStart;
	rb->pEnd = rb->pStart + size;
	rb->size = size;
#ifdef RINGBUF_EVENT
	rb->data_event = CreateEvent(NULL, /*  ManualReset: */ TRUE, /* InitialState: */ FALSE, NULL);
	rb->highwater_event = CreateEvent(NULL, /*  ManualReset: */ TRUE, /* InitialState: */ FALSE, NULL);
	rb->empty_event = CreateEvent(NULL, /* ManualReset: */ TRUE, /* InitialState: */ TRUE, NULL);
#endif
#ifdef RINGBUF_MUTEX
	pthread_mutex_init(&rb->mutex, NULL);
#endif
	return 0;
}

void RingBufDispose( RingBuf* rb)
{
	if (rb->pStart != NULL)
		os_free(rb->pStart);
#ifdef RINGBUF_EVENT
	/*
	 * TODO: CloseEvent() will fail if there's any waiters...
	 *       and the old semaphore posted an extra time
	 *       "just in case"
	 */
	if (rb->empty_event != NULL)
		CloseEvent(rb->empty_event);
	if (rb->data_event != NULL)
		CloseEvent(rb->data_event);
	if (rb->highwater_event != NULL)
		CloseEvent(rb->highwater_event);

#endif
#ifdef RINGBUF_MUTEX
	while (pthread_mutex_destroy(&rb->mutex) == EBUSY)
		SLEEP(1);
#endif
	memset(rb, 0, sizeof(RingBuf));
}

#define RINGBUF_FILL_LEVEL(rb)  (rb->pHead >= rb->pTail ? (rb->pHead - rb->pTail) \
								: (rb->size - (rb->pTail - (rb->pHead + 1))))

DWORD RingBufFull( RingBuf* rb )
{
	DWORD retval;

#ifdef RINGBUF_MUTEX
	pthread_mutex_lock(&rb->mutex);
#endif

	retval = RINGBUF_FILL_LEVEL(rb);

#ifdef RINGBUF_MUTEX
	pthread_mutex_unlock(&rb->mutex);
#endif

	return retval;
}

DWORD RingBufFree( RingBuf* rb )
{
	DWORD retval;

	retval = (rb->size - RingBufFull( rb ));

	return retval;
}

DWORD RingBufWrite( RingBuf* rb, const BYTE* src,  DWORD cnt )
{
	DWORD max, first, remain, fill_level;

	if (cnt == 0)
		return cnt;

	if (rb->pStart == NULL)
		return 0;

#ifdef RINGBUF_MUTEX
	pthread_mutex_lock(&rb->mutex);
#endif

	/* allowed to write at pEnd */
	max = rb->pEnd - rb->pHead + 1;

	fill_level = RINGBUF_FILL_LEVEL(rb);
	if (fill_level + cnt > rb->size)
		cnt = rb->size - fill_level;

	if (max >= cnt) {
		first = cnt;
		remain = 0;
	} else {
		first = max;
		remain = cnt - first;
	}

	rb_memcpy( rb->pHead, src, first );
	rb->pHead += first;
	src += first;

	if (remain) {

		rb->pHead = rb->pStart;
		rb_memcpy(rb->pHead, src, remain);
		rb->pHead += remain;
	}

	if (rb->pHead > rb->pEnd)
		rb->pHead = rb->pStart;

#ifdef RINGBUF_EVENT
	if (rb->empty_event != NULL)
		ResetEvent(rb->empty_event);
	if (rb->data_event != NULL)
		SetEvent(rb->data_event);
	if (rb->highwater_event != NULL && rb->highwater_mark != 0 && RINGBUF_FILL_LEVEL(rb) >= rb->highwater_mark)
		SetEvent(rb->highwater_event);
#endif

#ifdef RINGBUF_MUTEX
	pthread_mutex_unlock(&rb->mutex);
#endif

	return cnt;
}

/* Pass NULL dst to just forward pointer (after Peek) */
DWORD RingBufRead( RingBuf* rb, BYTE* dst,  DWORD cnt )
{
	DWORD max, first, remain, len;

#ifdef RINGBUF_MUTEX
	pthread_mutex_lock(&rb->mutex);
#endif

	len = RINGBUF_FILL_LEVEL(rb);

	if (len < cnt)
		cnt = len;

	/* allowed to read at pEnd */
	max = rb->pEnd - rb->pTail + 1;

	if (max >= cnt) {
		first = cnt;
		remain = 0;
	} else {
		first = max;
		remain = cnt - first;
	}

	if (first && dst != NULL) {
		rb_memcpy( dst, rb->pTail, first );
		dst += first;
	}
	rb->pTail += first;

	if (remain) {

		rb->pTail = rb->pStart;
		if (dst != NULL)
			rb_memcpy( dst, rb->pTail, remain );
		rb->pTail += remain;
	}

	if (rb->pTail > rb->pEnd)
		rb->pTail = rb->pStart;

#ifdef RINGBUF_EVENT
	if (rb->empty_event != NULL && RINGBUF_FILL_LEVEL(rb) == 0) {
		SetEvent(rb->empty_event);
		ResetEvent(rb->data_event);
	}
	if (rb->highwater_event != NULL && rb->highwater_mark != 0 && RINGBUF_FILL_LEVEL(rb) < rb->highwater_mark)
		ResetEvent(rb->highwater_event);
#endif

#ifdef RINGBUF_MUTEX
	pthread_mutex_unlock(&rb->mutex);
#endif

	return cnt;
}

DWORD RingBufPeek( RingBuf* rb, BYTE* dst,  DWORD cnt)
{
	DWORD max, first, remain, len;

	len = RingBufFull( rb );
	if (len == 0)
		return 0;

#ifdef RINGBUF_MUTEX
	pthread_mutex_lock(&rb->mutex);
#endif

	if (len < cnt)
		cnt = len;

	/* allowed to read at pEnd */
	max = rb->pEnd - rb->pTail + 1;

	if (max >= cnt) {
		first = cnt;
		remain = 0;
	} else {
		first = max;
		remain = cnt - first;
	}

	rb_memcpy( dst, rb->pTail, first );
	dst += first;

	if (remain) {
		rb_memcpy( dst, rb->pStart, remain );
	}

#ifdef RINGBUF_MUTEX
	pthread_mutex_unlock(&rb->mutex);
#endif

	return cnt;
}

/* Reset head and tail pointers */
void RingBufReInit(RingBuf* rb)
{
#ifdef RINGBUF_MUTEX
	pthread_mutex_lock(&rb->mutex);
#endif
	rb->pHead = rb->pTail = rb->pStart;
#ifdef RINGBUF_EVENT
	if (rb->empty_event != NULL)
		SetEvent(rb->empty_event);
	if (rb->data_event != NULL)
		ResetEvent(rb->data_event);
	if (rb->highwater_event != NULL && rb->highwater_mark != 0 && RINGBUF_FILL_LEVEL(rb) >= rb->highwater_mark)
		ResetEvent(rb->highwater_event);
#endif
#ifdef RINGBUF_MUTEX
	pthread_mutex_unlock(&rb->mutex);
#endif
}

/* End of RINGBUF.C */
