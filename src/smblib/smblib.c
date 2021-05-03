/* Synchronet message base (SMB) library routines */

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

/* ANSI C Library headers */

#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>		/* malloc */
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>	/* must come after sys/types.h */

/* SMB-specific headers */
#include "smblib.h"
#include "genwrap.h"
#include "filewrap.h"

/* Use smb_ver() and smb_lib_ver() to obtain these values */
#define SMBLIB_VERSION		"3.00"      /* SMB library version */
#define SMB_VERSION 		0x0300		/* SMB format version */
										/* High byte major, low byte minor */

static char* nulstr="";

int smb_ver(void)
{
	return(SMB_VERSION);
}

char* smb_lib_ver(void)
{
	return(SMBLIB_VERSION);
}

/****************************************************************************/
/* Open a message base of name 'smb->file'                                  */
/* Opens files for READing messages or updating message indices only        */
/****************************************************************************/
int smb_open(smb_t* smb)
{
	int			i;
	time_t		start=0;
	smbhdr_t	hdr;

	/* Set default values, if uninitialized */
	if(!smb->retry_time)
		smb->retry_time=10;		/* seconds */
	if(!smb->retry_delay 
		|| smb->retry_delay>(smb->retry_time*100))	/* at least ten retries */
		smb->retry_delay=250;	/* milliseconds */
	smb->shd_fp=smb->sdt_fp=smb->sid_fp=NULL;
	smb->sha_fp=smb->sda_fp=smb->hash_fp=NULL;
	smb->last_error[0]=0;
	smb->locked = FALSE;

	/* Check for message-base lock semaphore file (under maintenance?) */
	while(smb_islocked(smb)) {
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time)
				return(SMB_ERR_TIMEOUT); 
		SLEEP(smb->retry_delay);
	}

	if((i=smb_open_fp(smb,&smb->shd_fp,SH_DENYNO))!=SMB_SUCCESS)
		return(i);

	memset(&(smb->status),0,sizeof(smb->status));
	if(filelength(fileno(smb->shd_fp))>=(long)sizeof(smbhdr_t)) {
		if(smb_locksmbhdr(smb)!=SMB_SUCCESS) {
			smb_close(smb);
			/* smb_locksmbhdr set last_error */
			return(SMB_ERR_LOCK); 
		}
		memset(&hdr,0,sizeof(smbhdr_t));
		if(smb_fread(smb,&hdr,sizeof(smbhdr_t),smb->shd_fp)!=sizeof(smbhdr_t)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s reading header", __FUNCTION__);
			smb_close(smb);
			return(SMB_ERR_READ);
		}
		if(memcmp(hdr.smbhdr_id,SMB_HEADER_ID,LEN_HEADER_ID) && !smb->continue_on_error) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s corrupt SMB header ID: %02X %02X %02X %02X", __FUNCTION__
				,hdr.smbhdr_id[0]
				,hdr.smbhdr_id[1]
				,hdr.smbhdr_id[2]
				,hdr.smbhdr_id[3]
				);
			smb_close(smb);
			return(SMB_ERR_HDR_ID); 
		}
		if(hdr.version<0x110 && !smb->continue_on_error) {         /* Compatibility check */
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s insufficient header version: %X", __FUNCTION__
				,hdr.version);
			smb_close(smb);
			return(SMB_ERR_HDR_VER); 
		}
		if(smb_fread(smb,&(smb->status),sizeof(smbstatus_t),smb->shd_fp)!=sizeof(smbstatus_t)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s reading status", __FUNCTION__);
			smb_close(smb);
			return(SMB_ERR_READ); 
		}
		if((i=smb_unlocksmbhdr(smb))!=SMB_SUCCESS) {
			smb_close(smb);
			return(i);
		}
	}

	if((i=smb_open_fp(smb,&smb->sdt_fp,SH_DENYNO))!=SMB_SUCCESS)
		return(i);

	if((i=smb_open_index(smb)) != SMB_SUCCESS)
		return i;

	return(SMB_SUCCESS);
}

int smb_open_index(smb_t* smb)
{
	return smb_open_fp(smb, &smb->sid_fp, SH_DENYNO);
}

/****************************************************************************/
/* Closes the currently open message base									*/
/****************************************************************************/
void smb_close(smb_t* smb)
{
	if(smb->shd_fp!=NULL) {
		smb_unlocksmbhdr(smb);		   /* In case it's been locked */
		smb_close_fp(&smb->shd_fp); 
	}
	smb_close_fp(&smb->sdt_fp);
	smb_close_fp(&smb->sid_fp);
	smb_close_fp(&smb->sda_fp);
	smb_close_fp(&smb->sha_fp);
	smb_close_fp(&smb->hash_fp);
}

/****************************************************************************/
/* This set of functions is used to exclusively-lock an entire message base	*/
/* against any other process opening any of the message base files.			*/
/* Currently, this is only used while smbutil packs a message base.			*/
/* This is achieved with a semaphore lock file (e.g. mail.lock).			*/
/****************************************************************************/
static char* smb_lockfname(smb_t* smb, char* fname, size_t maxlen)
{
	safe_snprintf(fname,maxlen,"%s.lock",smb->file);
	return(fname);
}

/****************************************************************************/
/* This function is used to lock an entire message base for exclusive		*/
/* (typically for maintenance/repair)										*/
/****************************************************************************/
int smb_lock(smb_t* smb)
{
	char	path[MAX_PATH+1];
	int		file;
	time_t	start=0;

	smb_lockfname(smb,path,sizeof(path)-1);
	while((file=open(path,O_CREAT|O_EXCL|O_RDWR,S_IREAD|S_IWRITE))==-1) {
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"%s %d '%s' creating %s", __FUNCTION__
					,get_errno(),STRERROR(get_errno()),path);
				return(SMB_ERR_LOCK);
			}
		SLEEP(smb->retry_delay);
	}
	close(file);
	return(SMB_SUCCESS);
}

int smb_unlock(smb_t* smb)
{
	char	path[MAX_PATH+1];

	smb_lockfname(smb,path,sizeof(path)-1);
	if(remove(path)!=0) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s %d '%s' removing %s", __FUNCTION__
			,get_errno(),STRERROR(get_errno()),path);
		return(SMB_ERR_DELETE);
	}
	return(SMB_SUCCESS);
}

BOOL smb_islocked(smb_t* smb)
{
	char	path[MAX_PATH+1];

	if(access(smb_lockfname(smb,path,sizeof(path)-1),0)!=0)
		return(FALSE);
	safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s %s exists", __FUNCTION__,path);
	return(TRUE);
}

/****************************************************************************/
/* Truncates header file													*/
/* Retries for smb.retry_time number of seconds								*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int smb_trunchdr(smb_t* smb)
{
	time_t	start=0;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	rewind(smb->shd_fp);
	while(1) {
		if(chsize(fileno(smb->shd_fp),0L)==0)
			break;
		if(get_errno()!=EACCES && get_errno()!=EAGAIN) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s %d '%s' changing header file size", __FUNCTION__
				,get_errno(),STRERROR(get_errno()));
			return(SMB_ERR_WRITE);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {	 /* Time-out */
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"%s timeout changing header file size (retry_time=%lu)", __FUNCTION__
					,(ulong)smb->retry_time);
				return(SMB_ERR_TIMEOUT); 
			}
		SLEEP(smb->retry_delay);
	}
	return(SMB_SUCCESS);
}

/*********************************/
/* Message Base Header Functions */
/*********************************/

/****************************************************************************/
/* Attempts for smb.retry_time number of seconds to lock the msg base hdr	*/
/****************************************************************************/
int smb_locksmbhdr(smb_t* smb)
{
	time_t	start=0;

	if(smb->locked)
		return SMB_SUCCESS;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	while(1) {
		if(lock(fileno(smb->shd_fp),0L,sizeof(smbhdr_t)+sizeof(smbstatus_t))==0) {
			smb->locked=TRUE;
			return(SMB_SUCCESS);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time)
				break;
		/* In case we've already locked it */
		if(unlock(fileno(smb->shd_fp),0L,sizeof(smbhdr_t)+sizeof(smbstatus_t))==0)
			smb->locked=FALSE;
		else {
			SLEEP(smb->retry_delay);
		}
	}
	safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s timeout locking message base", __FUNCTION__);
	return(SMB_ERR_TIMEOUT);
}

/****************************************************************************/
/* Read the SMB header from the header file and place into smb.status		*/
/****************************************************************************/
int smb_getstatus(smb_t* smb)
{
	int 	i;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	fflush(smb->shd_fp);
	clearerr(smb->shd_fp);
	if(fseek(smb->shd_fp,sizeof(smbhdr_t),SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s %d '%s' seeking to %d in header file", __FUNCTION__
			,get_errno(),STRERROR(get_errno()), (int)sizeof(smbhdr_t));
		return(SMB_ERR_SEEK);
	}
	i=smb_fread(smb,&(smb->status),sizeof(smbstatus_t),smb->shd_fp);
	if(i==sizeof(smbstatus_t))
		return(SMB_SUCCESS);
	safe_snprintf(smb->last_error,sizeof(smb->last_error)
		,"%s reading status", __FUNCTION__);
	return(SMB_ERR_READ);
}

/****************************************************************************/
/* Writes message base header												*/
/****************************************************************************/
int smb_putstatus(smb_t* smb)
{
	int i;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->shd_fp);
	if(fseek(smb->shd_fp,sizeof(smbhdr_t),SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s %d '%s' seeking to %d in header file", __FUNCTION__
			,get_errno(),STRERROR(get_errno()), (int)sizeof(smbhdr_t));
		return(SMB_ERR_SEEK);
	}
	i=fwrite(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp);
	fflush(smb->shd_fp);
	if(i==sizeof(smbstatus_t))
		return(SMB_SUCCESS);
	safe_snprintf(smb->last_error,sizeof(smb->last_error)
		,"%s writing status", __FUNCTION__);
	return(SMB_ERR_WRITE);
}

