/* csv_file.c */

/* Functions to deal with comma-separated value (CSV) files and lists */

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

#include "csv_file.h"
#include "genwrap.h"	/* lastchar */
#include <stdlib.h>		/* malloc */

str_list_t csvParseLine(char* line)
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

	for(p=strtok(buf,",");p;p=strtok(NULL,","))
		strListAppend(&list,p,count++);

	free(buf);

	return(list);
}

named_string_t* csvParseRecord(char* record, str_list_t columns)
{
	size_t	len;
	size_t	i;
	char*	buf;
	char*	p;
	named_string_t*	str;

	len = sizeof(named_string_t)*(strListCount(columns)+1);

	if((str=(named_string_t*)malloc(len))==NULL)
		return(NULL);
	
	memset(str,0,len);

	if((buf=strdup(record))==NULL) {
		free(str);
		return(NULL);
	}

	p=strtok(buf,",");
	for(i=0;columns[i];i++) {
		str[i].name=columns[i];
		if(p!=NULL) {
			if((str[i].value=strdup(p))==NULL)
				break;
			p=strtok(NULL,",");
		}
	}
	free(buf);

	return(str);
}

named_string_t** csvParse(str_list_t records, str_list_t columns)
{
	size_t	i;
	named_string_t** list;

	if(records==NULL)
		return(NULL);

	if((list=(named_string_t**)malloc(sizeof(named_string_t*)*(strListCount(records)+1)))==NULL)
		return(NULL);

	for(i=0;records[i];i++)
		list[i]=csvParseRecord(records[i],columns);

	list[i]=NULL; /* terminate */

	return(list);
}

char* csvEncode(char* field)
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

char* csvLine(str_list_t columns)
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

str_list_t csvCreate(str_list_t data[], str_list_t columns)
{
	char*		p;
	str_list_t	list;
	size_t		i;
	size_t		li=0;

	if((list=strListInit())==NULL)
		return(NULL);

	if(columns!=NULL) {
		p=csvLine(columns);
		strListAppend(&list,p,li++);
		free(p);
	}
		
	if(data!=NULL)
		for(i=0;data[i]!=NULL;i++) {
			p=csvLine(data[i]);
			strListAppend(&list,p,li++);
			free(p);
		}

	return(list);
}

#if 0

int main()
{
	char* columns[] =	{"name", "rank", "serial number", NULL};
	str_list_t	data[3];
	str_list_t	list;
	size_t		i;

	data[0]=strListInit();
	strListPush(&data[0],"rob \"the stud\"");
	strListPush(&data[0],"general");
	strListPush(&data[0],"549-71-1344");

	data[1]=strListInit();
	strListPush(&data[1],"mark");
	strListPush(&data[1]," colonel");
	strListPush(&data[1],"x,xx");

	data[2]=NULL;

	list=csvCreate(data, columns);

	for(i=0;list[i];i++)
		printf("%s\n",list[i]);
}

#elif 1	/* decode */

void main(int argc, char** argv)
{
	named_string_t**	str;
	str_list_t	list;
	str_list_t	columns;
	FILE*		fp;
	size_t		i,j;


	if(argc<2) {
		printf("usage: csv_file <file.csv>\n");
		exit(0);
	}

	if((fp=fopen(argv[1],"r"))==NULL) {
		printf("Error opening %s\n",argv[1]);
		exit(0);
	}

	if((list=strListReadFile(fp, NULL, 0))==NULL) {
		printf("Error reading %s\n",argv[1]);
		exit(0);
	}

	if((columns=csvParseLine(strListRemove(&list,0)))==NULL) {
		printf("Error parsing columns header\n");
		exit(0);
	}

	if((str=csvParse(list,columns))==NULL) {
		printf("csvParse error\n");
		exit(1);
	}

	for(i=0;str[i];i++)
		for(j=0;str[i][j];j++)
			printf("%s[%d]=%s",str[i][j].name,i,str[i][j].value);
}

#endif