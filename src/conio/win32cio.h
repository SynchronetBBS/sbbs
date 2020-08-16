/* $Id: win32cio.h,v 1.15 2018/07/24 01:10:58 rswindell Exp $ */

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

#ifndef _WIN32CIO_H_
#define _WIN32CIO_H_

#ifdef __cplusplus
extern "C" {
#endif
int win32_kbhit(void);
int win32_getch(void);
int win32_getmouse(struct cio_mouse_event *mevent);
int win32_hidemouse(void);
int win32_showmouse(void);

int	win32_gettext(int left, int top, int right, int bottom, void*);
void	win32_gotoxy(int x, int y);
int	win32_puttext(int left, int top, int right, int bottom, void*);
void	win32_textmode(int newmode);
void	win32_setcursortype(int);
int	win32_getch(void);
int	win32_kbhit(void);
void	win32_settitle(const char *title);
int	win32_initciolib(long inmode);
void win32_copytext(const char *text, size_t buflen);
char *win32_getcliptext(void);
void	win32_suspend(void);
void	win32_resume(void);
void win32_getcustomcursor(int *s, int *e, int *r, int *b, int *v);
void win32_setcustomcursor(int s, int e, int r, int b, int v);
int win32_getvideoflags(void);
int win32_setpalette(uint32_t entry, uint16_t r, uint16_t g, uint16_t b);

#ifdef __cplusplus
}
#endif

#endif
