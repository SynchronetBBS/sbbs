/* smblib.c */

/* Synchronet message base (SMB) library routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
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

/* ANSI C Library headers */

#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>		/* malloc */
#include <string.h>
#include <ctype.h>		/* isdigit */
#include <sys/types.h>
#include <sys/stat.h>	/* must come after sys/types.h */

/* SMB-specific headers */
#include "smblib.h"
#include "genwrap.h"
#include "filewrap.h"
#include "md5.h"
#include "crc16.h"
#include "crc32.h"

/* Use smb_ver() and smb_lib_ver() to obtain these values */
#define SMBLIB_VERSION		"2.40"      /* SMB library version */
#define SMB_VERSION 		0x0121		/* SMB format version */
										/* High byte major, low byte minor */

static char* nulstr="";

int SMBCALL smb_ver(void)
{
	return(SMB_VERSION);
}

char* SMBCALL smb_lib_ver(void)
{
	return(SMBLIB_VERSION);
}

#if !defined(__unix__)
/****************************************************************************/
/* Truncates white-space chars off end of 'str'								*/
/* Need for non-Unix version of STRERROR macro.								*/
/****************************************************************************/
static char* DLLCALL truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=' ') c--;
	str[c]=0;
	return(str);
}
#endif

/****************************************************************************/
/* Open a message base of name 'smb->file'                                  */
/* Opens files for READing messages or updating message indices only        */
/****************************************************************************/
int SMBCALL smb_open(smb_t* smb)
{
    int			file;
    char		path[MAX_PATH+1];
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

	/* Check for message-base lock semaphore file (under maintenance?) */
	while(smb_islocked(smb)) {
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time)
				return(SMB_ERR_TIMEOUT); 
		SLEEP(smb->retry_delay);
	}

	SAFEPRINTF(path,"%s.shd",smb->file);
	if((file=sopen(path,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,S_IREAD|S_IWRITE))==-1) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) opening %s"
			,errno,STRERROR(errno),path);
		return(SMB_ERR_OPEN); 
	}

	if((smb->shd_fp=fdopen(file,"r+b"))==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) fdopening %s (%d)"
			,errno,STRERROR(errno),path,file);
		close(file);
		return(SMB_ERR_OPEN); 
	}

	if(filelength(file)>=sizeof(smbhdr_t)) {
		setvbuf(smb->shd_fp,smb->shd_buf,_IONBF,SHD_BLOCK_LEN);
		if(smb_locksmbhdr(smb)!=SMB_SUCCESS) {
			smb_close(smb);
			/* smb_lockmsghdr set last_error */
			return(SMB_ERR_LOCK); 
		}
		memset(&hdr,0,sizeof(smbhdr_t));
		if(smb_fread(smb,&hdr,sizeof(smbhdr_t),smb->shd_fp)!=sizeof(smbhdr_t)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) reading header"
				,ferror(smb->shd_fp),STRERROR(ferror(smb->shd_fp)));
			smb_close(smb);
			return(SMB_ERR_READ);
		}
		if(memcmp(hdr.id,SMB_HEADER_ID,LEN_HEADER_ID)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"corrupt SMB header ID: %.*s"
				,LEN_HEADER_ID,hdr.id);
			smb_close(smb);
			return(SMB_ERR_HDR_ID); 
		}
		if(hdr.version<0x110) {         /* Compatibility check */
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"insufficient header version: %X"
				,hdr.version);
			smb_close(smb);
			return(SMB_ERR_HDR_VER); 
		}
		if(smb_fread(smb,&(smb->status),sizeof(smbstatus_t),smb->shd_fp)!=sizeof(smbstatus_t)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) reading status"
				,ferror(smb->shd_fp),STRERROR(ferror(smb->shd_fp)));
			smb_close(smb);
			return(SMB_ERR_READ); 
		}
		smb_unlocksmbhdr(smb);
		rewind(smb->shd_fp); 
	}

	setvbuf(smb->shd_fp,smb->shd_buf,_IOFBF,SHD_BLOCK_LEN);

	SAFEPRINTF(path,"%s.sdt",smb->file);
	if((file=sopen(path,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,S_IREAD|S_IWRITE))==-1) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) opening %s"
			,errno,STRERROR(errno),path);
		smb_close(smb);
		return(SMB_ERR_OPEN); 
	}

	if((smb->sdt_fp=fdopen(file,"r+b"))==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) fdopening %s (%d)"
			,errno,STRERROR(errno),path,file);
		close(file);
		smb_close(smb);
		return(SMB_ERR_OPEN);
	}

	setvbuf(smb->sdt_fp,NULL,_IOFBF,2*1024);

	SAFEPRINTF(path,"%s.sid",smb->file);
	if((file=sopen(path,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,S_IREAD|S_IWRITE))==-1) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) opening %s"
			,errno,STRERROR(errno),path);
		smb_close(smb);
		return(SMB_ERR_OPEN); 
	}

	if((smb->sid_fp=fdopen(file,"r+b"))==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) fdopening %s (%d)"
			,errno,STRERROR(errno),path,file);
		close(file);
		smb_close(smb);
		return(SMB_ERR_OPEN); 
	}

	setvbuf(smb->sid_fp,NULL,_IOFBF,2*1024);

	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Closes the currently open message base									*/
/****************************************************************************/
void SMBCALL smb_close(smb_t* smb)
{
	if(smb->shd_fp!=NULL) {
		smb_unlocksmbhdr(smb);		   /* In case it's been locked */
		smb_close_fp(&smb->shd_fp); 
	}
	smb_close_fp(&smb->sid_fp);
	smb_close_fp(&smb->sdt_fp);
	smb_close_fp(&smb->sid_fp);
	smb_close_fp(&smb->sda_fp);
	smb_close_fp(&smb->sha_fp);
	smb_close_fp(&smb->hash_fp);
}

/****************************************************************************/
/* Opens a non-shareable file (FILE*) associated with a message base		*/
/* Retrys for retry_time number of seconds									*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int SMBCALL smb_open_fp(smb_t* smb, FILE** fp)
{
	int 	file;
	char	path[MAX_PATH+1];
	char*	ext;
	time_t	start=0;

	if(fp==&smb->sda_fp)
		ext="sda";
	else if(fp==&smb->sha_fp)
		ext="sha";
	else if(fp==&smb->hash_fp)
		ext="hash";
	else {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"opening %s: Illegal FILE* pointer argument: %p"
			,smb->file, fp);
		return(SMB_ERR_OPEN);
	}
	SAFEPRINTF2(path,"%s.%s",smb->file,ext);

	if(*fp!=NULL)	/* Already open! */
		return(SMB_SUCCESS);

	while(1) {
		if((file=sopen(path,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW,S_IREAD|S_IWRITE))!=-1)
			break;
		if(errno!=EACCES && errno!=EAGAIN) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) opening %s"
				,errno,STRERROR(errno),path);
			return(SMB_ERR_OPEN);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"timeout opening %s (retry_time=%ld)"
					,path,smb->retry_time);
				return(SMB_ERR_TIMEOUT); 
			}
		SLEEP(smb->retry_delay);
	}
	if((*fp=fdopen(file,"r+b"))==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) fdopening %s (%d)"
			,errno,STRERROR(errno),path,file);
		close(file);
		return(SMB_ERR_OPEN); 
	}
	setvbuf(*fp,NULL,_IOFBF,2*1024);
	return(SMB_SUCCESS);
}

void SMBCALL smb_close_fp(FILE** fp)
{
	if(fp!=NULL) {
		if(*fp!=NULL)
			fclose(*fp);
		*fp=NULL;
	}
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
int SMBCALL smb_lock(smb_t* smb)
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
					,"%d (%s) creating %s"
					,errno,STRERROR(errno),path);
				return(SMB_ERR_LOCK);
			}
		SLEEP(smb->retry_delay);
	}
	close(file);
	return(SMB_SUCCESS);
}

int SMBCALL smb_unlock(smb_t* smb)
{
	char	path[MAX_PATH+1];

	smb_lockfname(smb,path,sizeof(path)-1);
	if(remove(path)!=0) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) removing %s"
			,errno,STRERROR(errno),path);
		return(SMB_ERR_DELETE);
	}
	return(SMB_SUCCESS);
}

BOOL SMBCALL smb_islocked(smb_t* smb)
{
	char	path[MAX_PATH+1];

	if(access(smb_lockfname(smb,path,sizeof(path)-1),0)!=0)
		return(FALSE);
	safe_snprintf(smb->last_error,sizeof(smb->last_error),"%s exists",path);
	return(TRUE);
}

