/* Double-Linked-list library */

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

#ifndef _LINK_LIST_H
#define _LINK_LIST_H

#include <stddef.h>		/* size_t */
#include "wrapdll.h"
#include "str_list.h"	/* string list functions and types */

#if defined(LINK_LIST_THREADSAFE)
	#include "threadwrap.h"	/* mutexes */
	#include "semwrap.h"	/* semaphores */
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#define FIRST_NODE				((list_node_t*)NULL)	/* Special value to specify first node in list */
#define LAST_NODE				((list_node_t*)-1)		/* Special value to specify last node in list */

/* Valid link_list_t.flags and list_node_t.flags bits */
#define LINK_LIST_MALLOC		(1<<0)	/* Node data allocated with malloc() */
#define LINK_LIST_ALWAYS_FREE	(1<<1)	/* ALWAYS free node data in listFreeNodes() */
#define LINK_LIST_NEVER_FREE	(1<<2)	/* NEVER free node data (careful of memory leaks!) */
#define LINK_LIST_MUTEX			(1<<3)	/* Mutex-protected linked-list */
#define LINK_LIST_SEMAPHORE		(1<<4)	/* Semaphore attached to linked-list */
#define LINK_LIST_LOCKED		(1<<5)	/* Node is locked */
#define LINK_LIST_ATTACH		(1<<6)	/* Attach during init */

/* in case the default tag type is not sufficient for your needs, you can over-ride */
#if !defined(list_node_tag_t)			
	typedef long list_node_tag_t;
#endif
#if !defined(LIST_NODE_TAG_DEFAULT)
	#define LIST_NODE_TAG_DEFAULT	0
#endif

typedef struct list_node {
	void*				data;			/* pointer to some kind of data */
	struct list_node*	next;			/* next node in list (or NULL) */
	struct list_node*	prev;			/* previous node in list (or NULL) */
	struct link_list*	list;
	unsigned long		flags;			/* private use flags (by this library) */
	list_node_tag_t		tag;			/* application use value */
} list_node_t;

typedef struct link_list {
	list_node_t*		first;			/* first node in list (or NULL) */
	list_node_t*		last;			/* last node in list (or NULL) */
	unsigned long		flags;			/* private use flags (by this library) */
	long				count;			/* number of nodes in list */
	void*				private_data;	/* for use by the application/caller */
	long				refs;			/* reference counter (attached clients) */
	long				locks;			/* recursive lock counter */
#if defined(LINK_LIST_THREADSAFE)
	pthread_mutex_t		mutex;
	sem_t				sem;
#endif
} link_list_t;

/* Initialization, Allocation, and Freeing of Lists and Nodes */
DLLEXPORT link_list_t*	listInit(link_list_t* /* NULL to auto-allocate */, long flags);
DLLEXPORT BOOL			listFree(link_list_t*);
DLLEXPORT long			listFreeNodes(link_list_t*);
DLLEXPORT BOOL			listFreeNodeData(list_node_t* node);

/* Increment/decrement reference counter (and auto-free when zero), returns -1 on error */
DLLEXPORT long	listAttach(link_list_t*);
DLLEXPORT long	listDetach(link_list_t*);

#if defined(LINK_LIST_THREADSAFE)
DLLEXPORT BOOL	listSemPost(link_list_t*);
DLLEXPORT BOOL	listSemWait(link_list_t*);
DLLEXPORT BOOL	listSemTryWait(link_list_t*);
DLLEXPORT BOOL	listSemTryWaitBlock(link_list_t*, unsigned long timeout);
#endif

/* Lock/unlock linked lists (works best for mutex-protected lists) */
/* Locks are recursive (e.g. must call Unlock for each call to Lock */
DLLEXPORT BOOL	listLock(link_list_t*);
DLLEXPORT BOOL	listUnlock(link_list_t*);
DLLEXPORT BOOL	listIsLocked(const link_list_t*);
#define	listForceUnlock(list)	while(listUnlock(list)==TRUE)

/* Return count or index of nodes, or -1 on error */
DLLEXPORT long	listCountNodes(link_list_t*);
DLLEXPORT long	listNodeIndex(link_list_t*, list_node_t*);

/* Get/Set list private data */
DLLEXPORT void*	listSetPrivateData(link_list_t*, void*);
DLLEXPORT void*	listGetPrivateData(link_list_t*);

/* Return an allocated string list (which must be freed), array of all strings in linked list */
DLLEXPORT str_list_t listStringList(link_list_t*);

/* Return an allocated string list (which must be freed), subset of strings in linked list */
DLLEXPORT str_list_t listSubStringList(const list_node_t*, long max);

/* Free a string list returned from either of the above functions */
DLLEXPORT void*	listFreeStringList(str_list_t);

/* Extract subset (up to max number of nodes) in linked list (src_node) and place into dest_list */
/* dest_list == NULL, then allocate a return a new linked list */
DLLEXPORT link_list_t*	listExtract(link_list_t* dest_list, const list_node_t* src_node, long max);

