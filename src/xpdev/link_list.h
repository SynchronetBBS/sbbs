/* link_list.h */

/* Double-Linked-list library */

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

#ifndef _LINK_LIST_H
#define _LINK_LIST_H

#include <stddef.h>		/* size_t */
#include "str_list.h"	/* string list functions and types */

#if defined(LINK_LIST_THREADSAFE)
	#include "threadwrap.h"	/* mutexes */
	#include "semwrap.h"	/* semaphores */
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#define FIRST_NODE				NULL	/* Special value to specify first node in list */

/* Valid link_list_t.flags bits */
#define LINK_LIST_MALLOC		(1<<0)	/* List/node allocated with malloc() */
#define LINK_LIST_ALWAYS_FREE	(1<<1)	/* ALWAYS free node data when removing */
#define LINK_LIST_NEVER_FREE	(1<<2)	/* NEVER free node data when removing */
#define LINK_LIST_MUTEX			(1<<3)	/* Mutex-protected linked-list */
#define LINK_LIST_SEMAPHORE		(1<<4)	/* Semaphore attached to linked-list */
#define LINK_LIST_NODE_LOCKED	(1<<5)	/* Node is locked */

typedef struct list_node {
	void*				data;			/* pointer to some kind of data */
	struct list_node*	next;			/* next node in list (or NULL) */
	struct list_node*	prev;			/* previous node in list (or NULL) */
	struct link_list*	list;
	unsigned long		flags;			/* private use flags */
} list_node_t;

typedef struct link_list {
	list_node_t*		first;			/* first node in list (or NULL) */
	list_node_t*		last;			/* last node in list (or NULL) */
	unsigned long		flags;			/* private use flags */
	long				count;			/* number of nodes in list */
	void*				private_data;	/* for use by the application only */
#if defined(LINK_LIST_THREADSAFE)
	pthread_mutex_t		mutex;
	sem_t				sem;
#endif
} link_list_t;

/* Initialization, Allocation, and Freeing of Lists and Nodes */
link_list_t*	listInit(link_list_t* /* NULL to auto-allocate */, long flags);
BOOL			listFree(link_list_t*);
long			listFreeNodes(link_list_t*);
BOOL			listFreeNodeData(list_node_t* node);

#if defined(LINK_LIST_THREADSAFE)
BOOL			listSemPost(const link_list_t*);
BOOL			listSemWait(const link_list_t*);
BOOL			listSemTryWait(const link_list_t*);
BOOL			listSemTryWaitBlock(const link_list_t*, unsigned long timeout);
#endif

/* Lock/unlock mutex-protected linked lists (no-op for unprotected lists) */
void			listLock(const link_list_t*);
void			listUnlock(const link_list_t*);

/* Return count or index of nodes, or -1 on error */
long			listCountNodes(const link_list_t*);
long			listNodeIndex(const link_list_t*, list_node_t*);

/* Get/Set list private data */
void*			listSetPrivateData(link_list_t*, void*);
void*			listGetPrivateData(link_list_t*);

/* Return an allocated string list (which must be freed), array of all strings in linked list */
str_list_t		listStringList(const link_list_t*);

/* Return an allocated string list (which must be freed), subset of strings in linked list */
str_list_t		listSubStringList(const list_node_t*, long max);

/* Extract subset (up to max number of nodes) in linked list (src_node) and place into dest_list */
/* dest_list == NULL, then allocate a return a new linked list */
link_list_t*	listExtract(link_list_t* dest_list, const list_node_t* src_node, long max);

/* Simple search functions returning found node or NULL on error */
list_node_t*	listNodeAt(const link_list_t*, long index);
list_node_t*	listFindNode(const link_list_t*, void* data, size_t length);

/* Convenience functions */
list_node_t*	listFirstNode(const link_list_t*);
list_node_t*	listLastNode(const link_list_t*);
list_node_t*	listNextNode(const list_node_t*);
list_node_t*	listPrevNode(const list_node_t*);
void*			listNodeData(const list_node_t*);

/* Primitive node locking */
BOOL			listLockNode(list_node_t*);
BOOL			listUnlockNode(list_node_t*);
BOOL			listNodeIsLocked(const list_node_t*);

/* Add node to list, returns pointer to new node or NULL on error */
list_node_t*	listAddNode(link_list_t*, void* data, list_node_t* after /* NULL=insert */);

/* Add array of node data to list, returns number of nodes added (or negative on error) */
long			listAddNodes(link_list_t*, void** data, list_node_t* after /* NULL=insert */);

/* Add node to list, allocating and copying the data for the node */
list_node_t*	listAddNodeData(link_list_t*, const void* data, size_t length, list_node_t* after);

/* Add node to list, allocating and copying ASCIIZ string data */
list_node_t*	listAddNodeString(link_list_t*, const char* str, list_node_t* after);

/* Add a list of strings to the linked list, allocating and copying each */
long			listAddStringList(link_list_t*, str_list_t, list_node_t* after);

/* Add a list of nodes from a source linked list */
long			listAddNodeList(link_list_t*, const link_list_t* src, list_node_t* after); 

/* Merge a source linked list into the destination linked list */
/* after merging, the nodes in the source linked list should not be modified or freed */
long			listMerge(link_list_t* dest, const link_list_t* src, list_node_t* after);

/* Swap the data pointers and flags for 2 nodes (possibly in separate lists) */
BOOL			listSwapNodes(list_node_t* node1, list_node_t* node2);

/* Convenience macros for pushing, popping, and inserting nodes */
#define	listPushNode(list, data)				listAddNode(list, data, listLastNode(list))
#define listInsertNode(link, data)				listAddNode(list, data, FIRST_NODE)
#define listPushNodeData(list, data, length)	listAddNodeData(list, data, length, listLastNode(list))
#define	listInsertNodeData(list, data, length)	listAddNodeData(list, data, length, FIRST_NODE)
#define	listPushNodeString(list, str)			listAddNodeString(list, str, listLastNode(list))
#define listInsertNodeString(list, str)			listAddNodeString(list, str, FIRST_NODE)
#define	listPushStringList(list, str_list)		listAddStringList(list, str_list, listLastNode(list))
#define listInsertStringList(list, str_list)	listAddStringList(list, str_list, FIRST_NODE)
#define listPopNode(list)						listRemoveNode(list, listLastNode(list))

/* Remove node from list, returning the node's data (if not free'd) */
void*			listRemoveNode(link_list_t*, list_node_t* /* NULL=first */);

/* Remove multiple nodes from list, returning the number of nodes removed */
long			listRemoveNodes(link_list_t*, list_node_t* /* NULL=first */, long count);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