/****************************************************************************/
/* If the parameter 'push' is non-zero, this function stores the currently  */
/* open message base to the "virtual" smb stack. Up to SMB_STACK_LEN        */
/* message bases may be stored (defined in SMBDEFS.H).						*/
/* The parameter 'op' is the operation to perform on the stack. Either      */
/* SMB_STACK_PUSH, SMB_STACK_POP, or SMB_STACK_XCHNG						*/
/* If the operation is SMB_STACK_POP, this function restores a message base */
/* previously saved with a SMB_STACK_PUSH call to this same function.		*/
/* If the operation is SMB_STACK_XCHNG, then the current message base is	*/
/* exchanged with the message base on the top of the stack (most recently	*/
/* pushed.																	*/
/* If the current message base is not open, the SMB_STACK_PUSH and			*/
/* SMB_STACK_XCHNG operations do nothing									*/
/* Returns 0 on success, non-zero if stack full.                            */
/* If operation is SMB_STACK_POP or SMB_STACK_XCHNG, it always returns 0.	*/
/****************************************************************************/
int SMBCALL smb_stack(smb_t* smb, int op)
{
	static smb_t stack[SMB_STACK_LEN];
	static int	stack_idx;
	smb_t		tmp_smb;

	if(op==SMB_STACK_PUSH) {
		if(stack_idx>=SMB_STACK_LEN) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error),"SMB stack overflow");
			return(SMB_FAILURE);
		}
		if(smb->shd_fp==NULL || smb->sdt_fp==NULL || smb->sid_fp==NULL)
			return(SMB_SUCCESS);	  /* Msg base not open, do nothing */
		memcpy(&stack[stack_idx],smb,sizeof(smb_t));
		stack_idx++;
		return(SMB_SUCCESS); 
	}
	/* pop or xchng */
	if(!stack_idx)	/* Nothing on the stack, so do nothing */
		return(SMB_SUCCESS);
	if(op==SMB_STACK_XCHNG) {
		if(smb->shd_fp==NULL)
			return(SMB_SUCCESS);
		memcpy(&tmp_smb,smb,sizeof(smb_t));
	}

	stack_idx--;
	memcpy(smb,&stack[stack_idx],sizeof(smb_t));
	if(op==SMB_STACK_XCHNG) {
		memcpy(&stack[stack_idx],&tmp_smb,sizeof(smb_t));
		stack_idx++;
	}
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Truncates header file													*/
/* Retrys for smb.retry_time number of seconds								*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int SMBCALL smb_trunchdr(smb_t* smb)
{
	time_t	start=0;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	rewind(smb->shd_fp);
	while(1) {
		if(!chsize(fileno(smb->shd_fp),0L))
			break;
		if(errno!=EACCES && errno!=EAGAIN) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) changing header file size"
				,errno,STRERROR(errno));
			return(SMB_ERR_WRITE);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {	 /* Time-out */
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"timeout changing header file size (retry_time=%ld)"
					,smb->retry_time);
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
int SMBCALL smb_locksmbhdr(smb_t* smb)
{
	time_t	start=0;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
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
		SLEEP(smb->retry_delay);
	}
	safe_snprintf(smb->last_error,sizeof(smb->last_error),"timeout locking header");
	return(SMB_ERR_TIMEOUT);
}

/****************************************************************************/
/* Read the SMB header from the header file and place into smb.status		*/
/****************************************************************************/
int SMBCALL smb_getstatus(smb_t* smb)
{
	int 	i;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	setvbuf(smb->shd_fp,smb->shd_buf,_IONBF,SHD_BLOCK_LEN);
	clearerr(smb->shd_fp);
	if(fseek(smb->shd_fp,sizeof(smbhdr_t),SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) seeking to %u in header file"
			,errno,STRERROR(errno),sizeof(smbhdr_t));
		return(SMB_ERR_SEEK);
	}
	i=smb_fread(smb,&(smb->status),sizeof(smbstatus_t),smb->shd_fp);
	setvbuf(smb->shd_fp,smb->shd_buf,_IOFBF,SHD_BLOCK_LEN);
	if(i==sizeof(smbstatus_t))
		return(SMB_SUCCESS);
	safe_snprintf(smb->last_error,sizeof(smb->last_error)
		,"%d (%s) reading status",ferror(smb->shd_fp),STRERROR(ferror(smb->shd_fp)));
	return(SMB_ERR_READ);
}

/****************************************************************************/
/* Writes message base header												*/
/****************************************************************************/
int SMBCALL smb_putstatus(smb_t* smb)
{
	int i;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->shd_fp);
	if(fseek(smb->shd_fp,sizeof(smbhdr_t),SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) seeking to %u in header file"
			,errno,STRERROR(errno),sizeof(smbhdr_t));
		return(SMB_ERR_SEEK);
	}
	i=fwrite(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp);
	fflush(smb->shd_fp);
	if(i==sizeof(smbstatus_t))
		return(SMB_SUCCESS);
	safe_snprintf(smb->last_error,sizeof(smb->last_error)
		,"%d (%s) writing status",ferror(smb->shd_fp),STRERROR(ferror(smb->shd_fp)));
	return(SMB_ERR_WRITE);
}

/****************************************************************************/
/* Unlocks previously locks message base header 							*/
/****************************************************************************/
int SMBCALL smb_unlocksmbhdr(smb_t* smb)
{
	int result;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	result = unlock(fileno(smb->shd_fp),0L,sizeof(smbhdr_t)+sizeof(smbstatus_t));
	if(result==0)
		smb->locked=FALSE;
	return(result);
}

/********************************/
/* Individual Message Functions */
/********************************/

/****************************************************************************/
/* Is the offset a valid message header offset?								*/
/****************************************************************************/
static BOOL smb_valid_hdr_offset(smb_t* smb, ulong offset)
{
	if(offset<sizeof(smbhdr_t)+sizeof(smbstatus_t) 
		|| offset<smb->status.header_offset) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"invalid header offset: %lu (0x%lX)"
			,offset,offset);
		return(FALSE);
	}
	return(TRUE);
}

/****************************************************************************/
/* Attempts for smb.retry_time number of seconds to lock the hdr for 'msg'  */
/****************************************************************************/
int SMBCALL smb_lockmsghdr(smb_t* smb, smbmsg_t* msg)
{
	time_t	start=0;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	if(!smb_valid_hdr_offset(smb,msg->idx.offset))
		return(SMB_ERR_HDR_OFFSET);

	while(1) {
		if(!lock(fileno(smb->shd_fp),msg->idx.offset,sizeof(msghdr_t)))
			return(SMB_SUCCESS);
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) 
				break;
		/* In case we've already locked it */
		unlock(fileno(smb->shd_fp),msg->idx.offset,sizeof(msghdr_t)); 
		SLEEP(smb->retry_delay);
	}
	safe_snprintf(smb->last_error,sizeof(smb->last_error),"timeout locking header");
	return(SMB_ERR_TIMEOUT);
}

/****************************************************************************/
/* Fills msg->idx with message index based on msg->hdr.number				*/
/* OR if msg->hdr.number is 0, based on msg->offset (record offset).		*/
/* if msg.hdr.number does not equal 0, then msg->offset is filled too.		*/
/* Either msg->hdr.number or msg->offset must be initialized before 		*/
/* calling this function													*/
/* Returns 1 if message number wasn't found, 0 if it was                    */
/****************************************************************************/
int SMBCALL smb_getmsgidx(smb_t* smb, smbmsg_t* msg)
{
	idxrec_t idx;
	ulong	 l,length,total,bot,top;

	if(smb->sid_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"index not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sid_fp);

	length=filelength(fileno(smb->sid_fp));
	if(length<sizeof(idxrec_t)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"invalid index file length: %ld",length);
		return(SMB_ERR_FILE_LEN);
	}
	total=length/sizeof(idxrec_t);
	if(!total) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"invalid index file length: %ld",length);
		return(SMB_ERR_FILE_LEN);
	}

	if(!msg->hdr.number) {
		if(msg->offset*sizeof(idxrec_t)>=length) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"invalid index offset: %lu, byte offset: %lu, length: %lu"
				,msg->offset, msg->offset*sizeof(idxrec_t), length);
			return(SMB_ERR_HDR_OFFSET);
		}
		if(fseek(smb->sid_fp,msg->offset*sizeof(idxrec_t),SEEK_SET)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) seeking to %lu in index file"
				,errno,STRERROR(errno)
				,msg->offset*sizeof(idxrec_t));
			return(SMB_ERR_SEEK);
		}
		if(smb_fread(smb,&msg->idx,sizeof(idxrec_t),smb->sid_fp)!=sizeof(idxrec_t)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) reading index at offset %lu (byte %lu)"
				,ferror(smb->sid_fp),STRERROR(ferror(smb->sid_fp))
				,msg->offset,msg->offset*sizeof(idxrec_t));
			return(SMB_ERR_READ);
		}
		return(SMB_SUCCESS); 
	}

	bot=0;
	top=total;
	l=total/2; /* Start at middle index */
	while(1) {
		if(l>=total) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error),"msg %lu not found"
				,msg->hdr.number);
			return(SMB_ERR_NOT_FOUND);
		}
		if(fseek(smb->sid_fp,l*sizeof(idxrec_t),SEEK_SET)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) seeking to offset %lu (byte %lu) in index file"
				,errno,STRERROR(errno)
				,l,l*sizeof(idxrec_t));
			return(SMB_ERR_SEEK);
		}
		if(smb_fread(smb,&idx,sizeof(idxrec_t),smb->sid_fp)!=sizeof(idxrec_t)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) reading index at offset %lu (byte %lu)"
				,ferror(smb->sid_fp),STRERROR(ferror(smb->sid_fp)),l,l*sizeof(idxrec_t));
			return(SMB_ERR_READ);
		}
		if(bot==top-1 && idx.number!=msg->hdr.number) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error),"msg %lu not found"
				,msg->hdr.number);
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
	msg->offset=l;
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Reads the first index record in the open message base 					*/
/****************************************************************************/
int SMBCALL smb_getfirstidx(smb_t* smb, idxrec_t *idx)
{
	if(smb->sid_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"index not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sid_fp);
	if(fseek(smb->sid_fp,0,SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) seeking to beginning of index file"
			,errno,STRERROR(errno));
		return(SMB_ERR_SEEK);
	}
	if(smb_fread(smb,idx,sizeof(idxrec_t),smb->sid_fp)!=sizeof(idxrec_t)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) reading first index"
			,ferror(smb->sid_fp),STRERROR(ferror(smb->sid_fp)));
		return(SMB_ERR_READ);
	}
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Reads the last index record in the open message base 					*/
/****************************************************************************/
int SMBCALL smb_getlastidx(smb_t* smb, idxrec_t *idx)
{
	long length;

	if(smb->sid_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"index not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sid_fp);
	length=filelength(fileno(smb->sid_fp));
	if(length<sizeof(idxrec_t)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"invalid index file length: %ld",length);
		return(SMB_ERR_FILE_LEN);
	}
	if(fseek(smb->sid_fp,length-sizeof(idxrec_t),SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) seeking to %u in index file"
			,errno,STRERROR(errno)
			,(unsigned)(length-sizeof(idxrec_t)));
		return(SMB_ERR_SEEK);
	}
	if(smb_fread(smb,idx,sizeof(idxrec_t),smb->sid_fp)!=sizeof(idxrec_t)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) reading last index"
			,ferror(smb->sid_fp),STRERROR(ferror(smb->sid_fp)));
		return(SMB_ERR_READ);
	}
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Figures out the total length of the header record for 'msg'              */
/* Returns length 															*/
/****************************************************************************/
ulong SMBCALL smb_getmsghdrlen(smbmsg_t* msg)
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
ulong SMBCALL smb_getmsgdatlen(smbmsg_t* msg)
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
ulong SMBCALL smb_getmsgtxtlen(smbmsg_t* msg)
{
	int i;
	ulong length=0L;

	for(i=0;i<msg->total_hfields;i++)
		if(msg->hfield[i].type==SMB_COMMENT || msg->hfield[i].type==SMTPSYSMSG)
			length+=msg->hfield[i].length+2;
	for(i=0;i<msg->hdr.total_dfields;i++)
		if(msg->dfield[i].type==TEXT_BODY || msg->dfield[i].type==TEXT_TAIL)
			length+=msg->dfield[i].length;
	return(length);
}

