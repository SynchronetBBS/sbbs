/* sbbsexec.c */

/* Synchronet Windows NT/2000 VDD for FOSSIL and DOS I/O Interrupts */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
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

#include <windows.h> 
#include <stdio.h>
#include <vddsvc.h>
#include "vdd_func.h"
#include "ringbuf.h"

#define RINGBUF_SIZE_IN			10000
#define DEFAULT_MAX_MSG_SIZE	4000

__declspec(dllexport) void __cdecl VDDDispatch(void) 
{
	char			str[512];
	char			buf[5000];
	DWORD			count;
	DWORD			msgs;
	int				retval;
	int				node_num;
	BYTE*			p;
	vdd_status_t*	status;
	static  DWORD	writes;
	static  DWORD	bytes_written;
	static	DWORD	reads;
	static  DWORD	bytes_read;
	static  DWORD	inbuf_poll;
	static	DWORD	online_poll;
	static	DWORD	status_poll;
	static	DWORD	yields;
	static  HANDLE	hungup_event=NULL;
	static  HANDLE	rdslot=INVALID_HANDLE_VALUE;
	static  HANDLE	wrslot=INVALID_HANDLE_VALUE;
	static  RingBuf	rdbuf;
	static	FILE*	fp=NULL;

	retval=0;
	node_num=getBH();

	switch(getBL()) {

		case VDD_OPEN:
#if defined(_DEBUG)
			sprintf(str,"sbbsexec%d.log",node_num);
			fp=fopen(str,"wb");
#endif

			sprintf(str,"\\\\.\\mailslot\\sbbsexec\\wr%d",node_num);
			rdslot=CreateMailslot(str
				,sizeof(buf)			// Max message size (0=any)
				,MAILSLOT_WAIT_FOREVER 	// Read timeout
				,NULL);
			if(rdslot==INVALID_HANDLE_VALUE) {
				if(fp!=NULL)
					fprintf(fp,"!VDD_OPEN: Error %d opening %s\r\n"
						,GetLastError(),str);
				retval=1;
				break;
			}

			sprintf(str,"\\\\.\\mailslot\\sbbsexec\\rd%d",node_num);
			wrslot=CreateFile(str
				,GENERIC_WRITE
				,FILE_SHARE_READ
				,NULL
				,OPEN_EXISTING
				,FILE_ATTRIBUTE_NORMAL
				,(HANDLE) NULL);
			if(wrslot==INVALID_HANDLE_VALUE) {
				if(fp!=NULL)
					fprintf(fp,"!VDD_OPEN: Error %d opening %s\r\n"
						,GetLastError(),str);
				retval=2;
				break;
			}

			if(RingBufInit(&rdbuf, RINGBUF_SIZE_IN)!=0) {
				retval=3;
				break;
			}

			sprintf(str,"sbbsexec_hungup%d",node_num);
			hungup_event=OpenEvent(
				EVENT_ALL_ACCESS,	// access flag 
				FALSE,				// inherit flag 
				str);				// pointer to event-object name 
			if(hungup_event==NULL) {
				if(fp!=NULL)
					fprintf(fp,"!VDD_OPEN: Error %d opening %s\r\n"
						,GetLastError(),str);
				retval=4;
				break;
			}

			status_poll=0;
			inbuf_poll=0;
			online_poll=0;
			yields=0;

			retval=0;
			break;

		case VDD_CLOSE:
			if(fp!=NULL) {
				fprintf(fp,"VDD_CLOSE: rdbuf=%u "
					"status_poll=%u inbuf_poll=%u online_poll=%u yields=%u\r\n"
					,RingBufFull(&rdbuf),status_poll,inbuf_poll,online_poll,yields);
				fprintf(fp,"           read=%u bytes (in %u calls)\r\n",bytes_read,reads);
				fprintf(fp,"           wrote=%u bytes (in %u calls)\r\n",bytes_written,writes);
				fclose(fp);
			}
			CloseHandle(rdslot);
			CloseHandle(wrslot);
			if(hungup_event!=NULL)
				CloseHandle(hungup_event);
			RingBufDispose(&rdbuf);
			status_poll=0;
			retval=0;

			break;

		case VDD_READ:
			count = getCX();
			if(count != 1 && fp!=NULL)
				fprintf(fp,"VDD_READ of %d\r\n",count);

			p = (BYTE*) GetVDMPointer((ULONG)((getES() << 16)|getDI())
				,count,FALSE); 
			if(RingBufFull(&rdbuf)) {
				retval=RingBufRead(&rdbuf,p,count);
				if(retval==0 && fp!=NULL)
					fprintf(fp,"!VDD_READ: RingBufRead read 0\r\n");
				reads++;
				bytes_read+=retval;
				break;
			}
			if(!ReadFile(rdslot,buf,sizeof(buf),&retval,NULL)) {
				if(fp!=NULL)
					fprintf(fp,"!VDD_READ: ReadFile Error %d (size=%d)\r\n"
						,GetLastError(),retval);
				retval=0;
				break;
			}
			if(retval==0) {
				if(fp!=NULL)
					fprintf(fp,"!VDD_READ: ReadFile read 0\r\n");
				break;
			}
			RingBufWrite(&rdbuf,buf,retval);
			retval=RingBufRead(&rdbuf,p,count);
			if(retval==0 && fp!=NULL)
				fprintf(fp,"!VDD_READ: RingBufRead read 0 after write\r\n");
			reads++;
			bytes_read+=retval;
			break;

		case VDD_PEEK:
			count = getCX();
			if(count != 1 && fp!=NULL)
				fprintf(fp,"VDD_PEEK of %d\r\n",count);

			p = (BYTE*) GetVDMPointer((ULONG)((getES() << 16)|getDI())
				,count,FALSE); 
			if(RingBufFull(&rdbuf)) {
				retval=RingBufPeek(&rdbuf,p,count);
				break;
			}
			if(!ReadFile(rdslot,buf,sizeof(buf),&retval,NULL)) {
				if(fp!=NULL)
					fprintf(fp,"!VDD_PEEK: ReadFile Error %d\r\n"
						,GetLastError());
				retval=0;
				break;
			}
			if(retval==0) {
				if(fp!=NULL)
					fprintf(fp,"!VDD_PEEK: ReadFile read 0\r\n");
				break;
			}
			RingBufWrite(&rdbuf,buf,retval);
			retval=RingBufPeek(&rdbuf,p,count);
			break;

		case VDD_WRITE:
			count = getCX();
			if(count != 1 && fp!=NULL)
				fprintf(fp,"VDD_WRITE of %d\r\n",count);
			p = (BYTE*) GetVDMPointer((ULONG)((getES() << 16)|getDI())
				,count,FALSE); 
//			if(fp!=NULL)
//				fwrite(p,count,1,fp);
			if(!WriteFile(wrslot,p,count,&retval,NULL)) {
				if(fp!=NULL)
					fprintf(fp,"!VDD_WRITE: WriteFile Error %d (size=%d)\r\n"
						,GetLastError(),retval);
				retval=0;
			} else {
				writes++;
				bytes_written+=retval;
			}
			break;

		case VDD_STATUS:

			status_poll++;
			count = getCX();
			if(count != sizeof(vdd_status_t)) {
				if(fp!=NULL)
					fprintf(fp,"!VDD_STATUS: wrong size (%d!=%d)\r\n",count,sizeof(vdd_status_t));
				retval=sizeof(vdd_status_t);
				break;
			}
			status = (vdd_status_t*) GetVDMPointer((ULONG)((getES() << 16)|getDI())
				,count,FALSE); 

			/* INBUF FULL/SIZE */
			if(!GetMailslotInfo(
				rdslot,					// mailslot handle 
 				&status->inbuf_size,	// address of maximum message size 
				&status->inbuf_full,	// address of size of next message 
				&msgs,					// address of number of messages 
 				NULL					// address of read time-out 
				)) {
				status->inbuf_full=0;
				status->inbuf_size=DEFAULT_MAX_MSG_SIZE;
			}
			if(status->inbuf_full==MAILSLOT_NO_MESSAGE)
				status->inbuf_full=0;
			status->inbuf_full*=msgs;
			status->inbuf_full+=RingBufFull(&rdbuf);
			

			/* OUTBUF FULL/SIZE */
			if(!GetMailslotInfo(
				wrslot,					// mailslot handle 
 				&status->outbuf_size,	// address of maximum message size 
				&status->outbuf_full,	// address of size of next message 
				&msgs,					// address of number of messages 
 				NULL					// address of read time-out 
				)) {
				status->outbuf_full=0;
				status->outbuf_size=DEFAULT_MAX_MSG_SIZE;
			}
			if(status->outbuf_full==MAILSLOT_NO_MESSAGE)
				status->outbuf_full=0;
			status->outbuf_full*=msgs;
			
			/* ONLINE */
			if(WaitForSingleObject(hungup_event,0)==WAIT_OBJECT_0)
				status->online=0;
			else
				status->online=1;

			retval=0;	/* success */
			break;


		case VDD_INBUF_PURGE:
			if(fp!=NULL)
				fprintf(fp,"!VDD_INBUF_PURGE: NOT IMPLEMENTED\r\n");
			retval=0;
			break;

		case VDD_OUTBUF_PURGE:
			if(fp!=NULL)
				fprintf(fp,"!VDD_OUTBUF_PURGE: NOT IMPLEMENTED\r\n");
			retval=0;
			break;

		case VDD_INBUF_FULL:
			if(!GetMailslotInfo(
				rdslot,		// mailslot handle 
 				NULL,		// address of maximum message size 
				&retval,	// address of size of next message 
				&msgs,		// address of number of messages 
 				NULL		// address of read time-out 
				))
				retval=0;
			if(retval==MAILSLOT_NO_MESSAGE)
				retval=0;
			retval*=msgs;
			retval+=RingBufFull(&rdbuf);
			inbuf_poll++;
			break;

		case VDD_INBUF_SIZE:
			if(!GetMailslotInfo(
				rdslot,		// mailslot handle 
 				&retval,	// address of maximum message size 
				NULL,		// address of size of next message 
				NULL,		// address of number of messages 
 				NULL		// address of read time-out 
				))
				retval=DEFAULT_MAX_MSG_SIZE;
			break;

		case VDD_OUTBUF_FULL:
			if(!GetMailslotInfo(
				wrslot,		// mailslot handle 
 				NULL,		// address of maximum message size 
				&retval,	// address of size of next message 
				&msgs,		// address of number of messages 
 				NULL		// address of read time-out 
				))
				retval=0;
			if(retval==MAILSLOT_NO_MESSAGE)
				retval=0;
			retval*=msgs;
			break;

		case VDD_OUTBUF_SIZE:
			if(!GetMailslotInfo(
				wrslot,		// mailslot handle 
 				&retval,	// address of maximum message size 
				NULL,		// address of size of next message 
				NULL,		// address of number of messages 
 				NULL		// address of read time-out 
				)) 
				retval=DEFAULT_MAX_MSG_SIZE;
			break;

		case VDD_ONLINE:
			if(WaitForSingleObject(hungup_event,0)==WAIT_OBJECT_0)
				retval=0;
			else
				retval=1;
			online_poll++;
			break;

		case VDD_YIELD:
			Sleep(1);
			yields++;
			break;

		default:
			if(fp!=NULL)
				fprintf(fp,"!UNKNOWN VDD_OP: %d\r\n",getBL());
			break;
	}
	setAX((WORD)retval);
}

__declspec(dllexport) BOOL __cdecl VDDInitialize(IN PVOID DllHandle, IN ULONG Reason, 
IN PCONTEXT Context OPTIONAL)
{
    return TRUE;
}