/****************************************************************************/
/* Unlocks previously locked message base header 							*/
/****************************************************************************/
int smb_unlocksmbhdr(smb_t* smb)
{
	if(smb->locked) {
		if(smb->shd_fp==NULL) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msgbase not open", __FUNCTION__);
			return(SMB_ERR_NOT_OPEN);
		}
		if(unlock(fileno(smb->shd_fp),0L,sizeof(smbhdr_t)+sizeof(smbstatus_t))!=0) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s %d '%s' unlocking message base header", __FUNCTION__,get_errno(),STRERROR(get_errno()));
			return(SMB_ERR_UNLOCK);
		}
		smb->locked=FALSE;
	}
	return(SMB_SUCCESS);
}

/********************************/
/* Individual Message Functions */
/********************************/

/****************************************************************************/
/* Is the offset a valid message header offset?								*/
/****************************************************************************/
BOOL smb_valid_hdr_offset(smb_t* smb, ulong offset)
{
	if(offset<sizeof(smbhdr_t)+sizeof(smbstatus_t) 
		|| offset<smb->status.header_offset) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s invalid header offset: %lu (0x%lX)", __FUNCTION__
			,offset,offset);
		return(FALSE);
	}
	return(TRUE);
}

/****************************************************************************/
/* Attempts for smb.retry_time number of seconds to lock the hdr for 'msg'  */
/****************************************************************************/
int smb_lockmsghdr(smb_t* smb, smbmsg_t* msg)
{
	time_t	start=0;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	if(!smb_valid_hdr_offset(smb,msg->idx.offset))
		return(SMB_ERR_HDR_OFFSET);

	while(1) {
		if(lock(fileno(smb->shd_fp),msg->idx.offset,sizeof(msghdr_t))==0)
			return(SMB_SUCCESS);
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) 
				break;
		/* In case we've already locked it */
		if(unlock(fileno(smb->shd_fp),msg->idx.offset,sizeof(msghdr_t))!=0) {
			SLEEP(smb->retry_delay);
		}
	}
	safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s timeout locking header", __FUNCTION__);
	return(SMB_ERR_TIMEOUT);
}

/****************************************************************************/
/* Fills msg->idx with message index based on msg->hdr.number				*/
/* OR if msg->hdr.number is 0, based on msg->offset (record offset).		*/
/* if msg.hdr.number does not equal 0, then msg->offset is filled too.		*/
/* Either msg->hdr.number or msg->offset must be initialized before 		*/
/* calling this function													*/
/****************************************************************************/
int smb_getmsgidx(smb_t* smb, smbmsg_t* msg)
{
	idxrec_t	idx;
	long		byte_offset;
	ulong		l,total,bot,top;
	off_t		length;
	size_t		idxreclen = smb_idxreclen(smb);

	if(smb->sid_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s index not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sid_fp);

	length=filelength(fileno(smb->sid_fp));
	if(length<(long)idxreclen) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s invalid index file length: %ld", __FUNCTION__,length);
		return(SMB_ERR_FILE_LEN);
	}
	total=(ulong)(length/idxreclen);
	if(!total) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s invalid index file length: %ld", __FUNCTION__,length);
		return(SMB_ERR_FILE_LEN);
	}

	if(!msg->hdr.number) {
		if(msg->idx_offset<0)
			byte_offset=(long)(length-((-msg->idx_offset)*idxreclen));
		else
			byte_offset=msg->idx_offset*idxreclen;
		if(byte_offset>=length) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s invalid index offset: %ld, byte offset: %ld, length: %ld", __FUNCTION__
				,(long)msg->idx_offset, byte_offset, length);
			return(SMB_ERR_HDR_OFFSET);
		}
		if(ftell(smb->sid_fp) != byte_offset) {
			if(fseek(smb->sid_fp,byte_offset,SEEK_SET)) {
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"%s %d '%s' seeking to offset %ld (byte %lu) in index file", __FUNCTION__
					,get_errno(),STRERROR(get_errno())
					,(long)msg->idx_offset,byte_offset);
				return(SMB_ERR_SEEK);
			}
		}
		if(smb_fread(smb,&msg->idx,sizeof(msg->idx),smb->sid_fp)!=sizeof(msg->idx)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s reading index at offset %ld (byte %lu)", __FUNCTION__
				,(long)msg->idx_offset,byte_offset);
			return(SMB_ERR_READ);
		}
		/* Save the correct offset (from the beginning of the file) */
		msg->idx_offset=byte_offset/idxreclen;
		return(SMB_SUCCESS); 
	}

	bot=0;
	top=total;
	l=total/2; /* Start at middle index */
	while(1) {
		if(l>=total) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msg %lu not found"
				, __FUNCTION__, (ulong)msg->hdr.number);
			return(SMB_ERR_NOT_FOUND);
		}
		if(fseek(smb->sid_fp,l*idxreclen,SEEK_SET)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s %d '%s' seeking to offset %lu (byte %lu) in index file", __FUNCTION__
				,get_errno(),STRERROR(get_errno())
				,l,l*idxreclen);
			return(SMB_ERR_SEEK);
		}
		if(smb_fread(smb,&idx,sizeof(idx),smb->sid_fp)!=sizeof(idx)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s reading index at offset %lu (byte %lu)", __FUNCTION__
				,l,l*sizeof(idx));
			return(SMB_ERR_READ);
		}
		if(bot==top-1 && idx.number!=msg->hdr.number) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msg %lu not found"
				, __FUNCTION__, (ulong)msg->hdr.number);
			return(SMB_ERR_NOT_FOUND);
		}
		if(idx.number>msg->hdr.number) {
			top=l;
			l=bot+((top-bot)/2);
			continue; 
		}
		if(idx.number<msg->hdr.number) {
			bot=l;
			l=top-((top-bot)/2);
			continue; 
		}
		break; 
	}
	msg->idx=idx;
	msg->idx_offset=l;
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Count the number of msg index records with specific attribute flags		*/
/****************************************************************************/
uint32_t smb_count_idx_records(smb_t* smb, uint16_t mask, uint16_t cmp)
{
	int32_t offset = 0;
	uint32_t count = 0;
	for(offset = 0; ;offset++) {
		smbmsg_t msg;
		memset(&msg, 0, sizeof(msg));
		msg.idx_offset = offset;
		if(smb_getmsgidx(smb, &msg) != SMB_SUCCESS)
			break;
		if((msg.idx.attr & mask) == cmp)
			count++;
	}
	return count;
}

/****************************************************************************/
/* Returns the length (in bytes) of the SMB's index records					*/
/****************************************************************************/
size_t smb_idxreclen(smb_t* smb)
{
	if(smb->status.attr&SMB_FILE_DIRECTORY)
		return sizeof(fileidxrec_t);
	return sizeof(idxrec_t);
}

