/* str_list.c */

/* Functions to deal with NULL-terminated string lists */

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

#include <stdlib.h>		/* malloc and qsort */
#include <string.h>		/* strtok */
#include "genwrap.h"	/* stricmp */
#include "str_list.h"

str_list_t strListInit()
{
	str_list_t list;

	if((list=(str_list_t)malloc(sizeof(char*)))==NULL)
		return(NULL);

	list[0]=NULL;	/* terminated by default */
	return(list);
}

size_t strListCount(const str_list_t list)
{
	size_t i;

	if(list==NULL)
		return(0);

	for(i=0;list[i]!=NULL;i++)
		;

	return(i);
}

static str_list_t str_list_append(str_list_t* list, char* str, size_t index)
{
	str_list_t lp;

	if((lp=(str_list_t)realloc(*list,sizeof(char*)*(index+2)))==NULL)
		return(NULL);

	*list=lp;
	lp[index++]=str;
	lp[index]=NULL;	/* terminate list */

	return(lp);
}

static str_list_t str_list_insert(str_list_t* list, char* str, size_t index)
{
	size_t	i;
	size_t	count;
	str_list_t lp;

	count = strListCount(*list);
	if(index > count)	/* invalid index, do nothing */
		return(NULL);

	count++;
	if((lp=(str_list_t)realloc(*list,sizeof(char*)*(count+1)))==NULL)
		return(NULL);

	*list=lp;
	for(i=count; i>index; i--)
		lp[i]=lp[i-1];
	lp[index]=str;

	return(lp);
}

str_list_t strListRemove(str_list_t* list, size_t index)
{
	size_t	i;
	size_t	count;
	str_list_t lp;

	count = strListCount(*list);
	if(index >= count)	/* invalid index, do nothing */
		return(NULL);

	count--;
	if((lp=(str_list_t)realloc(*list,sizeof(char*)*(count+1)))==NULL)
		return(NULL);

	*list=lp;
	for(i=index; i<count; i++)
		lp[i]=lp[i+1];
	lp[count]=NULL;

	return(lp);
}

str_list_t strListAddAt(str_list_t* list, const char* str, size_t count)
{
	char* buf;

	if((buf=(char*)malloc(strlen(str)+1))==NULL)
		return(NULL);

	strcpy(buf,str);

	return(str_list_append(list,buf,count));
}

str_list_t strListAdd(str_list_t* list, const char* str)
{
	return strListAddAt(list,str,strListCount(*list));
}

str_list_t	strListAddList(str_list_t* list, const str_list_t add_list)
{
	size_t	i,j;

	j=strListCount(*list);
	for(i=0;add_list[i];i++)
		strListAddAt(list,add_list[i],j++);

	return(*list);
}

str_list_t strListInsert(str_list_t* list, const char* str, size_t index)
{
	char* buf;

	if((buf=(char*)malloc(strlen(str)+1))==NULL)
		return(NULL);

	strcpy(buf,str);

	return(str_list_insert(list,buf,index));
}

str_list_t	strListInsertList(str_list_t* list, const str_list_t add_list, size_t index)
{
	size_t	i;

	for(i=0;add_list[i];i++)
		strListInsert(list,add_list[i],index++);

	return(*list);
}

str_list_t strListSplit(str_list_t* list, char* str, const char* delimit)
{
	char*	token;

	if(list==NULL) {
		if((*list = strListInit())==NULL)
			return(NULL);
	}

	for(token = strtok(str, delimit); token!=NULL; token=strtok(NULL, delimit))
		strListAdd(list, token);

	return(*list);
}

str_list_t strListSplitCopy(str_list_t* list, const char* str, const char* delimit)
{
	char*	buf;

	if((buf=malloc(strlen(str)+1))==NULL)
		return(NULL);

	strcpy(buf,str);

	*list = strListSplit(list,buf,delimit);

	free(buf);

	return(*list);
}

str_list_t	strListMerge(str_list_t* list, str_list_t add_list)
{
	size_t	i,j;

	j=strListCount(*list);
	for(i=0;add_list[i];i++)
		str_list_append(list,add_list[i],j++);

	return(*list);
}

static int strListCompareAlpha(const void *arg1, const void *arg2)
{
   return stricmp(*(char**)arg1, *(char**)arg2);
}

static int strListCompareAlphaReverse(const void *arg1, const void *arg2)
{
   return stricmp(*(char**)arg2, *(char**)arg1);
}

static int strListCompareAlphaCase(const void *arg1, const void *arg2)
{
   return strcmp(*(char**)arg1, *(char**)arg2);
}

static int strListCompareAlphaCaseReverse(const void *arg1, const void *arg2)
{
   return strcmp(*(char**)arg2, *(char**)arg1);
}

void strListSortAlpha(str_list_t list)
{
	qsort(list,strListCount(list),sizeof(char*),strListCompareAlpha);
}

void strListSortAlphaReverse(str_list_t list)
{
	qsort(list,strListCount(list),sizeof(char*),strListCompareAlphaReverse);
}

void strListSortAlphaCase(str_list_t list)
{
	qsort(list,strListCount(list),sizeof(char*),strListCompareAlphaCase);
}

void strListSortAlphaCaseReverse(str_list_t list)
{
	qsort(list,strListCount(list),sizeof(char*),strListCompareAlphaCaseReverse);
}

void strListFreeStrings(str_list_t list)
{
	size_t i;

	if(list!=NULL) {
		for(i=0;list[i]!=NULL;i++)
			free(list[i]);
		list[0]=NULL;	/* terminate */
	}
}

void strListFree(str_list_t* list)
{
	if(*list!=NULL) {
		strListFreeStrings(*list);
		free(*list);
	}
}
