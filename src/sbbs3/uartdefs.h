/* uartdefs.h */

/* 8250 Universal Asynchronous Receiver/Transmitter definitions */

/* $Id: uartdefs.h,v 1.6 2018/07/24 01:11:08 rswindell Exp $ */

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

#ifndef _UARTDEFS_H
#define _UARTDEFS_H

/* UART Registers */
#define UART_BASE				0		/* tx/rx data */
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

#define UART_IER_MODEM_STATUS	(1<<3)
#define UART_IER_LINE_STATUS	(1<<2)
#define UART_IER_TX_EMPTY		(1<<1)
#define UART_IER_RX_DATA		(1<<0)

#define UART_IIR_MODEM_STATUS	0x00
#define UART_IIR_NONE			0x01	/* Bit 0=0, Interrupt Pending */
#define UART_IIR_TX_EMPTY		0x02	/* THRE (Transmit Holding Register Empty) */
#define UART_IIR_RX_DATA		0x04
#define UART_IIR_LINE_STATUS	0x06

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
#define UART_LCR_8_DATA_BITS	0x03		/* 8 data bits */

#define UART_MCR_DTR			(1<<0)
#define UART_MCR_RTS			(1<<1)
#define UART_MCR_AUX1			(1<<2)
#define UART_MCR_AUX2			(1<<3)
#define UART_MCR_LOOPBACK		(1<<4)

/* I/O Ports */
#define UART_COM1_IO_BASE		0x3f8
#define UART_COM2_IO_BASE		0x2f8
#define UART_COM3_IO_BASE		0x3e8
#define UART_COM4_IO_BASE		0x2e8

/* IRQs */
#define UART_COM1_IRQ			4
#define UART_COM2_IRQ			3
#define UART_COM3_IRQ			4
#define UART_COM4_IRQ			3

#endif /* Don't add anything after this line */
