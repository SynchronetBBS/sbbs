/* semfile.c */

/* $Id: semfile.c,v 1.8 2019/07/24 04:11:23 rswindell Exp $ */

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

#include <string.h>

#include "semfile.h"
#include "filewrap.h"
#include "dirwrap.h"
#include "genwrap.h"

#if !defined(NO_SOCKET_SUPPORT)
	#include "sockwrap.h"
#endif

/****************************************************************************/
/* This function compares a single semaphore file's							*/
/* date/time stamp (if the file exists) against the passed time stamp (t)	*/
/* updating the time stamp to the latest dated semaphore file and returning	*/
/* TRUE if any where newer than the initial value.							*/
/****************************************************************************/
BOOL DLLCALL semfile_check(time_t* t, const char* fname)
{
	time_t	ft;

	if(*t==0)	/* uninitialized */
		*t=time(NULL);

	if((ft=fdate(fname))==-1 || ft<=*t)
		return(FALSE);

	*t=ft;
	return(TRUE);
}

/****************************************************************************/
/* This function goes through a list of semaphore files, comparing the file	*/
/* date/time stamp (if the file exists) against the passed time stamp (t)	*/
/* updating the time stamp to the latest dated semaphore file and returning	*/
/* a pointer to the filename if any where newer than the initial timestamp.	*/
/****************************************************************************/
char* DLLCALL semfile_list_check(time_t* t, str_list_t filelist)
{
	char*	signaled=NULL;
	size_t		i;

	for(i=0;filelist[i]!=NULL;i++)
		if(semfile_check(t, filelist[i]))
			signaled = filelist[i];

	return(signaled);
}

str_list_t DLLCALL semfile_list_init(const char* parent, 
							   const char* action, const char* service)
{
	char	path[MAX_PATH+1];
	char	hostname[128];
	char*	p;
	str_list_t	list;

	if((list=strListInit())==NULL)
		return(NULL);
	SAFEPRINTF2(path,"%s%s",parent,action);
	strListPush(&list,path);
	SAFEPRINTF3(path,"%s%s.%s",parent,action,service);
	strListPush(&list,path);
#if !defined(NO_SOCKET_SUPPORT)
	if(gethostname(hostname,sizeof(hostname))==0) {
		SAFEPRINTF3(path,"%s%s.%s",parent,action,hostname);
		strListPush(&list,path);
		SAFEPRINTF4(path,"%s%s.%s.%s",parent,action,hostname,service);
		strListPush(&list,path);
		if((p=strchr(hostname,'.'))!=NULL) {
			*p=0;
			SAFEPRINTF3(path,"%s%s.%s",parent,action,hostname);
			strListPush(&list,path);
			SAFEPRINTF4(path,"%s%s.%s.%s",parent,action,hostname,service);
			strListPush(&list,path);
		}
	}
#endif
	return(list);
}

void DLLCALL semfile_list_add(str_list_t* filelist, const char* path)
{
	strListPush(filelist, path);
}

void DLLCALL semfile_list_free(str_list_t* filelist)
{
	strListFree(filelist);
}

BOOL DLLCALL semfile_signal(const char* fname, const char* text)
{
	int file;
	struct utimbuf ut;
#if !defined(NO_SOCKET_SUPPORT)
	char hostname[128];

	if(text==NULL && gethostname(hostname,sizeof(hostname))==0)
		text=hostname;
#endif
	if((file=open(fname,O_CREAT|O_WRONLY, DEFFILEMODE))<0)	/* use sopen instead? */
		return(FALSE);
	if(text!=NULL)
		write(file,text,strlen(text));
	close(file);

	/* update the time stamp */
	ut.actime = ut.modtime = time(NULL);
	return utime(fname, &ut)==0;
}
