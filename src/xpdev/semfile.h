/* Semaphore file functions */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _SEMFILE_H
#define _SEMFILE_H

#include <time.h>		/* time_t */
#include "str_list.h"	/* string list functions and types */
#include "wrapdll.h"	/* DLLEXPORT */

#if defined(__cplusplus)
extern "C" {
#endif

/* semfile.c */
DLLEXPORT BOOL		semfile_signal(const char* fname, const char* text);
DLLEXPORT BOOL		semfile_check(time_t* t, const char* fname);
DLLEXPORT char*		semfile_list_check(time_t* t, str_list_t filelist);
DLLEXPORT str_list_t	
					semfile_list_init(const char* parent, const char* action
												,const char* service);
DLLEXPORT void		semfile_list_add(str_list_t* filelist, const char* fname);
DLLEXPORT void		semfile_list_free(str_list_t* filelist);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
