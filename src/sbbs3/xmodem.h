/* xmodem.h */

/* Synchronet X/YMODEM Functions */

/* $Id: xmodem.h,v 1.24 2018/07/24 01:11:08 rswindell Exp $ */

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

#ifndef _XMODEM_H_
#define _XMODEM_H_

#include "gen_defs.h"

#define CPMEOF		CTRL_Z	/* CP/M End of file (^Z)					*/
#define XMODEM_MIN_BLOCK_SIZE	128
#define XMODEM_MAX_BLOCK_SIZE	1024

typedef struct {

	void*		cbdata;
	long*		mode;
	BOOL		cancelled;
	BOOL		crc_mode_supported;	/* for send */
	BOOL		g_mode_supported;	/* for send */
	unsigned	block_size;
	unsigned	max_block_size;		/* for recv */
	unsigned	ack_timeout;
	unsigned	byte_timeout;
	unsigned	send_timeout;
	unsigned	recv_timeout;
	unsigned	errors;
	unsigned	max_errors;
	unsigned	fallback_to_xmodem; /* fallback to Xmodem after this many Ymodem send attempts */
	unsigned	g_delay;
	ulong		total_files;
	int64_t		total_bytes;
	unsigned	sent_files;
	int64_t		sent_bytes;
	int			*log_level;
	int			(*lputs)(void*, int level, const char* str);
	void		(*progress)(void*, unsigned block_num, int64_t offset, int64_t fsize, time_t t);
	int			(*send_byte)(void*, uchar ch, unsigned timeout);
	int			(*recv_byte)(void*, unsigned timeout);
	BOOL		(*is_connected)(void*);
	BOOL		(*is_cancelled)(void*);
	void		(*flush)(void*);

} xmodem_t;


void		xmodem_init(xmodem_t*, void* cbdata, long* mode
						,int	(*lputs)(void*, int level, const char* str)
						,void	(*progress)(void* unused, unsigned block_num, int64_t offset, int64_t fsize, time_t t)
						,int	(*send_byte)(void*, uchar ch, unsigned timeout)
						,int	(*recv_byte)(void*, unsigned timeout)
						,BOOL	(*is_connected)(void*)
						,BOOL	(*is_cancelled)(void*)
						,void	(*flush)(void*)
						);
char*		xmodem_ver(char *buf);
const char* xmodem_source(void);
int			xmodem_cancel(xmodem_t*);
int			xmodem_get_ack(xmodem_t*, unsigned tries, unsigned block_num);
BOOL		xmodem_get_mode(xmodem_t*);
BOOL		xmodem_put_eot(xmodem_t*);
int			xmodem_put_ack(xmodem_t*);
int			xmodem_put_nak(xmodem_t*, unsigned block_num);
int			xmodem_get_block(xmodem_t*, uchar* block, unsigned block_num);
int			xmodem_put_block(xmodem_t*, uchar* block, unsigned block_size, unsigned block_num);
BOOL		xmodem_send_file(xmodem_t* xm, const char* fname, FILE* fp, time_t* start, uint64_t* sent);

#endif	/* Don't add anything after this line */
