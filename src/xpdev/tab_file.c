/* tab_file.c */

/* Functions to deal with tab-delimited files and lists */

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

#include "tab_file.h"
#include <stdlib.h>		/* malloc */

char* tabLine(str_list_t columns)
{
	char*	str=NULL;
	char*	p;
	size_t	i,len;

	if(columns==NULL)
		return(NULL);

	for(i=0;columns[i]!=NULL;i++) {
		len=strlen(columns[i])*2;
		if(str)
			len+=strlen(str);
		if((p=realloc(str,len))==NULL)
			break;
		str=p;
		if(i) strcat(str,"\t");
		else  *str=0;
		strcat(str,columns[i]);
	}

	return(str);
}

str_list_t tabCreateList(str_list_t records[], str_list_t columns)
{
	char*		p;
	str_list_t	list;
	size_t		i;
	size_t		li=0;

	if((list=strListInit())==NULL)
		return(NULL);

	if(columns!=NULL) {
		p=tabLine(columns);
		strListAppend(&list,p,li++);
		free(p);
	}
		
	if(records!=NULL)
		for(i=0;records[i]!=NULL;i++) {
			p=tabLine(records[i]);
			strListAppend(&list,p,li++);
			free(p);
		}

	return(list);
}

str_list_t tabParseLine(char* line)
{
	char*		p;
	char*		buf;
	size_t		count=0;
	str_list_t	list;

	if((list=strListInit())==NULL)
		return(NULL);

	if((buf=strdup(line))==NULL) {
		strListFree(&list);
		return(NULL);
	}

	for(p=strtok(buf,"\t");p;p=strtok(NULL,"\t"))
		strListAppend(&list,p,count++);

	free(buf);

	return(list);
}

str_list_t* tabParseList(str_list_t records, str_list_t* columns)
{
	size_t		i=0;
	str_list_t* list;

	if(records==NULL)
		return(NULL);

	if((list=(str_list_t*)malloc(sizeof(str_list_t*)*(strListCount(records)+1)))==NULL)
		return(NULL);

	if(columns!=NULL) {
		if((*columns=tabParseLine(records[i++]))==NULL)
			return(NULL);
	}

	while(records[i]!=NULL) {
		list[i]=tabParseLine(records[i]);
		i++;
	}

	list[i]=NULL; /* terminate */

	return(list);
}

#include "truncsp.c"

str_list_t*	tabReadFile(FILE* fp, str_list_t* columns)
{
	str_list_t*	records;
	str_list_t	lines;
	size_t		i;

	if((lines=strListReadFile(fp, NULL, 0))==NULL)
		return(NULL);

	/* truncate new-line chars off end of strings */
	for(i=0; lines[i]!=NULL; i++)
		truncnl(lines[i]);

	records=tabParseList(lines,columns);

	strListFree(&lines);

	return(records);
}

