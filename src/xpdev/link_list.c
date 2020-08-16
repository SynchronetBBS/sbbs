/* Double-Linked-list library */

/* $Id: link_list.c,v 1.64 2019/08/04 19:38:53 rswindell Exp $ */

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
#include "link_list.h"
#include "genwrap.h"

#if defined(_WIN32) && defined(LINK_LIST_USE_HEAPALLOC)
	#define malloc(size)	HeapAlloc(GetProcessHeap(), /* flags: */0, size)
	#define free(ptr)		HeapFree(GetProcessHeap(), /* flags: */0, ptr)
#endif

link_list_t* DLLCALL listInit(link_list_t* list, long flags)
{
	if((flags&LINK_LIST_MALLOC) || list==NULL) {
		if((list=(link_list_t*)malloc(sizeof(link_list_t)))==NULL)
			return(NULL);
		flags |= LINK_LIST_MALLOC;
	}

	memset(list,0,sizeof(link_list_t));

	list->flags = flags;

#if defined(LINK_LIST_THREADSAFE)
	if(list->flags&LINK_LIST_MUTEX) {
		list->mutex = pthread_mutex_initializer_np(/* recursive: */TRUE);
	}

	if(list->flags&LINK_LIST_SEMAPHORE)
		sem_init(&list->sem,0,0);
#endif

	if(flags&LINK_LIST_ATTACH)
		listAttach(list);

	return(list);
}

BOOL DLLCALL listFreeNodeData(list_node_t* node)
{
	if(node!=NULL && node->data!=NULL && !(node->flags&LINK_LIST_LOCKED)) {
		free(node->data);
		node->data = NULL;
		return(TRUE);
	}
	return(FALSE);
}

long DLLCALL listFreeNodes(link_list_t* list)
{
	list_node_t* node;
	list_node_t* next;

	if(list==NULL)
		return(-1);

	listLock(list);

	for(node=list->first; node!=NULL; node=next) {

		if(node->flags&LINK_LIST_LOCKED)
			break;

		if(((list->flags&LINK_LIST_ALWAYS_FREE) || (node->flags&LINK_LIST_MALLOC))
			&& !(list->flags&LINK_LIST_NEVER_FREE))
			listFreeNodeData(node);

		next = node->next;

		free(node);

		if(list->count)
			list->count--;
	}

	list->first = node;
	if(!list->count)
		list->last = NULL;

	listUnlock(list);

	return(list->count);
}

BOOL DLLCALL listFree(link_list_t* list)
{
	if(list==NULL)
		return(FALSE);

	if(listFreeNodes(list))
		return(FALSE);

#if defined(LINK_LIST_THREADSAFE)

	if(list->flags&LINK_LIST_MUTEX) {
		while(pthread_mutex_destroy((pthread_mutex_t*)&list->mutex)==EBUSY)
			SLEEP(1);
		list->flags&=~LINK_LIST_MUTEX;
	}

	if(list->flags&LINK_LIST_SEMAPHORE) {
		while(sem_destroy(&list->sem)==-1 && errno==EBUSY)
			SLEEP(1);
		//list->sem=(sem_t)NULL; /* Removed 08-20-08 - list->sem is never checked and this causes an error with gcc 4.1.2 (ThetaSigma) */
		list->flags&=~LINK_LIST_SEMAPHORE;
	}
#endif

	if(list->flags&LINK_LIST_MALLOC)
		free(list);

	return(TRUE);
}

long DLLCALL listAttach(link_list_t* list)
{
	if(list==NULL)
		return(-1);

	listLock(list);
	list->refs++;
	listUnlock(list);

	return(list->refs);
}

long DLLCALL listDettach(link_list_t* list)
{
	int refs;

	if(list==NULL || list->refs<1)
		return(-1);

	listLock(list);
	if((refs=--list->refs)==0)
		listFree(list);
	else
		listUnlock(list);

	return(refs);
}

void* DLLCALL listSetPrivateData(link_list_t* list, void* p)
{
	void* old;

	if(list==NULL)
		return(NULL);

	listLock(list);
	old=list->private_data;
	list->private_data=p;
	listUnlock(list);
	return(old);
}

void* DLLCALL listGetPrivateData(link_list_t* list)
{
	if(list==NULL)
		return(NULL);
	return(list->private_data);
}

#if defined(LINK_LIST_THREADSAFE)

