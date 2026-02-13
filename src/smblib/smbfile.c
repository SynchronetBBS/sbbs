/* Synchronet message base (SMB) FILE stream and FileBase routines  */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "smblib.h"

int smb_feof(FILE* fp)
{
	return feof(fp);
}

int smb_ferror(FILE* fp)
{
	return ferror(fp);
}

int smb_fflush(FILE* fp)
{
	return fflush(fp);
}

int smb_fgetc(FILE* fp)
{
	return fgetc(fp);
}

int smb_fputc(int ch, FILE* fp)
{
	return fputc(ch, fp);
}

int smb_fseek(FILE* fp, off_t offset, int whence)
{
	return fseeko(fp, offset, whence);
}

off_t smb_ftell(FILE* fp)
{
	return ftello(fp);
}

off_t smb_fgetlength(FILE* fp)
{
	return filelength(fileno(fp));
}

int smb_fsetlength(FILE* fp, off_t length)
{
	return chsize(fileno(fp), length);
}

void smb_rewind(FILE* fp)
{
	rewind(fp);
}

void smb_clearerr(FILE* fp)
{
	clearerr(fp);
}

size_t smb_fread(smb_t* smb, void* buf, size_t bytes, FILE* fp)
{
	size_t ret;
	time_t start = 0;
	int    count = 0;

	while (1) {
		if ((ret = fread(buf, sizeof(char), bytes, fp)) == bytes)
			return ret;
		if (feof(fp) || !FILE_RETRY_ERRNO(get_errno()))
			return ret;
		if (!start)
			start = time(NULL);
		else
		if (time(NULL) - start >= (time_t)smb->retry_time)
			break;
		++count;
		FILE_RETRY_DELAY(count, smb->retry_delay);
	}
	return ret;
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif

size_t smb_fwrite(smb_t* smb, const void* buf, size_t bytes, FILE* fp)
{
	return fwrite(buf, 1, bytes, fp);
}

/****************************************************************************/
/* Opens a non-shareable file (FILE*) associated with a message base		*/
/* Retries for retry_time number of seconds									*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int smb_open_fp(smb_t* smb, FILE** fp, int share)
{
	int    file;
	char   path[MAX_PATH + 1];
	char*  ext;
	time_t start = 0;
	int    count = 0;

	if (fp == &smb->shd_fp)
		ext = "shd";
	else if (fp == &smb->sid_fp)
		ext = "sid";
	else if (fp == &smb->sdt_fp)
		ext = "sdt";
	else if (fp == &smb->sda_fp)
		ext = "sda";
	else if (fp == &smb->sha_fp)
		ext = "sha";
	else if (fp == &smb->hash_fp)
		ext = "hash";
	else {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s opening %s: Illegal FILE* pointer argument: %p", __FUNCTION__
		              , smb->file, fp);
		return SMB_ERR_OPEN;
	}

	if (*fp != NULL)   /* Already open! */
		return SMB_SUCCESS;

	SAFEPRINTF2(path, "%s.%s", smb->file, ext);

	while (1) {
		if ((file = sopen(path, O_RDWR | O_CREAT | O_BINARY, share, DEFFILEMODE)) != -1)
			break;
		if (!FILE_RETRY_ERRNO(get_errno())) {
			safe_snprintf(smb->last_error, sizeof(smb->last_error)
			              , "%s %d '%s' opening %s", __FUNCTION__
			              , get_errno(), strerror(get_errno()), path);
			return SMB_ERR_OPEN;
		}
		if (!start)
			start = time(NULL);
		else
		if (time(NULL) - start >= (time_t)smb->retry_time) {
			safe_snprintf(smb->last_error, sizeof(smb->last_error)
			              , "%s timeout opening %s (errno=%d, retry_time=%lu)", __FUNCTION__
			              , path, get_errno(), (ulong)smb->retry_time);
			return SMB_ERR_TIMEOUT;
		}
		++count;
		FILE_RETRY_DELAY(count, smb->retry_delay);
	}
	if ((*fp = fdopen(file, "r+b")) == NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
		              , "%s %d '%s' fdopening %s (%d)", __FUNCTION__
		              , get_errno(), strerror(get_errno()), path, file);
		close(file);
		return SMB_ERR_OPEN;
	}
	setvbuf(*fp, NULL, _IOFBF, 64 * 1024);
	return SMB_SUCCESS;
}