static void set_convenience_ptr(smbmsg_t* msg, ushort hfield_type, void* hfield_dat)
{
	switch(hfield_type) {	/* convenience variables */
		case SENDER:
			if(!msg->from) {
				msg->from=(char*)hfield_dat;
				break; 
			}
		case FORWARDED: 	/* fall through */
			msg->forwarded=TRUE;
			break;
		case SENDERAGENT:
			if(!msg->forwarded)
				msg->from_agent=*(ushort *)hfield_dat;
			break;
		case SENDEREXT:
			if(!msg->forwarded)
				msg->from_ext=(char*)hfield_dat;
			break;
		case SENDERORG:
			if(!msg->forwarded)
				msg->from_org=(char*)hfield_dat;
			break;
		case SENDERNETTYPE:
			if(!msg->forwarded)
				msg->from_net.type=*(ushort *)hfield_dat;
			break;
		case SENDERNETADDR:
			if(!msg->forwarded)
				msg->from_net.addr=(char*)hfield_dat;
			break;
		case REPLYTO:
			msg->replyto=(char*)hfield_dat;
			break;
		case REPLYTOEXT:
			msg->replyto_ext=(char*)hfield_dat;
			break;
		case REPLYTOAGENT:
			msg->replyto_agent=*(ushort *)hfield_dat;
			break;
		case REPLYTONETTYPE:
			msg->replyto_net.type=*(ushort *)hfield_dat;
			break;
		case REPLYTONETADDR:
			msg->replyto_net.addr=(char*)hfield_dat;
			break;
		case RECIPIENT:
			msg->to=(char*)hfield_dat;
			break;
		case RECIPIENTEXT:
			msg->to_ext=(char*)hfield_dat;
			break;
		case RECIPIENTAGENT:
			msg->to_agent=*(ushort *)hfield_dat;
			break;
		case RECIPIENTNETTYPE:
			msg->to_net.type=*(ushort *)hfield_dat;
			break;
		case RECIPIENTNETADDR:
			msg->to_net.addr=(char*)hfield_dat;
			break;
		case SUBJECT:
			msg->subj=(char*)hfield_dat;
			break;
		case SMB_SUMMARY:
			msg->summary=(char*)hfield_dat;
			break;
		case SMB_EXPIRATION:
			msg->expiration=*(time_t*)hfield_dat;
			break;
		case SMB_PRIORITY:
			msg->priority=*(ulong*)hfield_dat;
			break;
		case SMB_COST:
			msg->cost=*(ulong*)hfield_dat;
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
	}
}

static void clear_convenience_ptrs(smbmsg_t* msg)
{
	msg->from=NULL;
	msg->from_ext=NULL;
	msg->from_org=NULL;
	memset(&msg->from_net,0,sizeof(net_t));

	msg->replyto=NULL;
	msg->replyto_ext=NULL;
	memset(&msg->replyto_net,0,sizeof(net_t));

	msg->to=NULL;
	msg->to_ext=NULL;
	memset(&msg->to_net,0,sizeof(net_t));

	msg->subj=NULL;
	msg->summary=NULL;
	msg->id=NULL;
	msg->reply_id=NULL;
	msg->reverse_path=NULL;
	msg->path=NULL;
	msg->newsgroups=NULL;

	msg->ftn_msgid=NULL;
	msg->ftn_reply=NULL;
	msg->ftn_area=NULL;
	msg->ftn_pid=NULL;
	msg->ftn_tid=NULL;
	msg->ftn_flags=NULL;
}

/****************************************************************************/
/* Read header information into 'msg' structure                             */
/* msg->idx.offset must be set before calling this function 				*/
/* Must call smb_freemsgmem() to free memory allocated for var len strs 	*/
/* Returns 0 on success, non-zero if error									*/
/****************************************************************************/
int SMBCALL smb_getmsghdr(smb_t* smb, smbmsg_t* msg)
{
	void	*vp,**vpp;
	ushort	i;
	ulong	l,offset;
	idxrec_t idx;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}

	if(!smb_valid_hdr_offset(smb,msg->idx.offset))
		return(SMB_ERR_HDR_OFFSET);

	rewind(smb->shd_fp);
	if(fseek(smb->shd_fp,msg->idx.offset,SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) seeking to %lu in header"
			,errno,STRERROR(errno)
			,msg->idx.offset);
		return(SMB_ERR_SEEK);
	}

	idx=msg->idx;
	offset=msg->offset;
	memset(msg,0,sizeof(smbmsg_t));
	msg->idx=idx;
	msg->offset=offset;
	if(smb_fread(smb,&msg->hdr,sizeof(msghdr_t),smb->shd_fp)!=sizeof(msghdr_t)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) reading msg header"
			,ferror(smb->shd_fp),STRERROR(ferror(smb->shd_fp)));
		return(SMB_ERR_READ);
	}
	if(memcmp(msg->hdr.id,SHD_HEADER_ID,LEN_HEADER_ID)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"corrupt message header ID: %.*s at offset %lu"
			,LEN_HEADER_ID,msg->hdr.id,msg->idx.offset);
		return(SMB_ERR_HDR_ID);
	}
	if(msg->hdr.version<0x110) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"insufficient header version: %X"
			,msg->hdr.version);
		return(SMB_ERR_HDR_VER);
	}
	l=sizeof(msghdr_t);
	if(msg->hdr.total_dfields && (msg->dfield
		=(dfield_t *)MALLOC(sizeof(dfield_t)*msg->hdr.total_dfields))==NULL) {
		smb_freemsgmem(msg);
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"malloc failure of %d bytes for %d data fields"
			,(int)sizeof(dfield_t)*msg->hdr.total_dfields, msg->hdr.total_dfields);
		return(SMB_ERR_MEM); 
	}
	i=0;
	while(i<msg->hdr.total_dfields && l<msg->hdr.length) {
		if(smb_fread(smb,&msg->dfield[i],sizeof(dfield_t),smb->shd_fp)!=sizeof(dfield_t)) {
			smb_freemsgmem(msg);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) reading data field %d"
				,ferror(smb->shd_fp),STRERROR(ferror(smb->shd_fp)),i);
			return(SMB_ERR_READ); 
		}
		i++;
		l+=sizeof(dfield_t); 
	}
	if(i<msg->hdr.total_dfields) {
		smb_freemsgmem(msg);
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"insufficient data fields read (%d instead of %d)"
			,i,msg->hdr.total_dfields);
		return(SMB_ERR_READ); 
	}
	while(l<msg->hdr.length) {
		i=msg->total_hfields;
		if((vpp=(void* *)REALLOC(msg->hfield_dat,sizeof(void* )*(i+1)))==NULL) {
			smb_freemsgmem(msg);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"realloc failure of %d bytes for header field data"
				,(int)sizeof(void*)*(i+1));
			return(SMB_ERR_MEM); 
		}
		msg->hfield_dat=vpp;
		if((vp=(hfield_t *)REALLOC(msg->hfield,sizeof(hfield_t)*(i+1)))==NULL) {
			smb_freemsgmem(msg);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"realloc failure of %d bytes for header fields"
				,(int)sizeof(hfield_t)*(i+1));
			return(SMB_ERR_MEM); 
		}
		msg->hfield=vp;
		msg->total_hfields++;
		if(smb_fread(smb,&msg->hfield[i],sizeof(hfield_t),smb->shd_fp)!=sizeof(hfield_t)) {
			smb_freemsgmem(msg);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) reading header field"
				,ferror(smb->shd_fp),STRERROR(ferror(smb->shd_fp)));
			return(SMB_ERR_READ); 
		}
		l+=sizeof(hfield_t);
		if((msg->hfield_dat[i]=(char*)MALLOC(msg->hfield[i].length+1))
			==NULL) {			/* Allocate 1 extra for ASCIIZ terminator */
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"malloc failure of %d bytes for header field %d"
				,msg->hfield[i].length+1, i);
			smb_freemsgmem(msg);  /* or 0 length field */
			return(SMB_ERR_MEM); 
		}
		memset(msg->hfield_dat[i],0,msg->hfield[i].length+1);  /* init to NULL */
		if(msg->hfield[i].length
			&& smb_fread(smb,msg->hfield_dat[i],msg->hfield[i].length,smb->shd_fp)!=msg->hfield[i].length) {
			smb_freemsgmem(msg);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) reading header field data"
				,ferror(smb->shd_fp),STRERROR(ferror(smb->shd_fp)));
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
	if(msg->reverse_path == NULL)
		msg->reverse_path = msg->from_net.addr;

	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Frees memory allocated for variable-length header fields in 'msg'        */
