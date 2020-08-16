/* dat_file.c */

/* Functions to deal with comma (CSV) and tab-delimited files and lists */

/* $Id: dat_file.c,v 1.8 2018/07/24 01:13:09 rswindell Exp $ */

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

#include "dat_file.h"
#include "genwrap.h"	/* lastchar */
#include "filewrap.h"	/* chsize */
#include <stdlib.h>		/* malloc */
#include <string.h>		/* strdup */

/***********************************/
/* CSV (Comma Separated Value) API */
/***********************************/

static char* csvEncode(char* field)
{
	char* dst;
	char* src;
	char* buf;
	char* comma;
	char* quote;
	char  first;
	char  last;
	char* nl;
	BOOL  enclose;

	if((buf=malloc(strlen(field)*2))==NULL)
		return(NULL);

	nl=strchr(field,'\n');
	comma=strchr(field,',');
	quote=strchr(field,'"');
	first=field[0];
	last=*lastchar(field);

	enclose = (quote || comma || nl || first==' ' || last==' ');

	dst=buf;
	if(enclose)
		*(dst++)='"';
	src=field;
	while(*src) {
		if(*src=='"')
			*(dst++)='"';	/* escape quotes */
		*(dst++)=*src++;
	}
	if(enclose)
		*(dst++)='"';

	*dst=0;

	return(buf);
}

char* csvLineCreator(const str_list_t columns)
{
	char*	str=NULL;
	char*	p;
	char*	val;
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
		if(i) strcat(str,",");
		else  *str=0;
		if((val=csvEncode(columns[i]))==NULL)
			break;
		strcat(str,val);
		free(val);
	}

	return(str);
}

str_list_t csvLineParser(const char* line)
{
	char*		p;
	char*		buf;
	char*		tmp;
	size_t		count=0;
	str_list_t	list;

	if((list=strListInit())==NULL)
		return(NULL);

	if((buf=strdup(line))==NULL) {
		strListFree(&list);
		return(NULL);
	}

	truncsp(buf);

	for(p=strtok_r(buf,",",&tmp);p;p=strtok_r(NULL,",",&tmp))
		strListAppend(&list,p,count++);

	free(buf);

	return(list);
}

/*********************/
/* Tab-Delimited API */
/*********************/

char* tabLineCreator(const str_list_t columns)
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

str_list_t tabLineParser(const char* line)
{
	char*		p;
	char*		buf;
	char*		tmp;
	size_t		count=0;
	str_list_t	list;

	if((list=strListInit())==NULL)
		return(NULL);

	if((buf=strdup(line))==NULL) {
		strListFree(&list);
		return(NULL);
	}

	for(p=strtok_r(buf,"\t",&tmp);p;p=strtok_r(NULL,"\t",&tmp))
		strListAppend(&list,p,count++);

	free(buf);

	return(list);
}

/* Generic API */

str_list_t dataCreateList(const str_list_t records[], const str_list_t columns, dataLineCreator_t lineCreator)
{
	char*		p;
	str_list_t	list;
	size_t		i;
	size_t		li=0;

	if((list=strListInit())==NULL)
		return(NULL);

	if(columns!=NULL) {
		p=lineCreator(columns);
		strListAppend(&list,p,li++);
		free(p);
	}
		
	if(records!=NULL)
		for(i=0;records[i]!=NULL;i++) {
			p=lineCreator(records[i]);
			strListAppend(&list,p,li++);
			free(p);
		}

	return(list);
}

BOOL dataWriteFile(FILE* fp, const str_list_t records[], const str_list_t columns, const char* separator
				   ,dataLineCreator_t lineCreator)
{
	size_t		count,total;
	str_list_t	list;

	rewind(fp);

	if(chsize(fileno(fp),0)!=0)	/* truncate */
		return(FALSE);

	if((list=dataCreateList(records,columns,lineCreator))==NULL)
		return(FALSE);

	total = strListCount(list);
	count = strListWriteFile(fp,list,separator);
	strListFree(&list);

	return(count == total);
}

str_list_t* dataParseList(const str_list_t records, str_list_t* columns, dataLineParser_t lineParser)
{
	size_t		ri=0;
	size_t		li=0;
	str_list_t* list;

	if(records==NULL)
		return(NULL);

	if((list=(str_list_t*)malloc(sizeof(str_list_t)*(strListCount(records)+1)))==NULL)
		return(NULL);

	if(columns!=NULL) {
		if((*columns=lineParser(records[ri++]))==NULL) {
			free(list);
			return(NULL);
		}
	}

	while(records[ri]!=NULL)
		list[li++]=lineParser(records[ri++]);

	list[li]=NULL; /* terminate */

	return(list);
}

str_list_t*	dataReadFile(FILE* fp, str_list_t* columns, dataLineParser_t lineParser)
{
	str_list_t*	records;
	str_list_t	lines;
	size_t		i;

	rewind(fp);

	if((lines=strListReadFile(fp, NULL, 0))==NULL)
		return(NULL);

	/* truncate line-feed chars off end of strings */
	for(i=0; lines[i]!=NULL; i++)
		truncnl(lines[i]);

	records=dataParseList(lines,columns,lineParser);

	strListFree(&lines);

	return(records);
}
