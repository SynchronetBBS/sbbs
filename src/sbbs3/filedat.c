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
#include "sauce.h"
#include "crc32.h"

/* libarchive: */
#include <archive.h>
#include <archive_entry.h>

 /****************************************************************************/
/****************************************************************************/
bool findfile(scfg_t* cfg, uint dirnum, const char *filename, file_t* file)
{
	smb_t smb;

	if(cfg == NULL || filename == NULL)
		return false;

	if(!smb_init_dir(cfg, &smb, dirnum))
		return false;
	if(smb_open_index(&smb) != SMB_SUCCESS)
		return false;
	int result = smb_findfile(&smb, filename, file);
	smb_close(&smb);
	return result == SMB_SUCCESS;
}

// This function may be called without opening the file base (for fast new-scans)
time_t newfiletime(smb_t* smb)
{
	char str[MAX_PATH + 1];
	SAFEPRINTF(str, "%s.sid", smb->file);
	return fdate(str);
}

time_t dir_newfiletime(scfg_t* cfg, uint dirnum)
{
	smb_t smb;

	if(!smb_init_dir(cfg, &smb, dirnum))
		return -1;
	return newfiletime(&smb);
}

// This function may be called without opening the file base (for fast new-scans)
bool newfiles(smb_t* smb, time_t t)
{
	return newfiletime(smb) > t;
}

// This function *must* be called with file base closed
bool update_newfiletime(smb_t* smb, time_t t)
{
	char path[MAX_PATH + 1];
	SAFEPRINTF(path, "%s.sid", smb->file);
	return setfdate(path, t) == 0;
}

