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
#include "uartdefs.h"
#include "vdd_func.h"
#include "ringbuf.h"
#include "genwrap.h"
#include "threadwrap.h"

#define RINGBUF_SIZE_IN			10000
#define DEFAULT_MAX_MSG_SIZE	4000
#define LINEAR_RX_BUFLEN		5000

/* UART Parameters and virtual registers */
WORD uart_io_base				= UART_COM1_IO_BASE;	/* COM1 */
BYTE uart_irq					= UART_COM1_IRQ;
BYTE uart_ier_reg				= 0;
BYTE uart_lcr_reg				= UART_LCR_8_DATA_BITS;
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

unsigned	polls_before_yield=1000;
unsigned	yield_interval=10;
HANDLE		hungup_event=NULL;
HANDLE		interrupt_event=NULL;
HANDLE		rdslot=INVALID_HANDLE_VALUE;
HANDLE		wrslot=INVALID_HANDLE_VALUE;
RingBuf		rdbuf;

void lputs(int level, char* msg)
{	
	char buf[1024];
	if(level > log_level)
		return;

	SAFEPRINTF(buf,"SBBS: %s\r\n", msg);
	OutputDebugString(buf);
}

static void lprintf(int level, const char *fmt, ...)
{
	char sbuf[1024];
	va_list argptr;

	if(level > log_level)
		return;

    va_start(argptr,fmt);
    _vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    lputs(level,sbuf);
}

/* Mutex-protected pending interrupt "queue" */
int pending_interrupts = 0;
CRITICAL_SECTION interrupt_mutex;

void set_interrupt_pending(BYTE intr, BOOL assert)
{
	EnterCriticalSection(&interrupt_mutex);
	if(assert) {
		if(uart_ier_reg&intr)				/* is interrupt enabled? */
			pending_interrupts |= intr;		/* flag as pending */
	} else /* de-assert */
		pending_interrupts &= ~intr;		/* remove as pending */
	SetEvent(interrupt_event);
	LeaveCriticalSection(&interrupt_mutex);
}

#define assert_interrupt(i)		set_interrupt_pending(i, TRUE)
#define deassert_interrupt(i)	set_interrupt_pending(i, FALSE)

void _cdecl interrupt_thread(void *arg)
{
	while(1) {
		if(WaitForSingleObject(interrupt_event,INFINITE)!=WAIT_OBJECT_0)
			break;
		if((uart_ier_reg&pending_interrupts) != 0) {
			lprintf(LOG_DEBUG,"VDDSimulateInterrupt (pending: %02X) - IER: %02X"
				,pending_interrupts, uart_ier_reg);
			VDDSimulateInterrupt(ICA_MASTER, uart_irq, 1);
		}
#if 0
		/* "Real 16550s should always reassert
		 *  this interrupt whenever the transmitter is idle and
		 *  the interrupt is enabled."
		 */
		if(pending_interrupts==0 && uart_ier_reg&UART_IER_TX_EMPTY)
			pending_interrupts|=UART_IER_TX_EMPTY;
#endif
	}
}

void _cdecl input_thread(void* arg)
{
	char	buf[LINEAR_RX_BUFLEN];
	int		count;

	while(1) {
		count=0;
		if(!ReadFile(rdslot,buf,sizeof(buf),&count,NULL)) {
			if(GetLastError()==ERROR_HANDLE_EOF)	/* closed by VDD_CLOSE */
				break;
			lprintf(LOG_ERR,"!VDD_READ: ReadFile Error %d (size=%d)"
				,GetLastError(),count);
			continue;
		}
		if(count==0) {
			lprintf(LOG_ERR,"!VDD_READ: ReadFile read 0");
			continue;
		}
		RingBufWrite(&rdbuf,buf,count);
		/* Set the "Data ready" bit in the LSR */
		uart_lsr_reg |= UART_LSR_DATA_READY;

		assert_interrupt(UART_IER_RX_DATA); /* assert rx data interrupt */
	}
}

unsigned vdd_read(BYTE* p, unsigned count)
{
	sem_wait(rdbuf.sem);
	count=RingBufRead(&rdbuf,p,count);
	if(count==0)
		lprintf(LOG_ERR,"!VDD_READ: RingBufRead read 0");

	return(count);
}

unsigned polls=0;
unsigned yields=0;

void yield()
{
	yields++;
	lprintf(LOG_DEBUG,"Yielding (polls=%u, yields=%u)", polls, yields);
	Sleep(1);
}

void maybe_yield()
{
	polls++;
	if(polls_before_yield && polls>=polls_before_yield && (polls%yield_interval)==0)
		yield();
}