/****************************************************************************/
void SMBCALL smb_freemsghdrmem(smbmsg_t* msg)
{
	ushort	i;

	for(i=0;i<msg->total_hfields;i++)
		if(msg->hfield_dat[i]) {
			FREE(msg->hfield_dat[i]);
			msg->hfield_dat[i]=NULL;
		}
	msg->total_hfields=0;
	if(msg->hfield) {
		FREE(msg->hfield);
		msg->hfield=NULL;
	}
	if(msg->hfield_dat) {
		FREE(msg->hfield_dat);
		msg->hfield_dat=NULL;
	}
	clear_convenience_ptrs(msg);	/* don't leave pointers to freed memory */
}

/****************************************************************************/
/* Frees memory allocated for 'msg'                                         */
/****************************************************************************/
void SMBCALL smb_freemsgmem(smbmsg_t* msg)
{
	if(msg->dfield) {
		FREE(msg->dfield);
		msg->dfield=NULL;
	}
	msg->hdr.total_dfields=0;
	smb_freemsghdrmem(msg);
}

/****************************************************************************/
/* Copies memory allocated for 'srcmsg' to 'msg'							*/
/****************************************************************************/
int SMBCALL smb_copymsgmem(smb_t* smb, smbmsg_t* msg, smbmsg_t* srcmsg)
{
	int i;

	memcpy(msg,srcmsg,sizeof(smbmsg_t));

	/* data field types/lengths */
	if(msg->hdr.total_dfields>0) {
		if((msg->dfield=(dfield_t *)MALLOC(msg->hdr.total_dfields*sizeof(dfield_t)))==NULL) {
			if(smb!=NULL)
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"malloc failure of %d bytes for %d data fields"
					,msg->hdr.total_dfields*sizeof(dfield_t), msg->hdr.total_dfields);
			return(SMB_ERR_MEM);
		}
		memcpy(msg->dfield,srcmsg->dfield,msg->hdr.total_dfields*sizeof(dfield_t));
	}

	/* header field types/lengths */
	if(msg->total_hfields>0) {
		if((msg->hfield=(hfield_t *)MALLOC(msg->total_hfields*sizeof(hfield_t)))==NULL) {
			if(smb!=NULL)
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"malloc failure of %d bytes for %d header fields"
					,msg->total_hfields*sizeof(hfield_t), msg->total_hfields);
			return(SMB_ERR_MEM);
		}
		memcpy(msg->hfield,srcmsg->hfield,msg->total_hfields*sizeof(hfield_t));

		/* header field data */
		if((msg->hfield_dat=(void**)MALLOC(msg->total_hfields*sizeof(void*)))==NULL) {
			if(smb!=NULL)
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"malloc failure of %d bytes for %d header fields"
					,msg->total_hfields*sizeof(void*), msg->total_hfields);
			return(SMB_ERR_MEM);
		}

		for(i=0;i<msg->total_hfields;i++) {
			if((msg->hfield_dat[i]=(void*)MALLOC(msg->hfield[i].length+1))==NULL) {
				if(smb!=NULL)
					safe_snprintf(smb->last_error,sizeof(smb->last_error)
						,"malloc failure of %d bytes for header field #%d"
						,msg->hfield[i].length+1, i+1);
				return(SMB_ERR_MEM);
			}
			memset(msg->hfield_dat[i],0,msg->hfield[i].length+1);
			memcpy(msg->hfield_dat[i],srcmsg->hfield_dat[i],msg->hfield[i].length);
		}
	}

	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Unlocks header for 'msg'                                                 */
/****************************************************************************/
int SMBCALL smb_unlockmsghdr(smb_t* smb, smbmsg_t* msg)
{
	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	if(!smb_valid_hdr_offset(smb,msg->idx.offset))
		return(SMB_ERR_HDR_OFFSET);
	return(unlock(fileno(smb->shd_fp),msg->idx.offset,sizeof(msghdr_t)));
}


/****************************************************************************/
/* Adds a header field to the 'msg' structure (in memory only)              */
/****************************************************************************/
int SMBCALL smb_hfield(smbmsg_t* msg, ushort type, size_t length, void* data)
{
	void**		vpp;
	hfield_t*	hp;
	int i;

	if(smb_getmsghdrlen(msg)+sizeof(hfield_t)+length>SMB_MAX_HDR_LEN)
		return(SMB_ERR_HDR_LEN);

	i=msg->total_hfields;
	if((hp=(hfield_t *)REALLOC(msg->hfield,sizeof(hfield_t)*(i+1)))==NULL) 
		return(SMB_ERR_MEM);

	msg->hfield=hp;
	if((vpp=(void* *)REALLOC(msg->hfield_dat,sizeof(void* )*(i+1)))==NULL) 
		return(SMB_ERR_MEM);
	
	msg->hfield_dat=vpp;
	msg->total_hfields++;
	msg->hfield[i].type=type;
	msg->hfield[i].length=length;
	if(length) {
		if((msg->hfield_dat[i]=(void* )MALLOC(length+1))==NULL) 
			return(SMB_ERR_MEM);	/* Allocate 1 extra for ASCIIZ terminator */
		memset(msg->hfield_dat[i],0,length+1);
		memcpy(msg->hfield_dat[i],data,length); 
		set_convenience_ptr(msg,type,msg->hfield_dat[i]);
	}
	else
		msg->hfield_dat[i]=NULL;

	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Adds a list of header fields to the 'msg' structure (in memory only)     */
/****************************************************************************/
int	SMBCALL smb_hfield_addlist(smbmsg_t* msg, hfield_t** hfield_list, void** hfield_dat)
{
	int			retval;
	unsigned	n;

	if(hfield_list==NULL)
		return(SMB_FAILURE);

	for(n=0;hfield_list[n]!=NULL;n++)
		if((retval=smb_hfield(msg
			,hfield_list[n]->type,hfield_list[n]->length,hfield_dat[n]))!=SMB_SUCCESS)
			return(retval);

	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Convenience function to add an ASCIIZ string header field				*/
/****************************************************************************/
int SMBCALL smb_hfield_str(smbmsg_t* msg, ushort type, const char* str)
{
	return smb_hfield(msg, type, strlen(str), (void*)str);
}

/****************************************************************************/
/* Appends data to an existing header field (in memory only)				*/
/****************************************************************************/
int SMBCALL smb_hfield_append(smbmsg_t* msg, ushort type, size_t length, void* data)
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

	if((p=(BYTE*)REALLOC(msg->hfield_dat[i],msg->hfield[i].length+length+1))==NULL) 
		return(SMB_ERR_MEM);	/* Allocate 1 extra for ASCIIZ terminator */

	msg->hfield_dat[i]=p;
	p+=msg->hfield[i].length;	/* skip existing data */
	memset(p,0,length+1);
	memcpy(p,data,length);		/* append */
	msg->hfield[i].length+=length;
	set_convenience_ptr(msg,type,msg->hfield_dat[i]);

	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Appends data to an existing ASCIIZ header field (in memory only)			*/
/****************************************************************************/
int SMBCALL smb_hfield_append_str(smbmsg_t* msg, ushort type, const char* str)
{
	return smb_hfield_append(msg, type, strlen(str), (void*)str);
}

/****************************************************************************/
/* Searches for a specific header field (by type) and returns it			*/
/****************************************************************************/
void* SMBCALL smb_get_hfield(smbmsg_t* msg, ushort type, hfield_t* hfield)
{
	int i;

	for(i=0;i<msg->total_hfields;i++)
		if(msg->hfield[i].type == type) {
			if(hfield != NULL)
				hfield = &msg->hfield[i];
			return(msg->hfield_dat[i]);
		}

	return(NULL);
}

/****************************************************************************/
/* Adds a data field to the 'msg' structure (in memory only)                */
/* Automatically figures out the offset into the data buffer from existing	*/
/* dfield lengths															*/
/****************************************************************************/
int SMBCALL smb_dfield(smbmsg_t* msg, ushort type, ulong length)
{
	dfield_t* dp;
	int i,j;

	i=msg->hdr.total_dfields;
	if((dp=(dfield_t *)REALLOC(msg->dfield,sizeof(dfield_t)*(i+1)))==NULL) 
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
/* Checks CRC history file for duplicate crc. If found, returns 1.			*/
/* If no dupe, adds to CRC history and returns 0, or negative if error. 	*/
/****************************************************************************/
int SMBCALL smb_addcrc(smb_t* smb, ulong crc)
{
	char	str[MAX_PATH+1];
	int 	file;
	int		wr;
	long	length;
	long	newlen;
	ulong	l,*buf;
	time_t	start=0;

	if(!smb->status.max_crcs)
		return(SMB_SUCCESS);

	SAFEPRINTF(str,"%s.sch",smb->file);
	while(1) {
		if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW,S_IREAD|S_IWRITE))!=-1)
			break;
		if(errno!=EACCES && errno!=EAGAIN) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) opening %s"
				,errno,STRERROR(errno),str);
			return(SMB_ERR_OPEN);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {
				safe_snprintf(smb->last_error,sizeof(smb->last_error)
					,"timeout opening %s (retry_time=%ld)"
					,str,smb->retry_time);
				return(SMB_ERR_TIMEOUT); 
			}
		SLEEP(smb->retry_delay);
	}

	length=filelength(file);
	if(length<0L || length%sizeof(long)) {
		close(file);
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"invalid file length: %ld", length);
		return(SMB_ERR_FILE_LEN); 
	}

	if(length!=0) {
		if((buf=(ulong*)MALLOC(length))==NULL) {
			close(file);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"malloc failure of %ld bytes"
				,length);
			return(SMB_ERR_MEM); 
		}

		if(read(file,buf,length)!=length) {
			close(file);
			FREE(buf);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) reading %ld bytes"
				,errno,STRERROR(errno),length);
			return(SMB_ERR_READ);
		}

		for(l=0;l<length/sizeof(long);l++)
			if(crc==buf[l])
				break;
		if(l<length/sizeof(long)) {					/* Dupe CRC found */
			close(file);
			FREE(buf);
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"duplicate message detected");
			return(SMB_DUPE_MSG);
		} 

		if(length>=(long)(smb->status.max_crcs*sizeof(long))) {
			newlen=(smb->status.max_crcs-1)*sizeof(long);
			chsize(file,0);	/* truncate it */
			lseek(file,0L,SEEK_SET);
			write(file,buf+(length-newlen),newlen); 
		}
		FREE(buf);
	}
	wr=write(file,&crc,sizeof(crc));	/* Write to the end */
	close(file);

	if(wr!=sizeof(crc)) {	
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) writing %u bytes"
			,errno,STRERROR(errno),sizeof(crc));
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
int SMBCALL smb_addmsghdr(smb_t* smb, smbmsg_t* msg, int storage)
{
	int		i;
	long	l;
	ulong	hdrlen;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}

	if(!smb->locked && smb_locksmbhdr(smb)!=SMB_SUCCESS)
		return(SMB_ERR_LOCK);

	hdrlen=smb_getmsghdrlen(msg);
	if(hdrlen>SMB_MAX_HDR_LEN) { /* headers are limited to 64k in size */
		smb_unlocksmbhdr(smb);
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"illegal message header length (%lu > %u)"
			,hdrlen,SMB_MAX_HDR_LEN);
		return(SMB_ERR_HDR_LEN);
	}

	if(!(msg->flags&MSG_FLAG_HASHED) /* not already hashed */
		&& (i=smb_hashmsg(smb,msg,NULL,FALSE))!=SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return(i);	/* Duplicate message? */
	}

	if((i=smb_getstatus(smb))!=SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return(i);
	}

	if(storage!=SMB_HYPERALLOC && (i=smb_open_ha(smb))!=SMB_SUCCESS) {
		smb_unlocksmbhdr(smb);
		return(i);
	}

	msg->hdr.length=(ushort)hdrlen;
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
		return(l); 
	}

	msg->idx.number=msg->hdr.number=smb->status.last_msg+1;
	msg->idx.offset=smb->status.header_offset+l;
	msg->idx.time=msg->hdr.when_imported.time;
	msg->idx.attr=msg->hdr.attr;
	msg->offset=smb->status.total_msgs;
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
/* Writes both header and index information for msg 'msg'                   */
/****************************************************************************/
int SMBCALL smb_putmsg(smb_t* smb, smbmsg_t* msg)
{
	int i;

	i=smb_putmsghdr(smb,msg);
	if(i)
		return(i);
	return(smb_putmsgidx(smb,msg));
}