BOOL DLLCALL listSemPost(link_list_t* list)
{
	if(list==NULL || !(list->flags&LINK_LIST_SEMAPHORE))
		return(FALSE);

	return(sem_post(&list->sem)==0);
}

BOOL DLLCALL listSemWait(link_list_t* list)
{
	if(list==NULL || !(list->flags&LINK_LIST_SEMAPHORE))
		return(FALSE);

	return(sem_wait(&list->sem)==0);
}

BOOL DLLCALL listSemTryWait(link_list_t* list)
{
	if(list==NULL || !(list->flags&LINK_LIST_SEMAPHORE))
		return(FALSE);

	return(sem_trywait(&list->sem)==0);
}

BOOL DLLCALL listSemTryWaitBlock(link_list_t* list, unsigned long timeout)
{
	if(list==NULL || !(list->flags&LINK_LIST_SEMAPHORE))
		return(FALSE);

	return(sem_trywait_block(&list->sem,timeout)==0);
}

#endif

BOOL DLLCALL listLock(link_list_t* list)
{
	int	ret=0;

	if(list==NULL)
		return(FALSE);
#if defined(LINK_LIST_THREADSAFE)
	if((list->flags&LINK_LIST_MUTEX) && (ret=pthread_mutex_lock(&list->mutex))==0)
#endif
		list->locks++;
	return(ret==0);
}

BOOL DLLCALL listIsLocked(const link_list_t* list)
{
	if(list==NULL)
		return(FALSE);
	return(list->locks > 0 ? TRUE : FALSE);
}

BOOL DLLCALL listUnlock(link_list_t* list)
{
	int	ret=0;

	if(list==NULL)
		return(FALSE);
#if defined(LINK_LIST_THREADSAFE)
	if((list->flags&LINK_LIST_MUTEX) && (ret=pthread_mutex_unlock(&list->mutex))==0)
#endif
		list->locks--;
	return(ret==0);
}

long DLLCALL listCountNodes(link_list_t* list)
{
	long			count=0;
	list_node_t*	node;

	if(list==NULL)
		return(-1);

	if(list->count)
		return(list->count);

	listLock(list);

	for(node=list->first; node!=NULL; node=node->next)
		count++;

	listUnlock(list);

	return(count);
}

list_node_t* DLLCALL listFindNode(link_list_t* list, const void* data, size_t length)
{
	list_node_t* node;

	if(list==NULL)
		return(NULL);

	listLock(list);

	for(node=list->first; node!=NULL; node=node->next) {
		if(length==0) {
			if(node->data==data)
				break;
		} else if(data==NULL) {
			if(node->tag==(list_node_tag_t)length)
				break;
		} else if(node->data!=NULL && memcmp(node->data,data,length)==0)
			break;
	}

	listUnlock(list);

	return(node);
}

ulong DLLCALL listCountMatches(link_list_t* list, const void* data, size_t length)
{
	list_node_t* node;
	ulong matches = 0;

	if(list==NULL)
		return 0;

	listLock(list);

	for(node=list->first; node!=NULL; node=node->next) {
		if(length==0) {
			if(node->data!=data)
				continue;
		} else if(data==NULL) {
			if(node->tag==(list_node_tag_t)length)
				continue;
		} else if(node->data==NULL || memcmp(node->data,data,length)!=0)
			continue;
		matches++;
	}

	listUnlock(list);

	return matches;
}

#ifndef NO_STR_LIST_SUPPORT

str_list_t DLLCALL listStringList(link_list_t* list)
{
	list_node_t*	node;
	str_list_t		str_list;
	size_t			count=0;

	if(list==NULL)
		return(NULL);

	if((str_list=strListInit())==NULL)
		return(NULL);

	listLock(list);

	for(node=list->first; node!=NULL; node=node->next) {
		if(node->data!=NULL)
			strListAppend(&str_list, (char*)node->data, count++);
	}

	listUnlock(list);

	return(str_list);
}

str_list_t DLLCALL listSubStringList(const list_node_t* node, long max)
{
	long			count;
	str_list_t		str_list;
	link_list_t*	list;

	if(node==NULL)
		return(NULL);

	if((str_list=strListInit())==NULL)
		return(NULL);

	list=node->list;
	listLock(list);

	for(count=0; count<max && node!=NULL; node=node->next) {
		if(node->data!=NULL)
			strListAppend(&str_list, (char*)node->data, count++);
	}

	listUnlock(list);

	return(str_list);
}