/****************************************************************************/
/* Reads the first index record in the open message base 					*/
/****************************************************************************/
int smb_getfirstidx(smb_t* smb, idxrec_t *idx)
{
	if(smb->sid_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s index not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sid_fp);
	if(fseek(smb->sid_fp,0,SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s %d '%s' seeking to beginning of index file", __FUNCTION__
			,get_errno(),STRERROR(get_errno()));
		return(SMB_ERR_SEEK);
	}
	if(smb_fread(smb,idx,sizeof(*idx),smb->sid_fp)!=sizeof(*idx)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s reading first index", __FUNCTION__);
		return(SMB_ERR_READ);
	}
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Reads the last index record in the open message base 					*/
/****************************************************************************/
int smb_getlastidx(smb_t* smb, idxrec_t *idx)
{
	off_t length;
	size_t idxreclen = smb_idxreclen(smb);

	if(smb->sid_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s index not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sid_fp);
	length=filelength(fileno(smb->sid_fp));
	if(length<(long)idxreclen) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s invalid index file length: %ld", __FUNCTION__,length);
		return(SMB_ERR_FILE_LEN);
	}
	if(fseeko(smb->sid_fp,length-idxreclen,SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s %d '%s' seeking to %u in index file", __FUNCTION__
			,get_errno(),STRERROR(get_errno())
			,(unsigned)(length-sizeof(idxrec_t)));
		return(SMB_ERR_SEEK);
	}
	if(smb_fread(smb,idx,sizeof(*idx),smb->sid_fp)!=sizeof(*idx)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s reading last index", __FUNCTION__);
		return(SMB_ERR_READ);
	}
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Finds index of last message imported at or after specified time			*/
/* If you want the message base locked during this operation, the caller	*/
/* must call smb_locksmbhdr() before, smb_unlocksmbhdr() after.				*/
/* Returns >= 0 on success, negative (SMB_* error) on failure.				*/
/****************************************************************************/
long smb_getmsgidx_by_time(smb_t* smb, idxrec_t* match, time_t t)
{
    int			result;
	long		match_offset;
	ulong		total, bot, top;
	idxrec_t	idx;
	size_t		idxreclen = smb_idxreclen(smb);

	if(match == NULL)
		return SMB_BAD_PARAMETER;

	memset(match, 0, sizeof(*match));

	if(t <= 0)
		return SMB_BAD_PARAMETER;

	total = (ulong)(filelength(fileno(smb->sid_fp))/idxreclen);

	if(!total)	/* Empty base */
		return SMB_ERR_NOT_FOUND;

	if((result=smb_getlastidx(smb, &idx)) != SMB_SUCCESS) {
		return result;
	}
	if((time_t)idx.time < t) {
		return SMB_ERR_NOT_FOUND;
	}

	match_offset = total - 1;
	*match = idx;

	bot = 0;
	top = total-1;
	clearerr(smb->sid_fp);
	while(bot <= top) {
		long idx_offset = (bot + top) / 2;
		if(fseek(smb->sid_fp, idx_offset * idxreclen, SEEK_SET) != 0)
			return SMB_ERR_SEEK;
		if(fread(&idx, 1, sizeof(idx), smb->sid_fp) != sizeof(idx))
			return SMB_ERR_READ;
		if((time_t)idx.time < t) {
			bot = idx_offset + 1;
			continue;
		}
		*match = idx;
		match_offset = idx_offset;
		if((time_t)idx.time > t && idx_offset > 0) {
			top = idx_offset - 1;
			continue;
		}
		break;
	}
	return match_offset;
}


/****************************************************************************/
/* Figures out the total length of the header record for 'msg'              */
/* Returns length 															*/
/****************************************************************************/
ulong smb_getmsghdrlen(smbmsg_t* msg)
{
	int i;
	ulong length;

	/* fixed portion */
	length=sizeof(msghdr_t);
	/* data fields */
	length+=msg->hdr.total_dfields*sizeof(dfield_t);
	/* header fields */
	for(i=0;i<msg->total_hfields;i++) {
		length+=sizeof(hfield_t);
		length+=msg->hfield[i].length; 
	}
	return(length);
}

/****************************************************************************/
/* Figures out the total length of the data buffer for 'msg'                */
/* Returns length															*/
/****************************************************************************/
ulong smb_getmsgdatlen(smbmsg_t* msg)
{
	int i;
	ulong length=0L;

	for(i=0;i<msg->hdr.total_dfields;i++)
		length+=msg->dfield[i].length;
	return(length);
}

/****************************************************************************/
/* Figures out the total length of the text buffer for 'msg'                */
/* Returns length															*/
/****************************************************************************/
ulong smb_getmsgtxtlen(smbmsg_t* msg)
{
	int i;
	ulong length=0L;

	for(i=0;i<msg->total_hfields;i++) {
		switch(msg->hfield[i].type) {
		case SMB_COMMENT:
		case SMTPSYSMSG:
		case SMB_POLL_ANSWER:
			length+=msg->hfield[i].length+2;
			break;
		}
	}
	for(i=0;i<msg->hdr.total_dfields;i++)
		if(msg->dfield[i].type==TEXT_BODY || msg->dfield[i].type==TEXT_TAIL)
			length+=msg->dfield[i].length;
	return(length);
}

static void set_convenience_ptr(smbmsg_t* msg, uint16_t hfield_type, void* hfield_dat)
{
	switch(hfield_type) {	/* convenience variables */
		case SENDER:
			msg->from=(char*)hfield_dat;
			break; 
		case FORWARDED:
			msg->forwarded = TRUE;
			break;
		case SENDERAGENT:
			msg->from_agent=*(uint16_t *)hfield_dat;
			break;
		case SENDEREXT:
			msg->from_ext=(char*)hfield_dat;
			break;
		case SENDERORG:
			msg->from_org=(char*)hfield_dat;
			break;
		case SENDERNETTYPE:
			msg->from_net.type=*(uint16_t *)hfield_dat;
			break;
		case SENDERNETADDR:
			msg->from_net.addr=(char*)hfield_dat;
			break;
		case SENDERIPADDR:
			msg->from_ip=(char*)hfield_dat;
			break;
		case SENDERHOSTNAME:
			msg->from_host=(char*)hfield_dat;
			break;
		case SENDERPROTOCOL:
			msg->from_prot=(char*)hfield_dat;
			break;
		case SENDERPORT:
			msg->from_port=(char*)hfield_dat;
			break;
		case SMB_AUTHOR:
			msg->author=(char*)hfield_dat;
			break;
		case SMB_AUTHOR_ORG:
			msg->author_org=(char*)hfield_dat;
			break;
		case REPLYTO:
			msg->replyto=(char*)hfield_dat;
			break;
		case REPLYTOLIST:
			msg->replyto_list = (char*)hfield_dat;
			break;
		case REPLYTOEXT:
			msg->replyto_ext=(char*)hfield_dat;
			break;
		case REPLYTOAGENT:
			msg->replyto_agent=*(uint16_t *)hfield_dat;
			break;
		case REPLYTONETTYPE:
			msg->replyto_net.type=*(uint16_t *)hfield_dat;
			break;
		case REPLYTONETADDR:
			msg->replyto_net.addr=(char*)hfield_dat;
			break;
		case RECIPIENT:
			msg->to=(char*)hfield_dat;
			break;
		case RECIPIENTLIST:
			msg->to_list = (char*)hfield_dat;
			break;
		case RECIPIENTEXT:
			msg->to_ext=(char*)hfield_dat;
			break;
		case RECIPIENTAGENT:
			msg->to_agent=*(uint16_t *)hfield_dat;
			break;
		case RECIPIENTNETTYPE:
			msg->to_net.type=*(uint16_t *)hfield_dat;
			break;
		case RECIPIENTNETADDR:
			msg->to_net.addr=(char*)hfield_dat;
			break;
		case SUBJECT:
			msg->subj=(char*)hfield_dat;
			break;
		case SMB_CARBONCOPY:
			msg->cc_list = (char*)hfield_dat;
			break;
		case SMB_SUMMARY:
			msg->summary=(char*)hfield_dat;
			break;
		case SMB_TAGS:
			msg->tags=(char*)hfield_dat;
			break;
		case SMB_EDITOR:
			msg->editor=(char*)hfield_dat;
			break;
		case SMB_COLUMNS:
			msg->columns=*(uint8_t*)hfield_dat;
			break;
		case SMB_EXPIRATION:
			msg->expiration=*(uint32_t*)hfield_dat;
			break;
		case SMB_COST:
			msg->cost=*(uint32_t*)hfield_dat;
			break;
		case RFC822MSGID:
			msg->id=(char*)hfield_dat;
			break;
		case RFC822REPLYID:
			msg->reply_id=(char*)hfield_dat;
			break;
		case SMTPREVERSEPATH:
			msg->reverse_path=(char*)hfield_dat;
			break;
		case SMTPFORWARDPATH:
			msg->forward_path=(char*)hfield_dat;
			break;
		case USENETPATH:
			msg->path=(char*)hfield_dat;
			break;
		case USENETNEWSGROUPS:
			msg->newsgroups=(char*)hfield_dat;
			break;
		case FIDOMSGID:
			msg->ftn_msgid=(char*)hfield_dat;
			break;
		case FIDOREPLYID:
			msg->ftn_reply=(char*)hfield_dat;
			break;
		case FIDOAREA:
			msg->ftn_area=(char*)hfield_dat;
			break;
		case FIDOPID:
			msg->ftn_pid=(char*)hfield_dat;
			break;
		case FIDOTID:
			msg->ftn_tid=(char*)hfield_dat;
			break;
		case FIDOFLAGS:
			msg->ftn_flags=(char*)hfield_dat;
			break;
		case FIDOCHARSET:
			msg->ftn_charset=(char*)hfield_dat;
			break;
		case FIDOBBSID:
			msg->ftn_bbsid=(char*)hfield_dat;
			break;
		case RFC822HEADER:
		{
			char* p = (char*)hfield_dat;
			if(strnicmp(p, "MIME-Version:", 13) == 0) {
				p += 13;
				SKIP_WHITESPACE(p);
				msg->mime_version = p;
				break;
			}
			if(strnicmp(p, "Content-Type:", 13) == 0) {
				p += 13;
				SKIP_WHITESPACE(p);
				msg->content_type = p;
				smb_parse_content_type(p, &(msg->text_subtype), &(msg->text_charset));
				break;
			}
			if(strnicmp(p, "Content-Transfer-Encoding:", 26) == 0) {
				p += 26;
				SKIP_WHITESPACE(p);
				msg->content_encoding = p;
				break;
			}
			break;
		}
	}
}

static void clear_convenience_ptrs(smbmsg_t* msg)
{
	msg->from=NULL;
	msg->from_ext=NULL;
	msg->from_org=NULL;
	msg->from_ip=NULL;
	msg->from_host=NULL;
	msg->from_prot=NULL;
	msg->from_port=NULL;
	memset(&msg->from_net,0,sizeof(net_t));

	msg->replyto=NULL;
	msg->replyto_ext=NULL;
	msg->replyto_list=NULL;
	memset(&msg->replyto_net,0,sizeof(net_t));

	msg->to=NULL;
	msg->to_ext=NULL;
	msg->to_list=NULL;
	memset(&msg->to_net,0,sizeof(net_t));

	msg->author=NULL;
	msg->author_org=NULL;
	msg->cc_list=NULL;
	msg->subj=NULL;
	msg->summary=NULL;
	msg->tags=NULL;
	msg->editor=NULL;
	msg->id=NULL;
	msg->reply_id=NULL;
	msg->reverse_path=NULL;
	msg->forward_path=NULL;
	msg->path=NULL;
	msg->newsgroups=NULL;
	msg->mime_version=NULL;
	msg->content_type=NULL;
	msg->content_encoding=NULL;
	msg->text_subtype=NULL;
	msg->text_charset=NULL;

	msg->ftn_msgid=NULL;
	msg->ftn_reply=NULL;
	msg->ftn_area=NULL;
	msg->ftn_pid=NULL;
	msg->ftn_tid=NULL;
	msg->ftn_flags=NULL;
	msg->ftn_charset=NULL;
}

/****************************************************************************/
/* Read header information into 'msg' structure                             */
/* msg->idx.offset must be set before calling this function 				*/
/* Must call smb_freemsgmem() to free memory allocated for var len strs 	*/
/* Returns 0 on success, non-zero if error									*/
/****************************************************************************/
int smb_getmsghdr(smb_t* smb, smbmsg_t* msg)
{
	void	*vp,**vpp;
	size_t	i;
	long	l,offset;
	fileidxrec_t idx;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}

	if(!smb_valid_hdr_offset(smb,msg->idx.offset))
		return(SMB_ERR_HDR_OFFSET);

	offset = ftell(smb->shd_fp);
	if(offset != msg->idx.offset) {
		if(fseek(smb->shd_fp,msg->idx.offset,SEEK_SET) != 0) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s %d '%s' seeking to offset %lu in header file", __FUNCTION__
				,get_errno(),STRERROR(get_errno())
				,(ulong)msg->idx.offset);
			return(SMB_ERR_SEEK);
		}
	}

	idx = msg->file_idx;
	offset=msg->idx_offset;
	memset(msg,0,sizeof(smbmsg_t));
	msg->file_idx = idx;
	msg->idx_offset=offset;
	if(smb_fread(smb,&msg->hdr,sizeof(msghdr_t),smb->shd_fp)!=sizeof(msghdr_t)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s reading msg header at offset %lu", __FUNCTION__, (ulong)msg->idx.offset);
		return(SMB_ERR_READ);
	}
	if(memcmp(msg->hdr.msghdr_id,SHD_HEADER_ID,LEN_HEADER_ID)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s corrupt message header ID (%02X %02X %02X %02X) at offset %lu", __FUNCTION__
			,msg->hdr.msghdr_id[0]
			,msg->hdr.msghdr_id[1]
			,msg->hdr.msghdr_id[2]
			,msg->hdr.msghdr_id[3]
			,(ulong)msg->idx.offset);
		return(SMB_ERR_HDR_ID);
	}
	if(msg->hdr.version<0x110) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s insufficient header version: %X at offset %lu", __FUNCTION__
			,msg->hdr.version, (ulong)msg->idx.offset);
		return(SMB_ERR_HDR_VER);
	}
	l=sizeof(msg->hdr);
	if(msg->hdr.total_dfields 
		&& (msg->dfield = malloc(sizeof(*msg->dfield)*msg->hdr.total_dfields)) == NULL) {
		smb_freemsgmem(msg);
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s malloc failure of %ld bytes for %u data fields", __FUNCTION__
			,(long)(sizeof(*msg->dfield) * msg->hdr.total_dfields), msg->hdr.total_dfields);
		return(SMB_ERR_MEM); 
	}
	i = fread(msg->dfield, sizeof(*msg->dfield), msg->hdr.total_dfields, smb->shd_fp);
	if(i != msg->hdr.total_dfields) {
		smb_freemsgmem(msg);
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s insufficient data fields read (%d instead of %d)", __FUNCTION__
			,(int)i,msg->hdr.total_dfields);
		return(SMB_ERR_READ); 
	}
	l += msg->hdr.total_dfields * sizeof(*msg->dfield);
	while(l < (long)msg->hdr.length) {
		i=msg->total_hfields;
		if((vpp=(void* *)realloc(msg->hfield_dat,sizeof(void* )*(i+1)))==NULL) {
			smb_freemsgmem(msg);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s realloc failure of %d bytes for header field data", __FUNCTION__
				,(int)(sizeof(void*)*(i+1)));
			return(SMB_ERR_MEM); 
		}
		msg->hfield_dat=vpp;
		if((vp=(hfield_t *)realloc(msg->hfield,sizeof(hfield_t)*(i+1)))==NULL) {
			smb_freemsgmem(msg);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s realloc failure of %d bytes for header fields", __FUNCTION__
				,(int)(sizeof(hfield_t)*(i+1)));
			return(SMB_ERR_MEM); 
		}
		msg->hfield=vp;
		if(smb_fread(smb,&msg->hfield[i],sizeof(hfield_t),smb->shd_fp)!=sizeof(hfield_t)) {
			smb_freemsgmem(msg);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s reading header field", __FUNCTION__);
			return(SMB_ERR_READ); 
		}
		l+=sizeof(hfield_t);
		if((msg->hfield_dat[i]=(char*)malloc(msg->hfield[i].length+1))
			==NULL) {			/* Allocate 1 extra for ASCIIZ terminator */
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s malloc failure of %d bytes for header field %d", __FUNCTION__
				,msg->hfield[i].length+1, (int)i);
			smb_freemsgmem(msg);  /* or 0 length field */
			return(SMB_ERR_MEM); 
		}
		msg->total_hfields++;
		memset(msg->hfield_dat[i],0,msg->hfield[i].length+1);  /* init to NULL */
		if(msg->hfield[i].length
			&& smb_fread(smb,msg->hfield_dat[i],msg->hfield[i].length,smb->shd_fp)
				!=(size_t)msg->hfield[i].length) {
			smb_freemsgmem(msg);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s reading header field data", __FUNCTION__);
			return(SMB_ERR_READ); 
		}
		set_convenience_ptr(msg,msg->hfield[i].type,msg->hfield_dat[i]);

		l+=msg->hfield[i].length; 
	}

	/* These convenience pointers must point to something */
	if(msg->from==NULL)	msg->from=nulstr;
	if(msg->to==NULL)	msg->to=nulstr;
	if(msg->subj==NULL)	msg->subj=nulstr;

	/* If no reverse path specified, use sender's address */
	if(msg->reverse_path == NULL && msg->from_net.type==NET_INTERNET)
		msg->reverse_path = msg->from_net.addr;

	/* Read (and discard) the remaining bytes in the current allocation block (if any) */
	while((offset = ftell(smb->shd_fp)) >=0 && ((offset - smb->status.header_offset) % SHD_BLOCK_LEN) != 0)
		(void)fgetc(smb->shd_fp);

	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Frees memory allocated for variable-length header fields in 'msg'        */