/****************************************************************************/
/* Writes index information for 'msg'                                       */
/* msg->idx 																*/
/* and msg->offset must be set prior to calling to this function			*/
/* Returns 0 if everything ok                                               */
/****************************************************************************/
int SMBCALL smb_putmsgidx(smb_t* smb, smbmsg_t* msg)
{
	if(smb->sid_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"index not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sid_fp);
	if(fseek(smb->sid_fp,msg->offset*sizeof(idxrec_t),SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) seeking to %u in header"
			,errno,STRERROR(errno)
			,(unsigned)(msg->offset*sizeof(idxrec_t)));
		return(SMB_ERR_SEEK);
	}
	if(!fwrite(&msg->idx,sizeof(idxrec_t),1,smb->sid_fp)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) writing index"
			,ferror(smb->sid_fp),STRERROR(ferror(smb->sid_fp)));
		return(SMB_ERR_WRITE);
	}
	fflush(smb->sid_fp);
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Writes header information for 'msg'                                      */
/* msg->hdr.length															*/
/* msg->idx.offset															*/
/* and msg->offset must be set prior to calling to this function			*/
/* Returns 0 if everything ok                                               */
/****************************************************************************/
int SMBCALL smb_putmsghdr(smb_t* smb, smbmsg_t* msg)
{
	ushort	i;
	ulong	hdrlen;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}

	if(!smb_valid_hdr_offset(smb,msg->idx.offset))
		return(SMB_ERR_HDR_OFFSET);

	clearerr(smb->shd_fp);
	if(fseek(smb->shd_fp,msg->idx.offset,SEEK_SET)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) seeking to %lu in index"
			,errno,STRERROR(errno),msg->idx.offset);
		return(SMB_ERR_SEEK);
	}

	/* Verify that the number of blocks required to stored the actual 
	   (calculated) header length does not exceed the number allocated. */
	hdrlen=smb_getmsghdrlen(msg);
	if(hdrlen>SMB_MAX_HDR_LEN) { /* headers are limited to 64k in size */
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"illegal message header length (%lu > %u)"
			,hdrlen,SMB_MAX_HDR_LEN);
		return(SMB_ERR_HDR_LEN);
	}
	if(smb_hdrblocks(hdrlen) > smb_hdrblocks(msg->hdr.length)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"illegal header length increase: %lu (%lu blocks) vs %hu (%lu blocks)"
			,hdrlen, smb_hdrblocks(hdrlen)
			,msg->hdr.length, smb_hdrblocks(msg->hdr.length));
		return(SMB_ERR_HDR_LEN);
	}
	msg->hdr.length=(ushort)hdrlen; /* store the actual header length */

	/**********************************/
	/* Set the message header ID here */
	/**********************************/
	memcpy(&msg->hdr.id,SHD_HEADER_ID,LEN_HEADER_ID);

	/************************************************/
	/* Write the fixed portion of the header record */
	/************************************************/
	if(!fwrite(&msg->hdr,sizeof(msghdr_t),1,smb->shd_fp)) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) writing fixed portion of header record"
			,ferror(smb->shd_fp),STRERROR(ferror(smb->shd_fp)));
		return(SMB_ERR_WRITE);
	}

	/************************************************/
	/* Write the data fields (each is fixed length) */
	/************************************************/
	for(i=0;i<msg->hdr.total_dfields;i++)
		if(!fwrite(&msg->dfield[i],sizeof(dfield_t),1,smb->shd_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) writing data field"
				,ferror(smb->shd_fp),STRERROR(ferror(smb->shd_fp)));
			return(SMB_ERR_WRITE);
		}

	/*******************************************/
	/* Write the variable length header fields */
	/*******************************************/
	for(i=0;i<msg->total_hfields;i++) {
		if(!fwrite(&msg->hfield[i],sizeof(hfield_t),1,smb->shd_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) writing header field"
				,ferror(smb->shd_fp),STRERROR(ferror(smb->shd_fp)));
			return(SMB_ERR_WRITE);
		}
		if(msg->hfield[i].length					 /* more then 0 bytes long */
			&& !fwrite(msg->hfield_dat[i],msg->hfield[i].length,1,smb->shd_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) writing header field data"
				,ferror(smb->shd_fp),STRERROR(ferror(smb->shd_fp)));
			return(SMB_ERR_WRITE); 
		}
	}

	while(hdrlen%SHD_BLOCK_LEN) {
		if(fputc(0,smb->shd_fp)==EOF) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) padding header block"
				,ferror(smb->shd_fp),STRERROR(ferror(smb->shd_fp)));
			return(SMB_ERR_WRITE); 			   /* pad block with NULL */
		}
		hdrlen++; 
	}
	fflush(smb->shd_fp);
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Creates a sub-board's initial header file                                */
/* Truncates and deletes other associated SMB files 						*/
/****************************************************************************/
int SMBCALL smb_create(smb_t* smb)
{
    char        str[MAX_PATH+1];
	smbhdr_t	hdr;

	if(smb->shd_fp==NULL || smb->sdt_fp==NULL || smb->sid_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	if(filelength(fileno(smb->shd_fp))>=sizeof(smbhdr_t)+sizeof(smbstatus_t)
		&& smb_locksmbhdr(smb)!=SMB_SUCCESS)  /* header exists, so lock it */
		return(SMB_ERR_LOCK);
	memset(&hdr,0,sizeof(smbhdr_t));
	memcpy(hdr.id,SMB_HEADER_ID,LEN_HEADER_ID);     
	hdr.version=SMB_VERSION;
	hdr.length=sizeof(smbhdr_t)+sizeof(smbstatus_t);
	smb->status.last_msg=smb->status.total_msgs=0;
	smb->status.header_offset=sizeof(smbhdr_t)+sizeof(smbstatus_t);
	rewind(smb->shd_fp);
	fwrite(&hdr,1,sizeof(smbhdr_t),smb->shd_fp);
	fwrite(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp);
	rewind(smb->shd_fp);
	chsize(fileno(smb->shd_fp),sizeof(smbhdr_t)+sizeof(smbstatus_t));
	fflush(smb->shd_fp);

	rewind(smb->sdt_fp);
	chsize(fileno(smb->sdt_fp),0L);
	rewind(smb->sid_fp);
	chsize(fileno(smb->sid_fp),0L);

	SAFEPRINTF(str,"%s.sda",smb->file);
	remove(str);						/* if it exists, delete it */
	SAFEPRINTF(str,"%s.sha",smb->file);
	remove(str);                        /* if it exists, delete it */
	SAFEPRINTF(str,"%s.sch",smb->file);
	remove(str);
	smb_unlocksmbhdr(smb);
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Returns number of data blocks required to store "length" amount of data  */
/****************************************************************************/
ulong SMBCALL smb_datblocks(ulong length)
{
	ulong blocks;

	blocks=length/SDT_BLOCK_LEN;
	if(length%SDT_BLOCK_LEN)
		blocks++;
	return(blocks);
}

/****************************************************************************/
/* Returns number of header blocks required to store "length" size header   */
/****************************************************************************/
ulong SMBCALL smb_hdrblocks(ulong length)
{
	ulong blocks;

	blocks=length/SHD_BLOCK_LEN;
	if(length%SHD_BLOCK_LEN)
		blocks++;
	return(blocks);
}

/****************************************************************************/
/* Finds unused space in data file based on block allocation table and		*/
/* marks space as used in allocation table.                                 */
/* File must be opened read/write DENY ALL									*/
/* Returns offset to beginning of data (in bytes, not blocks)				*/
/* Assumes smb_open_da() has been called									*/
/* smb_close_da() should be called after									*/
/* Returns negative on error												*/
/****************************************************************************/
long SMBCALL smb_allocdat(smb_t* smb, ulong length, ushort refs)
{
    ushort  i,j;
	ulong	l,blocks,offset=0L;

	if(smb->sda_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	blocks=smb_datblocks(length);
	j=0;	/* j is consecutive unused block counter */
	fflush(smb->sda_fp);
	rewind(smb->sda_fp);
	while(!feof(smb->sda_fp) && (long)offset>=0) {
		if(smb_fread(smb,&i,sizeof(i),smb->sda_fp)!=sizeof(i))
			break;
		offset+=SDT_BLOCK_LEN;
		if(!i) j++;
		else   j=0;
		if(j==blocks) {
			offset-=(blocks*SDT_BLOCK_LEN);
			break; 
		} 
	}
	if((long)offset<0) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"invalid data offset: %lu",offset);
		return(SMB_ERR_DAT_OFFSET);
	}
	clearerr(smb->sda_fp);
	if(fseek(smb->sda_fp,(offset/SDT_BLOCK_LEN)*sizeof(refs),SEEK_SET)) {
		return(SMB_ERR_SEEK);
	}
	for(l=0;l<blocks;l++)
		if(!fwrite(&refs,sizeof(refs),1,smb->sda_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) writing allocation bytes at offset %ld"
				,ferror(smb->sda_fp),STRERROR(ferror(smb->sda_fp))
				,((offset/SDT_BLOCK_LEN)+l)*sizeof(refs));
			return(SMB_ERR_WRITE);
		}
	fflush(smb->sda_fp);
	return(offset);
}

