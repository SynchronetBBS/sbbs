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

#include "scfgdefs.h"	// scfg_t
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT BOOL		getstats(scfg_t* cfg, char node, stats_t* stats);
DLLEXPORT ulong		getposts(scfg_t* cfg, uint subnum);
DLLEXPORT long		getfiles(scfg_t* cfg, uint dirnum);
DLLEXPORT BOOL		inc_sys_upload_stats(scfg_t*, ulong files, ulong bytes);
DLLEXPORT BOOL		inc_sys_download_stats(scfg_t*, ulong files, ulong bytes);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */