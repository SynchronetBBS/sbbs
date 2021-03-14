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
#include "archive.h"
#include "archive_entry.h"

 /****************************************************************************/
/****************************************************************************/
bool findfile(scfg_t* cfg, uint dirnum, const char *filename, smbfile_t* file)
{
	smb_t smb;

	if(cfg == NULL || filename == NULL)
		return false;

	if(smb_open_dir(cfg, &smb, dirnum) != SMB_SUCCESS)
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
str_list_t loadfilenames(scfg_t* cfg, smb_t* smb, const char* filespec, time_t t, bool sort, size_t* count)
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

	char** file_list = calloc(smb->status.total_files + 1, sizeof(char*));
	if(file_list == NULL)
		return NULL;

	fseek(smb->sid_fp, start * sizeof(fileidxrec_t), SEEK_SET);
	while(!feof(smb->sid_fp)) {
		fileidxrec_t fidx;

		if(smb_fread(smb, &fidx, sizeof(fidx), smb->sid_fp) != sizeof(fidx))
			break;

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
	if(sort)
		sortfilenames(file_list, *count, (enum file_sort)cfg->dir[smb->dirnum]->sort);

	return file_list;
}

// Load and optionally-sort files from an open filebase into a dynamically-allocated list of "objects"
smbfile_t* loadfiles(scfg_t* cfg, smb_t* smb, const char* filespec, time_t t, enum file_detail detail, bool sort, size_t* count)
{
	*count = 0;

	long start = 0;	
	if(t) {
		idxrec_t idx;
		start = smb_getmsgidx_by_time(smb, &idx, t);
		if(start < 0)
			return NULL;
	}

	smbfile_t* file_list = calloc(smb->status.total_files, sizeof(smbfile_t));
	if(file_list == NULL)
		return NULL;

	fseek(smb->sid_fp, start * sizeof(fileidxrec_t), SEEK_SET);
	long offset = start;
	while(!feof(smb->sid_fp)) {
		smbfile_t* f = &file_list[*count];

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
		int result = smb_getfile(smb, f, detail);
		if(result != SMB_SUCCESS)
			break;
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

bool loadfile(scfg_t* cfg, uint dirnum, const char* filename, smbfile_t* file, enum file_detail detail)
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

bool batch_file_add(scfg_t* cfg, uint usernumber, enum XFER_TYPE type, smbfile_t* f)
{
	FILE* fp = batch_list_open(cfg, usernumber, type, /* create: */true);
	if(fp == NULL)
		return false;
	fseek(fp, 0, SEEK_END);
	fprintf(fp, "\n[%s]\n", f->name);
	if(f->dir >= 0 && f->dir < cfg->total_dirs)
		fprintf(fp, "dir=%s\n", cfg->dir[f->dir]->code);
	fprintf(fp, "desc=%s\n", f->desc);
	fprintf(fp, "altpath=%u\n", f->hdr.altpath);
	fclose(fp);
	return true;
}

bool batch_file_get(scfg_t* cfg, str_list_t ini, const char* filename, smbfile_t* f)
{
	char value[INI_MAX_VALUE_LEN + 1];

	if(!iniSectionExists(ini, filename))
		return false;
	f->dir = batch_file_dir(cfg, ini, filename);
	if(f->dir < 0 || f->dir >= cfg->total_dirs)
		return false;
	smb_hfield_str(f, SMB_FILENAME, filename);
	smb_hfield_str(f, SMB_FILEDESC, iniGetString(ini, filename, "desc", NULL, value));
	f->hdr.altpath = iniGetShortInt(ini, filename, "altpath", 0);
	return true;
}

int batch_file_dir(scfg_t* cfg, str_list_t ini, const char* filename)
{
	char value[INI_MAX_VALUE_LEN + 1];
	return getdirnum(cfg, iniGetString(ini, filename, "dir", NULL, value));
}

bool batch_file_load(scfg_t* cfg, str_list_t ini, const char* filename, smbfile_t* f)
{
	if(!iniSectionExists(ini, filename))
		return false;
	f->dir = batch_file_dir(cfg, ini, filename);
	if(f->dir < 0)
		return false;
	return loadfile(cfg, f->dir, filename, f, file_detail_normal);
}

bool updatefile(scfg_t* cfg, smbfile_t* file)
{
	smb_t smb;

	if(smb_open_dir(cfg, &smb, file->dir) != SMB_SUCCESS)
		return false;

	int result = smb_updatemsg(&smb, file) == SMB_SUCCESS;
	smb_close(&smb);
	return result == SMB_SUCCESS;
}

bool removefile(scfg_t* cfg, uint dirnum, const char* filename)
{
	smb_t smb;

	if(smb_open_dir(cfg, &smb, dirnum) != SMB_SUCCESS)
		return false;

	int result;
	smbfile_t file;
	if((result = smb_loadfile(&smb, filename, &file, file_detail_normal)) == SMB_SUCCESS) {
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
	const char* name = f->name == NULL ? f->file_idx.name : f->name;
	if(f->dir >= cfg->total_dirs)
		safe_snprintf(path, MAX_PATH, "%s%s", cfg->temp_dir, name);
	else {
		safe_snprintf(path, MAX_PATH, "%s%s", f->hdr.altpath > 0 && f->hdr.altpath <= cfg->altpaths 
			? cfg->altpath[f->hdr.altpath-1] : cfg->dir[f->dir]->path
			,name);
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
	return (ulong)(f->size / rate_cps);
}

bool hashfile(scfg_t* cfg, smbfile_t* f)
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

bool addfile(scfg_t* cfg, uint dirnum, smbfile_t* f, const char* extdesc)
{
	char fpath[MAX_PATH + 1];
	smb_t smb;

	if(smb_open_dir(cfg, &smb, dirnum) != SMB_SUCCESS)
		return false;

	getfilepath(cfg, f, fpath);
	int result = smb_addfile(&smb, f, SMB_SELFPACK, extdesc, fpath);
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

ulong extract_files_from_archive(const char* archive, const char** file_list, const char* outdir, ulong max_files)
{
	struct archive *ar;
	struct archive_entry *entry;
	ulong extracted = 0;

	if((ar = archive_read_new()) == NULL)
		return 0;
	archive_read_support_filter_all(ar);
	archive_read_support_format_all(ar);
	if(archive_read_open_filename(ar, archive, 10240) != ARCHIVE_OK)
		return 0;
	while(archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
		const char* fname = archive_entry_pathname(entry);
		if(fname == NULL)
			continue;
		fname = getfname(fname);
		if(file_list != NULL) {
			int i;
			for (i = 0; file_list[i] != NULL; i++)
				if(wildmatch(fname, file_list[i], /* path: */false, /* case-sensitive: */false))
					break;
			if(file_list[i] == NULL)
				continue;
		}
		char fpath[MAX_PATH + 1];
		SAFEPRINTF2(fpath, "%s%s", outdir, fname);
		FILE* fp = fopen(fpath, "wb");
		if(fp == NULL)
			continue;

		const void *buff;
		size_t size;
		la_int64_t offset;

		int result;
		for(;;) {
			result = archive_read_data_block(ar, &buff, &size, &offset);
			if(result == ARCHIVE_EOF) {
				extracted++;
				break;
			}
			if(result < ARCHIVE_OK) {
//				const char* p = archive_error_string(ar);
				break;
			}
			if(fwrite(buff, 1, size, fp) != size)
				break;
		}
		fclose(fp);
		if(result != ARCHIVE_EOF)
			(void)remove(fpath);
		if(max_files && extracted >= max_files)
			break;
	}
	archive_read_free(ar);
	return extracted;
}

bool extract_diz(scfg_t* cfg, smbfile_t* f, str_list_t diz_fnames, char* path, size_t maxlen)
{
	int i;
	char archive[MAX_PATH + 1];
	char* default_diz_fnames[] = { "FILE_ID.DIZ", "DESC.SDI", NULL };

	getfilepath(cfg, f, archive);
	if(diz_fnames == NULL)
		diz_fnames = default_diz_fnames;

	if(!fexistcase(archive))
		return false;

	if(extract_files_from_archive(archive, diz_fnames, cfg->temp_dir, /* max_files: */1) > 0) {
		for(i = 0; diz_fnames[i] != NULL; i++) {
			safe_snprintf(path, maxlen, "%s%s", cfg->temp_dir, diz_fnames[i]);
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
		removecase(path);
		if(fexistcase(path))	// failed to delete?
			return false;
		system(cmdstr(cfg, /* user: */NULL, fextr->cmd, archive, diz_fnames[i], cmd, sizeof(cmd)));
		if(fexistcase(path))
			return true;
	}
	return false;
}

str_list_t read_diz(const char* path, size_t max_line_len)
{
	FILE* fp = fopen(path, "r");
	if(fp == NULL)
		return NULL;

	str_list_t lines = strListReadFile(fp, NULL, max_line_len);
	fclose(fp);
	return lines;
}

char* format_diz(str_list_t lines, char* str, size_t maxlen, bool allow_ansi)
{
	strListTruncateTrailingWhitespaces(lines);
	if(!allow_ansi) {
		for(size_t i = 0; lines[i] != NULL; i++) {
			strip_ansi(lines[i]);
			strip_ctrl(lines[i], lines[i]);
		}
	}
	return strListCombine(lines, str, maxlen, "\r\n");
}

// Take a verbose extended description (e.g. FILE_ID.DIZ)
// and convert to suitable short description
char* prep_file_desc(const char *src, char* dest)
{
	int out;

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

