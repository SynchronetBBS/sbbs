/* smblib.c */

/* Synchronet message base (SMB) library routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifndef __FreeBSD__
	#include <malloc.h>
#endif

#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* SMB-specific headers */
#include "smblib.h"
#include "genwrap.h"
#include "filewrap.h"

/* Use smb_ver() and smb_lib_ver() to obtain these values */
#define SMBLIB_VERSION		"2.16"      /* SMB library version */
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
	if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO))==-1) {
		sprintf(smb->last_error,"%d opening %s",errno,str);
		return(2); 
	}

	if((smb->shd_fp=fdopen(file,"r+b"))==NULL) {
		sprintf(smb->last_error,"%d fdopening %s (%d)",errno,str,file);
		close(file);
		return(4); 
	}

	if(filelength(file)>=sizeof(smbhdr_t)) {
		setvbuf(smb->shd_fp,smb->shd_buf,_IONBF,SHD_BLOCK_LEN);
		if(smb_locksmbhdr(smb)!=0) {
			smb_close(smb);
			/* smb_lockmsghdr set last_error */
			return(-1); 
		}
		memset(&hdr,0,sizeof(smbhdr_t));
		if(fread(&hdr,sizeof(smbhdr_t),1,smb->shd_fp)!=1) {
			sprintf(smb->last_error,"reading header");
			smb_close(smb);
			return(-10);
		}
		if(memcmp(hdr.id,SMB_HEADER_ID,LEN_HEADER_ID)) {
			sprintf(smb->last_error,"corrupt SMB header ID: %.*s",LEN_HEADER_ID,hdr.id);
			smb_close(smb);
			return(-2); 
		}
		if(hdr.version<0x110) {         /* Compatibility check */
			sprintf(smb->last_error,"insufficient header version: %X",hdr.version);
			smb_close(smb);
			return(-3); 
		}
		if(fread(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp)
			!=sizeof(smbstatus_t)) {
			sprintf(smb->last_error,"failed to read status");
			smb_close(smb);
			return(-4); 
		}
		smb_unlocksmbhdr(smb);
		rewind(smb->shd_fp); 
	}

	setvbuf(smb->shd_fp,smb->shd_buf,_IOFBF,SHD_BLOCK_LEN);

	sprintf(str,"%s.sdt",smb->file);
	if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO))==-1) {
		sprintf(smb->last_error,"%d opening %s",errno,str);
		smb_close(smb);
		return(1); 
	}

	if((smb->sdt_fp=fdopen(file,"r+b"))==NULL) {
		sprintf(smb->last_error,"%d fdopening %s (%d)",errno,str,file);
		close(file);
		smb_close(smb);
		return(5);
	}

	setvbuf(smb->sdt_fp,NULL,_IOFBF,2*1024);

	sprintf(str,"%s.sid",smb->file);
	if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYNO))==-1) {
		sprintf(smb->last_error,"%d opening %s",errno,str);
		smb_close(smb);
		return(3); 
	}

	if((smb->sid_fp=fdopen(file,"r+b"))==NULL) {
		sprintf(smb->last_error,"%d fdopening %s (%d)",errno,str,file);
		close(file);
		smb_close(smb);
		return(6); 
	}

	setvbuf(smb->sid_fp,NULL,_IOFBF,2*1024);

	return(0);
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
		if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW))!=-1)
			break;
		if(errno!=EACCES && errno!=EAGAIN) {
			sprintf(smb->last_error,"%d opening %s",errno,str);
			return(-1);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {
				sprintf(smb->last_error,"timeout opening %s (retry_time=%ld)"
					,str,smb->retry_time);
				return(-2); 
			}
		SLEEP(smb->retry_delay);
	}
	if((smb->sda_fp=fdopen(file,"r+b"))==NULL) {
		sprintf(smb->last_error,"%d fdopening %s (%d)",errno,str,file);
		close(file);
		return(-3); 
	}
	setvbuf(smb->sda_fp,NULL,_IOFBF,2*1024);
	return(0);
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
		if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW))!=-1)
			break;
		if(errno!=EACCES && errno!=EAGAIN) {
			sprintf(smb->last_error,"%d opening %s",errno,str);
			return(-1);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {
				sprintf(smb->last_error,"timeout opening %s (retry_time=%ld)"
					,str,smb->retry_time);
				return(-2); 
			}
		SLEEP(smb->retry_delay);
	}
	if((smb->sha_fp=fdopen(file,"r+b"))==NULL) {
		sprintf(smb->last_error,"%d fdopening %s (%d)",errno,str,file);
		close(file);
		return(-3); 
	}
	setvbuf(smb->sha_fp,NULL,_IOFBF,2*1024);
	return(0);
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
			return(1);
		}
		if(smb->shd_fp==NULL || smb->sdt_fp==NULL || smb->sid_fp==NULL)
			return(0);	  /* Msg base not open */
		memcpy(&stack[stack_idx],smb,sizeof(smb_t));
		stack_idx++;
		return(0); 
	}
	/* pop or xchng */
	if(!stack_idx)	/* Nothing on the stack, so do nothing */
		return(0);
	if(op==SMB_STACK_XCHNG) {
		if(smb->shd_fp==NULL)
			return(0);
		memcpy(&tmp_smb,smb,sizeof(smb_t));
	}

	stack_idx--;
	memcpy(smb,&stack[stack_idx],sizeof(smb_t));
	if(op==SMB_STACK_XCHNG) {
		memcpy(&stack[stack_idx],&tmp_smb,sizeof(smb_t));
		stack_idx++;
	}
	return(0);
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
			sprintf(smb->last_error,"%d changing header file size",errno);
			return(-1);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {	 /* Time-out */
				sprintf(smb->last_error,"timeout changing header file size (retry_time=%ld)"
					,smb->retry_time);
				return(-2); 
			}
		SLEEP(smb->retry_delay);
	}
	return(0);
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
			return(0);
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
	return(-1);
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
	fseek(smb->shd_fp,sizeof(smbhdr_t),SEEK_SET);
	i=fread(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp);
	setvbuf(smb->shd_fp,smb->shd_buf,_IOFBF,SHD_BLOCK_LEN);
	if(i==sizeof(smbstatus_t))
		return(0);
	sprintf(smb->last_error,"read %d instead of %d",i,sizeof(smbstatus_t));
	return(1);
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
	fseek(smb->shd_fp,sizeof(smbhdr_t),SEEK_SET);
	i=fwrite(&(smb->status),1,sizeof(smbstatus_t),smb->shd_fp);
	fflush(smb->shd_fp);
	if(i==sizeof(smbstatus_t))
		return(0);
	sprintf(smb->last_error,"wrote %d instead of %d",i,sizeof(smbstatus_t));
	return(1);
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
			return(0);
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
	return(-1);
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
		fseek(smb->sid_fp,msg->offset*sizeof(idxrec_t),SEEK_SET);
		if(!fread(&msg->idx,sizeof(idxrec_t),1,smb->sid_fp)) {
			sprintf(smb->last_error,"reading index");
			return(1);
		}
		return(0); 
	}

	length=filelength(fileno(smb->sid_fp));
	if(!length) {
		sprintf(smb->last_error,"invalid index file length: %ld",length);
		return(1);
	}
	total=length/sizeof(idxrec_t);
	if(!total) {
		sprintf(smb->last_error,"invalid index file length: %ld",length);
		return(1);
	}

	bot=0;
	top=total;
	l=total/2; /* Start at middle index */
	while(1) {
		fseek(smb->sid_fp,l*sizeof(idxrec_t),SEEK_SET);
		if(!fread(&idx,sizeof(idxrec_t),1,smb->sid_fp)) {
			sprintf(smb->last_error,"reading index");
			return(1);
		}
		if(bot==top-1 && idx.number!=msg->hdr.number) {
			sprintf(smb->last_error,"msg %ld not found",msg->hdr.number);
			return(1);
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
	return(0);
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
	fseek(smb->sid_fp,0,SEEK_SET);
	if(!fread(idx,sizeof(idxrec_t),1,smb->sid_fp)) {
		sprintf(smb->last_error,"reading index");
		return(-2);
	}
	return(0);
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
		return(-1);
	}
	fseek(smb->sid_fp,length-sizeof(idxrec_t),SEEK_SET);
	if(!fread(idx,sizeof(idxrec_t),1,smb->sid_fp)) {
		sprintf(smb->last_error,"reading index");
		return(-2);
	}
	return(0);
}

