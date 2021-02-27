/* Synchronet Filebase Access functions */

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

#ifndef _FILEDAT_H_
#define _FILEDAT_H_

#include "scfgdefs.h"	// scfg_t
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT BOOL		getfileixb(scfg_t* cfg, file_t* f);
DLLEXPORT BOOL		putfileixb(scfg_t* cfg, file_t* f);
DLLEXPORT BOOL		getfiledat(scfg_t* cfg, file_t* f);
DLLEXPORT BOOL		putfiledat(scfg_t* cfg, file_t* f);
DLLEXPORT void		putextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext);
DLLEXPORT void		getextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext);
DLLEXPORT char*		getfilepath(scfg_t* cfg, file_t* f, char* path);

DLLEXPORT BOOL		removefiledat(scfg_t* cfg, file_t* f);
DLLEXPORT BOOL		addfiledat(scfg_t* cfg, file_t* f);
DLLEXPORT BOOL		findfile(scfg_t* cfg, uint dirnum, char *filename);
DLLEXPORT char *	padfname(const char *filename, char *str);
DLLEXPORT char *	unpadfname(const char *filename, char *str);
DLLEXPORT BOOL		rmuserxfers(scfg_t* cfg, int fromuser, int destuser, char *fname);

DLLEXPORT int		update_uldate(scfg_t* cfg, file_t* f);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */