/* Synchronet file database-related exported functions */

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

#include "filedat.h"
#include "userdat.h"
#include "dat_rec.h"
#include "datewrap.h"	// time32()
#include "str_util.h"
#include "nopen.h"
#include "smblib.h"
#include "load_cfg.h"	// smb_open_dir()
#include "scfglib.h"

#include <stdbool.h>

/****************************************************************************/
/****************************************************************************/
BOOL findfile(scfg_t* cfg, uint dirnum, const char *filename)
{
	smb_t smb;

	if(cfg == NULL || filename == NULL)
		return FALSE;

	if(smb_open_dir(cfg, &smb, dirnum) != SMB_SUCCESS)
		return FALSE;

	int result = smb_findfile(&smb, filename, /* idx: */NULL);
	smb_close(&smb);
	return result == SMB_SUCCESS;
}

// This function may be called without opening the file base (for fast new-scans)
BOOL newfiles(smb_t* smb, time_t t)
{
	char str[MAX_PATH + 1];
	SAFEPRINTF(str, "%s.sid", smb->file);
	return fdate(str) > t;
}

// This function must be called with file base closed
BOOL datefileindex(smb_t* smb, time_t t)
{
	char path[MAX_PATH + 1];
	SAFEPRINTF(path, "%s.sid", smb->file);
	return setfdate(path, t) == 0;
}

static int filename_compare_a(const void *arg1, const void *arg2)
{
   return stricmp(*(char**) arg1, *(char**) arg2);
}

static int filename_compare_d(const void *arg1, const void *arg2)
{
   return stricmp(*(char**) arg2, *(char**) arg1);
}

static int filename_compare_ac(const void *arg1, const void *arg2)
{
   return strcmp(*(char**) arg1, *(char**) arg2);
}

static int filename_compare_dc(const void *arg1, const void *arg2)
{
   return strcmp(*(char**) arg2, *(char**) arg1);
}

void sortfilenames(str_list_t filelist, size_t count, enum file_sort order)
{
	switch(order) {
		case SORT_NAME_A:
			qsort(filelist, count, sizeof(*filelist), filename_compare_a);
			break;
		case SORT_NAME_D:
			qsort(filelist, count, sizeof(*filelist), filename_compare_d);
			break;
		case SORT_NAME_AC:
			qsort(filelist, count, sizeof(*filelist), filename_compare_ac);
			break;
		case SORT_NAME_DC:
			qsort(filelist, count, sizeof(*filelist), filename_compare_dc);
			break;
		default:
			break;
	}
}

// Return an optionally-sorted dynamically-allocated string list of filenames added since date/time (t)
str_list_t loadfilenames(scfg_t* cfg, smb_t* smb, const char* filespec, time_t t, BOOL sort, size_t* count)
{
	size_t c;

	if(count == NULL)
		count = &c;

	*count = 0;

	long start = 0;	
	if(t) {
		idxrec_t idx;
		start = smb_getmsgidx_by_time(smb, &idx, t);
		if(start < 0)
			return NULL;
	}

	char** file_list = malloc((smb->status.total_files + 1) * sizeof(char*));
	if(file_list == NULL)
		return NULL;
	memset(file_list, 0, (smb->status.total_files + 1) * sizeof(char*));

	fseek(smb->sid_fp, start * sizeof(fileidxrec_t), SEEK_SET);
	while(!feof(smb->sid_fp)) {
		fileidxrec_t fidx;

		if(smb_fread(smb, &fidx, sizeof(fidx), smb->sid_fp) != sizeof(fidx))
			break;

		if(fidx.idx.number==0)	/* invalid message number, ignore */
			continue;

		if(filespec != NULL && *filespec != '\0') {
			if(!wildmatchi(fidx.name, filespec, /* path: */FALSE))
				continue;
		}
		file_list[*count] = strdup(fidx.name);
		(*count)++;
	}
	if(sort)
		sortfilenames(file_list, *count, (enum file_sort)cfg->dir[smb->dirnum]->sort);

	return file_list;
}

