/* semfile.h */

/* Semaphore file functions */

/* $Id: semfile.h,v 1.2 2018/07/24 01:13:09 rswindell Exp $ */

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

#ifndef _SEMFILE_H
#define _SEMFILE_H

#include <time.h>		/* time_t */
#include "str_list.h"	/* string list functions and types */
#include "wrapdll.h"	/* DLLEXPORT and DLLCALL */

#if defined(__cplusplus)
extern "C" {
#endif

/* semfile.c */
DLLEXPORT BOOL		DLLCALL semfile_signal(const char* fname, const char* text);
DLLEXPORT BOOL		DLLCALL semfile_check(time_t* t, const char* fname);
DLLEXPORT char*		DLLCALL semfile_list_check(time_t* t, str_list_t filelist);
DLLEXPORT str_list_t	
					DLLCALL semfile_list_init(const char* parent, const char* action
												,const char* service);
DLLEXPORT void		DLLCALL semfile_list_add(str_list_t* filelist, const char* fname);
DLLEXPORT void		DLLCALL semfile_list_free(str_list_t* filelist);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
