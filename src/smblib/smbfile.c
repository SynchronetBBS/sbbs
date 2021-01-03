/* Synchronet message base (SMB) FILE stream I/O routines */

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
	return(feof(fp));
}

int smb_ferror(FILE* fp)
{
	return(ferror(fp));
}

int smb_fflush(FILE* fp)
{
	return(fflush(fp));
}

int smb_fgetc(FILE* fp)
{
	return(fgetc(fp));
}

int smb_fputc(int ch, FILE* fp)
{
	return(fputc(ch,fp));
}

int smb_fseek(FILE* fp, long offset, int whence)
{
	return(fseek(fp,offset,whence));
}

long smb_ftell(FILE* fp)
{
	return(ftell(fp));
}

long smb_fgetlength(FILE* fp)
{
	return(filelength(fileno(fp)));
}

int smb_fsetlength(FILE* fp, long length)
{
	return(chsize(fileno(fp),length));
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
	time_t start=0;

	while(1) {
		if((ret=fread(buf,sizeof(char),bytes,fp))==bytes)
			return(ret);
		if(feof(fp) || (get_errno()!=EDEADLOCK && get_errno()!=EACCES))
			return(ret);
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time)
				break;
		SLEEP(smb->retry_delay);
	}
	return(ret);
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif

size_t smb_fwrite(smb_t* smb, const void* buf, size_t bytes, FILE* fp)
{
	return(fwrite(buf,1,bytes,fp));
}

/****************************************************************************/
/* Opens a non-shareable file (FILE*) associated with a message base		*/
/* Retries for retry_time number of seconds									*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int smb_open_fp(smb_t* smb, FILE** fp, int share)
{
	int 	file;
	char	path[MAX_PATH+1];
	char*	ext;
	time_t	start=0;

	if(fp==&smb->shd_fp)
		ext="shd";
	else if(fp==&smb->sid_fp)
		ext="sid";
	else if(fp==&smb->sdt_fp)
		ext="sdt";
	else if(fp==&smb->sda_fp)
		ext="sda";
	else if(fp==&smb->sha_fp)
		ext="sha";
	else if(fp==&smb->hash_fp)
		ext="hash";
	else {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s opening %s: Illegal FILE* pointer argument: %p", __FUNCTION__
			,smb->file, fp);
		return(SMB_ERR_OPEN);
	}

	if(*fp!=NULL)	/* Already open! */
		return(SMB_SUCCESS);

	SAFEPRINTF2(path,"%s.%s",smb->file,ext);

	while(1) {
		if((file=sopen(path,O_RDWR|O_CREAT|O_BINARY,share,DEFFILEMODE))!=-1)
			break;
		if(get_errno()!=EACCES && get_errno()!=EAGAIN) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s %d '%s' opening %s", __FUNCTION__
				,get_errno(),STRERROR(get_errno()),path);
			return(SMB_ERR_OPEN);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"%s timeout opening %s (retry_time=%lu)", __FUNCTION__
					,path, (ulong)smb->retry_time);
				return(SMB_ERR_TIMEOUT); 
			}
		SLEEP(smb->retry_delay);
	}
	if((*fp=fdopen(file,"r+b"))==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s %d '%s' fdopening %s (%d)", __FUNCTION__
			,get_errno(),STRERROR(get_errno()),path,file);
		close(file);
		return(SMB_ERR_OPEN); 
	}
	setvbuf(*fp,NULL,_IOFBF, 2*1024);
	return(SMB_SUCCESS);
}

/****************************************************************************/
/****************************************************************************/
void smb_close_fp(FILE** fp)
{
	if(fp!=NULL) {
		if(*fp!=NULL)
			fclose(*fp);
		*fp=NULL;
	}
}

/****************************************************************************/
/* CASE-INSENSITIVE filename search through index (no wildcards)			*/
/****************************************************************************/
int smb_findfile(smb_t* smb, const char* filename, fileidxrec_t* idx)
{
	rewind(smb->sid_fp);
	while(!feof(smb->sid_fp)) {
		fileidxrec_t fidx;

		if(smb_fread(smb, &fidx, sizeof(fidx), smb->sid_fp) != sizeof(fidx))
			break;

		if(strnicmp(fidx.name, filename, sizeof(fidx.name)) == 0) {
			if(idx != NULL)
				*idx = fidx;
			return SMB_SUCCESS;
		}
	}
	return SMB_ERR_NOT_FOUND;
}

/****************************************************************************/
/****************************************************************************/
int smb_loadfile(smb_t* smb, const char* filename, smbfile_t* file)
{
	int result;

	if(filename == NULL)
		return SMB_BAD_PARAMETER;

	memset(file, 0, sizeof(*file));

	if((result = smb_findfile(smb, filename, &file->file_idx)) != SMB_SUCCESS)
		return result;

	if((result = smb_getmsghdr(smb, file)) != SMB_SUCCESS)
		return result;

	file->dir = smb->dirnum;

	return SMB_SUCCESS;
}

/****************************************************************************/
/****************************************************************************/
void smb_freefilemem(smbfile_t* file)
{
	smb_freemsgmem(file);
}

/****************************************************************************/
/****************************************************************************/
int smb_removefile(smb_t* smb, smbfile_t* file)
{
	int result;

	if(!smb->locked && smb_locksmbhdr(smb) != SMB_SUCCESS)
		return SMB_ERR_LOCK;

	file->hdr.attr |= MSG_DELETE;
	if((result = smb_putmsghdr(smb, file)) != SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return result;
	}
	if((result = smb_getstatus(smb)) != SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return result;
	}
	if((result = smb_open_ha(smb)) != SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return result;
	}
	if((result = smb_open_da(smb)) != SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return result;
	}
	result = smb_freemsg(smb, file);
	smb_close_ha(smb);
	smb_close_da(smb);

	// Now remove from index:
	if(result == SMB_SUCCESS) {
		rewind(smb->sid_fp);
		fileidxrec_t* fidx = malloc(smb->status.total_files * sizeof(*fidx));
		if(fidx == NULL) {
			smb_unlocksmbhdr(smb);
			return SMB_ERR_MEM;
		}
		if(fread(fidx, sizeof(*fidx), smb->status.total_files, smb->sid_fp) != smb->status.total_files) {
			free(fidx);
			smb_unlocksmbhdr(smb);
			return SMB_ERR_READ;
		}
		rewind(smb->sid_fp);
		for(uint32_t i = 0; i < smb->status.total_files; i++) {
			if(stricmp(fidx[i].name, file->name) == 0)
				continue;
			if(fwrite(fidx + i, sizeof(*fidx), 1, smb->sid_fp) != 1) {
				result = SMB_ERR_WRITE;
				break;
			}
		}
		free(fidx);
		if(result == SMB_SUCCESS) {
			fflush(smb->sid_fp);
			--smb->status.total_files;
			if(chsize(fileno(smb->sid_fp), smb->status.total_files * sizeof(*fidx)) != 0)
				result = SMB_ERR_DELETE;
			else
				result = smb_putstatus(smb);
		}
	}

	smb_unlocksmbhdr(smb);
	return result;
}