// Load and optionally-sort files from an open filebase into a dynamically-allocated list of "objects"
smbfile_t* loadfiles(scfg_t* cfg, smb_t* smb, const char* filespec, time_t t, BOOL extdesc, BOOL sort, size_t* count)
{
	*count = 0;

	long start = 0;	
	if(t) {
		idxrec_t idx;
		start = smb_getmsgidx_by_time(smb, &idx, t);
		if(start < 0)
			return NULL;
	}

	smbfile_t* file_list = malloc(smb->status.total_files * sizeof(smbfile_t));
	if(file_list == NULL)
		return NULL;
	memset(file_list, 0, smb->status.total_files * sizeof(smbfile_t));

	fseek(smb->sid_fp, start * sizeof(fileidxrec_t), SEEK_SET);
	while(!feof(smb->sid_fp)) {
		fileidxrec_t fidx;

		if(smb_fread(smb, &fidx, sizeof(fidx), smb->sid_fp) != sizeof(fidx))
			break;

		if(fidx.idx.number==0)	/* invalid message number, ignore */
			continue;

		if(filespec != NULL && *filespec != '\0') {
			if(!wildmatchi(fidx.name, filespec, /* path: */FALSE))
				continue;
		}
		smbfile_t file = { .idx = fidx.idx };
		int result = smb_getmsghdr(smb, &file);
		if(result != SMB_SUCCESS)
			break;
		if(extdesc)
			file.extdesc = smb_getmsgtxt(smb, &file, GETMSGTXT_ALL);
		file.dir = smb->dirnum;
		file_list[*count] = file;
		(*count)++;
	}
	if(sort)
		sortfiles(file_list, *count, (enum file_sort)cfg->dir[smb->dirnum]->sort);

	return file_list;
}

static int file_compare_name_a(const void* v1, const void* v2)
{
	smbfile_t* f1 = (smbfile_t*)v1;
	smbfile_t* f2 = (smbfile_t*)v2;

	return stricmp(f1->name, f2->name);
}

static int file_compare_name_ac(const void* v1, const void* v2)
{
	smbfile_t* f1 = (smbfile_t*)v1;
	smbfile_t* f2 = (smbfile_t*)v2;

	return strcmp(f1->name, f2->name);
}

static int file_compare_name_d(const void* v1, const void* v2)
{
	smbfile_t* f1 = (smbfile_t*)v1;
	smbfile_t* f2 = (smbfile_t*)v2;

	return stricmp(f2->name, f1->name);
}

static int file_compare_name_dc(const void* v1, const void* v2)
{
	smbfile_t* f1 = (smbfile_t*)v1;
	smbfile_t* f2 = (smbfile_t*)v2;

	return strcmp(f2->name, f1->name);
}

static int file_compare_date_a(const void* v1, const void* v2)
{
	smbfile_t* f1 = (smbfile_t*)v1;
	smbfile_t* f2 = (smbfile_t*)v2;

	return f1->hdr.when_imported.time - f2->hdr.when_imported.time;
}

static int file_compare_date_d(const void* v1, const void* v2)
{
	smbfile_t* f1 = (smbfile_t*)v1;
	smbfile_t* f2 = (smbfile_t*)v2;

	return f2->hdr.when_imported.time - f1->hdr.when_imported.time;
}

void sortfiles(smbfile_t* filelist, size_t count, enum file_sort order)
{
	switch(order) {
		case SORT_NAME_A:
			qsort(filelist, count, sizeof(*filelist), file_compare_name_a);
			break;
		case SORT_NAME_D:
			qsort(filelist, count, sizeof(*filelist), file_compare_name_d);
			break;
		case SORT_NAME_AC:
			qsort(filelist, count, sizeof(*filelist), file_compare_name_ac);
			break;
		case SORT_NAME_DC:
			qsort(filelist, count, sizeof(*filelist), file_compare_name_dc);
			break;
		case SORT_DATE_A:
			qsort(filelist, count, sizeof(*filelist), file_compare_date_a);
			break;
		case SORT_DATE_D:
			qsort(filelist, count, sizeof(*filelist), file_compare_date_d);
			break; 
	}
}

void freefiles(smbfile_t* filelist, size_t count)
{
	for(size_t i = 0; i < count; i++)
		smb_freefilemem(&filelist[i]);
	free(filelist);
}

BOOL loadfile(scfg_t* cfg, uint dirnum, const char* filename, smbfile_t* file)
{
	smb_t smb;

	if(smb_open_dir(cfg, &smb, dirnum) != SMB_SUCCESS)
		return FALSE;

	int result = smb_loadfile(&smb, filename, file);
	smb_close(&smb);
	if(cfg->dir[dirnum]->misc & DIR_FREE)
		file->cost = 0;
	return result == SMB_SUCCESS;
}

