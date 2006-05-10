/* sbbsexec.c */

/* Synchronet Windows NT/2000 VDD for FOSSIL and DOS I/O Interrupts */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2006 Rob Swindell - http://www.synchro.net/copyright.html		*
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

/* UART Registers */
#define UART_BASE				0
#define UART_IER				1		/* interrupt enable */
#define UART_IIR				2		/* interrupt identification */
#define UART_FCR				2		/* FIFO control */
#define UART_LCR				3		/* line control */
#define UART_MCR				4		/* modem control */
#define UART_LSR				5		/* line status */
#define UART_MSR				6		/* modem status */
#define UART_SCRATCH			7		/* scratch */
#define UART_IO_RANGE			UART_SCRATCH

const char* uart_reg_desc[] = { "base", "IER", "IIR", "LCR", "MCR", "LSR", "MSR", "scratch" };

#define UART_LSR_FIFO_ERROR		(1<<7)
#define UART_LSR_EMPTY_DATA		(1<<6)
#define UART_LSR_EMPTY_XMIT		(1<<5)
#define UART_LSR_BREAK			(1<<4)
#define UART_LSR_FRAME_ERROR	(1<<3)
#define UART_LSR_PARITY_ERROR	(1<<2)
#define UART_LSR_OVERRUN_ERROR	(1<<1)
#define UART_LSR_DATA_READY		(1<<0)

#define UART_MSR_DCD			(1<<7)
#define UART_MSR_RING			(1<<6)
#define UART_MSR_DSR			(1<<5)
#define UART_MSR_CTS			(1<<4)
#define UART_MSR_DCD_CHANGE		(1<<3)
#define UART_MSR_RING_CHANGE	(1<<2)
#define UART_MSR_DSR_CHANGE		(1<<1)
#define UART_MSR_CTS_CHANGE		(1<<0)

#define UART_LCR_DLAB			(1<<7)
#define UART_LCR_BREAK			(1<<6)
#define UART_LCR_DATA_BITS		0x02		/* 8 data bits */

/* I/O Ports */
#define UART_COM1_IO_BASE		0x3f8
#define UART_COM2_IO_BASE		0x2f8
#define UART_COM3_IO_BASE		0x3e8
#define UART_COM4_IO_BASE		0x2e8

/* UART Parameters */
WORD uart_io_base				= UART_COM1_IO_BASE;	/* COM1 */
BYTE uart_ier_reg				= 0;
BYTE uart_iir_reg				= 0;
BYTE uart_lcr_reg				= UART_LCR_DATA_BITS;
BYTE uart_mcr_reg				= 0;
BYTE uart_lsr_reg				= UART_LSR_EMPTY_DATA | UART_LSR_EMPTY_XMIT;
BYTE uart_msr_reg				= UART_MSR_CTS | UART_MSR_DSR;
BYTE uart_scratch_reg			= 0;
BYTE uart_divisor_latch_lsb		= 0x03;	/* 38400 */
BYTE uart_divisor_latch_msb		= 0x00;

#if defined (_DEBUG)
	int log_level = LOG_DEBUG;
#else
	int log_level = LOG_WARNING;
#endif

HANDLE	hungup_event=NULL;
HANDLE	rdslot=INVALID_HANDLE_VALUE;
HANDLE	wrslot=INVALID_HANDLE_VALUE;
RingBuf	rdbuf;

void lputs(int level, char* sbuf)
{
	if(level <= log_level)
		OutputDebugString(sbuf);
}

static void lprintf(int level, const char *fmt, ...)
{
	char sbuf[1024];
	va_list argptr;

    va_start(argptr,fmt);
    _vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    lputs(level,sbuf);
}

unsigned vdd_read(BYTE* p, size_t count)
{
	char			buf[5000];
	int				retval;

	if(RingBufFull(&rdbuf)) {
		retval=RingBufRead(&rdbuf,p,count);
		if(retval==0)
			lprintf(LOG_ERR,"!VDD_READ: RingBufRead read 0");
		return(retval);
	}

	if(!ReadFile(rdslot,buf,sizeof(buf),&retval,NULL)) {
		lprintf(LOG_ERR,"!VDD_READ: ReadFile Error %d (size=%d)"
			,GetLastError(),retval);
		return(0);
	}
	if(retval==0) {
		lprintf(LOG_ERR,"!VDD_READ: ReadFile read 0");
		return(0);
	}
	RingBufWrite(&rdbuf,buf,retval);
	retval=RingBufRead(&rdbuf,p,count);
	if(retval==0)
		lprintf(LOG_ERR,"!VDD_READ: RingBufRead read 0 after write");

	return(retval);
}

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
#if defined(_DEBUG)
	static	FILE*	fp=NULL;
