/* smblib.c */

/* Synchronet message base (SMB) library routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#if defined __WATCOMC__ || defined __TURBOC__
	#include <mem.h>
#else
	#include <memory.h>
#endif

#ifdef __WATCOMC__
	#include <dos.h>
#elif defined __TURBOC__
	#include <dir.h>
#endif

/* ANSI C Library headers */

#ifndef __unix__
	#include <malloc.h>
#endif

#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>	/* must come after sys/types.h */

/* SMB-specific headers */
#include "smblib.h"
#include "genwrap.h"
#include "filewrap.h"

/* Use smb_ver() and smb_lib_ver() to obtain these values */
#define SMBLIB_VERSION		"2.21"      /* SMB library version */
#define SMB_VERSION 		0x0121		/* SMB format version */
										/* High byte major, low byte minor */

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
    int file;
    char str[128];
	smbhdr_t hdr;

	/* Set default values, if uninitialized */
	if(!smb->retry_time)
		smb->retry_time=10;		/* seconds */
	if(!smb->retry_delay 
		|| smb->retry_delay>(smb->retry_time*100))	/* at least ten retries */
		smb->retry_delay=250;	/* milliseconds */
	smb->shd_fp=smb->sdt_fp=smb->sid_fp=NULL;
	smb->last_error[0]=0;
	sprintf(str,"%s.shd",smb->file);
	if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,S_IREAD|S_IWRITE))==-1) {
		sprintf(smb->last_error,"%d (%s) opening %s"
			,errno,STRERROR(errno),str);
		return(SMB_ERR_OPEN); 
	}

	if((smb->shd_fp=fdopen(file,"r+b"))==NULL) {
		sprintf(smb->last_error,"%d (%s) fdopening %s (%d)"
			,errno,STRERROR(errno),str,file);
		close(file);
		return(SMB_ERR_OPEN); 
	}

	if(filelength(file)>=sizeof(smbhdr_t)) {
		setvbuf(smb->shd_fp,smb->shd_buf,_IONBF,SHD_BLOCK_LEN);
		if(smb_locksmbhdr(smb)!=0) {
			smb_close(smb);
			/* smb_lockmsghdr set last_error */
			return(SMB_ERR_LOCK); 
		}
		memset(&hdr,0,sizeof(smbhdr_t));
		if(fread(&hdr,sizeof(smbhdr_t),1,smb->shd_fp)!=1) {
			sprintf(smb->last_error,"%d (%s) reading header"
				,errno,STRERROR(errno));
			smb_close(smb);
			return(SMB_ERR_READ);
		}
		if(memcmp(hdr.id,SMB_HEADER_ID,LEN_HEADER_ID)) {
			sprintf(smb->last_error,"corrupt SMB header ID: %.*s"
				,LEN_HEADER_ID,hdr.id);
			smb_close(smb);
			return(SMB_ERR_HDR_ID); 
		}
		if(hdr.version<0x110) {         /* Compatibility check */
			sprintf(smb->last_error,"insufficient header version: %X"
				,hdr.version);
			smb_close(smb);
			return(SMB_ERR_HDR_VER); 
		}
		if(fread(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp)
			!=sizeof(smbstatus_t)) {
			sprintf(smb->last_error,"%d (%s) reading status"
				,errno,STRERROR(errno));
			smb_close(smb);
			return(SMB_ERR_READ); 
		}
		smb_unlocksmbhdr(smb);
		rewind(smb->shd_fp); 
	}

	setvbuf(smb->shd_fp,smb->shd_buf,_IOFBF,SHD_BLOCK_LEN);

	sprintf(str,"%s.sdt",smb->file);
	if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,S_IREAD|S_IWRITE))==-1) {
		sprintf(smb->last_error,"%d (%s) opening %s"
			,errno,STRERROR(errno),str);
		smb_close(smb);
		return(SMB_ERR_OPEN); 
	}

	if((smb->sdt_fp=fdopen(file,"r+b"))==NULL) {
		sprintf(smb->last_error,"%d (%s) fdopening %s (%d)"
			,errno,STRERROR(errno),str,file);
		close(file);
		smb_close(smb);
		return(SMB_ERR_OPEN);
	}

	setvbuf(smb->sdt_fp,NULL,_IOFBF,2*1024);

	sprintf(str,"%s.sid",smb->file);
	if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO,S_IREAD|S_IWRITE))==-1) {
		sprintf(smb->last_error,"%d (%s) opening %s"
			,errno,STRERROR(errno),str);
		smb_close(smb);
		return(SMB_ERR_OPEN); 
	}

	if((smb->sid_fp=fdopen(file,"r+b"))==NULL) {
		sprintf(smb->last_error,"%d (%s) fdopening %s (%d)"
			,errno,STRERROR(errno),str,file);
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
		fclose(smb->shd_fp); 
	}
	if(smb->sid_fp!=NULL)
		fclose(smb->sid_fp);
	if(smb->sdt_fp!=NULL)
		fclose(smb->sdt_fp);
	smb->sid_fp=smb->shd_fp=smb->sdt_fp=NULL;
}

