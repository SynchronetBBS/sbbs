/* Synchronet Windows NT/2000 VDD for FOSSIL and DOS I/O Interrupts */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
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
#include "dirwrap.h"
#include "threadwrap.h"
#include "ini_file.h"
#include "git_branch.h"
#include "git_hash.h"
#include "eventwrap.h"

#define INI_FILENAME            "sbbsexec.ini"
#define RINGBUF_SIZE_IN         10000
#define DEFAULT_MAX_MSG_SIZE    4000
#define LINEAR_RX_BUFLEN        10000
#define READ_TIMEOUT            (30 * 1000) // X00REF.DOC "the timeout value is set to 30 seconds"

#define DEFAULT_UART        { \
			.io_base                = UART_COM1_IO_BASE, \
			.irq                    = UART_COM1_IRQ, \
			.ier_reg                = 0, \
			.lcr_reg                = UART_LCR_8_DATA_BITS, \
			.mcr_reg                = UART_MCR_DTR, \
			.lsr_reg                = UART_LSR_EMPTY_DATA | UART_LSR_EMPTY_XMIT, \
			.msr_reg                = UART_MSR_CTS | UART_MSR_DSR, \
			.scratch_reg            = 0, \
			.divisor_latch_lsb      = 0x03,/* 38400 */ \
			.divisor_latch_msb      = 0x00, \
			.fifo_enabled           = 0, \
			.virtualize             = FALSE \
}

/* UART Parameters and virtual registers */
struct uart {
	WORD io_base;
	BYTE irq;
	BYTE ier_reg;
	BYTE lcr_reg;
	BYTE mcr_reg;
	BYTE lsr_reg;
	BYTE msr_reg;
	BYTE scratch_reg;
	BYTE divisor_latch_lsb;
	BYTE divisor_latch_msb;
	BYTE fifo_enabled;
	BOOL virtualize;
} default_uart = DEFAULT_UART, uart = DEFAULT_UART;

// Notice: re-initialize global variables in VDDInitialize for NTVDMx64 and pre-Vista versions of Windows
int        log_level = LOG_INFO;

double     yield_interval = 1.0;
BOOL       dcd_changed = FALSE;
BOOL       hangup_supported = TRUE;
HANDLE     hangup_event = NULL;
HANDLE     hungup_event = NULL;
HANDLE     carrier_event = NULL;
HANDLE     interrupt_event = NULL;
HANDLE     rdslot = INVALID_HANDLE_VALUE;
HANDLE     wrslot = INVALID_HANDLE_VALUE;
RingBuf    rdbuf;
str_list_t ini;
char       ini_fname[MAX_PATH + 1];

void lputs(int level, char* msg)
{
	char buf[1024];
	if (level > log_level)
		return;

	SAFEPRINTF(buf, "SBBS: %s\r\n", msg);
	OutputDebugString(buf);
}

static void lprintf(int level, const char *fmt, ...)
{
	char    sbuf[1024];
	va_list argptr;

	if (level > log_level)
		return;

	va_start(argptr, fmt);
	_vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);
	lputs(level, sbuf);
}

void hangup()
{
	if (hangup_supported && hangup_event != NULL) {
		lputs(LOG_DEBUG, "Hanging-up at application request");
		SetEvent(hangup_event);
	}
}