/****************************************************************************/
void smb_freemsghdrmem(smbmsg_t* msg)
{
	uint16_t	i;

	for(i=0;i<msg->total_hfields;i++)
		if(msg->hfield_dat[i]) {
			free(msg->hfield_dat[i]);
			msg->hfield_dat[i]=NULL;
		}
	msg->total_hfields=0;
	if(msg->hfield) {
		free(msg->hfield);
		msg->hfield=NULL;
	}
	if(msg->hfield_dat) {
		free(msg->hfield_dat);
		msg->hfield_dat=NULL;
	}
	clear_convenience_ptrs(msg);	/* don't leave pointers to freed memory */
}

/****************************************************************************/
/* Frees memory allocated for 'msg'                                         */
/****************************************************************************/
void smb_freemsgmem(smbmsg_t* msg)
{
	if(msg->dfield) {
		free(msg->dfield);
		msg->dfield=NULL;
	}
	msg->hdr.total_dfields=0;
	FREE_AND_NULL(msg->text_subtype);
	FREE_AND_NULL(msg->text_charset);
	smb_freemsghdrmem(msg);
	if(msg->text != NULL) {
		free(msg->text);
		msg->text = NULL;
	}
	if(msg->tail != NULL) {
		free(msg->tail);
		msg->tail = NULL;
	}
}

/****************************************************************************/
/* Copies memory allocated for 'srcmsg' to 'msg'							*/
/****************************************************************************/
int smb_copymsgmem(smb_t* smb, smbmsg_t* msg, smbmsg_t* srcmsg)
{
	int i;

	memcpy(msg,srcmsg,sizeof(smbmsg_t));
	clear_convenience_ptrs(msg);

	/* data field types/lengths */
	if(msg->hdr.total_dfields>0) {
		if((msg->dfield=(dfield_t *)malloc(msg->hdr.total_dfields*sizeof(dfield_t)))==NULL) {
			if(smb!=NULL)
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"%s malloc failure of %ld bytes for %d data fields", __FUNCTION__
					,(long)(msg->hdr.total_dfields*sizeof(dfield_t)), msg->hdr.total_dfields);
			return(SMB_ERR_MEM);
		}
		memcpy(msg->dfield,srcmsg->dfield,msg->hdr.total_dfields*sizeof(dfield_t));
	}

	/* header field types/lengths */
	if(msg->total_hfields>0) {
		if((msg->hfield=(hfield_t *)malloc(msg->total_hfields*sizeof(hfield_t)))==NULL) {
			if(smb!=NULL)
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"%s malloc failure of %ld bytes for %d header fields", __FUNCTION__
					,(long)(msg->total_hfields*sizeof(hfield_t)), msg->total_hfields);
			return(SMB_ERR_MEM);
		}
		memcpy(msg->hfield,srcmsg->hfield,msg->total_hfields*sizeof(hfield_t));

		/* header field data */
		if((msg->hfield_dat=(void**)malloc(msg->total_hfields*sizeof(void*)))==NULL) {
			if(smb!=NULL)
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"%s malloc failure of %ld bytes for %d header fields", __FUNCTION__
					,(long)(msg->total_hfields*sizeof(void*)), msg->total_hfields);
			return(SMB_ERR_MEM);
		}

		for(i=0;i<msg->total_hfields;i++) {
			if((msg->hfield_dat[i]=(void*)malloc(msg->hfield[i].length+1))==NULL) {
				if(smb!=NULL)
					safe_snprintf(smb->last_error,sizeof(smb->last_error)
						,"%s malloc failure of %d bytes for header field #%d", __FUNCTION__
						,msg->hfield[i].length+1, i+1);
				return(SMB_ERR_MEM);
			}
			memset(msg->hfield_dat[i],0,msg->hfield[i].length+1);
			memcpy(msg->hfield_dat[i],srcmsg->hfield_dat[i],msg->hfield[i].length);
			set_convenience_ptr(msg, msg->hfield[i].type, msg->hfield_dat[i]);
		}
	}

	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Unlocks header for 'msg'                                                 */
