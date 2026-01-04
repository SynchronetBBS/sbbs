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

#ifndef _GETSTATS_H_
#define _GETSTATS_H_

#include <stdio.h>      // FILE
#include "scfgdefs.h"   // scfg_t
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT char*     dstats_fname(scfg_t*, uint node, char* path, size_t);
DLLEXPORT char*     cstats_fname(scfg_t*, uint node, char* path, size_t);
DLLEXPORT FILE*     fopen_dstats(scfg_t*, uint node, bool for_write);
DLLEXPORT FILE*     fopen_cstats(scfg_t*, uint node, bool for_write);
DLLEXPORT bool      fclose_cstats(FILE*);
DLLEXPORT bool      fclose_dstats(FILE*);
DLLEXPORT bool      fread_dstats(FILE*, stats_t*);
DLLEXPORT bool      fwrite_dstats(FILE*, const stats_t*, const char* function);
DLLEXPORT bool      fwrite_cstats(FILE*, const stats_t*);
DLLEXPORT void      parse_cstats(str_list_t, stats_t*);
DLLEXPORT bool      getstats(scfg_t*, uint node, stats_t*);
DLLEXPORT bool      getstats_cached(scfg_t*, uint node, stats_t*, int duration);
DLLEXPORT bool      putstats(scfg_t*, uint node, const stats_t*);
DLLEXPORT uint      getposts(scfg_t*, int subnum);
DLLEXPORT uint      getnewposts(scfg_t*, int subnum, uint32_t ptr);
DLLEXPORT uint      getfiles(scfg_t*, int dirnum);
DLLEXPORT uint      getnewfiles(scfg_t*, int dirnum, time_t ptr);
DLLEXPORT void      rolloverstats(stats_t*);
DLLEXPORT bool      inc_post_stats(scfg_t*, uint count);
DLLEXPORT bool      inc_email_stats(scfg_t*, uint count, bool feedback);
DLLEXPORT bool      inc_upload_stats(scfg_t*, uint files, uint64_t bytes);
DLLEXPORT bool      inc_download_stats(scfg_t*, uint files, uint64_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
