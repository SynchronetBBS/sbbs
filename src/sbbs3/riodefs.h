/* riodefs.h */

/* Synchronet legacy (v2) remote I/O constants */

/* $Id: riodefs.h,v 1.2 2018/07/24 01:11:07 rswindell Exp $ */

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

#ifndef _RIODEFS_H
#define _RIODEFS_H


/************************/
/* Remote I/O Constants */
/************************/

                            /* i/o mode and state flags */
#define CTSCK	0x1000		/*.check cts (mode only) */
#define RTSCK	0x2000		/*.check rts (mode only) */
#define TXBOF	0x0800		/*.transmit buffer overflow (outcom only) */
#define ABORT	0x0400		/*.check for ^C (mode), aborting (state) */
#define PAUSE	0x0200		/*.check for ^S (mode), pausing (state) */
#define NOINP	0x0100		/*.input buffer empty (incom only) */

                            /* status flags */
#define DCD 	0x0080		/*.DCD on */
#define RI		0x0040		/*.Ring indicate */
#define DSR 	0x0020		/*.Dataset ready */
#define CTS 	0x0010		/*.CTS on */
#define FERR	0x0008		/*.Frameing error */
#define PERR	0x0004		/*.Parity error */
#define OVRR	0x0002		/*.Overrun */
#define RXLOST	0x0001		/*.Receive buffer overflow */

/* rioctl() arguments */
/* returns mode or state flags in high 8 bits, status flags in low 8 bits */

                            /* the following return mode in high 8 bits */
#define IOMODE	0x0000		/*.no operation */
#define IOSM	0x0001		/*.i/o set mode flags */
#define IOCM	0x0002		/*.i/o clear mode flags */

#define GVERS	0x0007		/*.get version */
#define GUART	0x0107		/*.get uart */
#define GIRQN	0x0207		/*.get IRQ number */
#define GBAUD	0x0307		/*.get baud */

                            /* the following return state in high 8 bits */
#define IOSTATE 0x0004		/*.no operation */
#define IOSS	0x0005		/*.i/o set state flags */
#define IOCS	0x0006		/*.i/o clear state flags */
#define IOFB	0x0308		/*.i/o buffer flush */
#define IOFI	0x0208		/*.input buffer flush */
#define IOFO	0x0108		/*.output buffer flush */
#define IOCE	0x0009		/* i/o clear error flags */


                            /* return count (16bit) */
#define RXBC	0x000a		/*.get receive buffer count */
#define RXBS	0x010a		/*.get receive buffer size */
#define TXBC	0x000b		/*.get transmit buffer count */
#define TXBS	0x010b		/*.get transmit buffer size */
#define TXBF	0x020b		/*.get transmit buffer free space */
#define TXSYNC	0x000c		/*.sync transmition (seconds<<8|0x0c) */
#define IDLE    0x000d      /* suspend communication routines */
#define RESUME  0x010d      /* return from suspended state */
#define RLERC   0x000e      /* read line error count and clear */
#define CPTON   0x0110      /* set input translation flag for ctrl-p on */
#define CPTOFF  0x0010      /* set input translation flag for ctrl-p off */
#define GETCPT  0x8010      /* return the status of ctrl-p translation */
#define MSR     0x0011      /* read modem status register */
#define FIFOCTL 0x0012		/*.FIFO UART control */
#define TSTYPE  0x0013      /* Time-slice API type */
#define GETTST  0x8013      /* Get Time-slice API type */

#define I14DB   0x001d      /* DigiBoard int 14h driver */
#define I14PC   0x011d      /* PC int 14h driver */
#define I14PS   0x021d      /* PS/2 int 14h driver */
#define I14FO   0x031d      /* FOSSIL int 14h driver */

#define SMSMK   0x0014      /* set modem status mask */
#define SMLCR   0x0015      /* set modem line control register */
#define LFN81	0x0315		/*.set line format N81 */
#define LFE71	0x1A15		/*.set line format E71 */
#define SRXHL   0x001E      /* set receive flow control high limit */
#define SRXLL   0x001F      /* set receive flow control low limit */
#define IGCLS   0x0020      /* input gate close */
#define IGOPN   0xFF20      /* input gate open */


                            /* ivhctl() arguments */
#define INT29R	0x0001		/* copy int 29h output to remote */
#define INT29L	0x0002		/* Use _putlc for int 29h */
#define INT16	0x0010		/* return remote chars to int 16h calls */
#define INTCLR	0x0000		/* release int 16h, int 29h */

#define TS_INT28	 1
#define TS_WINOS2    2
#define TS_NODV      4

#define RIO_OUTCOM_STKLEN	4096	/* outcom_thread() stack size	*/
#define RIO_OUTCOM_BUFLEN	4096	/* outcom() buffer length		*/
#define RIO_INCOM_BUFLEN	1024	/* incom() buffer length		*/


#endif	/* Don't add anything after this line */