char* batch_list_name(scfg_t* cfg, uint usernumber, enum XFER_TYPE type, char* fname, size_t size)
{
	safe_snprintf(fname, size, "%suser/%04u.%sload", cfg->data_dir, usernumber
		,(type == XFER_UPLOAD || type == XFER_BATCH_UPLOAD) ? "up" : "dn");
	return fname;
}

FILE* batch_list_open(scfg_t* cfg, uint usernumber, enum XFER_TYPE type, BOOL create)
{
	char path[MAX_PATH + 1];
	return iniOpenFile(batch_list_name(cfg, usernumber, type, path, sizeof(path)), create);
}

str_list_t batch_list_read(scfg_t* cfg, uint usernumber, enum XFER_TYPE type)
{
	FILE* fp = batch_list_open(cfg, usernumber, type, /* create: */FALSE);
	if(fp == NULL)
		return NULL;
	str_list_t ini = iniReadFile(fp);
	iniCloseFile(fp);
	return ini;
}

BOOL batch_list_write(scfg_t* cfg, uint usernumber, enum XFER_TYPE type, str_list_t list)
{
	FILE* fp = batch_list_open(cfg, usernumber, type, /* create: */TRUE);
	if(fp == NULL)
		return FALSE;
	BOOL result = iniWriteFile(fp, list);
	iniCloseFile(fp);
	return result;
}

BOOL batch_list_clear(scfg_t* cfg, uint usernumber, enum XFER_TYPE type)
{
	char path[MAX_PATH + 1];
	return remove(batch_list_name(cfg, usernumber, type, path, sizeof(path))) == 0;
}

size_t batch_file_count(scfg_t* cfg, uint usernumber, enum XFER_TYPE type)
{
	FILE* fp = batch_list_open(cfg, usernumber, type, /* create: */FALSE);
	if(fp == NULL)
		return 0;
	size_t result = iniReadSectionCount(fp, /* prefix: */NULL);
	iniCloseFile(fp);
	return result;
}

BOOL batch_file_remove(scfg_t* cfg, uint usernumber, enum XFER_TYPE type, const char* filename)
{
	FILE* fp = batch_list_open(cfg, usernumber, type, /* create: */FALSE);
	if(fp == NULL)
		return FALSE;
	str_list_t ini = iniReadFile(fp);
	BOOL result = iniRemoveSection(&ini, filename);
	iniWriteFile(fp, ini);
	iniCloseFile(fp);
	iniFreeStringList(ini);
	return result;
}

BOOL batch_file_exists(scfg_t* cfg, uint usernumber, enum XFER_TYPE type, const char* filename)
{
	FILE* fp = batch_list_open(cfg, usernumber, type, /* create: */FALSE);
	if(fp == NULL)
		return FALSE;
	str_list_t ini = iniReadFile(fp);
	BOOL result = iniSectionExists(ini, filename);
	iniCloseFile(fp);
	iniFreeStringList(ini);
	return result;
}

BOOL batch_file_add(scfg_t* cfg, uint usernumber, enum XFER_TYPE type, smbfile_t* f)
{
	FILE* fp = batch_list_open(cfg, usernumber, type, /* create: */TRUE);
	if(fp == NULL)
		return FALSE;
	fseek(fp, 0, SEEK_END);
	fprintf(fp, "\n[%s]\n", f->name);
	if(f->dir >= 0 && f->dir < cfg->total_dirs)
		fprintf(fp, "dir=%s\n", cfg->dir[f->dir]->code);
	fprintf(fp, "desc=%s\n", f->desc);
	fprintf(fp, "altpath=%u\n", f->hdr.altpath);
	fclose(fp);
	return TRUE;
}

BOOL batch_file_get(scfg_t* cfg, str_list_t ini, const char* filename, smbfile_t* f)
{
	char value[INI_MAX_VALUE_LEN + 1];

	if(!iniSectionExists(ini, filename))
		return FALSE;
	f->dir = batch_file_dir(cfg, ini, filename);
	if(f->dir < 0 || f->dir >= cfg->total_dirs)
		return FALSE;
	smb_hfield_str(f, SMB_FILENAME, filename);
	smb_hfield_str(f, SMB_FILEDESC, iniGetString(ini, filename, "desc", NULL, value));
	f->hdr.altpath = iniGetShortInt(ini, filename, "altpath", 0);
	return TRUE;
}

