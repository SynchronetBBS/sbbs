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

/* Legacy file base API: */
DLLEXPORT BOOL		getfileixb(scfg_t* cfg, file_t* f);
DLLEXPORT BOOL		putfileixb(scfg_t* cfg, file_t* f);
DLLEXPORT BOOL		getfiledat(scfg_t* cfg, file_t* f);
DLLEXPORT BOOL		putfiledat(scfg_t* cfg, file_t* f);
DLLEXPORT void		putextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext);
DLLEXPORT void		getextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext);
DLLEXPORT char*		getfilepath(scfg_t* cfg, file_t* f, char* path);
DLLEXPORT int		openextdesc(scfg_t* cfg, uint dirnum);
DLLEXPORT void		fgetextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext, int file);
DLLEXPORT void		closeextdesc(int file);
DLLEXPORT BOOL		removefiledat(scfg_t* cfg, file_t* f);
DLLEXPORT BOOL		addfiledat(scfg_t* cfg, file_t* f);
DLLEXPORT char *	padfname(const char *filename, char *str);
DLLEXPORT char *	unpadfname(const char *filename, char *str);
DLLEXPORT BOOL		rmuserxfers(scfg_t* cfg, int fromuser, int destuser, char *fname);
DLLEXPORT int		update_uldate(scfg_t* cfg, file_t* f);

DLLEXPORT BOOL		findfile(scfg_t* cfg, uint dirnum, const char *filename);

/* New file base API: */
DLLEXPORT BOOL		 newfiles(smb_t*, time_t);
DLLEXPORT smbfile_t* loadfiles(smb_t*, const char* filespec, time_t, size_t* count);
DLLEXPORT void		 sortfiles(smbfile_t*, size_t count, enum file_sort);
DLLEXPORT void		 freefiles(smbfile_t*, size_t count);
DLLEXPORT BOOL		 loadfile(scfg_t*, uint dirnum, const char* filename, smbfile_t*);
DLLEXPORT BOOL		 updatefile(scfg_t*, smbfile_t*);
DLLEXPORT char*		 getfullfilepath(scfg_t*, smbfile_t*, char* path);
DLLEXPORT off_t		 getfilesize(scfg_t*, smbfile_t*);
DLLEXPORT time_t	 getfiledate(scfg_t*, smbfile_t*);
DLLEXPORT ulong		 gettimetodl(scfg_t*, smbfile_t*, uint rate_cps);
DLLEXPORT BOOL		 addfile(scfg_t*, uint dirnum, smbfile_t*, const char* extdesc);
DLLEXPORT BOOL		 removefile(scfg_t*, uint dirnum, const char* filename);
DLLEXPORT char*		 format_filename(const char* fname, char* buf, size_t, BOOL pad);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
