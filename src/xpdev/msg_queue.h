/* Uni or Bi-directional FIFO message queue */

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

#ifndef _MSG_QUEUE_H
#define _MSG_QUEUE_H

#include "link_list.h"
#include "threadwrap.h"
#include "wrapdll.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
	char			name[128];			/* for named-queues */
	link_list_t		in;
	link_list_t		out;
	pthread_t		owner_thread_id;	/* reads from in, writes to out */
	long			refs;
	unsigned long	flags;				/* private use flags */
	void*			private_data;
} msg_queue_t;

#define MSG_QUEUE_MALLOC		(1<<0)	/* Queue allocated with malloc() */
#define MSG_QUEUE_BIDIR			(1<<1)	/* Bi-directional message queue */
#define MSG_QUEUE_ORPHAN		(1<<2)	/* Owner has detached */

DLLEXPORT msg_queue_t*	msgQueueInit(msg_queue_t*, long flags);
DLLEXPORT BOOL			msgQueueFree(msg_queue_t*);

DLLEXPORT long			msgQueueAttach(msg_queue_t*);
DLLEXPORT long			msgQueueDetach(msg_queue_t*);
DLLEXPORT BOOL			msgQueueOwner(msg_queue_t*);

/* Get/Set queue private data */
DLLEXPORT void*			msgQueueSetPrivateData(msg_queue_t*, void*);
DLLEXPORT void*			msgQueueGetPrivateData(msg_queue_t*);

DLLEXPORT BOOL			msgQueueWait(msg_queue_t* q, long timeout);
DLLEXPORT long			msgQueueReadLevel(msg_queue_t*);
DLLEXPORT void*			msgQueueRead(msg_queue_t*, long timeout);
DLLEXPORT void*			msgQueuePeek(msg_queue_t*, long timeout);
DLLEXPORT void*			msgQueueFind(msg_queue_t*, const void*, size_t length);
DLLEXPORT list_node_t*	msgQueueFirstNode(msg_queue_t*);
DLLEXPORT list_node_t*	msgQueueLastNode(msg_queue_t*);
#define			msgQueueNextNode(node)			listNextNode(node)
#define			msgQueuePrevNode(node)			listPrevNode(node)
#define			msgQueueNodeData(node)			listNodeData(node)

DLLEXPORT long			msgQueueWriteLevel(msg_queue_t*);
DLLEXPORT BOOL			msgQueueWrite(msg_queue_t*, const void*, size_t length);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