/****************************************************************************/
/****************************************************************************/
void smb_close_fp(FILE** fp)
{
	if (fp != NULL) {
		if (*fp != NULL)
			fclose(*fp);
		*fp = NULL;
	}
}

/****************************************************************************/
/* maxlen includes the NUL terminator										*/
/****************************************************************************/
char* smb_fileidxname(const char* filename, char* buf, size_t maxlen)
{
	size_t fnlen = strlen(filename);
	char*  ext = getfext(filename);
	if (ext != NULL) {
		size_t extlen = strlen(ext);
		if (extlen >= maxlen - 1) {
			strncpy(buf, filename, maxlen);
			buf[maxlen - 1] = '\0';
		}
		else {
			fnlen -= extlen;
			if (fnlen > (maxlen - 1) - extlen)
				fnlen = (maxlen - 1) - extlen;
			safe_snprintf(buf, maxlen, "%-.*s%s", (int)fnlen, filename, ext);
		}
	} else {    /* no extension */
		strncpy(buf, filename, maxlen);
		buf[maxlen - 1] = '\0';
	}
	return buf;
}

/****************************************************************************/
/* Find file in index via either/or:										*/
/* -  CASE-INSENSITIVE 'filename' search through index (no wildcards)		*/
/* -  file content size and hash details found in 'file'					*/
/****************************************************************************/
int smb_findfile(smb_t* smb, const char* filename, smbfile_t* file)
{
	long       offset = 0;
	smbfile_t* f = file;
	smbfile_t  file_ = {0};
	if (f == NULL)
		f = &file_;
	uint64_t   fsize;
	char       fname[SMB_FILEIDX_NAMELEN + 1] = "";
	if (filename != NULL)
		smb_fileidxname(filename, fname, sizeof(fname));

	if (smb->sid_fp == NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s msgbase not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}
	f->dir = smb->dirnum;
	rewind(smb->sid_fp);
	while (!feof(smb->sid_fp)) {
		fileidxrec_t fidx;

		if (smb_fread(smb, &fidx, sizeof(fidx), smb->sid_fp) != sizeof(fidx))
			break;
		TERMINATE(fidx.name);

		f->idx_offset = offset++;

		if (filename != NULL) {
			if (stricmp(fidx.name, fname) != 0)
				continue;
			f->file_idx = fidx;
			return SMB_SUCCESS;
		}

		if (file == NULL)
			continue;

		fsize = smb_getfilesize(&f->idx);
		if ((f->file_idx.hash.flags & SMB_HASH_MASK) != 0 || fsize > 0) {
			if (fsize > 0 && fsize != smb_getfilesize(&fidx.idx))
				continue;
			if ((f->file_idx.hash.flags & SMB_HASH_CRC16) && f->file_idx.hash.data.crc16 != fidx.hash.data.crc16)
				continue;
			if ((f->file_idx.hash.flags & SMB_HASH_CRC32) && f->file_idx.hash.data.crc32 != fidx.hash.data.crc32)
				continue;
			if ((f->file_idx.hash.flags & SMB_HASH_MD5)
			    && memcmp(f->file_idx.hash.data.md5, fidx.hash.data.md5, sizeof(fidx.hash.data.md5)) != 0)
				continue;
			if ((f->file_idx.hash.flags & SMB_HASH_SHA1)
			    && memcmp(f->file_idx.hash.data.sha1, fidx.hash.data.sha1, sizeof(fidx.hash.data.sha1)) != 0)
				continue;
			f->file_idx = fidx;
			return SMB_SUCCESS;
		}
	}
	return SMB_ERR_NOT_FOUND;
}