time_t lastfiletime(smb_t* smb)
{
	idxrec_t idx;
	if(smb_getlastidx(smb, &idx) != SMB_SUCCESS)
		return (time_t)-1;
	return idx.time;
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

// Note: does not support sorting by date
void sortfilenames(str_list_t filelist, size_t count, enum file_sort order)
{
	switch(order) {
		case FILE_SORT_NAME_A:
			qsort(filelist, count, sizeof(*filelist), filename_compare_a);
			break;
		case FILE_SORT_NAME_D:
			qsort(filelist, count, sizeof(*filelist), filename_compare_d);
			break;
		case FILE_SORT_NAME_AC:
			qsort(filelist, count, sizeof(*filelist), filename_compare_ac);
			break;
		case FILE_SORT_NAME_DC:
			qsort(filelist, count, sizeof(*filelist), filename_compare_dc);
			break;
		default:
			break;
	}
}

// Return an optionally-sorted dynamically-allocated string list of filenames added since date/time (t)
// Note: does not support sorting by date (exception: natural sort order of date-ascending)
str_list_t loadfilenames(smb_t* smb, const char* filespec, time_t t, enum file_sort order, size_t* count)
{
	size_t count_;

	if(count == NULL)
		count = &count_;

	*count = 0;

	long start = 0;	
	if(t > 0) {
		idxrec_t idx;
		start = smb_getmsgidx_by_time(smb, &idx, t);
		if(start < 0)
			return NULL;
	}

	if(fseek(smb->sid_fp, start * sizeof(fileidxrec_t), SEEK_SET) != 0)
		return NULL;

	char** file_list = calloc(smb->status.total_files + 1, sizeof(char*));
	if(file_list == NULL)
		return NULL;

	size_t offset = start;
	while(!feof(smb->sid_fp) && offset < smb->status.total_files) {
		fileidxrec_t fidx;

		if(smb_fread(smb, &fidx, sizeof(fidx), smb->sid_fp) != sizeof(fidx))
			break;

		offset++;

		if(fidx.idx.number == 0)	/* invalid message number, ignore */
			continue;

		TERMINATE(fidx.name);

		if(filespec != NULL && *filespec != '\0') {
			if(!wildmatch(fidx.name, filespec, /* path: */false, /* case-sensitive: */false))
				continue;
		}
		file_list[*count] = strdup(fidx.name);
		(*count)++;
	}
	if(order != FILE_SORT_NATURAL)
		sortfilenames(file_list, *count, order);

	return file_list;
}

// Load and optionally-sort files from an open filebase into a dynamically-allocated list of "objects"
file_t* loadfiles(smb_t* smb, const char* filespec, time_t t, enum file_detail detail, enum file_sort order, size_t* count)
{
	*count = 0;

	long start = 0;	
	if(t) {
		idxrec_t idx;
		start = smb_getmsgidx_by_time(smb, &idx, t);
		if(start < 0)
			return NULL;
	}

	if(fseek(smb->sid_fp, start * sizeof(fileidxrec_t), SEEK_SET) != 0)
		return NULL;

	file_t* file_list = calloc(smb->status.total_files, sizeof(file_t));
	if(file_list == NULL)
		return NULL;

	size_t offset = start;
	size_t cnt = 0;
	while(!feof(smb->sid_fp) && offset < smb->status.total_files) {
		file_t* f = &file_list[cnt];

		if(smb_fread(smb, &f->file_idx, sizeof(f->file_idx), smb->sid_fp) != sizeof(f->file_idx))
			break;

		f->idx_offset = offset++;

		if(f->idx.number == 0)	/* invalid message number, ignore */
			continue;

		TERMINATE(f->file_idx.name);

		if(filespec != NULL && *filespec != '\0') {
			if(!wildmatch(f->file_idx.name, filespec, /* path: */false, /* case-sensitive: */false))
				continue;
		}
		cnt++;
	}
	if(order != FILE_SORT_NATURAL)
		sortfiles(file_list, cnt, order);

	// Read (and set convenience pointers) *after* sorting the list
	for((*count) = 0; (*count) < cnt; (*count)++) {
		int result = smb_getfile(smb, &file_list[*count], detail);
		if(result != SMB_SUCCESS)
			break;
	}
	return file_list;
}

static int file_compare_name_a(const void* v1, const void* v2)
{
	file_t* f1 = (file_t*)v1;
	file_t* f2 = (file_t*)v2;

	return stricmp(f1->file_idx.name, f2->file_idx.name);
}

static int file_compare_name_ac(const void* v1, const void* v2)
{
	file_t* f1 = (file_t*)v1;
	file_t* f2 = (file_t*)v2;

	return strcmp(f1->file_idx.name, f2->file_idx.name);
}

static int file_compare_name_d(const void* v1, const void* v2)
{
	file_t* f1 = (file_t*)v1;
	file_t* f2 = (file_t*)v2;

	return stricmp(f2->file_idx.name, f1->file_idx.name);
}

static int file_compare_name_dc(const void* v1, const void* v2)
{
	file_t* f1 = (file_t*)v1;
	file_t* f2 = (file_t*)v2;

	return strcmp(f2->file_idx.name, f1->file_idx.name);
}

static int file_compare_date_a(const void* v1, const void* v2)
{
	file_t* f1 = (file_t*)v1;
	file_t* f2 = (file_t*)v2;

	return f1->hdr.when_imported.time - f2->hdr.when_imported.time;
}

static int file_compare_date_d(const void* v1, const void* v2)
{
	file_t* f1 = (file_t*)v1;
	file_t* f2 = (file_t*)v2;

	return f2->hdr.when_imported.time - f1->hdr.when_imported.time;
}

void sortfiles(file_t* filelist, size_t count, enum file_sort order)
{
	switch(order) {
		case FILE_SORT_NAME_A:
			qsort(filelist, count, sizeof(*filelist), file_compare_name_a);
			break;
		case FILE_SORT_NAME_D:
			qsort(filelist, count, sizeof(*filelist), file_compare_name_d);
			break;
		case FILE_SORT_NAME_AC:
			qsort(filelist, count, sizeof(*filelist), file_compare_name_ac);
			break;
		case FILE_SORT_NAME_DC:
			qsort(filelist, count, sizeof(*filelist), file_compare_name_dc);
			break;
		case FILE_SORT_DATE_A:
			qsort(filelist, count, sizeof(*filelist), file_compare_date_a);
			break;
		case FILE_SORT_DATE_D:
			qsort(filelist, count, sizeof(*filelist), file_compare_date_d);
			break; 
	}
}

void freefiles(file_t* filelist, size_t count)
{
	for(size_t i = 0; i < count; i++)
		smb_freefilemem(&filelist[i]);
	free(filelist);
}

bool loadfile(scfg_t* cfg, uint dirnum, const char* filename, file_t* file, enum file_detail detail)
{
	smb_t smb;

	if(smb_open_dir(cfg, &smb, dirnum) != SMB_SUCCESS)
		return false;

	int result = smb_loadfile(&smb, filename, file, detail);
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

FILE* batch_list_open(scfg_t* cfg, uint usernumber, enum XFER_TYPE type, bool create)
{
	char path[MAX_PATH + 1];
	return iniOpenFile(batch_list_name(cfg, usernumber, type, path, sizeof(path)), create);
}

str_list_t batch_list_read(scfg_t* cfg, uint usernumber, enum XFER_TYPE type)
{
	FILE* fp = batch_list_open(cfg, usernumber, type, /* create: */false);
	if(fp == NULL)
		return NULL;
	str_list_t ini = iniReadFile(fp);
	iniCloseFile(fp);
	return ini;
}

bool batch_list_write(scfg_t* cfg, uint usernumber, enum XFER_TYPE type, str_list_t list)
{
	FILE* fp = batch_list_open(cfg, usernumber, type, /* create: */true);
	if(fp == NULL)
		return false;
	bool result = iniWriteFile(fp, list);
	iniCloseFile(fp);
	return result;
}

bool batch_list_clear(scfg_t* cfg, uint usernumber, enum XFER_TYPE type)
{
	char path[MAX_PATH + 1];
	return remove(batch_list_name(cfg, usernumber, type, path, sizeof(path))) == 0;
}

size_t batch_file_count(scfg_t* cfg, uint usernumber, enum XFER_TYPE type)
{
	FILE* fp = batch_list_open(cfg, usernumber, type, /* create: */false);
	if(fp == NULL)
		return 0;
	size_t result = iniReadSectionCount(fp, /* prefix: */NULL);
	iniCloseFile(fp);
	return result;
}

bool batch_file_remove(scfg_t* cfg, uint usernumber, enum XFER_TYPE type, const char* filename)
{
	FILE* fp = batch_list_open(cfg, usernumber, type, /* create: */false);
	if(fp == NULL)
		return false;
	str_list_t ini = iniReadFile(fp);
	bool result = iniRemoveSection(&ini, filename);
	iniWriteFile(fp, ini);
	iniCloseFile(fp);
	iniFreeStringList(ini);
	return result;
}

bool batch_file_exists(scfg_t* cfg, uint usernumber, enum XFER_TYPE type, const char* filename)
{
	FILE* fp = batch_list_open(cfg, usernumber, type, /* create: */false);
	if(fp == NULL)
		return false;
	str_list_t ini = iniReadFile(fp);
	bool result = iniSectionExists(ini, filename);
	iniCloseFile(fp);
	iniFreeStringList(ini);
	return result;
}

bool batch_file_add(scfg_t* cfg, uint usernumber, enum XFER_TYPE type, file_t* f)
{
	FILE* fp = batch_list_open(cfg, usernumber, type, /* create: */true);
	if(fp == NULL)
		return false;
	if(fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return false;
	}
	fprintf(fp, "\n[%s]\n", f->name);
	if(f->dir >= 0 && f->dir < cfg->total_dirs)
		fprintf(fp, "dir=%s\n", cfg->dir[f->dir]->code);
	if(f->desc != NULL)
		fprintf(fp, "desc=%s\n", f->desc);
	if(f->tags != NULL)
		fprintf(fp, "tags=%s\n", f->tags);
	fclose(fp);
	return true;
}

bool batch_file_get(scfg_t* cfg, str_list_t ini, const char* filename, file_t* f)
{
	char* p;
	char value[INI_MAX_VALUE_LEN + 1];

	if(!iniSectionExists(ini, filename))
		return false;
	f->dir = batch_file_dir(cfg, ini, filename);
	if(f->dir < 0 || f->dir >= cfg->total_dirs)
		return false;
	smb_hfield_str(f, SMB_FILENAME, filename);
	if((p = iniGetString(ini, filename, "desc", NULL, value)) != NULL)
		smb_hfield_str(f, SMB_FILEDESC, p);
	if((p = iniGetString(ini, filename, "tags", NULL, value)) != NULL)
		smb_hfield_str(f, SMB_TAGS, p);
	return true;
}

int batch_file_dir(scfg_t* cfg, str_list_t ini, const char* filename)
{
	char value[INI_MAX_VALUE_LEN + 1];
	return getdirnum(cfg, iniGetString(ini, filename, "dir", NULL, value));
}

bool batch_file_load(scfg_t* cfg, str_list_t ini, const char* filename, file_t* f)
{
	if(!iniSectionExists(ini, filename))
		return false;
	f->dir = batch_file_dir(cfg, ini, filename);
	if(f->dir < 0)
		return false;
	return loadfile(cfg, f->dir, filename, f, file_detail_normal);
}

bool updatefile(scfg_t* cfg, file_t* file)
{
	smb_t smb;

	if(smb_open_dir(cfg, &smb, file->dir) != SMB_SUCCESS)
		return false;

	int result = smb_updatemsg(&smb, file);
	smb_close(&smb);
	return result == SMB_SUCCESS;
}

bool removefile(scfg_t* cfg, uint dirnum, const char* filename)
{
	smb_t smb;

	if(smb_open_dir(cfg, &smb, dirnum) != SMB_SUCCESS)
		return false;

	int result;
	file_t file;
	if((result = smb_loadfile(&smb, filename, &file, file_detail_normal)) == SMB_SUCCESS) {
		result = smb_removefile(&smb, &file);
		smb_freefilemem(&file);
	}
	smb_close(&smb);
	return result == SMB_SUCCESS;
}

ulong getuserxfers(scfg_t* cfg, const char* from, uint to)
{
	smb_t smb;
	ulong found = 0;

	if(cfg == NULL)
		return 0;
	if(cfg->user_dir >= cfg->total_dirs)
		return 0;

	if(smb_open_dir(cfg, &smb, cfg->user_dir) != SMB_SUCCESS)
		return 0;

	char usernum[16];
	SAFEPRINTF(usernum, "%u", to);
	size_t count = 0;
	file_t* list = loadfiles(&smb, /* filespec */ NULL, /* since: */0, file_detail_normal, FILE_SORT_NATURAL, &count);
	for(size_t i = 0; i < count; i++) {
		file_t* f = &list[i];
		if(from != NULL && (f->from == NULL || stricmp(f->from, from) != 0))
			continue;
		if(to != 0) {
			str_list_t dest_user_list = strListSplitCopy(NULL, f->to_list, ",");
			int dest_user = strListFind(dest_user_list, usernum, /* case-sensitive: */true);
			strListFree(&dest_user_list);
			if(dest_user < 0)
				continue;
		}
		found++;
	}

	smb_close(&smb);
	freefiles(list, count);
	return found;
}

/****************************************************************************/
/* Returns full (case-corrected) path to specified file						*/
/* 'path' should be MAX_PATH + 1 chars in size								*/
/* If the directory is configured to allow file-checking and the file does	*/
/* not exist, the size element is set to -1.								*/
/****************************************************************************/
char* getfilepath(scfg_t* cfg, file_t* f, char* path)
{
	bool fchk = true;
	const char* name = f->name == NULL ? f->file_idx.name : f->name;
	if(f->dir >= cfg->total_dirs)
		safe_snprintf(path, MAX_PATH, "%s%s", cfg->temp_dir, name);
	else {
		safe_snprintf(path, MAX_PATH, "%s%s", cfg->dir[f->dir]->path, name);
		fchk = (cfg->dir[f->dir]->misc & DIR_FCHK) != 0;
	}
	if(f->size == 0 && fchk && !fexistcase(path))
		f->size = -1;
	return path;
}

off_t getfilesize(scfg_t* cfg, file_t* f)
{
	char fpath[MAX_PATH + 1];
	if(f->size > 0)
		return f->size;
	f->size = flength(getfilepath(cfg, f, fpath));
	return f->size;
}

time_t getfiletime(scfg_t* cfg, file_t* f)
{
	char fpath[MAX_PATH + 1];
	if(f->time > 0)
		return f->time;
	f->time = fdate(getfilepath(cfg, f, fpath));
	return f->time;
}

ulong gettimetodl(scfg_t* cfg, file_t* f, uint rate_cps)
{
	if(getfilesize(cfg, f) < 1)
		return 0;
	if(f->size <= (off_t)rate_cps)
		return 1;
	return (ulong)(f->size / rate_cps);
}

bool hashfile(scfg_t* cfg, file_t* f)
{
	bool result = false;
	smb_t smb;

	if(cfg->dir[f->dir]->misc & DIR_NOHASH)
		return false;

	if(smb_open_dir(cfg, &smb, f->dir) != SMB_SUCCESS)
		return false;

	if(!(smb.status.attr & SMB_NOHASH)) {
		char fpath[MAX_PATH + 1];
		getfilepath(cfg, f, fpath);
		if((f->file_idx.hash.flags = smb_hashfile(fpath, getfilesize(cfg, f), &f->file_idx.hash.data)) != 0)
			result = true;
	}
	smb_close(&smb);
	return result;
}

int file_client_hfields(file_t* f, client_t* client)
{
	int		i;

	if(client == NULL)
		return -1;

	if(*client->addr && (i = smb_hfield_str(f, SENDERIPADDR, client->addr)) != SMB_SUCCESS)
		return i;
	if(*client->host && (i = smb_hfield_str(f, SENDERHOSTNAME, client->host)) != SMB_SUCCESS)
		return i;
	if(client->protocol != NULL && (i = smb_hfield_str(f, SENDERPROTOCOL, client->protocol)) != SMB_SUCCESS)
		return i;
	if(client->port) {
		char	port[16];
		SAFEPRINTF(port,"%u",client->port);
		return smb_hfield_str(f, SENDERPORT, port);
	}
	return SMB_SUCCESS;
}

int file_sauce_hfields(file_t* f, struct sauce_charinfo* info)
{
	int		i;

	if(info == NULL)
		return -1;

	if(*info->author && (i = smb_new_hfield_str(f, SMB_AUTHOR, info->author)) != SMB_SUCCESS)
		return i;

	if(*info->group && (i = smb_new_hfield_str(f, SMB_AUTHOR_ORG, info->group)) != SMB_SUCCESS)
		return i;

	if(f->desc == NULL && *info->title && (i = smb_new_hfield_str(f, SMB_FILEDESC, info->title)) != SMB_SUCCESS)
		return i;

	return SMB_SUCCESS;
}

bool addfile(scfg_t* cfg, uint dirnum, file_t* f, const char* extdesc, client_t* client)
{
	char fpath[MAX_PATH + 1];
	smb_t smb;

	if(smb_open_dir(cfg, &smb, dirnum) != SMB_SUCCESS)
		return false;

	getfilepath(cfg, f, fpath);
	if(f->from_ip == NULL)
		file_client_hfields(f, client);
	int result = smb_addfile(&smb, f, SMB_SELFPACK, extdesc, /* contents: */NULL, fpath);
	smb_close(&smb);
	return result == SMB_SUCCESS;
}

/* 'size' does not include the NUL-terminator */
char* format_filename(const char* fname, char* buf, size_t size, bool pad)
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

int archive_type(const char* archive, char* str, size_t size)
{
	int result;
	struct archive *ar;
	struct archive_entry *entry;

	if((ar = archive_read_new()) == NULL) {
		safe_snprintf(str, size, "archive_read_new returned NULL");
		return -1;
	}
	archive_read_support_filter_all(ar);
	archive_read_support_format_all(ar);
	if((result = archive_read_open_filename(ar, archive, 10240)) != ARCHIVE_OK) {
		safe_snprintf(str, size, "archive_read_open_filename returned %d: %s"
			,result, archive_error_string(ar));
		archive_read_free(ar);
		return result;
	}
	result = archive_filter_code(ar, 0);
	if(result >= 0) {
		int comp = result;
		result = archive_read_next_header(ar, &entry);
		if(result != ARCHIVE_OK)
			safe_snprintf(str, size, "archive_read_next_header returned %d: %s"
				,result, archive_error_string(ar));
		else {
			result = archive_format(ar);
			if(comp > 0)
				safe_snprintf(str, size, "%s/%s", archive_filter_name(ar, 0), archive_format_name(ar));
			else
				safe_snprintf(str, size, "%s", archive_format_name(ar));
		}
	}
	archive_read_free(ar);
	return result;
}

str_list_t directory(const char* path)
{
	int			flags = GLOB_MARK;
	glob_t		g;

	if(glob(path, flags, NULL, &g) != 0)
		return NULL;
	str_list_t list = strListInit();
	for(size_t i = 0; i < g.gl_pathc; i++) {
		strListPush(&list, g.gl_pathv[i]);
	}
	globfree(&g);
	return list;
}

const char* supported_archive_formats[] = { "zip", "7z", "tgz", "tbz", NULL };
// Returns negative on error
long create_archive(const char* archive, const char* format
	,bool with_path, str_list_t file_list, char* error, size_t maxerrlen)
{
	int result;
	struct archive *ar;

	if(file_list == NULL || *file_list == NULL)
		return 0;

	if((ar = archive_write_new()) == NULL) {
		safe_snprintf(error, maxerrlen, "archive_write_new returned NULL");
		return -1;
	}
	if(stricmp(format, "tgz") == 0) {
		archive_write_add_filter_gzip(ar);
		archive_write_set_format_pax_restricted(ar);
	} else if(stricmp(format, "tbz") == 0) {
		archive_write_add_filter_bzip2(ar);
		archive_write_set_format_pax_restricted(ar);
	} else if(stricmp(format, "zip") == 0) {
		archive_write_set_format_zip(ar);
	} else if(stricmp(format, "7z") == 0) {
		archive_write_set_format_7zip(ar);
	} else {
		safe_snprintf(error, maxerrlen, "unsupported format: %s", format);
		return -2;
	}
	if((result = archive_write_open_filename(ar, archive)) != ARCHIVE_OK) {
		safe_snprintf(error, maxerrlen, "archive_write_open_filename(%s) returned %d: %s"
			,archive, result, archive_error_string(ar));
		archive_write_free(ar);
		return result;
	}
	ulong file_count = 0;
	for(;file_list[file_count] != NULL; file_count++) {
		struct archive_entry* entry;
		struct stat st;
		const char* filename = file_list[file_count];
		FILE* fp = fopen(filename, "rb");
		if(fp == NULL) {
			safe_snprintf(error, maxerrlen, "%d opening %s", errno, filename);
			break;
		}
		if(fstat(fileno(fp), &st) != 0) {
			safe_snprintf(error, maxerrlen, "%d fstat %s", errno, filename);
			fclose(fp);
			break;
		}
		if((entry = archive_entry_new()) == NULL) {
			safe_snprintf(error, maxerrlen, "archive_entry_new returned NULL");
			fclose(fp);
			break;
		}
		if(with_path)
			archive_entry_set_pathname(entry, filename);
		else
			archive_entry_set_pathname(entry, getfname(filename));
		archive_entry_set_size(entry, st.st_size);
		archive_entry_set_mtime(entry, st.st_mtime, 0);
		archive_entry_set_filetype(entry, AE_IFREG);
		archive_entry_set_perm(entry, 0644);
		if((result = archive_write_header(ar, entry)) != ARCHIVE_OK)
			safe_snprintf(error, maxerrlen, "archive_write_header returned %d", result);
		else while(!feof(fp)) {
			char buf[256 * 1024];
			size_t len = fread(buf, 1, sizeof(buf), fp);
			if((result = archive_write_data(ar, buf, len)) != len) {
				safe_snprintf(error, maxerrlen, "archive_write_data returned %d instead of %d", result, (int)len);
				break;
			} else
				result = ARCHIVE_OK;
		}
		fclose(fp);
		archive_entry_free(entry);
		if(result != ARCHIVE_OK)
			break;
	}
	archive_write_close(ar);
	archive_write_free(ar);
	if(file_list[file_count] != NULL)
		return result < 0 ? result : -1;
	return file_count;
}

long extract_files_from_archive(const char* archive, const char* outdir, const char* allowed_filename_chars
	,bool with_path, long max_files, str_list_t file_list, char* error, size_t maxerrlen)
{
	int result;
	struct archive *ar;
	struct archive_entry *entry;
	long extracted = 0;
	char fpath[MAX_PATH + 1];

	if(error == NULL)
		maxerrlen = 0;
	if(maxerrlen >= 1)
		*error = '\0';
	if((ar = archive_read_new()) == NULL) {
		safe_snprintf(error, maxerrlen, "archive_read_new returned NULL");
		return -1;
	}
	archive_read_support_filter_all(ar);
	archive_read_support_format_all(ar);
	if((result = archive_read_open_filename(ar, archive, 10240)) != ARCHIVE_OK) {
		safe_snprintf(error, maxerrlen, "archive_read_open_filename returned %d: %s"
			,result, archive_error_string(ar));
		archive_read_free(ar);
		return result >= 0 ? -1 : result;
	}
	while(1) {
		result = archive_read_next_header(ar, &entry);
		if(result != ARCHIVE_OK) {
			if(result != ARCHIVE_EOF)
				safe_snprintf(error, maxerrlen, "archive_read_next_header returned %d: %s"
					,result, archive_error_string(ar));
			break;
		}
		const char* pathname = archive_entry_pathname(entry);
		if(pathname == NULL)
			continue;
		int filetype = archive_entry_filetype(entry);
		if(filetype == AE_IFDIR) {
			if(!with_path)
				continue;
			if(strstr(pathname, "..") != NULL) {
				safe_snprintf(error, maxerrlen, "Illegal double-dots in path '%s'", pathname);
				break;
			}
			SAFECOPY(fpath, outdir);
			backslash(fpath);
			SAFECAT(fpath, pathname);
			if(mkpath(fpath) != 0) {
				char err[256];
				safe_snprintf(error, maxerrlen, "%d (%s) creating path '%s'", errno, safe_strerror(errno, err, sizeof(err)), fpath);
				break;
			}
			continue;
		}
		if(filetype != AE_IFREG)
			continue;
		char* filename = getfname(pathname);
		if(!with_path)
			pathname = filename;
		if(file_list != NULL) {
			int i;
			for (i = 0; file_list[i] != NULL; i++)
				if(wildmatch(pathname, file_list[i], with_path, /* case-sensitive: */false))
					break;
			if(file_list[i] == NULL)
				continue;
		}
		if(allowed_filename_chars != NULL
			&& *allowed_filename_chars != '\0'
			&& strspn(filename, allowed_filename_chars) != strlen(filename)) {
			safe_snprintf(error, maxerrlen, "disallowed filename '%s'", pathname);
			break;
		}
		SAFECOPY(fpath, outdir);
		backslash(fpath);
		SAFECAT(fpath, pathname);
		FILE* fp = fopen(fpath, "wb");
		if(fp == NULL) {
			char err[256];
			safe_snprintf(error, maxerrlen, "%d (%s) opening/creating '%s'", errno, safe_strerror(errno, err, sizeof(err)), fpath);
			break;
		}

		const void *buff;
		size_t size;
		int64_t offset;

		for(;;) {
			result = archive_read_data_block(ar, &buff, &size, &offset);
			if(result == ARCHIVE_EOF) {
				extracted++;
				break;
			}
			if(result < ARCHIVE_OK) {
				safe_snprintf(error, maxerrlen, "archive_read_data_block returned %d: %s"
					,result, archive_error_string(ar));
				break;
			}
			if(fwrite(buff, 1, size, fp) != size)
				break;
		}
		fclose(fp);
		if(result != ARCHIVE_EOF)
			(void)remove(fpath);
		if(max_files && extracted >= max_files) {
			safe_snprintf(error, maxerrlen, "maximum number of files (%lu) extracted", max_files);
			break;
		}
	}
	archive_read_free(ar);
	return extracted;
}

bool extract_diz(scfg_t* cfg, file_t* f, str_list_t diz_fnames, char* path, size_t maxlen)
{
	int i;
	char archive[MAX_PATH + 1];
	char* default_diz_fnames[] = { "FILE_ID.ANS", "FILE_ID.DIZ", "DESC.SDI", NULL };

	getfilepath(cfg, f, archive);
	if(diz_fnames == NULL)
		diz_fnames = default_diz_fnames;

	if(!fexistcase(archive))
		return false;

	for(i = 0; diz_fnames[i] != NULL; i++) {
		safe_snprintf(path, maxlen, "%s%s", cfg->temp_dir, diz_fnames[i]); // no slash
		removecase(path);
		if(fexistcase(path))	// failed to delete?
			return false;
	}

	if(extract_files_from_archive(archive
		,/* outdir: */cfg->temp_dir
		,/* allowed_filename_chars: */NULL /* any */
		,/* with_path: */false
		,/* max_files: */strListCount(diz_fnames)
		,/* file_list: */diz_fnames
		,/* error: */NULL, 0) >= 0) {
		for(i = 0; diz_fnames[i] != NULL; i++) {
			safe_snprintf(path, maxlen, "%s%s", cfg->temp_dir, diz_fnames[i]); // no slash
			if(fexistcase(path))
				return true;
		}
		return false;
	}

	char* fext = getfext(f->name);

	if(fext == NULL)
		return false;

	for(i = 0; i < cfg->total_fextrs; i++)
		if(stricmp(cfg->fextr[i]->ext, fext + 1) == 0 && chk_ar(cfg, cfg->fextr[i]->ar, /* user: */NULL, /* client: */NULL))
			break;
	if(i >= cfg->total_fextrs)
		return false;

	fextr_t* fextr = cfg->fextr[i];
	char cmd[512];
	for(i = 0; diz_fnames[i] != NULL; i++) {
		safe_snprintf(path, maxlen, "%s%s", cfg->temp_dir, diz_fnames[i]);
		system(cmdstr(cfg, /* user: */NULL, fextr->cmd, archive, diz_fnames[i], cmd, sizeof(cmd)));
		if(fexistcase(path))
			return true;
	}
	return false;
}

char* read_diz(const char* path, struct sauce_charinfo* sauce)
{
	if(sauce != NULL)
		memset(sauce, 0, sizeof(*sauce));

	off_t len = flength(path);
	if(len < 1)
		return NULL;

	FILE* fp = fopen(path, "rb");
	if(fp == NULL)
		return NULL;

	if(sauce != NULL)
		sauce_fread_charinfo(fp, /* type: */NULL, sauce);

	if(len > LEN_EXTDESC)
		len = LEN_EXTDESC;

	char* buf = calloc((size_t)len + 1, 1);
	if(buf != NULL) {
		size_t rd = fread(buf, 1, (size_t)len, fp);
		buf[rd] = 0;
		char* eof = strchr(buf, CTRL_Z);	// CP/M EOF
		if(eof != NULL)
			*eof = '\0';
	}
	fclose(fp);

	return buf;
}

char* format_diz(const char* src, char* dest, size_t maxlen, int width, bool ice)
{
	if(src == NULL) {
		*dest = '\0';
		return dest;
	}
	convert_ansi(src, dest, maxlen - 1, width, ice);
	return dest;
}

// Take a verbose extended description (e.g. FILE_ID.DIZ)
// and convert to suitable short description
char* prep_file_desc(const char* ext, char* dest)
{
	int out;
	char* src;
	char* buf = strdup(ext);
	if(buf == NULL)
		src = (char*)ext;
	else {
		src = buf;
		strip_ctrl(src, src);
	}

	FIND_ALPHANUMERIC(src);
	for(out = 0; *src != '\0' && out < LEN_FDESC; src++) {
		if(IS_WHITESPACE(*src) && out && IS_WHITESPACE(dest[out - 1]))
			continue;
		if(!IS_ALPHANUMERIC(*src) && out && *src == dest[out - 1])
			continue;
		if(*src == '\n') {
			if(out && !IS_WHITESPACE(dest[out - 1]))
				dest[out++] = ' ';
			continue;
		}
		if(IS_CONTROL(*src))
			continue;
		dest[out++] = *src;
	}
	dest[out] = '\0';
	free(buf);
	return dest;
}

static const char* quoted_string(const char* str, char* buf, size_t maxlen)
{
	if(strchr(str,' ')==NULL)
		return(str);
	safe_snprintf(buf,maxlen,"\"%s\"",str);
	return(buf);
}

#define QUOTED_STRING(ch, str, buf, maxlen) \
	((IS_ALPHA(ch) && IS_UPPERCASE(ch)) ? str : quoted_string(str,buf,maxlen))

/****************************************************************************/
/* Returns command line generated from instr with %c replacements           */
/* This is the C-exported version of sbbs_t::cmdstr()						*/
/****************************************************************************/
char* cmdstr(scfg_t* cfg, user_t* user, const char* instr, const char* fpath
						,const char* fspec, char* cmd, size_t maxlen)
{
	char	str[MAX_PATH+1];
    int		i,j,len;

	len=strlen(instr);
	for(i=j=0; i < len && j < (int)maxlen; i++) {
        if(instr[i]=='%') {
            i++;
            cmd[j]=0;
			int avail = maxlen - j;
			char ch=instr[i];
			if(IS_ALPHA(ch))
				ch=toupper(ch);
            switch(ch) {
                case 'A':   /* User alias */
					if(user!=NULL)
						strncat(cmd,QUOTED_STRING(instr[i],user->alias,str,sizeof(str)), avail);
                    break;
                case 'B':   /* Baud (DTE) Rate */
                    break;
                case 'C':   /* Connect Description */
					if(user!=NULL)
						strncat(cmd,user->modem, avail);
                    break;
                case 'D':   /* Connect (DCE) Rate */
                    break;
                case 'E':   /* Estimated Rate */
                    break;
                case 'F':   /* File path */
                    strncat(cmd,QUOTED_STRING(instr[i],fpath,str,sizeof(str)), avail);
                    break;
                case 'G':   /* Temp directory */
                    strncat(cmd,cfg->temp_dir, avail);
                    break;
                case 'H':   /* Port Handle or Hardware Flow Control */
                    break;
                case 'I':   /* IP address */
					if(user!=NULL)
						strncat(cmd,user->note, avail);
                    break;
                case 'J':
                    strncat(cmd,cfg->data_dir, avail);
                    break;
                case 'K':
                    strncat(cmd,cfg->ctrl_dir, avail);
                    break;
                case 'L':   /* Lines per message */
					if(user!=NULL)
						strncat(cmd,ultoa(cfg->level_linespermsg[user->level],str,10), avail);
                    break;
                case 'M':   /* Minutes (credits) for user */
					if(user!=NULL)
						strncat(cmd,ultoa(user->min,str,10), avail);
                    break;
                case 'N':   /* Node Directory (same as SBBSNODE environment var) */
                    strncat(cmd,cfg->node_dir, avail);
                    break;
                case 'O':   /* SysOp */
                    strncat(cmd,QUOTED_STRING(instr[i],cfg->sys_op,str,sizeof(str)), avail);
                    break;
                case 'P':   /* Client protocol */
                    break;
                case 'Q':   /* QWK ID */
                    strncat(cmd,cfg->sys_id, avail);
                    break;
                case 'R':   /* Rows */
					if(user!=NULL)
						strncat(cmd,ultoa(user->rows,str,10), avail);
                    break;
                case 'S':   /* File Spec */
                    strncat(cmd, fspec, avail);
                    break;
                case 'T':   /* Time left in seconds */
                    break;
                case 'U':   /* UART I/O Address (in hex) */
                    strncat(cmd,ultoa(cfg->com_base,str,16), avail);
                    break;
                case 'V':   /* Synchronet Version */
                    sprintf(str,"%s%c",VERSION,REVISION);
					strncat(cmd,str, avail);
                    break;
                case 'W':   /* Columns/width */
                    break;
                case 'X':
					if(user!=NULL)
						strncat(cmd,cfg->shell[user->shell]->code, avail);
                    break;
                case '&':   /* Address of msr */
                    break;
                case 'Y':
                    break;
                case 'Z':
                    strncat(cmd,cfg->text_dir, avail);
                    break;
				case '~':	/* DOS-compatible (8.3) filename */
				{
#ifdef _WIN32
					char sfpath[MAX_PATH+1];
					SAFECOPY(sfpath,fpath);
					GetShortPathName(fpath,sfpath,sizeof(sfpath));
					strncat(cmd,sfpath, avail);
#else
                    strncat(cmd,QUOTED_STRING(instr[i],fpath,str,sizeof(str)), avail);
#endif
					break;
				}
                case '!':   /* EXEC Directory */
                    strncat(cmd,cfg->exec_dir, avail);
                    break;
                case '@':   /* EXEC Directory for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
                    strncat(cmd,cfg->exec_dir, avail);
#endif
                    break;

                case '#':   /* Node number (same as SBBSNNUM environment var) */
                    sprintf(str,"%d",cfg->node_num);
                    strncat(cmd,str, avail);
                    break;
                case '*':
                    sprintf(str,"%03d",cfg->node_num);
                    strncat(cmd,str, avail);
                    break;
                case '$':   /* Credits */
					if(user!=NULL)
						strncat(cmd,ultoa(user->cdt+user->freecdt,str,10), avail);
                    break;
                case '%':   /* %% for percent sign */
                    strncat(cmd,"%", avail);
                    break;
				case '.':	/* .exe for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
					strncat(cmd,".exe", avail);
#endif
					break;
				case '?':	/* Platform */
#ifdef __OS2__
					strcpy(str,"OS2");
#else
					strcpy(str,PLATFORM_DESC);
#endif
					strlwr(str);
					strncat(cmd,str, avail);
					break;
				case '^':	/* Architecture */
					strncat(cmd, ARCHITECTURE_DESC, avail);
					break;
                default:    /* unknown specification */
                    if(IS_DIGIT(instr[i]) && user!=NULL) {
                        sprintf(str,"%0*d",instr[i]&0xf,user->number);
                        strncat(cmd,str, avail);
					}
                    break;
			}
            j=strlen(cmd);
		}
        else
            cmd[j++]=instr[i];
	}
    cmd[j]=0;

    return(cmd);
}

/****************************************************************************/
/* Is this filename made up of only the "safest" characters?				*/
/****************************************************************************/
bool safest_filename(const char* fname)
{
	return strspn(fname, SAFEST_FILENAME_CHARS) == strlen(fname);
}

/****************************************************************************/
/* Is this filename highly-suspicious?										*/
/****************************************************************************/
bool illegal_filename(const char *fname)
{
	size_t len = strlen(fname);

	if(len < 1)
		return true;
	if(strcspn(fname, ILLEGAL_FILENAME_CHARS) != len)
		return true;
	if(strstr(fname, "..") != NULL)
		return true;
	if(*fname == '-') // leading dash is a problem for argument parsing
		return true;
	if(*fname == '.') // leading dot hides files on *nix
		return true;
	if(*fname == ' ') // leading space is a problem for argument parsing (shells)
		return true;
	if(fname[len - 1] == '.') // a trailing dot is a problem for Windows
		return true;
	if(fname[len - 1] == ' ') // a trailing space is a problem for argument parsing (shells)
		return true;
	for(size_t i = 0; i < len; i++) {
		if(IS_CONTROL(fname[i])) // control characters in filenames are evil
			return true;
	}

	return false;
}

/****************************************************************************/
/* Checks if the filename chars meet the system requirements for upload		*/
/* Assumes the filename has already been checked with illegal_filename()	*/
/****************************************************************************/
bool allowed_filename(scfg_t* cfg, const char *fname)
{
	size_t len = strlen(fname);
	if(len < 1)
		return false;

	if(cfg->file_misc & FM_SAFEST)
		return safest_filename(fname);

	uchar min = (cfg->file_misc & FM_SPACES) ? ' ' : '!';
	uchar max = (cfg->file_misc & FM_EXASCII) ? 0xff : 0x7f;

	for(size_t i = 0; i < len; i++) {
		if((uchar)fname[i] < min || (uchar)fname[i] > max)
			return false;
	}
	return true;
}