void parse_ini(char* program)
{
	char section[MAX_PATH + 1];

	if (ini == NULL)   /* no initialization file */
		return;

	/* Read the root section of the sbbsexec.ini file */
	log_level = iniGetLogLevel(ini, program, "LogLevel", log_level);
	if (iniGetBool(ini, program, "Debug", FALSE))
		log_level = LOG_DEBUG;
	yield_interval = iniGetFloat(ini, program, "YieldInterval", yield_interval);
	hangup_supported = iniGetBool(ini, program, "CanDisconnect", hangup_supported);

	lprintf(LOG_INFO, "Parsed %s section of %s"
	        , program == ROOT_SECTION ? "root" : program
	        , ini_fname);

	/* [UART] section */
	if (program == ROOT_SECTION)
		SAFECOPY(section, "UART");
	else
		SAFEPRINTF(section, "%s.UART", program);

	uart.virtualize = iniGetBool(ini, section, "Virtualize", uart.virtualize);
	switch (iniGetInteger(ini, section, "ComPort", 0)) {
		case 1: /* COM1 */
			uart.irq        = UART_COM1_IRQ;
			uart.io_base    = UART_COM1_IO_BASE;
			break;
		case 2: /* COM2 */
			uart.irq        = UART_COM2_IRQ;
			uart.io_base    = UART_COM2_IO_BASE;
			break;
		case 3: /* COM3 */
			uart.irq        = UART_COM3_IRQ;
			uart.io_base    = UART_COM3_IO_BASE;
			break;
		case 4: /* COM4 */
			uart.irq        = UART_COM4_IRQ;
			uart.io_base    = UART_COM4_IO_BASE;
			break;
	}
	uart.irq = (BYTE)iniGetShortInt(ini, section, "IRQ", uart.irq);
	uart.io_base = iniGetShortInt(ini, section, "Address", uart.io_base);

	lprintf(LOG_INFO, "Parsed %s section of %s"
	        , section
	        , ini_fname);
}


/* Mutex-protected pending interrupt "queue" */
volatile int     pending_interrupts = 0;
CRITICAL_SECTION interrupt_mutex;

void set_interrupt_pending(BYTE intr, BOOL assert)
{
	EnterCriticalSection(&interrupt_mutex);
	lprintf(LOG_DEBUG, "%sasserting interrupt %02X (pending: %02X, IER: %02X)"
	        , assert ? "" : "de-", intr
	        , pending_interrupts
	        , uart.ier_reg);
	if (assert) {
		if (uart.ier_reg & intr) {                 /* is interrupt enabled? */
			pending_interrupts |= intr;         /* flag as pending */
			SetEvent(interrupt_event);
		}
	} else /* de-assert */
		pending_interrupts &= ~intr;        /* remove as pending */

	LeaveCriticalSection(&interrupt_mutex);
}

#define assert_interrupt(i)     set_interrupt_pending(i, TRUE)
#define deassert_interrupt(i)   set_interrupt_pending(i, FALSE)

void data_waiting(BOOL waiting)
{
	if (waiting && (uart.mcr_reg & UART_MCR_RTS)) {
		/* Set the "Data ready" bit in the LSR */
		uart.lsr_reg |= UART_LSR_DATA_READY;

		/* assert rx data interrupt */
		assert_interrupt(UART_IER_RX_DATA);
	} else {
		/* Clear the data ready bit in the LSR */
		uart.lsr_reg &= ~UART_LSR_DATA_READY;

		/* Clear data ready interrupt identification in IIR */
		deassert_interrupt(UART_IER_RX_DATA);
	}
}

void _cdecl interrupt_thread(void *arg)
{
	HANDLE handles[] = {interrupt_event, carrier_event};
	while (1) {
		DWORD handle_count = (carrier_event == NULL) ? 1 : 2;
		DWORD result = WaitForMultipleObjects(handle_count, handles, /* waitAll */ FALSE, INFINITE);
		if (result == WAIT_OBJECT_0 + 1) {
			lputs(LOG_DEBUG, "Carrier change detected in " __FUNCTION__);
			dcd_changed = TRUE;
			assert_interrupt(UART_IER_MODEM_STATUS);
			continue;
		}
		if (result != WAIT_OBJECT_0) {
			lprintf(LOG_NOTICE, "Waiting for interrupt returned %ld", result);
			break;
		}
		if ((uart.ier_reg & pending_interrupts) != 0) {
			lprintf(LOG_DEBUG, "VDDSimulateInterrupt (pending: %02X) - IER: %02X"
			        , pending_interrupts, uart.ier_reg);
			VDDSimulateInterrupt(ICA_MASTER, uart.irq, /* count: */ 1);
		}
#if 0
		/* "Real 16550s should always reassert
		 *  this interrupt whenever the transmitter is idle and
		 *  the interrupt is enabled."
		 */
		if (pending_interrupts == 0 && uart.ier_reg & UART_IER_TX_EMPTY)
			assert_interrupt(UART_IER_TX_EMPTY);
#endif
	}
}