/****************************************************************************/
/* Figures out the total length of the header record for 'msg'              */
/* Returns length															*/
/****************************************************************************/
uint SMBCALL smb_getmsghdrlen(smbmsg_t* msg)
{
	int i;

	/* fixed portion */
	msg->hdr.length=sizeof(msghdr_t);
	/* data fields */
	msg->hdr.length+=msg->hdr.total_dfields*sizeof(dfield_t);
	/* header fields */
	for(i=0;i<msg->total_hfields;i++) {
		msg->hdr.length+=sizeof(hfield_t);
		msg->hdr.length+=msg->hfield[i].length; 
	}
	return(msg->hdr.length);
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
	fseek(smb->shd_fp,msg->idx.offset,SEEK_SET);
	idx=msg->idx;
	offset=msg->offset;
	memset(msg,0,sizeof(smbmsg_t));
	msg->idx=idx;
	msg->offset=offset;
	if(!fread(&msg->hdr,sizeof(msghdr_t),1,smb->shd_fp)) {
		sprintf(smb->last_error,"reading msg header");
		return(-1);
	}
	if(memcmp(msg->hdr.id,SHD_HEADER_ID,LEN_HEADER_ID)) {
		sprintf(smb->last_error,"corrupt message header ID: %.*s",LEN_HEADER_ID,msg->hdr.id);
		return(-2);
	}
	if(msg->hdr.version<0x110) {
		sprintf(smb->last_error,"insufficient header version: %X",msg->hdr.version);
		return(-9);
	}
	l=sizeof(msghdr_t);
	if(msg->hdr.total_dfields && (msg->dfield
		=(dfield_t *)MALLOC(sizeof(dfield_t)*msg->hdr.total_dfields))==NULL) {
		smb_freemsgmem(msg);
		sprintf(smb->last_error,"malloc failure of %d bytes for %d data fields"
			,sizeof(dfield_t)*msg->hdr.total_dfields, msg->hdr.total_dfields);
		return(-3); 
	}
	i=0;
	while(i<msg->hdr.total_dfields && l<msg->hdr.length) {
		if(!fread(&msg->dfield[i],sizeof(dfield_t),1,smb->shd_fp)) {
			smb_freemsgmem(msg);
			sprintf(smb->last_error,"reading data field %d",i);
			return(-4); 
		}
		i++;
		l+=sizeof(dfield_t); 
	}
	if(i<msg->hdr.total_dfields) {
		smb_freemsgmem(msg);
		sprintf(smb->last_error,"insufficient data fields read (%d instead of %d)"
			,i,msg->hdr.total_dfields);
		return(-8); 
	}
	while(l<msg->hdr.length) {
		i=msg->total_hfields;
		if((vpp=(void* *)REALLOC(msg->hfield_dat,sizeof(void* )*(i+1)))==NULL) {
			smb_freemsgmem(msg);
			sprintf(smb->last_error
				,"realloc failure of %d bytes for header field data"
				,sizeof(void*)*(i+1));
			return(-3); 
		}
		msg->hfield_dat=vpp;
		if((vp=(hfield_t *)REALLOC(msg->hfield,sizeof(hfield_t)*(i+1)))==NULL) {
			smb_freemsgmem(msg);
			sprintf(smb->last_error
				,"realloc failure of %d bytes for header fields"
				,sizeof(hfield_t)*(i+1));
			return(-3); 
		}
		msg->hfield=vp;
		msg->total_hfields++;
		if(!fread(&msg->hfield[i],sizeof(hfield_t),1,smb->shd_fp)) {
			smb_freemsgmem(msg);
			sprintf(smb->last_error,"reading header field");
			return(-5); 
		}
		l+=sizeof(hfield_t);
		if((msg->hfield_dat[i]=(char*)MALLOC(msg->hfield[i].length+1))
			==NULL) {			/* Allocate 1 extra for NULL terminator */
			sprintf(smb->last_error
				,"malloc failure of %d bytes for header field %d"
				,msg->hfield[i].length+1, i);
			smb_freemsgmem(msg);  /* or 0 length field */
			return(-3); 
		}
		memset(msg->hfield_dat[i],0,msg->hfield[i].length+1);  /* init to NULL */
		if(msg->hfield[i].length
			&& !fread(msg->hfield_dat[i],msg->hfield[i].length,1,smb->shd_fp)) {
			smb_freemsgmem(msg);
			sprintf(smb->last_error,"reading header field data");
			return(-6); 
		}
		switch(msg->hfield[i].type) {	/* convenience variables */
			case SENDER:
				if(!msg->from) {
					msg->from=msg->hfield_dat[i];
					break; 
				}
			case FORWARDED: 	/* fall through */
				msg->forwarded=1;
				break;
			case SENDERAGENT:
				if(!msg->forwarded)
					msg->from_agent=*(ushort *)msg->hfield_dat[i];
				break;
			case SENDEREXT:
				if(!msg->forwarded)
					msg->from_ext=msg->hfield_dat[i];
				break;
			case SENDERNETTYPE:
				if(!msg->forwarded)
					msg->from_net.type=*(ushort *)msg->hfield_dat[i];
				break;
			case SENDERNETADDR:
				if(!msg->forwarded)
					msg->from_net.addr=msg->hfield_dat[i];
				break;
			case REPLYTO:
				msg->replyto=msg->hfield_dat[i];
				break;
			case REPLYTOEXT:
				msg->replyto_ext=msg->hfield_dat[i];
				break;
			case REPLYTOAGENT:
				msg->replyto_agent=*(ushort *)msg->hfield_dat[i];
				break;
			case REPLYTONETTYPE:
				msg->replyto_net.type=*(ushort *)msg->hfield_dat[i];
				break;
			case REPLYTONETADDR:
				msg->replyto_net.addr=msg->hfield_dat[i];
				break;
			case RECIPIENT:
				msg->to=msg->hfield_dat[i];
				break;
			case RECIPIENTEXT:
				msg->to_ext=msg->hfield_dat[i];
				break;
			case RECIPIENTAGENT:
				msg->to_agent=*(ushort *)msg->hfield_dat[i];
				break;
			case RECIPIENTNETTYPE:
				msg->to_net.type=*(ushort *)msg->hfield_dat[i];
				break;
			case RECIPIENTNETADDR:
				msg->to_net.addr=msg->hfield_dat[i];
				break;
			case SUBJECT:
				msg->subj=msg->hfield_dat[i];
				break;
			case RFC822MSGID:
				msg->id=msg->hfield_dat[i];
				break;
			case RFC822REPLYID:
				msg->reply_id=msg->hfield_dat[i];
				break;
			case USENETPATH:
				msg->path=msg->hfield_dat[i];
				break;
			case USENETNEWSGROUPS:
				msg->newsgroups=msg->hfield_dat[i];
				break;
			case FIDOMSGID:
				msg->ftn_msgid=msg->hfield_dat[i];
				break;
			case FIDOREPLYID:
				msg->ftn_reply=msg->hfield_dat[i];
				break;
			case FIDOAREA:
				msg->ftn_area=msg->hfield_dat[i];
				break;
			case FIDOPID:
				msg->ftn_pid=msg->hfield_dat[i];
				break;
			case FIDOFLAGS:
				msg->ftn_flags=msg->hfield_dat[i];
				break;
			
		}
		l+=msg->hfield[i].length; 
	}

	if(!msg->from || !msg->to || !msg->subj) {
		sprintf(smb->last_error,"missing required header field (from/to/subj)");
		smb_freemsgmem(msg);
		return(-7); 
	}
	return(0);
}

