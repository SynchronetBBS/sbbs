/* FOSSIL (FSC-15) structure and constant definitions */

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

#ifndef _FOSSDEFS_H
#define _FOSSDEFS_H

#include <gen_defs.h>

#define FOSSIL_REVISION			5		/* the latest and greatest (and last?) */
#define FOSSIL_INTERRUPT		0x14	/* x86 interrupt 14h */
#define FOSSIL_SIGNATURE		0x1954	/* magic number */

/* FOSSIL functions (in AH) */
#define FOSSIL_FUNC_SET_RATE	0x00
#define FOSSIL_FUNC_PUT_CHAR	0x01	/* transmit with wait */
#define FOSSIL_FUNC_GET_CHAR	0x02	/* receive char, with wait */
#define FOSSIL_FUNC_GET_STATUS	0x03
#define FOSSIL_FUNC_INIT		0x04
#define FOSSIL_FUNC_UNINIT		0x05
#define FOSSIL_FUNC_DTR			0x06
#define FOSSIL_FUNC_GET_TIMER	0x07
#define FOSSIL_FUNC_FLUSH_OUT	0x08
#define FOSSIL_FUNC_PURGE_OUT	0x09
#define FOSSIL_FUNC_PURGE_IN	0x0a
#define FOSSIL_FUNC_WRITE_CHAR	0x0b	/* transmit no wait */
#define FOSSIL_FUNC_PEEK		0x0c	/* non-destructive read ahead */
#define FOSSIL_FUNC_GET_KB		0x0d	/* keyboard read no wait */
#define FOSSIL_FUNC_GET_KB_WAIT	0x0e	/* keyboard read with wait */
#define FOSSIL_FUNC_FLOW_CTRL	0x0f	/* enable/disable flow control */
#define FOSSIL_FUNC_CTRL_C		0x10	/* Ctrl-C/K checking */
#define FOSSIL_FUNC_SET_CURSOR	0x11
#define FOSSIL_FUNC_GET_CURSOR	0x12
#define FOSSIL_FUNC_ANSI_PRINT	0x13
#define FOSSIL_FUNC_WATCHDOG	0x14	/* enable/disable watchdog processing */
#define FOSSIL_FUNC_BIOS_PRINT	0x15
#define FOSSIL_FUNC_TIMER_CHAIN	0x16	/* insert/deleted timer callbacks */
#define FOSSIL_FUNC_REBOOT		0x17
#define FOSSIL_FUNC_READ_BLOCK	0x18	/* read block, no wait */
#define FOSSIL_FUNC_WRITE_BLOCK	0x19	/* write block, no wait */
#define FOSSIL_FUNC_BREAK		0x1a
#define FOSSIL_FUNC_GET_INFO	0x1b

#define FOSSIL_FUNC_HIGHEST		FOSSIL_FUNC_GET_INFO

#define FOSSIL_BAUD_RATE_SHIFT	5
#define FOSSIL_BAUD_RATE_MASK	0xe0	/* 11100000 */
#define FOSSIL_BAUD_RATE_300	0x40	/* 010xxxxx */
#define FOSSIL_BAUD_RATE_600	0x60	/* 011xxxxx */
#define FOSSIL_BAUD_RATE_1200	0x80	/* 100xxxxx */
#define FOSSIL_BAUD_RATE_2400	0xa0	/* 101xxxxx */
#define FOSSIL_BAUD_RATE_4800	0xc0	/* 110xxxxx */
#define FOSSIL_BAUD_RATE_9600	0xe0	/* 111xxxxx */
#define FOSSIL_BAUD_RATE_19200	0x00	/* 000xxxxx - replaces old 110 baud */
#define FOSSIL_BAUD_RATE_38400	0x20	/* 001xxxxx - replaces old 150 baud */

unsigned fossil_baud_rate[] = {
	 19200
	,38400
	,300
	,600
	,1200
	,2400
	,4800
	,9600
};

#define FOSSIL_PARITY_SHIFT		3
#define FOSSIL_PARITY_MASK		0x18	/* 00011000 */
#define FOSSIL_PARITY_NONE		0x00	/* xxx00xxx (and xxx10xxx) */
#define FOSSIL_PARITY_ODD		0x08	/* xxx01xxx */
#define FOSSIL_PARITY_EVEN		0x18	/* xxx11xxx */

char fossil_parity[] = { 'N', 'O', 'N', 'E' };

#define FOSSIL_STOP_BITS_SHIFT	2
#define FOSSIL_STOP_BITS_MASK	0x04	/* 00000100 */
#define FOSSIL_STOP_BITS_1		0x00	/* xxxxx0xx */
#define FOSSIL_STOP_BITS_2		0x04	/* xxxxx1xx */

unsigned fossil_stop_bits[] = { 1, 2 };

#define FOSSIL_DATA_BITS_SHIFT	0
#define FOSSIL_DATA_BITS_MASK	0x03	/* 00000011 */
#define FOSSIL_DATA_BITS_5		0x00	/* xxxxxx00 */
#define FOSSIL_DATA_BITS_6		0x01	/* xxxxxx01 */
#define FOSSIL_DATA_BITS_7		0x02	/* xxxxxx10 */
#define FOSSIL_DATA_BITS_8		0x03	/* xxxxxx11 */

unsigned fossil_data_bits[] = { 5, 6, 7, 8 };

#define FOSSIL_MDM_STATUS_CTS_CHNG	(1<<0)	// Delta clear to send (not reliable)
#define FOSSIL_MDM_STATUS_DSR_CHNG	(1<<1)	// Delta data set ready (not reliable)
#define FOSSIL_MDM_STATUS_RI_CHNG	(1<<2)	// trailing edge of ring indicator (documented wrong in X00REF.DOC)
#define FOSSIL_MDM_STATUS_DCD_CHNG	(1<<3)	// Delta data carrier detect 
#define FOSSIL_MDM_STATUS_CTS		(1<<4)	// Clear to send
#define FOSSIL_MDM_STATUS_DSR		(1<<5)	// Data set ready
#define FOSSIL_MDM_STATUS_RI		(1<<6)	// Ring indicator
#define FOSSIL_MDM_STATUS_DCD		(1<<7)	// Data carrier detect

#define FOSSIL_LINE_STATUS_RDA		(1<<8)	// input data is available in buffer
#define FOSSIL_LINE_STATUS_OVRN		(1<<9)	// the input buffer has been overrun
#define FOSSIL_LINE_STATUS_THRE		(1<<13)	// room is available in output buffer
#define FOSSIL_LINE_STATUS_TSRE		(1<<14)	// output buffer is empty
#define FOSSIL_LINE_STATUS_TIMEOUT	(1<<15)	// Timeout (set by functions 1 and 2 only)

#define FOSSIL_CHAR_NOT_AVAILABLE	0xffff

#if defined(__GNUC__)
	#define PACKED_STRUCT __attribute__((packed))
#else	/* non-GCC compiler */
	#pragma pack(push)
	#pragma pack(1)
	#define PACKED_STRUCT
#endif

typedef struct {
    WORD    info_size;
    BYTE	curr_fossil;
    BYTE	curr_rev;
    DWORD	id_string;
    WORD	inbuf_size;
    WORD	inbuf_free;
    WORD	outbuf_size;
    WORD	outbuf_free;
    BYTE	screen_width;
    BYTE	screen_height;
    BYTE	baud_rate;
} PACKED_STRUCT fossil_info_t;

#if !defined(__GNUC__)
	#pragma pack(pop)		/* original packing */
#endif

#endif /* Don't add anything after this line */
