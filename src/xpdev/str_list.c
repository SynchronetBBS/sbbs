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

#include <stdlib.h>		/* malloc */
#include "str_list.h"

size_t strListCount(char** list)
{
	size_t i;

	if(list==NULL)
		return(0);

	for(i=0;list[i]!=NULL;i++)
		;

	return(i);
}

char** strListAdd(char*** list, char* str, size_t count)
{
	char**	lp;

	if(!count)
		count=strListCount(*list);

	if((lp=realloc(*list,sizeof(char*)*(count+2)))==NULL)
		return(NULL);

	*list=lp;
	if((lp[count]=malloc(strlen(str)+1))==NULL)
		return(NULL);

	strcpy(lp[count++],str);
	lp[count]=NULL;	/* terminate list */

	return(lp);
}

void strListFree(char*** list)
{
	size_t i;

	if(*list!=NULL) {

		for(i=0;(*list)[i]!=NULL;i++)
			free((*list)[i]);

		free(*list);
	}
}
