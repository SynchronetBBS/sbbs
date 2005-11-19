/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifdef __unix__
#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#include "ciolib.h"
#include "console.h"

#ifdef __cplusplus
extern "C" {
#endif
int x_puttext(int sx, int sy, int ex, int ey, void *fill);
int x_gettext(int sx, int sy, int ex, int ey, void *fill);
void x_textattr(int attr);
int x_kbhit(void);
void x_delay(long msec);
int x_wherey(void);
int x_wherex(void);
int x_putch(int ch);
void x_gotoxy(int x, int y);
void x_initciolib(long inmode);
void x_gettextinfo(struct text_info *info);
void x_setcursortype(int type);
int x_getch(void);
int x_getche(void);
int x_beep(void);
void x_textmode(int mode);
void x_setname(const char *name);
void x_settitle(const char *title);
void x_copytext(const char *text, size_t buflen);
char *x_getcliptext(void);
int x_setfont(int font, int force);
int x_getfont(void);
int x_loadfont(char *filename);
#ifdef __cplusplus
}
#endif

#endif