/****************************************************************************/
/* Opens the data block allocation table message base 'smb->file'           */
/* Retrys for retry_time number of seconds									*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int SMBCALL smb_open_da(smb_t* smb)
{
	int 	file;
	char	str[128];
	time_t	start=0;

	sprintf(str,"%s.sda",smb->file);
	while(1) {
		if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW,S_IREAD|S_IWRITE))!=-1)
			break;
		if(errno!=EACCES && errno!=EAGAIN) {
			sprintf(smb->last_error,"%d (%s) opening %s"
				,errno,STRERROR(errno),str);
			return(SMB_ERR_OPEN);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {
				sprintf(smb->last_error,"timeout opening %s (retry_time=%ld)"
					,str,smb->retry_time);
				return(SMB_ERR_TIMEOUT); 
			}
		SLEEP(smb->retry_delay);
	}
	if((smb->sda_fp=fdopen(file,"r+b"))==NULL) {
		sprintf(smb->last_error,"%d (%s) fdopening %s (%d)"
			,errno,STRERROR(errno),str,file);
		close(file);
		return(SMB_ERR_OPEN); 
	}
	setvbuf(smb->sda_fp,NULL,_IOFBF,2*1024);
	return(SMB_SUCCESS);
}

void SMBCALL smb_close_da(smb_t* smb)
{
	if(smb->sda_fp!=NULL)
		fclose(smb->sda_fp);
	smb->sda_fp=NULL;
}

/****************************************************************************/
/* Opens the header block allocation table for message base 'smb.file'      */
/* Retrys for smb.retry_time number of seconds								*/
/* Return 0 on success, non-zero otherwise									*/
/****************************************************************************/
int SMBCALL smb_open_ha(smb_t* smb)
{
	int 	file;
	char	str[128];
	time_t	start=0;

	sprintf(str,"%s.sha",smb->file);
	while(1) {
		if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW,S_IREAD|S_IWRITE))!=-1)
			break;
		if(errno!=EACCES && errno!=EAGAIN) {
			sprintf(smb->last_error,"%d (%s) opening %s"
				,errno,STRERROR(errno),str);
			return(SMB_ERR_OPEN);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {
				sprintf(smb->last_error,"timeout opening %s (retry_time=%ld)"
					,str,smb->retry_time);
				return(SMB_ERR_TIMEOUT); 
			}
		SLEEP(smb->retry_delay);
	}
	if((smb->sha_fp=fdopen(file,"r+b"))==NULL) {
		sprintf(smb->last_error,"%d (%s) fdopening %s (%d)"
			,errno,STRERROR(errno),str,file);
		close(file);
		return(SMB_ERR_OPEN); 
	}
	setvbuf(smb->sha_fp,NULL,_IOFBF,2*1024);
	return(SMB_SUCCESS);
}

