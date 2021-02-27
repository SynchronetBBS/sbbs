/* Synchronet message base (SMB) FILE stream I/O routines */

/* $Id: smbfile.c,v 1.17 2020/04/14 07:08:50 rswindell Exp $ */

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
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "smblib.h"

int SMBCALL smb_feof(FILE* fp)
{
	return(feof(fp));
}

int SMBCALL smb_ferror(FILE* fp)
{
	return(ferror(fp));
}

int SMBCALL smb_fflush(FILE* fp)
{
	return(fflush(fp));
}

int SMBCALL smb_fgetc(FILE* fp)
{
	return(fgetc(fp));
}

int SMBCALL smb_fputc(int ch, FILE* fp)
{
	return(fputc(ch,fp));
}

int SMBCALL smb_fseek(FILE* fp, long offset, int whence)
{
	return(fseek(fp,offset,whence));
}

long SMBCALL smb_ftell(FILE* fp)
{
	return(ftell(fp));
}

long SMBCALL smb_fgetlength(FILE* fp)
{
	return(filelength(fileno(fp)));
}

int SMBCALL smb_fsetlength(FILE* fp, long length)
{
	return(chsize(fileno(fp),length));
}

void SMBCALL smb_rewind(FILE* fp)
{
	rewind(fp);
}

void SMBCALL smb_clearerr(FILE* fp)
{
	clearerr(fp);
}

size_t SMBCALL smb_fread(smb_t* smb, void* buf, size_t bytes, FILE* fp)
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

size_t SMBCALL smb_fwrite(smb_t* smb, const void* buf, size_t bytes, FILE* fp)
{
	return(fwrite(buf,1,bytes,fp));
}

/****************************************************************************/
/* Opens a non-shareable file (FILE*) associated with a message base		*/
/* Retries for retry_time number of seconds									*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int SMBCALL smb_open_fp(smb_t* smb, FILE** fp, int share)
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
void SMBCALL smb_close_fp(FILE** fp)
{
	if(fp!=NULL) {
		if(*fp!=NULL)
			fclose(*fp);
		*fp=NULL;
	}
}