void _cdecl input_thread(void* arg)
{
	char buf[LINEAR_RX_BUFLEN];
	int  space;
	int  count;

	lputs(LOG_DEBUG, "input_thread: started");
	while (1) {
		space = RingBufFree(&rdbuf);
		if (space < 1) {
			lputs(LOG_WARNING, "input_thread: input buffer full!");
			YIELD();
			continue;
		}
		if (space > sizeof(buf))
			space = sizeof(buf);
		if (!ReadFile(rdslot, buf, space, &count, NULL)) {
			if (GetLastError() == ERROR_HANDLE_EOF) {  /* closed by VDD_CLOSE */
				lputs(LOG_INFO, "input_thread: ReadFile returned EOF");
				break;
			}
			lprintf(LOG_ERR, "!input_thread: ReadFile Error %d (space=%d, count=%d)"
			        , GetLastError(), space, count);
			YIELD();
			continue;
		}
		if (count == 0) {
			lputs(LOG_ERR, "!input_thread: ReadFile read 0");
			YIELD();
			continue;
		}
		RingBufWrite(&rdbuf, buf, count);

		if (uart.virtualize) {
			data_waiting(TRUE);
		}
	}
	lputs(LOG_DEBUG, "input_thread: terminated");
}

unsigned vdd_read(BYTE* p, unsigned count)
{
	count = RingBufRead(&rdbuf, p, count);
	if (count == 0) {
		lputs(LOG_ERR, "!VDD_READ: RingBufRead read 0, waiting");
		if (WaitForEvent(rdbuf.data_event, READ_TIMEOUT) != WAIT_OBJECT_0)
			lputs(LOG_ERR, "!VDD_READ: rdbuf sem timeout");
		count = RingBufRead(&rdbuf, p, count);
		lputs(LOG_ERR, "!VDD_READ: RingBufRead read 0 (after wait)");
	}

	return count;
}

unsigned yields = 0;

void yield()
{
	yields++;
	lprintf(LOG_DEBUG, "Yielding (yields=%u)", yields);
	Sleep(1);
}

long double last_yield = 0;

void maybe_yield()
{
	long double t;

	t = xp_timer();

	if (yield_interval && (t - last_yield) * 1000.0 >= yield_interval) {
		yield();
		last_yield = t;
	}
}

void reset_yield()
{
	last_yield = xp_timer();
}

/***********************/
/* UART Virtualization */
/***********************/

static char *chr(uchar ch)
{
	static char str[25];

	if (ch >= ' ' && ch <= '~')
		sprintf(str, "'%c' (%02Xh)", ch, ch);
	else if (ch < ' ')
		sprintf(str, "^%c  (%02Xh)", '@' + ch, ch);
	else
		sprintf(str, "%u (%02Xh)", ch, ch);
	return str;
}

