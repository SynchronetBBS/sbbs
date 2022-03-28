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

#include <stdio.h>		// FILE
#include "scfgdefs.h"	// scfg_t
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT char*		dstats_fname(scfg_t*, uint node, char* path, size_t);
DLLEXPORT char*		cstats_fname(scfg_t*, uint node, char* path, size_t);
DLLEXPORT FILE*		fopen_dstats(scfg_t*, uint node, BOOL for_write);
DLLEXPORT FILE*		fopen_cstats(scfg_t*, uint node, BOOL for_write);
DLLEXPORT BOOL		fclose_cstats(FILE*);
DLLEXPORT BOOL		fclose_dstats(FILE*);
DLLEXPORT BOOL		fread_dstats(FILE*, stats_t*);
DLLEXPORT BOOL		fwrite_dstats(FILE*, const stats_t*);
DLLEXPORT BOOL		fwrite_cstats(FILE*, const stats_t*);
DLLEXPORT BOOL		getstats(scfg_t*, uint node, stats_t*);
DLLEXPORT BOOL		putstats(scfg_t*, uint node, const stats_t*);
DLLEXPORT ulong		getposts(scfg_t*, uint subnum);
DLLEXPORT long		getfiles(scfg_t*, uint dirnum);
DLLEXPORT void		rolloverstats(stats_t*);
DLLEXPORT BOOL		inc_post_stats(scfg_t*, uint count);
DLLEXPORT BOOL		inc_email_stats(scfg_t*, uint count, BOOL feedback);
DLLEXPORT BOOL		inc_upload_stats(scfg_t*, ulong files, uint64_t bytes);
DLLEXPORT BOOL		inc_download_stats(scfg_t*, ulong files, uint64_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
