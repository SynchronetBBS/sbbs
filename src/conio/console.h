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


#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>

#include <gen_defs.h>
#include <semwrap.h>

#include "vidmodes.h"

extern sem_t	console_mode_changed;
extern sem_t	copybuf_set;
extern sem_t	pastebuf_request;
extern sem_t	pastebuf_set;
extern sem_t	font_set;
extern int		new_font;
extern int		font_force;
extern int		setfont_return;
extern pthread_mutex_t	copybuf_mutex;
extern char *copybuf;
extern char *pastebuf;

extern int CurrMode;

extern int InitCS;
extern int InitCE;

extern WORD *vmem;

extern BYTE CursRow;
extern BYTE CursCol;
extern BYTE CursStart;
extern BYTE CursEnd;

extern WORD DpyCols;
extern BYTE DpyRows;

extern int FH,FW;

extern int x_nextchar;

extern int console_new_mode;

int init_window();
int video_init();
int init_mode(int mode);
int tty_read(int flag);
int tty_peek(int flag);
int tty_kbhit(void);
void tty_beep(void);
void x_win_title(const char *title);
int console_init(void);

#define	TTYF_BLOCK	0x00000008
#define	TTYF_POLL	0x00000010
#define NO_NEW_MODE -999

#endif