/****************************************************************************/
/* Allocates space for data, but doesn't search for unused blocks           */
/* Returns negative on error												*/
/****************************************************************************/
long SMBCALL smb_fallocdat(smb_t* smb, ulong length, ushort refs)
{
	ulong	l,blocks,offset;

	if(smb->sda_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	fflush(smb->sda_fp);
	clearerr(smb->sda_fp);
	blocks=smb_datblocks(length);
	if(fseek(smb->sda_fp,0L,SEEK_END)) {
		return(SMB_ERR_SEEK);
	}
	offset=(ftell(smb->sda_fp)/sizeof(refs))*SDT_BLOCK_LEN;
	if((long)offset<0) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"invalid data offset: %lu",offset);
		return(SMB_ERR_DAT_OFFSET);
	}
	for(l=0;l<blocks;l++)
		if(!fwrite(&refs,sizeof(refs),1,smb->sda_fp))
			break;
	fflush(smb->sda_fp);
	if(l<blocks) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"%d (%s) writing allocation bytes"
			,ferror(smb->sda_fp),STRERROR(ferror(smb->sda_fp)));
		return(SMB_ERR_WRITE);
	}
	return(offset);
}

/****************************************************************************/
/* De-allocates space for data												*/
/* Returns non-zero on error												*/
/****************************************************************************/
int SMBCALL smb_freemsgdat(smb_t* smb, ulong offset, ulong length, ushort refs)
{
	BOOL	da_opened=FALSE;
	int		retval=0;
	ushort	i;
	ulong	l,blocks;
	ulong	sda_offset;

	if(smb->status.attr&SMB_HYPERALLOC)	/* do nothing */
		return(SMB_SUCCESS);

	blocks=smb_datblocks(length);

	if(smb->sda_fp==NULL) {
		if((i=smb_open_da(smb))!=SMB_SUCCESS)
			return(i);
		da_opened=TRUE;
	}

	clearerr(smb->sda_fp);
	for(l=0;l<blocks;l++) {
		sda_offset=((offset/SDT_BLOCK_LEN)+l)*sizeof(i);
		if(fseek(smb->sda_fp,sda_offset,SEEK_SET)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) seeking to %lu (0x%lX) of allocation file"
				,errno,STRERROR(errno)
				,sda_offset,sda_offset);
			retval=SMB_ERR_SEEK;
			break;
		}
		if(smb_fread(smb,&i,sizeof(i),smb->sda_fp)!=sizeof(i)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) reading allocation bytes at offset %ld"
				,ferror(smb->sda_fp),STRERROR(ferror(smb->sda_fp))
				,sda_offset);
			retval=SMB_ERR_READ;
			break;
		}
		if(refs==SMB_ALL_REFS || refs>i)
			i=0;			/* don't want to go negative */
		else
			i-=refs;
		if(fseek(smb->sda_fp,-(int)sizeof(i),SEEK_CUR)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) seeking backwards 2 bytes in allocation file"
				,errno,STRERROR(errno));
			retval=SMB_ERR_SEEK;
			break;
		}
		if(!fwrite(&i,sizeof(i),1,smb->sda_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) writing allocation bytes at offset %ld"
				,ferror(smb->sda_fp),STRERROR(ferror(smb->sda_fp))
				,sda_offset);
			retval=SMB_ERR_WRITE; 
			break;
		}
	}
	fflush(smb->sda_fp);
	if(da_opened)
		smb_close_da(smb);
	return(retval);
}

/****************************************************************************/
/* Adds to data allocation records for blocks starting at 'offset'          */
/* Returns non-zero on error												*/
/****************************************************************************/
int SMBCALL smb_incdat(smb_t* smb, ulong offset, ulong length, ushort refs)
{
	ushort	i;
	ulong	l,blocks;

	if(smb->sda_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sda_fp);
	blocks=smb_datblocks(length);
	for(l=0;l<blocks;l++) {
		if(fseek(smb->sda_fp,((offset/SDT_BLOCK_LEN)+l)*sizeof(i),SEEK_SET)) {
			return(SMB_ERR_SEEK);
		}
		if(smb_fread(smb,&i,sizeof(i),smb->sda_fp)!=sizeof(i)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) reading allocation record at offset %ld"
				,ferror(smb->sda_fp),STRERROR(ferror(smb->sda_fp))
				,((offset/SDT_BLOCK_LEN)+l)*sizeof(i));
			return(SMB_ERR_READ);
		}
		i+=refs;
		if(fseek(smb->sda_fp,-(int)sizeof(i),SEEK_CUR)) {
			return(SMB_ERR_SEEK);
		}
		if(!fwrite(&i,sizeof(i),1,smb->sda_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) writing allocation record at offset %ld"
				,ferror(smb->sda_fp),STRERROR(ferror(smb->sda_fp))
				,((offset/SDT_BLOCK_LEN)+l)*sizeof(i));
			return(SMB_ERR_WRITE); 
		}
	}
	fflush(smb->sda_fp);
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Increments data allocation records (message references) by number of		*/
/* header references specified (usually 1)									*/
/* The opposite function of smb_freemsg()									*/
/****************************************************************************/
int SMBCALL smb_incmsg_dfields(smb_t* smb, smbmsg_t* msg, ushort refs)
{
	int		i=0;
	BOOL	da_opened=FALSE;
	ushort	x;

	if(smb->status.attr&SMB_HYPERALLOC)  /* Nothing to do */
		return(SMB_SUCCESS);

	if(smb->sda_fp==NULL) {
		if((i=smb_open_da(smb))!=SMB_SUCCESS)
			return(i);
		da_opened=TRUE;
	}

	for(x=0;x<msg->hdr.total_dfields;x++) {
		if((i=smb_incdat(smb,msg->hdr.offset+msg->dfield[x].offset
			,msg->dfield[x].length,refs))!=SMB_SUCCESS)
			break; 
	}

	if(da_opened)
		smb_close_da(smb);
	return(i);
}

/****************************************************************************/
/* De-allocates blocks for header record									*/
/* Returns non-zero on error												*/
/****************************************************************************/
int SMBCALL smb_freemsghdr(smb_t* smb, ulong offset, ulong length)
{
	uchar	c=0;
	ulong	l,blocks;

	if(smb->sha_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sha_fp);
	blocks=smb_hdrblocks(length);
	if(fseek(smb->sha_fp,offset/SHD_BLOCK_LEN,SEEK_SET))
		return(SMB_ERR_SEEK);
	for(l=0;l<blocks;l++)
		if(!fwrite(&c,1,1,smb->sha_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) writing allocation record"
				,ferror(smb->sha_fp),STRERROR(ferror(smb->sha_fp)));
			return(SMB_ERR_WRITE);
		}
	fflush(smb->sha_fp);
	return(SMB_SUCCESS);
}