/****************************************************************************/
int smb_unlockmsghdr(smb_t* smb, smbmsg_t* msg)
{
	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	if(!smb_valid_hdr_offset(smb,msg->idx.offset))
		return(SMB_ERR_HDR_OFFSET);
	return(unlock(fileno(smb->shd_fp),msg->idx.offset,sizeof(msghdr_t)));
}


/****************************************************************************/
/* Adds a header field to the 'msg' structure (in memory only)              */
/****************************************************************************/
int smb_hfield_add(smbmsg_t* msg, uint16_t type, size_t length, void* data, BOOL insert)
{
	void**		vpp;
	hfield_t*	hp;
	int i;

	if(smb_getmsghdrlen(msg)+sizeof(hfield_t)+length>SMB_MAX_HDR_LEN)
		return(SMB_ERR_HDR_LEN);

	i=msg->total_hfields;
	if((hp=(hfield_t *)realloc(msg->hfield,sizeof(hfield_t)*(i+1)))==NULL) 
		return(SMB_ERR_MEM);
	msg->hfield=hp;

	if((vpp=(void* *)realloc(msg->hfield_dat,sizeof(void* )*(i+1)))==NULL) 
		return(SMB_ERR_MEM);
	msg->hfield_dat=vpp;

	if(insert) {
		memmove(msg->hfield+1, msg->hfield, sizeof(hfield_t)*i);
		memmove(msg->hfield_dat+1, msg->hfield_dat, sizeof(void*)*i);
		i=0;
	}
	msg->total_hfields++;
	msg->hfield[i].type=type;
	msg->hfield[i].length=(uint16_t)length;
	if((msg->hfield_dat[i]=(void* )malloc(length+1))==NULL) 
		return(SMB_ERR_MEM);	/* Allocate 1 extra for ASCIIZ terminator */
	memset(msg->hfield_dat[i],0,length+1);
	memcpy(msg->hfield_dat[i],data,length); 
	set_convenience_ptr(msg,type,msg->hfield_dat[i]);

	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Adds a list of header fields to the 'msg' structure (in memory only)     */
/****************************************************************************/
int	smb_hfield_add_list(smbmsg_t* msg, hfield_t** hfield_list, void** hfield_dat, BOOL insert)
{
	int			retval;
	unsigned	n;

	if(hfield_list==NULL)
		return(SMB_FAILURE);

	for(n=0;hfield_list[n]!=NULL;n++)
		if((retval=smb_hfield_add(msg
			,hfield_list[n]->type,hfield_list[n]->length,hfield_dat[n],insert))!=SMB_SUCCESS)
			return(retval);

	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Convenience function to add an ASCIIZ string header field (or blank)		*/
/****************************************************************************/
int smb_hfield_add_str(smbmsg_t* msg, uint16_t type, const char* str, BOOL insert)
{
	return smb_hfield_add(msg, type, str==NULL ? 0:strlen(str), (void*)str, insert);
}

/****************************************************************************/
/* Convenience function to add an ASCIIZ string header field (NULL ignored)	*/
/****************************************************************************/
int smb_hfield_string(smbmsg_t* msg, uint16_t type, const char* str)
{
	if(str == NULL)
		return SMB_ERR_HDR_FIELD;
	return smb_hfield_add(msg, type, strlen(str), (void*)str, /* insert */FALSE);
}

/****************************************************************************/
/* Convenience function to a network address header field to msg			*/
/* Pass NULL for net_type to have the auto-detected net_type hfield	added	*/
/* as well.																	*/
/****************************************************************************/
int	smb_hfield_add_netaddr(smbmsg_t* msg, uint16_t type, const char* addr, uint16_t* net_type, BOOL insert)
{
	int			result;
	fidoaddr_t	sys_addr = {0,0,0,0};	/* replace unspecified fields with 0 (don't assume 1:1/1) */
	fidoaddr_t	fidoaddr;
	uint16_t	tmp_net_type=NET_UNKNOWN;

	if(addr == NULL)
		return SMB_ERR_HDR_FIELD;
	SKIP_WHITESPACE(addr);
	if(net_type==NULL)
		net_type=&tmp_net_type;
	if(*net_type==NET_UNKNOWN) {
		*net_type=smb_netaddr_type(addr);
		if(*net_type==NET_NONE)
			return SMB_ERR_NOT_FOUND;
	}
	if(*net_type!=NET_NONE && *net_type!=NET_INTERNET) {	/* Only Internet net-addresses are allowed to have '@' in them */
		const char* p = strchr(addr, '@');
		if(p != NULL) {
			p++;
			SKIP_WHITESPACE(p);
			if(*p == 0)
				return SMB_ERR_NOT_FOUND;
			addr = p;
		}
	}
	if(*net_type==NET_FIDO) {
		fidoaddr=smb_atofaddr(&sys_addr,addr);
		result = smb_hfield_add(msg,type,sizeof(fidoaddr),&fidoaddr,insert);
	} else {
		result = smb_hfield_add_str(msg,type,addr,insert);
	}
	if(result==SMB_SUCCESS && net_type == &tmp_net_type) {
		// *NETTYPE is always *NETADDR-1
		result = smb_hfield_add(msg,type-1,sizeof(*net_type),net_type,insert);
	}
	return result;
}

/****************************************************************************/
/* Appends data to an existing header field (in memory only)				*/
/****************************************************************************/
int smb_hfield_append(smbmsg_t* msg, uint16_t type, size_t length, void* data)
{
	int		i;
	BYTE*	p;

	if(length==0)	/* nothing to append */
		return(SMB_SUCCESS);

	if(msg->total_hfields<1)
		return(SMB_ERR_NOT_FOUND);

	/* search for the last header field of this type */
	for(i=msg->total_hfields-1;i>=0;i--)
		if(msg->hfield[i].type == type)
			break;
	if(i<0)
		return(SMB_ERR_NOT_FOUND);

	if(smb_getmsghdrlen(msg)+length>SMB_MAX_HDR_LEN)
		return(SMB_ERR_HDR_LEN);

	if((p=(BYTE*)realloc(msg->hfield_dat[i],msg->hfield[i].length+length+1))==NULL) 
		return(SMB_ERR_MEM);	/* Allocate 1 extra for ASCIIZ terminator */

	msg->hfield_dat[i]=p;
	p+=msg->hfield[i].length;	/* skip existing data */
	memset(p,0,length+1);
	memcpy(p,data,length);		/* append */
	msg->hfield[i].length+=(uint16_t)length;
	set_convenience_ptr(msg,type,msg->hfield_dat[i]);

	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Appends data to an existing ASCIIZ header field (in memory only)			*/
/****************************************************************************/
int smb_hfield_append_str(smbmsg_t* msg, uint16_t type, const char* str)
{
	return smb_hfield_append(msg, type, str==NULL ? 0:strlen(str), (void*)str);
}

/****************************************************************************/
/* Replaces an header field value (in memory only)							*/
/****************************************************************************/
int smb_hfield_replace(smbmsg_t* msg, uint16_t type, size_t length, void* data)
{
	int		i;
	void*	p;

	if(msg->total_hfields<1)
		return(SMB_ERR_NOT_FOUND);

	/* search for the last header field of this type */
	for(i=msg->total_hfields-1;i>=0;i--)
		if(msg->hfield[i].type == type)
			break;
	if(i<0)
		return(SMB_ERR_NOT_FOUND);

	if((p=(BYTE*)realloc(msg->hfield_dat[i],length+1))==NULL) 
		return(SMB_ERR_MEM);	/* Allocate 1 extra for ASCIIZ terminator */

	msg->hfield_dat[i]=p;
	memset(p,0,length+1);
	memcpy(p,data,length);
	msg->hfield[i].length=(uint16_t)length;
	set_convenience_ptr(msg,type,msg->hfield_dat[i]);

	return SMB_SUCCESS;
}

/****************************************************************************/
/* Replace an existing ASCIIZ header field value (in memory only)			*/
/****************************************************************************/
int smb_hfield_replace_str(smbmsg_t* msg, uint16_t type, const char* str)
{
	return smb_hfield_replace(msg, type, str==NULL ? 0:strlen(str), (void*)str);
}

/****************************************************************************/
/* Searches for a specific header field (by type) and returns it			*/
/****************************************************************************/
void* smb_get_hfield(smbmsg_t* msg, uint16_t type, hfield_t** hfield)
{
	int i;

	for(i=0;i<msg->total_hfields;i++)
		if(msg->hfield[i].type == type) {
			if(hfield != NULL)
				*hfield = &msg->hfield[i];
			return(msg->hfield_dat[i]);
		}

	return(NULL);
}

/****************************************************************************/
/* Add or replace a specific header field (by type)							*/
/****************************************************************************/
int smb_new_hfield(smbmsg_t* msg, uint16_t type, size_t length, void* data)
{
	if(smb_get_hfield(msg, type, NULL))
		return smb_hfield_replace(msg, type, length, data);
	else
		return smb_hfield_add(msg, type, length, data, /* insert */FALSE);
}

/****************************************************************************/
/* Add or replace a specific string header field (by type)					*/
/****************************************************************************/
int smb_new_hfield_str(smbmsg_t* msg, uint16_t type, const char* str)
{
	return smb_new_hfield(msg, type, str == NULL ? 0 : strlen(str), (void*)str);
}

/****************************************************************************/
/* Adds a data field to the 'msg' structure (in memory only)                */
/* Automatically figures out the offset into the data buffer from existing	*/
/* dfield lengths															*/
/****************************************************************************/
int smb_dfield(smbmsg_t* msg, uint16_t type, ulong length)
{
	dfield_t* dp;
	int i,j;

	i=msg->hdr.total_dfields;
	if((dp=(dfield_t *)realloc(msg->dfield,sizeof(dfield_t)*(i+1)))==NULL) 
		return(SMB_ERR_MEM);
	
	msg->dfield=dp;
	msg->hdr.total_dfields++;
	msg->dfield[i].type=type;
	msg->dfield[i].length=length;
	for(j=msg->dfield[i].offset=0;j<i;j++)
		msg->dfield[i].offset+=msg->dfield[j].length;
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Checks CRC history file for duplicate crc. If found, returns SMB_DUPE_MSG*/
/* If no dupe, adds to CRC history and returns 0, or negative if error. 	*/
/****************************************************************************/
int smb_addcrc(smb_t* smb, uint32_t crc)
{
	char	str[MAX_PATH+1];
	int 	file;
	int		wr;
	off_t	length;
	long	newlen;
	ulong	l;
	uint32_t *buf;
	time_t	start=0;

	if(!smb->status.max_crcs)
		return(SMB_SUCCESS);

	SAFEPRINTF(str,"%s.sch",smb->file);
	while(1) {
		if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW, DEFFILEMODE))!=-1)
			break;
		if(get_errno()!=EACCES && get_errno()!=EAGAIN) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s %d '%s' opening %s", __FUNCTION__
				,get_errno(),STRERROR(get_errno()),str);
			return(SMB_ERR_OPEN);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"%s timeout opening %s (retry_time=%lu)", __FUNCTION__
					,str, (ulong)smb->retry_time);
				return(SMB_ERR_TIMEOUT); 
			}
		SLEEP(smb->retry_delay);
	}

	length=filelength(file);
	if(length<0L || length%sizeof(uint32_t)) {
		close(file);
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s invalid file length: %ld", __FUNCTION__, length);
		return(SMB_ERR_FILE_LEN); 
	}

	if(length!=0) {
		if((buf=(uint32_t*)malloc((size_t)length))==NULL) {
			close(file);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s malloc failure of %ld bytes", __FUNCTION__
				,length);
			return(SMB_ERR_MEM); 
		}

		if(read(file,buf,(uint)length)!=length) {
			close(file);
			free(buf);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s %d '%s' reading %ld bytes", __FUNCTION__
				,get_errno(),STRERROR(get_errno()),length);
			return(SMB_ERR_READ);
		}

		for(l=0;l<length/sizeof(int32_t);l++)
			if(crc==buf[l])
				break;
		if(l<length/sizeof(int32_t)) {					/* Dupe CRC found */
			close(file);
			free(buf);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s duplicate message text CRC detected", __FUNCTION__);
			return(SMB_DUPE_MSG);
		} 

		if(length>=(long)(smb->status.max_crcs*sizeof(int32_t))) {
			newlen=(smb->status.max_crcs-1)*sizeof(int32_t);
			chsize(file,0);	/* truncate it */
			lseek(file,0L,SEEK_SET);
			write(file,buf+(length-newlen),newlen); 
		}
		free(buf);
	}
	wr=write(file,&crc,sizeof(crc));	/* Write to the end */
	close(file);

	if(wr!=sizeof(crc)) {	
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s %d '%s' writing %ld bytes", __FUNCTION__
			,get_errno(),STRERROR(get_errno()),(long)sizeof(crc));
		return(SMB_ERR_WRITE);
	}

	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Creates a new message header record in the header file.					*/
