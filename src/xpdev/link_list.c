/* link_list.c */

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

#include <stdlib.h>		/* malloc */
#include <string.h>		/* memset */
#include "link_list.h"

link_list_t* listInit(link_list_t* list, unsigned long flags)
{
	if(flags&LINK_LIST_MALLOC || list==NULL) {
		if((list=(link_list_t*)malloc(sizeof(link_list_t)))==NULL)
			return(NULL);
		flags |= LINK_LIST_MALLOC;
	} 

	memset(list,0,sizeof(link_list_t));

	list->flags = flags;

	return(list);
}

void listFreeNodeData(list_node_t* node)
{
	if(node!=NULL && node->data!=NULL) {
		free(node->data);
		node->data = NULL;
	}
}

void listFreeNodes(link_list_t* list)
{
	list_node_t* node;
	list_node_t* next;

	for(node=list->first; node!=NULL; node=next) {

		if(list->flags&LINK_LIST_AUTO_FREE)
			listFreeNodeData(node);

		next = node->next;

		free(node);
	}

	list->first = NULL;
	list->last = NULL;
	list->count = 0;
}

link_list_t* listFree(link_list_t* list)
{
	if(list==NULL)
		return(NULL);

	listFreeNodes(list);

	if(list->flags&LINK_LIST_MALLOC)
		free(list), list=NULL;

	return(list);
}

size_t listCountNodes(const link_list_t* list)
{
	size_t count=0;
	list_node_t* node;

	if(list==NULL)
		return(0);

	if(list->count)
		return(list->count);

	for(node=list->first; node!=NULL; node=node->next)
		count++;

	return(count);
}

list_node_t* listFirstNode(const link_list_t* list)
{
	if(list==NULL)
		return(NULL);

	return(list->first);
}

list_node_t* listLastNode(const link_list_t* list)
{
	list_node_t* node;
	list_node_t* last=NULL;

	if(list==NULL)
		return(NULL);

	if(list->last!=NULL)
		return(list->last);

	for(node=list->first; node!=NULL; node=node->next)
		last=node;

	return(last);
}

list_node_t* listNextNode(const list_node_t* node)
{
	if(node==NULL)
		return(NULL);

	return(node->next);
}

list_node_t* listPrevNode(const list_node_t* node)
{
	if(node==NULL)
		return(NULL);

	return(node->prev);
}

void* listNodeData(const list_node_t* node)
{
	if(node==NULL)
		return(NULL);

	return(node->data);
}

list_node_t* listAddNode(link_list_t* list, void* data, list_node_t* after)
{
	list_node_t* node;

	if(list==NULL)
		return(NULL);

	if((node=(list_node_t*)malloc(sizeof(list_node_t)))==NULL)
		return(NULL);

	memset(node,0,sizeof(list_node_t));

	node->data = data;
	node->prev = after;

	if(after==list->last)					/* append to list */
		list->last = node;
	if(after==NULL && list->first!=NULL) {	/* insert at beginning of list */
		list->first->prev = node;
		list->first = node;
	}
	if(after!=NULL) {
		if(after->next!=NULL) {
			after->next->prev = node;
			node->next = after->next;
		}
		after->next = node;
	}

	list->count++;

	return(node);
}


list_node_t* listPushNode(link_list_t* list, void* data)
{
	return(listAddNode(list, data, listLastNode(list)));
}

list_node_t* listInsertNode(link_list_t* list, void* data)
{
	return(listAddNode(list, data, NULL));	
}

list_node_t* listAddNodeData(link_list_t* list, const void* data, size_t length, list_node_t* after)
{
	list_node_t*	node;
	void*			buf;

	if((buf=malloc(length))==NULL)
		return(NULL);
	memcpy(buf,data,length);

	if((node=listAddNode(list,buf,after))==NULL) {
		free(buf);
		return(NULL);
	}
	
	return(node);
}

list_node_t* listPushNodeData(link_list_t* list, const void* data, size_t length)
{
	return(listAddNodeData(list, data, length, listLastNode(list)));
}

list_node_t* listInsertNodeData(link_list_t* list, const void* data, size_t length)
{
	return(listAddNodeData(list, data, length, NULL));	
}

list_node_t* listAddNodeString(link_list_t* list, const char* str, list_node_t* after)
{
	list_node_t*	node;
	char*			buf;
	size_t			length;

	if(str==NULL)
		return(NULL);

	length = strlen(str)+1;

	if((buf=malloc(length))==NULL)
		return(NULL);
	memcpy(buf,str,length);

	if((node=listAddNode(list,buf,after))==NULL) {
		free(buf);
		return(NULL);
	}
	
	return(node);
}

list_node_t* listPushNodeString(link_list_t* list, const char* str)
{
	return(listAddNodeString(list, str, listLastNode(list)));
}

list_node_t* listInsertNodeString(link_list_t* list, const char* str)
{
	return(listAddNodeString(list, str, NULL));	
}

void* listRemoveNode(link_list_t* list, list_node_t* node)
{
	void*	data;

	if(list==NULL || node==NULL)
		return(NULL);

	if(node->prev!=NULL)
		node->prev->next = node->next;
	if(node->next!=NULL)
		node->next->prev = node->prev;
	if(list->first==node)
		list->first = node->next;
	if(list->last==node)
		list->last = node->prev;

	if(list->flags&LINK_LIST_AUTO_FREE)
		listFreeNodeData(node);
	data=node->data;

	free(node);

	if(list->count)
		list->count--;

	return(data);
}

void listRemoveNodeData(link_list_t* list, list_node_t* node)
{
	void*	data;

	if((data=listRemoveNode(list, node))!=NULL)
		free(data);
}


void* listPopNode(link_list_t* list)
{
	return(listRemoveNode(list, listLastNode(list)));
}
