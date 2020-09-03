/* msdirent.c */

/* POSIX directory operations using Microsoft _findfirst/next functions. */

/* $Id: msdirent.c,v 1.3 2018/07/24 01:11:07 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
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

#include "msdirent.h"
#include <errno.h>		/* ENOMEM & ENOENT definitions */
#include <stdio.h>		/* sprintf prototype */

DIR* opendir(const char* dirname)
{
	DIR*	dir;

	if((dir=(DIR*)calloc(1,sizeof(DIR)))==NULL) {
		errno=ENOMEM;
		return(NULL);
	}
	sprintf(dir->filespec,"%.*s",sizeof(dir->filespec)-5,dirname);
	if(*dir->filespec && dir->filespec[strlen(dir->filespec)-1]!='\\')
		strcat(dir->filespec,"\\");
	strcat(dir->filespec,"*.*");
	dir->handle=_findfirst(dir->filespec,&dir->finddata);
	if(dir->handle==-1) {
		errno=ENOENT;
		free(dir);
		return(NULL);
	}
	return(dir);
}
struct dirent* readdir(DIR* dir)
{
	if(dir==NULL)
		return(NULL);
	if(dir->end==TRUE)
		return(NULL);
	if(dir->handle==-1)
		return(NULL);
	sprintf(dir->dirent.d_name,"%.*s",sizeof(struct dirent)-1,dir->finddata.name);
	if(_findnext(dir->handle,&dir->finddata)!=0)
		dir->end=TRUE;
	return(&dir->dirent);
}
int closedir (DIR* dir)
{
	if(dir==NULL)
		return(-1);
	_findclose(dir->handle);
	free(dir);
	return(0);
}
void rewinddir(DIR* dir)
{
	if(dir==NULL)
		return;
	_findclose(dir->handle);
	dir->end=FALSE;
	dir->handle=_findfirst(dir->filespec,&dir->finddata);
}