/* If storage is SMB_SELFPACK, self-packing conservative allocation is used */
/* If storage is SMB_FASTALLOC, fast allocation is used 					*/
/* If storage is SMB_HYPERALLOC, no allocation tables are used (fastest)	*/
/* This function will UN-lock the SMB header								*/
/****************************************************************************/
int smb_addmsghdr(smb_t* smb, smbmsg_t* msg, int storage)
{
	int		i;
	off_t	l;
	ulong	hdrlen;
	off_t	idxlen;
	size_t	idxreclen = smb_idxreclen(smb);

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}

	if(!smb->locked && smb_locksmbhdr(smb)!=SMB_SUCCESS)
		return(SMB_ERR_LOCK);

	hdrlen=smb_getmsghdrlen(msg);
	if(hdrlen>SMB_MAX_HDR_LEN) { /* headers are limited to 64k in size */
		smb_unlocksmbhdr(smb);
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s illegal message header length (%lu > %u)", __FUNCTION__
			,hdrlen,SMB_MAX_HDR_LEN);
		return(SMB_ERR_HDR_LEN);
	}

	if((i=smb_getstatus(smb))!=SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return(i);
	}

	idxlen = filelength(fileno(smb->sid_fp));
	if(idxlen != (smb->status.total_msgs * idxreclen)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
			,"%s index file length (%ld), expected (%ld)", __FUNCTION__
			,idxlen, smb->status.total_msgs * idxreclen);
		smb_unlocksmbhdr(smb);
		return SMB_ERR_FILE_LEN;
	}
		
	msg->hdr.number=smb->status.last_msg+1;

	if(msg->hdr.thread_id==0)	/* new thread being started */
		msg->hdr.thread_id=msg->hdr.number;

	/* This is *not* a dupe-check */
	if(!(msg->flags&MSG_FLAG_HASHED) /* not already hashed */
		&& (i=smb_hashmsg(smb,msg,/* text: */NULL,/* update? */TRUE))!=SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return(i);	/* error updating hash table */
	}

	if(storage!=SMB_HYPERALLOC && (i=smb_open_ha(smb))!=SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return(i);
	}

	if(msg->hdr.version==0)
		msg->hdr.version=SMB_VERSION;
	msg->hdr.length=(uint16_t)hdrlen;
	if(storage==SMB_HYPERALLOC)
		l=smb_hallochdr(smb);
	else if(storage==SMB_FASTALLOC)
		l=smb_fallochdr(smb,msg->hdr.length);
	else
		l=smb_allochdr(smb,msg->hdr.length);
	if(storage!=SMB_HYPERALLOC)
		smb_close_ha(smb);
	if(l<0) {
		smb_unlocksmbhdr(smb);
		return (int)l; 
	}

	msg->idx.offset=(uint32_t)(smb->status.header_offset + l);
	msg->idx_offset=smb->status.total_msgs;
	msg->hdr.attr &= ~MSG_DELETE;
	i=smb_putmsg(smb,msg);
	if(i==SMB_SUCCESS) {
		smb->status.last_msg++;
		smb->status.total_msgs++;
		smb_putstatus(smb);
	}
	smb_unlocksmbhdr(smb);
	return(i);
}

/****************************************************************************/
/* Safely updates existing index and header records for msg					*/
/* Nothing should be locked prior to calling this function and nothing		*/
/* should (normally) be locked when it exits								*/
/****************************************************************************/
int smb_updatemsg(smb_t* smb, smbmsg_t* msg)
{
	int retval;

	/* Insure no one else can be changing the index at this time */
	if((retval=smb_locksmbhdr(smb))!=SMB_SUCCESS)
		return(retval);
	/* Get the current index record offset (for later update by smb_putmsgidx) */
	if((retval=smb_getmsgidx(smb, msg))==SMB_SUCCESS) {
		/* Don't let any one else read or write this header while we're updating it */
		if((retval=smb_lockmsghdr(smb,msg))==SMB_SUCCESS) {
			retval=smb_putmsg(smb,msg);
			smb_unlockmsghdr(smb,msg);
		}
	}
	smb_unlocksmbhdr(smb);
	return(retval);
}

/****************************************************************************/
/* Writes both header and index information for msg 'msg'                   */
/****************************************************************************/
int smb_putmsg(smb_t* smb, smbmsg_t* msg)
{
	int i;

	smb_init_idx(smb,msg);

	if((i=smb_putmsghdr(smb,msg))!=SMB_SUCCESS)
		return(i);

	return(smb_putmsgidx(smb,msg));
}

/****************************************************************************/
/* Initializes/re-synchronizes all the fields of the message index record	*/
/* with the values from the message header (except for the header offset)	*/
/****************************************************************************/
int smb_init_idx(smb_t* smb, smbmsg_t* msg)
{
	if(msg->hdr.type == SMB_MSG_TYPE_BALLOT) {
		msg->idx.votes = msg->hdr.votes;
		msg->idx.remsg = msg->hdr.thread_back;
	} else if(msg->hdr.type == SMB_MSG_TYPE_FILE) {
		if(msg->name != NULL)
			smb_fileidxname(msg->name, msg->file_idx.name, sizeof(msg->file_idx.name));
		if(msg->size > 0)
			msg->idx.size = (uint32_t)msg->size;
	} else {
		msg->idx.subj = smb_subject_crc(msg->subj);
		if(smb->status.attr & SMB_EMAIL) {
			if(msg->to_ext)
				msg->idx.to = atoi(msg->to_ext);
			else
				msg->idx.to = 0;
			if(msg->from_ext)
				msg->idx.from = atoi(msg->from_ext);
			else
				msg->idx.from = 0; 
		} else {
			msg->idx.to = smb_name_crc(msg->to);
			msg->idx.from = smb_name_crc(msg->from);
		}
	}
	/* Make sure these index/header fields are always *nsync */
	msg->idx.number	= msg->hdr.number;
	msg->idx.attr	= msg->hdr.attr;	
	msg->idx.time	= msg->hdr.when_imported.time;

	return(SMB_SUCCESS);
}