__declspec(dllexport) void __cdecl VDDDispatch(void) 
{
	char			str[512];
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

	retval=0;
	node_num=getBH();

	lprintf(LOG_DEBUG,"VDD_OP: %d (arg=%X)", getBL(),getCX());

	switch(getBL()) {

		case VDD_OPEN:
#if 0
			sprintf(str,"sbbsexec%d.log",node_num);
			fp=fopen(str,"wb");
#endif

			sprintf(str,"\\\\.\\mailslot\\sbbsexec\\wr%d",node_num);
			rdslot=CreateMailslot(str
				,LINEAR_RX_BUFLEN		/* Max message size (0=any) */
				,MAILSLOT_WAIT_FOREVER 	/* Read timeout */
				,NULL);
			if(rdslot==INVALID_HANDLE_VALUE) {
				lprintf(LOG_ERR,"!VDD_OPEN: Error %d opening %s"
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
				lprintf(LOG_ERR,"!VDD_OPEN: Error %d opening %s"
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
				EVENT_ALL_ACCESS,	/* access flag  */
				FALSE,				/* inherit flag  */
				str);				/* pointer to event-object name  */
			if(hungup_event==NULL) {
				lprintf(LOG_ERR,"!VDD_OPEN: Error %d opening %s"
					,GetLastError(),str);
				retval=4;
				break;
			}

			status_poll=0;
			inbuf_poll=0;
			online_poll=0;
			yields=0;

			lprintf(LOG_DEBUG,"VDD_OPEN: Opened successfully");

			_beginthread(input_thread, 0, NULL);

			retval=0;
			break;

		case VDD_CLOSE:
			lprintf(LOG_DEBUG,"VDD_CLOSE: rdbuf=%u "
				"status_poll=%u inbuf_poll=%u online_poll=%u yields=%u"
				,RingBufFull(&rdbuf),status_poll,inbuf_poll,online_poll,yields);
			lprintf(LOG_DEBUG,"           read=%u bytes (in %u calls)",bytes_read,reads);
			lprintf(LOG_DEBUG,"           wrote=%u bytes (in %u calls)",bytes_written,writes);

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
			if(count != 1)
				lprintf(LOG_DEBUG,"VDD_READ of %d",count);
			p = (BYTE*) GetVDMPointer((ULONG)((getES() << 16)|getDI())
				,count,FALSE); 
			retval=vdd_read(p, count);
			reads++;
			bytes_read+=retval;
			break;

		case VDD_PEEK:
			count = getCX();
			if(count != 1)
				lprintf(LOG_DEBUG,"VDD_PEEK of %d",count);

			p = (BYTE*) GetVDMPointer((ULONG)((getES() << 16)|getDI())
				,count,FALSE); 
			retval=RingBufPeek(&rdbuf,p,count);
			break;

		case VDD_WRITE:
			count = getCX();
			if(count != 1)
				lprintf(LOG_DEBUG,"VDD_WRITE of %d",count);
			p = (BYTE*) GetVDMPointer((ULONG)((getES() << 16)|getDI())
				,count,FALSE); 
			if(!WriteFile(wrslot,p,count,&retval,NULL)) {
				lprintf(LOG_ERR,"!VDD_WRITE: WriteFile Error %d (size=%d)"
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
				lprintf(LOG_DEBUG,"!VDD_STATUS: wrong size (%d!=%d)",count,sizeof(vdd_status_t));
				retval=sizeof(vdd_status_t);
				break;
			}
			status = (vdd_status_t*) GetVDMPointer((ULONG)((getES() << 16)|getDI())
				,count,FALSE); 

			status->inbuf_full=RingBufFull(&rdbuf);

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
			lprintf(LOG_WARNING,"!VDD_INBUF_PURGE: NOT IMPLEMENTED");
			retval=0;
			break;

		case VDD_OUTBUF_PURGE:
			lprintf(LOG_WARNING,"!VDD_OUTBUF_PURGE: NOT IMPLEMENTED");
			retval=0;
			break;

		case VDD_INBUF_FULL:
			retval=RingBufFull(&rdbuf);
			inbuf_poll++;
			break;

		case VDD_INBUF_SIZE:
			retval=RINGBUF_SIZE_IN;
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
			yield();
			break;

		case VDD_CONFIG_YIELD:
			polls_before_yield=getCX();
			break;

		default:
			lprintf(LOG_ERR,"!UNKNOWN VDD_OP: %d",getBL());
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
					lprintf(LOG_ERR,"!VDD_WRITE: WriteFile Error %d (size=%d)"
						,GetLastError(),retval);
				} else {
					assert_interrupt(UART_IER_TX_EMPTY);
					polls=0;
				}
			}
			break;
		case UART_IER:
			if(uart_lcr_reg&UART_LCR_DLAB) {
				uart_divisor_latch_msb = data;
				lprintf(LOG_DEBUG,"set divisor latch high byte: %02X", data);
			} else
				uart_ier_reg = data;
			assert_interrupt(UART_IER_TX_EMPTY);	/* should this be re-asserted for all writes? */
			break;
		case UART_IIR: /* FCR not supported */
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

VOID uart_rdport(WORD port, PBYTE data)
{
	int reg = port - uart_io_base;
	static BYTE last_msr;
	static BYTE last_lsr;
	DWORD avail;

	lprintf(LOG_DEBUG,"read of port: %x (%s)", port, uart_reg_desc[reg]);

	switch(reg) {
		case UART_BASE:
			if(uart_lcr_reg&UART_LCR_DLAB) {
				lprintf(LOG_DEBUG,"reading divisor latch LSB");
				*data = uart_divisor_latch_lsb;
				break;
			}
			if((avail=RingBufFull(&rdbuf))!=0) {
				vdd_read(data,sizeof(BYTE));
				lprintf(LOG_DEBUG,"READ DATA: 0x%02X", *data);
				polls=0;
				avail--;
			}
			if(avail==0) {
				/* Clear the data ready bit in the LSR */
				uart_lsr_reg &= ~UART_LSR_DATA_READY;

				/* Clear data ready interrupt identification in IIR */
				deassert_interrupt(UART_IER_RX_DATA);
			}
			return;	/* early return */
		case UART_IER:
			if(uart_lcr_reg&UART_LCR_DLAB) {
				lprintf(LOG_DEBUG,"reading divisor latch MSB");
				*data = uart_divisor_latch_msb;
			} else
				*data = uart_ier_reg;
			break;
		case UART_IIR:
			/* Report IIR based on *priority* of pending interrupts */
			if(pending_interrupts & UART_IER_LINE_STATUS)
				*data = UART_IIR_LINE_STATUS;
			else if(pending_interrupts & UART_IER_RX_DATA)
				*data = UART_IIR_RX_DATA;
			else if(pending_interrupts & UART_IER_TX_EMPTY) {
				*data = UART_IIR_TX_EMPTY;
				/* "Transmit Holding Register Empty" interrupt */
				/*  is reset on read of IIR */
				deassert_interrupt(UART_IER_TX_EMPTY);
			}
			else if(pending_interrupts & UART_IER_MODEM_STATUS)
				*data = UART_IIR_MODEM_STATUS;
			else
				*data = UART_IIR_NONE;
			break;
		case UART_LCR:
			*data = uart_lcr_reg;
			break;
		case UART_MCR:
			*data = uart_mcr_reg;
			break;
		case UART_LSR:
			*data = uart_lsr_reg;
			if(uart_lsr_reg == last_lsr)
				maybe_yield();
			else {
				polls=0;
				last_lsr = uart_lsr_reg;
			}
			/* Clear line status interrupt pending */
			deassert_interrupt(UART_IER_LINE_STATUS);
			break;
		case UART_MSR:
			if(WaitForSingleObject(hungup_event,0)==WAIT_OBJECT_0)
				uart_msr_reg &=~ UART_MSR_DCD;
			else
				uart_msr_reg |= UART_MSR_DCD;
			*data = uart_msr_reg;
			if(uart_msr_reg == last_msr)
				maybe_yield();
			else {
				polls=0;
				last_msr = uart_msr_reg;
			}
			/* Clear modem status interrupt pending */
			deassert_interrupt(UART_IER_MODEM_STATUS);
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
	char revision[16];
	VDD_IO_HANDLERS  IOHandlers = { NULL };
	VDD_IO_PORTRANGE PortRange;

	sscanf("$Revision$", "%*s %s", revision);

	lprintf(LOG_INFO,"Synchronet Virutal Device Driver, rev %s %s %s"
		,revision, __DATE__, __TIME__);

	lprintf(LOG_DEBUG,"VDDInitialize, Reason: 0x%lX", Reason);

	/* Reason is always 0 (DLL_PROCESS_DETACH) which doesn't jive with the
	   Microsoft NT DDK docs */
	IOHandlers.inb_handler = uart_rdport;
	IOHandlers.outb_handler = uart_wrport;
	PortRange.First=uart_io_base;
	PortRange.Last=uart_io_base + UART_IO_RANGE;

	VDDInstallIOHook(hVDD, 1, &PortRange, &IOHandlers);

	interrupt_event=CreateEvent(NULL,FALSE,FALSE,NULL);
	InitializeCriticalSection(&interrupt_mutex);

	_beginthread(interrupt_thread, 0, NULL);

    return TRUE;
}