/****************************************************************************/
/****************************************************************************/
int SMBCALL smb_freemsg_dfields(smb_t* smb, smbmsg_t* msg, ushort refs)
{
	int		i;
	ushort	x;

	for(x=0;x<msg->hdr.total_dfields;x++) {
		if((i=smb_freemsgdat(smb,msg->hdr.offset+msg->dfield[x].offset
			,msg->dfield[x].length,refs))!=SMB_SUCCESS)
			return(i); 
	}
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Frees all allocated header and data blocks (1 reference) for 'msg'       */
/****************************************************************************/
int SMBCALL smb_freemsg(smb_t* smb, smbmsg_t* msg)
{
	int 	i;

	if(smb->status.attr&SMB_HYPERALLOC)  /* Nothing to do */
		return(SMB_SUCCESS);

	if(!smb_valid_hdr_offset(smb,msg->idx.offset))
		return(SMB_ERR_HDR_OFFSET);

	if((i=smb_freemsg_dfields(smb,msg,1))!=SMB_SUCCESS)
		return(i);

	return(smb_freemsghdr(smb,msg->idx.offset-smb->status.header_offset
		,msg->hdr.length));
}

/****************************************************************************/
/* Finds unused space in header file based on block allocation table and	*/
/* marks space as used in allocation table.                                 */
/* File must be opened read/write DENY ALL									*/
/* Returns offset to beginning of header (in bytes, not blocks) 			*/
/* Assumes smb_open_ha() has been called									*/
/* smb_close_ha() should be called after									*/
/* Returns negative value on error 											*/
/****************************************************************************/
long SMBCALL smb_allochdr(smb_t* smb, ulong length)
{
	uchar	c;
	ushort	i;
	ulong	l,blocks,offset=0;

	if(smb->sha_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	blocks=smb_hdrblocks(length);
	i=0;	/* i is consecutive unused block counter */
	fflush(smb->sha_fp);
	rewind(smb->sha_fp);
	while(!feof(smb->sha_fp)) {
		if(smb_fread(smb,&c,sizeof(c),smb->sha_fp)!=sizeof(c)) 
			break;
		offset+=SHD_BLOCK_LEN;
		if(!c) i++;
		else   i=0;
		if(i==blocks) {
			offset-=(blocks*SHD_BLOCK_LEN);
			break; 
		} 
	}
	clearerr(smb->sha_fp);
	if(fseek(smb->sha_fp,offset/SHD_BLOCK_LEN,SEEK_SET))
		return(SMB_ERR_SEEK);
	c=1;
	for(l=0;l<blocks;l++)
		if(!fwrite(&c,1,1,smb->sha_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) writing allocation record"
				,ferror(smb->sha_fp),STRERROR(ferror(smb->sha_fp)));
			return(SMB_ERR_WRITE);
		}
	fflush(smb->sha_fp);
	return(offset);
}

/****************************************************************************/
/* Allocates space for index, but doesn't search for unused blocks          */
/* Returns negative value on error 											*/
/****************************************************************************/
long SMBCALL smb_fallochdr(smb_t* smb, ulong length)
{
	uchar	c=1;
	ulong	l,blocks,offset;

	if(smb->sha_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	blocks=smb_hdrblocks(length);
	fflush(smb->sha_fp);
	clearerr(smb->sha_fp);
	if(fseek(smb->sha_fp,0L,SEEK_END))
		return(SMB_ERR_SEEK);
	offset=ftell(smb->sha_fp)*SHD_BLOCK_LEN;
	for(l=0;l<blocks;l++)
		if(!fwrite(&c,1,1,smb->sha_fp)) {
			safe_snprintf(smb->last_error,sizeof(smb->last_error)
				,"%d (%s) writing allocation record"
				,ferror(smb->sha_fp),STRERROR(ferror(smb->sha_fp)));
			return(SMB_ERR_WRITE);
		}
	fflush(smb->sha_fp);
	return(offset);
}

/************************************************************************/
/* Allocate header blocks using Hyper Allocation						*/
/* this function should be most likely not be called from anywhere but	*/
/* smb_addmsghdr()														*/
/************************************************************************/
long SMBCALL smb_hallochdr(smb_t* smb)
{
	ulong offset;

	if(smb->shd_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error),"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	fflush(smb->shd_fp);
	if(fseek(smb->shd_fp,0L,SEEK_END))
		return(SMB_ERR_SEEK);
	offset=ftell(smb->shd_fp);
	if(offset<smb->status.header_offset) 	/* Header file truncated?!? */
		return(smb->status.header_offset);

	offset-=smb->status.header_offset;		/* SMB headers not included */

	/* Even block boundry */
	offset+=PAD_LENGTH_FOR_ALIGNMENT(offset,SHD_BLOCK_LEN);

	return(offset);
}

/************************************************************************/
/* Allocate data blocks using Hyper Allocation							*/
/* smb_locksmbhdr() should be called before this function and not		*/
/* unlocked until all data fields for this message have been written	*/
/* to the SDT file														*/
/************************************************************************/
long SMBCALL smb_hallocdat(smb_t* smb)
{
	long offset;

	if(smb->sdt_fp==NULL) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	fflush(smb->sdt_fp);
	offset=filelength(fileno(smb->sdt_fp));
	if(offset<0) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"invalid file length: %lu",(ulong)offset);
		return(SMB_ERR_FILE_LEN);
	}
	if(fseek(smb->sdt_fp,0L,SEEK_END))
		return(SMB_ERR_SEEK);
	offset=ftell(smb->sdt_fp);
	if(offset<0) {
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"invalid file offset: %ld",offset);
		return(SMB_ERR_DAT_OFFSET);
	}

	/* Make sure even block boundry */
	offset+=PAD_LENGTH_FOR_ALIGNMENT(offset,SDT_BLOCK_LEN);

	return(offset);
}

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
	size_t ret=0;
	time_t start=0;

	while(1) {
		if((ret=fread(buf,sizeof(char),bytes,fp))==bytes)
			return(ret);
		if(ferror(fp)!=EDEADLOCK)
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

size_t SMBCALL smb_fwrite(smb_t* smb, void* buf, size_t bytes, FILE* fp)
{
	return(fwrite(buf,1,bytes,fp));
}

/************************************************************************/
/* Returns difference from specified timezone and UTC/GMT				*/
/************************************************************************/
int SMBCALL smb_tzutc(short zone)
{
	int tz;

	if(OTHER_ZONE(zone))
		return(zone);

	tz=zone&0xfff;
	if(zone&(WESTERN_ZONE|US_ZONE))	/* West of UTC? */
		return(-tz);
	return(tz);
}

char* SMBCALL smb_hfieldtype(ushort type)
{
	static char str[8];

	switch(type) {
		case SENDER:			return("Sender");
		case SENDERAGENT:		return("SenderAgent");
		case SENDERNETTYPE:		return("SenderNetType");
		case SENDERNETADDR:		return("SenderNetAddr");
		case SENDEREXT:			return("SenderExt");
		case SENDERORG:			return("SenderOrg");
		case SENDERIPADDR:		return("SenderIpAddr");
		case SENDERHOSTNAME:	return("SenderHostName");
		case SENDERPROTOCOL:	return("SenderProtocol");
		case SENDERPORT:		return("SenderPort");

		case REPLYTO:			return("ReplyTo");
		case REPLYTOAGENT:		return("ReplyToAgent");
		case REPLYTONETTYPE:	return("ReplyToNetType");
		case REPLYTONETADDR:	return("ReplyToNetAddr");
		case REPLYTOEXT:		return("ReplyToExt");
								
		case RECIPIENT:			return("Recipient");
		case RECIPIENTAGENT:	return("RecipientAgent");
		case RECIPIENTNETTYPE:	return("RecipientNetType");
		case RECIPIENTNETADDR:	return("RecipientNetAddr");
		case RECIPIENTEXT:		return("RecipientExt");

		case SUBJECT:			return("Subject");
		case SMB_SUMMARY:		return("Summary");
		case SMB_COMMENT:		return("Comment");
		case SMB_CARBONCOPY:	return("CarbonCopy");
		case SMB_GROUP:			return("Group");
		case SMB_EXPIRATION:	return("Expiration");
		case SMB_PRIORITY:		return("Priority");
		case SMB_COST:			return("Cost");

		case FIDOCTRL:			return("FidoCtrl");
		case FIDOAREA:			return("FidoArea");
		case FIDOSEENBY:		return("FidoSeenBy");
		case FIDOPATH:			return("FidoPath");
		case FIDOMSGID:			return("FidoMsgID");
		case FIDOREPLYID:		return("FidoReplyID");
		case FIDOPID:			return("FidoPID");
		case FIDOFLAGS:			return("FidoFlags");
		case FIDOTID:			return("FidoTID");

		case RFC822HEADER:		return("RFC822Header");
		case RFC822MSGID:		return("RFC822MsgID");
		case RFC822REPLYID:		return("RFC822ReplyID");
		case RFC822TO:			return("RFC822To");
		case RFC822FROM:		return("RFC822From");
		case RFC822REPLYTO:		return("RFC822ReplyTo");

		case USENETPATH:		return("UsenetPath");
		case USENETNEWSGROUPS:	return("UsenetNewsgroups");

		case SMTPCOMMAND:		return("SMTPCommand");
		case SMTPREVERSEPATH:	return("SMTPReversePath");

		case SMTPSYSMSG:		return("SMTPSysMsg");

		case UNKNOWN:			return("UNKNOWN");
		case UNKNOWNASCII:		return("UNKNOWNASCII");
		case UNUSED:			return("UNUSED");
	}
	sprintf(str,"%02Xh",type);
	return(str);
}

ushort SMBCALL smb_hfieldtypelookup(const char* str)
{
	ushort type;

	if(isdigit(*str))
		return((ushort)strtol(str,NULL,0));

	for(type=0;type<=UNUSED;type++)
		if(stricmp(str,smb_hfieldtype(type))==0)
			return(type);

	return(UNKNOWN);
}

char* SMBCALL smb_dfieldtype(ushort type)
{
	static char str[8];

	switch(type) {
		case TEXT_BODY: return("TEXT_BODY");
		case TEXT_TAIL: return("TEXT_TAIL");
		case UNUSED:	return("UNUSED");
	}
	sprintf(str,"%02Xh",type);
	return(str);
}

int SMBCALL smb_updatethread(smb_t* smb, smbmsg_t* remsg, ulong newmsgnum)
{
	int			retval=SMB_ERR_NOT_FOUND;
	ulong		nextmsgnum;
	smbmsg_t	nextmsg;

	if(!remsg->hdr.thread_first) {	/* New msg is first reply */
		remsg->hdr.thread_first=newmsgnum;
		if((retval=smb_lockmsghdr(smb,remsg))!=SMB_SUCCESS)
			return(retval);
		retval=smb_putmsghdr(smb,remsg);
		smb_unlockmsghdr(smb,remsg);
		return(retval);
	}
	
	/* Search for last reply and extend chain */
	memset(&nextmsg,0,sizeof(nextmsg));
	nextmsgnum=remsg->hdr.thread_first;	/* start with first reply */
	while(1) {
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

/**************************/
/* Hash-related functions */
/**************************/

/* If return value is SMB_ERROR_NOT_FOUND, hash file is left open */
int SMBCALL smb_findhash(smb_t* smb, hash_t** compare, hash_t* found_hash, BOOL mark)
{
	int		retval;
	BOOL	found=FALSE;
	size_t	c,count;
	hash_t	hash;

	if(found_hash!=NULL)
		memset(found_hash,0,sizeof(hash_t));

	if((retval=smb_open_hash(smb))!=SMB_SUCCESS)
		return(retval);

	COUNT_LIST_ITEMS(compare, count);

	if(count) {

		rewind(smb->hash_fp);
		while(!feof(smb->hash_fp)) {
			memset(&hash,0,sizeof(hash));
			if(smb_fread(smb,&hash,sizeof(hash),smb->hash_fp)!=sizeof(hash))
				break;

			if(hash.flags==0)
				continue;		/* invalid hash record (!?) */

			for(c=0;compare[c]!=NULL;c++) {

				if(compare[c]->source!=hash.source)
					continue;	/* wrong source */
				if(compare[c]->flags&SMB_HASH_MARKED)
					continue;	/* already marked */
				if((compare[c]->flags&SMB_HASH_PROC_MASK)!=(hash.flags&SMB_HASH_PROC_MASK))
					continue;	/* wrong pre-process flags */
				if((compare[c]->flags&hash.flags&SMB_HASH_MASK)==0)	
					continue;	/* no matching hashes */
				if(compare[c]->flags&hash.flags&SMB_HASH_CRC16 
					&& compare[c]->crc16!=hash.crc16)
					continue;	/* wrong crc-16 */
				if(compare[c]->flags&hash.flags&SMB_HASH_CRC32
					&& compare[c]->crc32!=hash.crc32)
					continue;	/* wrong crc-32 */
				if(compare[c]->flags&hash.flags&SMB_HASH_MD5 
					&& memcmp(compare[c]->md5,hash.md5,sizeof(hash.md5)))
					continue;	/* wrong crc-16 */
				
				/* successful match! */
				break;	/* can't match more than one, so stop comparing */
			}

			if(compare[c]==NULL)
				continue;	/* no match */

			found=TRUE;

			if(found_hash!=NULL)
				memcpy(found_hash,&hash,sizeof(hash));

			if(!mark)
				break;

			compare[c]->flags|=SMB_HASH_MARKED;
		}
		if(found) {
			smb_close_hash(smb);
			return(SMB_SUCCESS);
		}
	}

	/* hash file left open */
	return(SMB_ERR_NOT_FOUND);
}

int SMBCALL smb_addhashes(smb_t* smb, hash_t** hashes)
{
	int		retval;
	size_t	h;

	COUNT_LIST_ITEMS(hashes, h);
	if(!h)	/* nothing to add */
		return(SMB_SUCCESS);

	if((retval=smb_open_hash(smb))!=SMB_SUCCESS)
		return(retval);

	fseek(smb->hash_fp,0,SEEK_END);

	for(h=0;hashes[h]!=NULL;h++) {

		/* skip hashes marked by smb_findhash() */
		if(hashes[h]->flags&SMB_HASH_MARKED)	
			continue;	
	
		if(smb_fwrite(smb,hashes[h],sizeof(hash_t),smb->hash_fp)!=sizeof(hash_t))
			return(SMB_ERR_WRITE);
	}

	smb_close_hash(smb);

	return(SMB_SUCCESS);
}

static char* strip_chars(uchar* str, uchar* set)
{
	char*	src;
	char*	dst;
	char*	tmp;

	if((tmp=strdup(str))==NULL)
		return(NULL);
	for(src=tmp,dst=str;*src;src++) {
		if(strchr(set,*src)==NULL)
			*(dst++)=*src;
	}
	*dst=0;
	
	return(str);
}

/* Allocates and calculates hashes of data (based on flags)					*/
/* Returns NULL on failure													*/
hash_t* SMBCALL smb_hash(ulong msgnum, ulong t, unsigned source, unsigned flags
						 ,const void* data, size_t length)
{
	hash_t*	hash;

	if((hash=(hash_t*)malloc(sizeof(hash_t)))==NULL)
		return(NULL);

	hash->number=msgnum;
	hash->time=t;
	hash->source=source;
	hash->flags=flags;
	if(flags&SMB_HASH_CRC16)
		hash->crc16=crc16((char*)data,length);
	if(flags&SMB_HASH_CRC32)
		hash->crc32=crc32((char*)data,length);
	if(flags&SMB_HASH_MD5)
		MD5_calc(hash->md5,data,length);

	return(hash);
}

/* Allocates and calculates hashes of data (based on flags)					*/
/* Supports string hash "pre-processing" (e.g. lowercase, strip whitespace)	*/
/* Returns NULL on failure													*/
hash_t* SMBCALL smb_hashstr(ulong msgnum, ulong t, unsigned source, unsigned flags
							,const char* str)
{
	char*	p=(uchar*)str;
	hash_t*	hash;

	if(flags&SMB_HASH_PROC_MASK) {	/* string pre-processing */
		if((p=strdup(str))==NULL)
			return(NULL);
		if(flags&SMB_HASH_LOWERCASE)
			strlwr(p);
		if(flags&SMB_HASH_STRIP_WSP)
			strip_chars(p," \t\r\n");
	}
	
	hash=smb_hash(msgnum, t, source, flags, p, strlen(p));

	if(p!=str)	/* duped string */
		free(p);

	return(hash);
}

/* Allocatese and calculates all hashes for a single message				*/
/* Returns NULL on failure													*/
hash_t** SMBCALL smb_msghashes(smb_t* smb, smbmsg_t* msg, const uchar* text)
{
	size_t		h=0;
	uchar		flags=SMB_HASH_CRC16|SMB_HASH_CRC32|SMB_HASH_MD5;
	hash_t**	hashes;	/* This is a NULL-terminated list of hashes */
	hash_t*		hash;
	time_t		t=time(NULL);

#define SMB_MAX_HASH_COUNT 4

	if((hashes=(hash_t**)malloc(sizeof(hash_t*)*SMB_MAX_HASH_COUNT))==NULL)
		return(NULL);

	memset(hashes, 0, sizeof(hash_t*)*SMB_MAX_HASH_COUNT);

	if(msg->id!=NULL
		&& (hash=smb_hashstr(msg->hdr.number, t, RFC822MSGID, flags, msg->id))!=NULL)
		hashes[h++]=hash;

	if(msg->ftn_msgid!=NULL
		&& (hash=smb_hashstr(msg->hdr.number, t, FIDOMSGID, flags, msg->ftn_msgid))!=NULL)
		hashes[h++]=hash;

	if(text!=NULL
		&& (hash=smb_hashstr(msg->hdr.number, t, TEXT_BODY, flags|SMB_HASH_STRIP_WSP, text))!=NULL)
		hashes[h++]=hash;

	return(hashes);
}

/* Calculates and stores the hashes for a single message					*/
int SMBCALL smb_hashmsg(smb_t* smb, smbmsg_t* msg, const uchar* text, BOOL update)
{
	size_t		n;
	int			retval=SMB_SUCCESS;
	hash_t		found;
	hash_t**	hashes;	/* This is a NULL-terminated list of hashes */

	hashes=smb_msghashes(smb,msg,text);

	if(smb_findhash(smb, hashes, &found, update)==SMB_SUCCESS && !update) {
		retval=SMB_DUPE_MSG;
		safe_snprintf(smb->last_error,sizeof(smb->last_error)
			,"Duplicate type %u hash found for message #%lu"
			,found.source, found.number);
	} else
		if((retval=smb_addhashes(smb,hashes))==SMB_SUCCESS)
			msg->flags|=MSG_FLAG_HASHED;

	FREE_LIST(hashes,n);

	return(retval);
}

/* length=0 specifies ASCIIZ data											*/
int SMBCALL smb_getmsgidx_by_hash(smb_t* smb, smbmsg_t* msg, unsigned source
								 ,unsigned flags, const void* data, size_t length)
{
	int			retval;
	size_t		n;
	hash_t**	hashes;
	hash_t		found;

	if((hashes=(hash_t**)malloc(sizeof(hash_t*)*2))==NULL)
		return(SMB_ERR_MEM);

	if(length==0)
		hashes[0]=smb_hashstr(0,0,source,flags,data);
	else
		hashes[0]=smb_hash(0,0,source,flags,data,length);
	if(hashes[0]==NULL)
		return(SMB_ERR_MEM);

	hashes[1]=NULL;	/* terminate list */

	memset(&found,0,sizeof(found));
	if((retval=smb_findhash(smb, hashes, &found, FALSE))==SMB_SUCCESS) {
		if(found.number==0)
			retval=SMB_FAILURE;	/* use better error value here? */
		else {
			msg->hdr.number=found.number;
			retval=smb_getmsgidx(smb, msg);
		}
	}

	FREE_LIST(hashes,n);

	return(retval);
}

int SMBCALL smb_getmsghdr_by_hash(smb_t* smb, smbmsg_t* msg, unsigned source
								 ,unsigned flags, const void* data, size_t length)
{
	int retval;

	if((retval=smb_getmsgidx_by_hash(smb,msg,source,flags,data,length))!=SMB_SUCCESS)
		return(retval);

	if((retval=smb_lockmsghdr(smb,msg))!=SMB_SUCCESS)
		return(retval);

	retval=smb_getmsghdr(smb,msg);

	smb_unlockmsghdr(smb,msg); 

	return(retval);
}


/* End of SMBLIB.C */
