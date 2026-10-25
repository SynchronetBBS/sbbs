/* Synchronet QWK packet jobs (pack/unpack on behalf of any server or script) */

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

#ifndef QWKJOB_H_
#define QWKJOB_H_

#include <stddef.h>         /* size_t */
#include "gen_defs.h"       /* uint */
#include "scfgdefs.h"       /* scfg_t */
#include "startup.h"        /* bbs_startup_t */
#include "nopen.h"          /* fmutex_t */
#include "dllexport.h"

/* Maximum on-demand jobs running at once in one process */
#define QWKJOB_MAX_CONCURRENT   4

struct mqtt;

typedef struct qwkjob_host {
	const char* name;               /* log prefix, e.g. "ftp", "web", "srvc" */
	const char* host_name;          /* for fmutex ownership text */
	int (*lputs)(void* cbdata, int level, const char* str);
	void* cbdata;
	struct mqtt* mqtt;              /* may be NULL */
	const char* temp_root;          /* caller's scfg.temp_dir (with trailing slash) */
	volatile bool* terminated;      /* may be NULL; polled only between job steps */
	char** text;                    /* the calling server's text[] array */
	const bbs_startup_t* startup;   /* Terminal Server hosts only, else NULL */
	bool is_event_thread;           /* true only for the Terminal Server's event thread */
	const char* event_code;         /* event-thread log prefix ("packQWK", ...), else NULL */
} qwkjob_host_t;

enum qwkjob_result {
	QWKJOB_OK = 0,          /* packet created / packet imported */
	QWKJOB_EMPTY,           /* pack: nothing new, no packet */
	QWKJOB_BUSY,            /* user/hub lock held by another job */
	QWKJOB_THROTTLED,       /* concurrency cap reached in this process */
	QWKJOB_USER_ONLINE,     /* user pack refused: user is on a terminal node */
	QWKJOB_BAD_USER,        /* no such user, deleted or inactive */
	QWKJOB_BAD_HUB,         /* no such hub ID, or hub disabled */
	QWKJOB_FAILED           /* error; details logged via host->lputs */
};

#ifdef __cplusplus
extern "C" {
#endif

/* User direction */
DLLEXPORT enum qwkjob_result qwkjob_pack(scfg_t*, const qwkjob_host_t*
                                         , uint usernum, uint* msgcnt
                                         , char* outpath, size_t outpath_size);
DLLEXPORT enum qwkjob_result qwkjob_unpack_rep(scfg_t*, const qwkjob_host_t*
                                               , uint usernum, const char* reppath);

DLLEXPORT const char* qwkjob_result_str(enum qwkjob_result);

#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