VOID uart_wrport(WORD port, BYTE data)
{
	int reg = port - uart.io_base;
	int retval;

	lprintf(LOG_DEBUG, "write of port: %x (%s) <- %02X", port, uart_reg_desc[reg], data);

	switch (reg) {
		case UART_BASE:
			if (uart.lcr_reg & UART_LCR_DLAB) {
				uart.divisor_latch_lsb = data;
				lprintf(LOG_DEBUG, "set divisor latch low byte: %02X", data);
			} else { // Transmitter Holding Register
				if (log_level >= LOG_DEBUG)
					lprintf(LOG_DEBUG, "WRITE DATA: %s", chr(data));
				if (!WriteFile(wrslot, &data, sizeof(BYTE), &retval, NULL)) {
					lprintf(LOG_ERR, "!VDD_WRITE: WriteFile Error %d (size=%d)"
					        , GetLastError(), retval);
				} else {
					assert_interrupt(UART_IER_TX_EMPTY);
//					reset_yield();
				}
			}
			break;
		case UART_IER:
			if (uart.lcr_reg & UART_LCR_DLAB) {
				uart.divisor_latch_msb = data;
				lprintf(LOG_DEBUG, "set divisor latch high byte: %02X", data);
			} else {
				uart.ier_reg = data;
				lprintf(LOG_DEBUG, "Set IER to %02X", data);
			}
			assert_interrupt(UART_IER_TX_EMPTY);    /* should this be re-asserted for all writes? */
			break;
		case UART_FCR:
			uart.fifo_enabled = data & UART_FCR_ENABLE_FIFO;
			lprintf(LOG_INFO, "Set FCR to %02X", data);
			break;
		case UART_LCR:
			uart.lcr_reg = data;
			lprintf(LOG_INFO, "Set LCR to %02X", data);
			break;
		case UART_MCR:
			uart.mcr_reg = data;
			if ((uart.mcr_reg & UART_MCR_DTR) == 0)    /* Dropping DTR (i.e. "hangup") */
				hangup();
			data_waiting(RingBufFull(&rdbuf));
			lprintf(LOG_INFO, "Set MCR to %02X", data);
			break;
		case UART_SCRATCH:
			uart.scratch_reg = data;
			lprintf(LOG_INFO, "Set scratch register to %02X", data);
			break;
		default:
			lprintf(LOG_ERR, "UNSUPPORTED register: %u", reg);
			break;

	}
}

VOID uart_rdport(WORD port, PBYTE data)
{
	int   reg = port - uart.io_base;
	DWORD avail;

	lprintf(LOG_DEBUG, "read of port: %x (%s)", port, uart_reg_desc[reg]);

	switch (reg) {
		case UART_BASE:
			if (uart.lcr_reg & UART_LCR_DLAB) {
				lputs(LOG_DEBUG, "reading divisor latch LSB");
				*data = uart.divisor_latch_lsb;
				break;
			}
			if ((avail = RingBufFull(&rdbuf)) != 0) {
				vdd_read(data, sizeof(BYTE));
				lprintf(LOG_DEBUG, "READ DATA: %s", chr(*data));
				avail--;
//				reset_yield();
			} else
				*data = 0;
			data_waiting(avail);
			break;
		case UART_IER:
			if (uart.lcr_reg & UART_LCR_DLAB) {
				lputs(LOG_DEBUG, "reading divisor latch MSB");
				*data = uart.divisor_latch_msb;
			} else
				*data = uart.ier_reg;
			break;
		case UART_IIR:
		{
			/* Report IIR based on *priority* of pending interrupts */
			if (pending_interrupts & UART_IER_LINE_STATUS)
				*data = UART_IIR_LINE_STATUS;
			else if (pending_interrupts & UART_IER_RX_DATA)
				*data = UART_IIR_RX_DATA;
			else if (pending_interrupts & UART_IER_TX_EMPTY) {
				*data = UART_IIR_TX_EMPTY;
				/* "Transmit Holding Register Empty" interrupt */
				/*  is reset on read of IIR */
				deassert_interrupt(UART_IER_TX_EMPTY);
			}
			else if (pending_interrupts & UART_IER_MODEM_STATUS)
				*data = UART_IIR_MODEM_STATUS;
			else
				*data = UART_IIR_NONE;
			if (pending_interrupts)
				SetEvent(interrupt_event);
			if (uart.fifo_enabled)
				*data |= UART_IIR_FIFO_ENABLED;
			lprintf(LOG_DEBUG, "IIR: %02X (pending: %02X)", *data, pending_interrupts);
			break;
		}
		case UART_LCR:
			*data = uart.lcr_reg;
			lprintf(LOG_INFO, "LCR: %02X", uart.lcr_reg);
			break;
		case UART_MCR:
			*data = uart.mcr_reg;
			lprintf(LOG_INFO, "MCR: %02X", uart.mcr_reg);
			break;
		case UART_LSR:
			*data = uart.lsr_reg;
			/* Clear line status interrupt pending */
			deassert_interrupt(UART_IER_LINE_STATUS);
			lprintf(LOG_DEBUG, "LSR: %02X", *data);
			break;
		case UART_MSR:
			if (WaitForSingleObject(hungup_event, 0) == WAIT_OBJECT_0)
				uart.msr_reg &= ~UART_MSR_DCD;
			else
				uart.msr_reg |= UART_MSR_DCD;
			if (dcd_changed)
				uart.msr_reg |= UART_MSR_DCD_CHANGE;
			else
				uart.msr_reg &= ~UART_MSR_DCD_CHANGE;
			dcd_changed = FALSE;
			*data = uart.msr_reg;
			/* Clear modem status interrupt pending */
			deassert_interrupt(UART_IER_MODEM_STATUS);
			lprintf(LOG_DEBUG, "MSR: %02X", *data);
			break;
		case UART_SCRATCH:
			*data = uart.scratch_reg;
			break;
		default:
			lprintf(LOG_ERR, "UNSUPPORTED register: %u", reg);
			break;
	}

	if (reg != UART_BASE)
		lprintf(LOG_DEBUG, "returning 0x%02X", *data);
}

