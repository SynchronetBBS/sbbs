/* msg_queue.h */

/* Uni or Bi-directional FIFO message queue */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifndef _MSG_QUEUE_H
#define _MSG_QUEUE_H

#include "link_list.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
	char			name[128];			/* for named-queues */
	link_list_t		in;
	link_list_t		out;
	DWORD			owner_thread_id;	/* reads from in, writes to out */
	long			refs;
	unsigned long	flags;				/* private use flags */
	void*			private_data;
} msg_queue_t;

#define MSG_QUEUE_MALLOC		(1<<0)	/* Queue allocated with malloc() */
#define MSG_QUEUE_BIDIR			(1<<1)	/* Bi-directional message queue */

msg_queue_t*	msgQueueInit(msg_queue_t*, long flags);
BOOL			msgQueueFree(msg_queue_t*);

long			msgQueueAttach(msg_queue_t*);
long			msgQueueDetach(msg_queue_t*);

/* Get/Set queue private data */
void*			msgQueueSetPrivateData(msg_queue_t*, void*);
void*			msgQueueGetPrivateData(msg_queue_t*);

long			msgQueueReadLevel(msg_queue_t*);
void*			msgQueueRead(msg_queue_t*, long timeout);
void*			msgQueuePeek(msg_queue_t*, long timeout);
void*			msgQueueFind(msg_queue_t*, const void*, size_t length);
list_node_t*	msgQueueFirstNode(msg_queue_t*);
list_node_t*	msgQueueLastNode(msg_queue_t*);
#define			msgQueueNextNode(node)			listNextNode(node)
#define			msgQueuePrevNode(node)			listPrevNode(node)
#define			msgQueueNodeData(node)			listNodeData(node)

long			msgQueueWriteLevel(msg_queue_t*);
BOOL			msgQueueWrite(msg_queue_t*, const void*, size_t length);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