#endif

	retval=0;
	node_num=getBH();

#if TRUE && defined(_DEBUG)
	if(fp!=NULL)
		fprintf(fp,"VDD_OP: %d\r\n",getBL());
#endif
	lprintf(LOG_DEBUG,"VDD_OP: %d", getBL());

	switch(getBL()) {

		case VDD_OPEN:
#if defined(_DEBUG)
			sprintf(str,"sbbsexec%d.log",node_num);
			fp=fopen(str,"wb");
#endif

			sprintf(str,"\\\\.\\mailslot\\sbbsexec\\wr%d",node_num);
			rdslot=CreateMailslot(str
				,sizeof(buf)			/* Max message size (0=any) */
				,MAILSLOT_WAIT_FOREVER 	/* Read timeout */
				,NULL);
			if(rdslot==INVALID_HANDLE_VALUE) {
#if defined(_DEBUG)
				if(fp!=NULL)
					fprintf(fp,"!VDD_OPEN: Error %d opening %s\r\n"
						,GetLastError(),str);
#endif
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
#if defined(_DEBUG)
				if(fp!=NULL)
					fprintf(fp,"!VDD_OPEN: Error %d opening %s\r\n"
						,GetLastError(),str);
#endif
				retval=2;
				break;
			}

			if(RingBufInit(&rdbuf, RINGBUF_SIZE_IN)!=0) {
				retval=3;
				break;
			}

			sprintf(str,"sbbsexec_hungup%d",node_num);
			hungup_event=OpenEvent(
				EVENT_ALL_ACCESS,	/* access flag  */
				FALSE,				/* inherit flag  */
				str);				/* pointer to event-object name  */
			if(hungup_event==NULL) {
#if defined(_DEBUG)
				if(fp!=NULL)
					fprintf(fp,"!VDD_OPEN: Error %d opening %s\r\n"
						,GetLastError(),str);
#endif
				retval=4;
				break;
			}

			status_poll=0;
			inbuf_poll=0;
			online_poll=0;
			yields=0;

#if defined(_DEBUG)
			if(fp!=NULL)
				fprintf(fp,"VDD_OPEN: Opened successfully\r\n");
#endif

			retval=0;
			break;

		case VDD_CLOSE:
#if defined(_DEBUG)
			if(fp!=NULL) {
				fprintf(fp,"VDD_CLOSE: rdbuf=%u "
					"status_poll=%u inbuf_poll=%u online_poll=%u yields=%u\r\n"
					,RingBufFull(&rdbuf),status_poll,inbuf_poll,online_poll,yields);
				fprintf(fp,"           read=%u bytes (in %u calls)\r\n",bytes_read,reads);
				fprintf(fp,"           wrote=%u bytes (in %u calls)\r\n",bytes_written,writes);
				fclose(fp);
			}
#endif
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
#if defined(_DEBUG)
			if(count != 1)
				lprintf(LOG_DEBUG,"VDD_READ of %d",count);
#endif
			p = (BYTE*) GetVDMPointer((ULONG)((getES() << 16)|getDI())
				,count,FALSE); 
			if(RingBufFull(&rdbuf)) {
				retval=RingBufRead(&rdbuf,p,count);
				if(retval==0)
					lprintf(LOG_ERR,"!VDD_READ: RingBufRead read 0");
				reads++;
				bytes_read+=retval;
				break;
			}
			if(!ReadFile(rdslot,buf,sizeof(buf),&retval,NULL)) {
				lprintf(LOG_ERR,"!VDD_READ: ReadFile Error %d (size=%d)"
					,GetLastError(),retval);
				retval=0;
				break;
			}
			if(retval==0) {
				lprintf(LOG_ERR,"!VDD_READ: ReadFile read 0");
				break;
			}
			RingBufWrite(&rdbuf,buf,retval);
			retval=RingBufRead(&rdbuf,p,count);
			if(retval==0)
				lprintf(LOG_ERR,"!VDD_READ: RingBufRead read 0 after write");
			reads++;
			bytes_read+=retval;
			break;

		case VDD_PEEK:
			count = getCX();
#if defined(_DEBUG)
			if(count != 1 && fp!=NULL)
				fprintf(fp,"VDD_PEEK of %d\r\n",count);
#endif

			p = (BYTE*) GetVDMPointer((ULONG)((getES() << 16)|getDI())
				,count,FALSE); 
			if(RingBufFull(&rdbuf)) {
				retval=RingBufPeek(&rdbuf,p,count);
				break;
			}
			if(!ReadFile(rdslot,buf,sizeof(buf),&retval,NULL)) {
#if defined(_DEBUG)
				if(fp!=NULL)
					fprintf(fp,"!VDD_PEEK: ReadFile Error %d\r\n"
						,GetLastError());
#endif
				retval=0;
				break;
			}
			if(retval==0) {
#if defined(_DEBUG)
				if(fp!=NULL)
					fprintf(fp,"!VDD_PEEK: ReadFile read 0\r\n");
#endif
				break;
			}
			RingBufWrite(&rdbuf,buf,retval);
			retval=RingBufPeek(&rdbuf,p,count);
			break;

		case VDD_WRITE:
			count = getCX();
#if defined(_DEBUG)
			if(count != 1 && fp!=NULL)
				fprintf(fp,"VDD_WRITE of %d\r\n",count);
#endif
			p = (BYTE*) GetVDMPointer((ULONG)((getES() << 16)|getDI())
				,count,FALSE); 
			if(!WriteFile(wrslot,p,count,&retval,NULL)) {
#if defined(_DEBUG)
				if(fp!=NULL)
					fprintf(fp,"!VDD_WRITE: WriteFile Error %d (size=%d)\r\n"
						,GetLastError(),retval);
#endif
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
#if defined(_DEBUG)
				if(fp!=NULL)
					fprintf(fp,"!VDD_STATUS: wrong size (%d!=%d)\r\n",count,sizeof(vdd_status_t));
#endif
				retval=sizeof(vdd_status_t);
				break;
			}
			status = (vdd_status_t*) GetVDMPointer((ULONG)((getES() << 16)|getDI())
				,count,FALSE); 

			/* INBUF FULL/SIZE */
			if(!GetMailslotInfo(
				rdslot,					/* mailslot handle  */
 				&status->inbuf_size,	/* address of maximum message size  */
				&status->inbuf_full,	/* address of size of next message  */
				&msgs,					/* address of number of messages  */
 				NULL					/* address of read time-out  */
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
				wrslot,					/* mailslot handle  */
 				&status->outbuf_size,	/* address of maximum message size  */
				&status->outbuf_full,	/* address of size of next message  */
				&msgs,					/* address of number of messages  */
 				NULL					/* address of read time-out  */
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
#if defined(_DEBUG)
			if(fp!=NULL)
				fprintf(fp,"!VDD_INBUF_PURGE: NOT IMPLEMENTED\r\n");
#endif
			retval=0;
			break;

		case VDD_OUTBUF_PURGE:
#if defined(_DEBUG)
			if(fp!=NULL)
				fprintf(fp,"!VDD_OUTBUF_PURGE: NOT IMPLEMENTED\r\n");
#endif
			retval=0;
			break;

		case VDD_INBUF_FULL:
			if(!GetMailslotInfo(
				rdslot,		/* mailslot handle  */
 				NULL,		/* address of maximum message size  */
				&retval,	/* address of size of next message  */
				&msgs,		/* address of number of messages  */
 				NULL		/* address of read time-out  */
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
				rdslot,		/* mailslot handle  */
 				&retval,	/* address of maximum message size  */
				NULL,		/* address of size of next message  */
				NULL,		/* address of number of messages  */
 				NULL		/* address of read time-out  */
				))
				retval=DEFAULT_MAX_MSG_SIZE;
			break;

		case VDD_OUTBUF_FULL:
			if(!GetMailslotInfo(
				wrslot,		/* mailslot handle  */
 				NULL,		/* address of maximum message size  */
				&retval,	/* address of size of next message  */
				&msgs,		/* address of number of messages  */
 				NULL		/* address of read time-out  */
				))
				retval=0;
			if(retval==MAILSLOT_NO_MESSAGE)
				retval=0;
			retval*=msgs;
			break;

		case VDD_OUTBUF_SIZE:
			if(!GetMailslotInfo(
				wrslot,		/* mailslot handle  */
 				&retval,	/* address of maximum message size  */
				NULL,		/* address of size of next message  */
				NULL,		/* address of number of messages  */
 				NULL		/* address of read time-out  */
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
#if defined(_DEBUG)
			if(fp!=NULL)
				fprintf(fp,"!UNKNOWN VDD_OP: %d\r\n",getBL());
#endif
			break;
	}
	setAX((WORD)retval);
}

VOID uart_wrport(WORD port, BYTE data)
{
	int reg = port - uart_io_base;
	int retval;

	lprintf(LOG_DEBUG,"write of port: %x (%s) <- %02X", port, uart_reg_desc[reg], data);

	switch(reg) {
		case UART_BASE:
			if(uart_lcr_reg&UART_LCR_DLAB) {
				uart_divisor_latch_lsb = data;
				lprintf(LOG_DEBUG,"set divisor latch low byte: %02X", data);
			} else {
				lprintf(LOG_DEBUG,"WRITE DATA: %02X", data);
				if(!WriteFile(wrslot,&data,sizeof(BYTE),&retval,NULL)) {
					lprintf(LOG_ERR,"!VDD_WRITE: WriteFile Error %d (size=%d)\r\n"
						,GetLastError(),retval);
				}
			}
			break;
		case UART_IER:
			if(uart_lcr_reg&UART_LCR_DLAB) {
				uart_divisor_latch_msb = data;
				lprintf(LOG_DEBUG,"set divisor latch high byte: %02X", data);
			} else
				uart_ier_reg = data;
			break;
		case UART_IIR:
			uart_iir_reg = data;
			break;
		case UART_LCR:
			uart_lcr_reg = data;
			break;
		case UART_MCR:
			uart_mcr_reg = data;
			break;
		case UART_SCRATCH:
			uart_scratch_reg = data;
			break;
		default:
			lprintf(LOG_ERR,"UNSUPPORTED register: %u", reg);
			break;
			
	}
}

BOOL data_ready(void)
{
	DWORD	data=0;

	if(RingBufFull(&rdbuf))
		return(TRUE);
	GetMailslotInfo(
		rdslot,			/* mailslot handle  */
 		NULL,			/* address of maximum message size  */
		NULL,			/* address of size of next message  */
		&data,			/* address of number of messages  */
 		NULL			/* address of read time-out  */
		);
	return(data);
}

VOID uart_rdport(WORD port, PBYTE data)
{
	int reg = port - uart_io_base;

	lprintf(LOG_DEBUG,"read of port: %x (%s)", port, uart_reg_desc[reg]);

	switch(reg) {
		case UART_BASE:
			if(uart_lcr_reg&UART_LCR_DLAB)
				*data = uart_divisor_latch_lsb;
			else if(data_ready()) {
				vdd_read(data,sizeof(BYTE));
				lprintf(LOG_DEBUG,"READ DATA: 0x%02X", *data);
				return;
			}
			break;
		case UART_IER:
			if(uart_lcr_reg&UART_LCR_DLAB)
				*data = uart_divisor_latch_msb;
			else
				*data = uart_ier_reg;
			break;
		case UART_IIR:
			*data = uart_iir_reg;
			break;
		case UART_LCR:
			*data = uart_lcr_reg;
			break;
		case UART_MCR:
			*data = uart_mcr_reg;
			break;
		case UART_LSR:
			if(data_ready())
				uart_lsr_reg |= UART_LSR_DATA_READY;
			else
				uart_lsr_reg &=~ UART_LSR_DATA_READY;

			*data = uart_lsr_reg;
			break;
		case UART_MSR:
			if(WaitForSingleObject(hungup_event,0)==WAIT_OBJECT_0)
				uart_msr_reg &=~ UART_MSR_DCD;
			else
				uart_msr_reg |= UART_MSR_DCD;
			*data = uart_msr_reg;
			break;
		case UART_SCRATCH:
			*data = uart_scratch_reg;
			break;
		default:
			lprintf(LOG_ERR,"UNSUPPORTED register: %u", reg);
			break;
	}

	lprintf(LOG_DEBUG, "returning 0x%02X", *data);
}

__declspec(dllexport) BOOL __cdecl VDDInitialize(IN PVOID hVDD, IN ULONG Reason, 
IN PCONTEXT Context OPTIONAL)
{
	VDD_IO_HANDLERS  IOHandlers = { NULL };
	VDD_IO_PORTRANGE PortRange;

	lprintf(LOG_DEBUG,"VDDInitialize, Reason: %u", Reason);
	
	switch (Reason) {
 
		case DLL_PROCESS_DETACH:
		case DLL_PROCESS_ATTACH:

//BOOL VDDInstallIOHook(hVDD, cPortRange, pPortRange, 10handler)
//  IN HANDLE  hVdd;
//  IN WORD  cPortRange;
//  IN PVDD_IO_PORTRANGE  pPortRange;
//  IN PVDD_IO_HANDLERS  10handler;

			IOHandlers.inb_handler = uart_rdport;
			IOHandlers.outb_handler = uart_wrport;
			PortRange.First=uart_io_base;
			PortRange.Last=uart_io_base + UART_IO_RANGE;

			VDDInstallIOHook(hVDD, 1, &PortRange, &IOHandlers);
			break;
#if 0
		case DLL_PROCESS_DETACH:

// VOID VDDDeInstalllOHook(hVdd, cPortRange, pPortRange)
// IN HANDLE  hVdd;
//   IN WORD  cPortRange;
//   IN PVDD_IO_PORTRANGE  pPortRange;

			VDDDeInstallIOHook(hVDD, 1, &PortRange);

			break;
#endif
		default:
			break;
	}

    return TRUE;
}