/****************************************************************************/
/****************************************************************************/
int smb_loadfile(smb_t* smb, const char* filename, smbfile_t* file, enum file_detail detail)
{
	int result;

	memset(file, 0, sizeof(*file));

	if ((result = smb_findfile(smb, filename, file)) != SMB_SUCCESS)
		return result;

	return smb_getfile(smb, file, detail);
}

/****************************************************************************/
/****************************************************************************/
int smb_getfile(smb_t* smb, smbfile_t* file, enum file_detail detail)
{
	int result;

	file->name = file->file_idx.name;
	file->hdr.when_written.time = file->idx.time;
	if (detail > file_detail_index) {
		if ((result = smb_getmsghdr(smb, file)) != SMB_SUCCESS)
			return result;
		if (detail >= file_detail_extdesc)
			file->extdesc = smb_getmsgtxt(smb, file, GETMSGTXT_BODY_ONLY);
		if (detail >= file_detail_auxdata)
			file->auxdata = smb_getmsgtxt(smb, file, GETMSGTXT_TAIL_ONLY);
	}
	file->dir = smb->dirnum;

	return SMB_SUCCESS;
}

/****************************************************************************/
/* Writes both header and index information for file 'file'                 */
/* Like smb_putmsg() but doesn't (re)-initialize index (idx)				*/
/****************************************************************************/
int smb_putfile(smb_t* smb, smbfile_t* file)
{
	int result;

	if ((result = smb_putmsghdr(smb, file)) != SMB_SUCCESS)
		return result;

	return smb_putmsgidx(smb, file);
}

/****************************************************************************/
/****************************************************************************/
void smb_freefilemem(smbfile_t* file)
{
	smb_freemsgmem(file);
}

/****************************************************************************/
/****************************************************************************/
int smb_addfile(smb_t* smb, smbfile_t* file, int storage, const char* extdesc, const char* auxdata, const char* path)
{
	if (file->name == NULL || *file->name == '\0') {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s missing name", __FUNCTION__);
		return SMB_ERR_HDR_FIELD;
	}
	if (smb_findfile(smb, file->name, NULL) == SMB_SUCCESS) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s duplicate name found: %s", __FUNCTION__, file->name);
		return SMB_DUPE_MSG;
	}
	if (path != NULL) {
		file->size = flength(path);
		file->hdr.when_written.time = (uint32_t)fdate(path);
		if (!(smb->status.attr & SMB_NOHASH) && file->file_idx.hash.flags == 0)
			file->file_idx.hash.flags = smb_hashfile(path, file->size, &file->file_idx.hash.data);
	}
	file->hdr.attr |= MSG_FILE;
	file->hdr.type = SMB_MSG_TYPE_FILE;
	return smb_addmsg(smb, file, storage, SMB_HASH_SOURCE_NONE, XLAT_NONE
	                  , /* body: */ (const uchar*)extdesc, /* tail: */ (const uchar*)auxdata);
}

/****************************************************************************/
/* Like smb_addfile(), except 'auxdata' is a str_list_t: 'list'			*/
/****************************************************************************/
int smb_addfile_withlist(smb_t* smb, smbfile_t* file, int storage, const char* extdesc, str_list_t list, const char* path)
{
	char* auxdata = NULL;
	int   result;

	if (list != NULL && *list != NULL) {
		size_t size = strListCount(list) * 1024;
		if (size > 0) {
			auxdata = calloc(1, size);
			if (auxdata == NULL)
				return SMB_ERR_MEM;
			strListCombine(list, auxdata, size - 1, "\r\n");
		}
	}
	result = smb_addfile(smb, file, storage, extdesc, auxdata, path);
	free(auxdata);
	return result;
}