void SMBCALL smb_close_ha(smb_t* smb)
{
	if(smb->sha_fp!=NULL)
		fclose(smb->sha_fp);
	smb->sha_fp=NULL;
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
			sprintf(smb->last_error,"SMB stack overflow");
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
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	rewind(smb->shd_fp);
	while(1) {
		if(!chsize(fileno(smb->shd_fp),0L))
			break;
		if(errno!=EACCES && errno!=EAGAIN) {
			sprintf(smb->last_error,"%d (%s) changing header file size"
				,errno,STRERROR(errno));
			return(SMB_ERR_WRITE);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {	 /* Time-out */
				sprintf(smb->last_error,"timeout changing header file size (retry_time=%ld)"
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
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	while(1) {
		if(lock(fileno(smb->shd_fp),0L,sizeof(smbhdr_t)+sizeof(smbstatus_t))==0) {
			smb->locked=1;	/* TRUE */
			return(SMB_SUCCESS);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) 
				break;						
		/* In case we've already locked it */
		if(unlock(fileno(smb->shd_fp),0L,sizeof(smbhdr_t)+sizeof(smbstatus_t))==0)
			smb->locked=0;	/* FALSE */
		SLEEP(smb->retry_delay);
	}
	sprintf(smb->last_error,"timeout locking header");
	return(SMB_ERR_TIMEOUT);
}

/****************************************************************************/
/* Read the SMB header from the header file and place into smb.status		*/
/****************************************************************************/
int SMBCALL smb_getstatus(smb_t* smb)
{
	int 	i;

	if(smb->shd_fp==NULL) {
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	setvbuf(smb->shd_fp,smb->shd_buf,_IONBF,SHD_BLOCK_LEN);
	clearerr(smb->shd_fp);
	if(fseek(smb->shd_fp,sizeof(smbhdr_t),SEEK_SET)) {
		sprintf(smb->last_error,"%d (%s) seeking to %u in header file"
			,errno,STRERROR(errno),sizeof(smbhdr_t));
		return(SMB_ERR_SEEK);
	}
	i=fread(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp);
	setvbuf(smb->shd_fp,smb->shd_buf,_IOFBF,SHD_BLOCK_LEN);
	if(i==sizeof(smbstatus_t))
		return(SMB_SUCCESS);
	sprintf(smb->last_error,"%d (%s) reading status",errno,STRERROR(errno));
	return(SMB_ERR_READ);
}

/****************************************************************************/
/* Writes message base header												*/
/****************************************************************************/
int SMBCALL smb_putstatus(smb_t* smb)
{
	int i;

	if(smb->shd_fp==NULL) {
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->shd_fp);
	if(fseek(smb->shd_fp,sizeof(smbhdr_t),SEEK_SET)) {
		sprintf(smb->last_error,"%d (%s) seeking to %u in header file"
			,errno,STRERROR(errno),sizeof(smbhdr_t));
		return(SMB_ERR_SEEK);
	}
	i=fwrite(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp);
	fflush(smb->shd_fp);
	if(i==sizeof(smbstatus_t))
		return(SMB_SUCCESS);
	sprintf(smb->last_error,"%d (%s) writing status",errno,STRERROR(errno));
	return(SMB_ERR_WRITE);
}

/****************************************************************************/
/* Unlocks previously locks message base header 							*/
/****************************************************************************/
int SMBCALL smb_unlocksmbhdr(smb_t* smb)
{
	int result;

	if(smb->shd_fp==NULL) {
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	result = unlock(fileno(smb->shd_fp),0L,sizeof(smbhdr_t)+sizeof(smbstatus_t));
	if(result==0)
		smb->locked=0;	/* FALSE */
	return(result);
}

/********************************/
/* Individual Message Functions */
/********************************/

/****************************************************************************/
/* Attempts for smb.retry_time number of seconds to lock the hdr for 'msg'  */
/****************************************************************************/
int SMBCALL smb_lockmsghdr(smb_t* smb, smbmsg_t* msg)
{
	time_t	start=0;

	if(smb->shd_fp==NULL) {
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
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
	sprintf(smb->last_error,"timeout locking header");
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
		sprintf(smb->last_error,"index not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sid_fp);
	if(!msg->hdr.number) {
		if(fseek(smb->sid_fp,msg->offset*sizeof(idxrec_t),SEEK_SET)) {
			sprintf(smb->last_error,"%d (%s) seeking to %u in index file"
				,errno,STRERROR(errno)
				,(unsigned)(msg->offset*sizeof(idxrec_t)));
			return(SMB_ERR_SEEK);
		}
		if(!fread(&msg->idx,sizeof(idxrec_t),1,smb->sid_fp)) {
			sprintf(smb->last_error,"%d (%s) reading index"
				,errno,STRERROR(errno));
			return(SMB_ERR_READ);
		}
		return(SMB_SUCCESS); 
	}

	length=filelength(fileno(smb->sid_fp));
	if(!length) {
		sprintf(smb->last_error,"invalid index file length: %ld",length);
		return(SMB_ERR_FILE_LEN);
	}
	total=length/sizeof(idxrec_t);
	if(!total) {
		sprintf(smb->last_error,"invalid index file length: %ld",length);
		return(SMB_ERR_FILE_LEN);
	}

	bot=0;
	top=total;
	l=total/2; /* Start at middle index */
	while(1) {
		if(fseek(smb->sid_fp,l*sizeof(idxrec_t),SEEK_SET)) {
			sprintf(smb->last_error,"%d (%s) seeking to %u in index file"
				,errno,STRERROR(errno)
				,(unsigned)(l*sizeof(idxrec_t)));
			return(SMB_ERR_SEEK);
		}
		if(!fread(&idx,sizeof(idxrec_t),1,smb->sid_fp)) {
			sprintf(smb->last_error,"%d (%s) reading index"
				,errno,STRERROR(errno));
			return(SMB_ERR_READ);
		}
		if(bot==top-1 && idx.number!=msg->hdr.number) {
			sprintf(smb->last_error,"msg %ld not found"
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
		sprintf(smb->last_error,"index not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sid_fp);
	if(fseek(smb->sid_fp,0,SEEK_SET)) {
		sprintf(smb->last_error,"%d (%s) seeking to beginning of index file"
			,errno,STRERROR(errno));
		return(SMB_ERR_SEEK);
	}
	if(!fread(idx,sizeof(idxrec_t),1,smb->sid_fp)) {
		sprintf(smb->last_error,"%d (%s) reading index"
			,errno,STRERROR(errno));
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
		sprintf(smb->last_error,"index not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sid_fp);
	length=filelength(fileno(smb->sid_fp));
	if(length<sizeof(idxrec_t)) {
		sprintf(smb->last_error,"invalid index file length: %ld",length);
		return(SMB_ERR_FILE_LEN);
	}
	if(fseek(smb->sid_fp,length-sizeof(idxrec_t),SEEK_SET)) {
		sprintf(smb->last_error,"%d (%s) seeking to %u in index file"
			,errno,STRERROR(errno)
			,(unsigned)(length-sizeof(idxrec_t)));
		return(SMB_ERR_SEEK);
	}
	if(!fread(idx,sizeof(idxrec_t),1,smb->sid_fp)) {
		sprintf(smb->last_error,"%d (%s) reading index",errno,STRERROR(errno));
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

static void set_convenience_ptr(smbmsg_t* msg, ushort hfield_type, void* hfield_dat)
{
	switch(hfield_type) {	/* convenience variables */
		case SENDER:
			if(!msg->from) {
				msg->from=(char*)hfield_dat;
				break; 
			}
		case FORWARDED: 	/* fall through */
			msg->forwarded=1;
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
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	rewind(smb->shd_fp);
	if(fseek(smb->shd_fp,msg->idx.offset,SEEK_SET)) {
		sprintf(smb->last_error,"%d (%s) seeking to %u in header"
			,errno,STRERROR(errno)
			,(unsigned)msg->idx.offset);
		return(SMB_ERR_SEEK);
	}

	idx=msg->idx;
	offset=msg->offset;
	memset(msg,0,sizeof(smbmsg_t));
	msg->idx=idx;
	msg->offset=offset;
	if(!fread(&msg->hdr,sizeof(msghdr_t),1,smb->shd_fp)) {
		sprintf(smb->last_error,"%d (%s) reading msg header"
			,errno,STRERROR(errno));
		return(SMB_ERR_READ);
	}
	if(memcmp(msg->hdr.id,SHD_HEADER_ID,LEN_HEADER_ID)) {
		sprintf(smb->last_error,"corrupt message header ID: %.*s"
			,LEN_HEADER_ID,msg->hdr.id);
		return(SMB_ERR_HDR_ID);
	}
	if(msg->hdr.version<0x110) {
		sprintf(smb->last_error,"insufficient header version: %X"
			,msg->hdr.version);
		return(SMB_ERR_HDR_VER);
	}
	l=sizeof(msghdr_t);
	if(msg->hdr.total_dfields && (msg->dfield
		=(dfield_t *)MALLOC(sizeof(dfield_t)*msg->hdr.total_dfields))==NULL) {
		smb_freemsgmem(msg);
		sprintf(smb->last_error,"malloc failure of %d bytes for %d data fields"
			,(int)sizeof(dfield_t)*msg->hdr.total_dfields, msg->hdr.total_dfields);
		return(SMB_ERR_MEM); 
	}
	i=0;
	while(i<msg->hdr.total_dfields && l<msg->hdr.length) {
		if(!fread(&msg->dfield[i],sizeof(dfield_t),1,smb->shd_fp)) {
			smb_freemsgmem(msg);
			sprintf(smb->last_error,"%d (%s) reading data field %d"
				,errno,STRERROR(errno),i);
			return(SMB_ERR_READ); 
		}
		i++;
		l+=sizeof(dfield_t); 
	}
	if(i<msg->hdr.total_dfields) {
		smb_freemsgmem(msg);
		sprintf(smb->last_error,"insufficient data fields read (%d instead of %d)"
			,i,msg->hdr.total_dfields);
		return(SMB_ERR_READ); 
	}
	while(l<msg->hdr.length) {
		i=msg->total_hfields;
		if((vpp=(void* *)REALLOC(msg->hfield_dat,sizeof(void* )*(i+1)))==NULL) {
			smb_freemsgmem(msg);
			sprintf(smb->last_error
				,"realloc failure of %d bytes for header field data"
				,(int)sizeof(void*)*(i+1));
			return(SMB_ERR_MEM); 
		}
		msg->hfield_dat=vpp;
		if((vp=(hfield_t *)REALLOC(msg->hfield,sizeof(hfield_t)*(i+1)))==NULL) {
			smb_freemsgmem(msg);
			sprintf(smb->last_error
				,"realloc failure of %d bytes for header fields"
				,(int)sizeof(hfield_t)*(i+1));
			return(SMB_ERR_MEM); 
		}
		msg->hfield=vp;
		msg->total_hfields++;
		if(!fread(&msg->hfield[i],sizeof(hfield_t),1,smb->shd_fp)) {
			smb_freemsgmem(msg);
			sprintf(smb->last_error,"%d (%s) reading header field"
				,errno,STRERROR(errno));
			return(SMB_ERR_READ); 
		}
		l+=sizeof(hfield_t);
		if((msg->hfield_dat[i]=(char*)MALLOC(msg->hfield[i].length+1))
			==NULL) {			/* Allocate 1 extra for ASCIIZ terminator */
			sprintf(smb->last_error
				,"malloc failure of %d bytes for header field %d"
				,msg->hfield[i].length+1, i);
			smb_freemsgmem(msg);  /* or 0 length field */
			return(SMB_ERR_MEM); 
		}
		memset(msg->hfield_dat[i],0,msg->hfield[i].length+1);  /* init to NULL */
		if(msg->hfield[i].length
			&& !fread(msg->hfield_dat[i],msg->hfield[i].length,1,smb->shd_fp)) {
			smb_freemsgmem(msg);
			sprintf(smb->last_error,"%d (%s) reading header field data"
				,errno,STRERROR(errno));
			return(SMB_ERR_READ); 
		}
		set_convenience_ptr(msg,msg->hfield[i].type,msg->hfield_dat[i]);

		l+=msg->hfield[i].length; 
	}

	if(!msg->from || !msg->to || !msg->subj) {
		sprintf(smb->last_error,"missing required header field (from/to/subj)");
		smb_freemsgmem(msg);
		return(SMB_ERR_HDR_FIELD); 
	}

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
int SMBCALL smb_copymsgmem(smbmsg_t* msg, smbmsg_t* srcmsg)
{
	int i;

	memcpy(msg,srcmsg,sizeof(smbmsg_t));

	/* data field types/lengths */
	if((msg->dfield=(dfield_t *)MALLOC(msg->hdr.total_dfields*sizeof(dfield_t)))==NULL)
		return(SMB_ERR_MEM);
	memcpy(msg->dfield,srcmsg->dfield,msg->hdr.total_dfields*sizeof(dfield_t));

	/* header field types/lengths */
	if((msg->hfield=(hfield_t *)MALLOC(msg->total_hfields*sizeof(hfield_t)))==NULL)
		return(SMB_ERR_MEM);
	memcpy(msg->hfield,srcmsg->hfield,msg->total_hfields*sizeof(hfield_t));

	/* header field data */
	if((msg->hfield_dat=(void**)MALLOC(msg->total_hfields*sizeof(void*)))==NULL)
		return(SMB_ERR_MEM);

	for(i=0;i<msg->total_hfields;i++) {
		if((msg->hfield_dat[i]=(void*)MALLOC(msg->hfield[i].length))==NULL)
			return(SMB_ERR_MEM);
		memcpy(msg->hfield_dat[i],srcmsg->hfield_dat[i],msg->hfield[i].length);
	}

	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Unlocks header for 'msg'                                                 */
/****************************************************************************/
int SMBCALL smb_unlockmsghdr(smb_t* smb, smbmsg_t* msg)
{
	if(smb->shd_fp==NULL) {
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
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

	if(msg->hdr.length==0) 	/* uninitialized */
		msg->hdr.length=(ushort)smb_getmsghdrlen(msg);

	if(msg->hdr.length+length>SMB_MAX_HDR_LEN)
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

	/* Maintain hdr.length so we don't exceed SMB_MAX_HDR_LEN */
	msg->hdr.length+=sizeof(hfield_t);
	msg->hdr.length+=length;
	return(SMB_SUCCESS);
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

	if(msg->hdr.length==0) 	/* uninitialized */
		msg->hdr.length=(ushort)smb_getmsghdrlen(msg);

	if(msg->hdr.length+length>SMB_MAX_HDR_LEN)
		return(SMB_ERR_HDR_LEN);

	if((p=(BYTE*)REALLOC(msg->hfield_dat[i],msg->hfield[i].length+length+1))==NULL) 
		return(SMB_ERR_MEM);	/* Allocate 1 extra for ASCIIZ terminator */

	msg->hfield_dat[i]=p;
	p+=msg->hfield[i].length;	/* skip existing data */
	memset(p,0,length+1);
	memcpy(p,data,length);		/* append */
	msg->hfield[i].length+=length;
	set_convenience_ptr(msg,type,msg->hfield_dat[i]);

	/* Maintain hdr.length so we don't exceed SMB_MAX_HDR_LEN */
	msg->hdr.length+=length;
	return(SMB_SUCCESS);
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
	char	str[128];
	int 	file;
	long	length;
	ulong	l,*buf;
	time_t	start=0;

	if(!smb->status.max_crcs)
		return(SMB_SUCCESS);

	sprintf(str,"%s.sch",smb->file);
	while(1) {
		if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW,S_IREAD|S_IWRITE))!=-1)
			break;
		if(errno!=EACCES && errno!=EAGAIN) {
			sprintf(smb->last_error,"%d (%s) opening %s"
				,errno,STRERROR(errno),str);
			return(SMB_ERR_OPEN);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {
				sprintf(smb->last_error,"timeout opening %s (retry_time=%ld)"
					,str,smb->retry_time);
				return(SMB_ERR_TIMEOUT); 
			}
		SLEEP(smb->retry_delay);
	}

	length=filelength(file);
	if(length<0L) {
		close(file);
		sprintf(smb->last_error,"invalid file length: %ld", length);
		return(SMB_ERR_FILE_LEN); 
	}
	if((buf=(ulong*)MALLOC(smb->status.max_crcs*4))==NULL) {
		close(file);
		sprintf(smb->last_error
			,"malloc failure of %ld bytes"
			,smb->status.max_crcs*4);
		return(SMB_ERR_MEM); 
	}
	if((ulong)length>=smb->status.max_crcs*4L) { /* Reached or exceeds max crcs */
		read(file,buf,smb->status.max_crcs*4);
		for(l=0;l<smb->status.max_crcs;l++)
			if(crc==buf[l])
				break;
		if(l<smb->status.max_crcs) {				/* Dupe CRC found */
			close(file);
			FREE(buf);
			sprintf(smb->last_error
				,"duplicate message detected");
			return(SMB_DUPE_MSG);
		}
		chsize(file,0L);				/* truncate it */
		lseek(file,0L,SEEK_SET);
		write(file,buf+4,(smb->status.max_crcs-1)*4); 
	}

	else if(length/4) { 					/* Less than max crcs */
		read(file,buf,length);
		for(l=0;l<(ulong)(length/4);l++)
			if(crc==buf[l])
				break;
		if(l<(ulong)(length/4L)) {					/* Dupe CRC found */
			close(file);
			FREE(buf);
			sprintf(smb->last_error
				,"duplicate message detected");
			return(SMB_DUPE_MSG);
		} 
	}

	lseek(file,0L,SEEK_END);
	write(file,&crc,sizeof(crc)); 			   /* Write to the end */
	FREE(buf);
	close(file);
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

	hdrlen=smb_getmsghdrlen(msg);
	if(hdrlen>SMB_MAX_HDR_LEN) { /* headers are limited to 64k in size */
		sprintf(smb->last_error
			,"illegal message header length (%lu > %u)"
			,hdrlen,SMB_MAX_HDR_LEN);
		return(SMB_ERR_HDR_LEN);
	}

	if(!smb->locked && smb_locksmbhdr(smb))
		return(SMB_ERR_LOCK);
	if((i=smb_getstatus(smb))!=0) {
		smb_unlocksmbhdr(smb);
		return(i);
	}

	if(storage!=SMB_HYPERALLOC && (i=smb_open_ha(smb))!=0) {
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
	if(i==0) {	/* success */
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
		sprintf(smb->last_error,"index not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sid_fp);
	if(fseek(smb->sid_fp,msg->offset*sizeof(idxrec_t),SEEK_SET)) {
		sprintf(smb->last_error,"%d (%s) seeking to %u in header"
			,errno,STRERROR(errno)
			,(unsigned)(msg->offset*sizeof(idxrec_t)));
		return(SMB_ERR_SEEK);
	}
	if(!fwrite(&msg->idx,sizeof(idxrec_t),1,smb->sid_fp)) {
		sprintf(smb->last_error,"%d (%s) writing index",errno,STRERROR(errno));
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
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	if(msg->idx.offset<sizeof(smbhdr_t)+sizeof(smbstatus_t) 
		|| msg->idx.offset<smb->status.header_offset) {
		sprintf(smb->last_error,"invalid header offset: %ld",msg->idx.offset);
		return(SMB_ERR_HDR_OFFSET);
	}
	clearerr(smb->shd_fp);
	if(fseek(smb->shd_fp,msg->idx.offset,SEEK_SET)) {
		sprintf(smb->last_error,"%d (%s) seeking to %ld in index"
			,errno,STRERROR(errno),msg->idx.offset);
		return(SMB_ERR_SEEK);
	}

	/* Verify that the number of blocks required to stored the actual 
	   (calculated) header length does not exceed the number allocated. */
	hdrlen=smb_getmsghdrlen(msg);
	if(hdrlen>SMB_MAX_HDR_LEN) { /* headers are limited to 64k in size */
		sprintf(smb->last_error
			,"illegal message header length (%lu > %u)"
			,hdrlen,SMB_MAX_HDR_LEN);
		return(SMB_ERR_HDR_LEN);
	}
	if(smb_hdrblocks(hdrlen) > smb_hdrblocks(msg->hdr.length)) {
		sprintf(smb->last_error,"illegal header length increase: "
			"%lu (%lu blocks) vs %hu (%lu blocks)"
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
		sprintf(smb->last_error,"%d (%s) writing fixed portion of header record"
			,errno,STRERROR(errno));
		return(SMB_ERR_WRITE);
	}

	/************************************************/
	/* Write the data fields (each is fixed length) */
	/************************************************/
	for(i=0;i<msg->hdr.total_dfields;i++)
		if(!fwrite(&msg->dfield[i],sizeof(dfield_t),1,smb->shd_fp)) {
			sprintf(smb->last_error,"%d (%s) writing data field"
				,errno,STRERROR(errno));
			return(SMB_ERR_WRITE);
		}

	/*******************************************/
	/* Write the variable length header fields */
	/*******************************************/
	for(i=0;i<msg->total_hfields;i++) {
		if(!fwrite(&msg->hfield[i],sizeof(hfield_t),1,smb->shd_fp)) {
			sprintf(smb->last_error,"%d (%s) writing header field"
				,errno,STRERROR(errno));
			return(SMB_ERR_WRITE);
		}
		if(msg->hfield[i].length					 /* more then 0 bytes long */
			&& !fwrite(msg->hfield_dat[i],msg->hfield[i].length,1,smb->shd_fp)) {
			sprintf(smb->last_error,"%d (%s) writing header field data"
				,errno,STRERROR(errno));
			return(SMB_ERR_WRITE); 
		}
	}

	while(hdrlen%SHD_BLOCK_LEN) {
		if(fputc(0,smb->shd_fp)==EOF) {
			sprintf(smb->last_error,"%d (%s) padding header block"
				,errno,STRERROR(errno));
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
    char        str[128];
	smbhdr_t	hdr;

	if(smb->shd_fp==NULL || smb->sdt_fp==NULL || smb->sid_fp==NULL) {
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	if(filelength(fileno(smb->shd_fp))>=sizeof(smbhdr_t)+sizeof(smbstatus_t)
		&& smb_locksmbhdr(smb))  /* header exists, so lock it */
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

	sprintf(str,"%s.sda",smb->file);
	remove(str);						/* if it exists, delete it */
	sprintf(str,"%s.sha",smb->file);
	remove(str);                        /* if it exists, delete it */
	sprintf(str,"%s.sch",smb->file);
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
long SMBCALL smb_allocdat(smb_t* smb, ulong length, ushort headers)
{
    ushort  i,j;
	ulong	l,blocks,offset=0L;

	if(smb->sda_fp==NULL) {
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	blocks=smb_datblocks(length);
	j=0;	/* j is consecutive unused block counter */
	fflush(smb->sda_fp);
	rewind(smb->sda_fp);
	while(!feof(smb->sda_fp)) {
		if(!fread(&i,2,1,smb->sda_fp))
			break;
		offset+=SDT_BLOCK_LEN;
		if(!i) j++;
		else   j=0;
		if(j==blocks) {
			offset-=(blocks*SDT_BLOCK_LEN);
			break; 
		} 
	}
	clearerr(smb->sda_fp);
	if(fseek(smb->sda_fp,(offset/SDT_BLOCK_LEN)*2L,SEEK_SET)) {
		return(SMB_ERR_SEEK);
	}
	for(l=0;l<blocks;l++)
		if(!fwrite(&headers,2,1,smb->sda_fp)) {
			sprintf(smb->last_error,"%d (%s) writing allocation bytes"
				,errno,STRERROR(errno));
			return(SMB_ERR_WRITE);
		}
	fflush(smb->sda_fp);
	return(offset);
}

/****************************************************************************/
/* Allocates space for data, but doesn't search for unused blocks           */
/* Returns negative on error												*/
/****************************************************************************/
long SMBCALL smb_fallocdat(smb_t* smb, ulong length, ushort headers)
{
	ulong	l,blocks,offset;

	if(smb->sda_fp==NULL) {
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	fflush(smb->sda_fp);
	clearerr(smb->sda_fp);
	blocks=smb_datblocks(length);
	if(fseek(smb->sda_fp,0L,SEEK_END)) {
		return(SMB_ERR_SEEK);
	}
	offset=(ftell(smb->sda_fp)/2L)*SDT_BLOCK_LEN;
	for(l=0;l<blocks;l++)
		if(!fwrite(&headers,2,1,smb->sda_fp))
			break;
	fflush(smb->sda_fp);
	if(l<blocks) {
		sprintf(smb->last_error,"%d (%s) writing allocation bytes"
			,errno,STRERROR(errno));
		return(SMB_ERR_WRITE);
	}
	return(offset);
}

/****************************************************************************/
/* De-allocates space for data												*/
/* Returns non-zero on error												*/
/****************************************************************************/
int SMBCALL smb_freemsgdat(smb_t* smb, ulong offset, ulong length
			, ushort headers)
{
	int		da_opened=0;
	int		retval=0;
	ushort	i;
	ulong	l,blocks;

	if(smb->status.attr&SMB_HYPERALLOC)	/* do nothing */
		return(SMB_SUCCESS);

	blocks=smb_datblocks(length);

	if(smb->sda_fp==NULL) {
		if((i=smb_open_da(smb))!=0)
			return(i);
		da_opened=1;
	}

	clearerr(smb->sda_fp);
	for(l=0;l<blocks;l++) {
		if(fseek(smb->sda_fp,((offset/SDT_BLOCK_LEN)+l)*2L,SEEK_SET)) {
			sprintf(smb->last_error
				,"%d (%s) seeking to %ld of allocation file"
				,errno,STRERROR(errno)
				,((offset/SDT_BLOCK_LEN)+l)*2L);
			retval=SMB_ERR_SEEK;
			break;
		}
		if(!fread(&i,2,1,smb->sda_fp)) {
			sprintf(smb->last_error,"%d (%s) reading allocation bytes"
				,errno,STRERROR(errno));
			retval=SMB_ERR_READ;
			break;
		}
		if(!headers || headers>i)
			i=0;			/* don't want to go negative */
		else
			i-=headers;
		if(fseek(smb->sda_fp,-2L,SEEK_CUR)) {
			sprintf(smb->last_error,"%d (%s) seeking backwards 2 bytes in allocation file"
				,errno,STRERROR(errno));
			retval=SMB_ERR_SEEK;
			break;
		}
		if(!fwrite(&i,2,1,smb->sda_fp)) {
			sprintf(smb->last_error,"%d (%s) writing allocation bytes"
				,errno,STRERROR(errno));
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
int SMBCALL smb_incdat(smb_t* smb, ulong offset, ulong length, ushort headers)
{
	ushort	i;
	ulong	l,blocks;

	if(smb->sda_fp==NULL) {
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sda_fp);
	blocks=smb_datblocks(length);
	for(l=0;l<blocks;l++) {
		if(fseek(smb->sda_fp,((offset/SDT_BLOCK_LEN)+l)*2L,SEEK_SET)) {
			return(SMB_ERR_SEEK);
		}
		if(!fread(&i,2,1,smb->sda_fp)) {
			sprintf(smb->last_error,"%d (%s) reading allocation record"
				,errno,STRERROR(errno));
			return(SMB_ERR_READ);
		}
		i+=headers;
		if(fseek(smb->sda_fp,-2L,SEEK_CUR)) {
			return(SMB_ERR_SEEK);
		}
		if(!fwrite(&i,2,1,smb->sda_fp)) {
			sprintf(smb->last_error,"%d (%s) writing allocation record"
				,errno,STRERROR(errno));
			return(SMB_ERR_WRITE); 
		}
	}
	fflush(smb->sda_fp);
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Increments data allocation records (message references) by number of		*/
/* headers specified (usually 1)											*/
/* The opposite function of smb_freemsg()									*/
/****************************************************************************/
int SMBCALL smb_incmsg(smb_t* smb, smbmsg_t* msg)
{
	int		i=0;
	int		da_opened=0;
	ushort	x;

	if(smb->status.attr&SMB_HYPERALLOC)  /* Nothing to do */
		return(SMB_SUCCESS);

	if(smb->sda_fp==NULL) {
		if((i=smb_open_da(smb))!=0)
			return(i);
		da_opened=1;
	}

	for(x=0;x<msg->hdr.total_dfields;x++) {
		if((i=smb_incdat(smb,msg->hdr.offset+msg->dfield[x].offset
			,msg->dfield[x].length,1))!=0)
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
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	clearerr(smb->sha_fp);
	blocks=smb_hdrblocks(length);
	if(fseek(smb->sha_fp,offset/SHD_BLOCK_LEN,SEEK_SET))
		return(SMB_ERR_SEEK);
	for(l=0;l<blocks;l++)
		if(!fwrite(&c,1,1,smb->sha_fp)) {
			sprintf(smb->last_error,"%d (%s) writing allocation record"
				,errno,STRERROR(errno));
			return(SMB_ERR_WRITE);
		}
	fflush(smb->sha_fp);
	return(SMB_SUCCESS);
}

/****************************************************************************/
/* Frees all allocated header and data blocks for 'msg'                     */
/****************************************************************************/
int SMBCALL smb_freemsg(smb_t* smb, smbmsg_t* msg)
{
	int 	i;
	ushort	x;

	if(smb->status.attr&SMB_HYPERALLOC)  /* Nothing to do */
		return(SMB_SUCCESS);

	for(x=0;x<msg->hdr.total_dfields;x++) {
		if((i=smb_freemsgdat(smb,msg->hdr.offset+msg->dfield[x].offset
			,msg->dfield[x].length,1))!=0)
			return(i); 
	}
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
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	blocks=smb_hdrblocks(length);
	i=0;	/* i is consecutive unused block counter */
	fflush(smb->sha_fp);
	rewind(smb->sha_fp);
	while(!feof(smb->sha_fp)) {
		if(!fread(&c,1,1,smb->sha_fp)) 
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
			sprintf(smb->last_error,"%d (%s) writing allocation record"
				,errno,STRERROR(errno));
			return(SMB_ERR_WRITE);
		}
	fflush(smb->sha_fp);
	return(offset);
}

/****************************************************************************/
/* Allocates space for index, but doesn't search for unused blocks          */
/* Returns negative value on error 												*/
/****************************************************************************/
long SMBCALL smb_fallochdr(smb_t* smb, ulong length)
{
	uchar	c=1;
	ulong	l,blocks,offset;

	if(smb->sha_fp==NULL) {
		sprintf(smb->last_error,"msgbase not open");
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
			sprintf(smb->last_error,"%d (%s) writing allocation record"
				,errno,STRERROR(errno));
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
	ulong l;

	if(smb->shd_fp==NULL) {
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	fflush(smb->shd_fp);
	if(fseek(smb->shd_fp,0L,SEEK_END))
		return(SMB_ERR_SEEK);
	l=ftell(smb->shd_fp);
	if(l<smb->status.header_offset) 			 /* Header file truncated?!? */
		return(smb->status.header_offset);
	while((l-smb->status.header_offset)%SHD_BLOCK_LEN)	/* Even block boundry */
		l++;
	return(l-smb->status.header_offset);
}

/************************************************************************/
/* Allocate data blocks using Hyper Allocation							*/
/* smb_locksmbhdr() should be called before this function and not		*/
/* unlocked until all data fields for this message have been written	*/
/* to the SDT file														*/
/************************************************************************/
long SMBCALL smb_hallocdat(smb_t* smb)
{
	long l;

	if(smb->sdt_fp==NULL) {
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	fflush(smb->sdt_fp);
	if(fseek(smb->sdt_fp,0L,SEEK_END))
		return(SMB_ERR_SEEK);
	l=ftell(smb->sdt_fp);
	if(l<=0)
		return(l);
	while(l%SDT_BLOCK_LEN)					/* Make sure even block boundry */
		l++;
	return(l);
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

long SMBCALL smb_fread(void HUGE16* buf, long bytes, FILE* fp)
{
#ifdef __FLAT__
	return(fread(buf,1,bytes,fp));
#else
	long count;

	for(count=bytes;count>0x7fff;count-=0x7fff,(char*)buf+=0x7fff)
		if(fread((char*)buf,1,0x7fff,fp)!=0x7fff)
			return(bytes-count);
	if(fread((char*)buf,1,(size_t)count,fp)!=(size_t)count)
		return(bytes-count);
	return(bytes);
#endif
}

long SMBCALL smb_fwrite(void HUGE16* buf, long bytes, FILE* fp)
{
#ifdef __FLAT__
	return(fwrite(buf,1,bytes,fp));
#else
	long count;

	for(count=bytes;count>0x7fff;count-=0x7fff,(char*)buf+=0x7fff)
		if(fwrite((char*)buf,1,0x7fff,fp)!=0x7fff)
			return(bytes-count);
	if(fwrite((char*)buf,1,(size_t)count,fp)!=(size_t)count)
		return(bytes-count);
	return(bytes);
#endif
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
		case SENDER:			return("SENDER");
		case SENDERAGENT:		return("SENDERAGENT");
		case SENDERNETTYPE:		return("SENDERNETTYPE");
		case SENDERNETADDR:		return("SENDERNETADDR");
		case SENDEREXT:			return("SENDEREXT");
		case SENDERORG:			return("SENDERORG");

		case REPLYTO:			return("REPLYTO");
		case REPLYTOAGENT:		return("REPLYTOAGENT");
		case REPLYTONETTYPE:	return("REPLYTONETTYPE");
		case REPLYTONETADDR:	return("REPLYTONETADDR");
		case REPLYTOEXT:		return("REPLYTOEXT");
								
		case RECIPIENT:			return("RECIPIENT");
		case RECIPIENTAGENT:	return("RECIPIENTAGENT");
		case RECIPIENTNETTYPE:	return("RECIPIENTNETTYPE");
		case RECIPIENTNETADDR:	return("RECIPIENTNETADDR");
		case RECIPIENTEXT:		return("RECIPIENTEXT");

		case SUBJECT:			return("SUBJECT");
		case SMB_SUMMARY:		return("SUMMARY");
		case SMB_COMMENT:		return("COMMENT");
		case SMB_CARBONCOPY:	return("CARBONCOPY");
		case SMB_GROUP:			return("GROUP");
		case SMB_EXPIRATION:	return("EXPIRATION");
		case SMB_PRIORITY:		return("PRIORITY");
		case SMB_COST:			return("COST");

		case FIDOCTRL:			return("FIDOCTRL");
		case FIDOAREA:			return("FIDOAREA");
		case FIDOSEENBY:		return("FIDOSEENBY");
		case FIDOPATH:			return("FIDOPATH");
		case FIDOMSGID:			return("FIDOMSGID");
		case FIDOREPLYID:		return("FIDOREPLYID");
		case FIDOPID:			return("FIDOPID");
		case FIDOFLAGS:			return("FIDOFLAGS");
		case FIDOTID:			return("FIDOTID");

		case RFC822HEADER:		return("RFC822HEADER");
		case RFC822MSGID:		return("RFC822MSGID");
		case RFC822REPLYID:		return("RFC822REPLYID");
		case RFC822TO:			return("RFC822TO");
		case RFC822FROM:		return("RFC822FROM");
		case RFC822REPLYTO:		return("RFC822REPLYTO");

		case USENETPATH:		return("USENETPATH");
		case USENETNEWSGROUPS:	return("USENETNEWSGROUPS");

		case SMTPCOMMAND:		return("SMTPCOMMAND");
		case SMTPREVERSEPATH:	return("SMTPREVERSEPATH");

		case SMTPSYSMSG:		return("SMTPSYSMSG");

		case UNKNOWN:			return("UNKNOWN");
		case UNKNOWNASCII:		return("UNKNOWNASCII");
		case UNUSED:			return("UNUSED");
	}
	sprintf(str,"%02Xh",type);
	return(str);
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

/* End of SMBLIB.C */

