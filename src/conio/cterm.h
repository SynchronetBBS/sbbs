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

#ifndef _CTERM_H_
#define _CTERM_H_

#include <stdio.h>	/* FILE* */

enum {
	 CTERM_MUSIC_NORMAL
	,CTERM_MUSIC_LEGATO
	,CTERM_MUSIC_STACATTO
};

enum {
	 CTERM_LOG_NONE
	,CTERM_LOG_ASCII
	,CTERM_LOG_RAW
};

#define CTERM_LOG_MASK	0x7f
#define CTERM_LOG_PAUSED	0x80

struct cterminal {
	int emulation;
	int c64reversemode;
	int	height;
	int	width;
	int	x;
	int	y;
	char *buffer;
	int	attr;
	int save_xpos;
	int save_ypos;
	char	escbuf[1024];
	int	sequence;
	int music_enable;
	char	musicbuf[1024];
	int music;
	int	tempo;
	int	octave;
	int notelen;
	int noteshape;
	int musicfore;
	char *scrollback;
	int backpos;
	int backlines;
	int	xpos;
	int ypos;
	int log;
	FILE* logfile;
	char	DA[1024];
	char	fontbuf[4096];
	int		font_slot;
	int		font_size;
	int		font_read;
	int		font_start_time;
	int		doorway_mode;
	int		doorway_char;
};

#define CTERM_MUSIC_SYNCTERM	0
#define	CTERM_MUSIC_BANSI		1
#define	CTERM_MUSIC_ENABLED		2

enum {
	 CTERM_EMULATION_ANSI_BBS
	,CTERM_EMULATION_PETASCII
	,CTERM_EMULATION_ATASCII
};

#ifdef __cplusplus
extern "C" {
#endif

extern struct cterminal cterm;

void cterm_init(int height, int width, int xpos, int ypos, int backlines, unsigned char *scrollback, int emulation);
char *cterm_write(unsigned char *buf, int buflen, char *retbuf, size_t retsize, int *speed);
int cterm_openlog(char *logfile, int logtype);
void cterm_closelog(void);
void cterm_end(void);
#ifdef __cplusplus
}
#endif

#endif