BOOL smb_msg_is_from(smbmsg_t* msg, const char* name, enum smb_net_type net_type, const void* net_addr)
{
	if(stricmp(msg->from, name) != 0)
		return FALSE;

	if(msg->from_net.type != net_type)
		return FALSE;

	switch(net_type) {
		case NET_NONE:
			return TRUE;
		case NET_FIDO:
			return memcmp(msg->from_net.addr, net_addr, sizeof(fidoaddr_t)) == 0;
		default:
			return stricmp(msg->from_net.addr, net_addr) == 0;
	}
}

BOOL smb_msg_is_utf8(const smbmsg_t* msg)
{
	for(int i=0; i < msg->total_hfields; i++) {
		switch(msg->hfield[i].type) {
		case FIDOCTRL:
			if(strncmp(msg->hfield_dat[i], "CHRS: UTF-8", 11) == 0)
				return TRUE;
		}
	}
	if(msg->ftn_charset != NULL && strncmp(msg->ftn_charset, "UTF-8", 5) == 0)
		return TRUE;
	return msg->text_charset != NULL && stricmp(msg->text_charset, "utf-8") == 0;
}

uint16_t smb_voted_already(smb_t* smb, uint32_t msgnum, const char* name, enum smb_net_type net_type, void* net_addr)
{
	uint16_t votes = 0;
	smbmsg_t msg = {0};

	if(smb->sid_fp==NULL) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error), "%s index not open", __FUNCTION__);
		return SMB_ERR_NOT_OPEN;
	}
	clearerr(smb->sid_fp);
	if(fseek(smb->sid_fp,0,SEEK_SET)) {
		safe_snprintf(smb->last_error, sizeof(smb->last_error)
			,"%s %d '%s' seeking to beginning of index file", __FUNCTION__
			,get_errno(), STRERROR(get_errno()));
		return SMB_ERR_SEEK;
	}
	memset(&msg, 0, sizeof(msg));
	while(!votes && smb_fread(smb, &msg.idx, sizeof(msg.idx), smb->sid_fp) == sizeof(msg.idx)) {
		if(!(msg.idx.attr&MSG_VOTE) || msg.idx.attr&MSG_POLL)
			continue;
		if(msg.idx.remsg != msgnum)
			continue;
		if(smb_getmsghdr(smb, &msg) != SMB_SUCCESS)
			continue;
		if(smb_msg_is_from(&msg, name, net_type, net_addr)) {
			switch(msg.idx.attr&MSG_VOTE) {
			case MSG_VOTE:
				votes = msg.hdr.votes;
				break;
			case MSG_UPVOTE:
				votes = 1;
				break;
			case MSG_DOWNVOTE:
				votes = 2;
				break;
			}
		}
		smb_freemsgmem(&msg);
	}
	return votes;
}

/****************************************************************************/
/* Writes index information for 'msg'                                       */
/* msg->idx 																*/
/* and msg->offset must be set prior to calling to this function			*/
/* Returns 0 if everything ok                                               */
/****************************************************************************/
int smb_putmsgidx(smb_t* smb, smbmsg_t* msg)
{
	off_t length;
	size_t	idxreclen = smb_idxreclen(smb);

	if(smb->sid_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s index not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sid_fp);
	length = filelength(fileno(smb->sid_fp));
	if(length < (long)(msg->idx_offset*idxreclen)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s invalid index offset: %ld, byte offset: %lu, length: %lu", __FUNCTION__
			,(long)msg->idx_offset, msg->idx_offset*idxreclen, length);
		return(SMB_ERR_HDR_OFFSET);
	}
	if(fseek(smb->sid_fp,msg->idx_offset*idxreclen,SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s %d '%s' seeking to %u in index file", __FUNCTION__
			,get_errno(),STRERROR(get_errno())
			,(unsigned)(msg->idx_offset*idxreclen));
		return(SMB_ERR_SEEK);
	}
	if(smb->status.attr & SMB_FILE_DIRECTORY) {
		if(!fwrite(&msg->file_idx, sizeof(msg->file_idx), 1, smb->sid_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s %d '%s' writing index", __FUNCTION__
				,get_errno(),STRERROR(get_errno()));
			return(SMB_ERR_WRITE);
		}
	} else {
		if(!fwrite(&msg->idx,sizeof(msg->idx),1,smb->sid_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s writing index", __FUNCTION__);
			return(SMB_ERR_WRITE);
		}
	}
	return fflush(smb->sid_fp);	/* SMB_SUCCESS == 0 */
}

