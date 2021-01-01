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

DLLEXPORT BOOL			findfile(scfg_t* cfg, uint dirnum, const char *filename);
DLLEXPORT BOOL			newfiles(smb_t*, time_t);
DLLEXPORT BOOL			loadfile(scfg_t*, uint dirnum, const char* filename, smbfile_t*);
DLLEXPORT smbfile_t*	loadfiles(scfg_t*, smb_t*, const char* filespec, time_t, size_t* count);
DLLEXPORT void			sortfiles(smbfile_t*, size_t count, enum file_sort);
DLLEXPORT void			freefiles(smbfile_t*, size_t count);
DLLEXPORT BOOL			updatefile(scfg_t*, smbfile_t*);
DLLEXPORT char*			getfilepath(scfg_t*, smbfile_t*, char* path);
DLLEXPORT off_t			getfilesize(scfg_t*, smbfile_t*);
DLLEXPORT time_t		getfiletime(scfg_t*, smbfile_t*);
DLLEXPORT ulong			gettimetodl(scfg_t*, smbfile_t*, uint rate_cps);
DLLEXPORT BOOL			addfile(scfg_t*, uint dirnum, smbfile_t*, const char* extdesc);
DLLEXPORT BOOL			removefile(scfg_t*, uint dirnum, const char* filename);
DLLEXPORT char*			format_filename(const char* fname, char* buf, size_t, BOOL pad);

/* Batch file transfer queues */
DLLEXPORT char*			batch_list_name(scfg_t* , uint usernumber, enum XFER_TYPE, char* fname, size_t);
DLLEXPORT FILE*			batch_list_open(scfg_t* , uint usernumber, enum XFER_TYPE, BOOL create);
DLLEXPORT str_list_t	batch_list_read(scfg_t* , uint usernumber, enum XFER_TYPE);
DLLEXPORT BOOL			batch_list_write(scfg_t*, uint usernumber, enum XFER_TYPE, str_list_t list);
DLLEXPORT BOOL			batch_list_clear(scfg_t*, uint usernumber, enum XFER_TYPE);

DLLEXPORT BOOL			batch_file_add(scfg_t*, uint usernumber, enum XFER_TYPE, smbfile_t*);
DLLEXPORT BOOL			batch_file_exists(scfg_t*, uint usernumber, enum XFER_TYPE, const char* filename);
DLLEXPORT BOOL			batch_file_remove(scfg_t*, uint usernumber, enum XFER_TYPE, const char* filename);
DLLEXPORT size_t		batch_file_count(scfg_t*, uint usernumber, enum XFER_TYPE);
DLLEXPORT BOOL			batch_file_get(scfg_t*, str_list_t, const char* filename, smbfile_t*);
DLLEXPORT int			batch_file_dir(scfg_t*, str_list_t, const char* filename);
DLLEXPORT BOOL			batch_file_load(scfg_t*, str_list_t, const char* filename, smbfile_t*);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
