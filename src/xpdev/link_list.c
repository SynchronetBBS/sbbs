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

link_list_t* listInit(link_list_t* list)
{
	unsigned long flags=0;

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

		if(list->flags&LINK_LIST_ALWAYS_FREE || node->flags&LINK_LIST_MALLOC)
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

long listCountNodes(const link_list_t* list)
{
	long count=0;
	list_node_t* node;

	if(list==NULL)
		return(-1);

	if(list->count)
		return(list->count);

	for(node=list->first; node!=NULL; node=node->next)
		count++;

	return(count);
}

list_node_t* listFindNode(const link_list_t* list, void* data, size_t length)
{
	list_node_t* node;

	if(list==NULL)
		return(NULL);

	for(node=list->first; node!=NULL; node=node->next)
		if(node->data!=NULL && memcmp(node->data,data,length)==0)
			break;

	return(node);
}

str_list_t listStringList(const link_list_t* list)
{
	list_node_t*	node;
	str_list_t		str_list;

	if(list==NULL)
		return(NULL);

	if((str_list=strListInit())==NULL)
		return(NULL);

	for(node=list->first; node!=NULL; node=node->next) {
		if(node->data!=NULL)
			strListAdd(&str_list, node->data);
	}

	return(str_list);
}

str_list_t listSubStringList(const list_node_t* node, long max)
{
	long			count=0;
	str_list_t		str_list;

	if(node==NULL)
		return(NULL);

	if((str_list=strListInit())==NULL)
		return(NULL);

	for(count=0; count<max && node!=NULL; node=node->next) {
		if(node->data!=NULL) {
			strListAdd(&str_list, node->data);
			count++;
		}
	}

	return(str_list);
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

long listNodeIndex(const link_list_t* list, list_node_t* find_node)
{
	long			i=0;
	list_node_t*	node;

	if(list==NULL)
		return(-1);

	for(node=list->first; node!=NULL; node=node->next)
		if(node==find_node)
			break;

	if(node==NULL)
		return(-1);

	return(i);
}

list_node_t* listNodeAt(const link_list_t* list, long index)
{
	long			i=0;
	list_node_t*	node;

	if(list==NULL || index<0)
		return(NULL);

	for(node=list->first; node!=NULL && i<index; node=node->next)
		i++;

	return(node);
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

static list_node_t* list_add_node(link_list_t* list, list_node_t* node, list_node_t* after)
{
	if(list==NULL)
		return(NULL);

	node->prev = after;

	if(after==list->last)					/* append to list */
		list->last = node;
	if(after==NULL) {						/* insert at beginning of list */
		if(list->first!=NULL)
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

list_node_t* listAddNode(link_list_t* list, void* data, list_node_t* after)
{
	list_node_t* node;

	if(list==NULL)
		return(NULL);

	if((node=(list_node_t*)malloc(sizeof(list_node_t)))==NULL)
		return(NULL);

	return(list_add_node(list,node,after));
}

long listAddNodes(link_list_t* list, void** data, list_node_t* after)
{
	long			i;
	list_node_t*	node=NULL;

	if(data==NULL)
		return(-1);

	for(i=0;data[i];i++)
		if((node=listAddNode(list,data[i],node==NULL ? after:node))==NULL)
			return(i);

	return(i);
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
	node->flags |= LINK_LIST_MALLOC;
	
	return(node);
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
	node->flags |= LINK_LIST_MALLOC;

	return(node);
}

long listAddStringList(link_list_t* list, str_list_t str_list, list_node_t* after)
{
	long			i;
	list_node_t*	node=NULL;

	if(str_list==NULL)
		return(-1);

	for(i=0;str_list[i];i++)
		if((node=listAddNodeString(list,str_list[i],node==NULL ? after:node))==NULL)
			return(i);

	return(i);
}

long listAddNodeList(link_list_t* list, const link_list_t* src, list_node_t* after)
{
	long			count=0;
	list_node_t*	node=NULL;
	list_node_t*	src_node;

	if(src==NULL)
		return(-1);

	for(src_node=src->first; src_node!=NULL; src_node=src_node->next, count++) {
		if((node=listAddNode(list, src_node->data, node==NULL ? after:node))==NULL)
			return(count);
		node->flags = src_node->flags;
	}

	return(count);
}

long listMerge(link_list_t* list, const link_list_t* src, list_node_t* after)
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

link_list_t* listExtract(link_list_t* dest_list, const list_node_t* node, long max)
{
	long			count=0;
	link_list_t*	list;

	if(node==NULL)
		return(NULL);

	if((list=listInit(dest_list))==NULL)
		return(NULL);

	for(count=0; count<max && node!=NULL; node=node->next) {
		listAddNode(list, node->data, list->last);
		count++;
	}

	return(list);
}

void* listRemoveNode(link_list_t* list, list_node_t* node)
{
	void*	data;

	if(list==NULL)
		return(NULL);

	if(node==NULL)
		node=list->first;
	if(node==NULL)
		return(NULL);

	if(node->prev!=NULL)
		node->prev->next = node->next;
	if(node->next!=NULL)
		node->next->prev = node->prev;
	if(list->first==node)
		list->first = node->next;
	if(list->last==node)
		list->last = node->prev;

	if(list->flags&LINK_LIST_ALWAYS_FREE || node->flags&LINK_LIST_MALLOC)
		listFreeNodeData(node);

	data = node->data;

	free(node);

	if(list->count)
		list->count--;

	return(data);
}

long listRemoveNodes(link_list_t* list, list_node_t* node, long max)
{
	long count;

	if(list==NULL)
		return(-1);

	if(node==NULL)
		node=list->first;

	for(count=0; node!=NULL && count<max; node=node->next, count++)
		listRemoveNode(list, node);
	
	return(count);
}

#if 0

#include <stdio.h>	/* printf, sprintf */

int main(int arg, char** argv)
{
	int		i;
	char*	p;
	char	str[32];
	link_list_t list;

	listInit(&list,0);
	for(i=0;i<100;i++) {
		sprintf(str,"%u",i);
		listPushNodeString(&list,str);
	}

	while((p=listRemoveNode(&list,NULL))!=NULL)
		printf("%d %s\n",listCountNodes(&list),p), free(p);

	gets(str);
	return 0;
}

#endif