/****************************************************************************/
/* Frees memory allocated for 'msg'                                         */
/****************************************************************************/
void SMBCALL smb_freemsgmem(smbmsg_t* msg)
{
	ushort	i;

	if(msg->dfield) {
		FREE(msg->dfield);
		msg->dfield=NULL;
	}
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
}

/****************************************************************************/
/* Copies memory allocated for 'srcmsg' to 'msg'							*/
/****************************************************************************/
int SMBCALL smb_copymsgmem(smbmsg_t* msg, smbmsg_t* srcmsg)
{
	int i;

	memcpy(msg,srcmsg,sizeof(smbmsg_t));

	/* data field types/lengths */
	if((msg->dfield=MALLOC(msg->hdr.total_dfields*sizeof(dfield_t)))==NULL)
		return(1);
	memcpy(msg->dfield,srcmsg->dfield,msg->hdr.total_dfields*sizeof(dfield_t));

	/* header field types/lengths */
	if((msg->hfield=MALLOC(msg->total_hfields*sizeof(hfield_t)))==NULL)
		return(2);
	memcpy(msg->hfield,srcmsg->hfield,msg->total_hfields*sizeof(hfield_t));

	/* header field data */
	if((msg->hfield_dat=MALLOC(msg->total_hfields*sizeof(void*)))==NULL)
		return(3);

	for(i=0;i<msg->total_hfields;i++) {
		if((msg->hfield_dat[i]=(char*)MALLOC(msg->hfield[i].length))==NULL)
			return(4);
		memcpy(msg->hfield_dat[i],srcmsg->hfield_dat[i],msg->hfield[i].length);
	}

	return(0);
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
	void* vp,**vpp;
	int i;

	i=msg->total_hfields;
	if((vp=(hfield_t *)REALLOC(msg->hfield,sizeof(hfield_t)*(i+1)))==NULL) 
		return(1);

	msg->hfield=vp;
	if((vpp=(void* *)REALLOC(msg->hfield_dat,sizeof(void* )*(i+1)))==NULL) 
		return(2);
	
	msg->hfield_dat=vpp;
	msg->total_hfields++;
	msg->hfield[i].type=type;
	msg->hfield[i].length=length;
	if(length) {
		if((msg->hfield_dat[i]=(void* )MALLOC(length))==NULL) 
			return(4);
		memcpy(msg->hfield_dat[i],data,length); 
	}
	else
		msg->hfield_dat[i]=NULL;
	return(0);
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
	void* vp;
	int i,j;

	i=msg->hdr.total_dfields;
	if((vp=(dfield_t *)REALLOC(msg->dfield,sizeof(dfield_t)*(i+1)))==NULL) 
		return(1);
	
	msg->dfield=vp;
	msg->hdr.total_dfields++;
	msg->dfield[i].type=type;
	msg->dfield[i].length=length;
	for(j=msg->dfield[i].offset=0;j<i;j++)
		msg->dfield[i].offset+=msg->dfield[j].length;
	return(0);
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
		return(0);

	sprintf(str,"%s.sch",smb->file);
	while(1) {
		if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW))!=-1)
			break;
		if(errno!=EACCES && errno!=EAGAIN) {
			sprintf(smb->last_error,"%d opening %s", errno, str);
			return(-1);
		}
		if(!start)
			start=time(NULL);
		else
			if(time(NULL)-start>=(time_t)smb->retry_time) {
				sprintf(smb->last_error,"timeout opening %s (retry_time=%ld)"
					,str,smb->retry_time);
				return(-2); 
			}
		SLEEP(smb->retry_delay);
	}

	length=filelength(file);
	if(length<0L) {
		close(file);
		sprintf(smb->last_error,"invalid file length: %ld", length);
		return(-4); 
	}
	if((buf=(ulong*)MALLOC(smb->status.max_crcs*4))==NULL) {
		close(file);
		sprintf(smb->last_error
			,"malloc failure of %ld bytes"
			,smb->status.max_crcs*4);
		return(-3); 
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
			return(1); 
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
			return(1); 
		} 
	}

	lseek(file,0L,SEEK_END);
	write(file,&crc,sizeof(crc)); 			   /* Write to the end */
	FREE(buf);
	close(file);
	return(0);
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
	int i;
	long l;

	if(!smb->locked && smb_locksmbhdr(smb))
		return(1);
	if(smb_getstatus(smb)) {
		smb_unlocksmbhdr(smb);
		return(2);
	}

	if(storage!=SMB_HYPERALLOC && (i=smb_open_ha(smb))!=0) {
		smb_unlocksmbhdr(smb);
		return(i);
	}

	msg->hdr.length=smb_getmsghdrlen(msg);
	if(storage==SMB_HYPERALLOC)
		l=smb_hallochdr(smb);
	else if(storage==SMB_FASTALLOC)
		l=smb_fallochdr(smb,msg->hdr.length);
	else
		l=smb_allochdr(smb,msg->hdr.length);
	if(storage!=SMB_HYPERALLOC)
		smb_close_ha(smb);
	if(l==-1L) {
		smb_unlocksmbhdr(smb);
		return(-1); 
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
	fseek(smb->sid_fp,msg->offset*sizeof(idxrec_t),SEEK_SET);
	if(!fwrite(&msg->idx,sizeof(idxrec_t),1,smb->sid_fp)) {
		sprintf(smb->last_error,"writing index");
		return(1);
	}
	fflush(smb->sid_fp);
	return(0);
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
	ulong	l;

	if(smb->shd_fp==NULL) {
		sprintf(smb->last_error,"msgbase not open");
		return(SMB_ERR_NOT_OPEN);
	}
	if(msg->idx.offset<sizeof(smbhdr_t)+sizeof(smbstatus_t) 
		|| msg->idx.offset<smb->status.header_offset) {
		sprintf(smb->last_error,"invalid header offset: %ld",msg->idx.offset);
		return(-7);
	}
	clearerr(smb->shd_fp);
	if(fseek(smb->shd_fp,msg->idx.offset,SEEK_SET)) {
		sprintf(smb->last_error,"seeking to %ld in index",msg->idx.offset);
		return(-1);
	}

	/**********************************/
	/* Set the message header ID here */
	/**********************************/
	memcpy(&msg->hdr.id,SHD_HEADER_ID,LEN_HEADER_ID);

	/************************************************/
	/* Write the fixed portion of the header record */
	/************************************************/
	if(!fwrite(&msg->hdr,sizeof(msghdr_t),1,smb->shd_fp)) {
		sprintf(smb->last_error,"writing fixed portion of header record");
		return(-2);
	}

	/************************************************/
	/* Write the data fields (each is fixed length) */
	/************************************************/
	for(i=0;i<msg->hdr.total_dfields;i++)
		if(!fwrite(&msg->dfield[i],sizeof(dfield_t),1,smb->shd_fp)) {
			sprintf(smb->last_error,"writing data field");
			return(-3);
		}

	/*******************************************/
	/* Write the variable length header fields */
	/*******************************************/
	for(i=0;i<msg->total_hfields;i++) {
		if(!fwrite(&msg->hfield[i],sizeof(hfield_t),1,smb->shd_fp)) {
			sprintf(smb->last_error,"writing header field");
			return(-4);
		}
		if(msg->hfield[i].length					 /* more then 0 bytes long */
			&& !fwrite(msg->hfield_dat[i],msg->hfield[i].length,1,smb->shd_fp)) {
			sprintf(smb->last_error,"writing header field data");
			return(-5); 
		}
	}

	l=smb_getmsghdrlen(msg);
	while(l%SHD_BLOCK_LEN) {
		if(fputc(0,smb->shd_fp)==EOF) {
			sprintf(smb->last_error,"padding header block");
			return(-6); 			   /* pad block with NULL */
		}
		l++; 
	}
	fflush(smb->shd_fp);
	return(0);
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
		return(1);
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
	return(0);
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
	fseek(smb->sda_fp,(offset/SDT_BLOCK_LEN)*2L,SEEK_SET);
	for(l=0;l<blocks;l++)
		if(!fwrite(&headers,2,1,smb->sda_fp)) {
			sprintf(smb->last_error,"writing allocation bytes");
			return(-1);
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
	fseek(smb->sda_fp,0L,SEEK_END);
	offset=(ftell(smb->sda_fp)/2L)*SDT_BLOCK_LEN;
	for(l=0;l<blocks;l++)
		if(!fwrite(&headers,2,1,smb->sda_fp))
			break;
	fflush(smb->sda_fp);
	if(l<blocks) {
		sprintf(smb->last_error,"writing allocation bytes");
		return(-1L);
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
		return(0);

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
				,"seeking to %ld of allocation file"
				,((offset/SDT_BLOCK_LEN)+l)*2L);
			retval=1;
			break;
		}
		if(!fread(&i,2,1,smb->sda_fp)) {
			sprintf(smb->last_error,"reading allocation bytes");
			retval=2;
			break;
		}
		if(!headers || headers>i)
			i=0;			/* don't want to go negative */
		else
			i-=headers;
		if(fseek(smb->sda_fp,-2L,SEEK_CUR)) {
			sprintf(smb->last_error,"seeking backwards 2 bytes in allocation file");
			retval=3;
			break;
		}
		if(!fwrite(&i,2,1,smb->sda_fp)) {
			sprintf(smb->last_error,"writing allocation bytes");
			retval=4; 
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
		fseek(smb->sda_fp,((offset/SDT_BLOCK_LEN)+l)*2L,SEEK_SET);
		if(!fread(&i,2,1,smb->sda_fp)) {
			sprintf(smb->last_error,"reading allocation record");			
			return(1);
		}
		i+=headers;
		fseek(smb->sda_fp,-2L,SEEK_CUR);
		if(!fwrite(&i,2,1,smb->sda_fp)) {
			sprintf(smb->last_error,"writing allocation record");
			return(2); 
		}
	}
	fflush(smb->sda_fp);
	return(0);
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
	fseek(smb->sha_fp,offset/SHD_BLOCK_LEN,SEEK_SET);
	for(l=0;l<blocks;l++)
		if(!fwrite(&c,1,1,smb->sha_fp)) {
			sprintf(smb->last_error,"writing allocation record");
			return(1);
		}
	fflush(smb->sha_fp);
	return(0);
}

