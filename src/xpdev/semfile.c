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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
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
/* true if any where newer than the initial value.							*/
/* If the 't' parameter is NULL, only file existence is checked (not time).	*/
/****************************************************************************/
bool semfile_check(time_t* t, const char* fname)
{
	time_t ft;

	if (t == NULL)
		return fexist(fname);

	if (*t == 0)   /* uninitialized */
		*t = time(NULL);

	if ((ft = fdate(fname)) == -1 || ft <= *t)
		return false;

	*t = ft;
	return true;
}

/****************************************************************************/
/* This function goes through a list of semaphore files, comparing the file	*/
/* date/time stamp (if the file exists) against the passed time stamp (t)	*/
/* updating the time stamp to the latest dated semaphore file and returning	*/
/* a pointer to the filename if any where newer than the initial timestamp.	*/
/****************************************************************************/
char* semfile_list_check(time_t* t, str_list_t filelist)
{
	char*  signaled = NULL;
	size_t i;

	for (i = 0; filelist[i] != NULL; i++)
		if (semfile_check(t, filelist[i]))
			signaled = filelist[i];

	return signaled;
}

str_list_t semfile_list_init(const char* parent,
                             const char* action, const char* service)
{
	char       path[MAX_PATH + 1];
	char       hostname[128];
	char*      p;
	str_list_t list;

	if ((list = strListInit()) == NULL)
		return NULL;
	SAFEPRINTF2(path, "%s%s", parent, action);
	strListPush(&list, path);
	SAFEPRINTF3(path, "%s%s.%s", parent, action, service);
	strListPush(&list, path);
#if !defined(NO_SOCKET_SUPPORT)
	if (gethostname(hostname, sizeof(hostname)) == 0) {
		SAFEPRINTF3(path, "%s%s.%s", parent, action, hostname);
		strListPush(&list, path);
		SAFEPRINTF4(path, "%s%s.%s.%s", parent, action, hostname, service);
		strListPush(&list, path);
		if ((p = strchr(hostname, '.')) != NULL) {
			*p = 0;
			SAFEPRINTF3(path, "%s%s.%s", parent, action, hostname);
			strListPush(&list, path);
			SAFEPRINTF4(path, "%s%s.%s.%s", parent, action, hostname, service);
			strListPush(&list, path);
		}
	}
#endif
	return list;
}

void semfile_list_add(str_list_t* filelist, const char* path)
{
	strListPush(filelist, path);
}

void semfile_list_free(str_list_t* filelist)
{
	strListFree(filelist);
}

bool semfile_signal(const char* fname, const char* text)
{
	int     file;
	size_t  textlen = 0;
	ssize_t wrlen = 0;
#if !defined(NO_SOCKET_SUPPORT)
	char    hostname[128];

	if (text == NULL && gethostname(hostname, sizeof(hostname)) == 0)
		text = hostname;
#endif
	if ((file = open(fname, O_CREAT | O_WRONLY, DEFFILEMODE)) < 0)  /* use sopen instead? */
		return false;
	if (text != NULL)
		wrlen = write(file, text, textlen = strlen(text));
	close(file);

	/* update the time stamp */
	return utime(fname, NULL) == 0 && wrlen == (ssize_t)textlen;
}