/****************************************************************************/
/* Writes header information for 'msg'                                      */
/* msg->hdr.length															*/
/* msg->idx.offset															*/
/* and msg->offset must be set prior to calling to this function			*/
/* Returns 0 if everything ok                                               */
/****************************************************************************/
int smb_putmsghdr(smb_t* smb, smbmsg_t* msg)
{
	uint16_t	i;
	ulong	hdrlen;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}

	if(!smb_valid_hdr_offset(smb,msg->idx.offset))
		return(SMB_ERR_HDR_OFFSET);

	clearerr(smb->shd_fp);
	if(fseek(smb->shd_fp,msg->idx.offset,SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s %d '%s' seeking to %lu in header file", __FUNCTION__
			,get_errno(),STRERROR(get_errno()), (ulong)msg->idx.offset);
		return(SMB_ERR_SEEK);
	}
	/* Verify that the number of blocks required to stored the actual 
	   (calculated) header length does not exceed the number allocated. */
	hdrlen=smb_getmsghdrlen(msg);
	if(hdrlen>SMB_MAX_HDR_LEN) { /* headers are limited to 64k in size */
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s illegal message header length (%lu > %u)", __FUNCTION__
			,hdrlen,SMB_MAX_HDR_LEN);
		return(SMB_ERR_HDR_LEN);
	}
	if(smb_hdrblocks(hdrlen) > smb_hdrblocks(msg->hdr.length)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s illegal header length increase: %lu (%lu blocks, %hu hfields, %hu dfields) vs %hu (%lu blocks)", __FUNCTION__
			,hdrlen, smb_hdrblocks(hdrlen), msg->total_hfields, msg->hdr.total_dfields
			,msg->hdr.length, smb_hdrblocks(msg->hdr.length));
		return(SMB_ERR_HDR_LEN);
	}
	msg->hdr.length=(uint16_t)hdrlen; /* store the actual header length */
	/**********************************/
	/* Set the message header ID here */
	/**********************************/
	memcpy(&msg->hdr.msghdr_id,SHD_HEADER_ID,LEN_HEADER_ID);

	/************************************************/
	/* Write the fixed portion of the header record */
	/************************************************/
	if(!fwrite(&msg->hdr,sizeof(msghdr_t),1,smb->shd_fp)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%s writing fixed portion of header record", __FUNCTION__);
		return(SMB_ERR_WRITE);
	}

	/************************************************/
	/* Write the data fields (each is fixed length) */
	/************************************************/
	for(i=0;i<msg->hdr.total_dfields;i++)
		if(!fwrite(&msg->dfield[i],sizeof(dfield_t),1,smb->shd_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s writing data field", __FUNCTION__);
			return(SMB_ERR_WRITE);
		}
	/*******************************************/
	/* Write the variable length header fields */
	/*******************************************/
	for(i=0;i<msg->total_hfields;i++) {
		if(!fwrite(&msg->hfield[i],sizeof(hfield_t),1,smb->shd_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s writing header field", __FUNCTION__);
			return(SMB_ERR_WRITE);
		}
		if(msg->hfield[i].length					 /* more then 0 bytes long */
			&& !fwrite(msg->hfield_dat[i],msg->hfield[i].length,1,smb->shd_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s writing header field data", __FUNCTION__);
			return(SMB_ERR_WRITE); 
		}
	}

	while(hdrlen%SHD_BLOCK_LEN) {
		if(fputc(0,smb->shd_fp)!=0) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%s padding header block", __FUNCTION__);
			return(SMB_ERR_WRITE); 			   /* pad block with NULL */
		}
		hdrlen++; 
	}
	return fflush(smb->shd_fp);	/* SMB_SUCCESS == 0 */
}

/****************************************************************************/
/* Initializes a message base's header (SMBHDR) record 						*/
/****************************************************************************/
int smb_initsmbhdr(smb_t* smb)
{
	smbhdr_t	hdr;

	if(smb->shd_fp==NULL || smb->sdt_fp==NULL || smb->sid_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s msgbase not open", __FUNCTION__);
		return(SMB_ERR_NOT_OPEN);
	}
	if(filelength(fileno(smb->shd_fp))>=(long)(sizeof(smbhdr_t)+sizeof(smbstatus_t))
		&& smb_locksmbhdr(smb)!=SMB_SUCCESS)  /* header exists, so lock it */
		return(SMB_ERR_LOCK);
	memset(&hdr,0,sizeof(smbhdr_t));
	memcpy(hdr.smbhdr_id,SMB_HEADER_ID,LEN_HEADER_ID);
	hdr.version=SMB_VERSION;
	hdr.length=sizeof(smbhdr_t)+sizeof(smbstatus_t);
	smb->status.last_msg=smb->status.total_msgs=0;
	smb->status.header_offset=sizeof(smbhdr_t)+sizeof(smbstatus_t);
	rewind(smb->shd_fp);
	if(!fwrite(&hdr,sizeof(smbhdr_t),1,smb->shd_fp))
		return SMB_ERR_WRITE;
	if(!fwrite(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp))
		return SMB_ERR_WRITE;

	return SMB_SUCCESS;
}

/****************************************************************************/
/* Creates a sub-board's initial header file                                */
/* Truncates and deletes other associated SMB files 						*/
/****************************************************************************/
int smb_create(smb_t* smb)
{
    char        str[MAX_PATH+1];
	FILE*		fp;
	int			retval;

	if((retval = smb_initsmbhdr(smb)) != SMB_SUCCESS)
		return retval;

	rewind(smb->shd_fp);
	chsize(fileno(smb->shd_fp),sizeof(smbhdr_t)+sizeof(smbstatus_t));
	fflush(smb->shd_fp);

	rewind(smb->sdt_fp);
	chsize(fileno(smb->sdt_fp),0L);
	rewind(smb->sid_fp);
	chsize(fileno(smb->sid_fp),0L);

	SAFEPRINTF(str,"%s.ini",smb->file);
	if((fp = fopen(str, "w")) != NULL) {
		fprintf(fp, "Created = 0x%lx\n", (long)time(NULL));
		fclose(fp);
	}
	SAFEPRINTF(str,"%s.sda",smb->file);
	(void)remove(str);						/* if it exists, delete it */
	SAFEPRINTF(str,"%s.sha",smb->file);
	(void)remove(str);                        /* if it exists, delete it */
	SAFEPRINTF(str,"%s.sch",smb->file);
	(void)remove(str);
	SAFEPRINTF(str,"%s.hash",smb->file);
	(void)remove(str);
	smb_unlocksmbhdr(smb);
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Returns number of data blocks required to store "length" amount of data  */
/****************************************************************************/
ulong smb_datblocks(off_t length)
{
	ulong blocks;

	blocks=(ulong)(length / SDT_BLOCK_LEN);
	if(length%SDT_BLOCK_LEN)
		blocks++;
	return(blocks);
}

/****************************************************************************/
/* Returns number of header blocks required to store "length" size header   */
/****************************************************************************/
ulong smb_hdrblocks(ulong length)
{
	ulong blocks;

	blocks=length/SHD_BLOCK_LEN;
	if(length%SHD_BLOCK_LEN)
		blocks++;
	return(blocks);
}

/****************************************************************************/
/* Returns difference from specified timezone and UTC/GMT (in minutes)		*/
/****************************************************************************/
int smb_tzutc(int16_t zone)
{
	int tz;

	if(OTHER_ZONE(zone))
		return(zone);

	tz=zone&0xfff;
	if(zone&(WESTERN_ZONE|US_ZONE)) {	/* West of UTC? */
		if(zone&DAYLIGHT)
			tz-=SMB_DST_OFFSET;			/* ToDo: Daylight Saving Time adjustment is *not* always +60 minutes */
		return(-tz);
	}
	if(zone&DAYLIGHT)
		tz+=SMB_DST_OFFSET;				/* ToDo: Daylight Saving Time adjustment is *not* always +60 minutes */
	return(tz);
}

/****************************************************************************/
/* The caller needs to call smb_unlockmsghdr(smb,remsg)						*/
/****************************************************************************/
int smb_updatethread(smb_t* smb, smbmsg_t* remsg, ulong newmsgnum)
{
	int			retval=SMB_ERR_NOT_FOUND;
	ulong		nextmsgnum;
	smbmsg_t	nextmsg;

 	if(!remsg->hdr.thread_first) {	/* New msg is first reply */
		if((remsg->idx_offset==0 || remsg->idx.offset==0)		/* index not read? */
			&& (retval=smb_getmsgidx(smb,remsg))!=SMB_SUCCESS)
			return(retval);
		if(!remsg->hdr.length) {	/* header not read? */
			if((retval=smb_lockmsghdr(smb,remsg))!=SMB_SUCCESS)
				return(retval);
			if((retval=smb_getmsghdr(smb,remsg))!=SMB_SUCCESS)
				return(retval);
		}
		remsg->hdr.thread_first=newmsgnum;
		remsg->idx.attr = (remsg->hdr.attr |= MSG_REPLIED);
		retval=smb_putmsg(smb,remsg);
		return(retval);
	}

	/* Search for last reply and extend chain */
	memset(&nextmsg,0,sizeof(nextmsg));
	nextmsgnum=remsg->hdr.thread_first;	/* start with first reply */

	while(nextmsgnum > nextmsg.hdr.number) {
		nextmsg.idx.offset=0;
		nextmsg.hdr.number=nextmsgnum;
		if(smb_getmsgidx(smb, &nextmsg)!=SMB_SUCCESS) /* invalid thread origin */
			break;
		if(smb_lockmsghdr(smb,&nextmsg)!=SMB_SUCCESS)
			break;
		if(smb_getmsghdr(smb, &nextmsg)!=SMB_SUCCESS) {
			smb_unlockmsghdr(smb,&nextmsg); 
			break;
		}
		if(nextmsg.hdr.thread_next && nextmsg.hdr.thread_next!=nextmsgnum) {
			nextmsgnum=nextmsg.hdr.thread_next;
			smb_unlockmsghdr(smb,&nextmsg);
			smb_freemsgmem(&nextmsg);
			continue; 
		}
		nextmsg.hdr.thread_next=newmsgnum;
		retval=smb_putmsghdr(smb,&nextmsg);
		smb_unlockmsghdr(smb,&nextmsg);
		smb_freemsgmem(&nextmsg);
		break; 
	}

	return(retval);
}

/* Find oldest *existing* message in the thread referenced by the passed msg */
uint32_t smb_first_in_thread(smb_t* smb, smbmsg_t* remsg, msghdr_t* hdr)
{
	smbmsg_t msg;

	if(!remsg->hdr.thread_back) {
		if(hdr != NULL)
			*hdr = remsg->hdr;
		return remsg->hdr.number;
	}

	memset(&msg, 0, sizeof(msg));
	if(remsg->hdr.thread_id != 0) {
		msg.hdr.number = remsg->hdr.thread_id;
		if(smb_getmsgidx(smb, &msg) == SMB_SUCCESS 
			&& smb_getmsghdr(smb, &msg) == SMB_SUCCESS) {
			smb_freemsgmem(&msg);
			if(hdr != NULL)
				*hdr = msg.hdr;
			return msg.hdr.number;
		}
	}
	/* Walk the thread backwards to find the oldest msg in thread */
	uint32_t msgnum = msg.hdr.number = remsg->hdr.number;
	msg.hdr.thread_back = remsg->hdr.thread_back;
	while(msg.hdr.thread_back != 0 && msg.hdr.thread_back < msg.hdr.number) {
		msg.hdr.number = msg.hdr.thread_back;
		if(smb_getmsgidx(smb, &msg) != SMB_SUCCESS)
			break;
		if(smb_getmsghdr(smb, &msg) != SMB_SUCCESS)
			break;
		smb_freemsgmem(&msg);
		msgnum = msg.hdr.number;
		if(hdr != NULL)
			*hdr = msg.hdr;
	}

	return msgnum;
}

uint32_t smb_next_in_thread(smb_t* smb, smbmsg_t* remsg, msghdr_t* hdr)
{
	smbmsg_t msg;

	memset(&msg, 0, sizeof(msg));
	msg.hdr.number = remsg->hdr.number;
	if(smb_getmsgidx(smb, &msg) != SMB_SUCCESS)
		return 0;
	if(smb_getmsghdr(smb, &msg) != SMB_SUCCESS)
		return 0;
	smb_freemsgmem(&msg);
	if(hdr != NULL)
		*hdr = msg.hdr;
	if(msg.hdr.thread_next <= msg.hdr.number)	/* Prevent circular thread */
		return 0;
	return msg.hdr.thread_next;
}

/* Last in this context does not mean newest */
uint32_t smb_last_in_branch(smb_t* smb, smbmsg_t* remsg)
{
	smbmsg_t msg;

	if(remsg->hdr.thread_first == 0)
		return remsg->hdr.number;
	memset(&msg, 0, sizeof(msg));
	msg.hdr.number = remsg->hdr.thread_first;

	uint32_t next;
	while((next = smb_next_in_thread(smb, &msg, &msg.hdr)) != 0)
		msg.hdr.number = next;

	return smb_last_in_branch(smb, &msg);
}


/* Last in this context does not mean newest */
uint32_t smb_last_in_thread(smb_t* smb, smbmsg_t* remsg)
{
	smbmsg_t msg;
	memset(&msg, 0, sizeof(msg));

	uint32_t first = smb_first_in_thread(smb, remsg, &msg.hdr);

	if(first <= 0)
		return remsg->hdr.number;

	return smb_last_in_branch(smb, &msg);
}

SMBEXPORT enum smb_msg_type smb_msg_type(smb_msg_attr_t attr)
{
	if(attr & MSG_FILE)
		return SMB_MSG_TYPE_FILE;
	switch (attr&MSG_POLL_VOTE_MASK) {
		case 0:
			return SMB_MSG_TYPE_NORMAL;
		case MSG_POLL:
			return SMB_MSG_TYPE_POLL;
		case MSG_POLL_CLOSURE:
			return SMB_MSG_TYPE_POLL_CLOSURE;
		default:
			return SMB_MSG_TYPE_BALLOT;
	}
}

// Return count of messages of the desired types (bit-mask), as read from index
// Does so as fast as possible, without locking
SMBEXPORT size_t smb_msg_count(smb_t* smb, unsigned types)
{
	off_t index_length = filelength(fileno(smb->sid_fp));
	if(index_length < sizeof(idxrec_t))
		return 0;

	uint32_t total = (uint32_t)(index_length / sizeof(idxrec_t));
	if(total < 1)
		return 0;

	idxrec_t*	idx;
	if((idx = calloc(total, sizeof(*idx))) == NULL)
		return 0;

	rewind(smb->sid_fp);
	size_t result = fread(idx, sizeof(*idx), total, smb->sid_fp);

	size_t count = 0;
	for(size_t i = 0; i < result; i++) {
		if(types & (1 << smb_msg_type(idx[i].attr)))
			count++;
	}
	free(idx);
	return count;
}

/* End of SMBLIB.C */