int batch_file_dir(scfg_t* cfg, str_list_t ini, const char* filename)
{
	char value[INI_MAX_VALUE_LEN + 1];
	return getdirnum(cfg, iniGetString(ini, filename, "dir", NULL, value));
}

BOOL batch_file_load(scfg_t* cfg, str_list_t ini, const char* filename, smbfile_t* f)
{
	if(!iniSectionExists(ini, filename))
		return FALSE;
	f->dir = batch_file_dir(cfg, ini, filename);
	if(f->dir < 0)
		return FALSE;
	return loadfile(cfg, f->dir, filename, f);
}

BOOL updatefile(scfg_t* cfg, smbfile_t* file)
{
	smb_t smb;

	if(smb_open_dir(cfg, &smb, file->dir) != SMB_SUCCESS)
		return FALSE;

	int result = smb_updatemsg(&smb, file) == SMB_SUCCESS;
	smb_close(&smb);
	return result == SMB_SUCCESS;
}

BOOL removefile(scfg_t* cfg, uint dirnum, const char* filename)
{
	smb_t smb;

	if(smb_open_dir(cfg, &smb, dirnum) != SMB_SUCCESS)
		return FALSE;

	int result;
	smbfile_t file;
	if((result = smb_loadfile(&smb, filename, &file)) == SMB_SUCCESS) {
		result = smb_removefile(&smb, &file);
		smb_freefilemem(&file);
	}
	smb_close(&smb);
	return result == SMB_SUCCESS;
}

/****************************************************************************/
/* Returns full (case-corrected) path to specified file						*/
/* 'path' should be MAX_PATH + 1 chars in size								*/
/* If the directory is configured to allow file-checking and the file does	*/
/* not exist, the size element is set to -1.								*/
/****************************************************************************/
char* getfilepath(scfg_t* cfg, smbfile_t* f, char* path)
{
	bool fchk = true;
	if(f->dir >= cfg->total_dirs)
		safe_snprintf(path, MAX_PATH, "%s%s", cfg->temp_dir, f->name);
	else {
		safe_snprintf(path, MAX_PATH, "%s%s", f->hdr.altpath > 0 && f->hdr.altpath <= cfg->altpaths 
			? cfg->altpath[f->hdr.altpath-1] : cfg->dir[f->dir]->path
			,f->name);
		fchk = (cfg->dir[f->dir]->misc & DIR_FCHK) != 0;
	}
	if(f->size == 0 && fchk && !fexistcase(path))
		f->size = -1;
	return path;
}

off_t getfilesize(scfg_t* cfg, smbfile_t* f)
{
	char fpath[MAX_PATH + 1];
	if(f->size > 0)
		return f->size;
	f->size = flength(getfilepath(cfg, f, fpath));
	return f->size;
}

time_t getfiletime(scfg_t* cfg, smbfile_t* f)
{
	char fpath[MAX_PATH + 1];
	if(f->time > 0)
		return f->time;
	f->time = fdate(getfilepath(cfg, f, fpath));
	return f->time;
}

ulong gettimetodl(scfg_t* cfg, smbfile_t* f, uint rate_cps)
{
	if(getfilesize(cfg, f) < 1)
		return 0;
	if(f->size <= (off_t)rate_cps)
		return 1;
	return f->size / rate_cps;
}

BOOL addfile(scfg_t* cfg, uint dirnum, smbfile_t* file, const char* extdesc)
{
	smb_t smb;

	if(smb_open_dir(cfg, &smb, dirnum) != SMB_SUCCESS)
		return FALSE;

	int result = smb_addfile(&smb, file, SMB_SELFPACK, extdesc);
	smb_close(&smb);
	return result == SMB_SUCCESS;
}

/* 'size' does not include the NUL-terminator */
char* format_filename(const char* fname, char* buf, size_t size, BOOL pad)
{
	size_t fnlen = strlen(fname);
	char* ext = getfext(fname);
	if(ext != NULL) {
		size_t extlen = strlen(ext);
		if(extlen >= size)
			safe_snprintf(buf, size + 1, "%s", fname);
		else {
			fnlen -= extlen;
			if(fnlen > size - extlen)
				fnlen = size - extlen;
			safe_snprintf(buf, size + 1, "%-*.*s%s", (int)(pad ? (size - extlen) : 0), (int)fnlen, fname, ext);
		}
	} else	/* no extension */
		snprintf(buf, size + 1, "%s", fname);
	return buf;
}

