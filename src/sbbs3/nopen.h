/* "Network-file Open" (nopen) and friends */

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

#ifndef NOPEN_H_
#define NOPEN_H_

#include <stdio.h>          /* FILE */
#include <fcntl.h>          /* O_RDONLY */
#include "gen_defs.h"       /* bool */
#include "dirwrap.h"        /* MAX_PATH */
#include "str_list.h"       /* str_list_t */

#define FNOPEN_BUF_SIZE     (64 * 1024)
#define FCOPY_BUF_SIZE      (256 * 1024)
#define LOOP_NOPEN    100   /* Retries before file access denied			*/
#define LOCK_RETRY_DELAY 100 // Milliseconds between retries on locked resource
#define FMUTEX_UNLINK_RETRIES 10 // Retries of a mutex file removal another host is blocking
#define WRITTEN_BY_PREFIX "; Written by "
/* printf format and arguments of a written_by() writer naming a program and
   the Synchronet build: the caller includes sbbsdefs.h and the git_*.h headers */
#define WRITTEN_BY_BUILD_FMT "%s (Synchronet v%s%c %s/%s)"
#define WRITTEN_BY_BUILD_ARGS(program) (program), VERSION, REVISION, GIT_BRANCH, GIT_HASH

typedef struct {
	int fd;
	time_t time;
	char name[MAX_PATH + 1];
} fmutex_t;

#ifdef __cplusplus
extern "C" {
#endif

int     nopen(const char* str, uint access);
FILE *  fnopen(int* file, const char* str, uint access);
bool    ftouch(const char* fname);
bool    fmutex(const char* fname, const char* text, long max_age, time_t*);
fmutex_t fmutex_init(void);
bool    fmutex_open(fmutex_t*, const char* text, long max_age);
bool    fmutex_close(fmutex_t*);
bool    fcompare(const char* fn1, const char* fn2);
bool    nfcopy(const char* src, const char* dest);
bool    backup(const char* org, int backup_level, bool ren);
char*   written_by(char* buf, size_t size, const char* writer);
void    ini_set_written_by(str_list_t* ini, const char* writer);

#ifdef __cplusplus
}
#endif

#endif  /* Don't add anything after this line */
