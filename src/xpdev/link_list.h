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

#if defined(__cplusplus)
extern "C" {
#endif

/* Valid link_list_t.flags bits */
#define LINK_LIST_MALLOC		(1<<0)	/* List allocated with malloc() */
#define LINK_LIST_AUTO_FREE		(1<<1)	/* Free node data automatically */

typedef struct list_node {
	void*				data;		/* pointer to some kind of data */
	struct list_node*	next;		/* next node in list (or NULL) */
	struct list_node*	prev;		/* previous node in list (or NULL) */
} list_node_t;

typedef struct {
	list_node_t*		first;		/* first node in list (or NULL) */
	list_node_t*		last;		/* last node in list (or NULL) */
	unsigned long		flags;		/* flags passed to listInit() */
	size_t				count;		/* number of nodes in list */
} link_list_t;

/* Initialization, Allocation, and Freeing of Lists and Nodes */
link_list_t*	listInit(link_list_t*, unsigned long flags);
link_list_t*	listFree(link_list_t*);
void			listFreeNodes(link_list_t*);
void			listFreeNodeData(list_node_t* node);

/* Convenience functions */
size_t			listCountNodes(const link_list_t*);
list_node_t*	listFirstNode(const link_list_t*);
list_node_t*	listLastNode(const link_list_t*);
list_node_t*	listNextNode(const list_node_t*);
list_node_t*	listPrevNode(const list_node_t*);
void*			listNodeData(const list_node_t*);

/* Add node to list, returns pointer to new node */
list_node_t*	listAddNode(link_list_t*, void* data, list_node_t* after);
list_node_t*	listPushNode(link_list_t*, void* data);
list_node_t*	listInsertNode(link_list_t*, void* data);

/* Add node to list, allocating and copying the data for the node */
list_node_t*	listAddNodeData(link_list_t*, const void* data, size_t length, list_node_t* after);
list_node_t*	listPushNodeData(link_list_t*, const void* data, size_t length);
list_node_t*	listInsertNodeData(link_list_t*, const void* data, size_t length);

/* Add node to list, allocating and copying string (ACIIZ / null-terminated char*) */
list_node_t*	listAddNodeString(link_list_t*, const char* str, list_node_t* after);
list_node_t*	listPushNodeString(link_list_t*, const char* str);
list_node_t*	listInsertNodeString(link_list_t*, const char* str);

/* Remove node from list, returning the node's data (if not free'd) */
void*			listPopNode(link_list_t*);
void*			listRemoveNode(link_list_t*, list_node_t*);
void			listRemoveNodeData(link_list_t* list, list_node_t* node);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
