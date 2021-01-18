/* Synchronet Configuration Load/Preparation functions */

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

#ifndef _LOAD_CFG_H_
#define _LOAD_CFG_H_

#include "scfgdefs.h"	// scfg_t
#include "smblib.h"
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT BOOL		load_cfg(scfg_t* cfg, char* text[], BOOL prep, char* error, size_t);
DLLEXPORT void		free_cfg(scfg_t* cfg);
DLLEXPORT void		free_text(char* text[]);
DLLEXPORT ushort	sys_timezone(scfg_t* cfg);
DLLEXPORT char *	prep_dir(const char* base, char* dir, size_t buflen);
DLLEXPORT char *	prep_code(char *str, const char* prefix);
DLLEXPORT int 		md(const char *path);
DLLEXPORT int		smb_storage_mode(scfg_t*, smb_t*);
DLLEXPORT int		smb_open_sub(scfg_t*, smb_t*, unsigned int subnum);
DLLEXPORT BOOL		smb_init_dir(scfg_t*, smb_t*, unsigned int dirnum);
DLLEXPORT int		smb_open_dir(scfg_t*, smb_t*, unsigned int dirnum);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