/* Simple search functions returning found node or NULL on error */
DLLEXPORT list_node_t*	listNodeAt(link_list_t*, long index);
/* Find a specific node by data or tag */
/* Pass length of 0 to search by data pointer rather than by data content comparison (memcmp) */
DLLEXPORT list_node_t*	listFindNode(link_list_t*, const void* data, size_t length);
/* Find a specific node by its tag value */
#define listFindTaggedNode(list, tag)	listFindNode(list, NULL, tag)
/* Pass length of 0 to search by data pointer rather than by data content comparison (memcmp) */
DLLEXPORT ulong			listCountMatches(link_list_t*, const void* data, size_t length);

/* Convenience functions */
DLLEXPORT list_node_t*	listFirstNode(link_list_t*);
DLLEXPORT list_node_t*	listLastNode(link_list_t*);
DLLEXPORT list_node_t*	listNextNode(const list_node_t*);
DLLEXPORT list_node_t*	listPrevNode(const list_node_t*);
DLLEXPORT void*			listNodeData(const list_node_t*);

/* Primitive node locking (not recursive) */
DLLEXPORT BOOL listLockNode(list_node_t*);
DLLEXPORT BOOL listUnlockNode(list_node_t*);
DLLEXPORT BOOL listNodeIsLocked(const list_node_t*);

/* Add node to list, returns pointer to new node or NULL on error */
DLLEXPORT list_node_t*	listAddNode(link_list_t*, void* data, list_node_tag_t, list_node_t* after /* NULL=insert */);

/* Add array of node data to list, returns number of nodes added (or negative on error) */
/* tag array may be NULL */
DLLEXPORT long		listAddNodes(link_list_t*, void** data, list_node_tag_t*, list_node_t* after /* NULL=insert */);

/* Add node to list, allocating and copying the data for the node */
DLLEXPORT list_node_t*	listAddNodeData(link_list_t*, const void* data, size_t length, list_node_tag_t, list_node_t* after);

/* Add node to list, allocating and copying ASCIIZ string data */
DLLEXPORT list_node_t*	listAddNodeString(link_list_t*, const char* str, list_node_tag_t, list_node_t* after);

/* Add a list of strings to the linked list, allocating and copying each */
/* tag array may be NULL */
DLLEXPORT long		listAddStringList(link_list_t*, str_list_t, list_node_tag_t*, list_node_t* after);

/* Add a list of nodes from a source linked list */
DLLEXPORT long		listAddNodeList(link_list_t*, const link_list_t* src, list_node_t* after); 

/* Merge a source linked list into the destination linked list */
/* after merging, the nodes in the source linked list should not be modified or freed */
DLLEXPORT long		listMerge(link_list_t* dest, const link_list_t* src, list_node_t* after);

/* Swap the data pointers and flags for 2 nodes (possibly in separate lists) */
DLLEXPORT BOOL		listSwapNodes(list_node_t* node1, list_node_t* node2);

/* Convenience macros for pushing, popping, and inserting nodes */
#define	listPushNode(list, data)				listAddNode(list, data, LIST_NODE_TAG_DEFAULT, LAST_NODE)
#define listInsertNode(list, data)				listAddNode(list, data, LIST_NODE_TAG_DEFAULT, FIRST_NODE)
#define listPushNodeData(list, data, length)	listAddNodeData(list, data, length, LIST_NODE_TAG_DEFAULT, LAST_NODE)
#define	listInsertNodeData(list, data, length)	listAddNodeData(list, data, length, LIST_NODE_TAG_DEFAULT, FIRST_NODE)
#define	listPushNodeString(list, str)			listAddNodeString(list, str, LIST_NODE_TAG_DEFAULT, LAST_NODE)
#define listInsertNodeString(list, str)			listAddNodeString(list, str, LIST_NODE_TAG_DEFAULT, FIRST_NODE)
#define	listPushStringList(list, str_list)		listAddStringList(list, str_list, NULL, LAST_NODE)
#define listInsertStringList(list, str_list)	listAddStringList(list, str_list, NULL, FIRST_NODE)
#define listPopNode(list)						listRemoveNode(list, LAST_NODE, FALSE)
#define listShiftNode(list)						listRemoveNode(list, FIRST_NODE, FALSE)

/* Remove node from list, returning the node's data (if not free'd) */
DLLEXPORT void*	listRemoveNode(link_list_t*, list_node_t* /* NULL=first */, BOOL free_data);
DLLEXPORT void* listRemoveTaggedNode(link_list_t*, list_node_tag_t, BOOL free_data);

/* Remove multiple nodes from list, returning the number of nodes removed */
DLLEXPORT long	listRemoveNodes(link_list_t*, list_node_t* /* NULL=first */, long count, BOOL free_data);

/* Reverse the nodes in a list */
DLLEXPORT void listReverse(link_list_t*);

/* Return >= 0 (count of nodes) if list is valid, negative otherwise */
DLLEXPORT long listVerify(link_list_t*);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