void* DLLCALL listFreeStringList(str_list_t list)
{
	strListFree(&list);
	return(list);
}

#endif	/* #ifndef NO_STR_LIST_SUPPORT */

list_node_t* DLLCALL listFirstNode(link_list_t* list)
{
	list_node_t*	node;

	if(list==NULL)
		return(NULL);

	listLock(list);
	node=list->first;
	listUnlock(list);

	return(node);
}

list_node_t* DLLCALL listLastNode(link_list_t* list)
{
	list_node_t* node;
	list_node_t* last=NULL;

	if(list==NULL)
		return(NULL);

	listLock(list);
	if(list->last!=NULL)
		last=list->last;
	else
		for(node=list->first; node!=NULL; node=node->next)
			last=node;
	listUnlock(list);

	return(last);
}

long DLLCALL listNodeIndex(link_list_t* list, list_node_t* find_node)
{
	long			i=0;
	list_node_t*	node;

	if(list==NULL)
		return(-1);

	listLock(list);

	for(node=list->first; node!=NULL; node=node->next)
		if(node==find_node)
			break;

	listUnlock(list);

	if(node==NULL)
		return(-1);

	return(i);
}

list_node_t* DLLCALL listNodeAt(link_list_t* list, long index)
{
	long			i=0;
	list_node_t*	node;

	if(list==NULL || index<0)
		return(NULL);

	listLock(list);

	for(node=list->first; node!=NULL && i<index; node=node->next)
		i++;

	listUnlock(list);

	return(node);
}

list_node_t* DLLCALL listNextNode(const list_node_t* node)
{
	list_node_t*	next;

	if(node==NULL)
		return(NULL);

	listLock(node->list);
	next=node->next;
	listUnlock(node->list);

	return(next);
}

list_node_t* DLLCALL listPrevNode(const list_node_t* node)
{
	list_node_t*	prev;

	if(node==NULL)
		return(NULL);

	listLock(node->list);
	prev=node->prev;
	listUnlock(node->list);

	return(prev);
}

void* DLLCALL listNodeData(const list_node_t* node)
{
	void*	data;

	if(node==NULL)
		return(NULL);

	listLock(node->list);
	data=node->data;
	listUnlock(node->list);

	return(data);
}

BOOL DLLCALL listNodeIsLocked(const list_node_t* node)
{
	return(node!=NULL && (node->flags&LINK_LIST_LOCKED));
}

BOOL DLLCALL listLockNode(list_node_t* node)
{
	if(node==NULL || (node->flags&LINK_LIST_LOCKED))
		return(FALSE);

	listLock(node->list);
	node->flags|=LINK_LIST_LOCKED;
	listUnlock(node->list);

	return(TRUE);
}

BOOL DLLCALL listUnlockNode(list_node_t* node)
{
	if(!listNodeIsLocked(node))
		return(FALSE);

	listLock(node->list);
	node->flags&=~LINK_LIST_LOCKED;
	listUnlock(node->list);

	return(TRUE);
}

static list_node_t* DLLCALL list_add_node(link_list_t* list, list_node_t* node, list_node_t* after)
{
	if(list==NULL)
		return(NULL);

	listLock(list);

	node->list = list;
	if(after==LAST_NODE)					/* e.g. listPushNode() */
		after=list->last;
	node->prev = after;

	if(after==list->last)					/* append to list */
		list->last = node;
	if(after==FIRST_NODE) {					/* insert at beginning of list */
		node->next = list->first;
		if(node->next!=NULL)
			node->next->prev = node;
		list->first = node;
	} else {
		if(after->next!=NULL) {
			after->next->prev = node;
			node->next = after->next;
		}
		after->next = node;
	}

	list->count++;

	listUnlock(list);

#if defined(LINK_LIST_THREADSAFE)
	if(list->flags&LINK_LIST_SEMAPHORE)
		listSemPost(list);
#endif

	return(node);
}

list_node_t* DLLCALL listAddNode(link_list_t* list, void* data, list_node_tag_t tag, list_node_t* after)
{
	list_node_t* node;

	if(list==NULL)
		return(NULL);

	if((node=(list_node_t*)malloc(sizeof(list_node_t)))==NULL)
		return(NULL);

	memset(node,0,sizeof(list_node_t));
	node->data = data;
	node->tag = tag;

	return(list_add_node(list,node,after));
}

