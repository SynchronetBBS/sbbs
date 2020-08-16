/* msg_queue.c */

/* Uni or Bi-directional FIFO message queue */

/* $Id: msg_queue.c,v 1.15 2019/08/22 01:40:21 rswindell Exp $ */

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

#include <stdlib.h>		/* malloc */
#include <string.h>		/* memset */

#include "genwrap.h"	/* msclock() */
#include "threadwrap.h"	/* pthread_self */
#include "msg_queue.h"

msg_queue_t* DLLCALL msgQueueInit(msg_queue_t* q, long flags)
{
	if(q==NULL) {
		if((q=(msg_queue_t*)malloc(sizeof(msg_queue_t)))==NULL)
			return(NULL);
		flags |= MSG_QUEUE_MALLOC;
	} 

	memset(q,0,sizeof(msg_queue_t));

	q->flags = flags;
	q->refs = 1;
	q->owner_thread_id = pthread_self();

	if(q->flags&MSG_QUEUE_BIDIR)
		listInit(&q->in,LINK_LIST_SEMAPHORE|LINK_LIST_MUTEX);
	listInit(&q->out,LINK_LIST_SEMAPHORE|LINK_LIST_MUTEX);

	return(q);
}

BOOL DLLCALL msgQueueOwner(msg_queue_t* q)
{
	if(q==NULL)
		return(FALSE);

	return q->owner_thread_id == pthread_self();
}

BOOL DLLCALL msgQueueFree(msg_queue_t* q)
{
	if(q==NULL)
		return(FALSE);

	listFree(&q->in);
	listFree(&q->out);

	if(q->flags&MSG_QUEUE_MALLOC)
		free(q);

	return(TRUE);
}

long DLLCALL msgQueueAttach(msg_queue_t* q)
{
	if(q==NULL)
		return(-1);

	q->refs++;

	return(q->refs);
}

long DLLCALL msgQueueDetach(msg_queue_t* q)
{
	int refs;

	if(q==NULL || q->refs<1)
		return(-1);

	if(msgQueueOwner(q))
		q->flags |= MSG_QUEUE_ORPHAN;

	if((refs=--q->refs)==0)
		msgQueueFree(q);

	return(refs);
}

void* DLLCALL msgQueueSetPrivateData(msg_queue_t* q, void* p)
{
	void* old;

	if(q==NULL)
		return(NULL);

	old=q->private_data;
	q->private_data=p;
	return(old);
}

void* DLLCALL msgQueueGetPrivateData(msg_queue_t* q)
{
	if(q==NULL)
		return(NULL);
	return(q->private_data);
}

static link_list_t* msgQueueReadList(msg_queue_t* q)
{
	if(q==NULL)
		return(NULL);

	if((q->flags&MSG_QUEUE_BIDIR)
		&& q->owner_thread_id == pthread_self())
		return(&q->in);
	return(&q->out);
}

static link_list_t* msgQueueWriteList(msg_queue_t* q)
{
	if(q==NULL)
		return(NULL);

	if(!(q->flags&MSG_QUEUE_BIDIR)
		|| q->owner_thread_id == pthread_self())
		return(&q->out);
	return(&q->in);
}

long DLLCALL msgQueueReadLevel(msg_queue_t* q)
{
	return listCountNodes(msgQueueReadList(q));
}

static BOOL list_wait(link_list_t* list, long timeout)
{
#if defined(LINK_LIST_THREADSAFE)
	if(timeout<0)	/* infinite */
		return listSemWait(list);
	if(timeout==0)	/* poll */
		return listSemTryWait(list);

	return listSemTryWaitBlock(list,timeout);
#else
	clock_t	start;
	long	count;
	
	start=msclock();
	while((count=listCountNodes(list))==0) {
		if(timeout==0)
			break;
		if(timeout>0 && msclock()-start > timeout)
			break;
		YIELD();
	}
	return(INT_TO_BOOL(count));
#endif
}

BOOL DLLCALL msgQueueWait(msg_queue_t* q, long timeout)
{
	BOOL			result;
	link_list_t*	list = msgQueueReadList(q);

	if((result=list_wait(list,timeout))==TRUE)
#if defined(LINK_LIST_THREADSAFE)
		listSemPost(list)	/* Replace the semaphore we just cleared */
#endif
		;

	return(result);
}

void* DLLCALL msgQueueRead(msg_queue_t* q, long timeout)
{
	link_list_t*	list = msgQueueReadList(q);

	list_wait(list,timeout);

	return listShiftNode(list);
}

void* DLLCALL msgQueuePeek(msg_queue_t* q, long timeout)
{
	link_list_t*	list = msgQueueReadList(q);

	if(list_wait(list,timeout))
#if defined(LINK_LIST_THREADSAFE)
		listSemPost(list)	/* Replace the semaphore we just cleared */
#endif
		;

	return  listNodeData(listFirstNode(list));
}

void* DLLCALL msgQueueFind(msg_queue_t* q, const void* data, size_t length)
{
	link_list_t*	list = msgQueueReadList(q);
	list_node_t*	node;

	if((node=listFindNode(list,data,length))==NULL)
		return(NULL);
	return listRemoveNode(list,node,/* Free Data? */FALSE);
}

list_node_t* DLLCALL msgQueueFirstNode(msg_queue_t* q)
{
	return listFirstNode(msgQueueReadList(q));
}

list_node_t* DLLCALL msgQueueLastNode(msg_queue_t* q)
{
	return listLastNode(msgQueueReadList(q));
}

long DLLCALL msgQueueWriteLevel(msg_queue_t* q)
{
	return listCountNodes(msgQueueWriteList(q));
}

BOOL DLLCALL msgQueueWrite(msg_queue_t* q, const void* data, size_t length)
{
	return listPushNodeData(msgQueueWriteList(q),data,length)!=NULL;
}

