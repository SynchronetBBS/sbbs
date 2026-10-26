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

/* Seconds before a user or hub lock file is presumed left by a crashed holder and taken over */
#define QWKJOB_LOCK_MAX_AGE     (60 * 60)
/* Seconds a lock file may be held before a refused job is logged at LOG_NOTICE (else LOG_DEBUG) */
#define QWKJOB_LOCK_NOTICE_AGE  (5 * 60)

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
/* Hub direction */
DLLEXPORT enum qwkjob_result qwkjob_pack_rep(scfg_t*, const qwkjob_host_t*
                                             , uint hubnum, uint* msgcnt
                                             , char* outpath, size_t outpath_size);
DLLEXPORT enum qwkjob_result qwkjob_unpack_qwk(scfg_t*, const qwkjob_host_t*
                                               , uint hubnum, const char* qwkpath);
/* Event thread only: hold the hub lock across pack + call-out */
DLLEXPORT bool qwkjob_lock_hub(scfg_t*, const qwkjob_host_t*, uint hubnum, fmutex_t*);
DLLEXPORT void qwkjob_unlock_hub(const qwkjob_host_t*, fmutex_t*);
DLLEXPORT enum qwkjob_result qwkjob_pack_rep_locked(scfg_t*, const qwkjob_host_t*
                                                    , uint hubnum, uint* msgcnt
                                                    , char* outpath, size_t outpath_size);

DLLEXPORT const char* qwkjob_result_str(enum qwkjob_result);
DLLEXPORT int qwkjob_find_hub(scfg_t*, const char* hub_id);  /* -1 if none */

/* Path of the fmutex lock file that serializes QWK jobs for one user */
DLLEXPORT char* qwkjob_user_lock_name(const scfg_t*, uint usernum, char* buf, size_t size);

/* After a failed fmutex_open(): describes the held lock (holder, age) in buf, returns the log level */
DLLEXPORT int qwkjob_lock_busy(const fmutex_t*, char* buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