/* VDD DOS Interface (mainly for FOSSIL driver in dosxtrn.exe) */

__declspec(dllexport) void __cdecl VDDDispatch(void)
{
	char                    str[512];
	DWORD                   count;
	DWORD                   msgs;
	int                     retval;
	int                     node_num;
	BYTE*                   p;
	vdd_status_t*           status;
	static DWORD            writes;
	static DWORD            bytes_written;
	static DWORD            reads;
	static DWORD            bytes_read;
	static DWORD            inbuf_poll;
	static DWORD            online_poll;
	static DWORD            status_poll;
	static DWORD            vdd_yields;
	static DWORD            vdd_calls;
	VDD_IO_HANDLERS         IOHandlers = { NULL };
	static VDD_IO_PORTRANGE PortRange;

	retval = 0;
	node_num = getBH();

	lprintf(LOG_DEBUG, "VDD_OP: (handle=%d) %d (arg=%X)", getAX(), getBL(), getCX());
	vdd_calls++;

	switch (getBL()) {

		case VDD_OPEN:

			lprintf(LOG_INFO, "Synchronet Virtual Device Driver, %s/%s %s"
			        , GIT_BRANCH, GIT_HASH, GIT_DATE);
#if 0
			sprintf(str, "sbbsexec%d.log", node_num);
			fp = fopen(str, "wb");
#endif

			sprintf(str, "\\\\.\\mailslot\\sbbsexec\\wr%d", node_num);
			rdslot = CreateMailslot(str
			                        , 0 //LINEAR_RX_BUFLEN		/* Max message size (0=any) */
			                        , MAILSLOT_WAIT_FOREVER /* Read timeout */
			                        , NULL);
			if (rdslot == INVALID_HANDLE_VALUE) {
				lprintf(LOG_ERR, "!VDD_OPEN: Error %d opening %s"
				        , GetLastError(), str);
				retval = 1;
				break;
			}

			sprintf(str, "\\\\.\\mailslot\\sbbsexec\\rd%d", node_num);
			wrslot = CreateFile(str
			                    , GENERIC_WRITE
			                    , FILE_SHARE_READ
			                    , NULL
			                    , OPEN_EXISTING
			                    , FILE_ATTRIBUTE_NORMAL
			                    , (HANDLE) NULL);
			if (wrslot == INVALID_HANDLE_VALUE) {
				lprintf(LOG_ERR, "!VDD_OPEN: Error %d opening %s"
				        , GetLastError(), str);
				retval = 2;
				break;
			}

			if (RingBufInit(&rdbuf, RINGBUF_SIZE_IN) != 0) {
				retval = 3;
				break;
			}

			sprintf(str, "sbbsexec_hungup%d", node_num);
			hungup_event = OpenEvent(
				EVENT_ALL_ACCESS,   /* access flag  */
				FALSE,              /* inherit flag  */
				str);               /* pointer to event-object name  */
			if (hungup_event == NULL) {
				lprintf(LOG_ERR, "!VDD_OPEN: Error %d opening %s"
				        , GetLastError(), str);
				retval = 4;
				break;
			}

			sprintf(str, "sbbsexec_hangup%d", node_num);
			hangup_event = OpenEvent(
				EVENT_ALL_ACCESS,   /* access flag  */
				FALSE,              /* inherit flag  */
				str);               /* pointer to event-object name  */
			if (hangup_event == NULL) {
				lprintf(LOG_WARNING, "!VDD_OPEN: Error %d opening %s"
				        , GetLastError(), str);
			}

			sprintf(str, "sbbsexec_carrier%d", node_num);
			carrier_event = OpenEvent(
				EVENT_ALL_ACCESS,   /* access flag  */
				FALSE,              /* inherit flag  */
				str);               /* pointer to event-object name  */
			if (carrier_event == NULL) {
				lprintf(LOG_WARNING, "!VDD_OPEN: Error %d opening %s"
				        , GetLastError(), str);
			}

			status_poll = 0;
			inbuf_poll = 0;
			online_poll = 0;
			yields = 0;

			lprintf(LOG_INFO, "Yield interval: %f milliseconds", yield_interval);

			if (uart.virtualize) {
				lprintf(LOG_INFO, "Virtualizing UART (0x%x, IRQ %u)"
				        , uart.io_base, uart.irq);

				IOHandlers.inb_handler = uart_rdport;
				IOHandlers.outb_handler = uart_wrport;
				PortRange.First = uart.io_base;
				PortRange.Last = uart.io_base + UART_IO_RANGE;

				VDDInstallIOHook((HANDLE)getAX(), 1, &PortRange, &IOHandlers);

				interrupt_event = CreateEvent(
					NULL    // pointer to security attributes
					, FALSE  // flag for manual-reset event (TRUE = level-trigger, FALSE = edge-trigger)
					, FALSE  // flag for initial state
					, NULL   // pointer to event-object name
					);
				InitializeCriticalSection(&interrupt_mutex);

				_beginthread(interrupt_thread, 0, NULL);
			}

			lprintf(LOG_DEBUG, "VDD_OPEN: Opened successfully (wrslot=%p)", wrslot);

			_beginthread(input_thread, 0, NULL);

			retval = 0;
			break;

		case VDD_CLOSE:
			lprintf(LOG_INFO, "VDD_CLOSE: rdbuf=%u "
			        "status_poll=%u inbuf_poll=%u online_poll=%u yields=%u vdd_yields=%u vdd_calls=%u"
			        , RingBufFull(&rdbuf), status_poll, inbuf_poll, online_poll
			        , yields, vdd_yields, vdd_calls);
			lprintf(LOG_INFO, "           read=%u bytes (in %u calls)", bytes_read, reads);
			lprintf(LOG_INFO, "           wrote=%u bytes (in %u calls)", bytes_written, writes);

			if (uart.virtualize) {
				lputs(LOG_INFO, "Uninstalling Virtualizaed UART IO Hook");
				VDDDeInstallIOHook((HANDLE)getAX(), 1, &PortRange);
				DeleteCriticalSection(&interrupt_mutex);
			}

			if (rdslot != INVALID_HANDLE_VALUE) {
				CloseHandle(rdslot);
				rdslot = INVALID_HANDLE_VALUE;
			}
			if (wrslot != INVALID_HANDLE_VALUE) {
				CloseHandle(wrslot);
				wrslot = INVALID_HANDLE_VALUE;
			}
			if (hangup_event != NULL) {
				CloseHandle(hangup_event);
				hangup_event = NULL;
			}
			if (hungup_event != NULL) {
				CloseHandle(hungup_event);
				hungup_event = NULL;
			}
			if (carrier_event != NULL) {
				CloseHandle(carrier_event);
				carrier_event = NULL;
			}
			if (interrupt_event != NULL) {
				CloseHandle(interrupt_event);
				interrupt_event = NULL;
			}

#if 0   /* This isn't strictly necessary...
		   and possibly the cause of a NULL dereference in the input_thread */
			RingBufDispose(&rdbuf);
#endif
			status_poll = 0;
			retval = 0;

			break;

		case VDD_READ:
			count = getCX();
			if (count != 1)
				lprintf(LOG_DEBUG, "VDD_READ of %d", count);
			p = (BYTE*) GetVDMPointer((ULONG)((getES() << 16) | getDI())
			                          , count, FALSE);
			retval = vdd_read(p, count);
			reads++;
			bytes_read += retval;
			reset_yield();
			break;

		case VDD_PEEK:
			count = getCX();
			if (count != 1)
				lprintf(LOG_DEBUG, "VDD_PEEK of %d", count);

			p = (BYTE*) GetVDMPointer((ULONG)((getES() << 16) | getDI())
			                          , count, FALSE);
			retval = RingBufPeek(&rdbuf, p, count);
			reset_yield();
			break;

		case VDD_WRITE:
			count = getCX();
			if (count != 1)
				lprintf(LOG_DEBUG, "VDD_WRITE of %d", count);
			p = (BYTE*) GetVDMPointer((ULONG)((getES() << 16) | getDI())
			                          , count, FALSE);
			if (!WriteFile(wrslot, p, count, &retval, NULL)) {
				lprintf(LOG_ERR, "!VDD_WRITE: WriteFile Error %d (size=%d)"
				        , GetLastError(), retval);
				retval = 0;
			} else {
				writes++;
				bytes_written += retval;
				reset_yield();
			}
			break;

		case VDD_STATUS:

			status_poll++;
			count = getCX();
			if (count != sizeof(vdd_status_t)) {
				lprintf(LOG_DEBUG, "!VDD_STATUS: wrong size (%d!=%d)", count, sizeof(vdd_status_t));
				retval = sizeof(vdd_status_t);
				break;
			}
			status = (vdd_status_t*) GetVDMPointer((ULONG)((getES() << 16) | getDI())
			                                       , count, FALSE);

			status->inbuf_size = RINGBUF_SIZE_IN;
			status->inbuf_full = RingBufFull(&rdbuf);
			msgs = 0;

			/* OUTBUF FULL/SIZE */
			if (!GetMailslotInfo(
					wrslot,             /* mailslot handle  */
					&status->outbuf_size, /* address of maximum message size  */
					&status->outbuf_full, /* address of size of next message  */
					&msgs,              /* address of number of messages  */
					NULL                /* address of read time-out  */
					)) {
				// Known to fail with error 87 (The Parameter is Incorrect) on Windows 7 <shrug>
				lprintf(LOG_DEBUG, "!VDD_STATUS: GetMailSlotInfo(%p) failed, error %u (msgs=%u, inbuf_full=%u, inbuf_size=%u)"
				        , wrslot
				        , GetLastError(), msgs, status->inbuf_full, status->inbuf_size);
				status->outbuf_full = 0;
				status->outbuf_size = DEFAULT_MAX_MSG_SIZE;
			} else
				lprintf(LOG_DEBUG, "VDD_STATUS: MailSlot output: maxmsgsize=%u, nextmsgsize=%d, msgs=%u, input: %u"
				        , status->outbuf_size
				        , status->outbuf_full
				        , msgs
				        , status->inbuf_full);
			if (status->outbuf_full == MAILSLOT_NO_MESSAGE)
				status->outbuf_full = 0;
			status->outbuf_full *= msgs;

			/* ONLINE */
			if (WaitForSingleObject(hungup_event, 0) == WAIT_OBJECT_0)
				status->online = 0;
			else
				status->online = 1;

			retval = 0;   /* success */
			break;

		case VDD_INBUF_PURGE:
			RingBufReInit(&rdbuf);
			retval = 0;
			break;

		case VDD_OUTBUF_PURGE:
			lputs(LOG_WARNING, "!VDD_OUTBUF_PURGE: NOT IMPLEMENTED");
			retval = 0;
			break;

		case VDD_INBUF_FULL:
			retval = RingBufFull(&rdbuf);
			inbuf_poll++;
			break;

		case VDD_INBUF_SIZE:
			retval = RINGBUF_SIZE_IN;
			break;

		case VDD_OUTBUF_FULL:
			if (!GetMailslotInfo(
					wrslot, /* mailslot handle  */
					NULL,   /* address of maximum message size  */
					&retval, /* address of size of next message  */
					&msgs,  /* address of number of messages  */
					NULL    /* address of read time-out  */
					))
				retval = 0;
			if (retval == MAILSLOT_NO_MESSAGE)
				retval = 0;
			retval *= msgs;
			break;

		case VDD_OUTBUF_SIZE:
			if (!GetMailslotInfo(
					wrslot, /* mailslot handle  */
					&retval, /* address of maximum message size  */
					NULL,   /* address of size of next message  */
					NULL,   /* address of number of messages  */
					NULL    /* address of read time-out  */
					))
				retval = DEFAULT_MAX_MSG_SIZE;
			break;

		case VDD_ONLINE:
			if (WaitForSingleObject(hungup_event, 0) == WAIT_OBJECT_0)
				retval = 0;
			else
				retval = 1;
			online_poll++;
			break;

		case VDD_YIELD:         /* forced yield */
			vdd_yields++;
			yield();
			break;

		case VDD_MAYBE_YIELD:   /* yield if YieldInterval is enabled and expired */
			maybe_yield();
			break;

		case VDD_LOAD_INI_FILE: /* Load and parse settings file */
		{
			FILE* fp;
			char  cwd[MAX_PATH + 1];

			/* Load exec/sbbsexec.ini first (setting default values) */
			count = getCX();
			p = (BYTE*)GetVDMPointer((ULONG)((getES() << 16) | getDI())
			                         , count, FALSE);
			lprintf(LOG_INFO, "LOAD_INI_FILE: %s", p);
			if (isdir(p))
				iniFileName(ini_fname, sizeof(ini_fname), p, INI_FILENAME);
			else
				SAFECOPY(ini_fname, p);
			if ((fp = fopen(ini_fname, "r")) != NULL) {
				iniFreeStringList(ini);
				ini = iniReadFile(fp);
				fclose(fp);
				parse_ini(ROOT_SECTION);
				lprintf(LOG_INFO, "Parsed root section of: %s", ini_fname);
			}

			/* Load cwd/sbbsexec.ini second (over-riding default values) */
			GetCurrentDirectory(sizeof(cwd), cwd);
			if (getfname(p) == p)
				iniFileName(ini_fname, sizeof(ini_fname), cwd, p);
			else
				iniFileName(ini_fname, sizeof(ini_fname), cwd, INI_FILENAME);
			if ((fp = fopen(ini_fname, "r")) != NULL) {
				iniFreeStringList(ini);
				ini = iniReadFile(fp);
				fclose(fp);
				parse_ini(ROOT_SECTION);
				lprintf(LOG_INFO, "Parsed root section of: %s", ini_fname);
			}
		}
		break;

		case VDD_LOAD_INI_SECTION:  /* Parse (program-specific) sub-section of settings file */
			count = getCX();
			p = (BYTE*)GetVDMPointer((ULONG)((getES() << 16) | getDI())
			                         , count, FALSE);
			lprintf(LOG_INFO, "LOAD_INI_SECTION: %s", p);
			parse_ini(p);
			break;

		case VDD_DEBUG_OUTPUT:  /* Send string to debug output */
			count = getCX();
			p = (BYTE*)GetVDMPointer((ULONG)((getES() << 16) | getDI())
			                         , count, FALSE);
			lputs(LOG_INFO, p);
			break;

		case VDD_HANGUP:
			hangup();
			break;

		case VDD_VIRTUALIZE_UART:
			uart.virtualize = TRUE;
			break;

		default:
			lprintf(LOG_ERR, "!UNKNOWN VDD_OP: %d", getBL());
			break;
	}
	setAX((WORD)retval);
}

__declspec(dllexport) BOOL __cdecl VDDInitialize(IN PVOID hVDD, IN ULONG Reason,
                                                 IN PCONTEXT Context OPTIONAL)
{
	memcpy(&uart, &default_uart, sizeof(uart));
	log_level = LOG_INFO;
	lputs(LOG_INFO, __FUNCTION__);
	return TRUE;
}