long DLLCALL listAddNodes(link_list_t* list, void** data, list_node_tag_t* tag, list_node_t* after)
{
	long			i;
	list_node_t*	node=NULL;

	if(data==NULL)
		return(-1);

	for(i=0; data[i]!=NULL ;i++)
		if((node=listAddNode(list,data[i],tag==NULL ? LIST_NODE_TAG_DEFAULT : *(tag++),node==NULL ? after:node))==NULL)
			return(i);

	return(i);
}

list_node_t* DLLCALL listAddNodeData(link_list_t* list, const void* data, size_t length, list_node_tag_t tag, list_node_t* after)
{
	list_node_t*	node;
	void*			buf;

	if((buf=malloc(length))==NULL)
		return(NULL);
	memcpy(buf,data,length);

	if((node=listAddNode(list,buf,tag,after))==NULL) {
		free(buf);
		return(NULL);
	}
	node->flags |= LINK_LIST_MALLOC;

	return(node);
}

list_node_t* DLLCALL listAddNodeString(link_list_t* list, const char* str, list_node_tag_t tag, list_node_t* after)
{
	list_node_t*	node;
	char*			buf;

	if(str==NULL)
		return(NULL);

	if((buf=strdup(str))==NULL)
		return(NULL);

	if((node=listAddNode(list,buf,tag,after))==NULL) {
		free(buf);
		return(NULL);
	}
	node->flags |= LINK_LIST_MALLOC;

	return(node);
}

#ifndef NO_STR_LIST_SUPPORT

long DLLCALL listAddStringList(link_list_t* list, str_list_t str_list, list_node_tag_t* tag, list_node_t* after)
{
	long			i;
	list_node_t*	node=NULL;

	if(str_list==NULL)
		return(-1);

	for(i=0; str_list[i]!=NULL ;i++)
		if((node=listAddNodeString(list,str_list[i],tag==NULL ? LIST_NODE_TAG_DEFAULT : *(tag++),node==NULL ? after:node))==NULL)
			return(i);

	return(i);
}

#endif

long DLLCALL listAddNodeList(link_list_t* list, const link_list_t* src, list_node_t* after)
{
	long			count=0;
	list_node_t*	node=NULL;
	list_node_t*	src_node;

	if(src==NULL)
		return(-1);

	for(src_node=src->first; src_node!=NULL; src_node=src_node->next, count++) {
		if((node=listAddNode(list, src_node->data, src_node->tag, node==NULL ? after:node))==NULL)
			return(count);
		node->flags = src_node->flags;
	}

	return(count);
}

long DLLCALL listMerge(link_list_t* list, const link_list_t* src, list_node_t* after)
{
	long			count=0;
	list_node_t*	node=NULL;
	list_node_t*	src_node;

	if(src==NULL)
		return(-1);

	for(src_node=src->first; src_node!=NULL; src_node=src_node->next, count++)
		if((node=list_add_node(list, src_node, node==NULL ? after:node))==NULL)
			return(count);

	return(count);
}

link_list_t* DLLCALL listExtract(link_list_t* dest_list, const list_node_t* node, long max)
{
	long			count;
	link_list_t*	list;

	if(node==NULL || node->list==NULL)
		return(NULL);

	if((list=listInit(dest_list, node->list->flags))==NULL)
		return(NULL);

	for(count=0; count<max && node!=NULL; node=node->next) {
		listAddNode(list, node->data, node->tag, list->last);
		count++;
	}

	return(list);
}

static void* list_remove_node(link_list_t* list, list_node_t* node, BOOL free_data)
{
	void*	data;

	if(node==FIRST_NODE)
		node=list->first;
	else if(node==LAST_NODE)
		node=list->last;
	if(node==NULL)
		return(NULL);

	if(node->flags&LINK_LIST_LOCKED)
		return(NULL);

	if(node->prev!=NULL)
		node->prev->next = node->next;
	if(node->next!=NULL)
		node->next->prev = node->prev;
	if(list->first==node)
		list->first = node->next;
	if(list->last==node)
		list->last = node->prev;

	if(free_data)
		listFreeNodeData(node);

	data = node->data;

	free(node);

	if(list->count)
		list->count--;

	return(data);
}

void* DLLCALL listRemoveNode(link_list_t* list, list_node_t* node, BOOL free_data)
{
	void*	data;

	if(list==NULL)
		return(NULL);

	listLock(list);

	data = list_remove_node(list, node, free_data);

	listUnlock(list);

	return(data);
}

