/* $Id: ansi_cio.h,v 1.11 2018/07/24 01:10:58 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#include "ciolib.h"

#ifdef __cplusplus
extern "C" {
#endif
extern int	CIOLIB_ANSI_TIMEOUT;
extern int  (*ciolib_ansi_readbyte_cb)(void);
extern int  (*ciolib_ansi_writebyte_cb)(const unsigned char ch);
extern int  (*ciolib_ansi_initio_cb)(void);
extern int  (*ciolib_ansi_writestr_cb)(const unsigned char *str, size_t len);

int ansi_readbyte_cb(void);
int ansi_writebyte_cb(const unsigned char ch);
int ansi_initio_cb(void);
int ansi_writestr_cb(const unsigned char *str, size_t len);

int ansi_puttext(int sx, int sy, int ex, int ey, void *fill);
int ansi_gettext(int sx, int sy, int ex, int ey, void *fill);
void ansi_textattr(int);
int ansi_kbhit(void);
void ansi_delay(long msec);
int ansi_wherey(void);
int ansi_wherex(void);
int ansi_putch(int ch);
void ansi_gotoxy(int x, int y);
int ansi_initciolib(long inmode);
void ansi_gettextinfo(struct text_info *info);
void ansi_setcursortype(int type);
int ansi_getch(void);
int ansi_getche(void);
void ansi_beep(void);
void ansi_textmode(int mode);
#ifdef __cplusplus
}
#endif