/****************************************************************************/
/* Frees all allocated header and data blocks for 'msg'                     */
/****************************************************************************/
int SMBCALL smb_freemsg(smb_t* smb, smbmsg_t* msg)
{
	int 	i;
	ushort	x;

	if(smb->status.attr&SMB_HYPERALLOC)  /* Nothing to do */
		return(0);

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
/* Returns -1L on error 													*/
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
	fseek(smb->sha_fp,offset/SHD_BLOCK_LEN,SEEK_SET);
	c=1;
	for(l=0;l<blocks;l++)
		if(!fwrite(&c,1,1,smb->sha_fp)) {
			sprintf(smb->last_error,"writing allocation record");
			return(-1L);
		}
	fflush(smb->sha_fp);
	return(offset);
}

/****************************************************************************/
/* Allocates space for index, but doesn't search for unused blocks          */
/* Returns -1L on error 													*/
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
	fseek(smb->sha_fp,0L,SEEK_END);
	offset=ftell(smb->sha_fp)*SHD_BLOCK_LEN;
	for(l=0;l<blocks;l++)
		if(!fwrite(&c,1,1,smb->sha_fp)) {
			sprintf(smb->last_error,"writing allocation record");
			return(-1L);
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
	fseek(smb->shd_fp,0L,SEEK_END);
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
	fseek(smb->sdt_fp,0L,SEEK_END);
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

/* End of SMBLIB.C */