/****************************************************************************/
/****************************************************************************/
int smb_renewfile(smb_t* smb, smbfile_t* file, int storage, const char* path)
{
	int result;
	if ((result = smb_removefile(smb, file)) != SMB_SUCCESS)
		return result;
	file->hdr.when_imported.time = 0; // Reset the import date/time
	return smb_addfile(smb, file, storage, file->extdesc, file->auxdata, path);
}

/****************************************************************************/
/****************************************************************************/
int smb_removefile(smb_t* smb, smbfile_t* file)
{
	int  result;
	int  removed = 0;
	char fname[SMB_FILEIDX_NAMELEN + 1] = "";

	if (file->total_hfields < 1) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s header has %u fields"
		              , __FUNCTION__, file->total_hfields);
		return SMB_ERR_HDR_FIELD;
	}

	if (!smb->smbhdr_locked && smb_locksmbhdr(smb) != SMB_SUCCESS)
		return SMB_ERR_LOCK;

	file->hdr.attr |= MSG_DELETE;
	if ((result = smb_putmsghdr(smb, file)) != SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return result;
	}
	if ((result = smb_getstatus(smb)) != SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return result;
	}
	if ((result = smb_open_ha(smb)) != SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return result;
	}
	if ((result = smb_open_da(smb)) != SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return result;
	}
	result = smb_freemsg(smb, file);
	smb_close_ha(smb);
	smb_close_da(smb);

	// Now remove from index:
	smb_fileidxname(file->name, fname, sizeof(fname));
	if (result == SMB_SUCCESS) {
		rewind(smb->sid_fp);
		fileidxrec_t* fidx = malloc(smb->status.total_files * sizeof(*fidx));
		if (fidx == NULL) {
			smb_unlocksmbhdr(smb);
			return SMB_ERR_MEM;
		}
		if (fread(fidx, sizeof(*fidx), smb->status.total_files, smb->sid_fp) != smb->status.total_files) {
			free(fidx);
			smb_unlocksmbhdr(smb);
			return SMB_ERR_READ;
		}
		rewind(smb->sid_fp);
		for (uint32_t i = 0; i < smb->status.total_files; i++) {
			if (strnicmp(fidx[i].name, fname, sizeof(fname) - 1) == 0) {
				removed++;
				continue;
			}
			if (fwrite(fidx + i, sizeof(*fidx), 1, smb->sid_fp) != 1) {
				safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s re-writing index"
				              , __FUNCTION__);
				result = SMB_ERR_WRITE;
				break;
			}
		}
		free(fidx);
		if (result == SMB_SUCCESS) {
			if (removed < 1) {
				safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s name not found: %s"
				              , __FUNCTION__, fname);
				result = SMB_ERR_NOT_FOUND;
			} else {
				fflush(smb->sid_fp);
				smb->status.total_files -= removed;
				if (chsize(fileno(smb->sid_fp), smb->status.total_files * sizeof(*fidx)) != 0) {
					safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s error %d truncating index"
					              , __FUNCTION__, errno);
					result = SMB_ERR_DELETE;
				} else
					result = smb_putstatus(smb);
			}
		}
	}

	smb_unlocksmbhdr(smb);
	return result;
}

/****************************************************************************/
/****************************************************************************/
int smb_removefile_by_name(smb_t* smb, const char* filename)
{
	int       result;
	smbfile_t file;
	if ((result = smb_loadfile(smb, filename, &file, file_detail_normal)) != SMB_SUCCESS)
		return result;
	result = smb_removefile(smb, &file);
	smb_freefilemem(&file);
	return result;
}

uint64_t smb_getfilesize(idxrec_t* idx)
{
	return ((uint64_t)idx->size) | (((uint64_t)idx->size_ext) << 32);
}

int smb_setfilesize(idxrec_t* idx, uint64_t size)
{
	if (size > 0xffffffffffffULL)
		return SMB_ERR_FILE_LEN;

	idx->size = (uint32_t)size;
	idx->size_ext = (uint16_t)(size >> 32);

	return SMB_SUCCESS;
}