void* DLLCALL listRemoveTaggedNode(link_list_t* list, list_node_tag_t tag, BOOL free_data)
{
	void*			data=NULL;
	list_node_t*	node;

	if(list==NULL)
		return(NULL);

	listLock(list);

	if((node=listFindTaggedNode(list, tag)) != NULL)
		data = list_remove_node(list, node, free_data);

	listUnlock(list);

	return(data);
}

long DLLCALL listRemoveNodes(link_list_t* list, list_node_t* node, long max, BOOL free_data)
{
	list_node_t	*next_node;
	long count;

	if(list==NULL)
		return(-1);

	listLock(list);

	if(node==FIRST_NODE)
		node=list->first;
	if(node==LAST_NODE)
		node=list->last;

	for(count=0; node!=NULL && count<max; node=next_node, count++) {
		next_node = node->next;
		if(listRemoveNode(list, node, free_data)==NULL)
			break;
	}

	listUnlock(list);

	return(count);
}

BOOL DLLCALL listSwapNodes(list_node_t* node1, list_node_t* node2)
{
	list_node_t	tmp;

	if(node1==NULL || node2==NULL || node1==node2)
		return(FALSE);

	if(listNodeIsLocked(node1) || listNodeIsLocked(node2))
		return(FALSE);

	if(node1->list==NULL || node2->list==NULL)
		return(FALSE);

#if defined(LINK_LIST_THREADSAFE)
	listLock(node1->list);
	if(node1->list != node2->list)
		listLock(node2->list);
#endif

	tmp=*node1;
	node1->tag=node2->tag;
	node1->data=node2->data;
	node1->flags=node2->flags;
	node2->tag=tmp.tag;
	node2->data=tmp.data;
	node2->flags=tmp.flags;

#if defined(LINK_LIST_THREADSAFE)
	listUnlock(node1->list);
	if(node1->list != node2->list)
		listUnlock(node2->list);
#endif

	return(TRUE);
}

static void list_update_prev(link_list_t* list)
{
	list_node_t* node;
	list_node_t* prev = NULL;

	if(list == NULL)
		return;

	node = list->first;
	while(node != NULL) {
		node->prev = prev;
		prev = node;
		node = node->next;
	}
}

void DLLCALL listReverse(link_list_t* list)
{
	list_node_t* node;
	list_node_t* prev;

	if(list == NULL)
		return;

	node = list->first;

	if(node == NULL)
		return;

	listLock(list);

	list->last = list->first;

	prev = NULL;
	while(node != NULL) {
		list_node_t* next = node->next;
		node->next = prev;
		prev = node;
		node = next;
	}

	list->first = prev;

	list_update_prev(list);

	listUnlock(list);
}

long DLLCALL listVerify(link_list_t* list)
{
	list_node_t* node;
	list_node_t* prev = NULL;
	long result = 0;

	if(list == NULL)
		return -1;

	listLock(list);

	node = list->first;
	while(node != NULL) {
		if(node->list != list) {
			result = -2;
			break;
		}
		if(node->prev != prev) {
			result = -3;
			break;
		}
		prev = node;
		node = node->next;
		result++;
	}
	if(result >= 0 && list->last != prev)
		result = -4;

	if(result >= 0 && result != list->count)
		result = -5;

	listUnlock(list);

	return result;
}

#if 0

#include <stdio.h>	/* printf, sprintf */

int main(int arg, char** argv)
{
	int		i;
	long	result;
	char*	p;
	char	str[32];
	link_list_t list;

	listInit(&list,0);
	if((result = listVerify(&list)) < 0) {
		fprintf(stderr, "line %d: listVerify() returned %ld\n", __LINE__, result);
		return EXIT_FAILURE;
	}

	for(i=0; i<100; i++) {
		sprintf(str,"%u",i);
		listPushNodeString(&list,str);
	}
	if((result = listVerify(&list)) < 0) {
		fprintf(stderr, "line %d: listVerify() returned %ld\n", __LINE__, result);
		return EXIT_FAILURE;
	}

	listReverse(&list);
	if((result = listVerify(&list)) < 0) {
		fprintf(stderr, "line %d: listVerify() returned %ld\n", __LINE__, result);
		return EXIT_FAILURE;
	}

	while((p=listShiftNode(&list))!=NULL)
		printf("%d %s\n",listCountNodes(&list),p), free(p);
	if((result = listVerify(&list)) < 0) {
		fprintf(stderr, "line %d: listVerify() returned %ld\n", __LINE__, result);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

#endif
