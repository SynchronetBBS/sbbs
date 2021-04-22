/* Synchronet FileBase Access functions */

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
#include "smblib.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT bool			newfiles(smb_t*, time_t);
DLLEXPORT time_t		newfiletime(smb_t*);
DLLEXPORT bool			update_newfiletime(smb_t*, time_t);
DLLEXPORT time_t		dir_newfiletime(scfg_t*, uint dirnum);
DLLEXPORT time_t		lastfiletime(smb_t*); // Reads the last index record

DLLEXPORT bool			findfile(scfg_t* cfg, uint dirnum, const char *filename, file_t*);
DLLEXPORT bool			loadfile(scfg_t*, uint dirnum, const char* filename, file_t*, enum file_detail);
DLLEXPORT file_t*		loadfiles(smb_t*, const char* filespec, time_t, enum file_detail, enum file_sort, size_t* count);
DLLEXPORT void			sortfiles(file_t*, size_t count, enum file_sort);
DLLEXPORT void			freefiles(file_t*, size_t count);
DLLEXPORT str_list_t	loadfilenames(smb_t*, const char* filespec, time_t t, enum file_sort, size_t* count);
DLLEXPORT void			sortfilenames(str_list_t, size_t count, enum file_sort);
DLLEXPORT bool			updatefile(scfg_t*, file_t*);
DLLEXPORT char*			getfilepath(scfg_t*, file_t*, char* path);
DLLEXPORT off_t			getfilesize(scfg_t*, file_t*);
DLLEXPORT time_t		getfiletime(scfg_t*, file_t*);
DLLEXPORT ulong			gettimetodl(scfg_t*, file_t*, uint rate_cps);
DLLEXPORT ulong			getuserxfers(scfg_t*, const char* from, uint to);
DLLEXPORT bool			hashfile(scfg_t*, file_t*);
DLLEXPORT bool			addfile(scfg_t*, uint dirnum, file_t*, const char* extdesc);
DLLEXPORT bool			removefile(scfg_t*, uint dirnum, const char* filename);
DLLEXPORT char*			format_filename(const char* fname, char* buf, size_t, bool pad);
DLLEXPORT bool			extract_diz(scfg_t*, file_t*, str_list_t diz_fname, char* path, size_t);
DLLEXPORT str_list_t	read_diz(const char* path);
DLLEXPORT char*			format_diz(str_list_t lines, char*, size_t maxlen, bool allow_ansi);
DLLEXPORT char*			prep_file_desc(const char *src, char* dst);

DLLEXPORT str_list_t	directory(const char* path);
DLLEXPORT long			create_archive(const char* archive, const char* format
						               ,bool with_path, str_list_t file_list, char* error, size_t maxerrlen);
DLLEXPORT char*			cmdstr(scfg_t*, user_t*, const char* instr, const char* fpath, const char* fspec, char* cmd, size_t);
DLLEXPORT long			extract_files_from_archive(const char* archive, const char* outdir, const char* allowed_filename_chars
						                           ,bool with_path, long max_files, str_list_t file_list, char* error, size_t);
DLLEXPORT int			archive_type(const char* archive, char* str, size_t size);
extern const char*		supported_archive_formats[];

/* Batch file transfer queues */
DLLEXPORT char*			batch_list_name(scfg_t* , uint usernumber, enum XFER_TYPE, char* fname, size_t);
DLLEXPORT FILE*			batch_list_open(scfg_t* , uint usernumber, enum XFER_TYPE, bool create);
DLLEXPORT str_list_t	batch_list_read(scfg_t* , uint usernumber, enum XFER_TYPE);
DLLEXPORT bool			batch_list_write(scfg_t*, uint usernumber, enum XFER_TYPE, str_list_t list);
DLLEXPORT bool			batch_list_clear(scfg_t*, uint usernumber, enum XFER_TYPE);

DLLEXPORT bool			batch_file_add(scfg_t*, uint usernumber, enum XFER_TYPE, file_t*);
DLLEXPORT bool			batch_file_exists(scfg_t*, uint usernumber, enum XFER_TYPE, const char* filename);
DLLEXPORT bool			batch_file_remove(scfg_t*, uint usernumber, enum XFER_TYPE, const char* filename);
DLLEXPORT size_t		batch_file_count(scfg_t*, uint usernumber, enum XFER_TYPE);
DLLEXPORT bool			batch_file_get(scfg_t*, str_list_t, const char* filename, file_t*);
DLLEXPORT int			batch_file_dir(scfg_t*, str_list_t, const char* filename);
DLLEXPORT bool			batch_file_load(scfg_t*, str_list_t, const char* filename, file_t*);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
