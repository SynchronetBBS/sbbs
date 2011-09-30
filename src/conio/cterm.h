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
#include <link_list.h>
#include <semwrap.h>

typedef enum {
	 CTERM_MUSIC_NORMAL
	,CTERM_MUSIC_LEGATO
	,CTERM_MUSIC_STACATTO
} cterm_noteshape_t;

typedef enum {
	 CTERM_LOG_NONE
	,CTERM_LOG_ASCII
	,CTERM_LOG_RAW
} cterm_log_t;

typedef enum {
	 CTERM_EMULATION_ANSI_BBS
	,CTERM_EMULATION_PETASCII
	,CTERM_EMULATION_ATASCII
} cterm_emulation_t;

typedef enum {
	CTERM_MUSIC_SYNCTERM,
	CTERM_MUSIC_BANSI,
	CTERM_MUSIC_ENABLED
} cterm_music_t;

#define CTERM_LOG_MASK	0x7f
#define CTERM_LOG_PAUSED	0x80

struct cterminal {
	/* conio stuff */
	int	x;		// X position of the left side on the screen
	int	y;		// Y position of the top pn the screen

	/* emulation mode */
	cterm_emulation_t	emulation;
	int					height;			// Height of the terminal buffer
	int					width;			// Width of the terminal buffer
	int					quiet;			// No sounds are made
	char				*scrollback;
	int					backlines;		// Number of lines in scrollback
	char				DA[1024];		// Device Attributes

	/* emulation state */
	int					started;		// Indicates that conio functions are being called
	int					c64reversemode;	// Commodore 64 reverse mode state
	unsigned char		attr;			// Current attribute
	int					save_xpos;		// Saved position (for later restore)
	int					save_ypos;
	int					sequence;		// An escape sequence is being parsed
	char				escbuf[1024];
	cterm_music_t		music_enable;	// The remotely/locally controled music state
	char				musicbuf[1024];
	int					music;			// ANSI music is being parsed
	int					tempo;
	int					octave;
	int					notelen;
	cterm_noteshape_t	noteshape;
	int					musicfore;
	int					playnote_thread_running;
	link_list_t			notes;
	sem_t				playnote_thread_terminated;
	sem_t				note_completed_sem;
	int					backpos;
	int					xpos;
	int					ypos;
	cterm_log_t			log;
	FILE*				logfile;
	char				fontbuf[4096];	// Remote font
	int					font_read;		// Current position in fontbuf
	int					font_slot;
	int					font_size;		// Bytes
	int					doorway_mode;
	int					doorway_char;	// Indicates next char is a "doorway" mode char
	int					cursor;			// Current cursor mode (Normal or None)

	/* conio function pointers */
#ifdef CTERM_WITHOUT_CONIO
	void	(*ciolib_gotoxy)		(struct cterminal *,int,int);
	int		(*ciolib_wherex)		(struct cterminal *);
	int		(*ciolib_wherey)		(struct cterminal *);
	int		(*ciolib_gettext)		(struct cterminal *,int,int,int,int,unsigned char *);
	void	(*ciolib_gettextinfo)	(struct cterminal *,struct text_info *);
	void	(*ciolib_textattr)		(struct cterminal *,int);
	void	(*ciolib_setcursortype)	(struct cterminal *,int);
	int		(*ciolib_movetext)		(struct cterminal *,int,int,int,int,int,int);
	void	(*ciolib_clreol)		(struct cterminal *);
	void	(*ciolib_clrscr)		(struct cterminal *);
	void	(*ciolib_setvideoflags)	(struct cterminal *,int flags);
	int		(*ciolib_getvideoflags)	(struct cterminal *);
	int		(*ciolib_putch)			(struct cterminal *,int);
	int		(*ciolib_puttext)		(struct cterminal *,int,int,int,int,unsigned char *);
	void	(*ciolib_window)		(struct cterminal *,int,int,int,int);
	int		(*ciolib_cputs)			(struct cterminal *,char *);
	int		(*ciolib_setfont)		(struct cterminal *,int font, int force, int font_num);
#else
	void	(*ciolib_gotoxy)		(int,int);
	int		(*ciolib_wherex)		(void);
	int		(*ciolib_wherey)		(void);
	int		(*ciolib_gettext)		(int,int,int,int,unsigned char *);
	void	(*ciolib_gettextinfo)	(struct text_info *);
	void	(*ciolib_textattr)		(int);
	void	(*ciolib_setcursortype)	(int);
	int		(*ciolib_movetext)		(int,int,int,int,int,int);
	void	(*ciolib_clreol)		(void);
	void	(*ciolib_clrscr)		(void);
	void	(*ciolib_setvideoflags)	(int flags);
	int		(*ciolib_getvideoflags)	(void);
	int		(*ciolib_putch)			(int);
	int		(*ciolib_puttext)		(int,int,int,int,unsigned char *);
	void	(*ciolib_window)		(int,int,int,int);
	int		(*ciolib_cputs)			(char *);
	int		(*ciolib_setfont)		(int font, int force, int font_num);
#endif
	int 	*_wscroll;
	int		*puttext_can_move;
	int		*hold_update;
	void	*extra;
};

#ifdef __cplusplus
extern "C" {
#endif

struct cterminal *cterm_init(int height, int width, int xpos, int ypos, int backlines, unsigned char *scrollback, int emulation);
char *cterm_write(struct cterminal *cterm, const unsigned char *buf, int buflen, char *retbuf, size_t retsize, int *speed);
int cterm_openlog(struct cterminal *cterm, char *logfile, int logtype);
void cterm_closelog(struct cterminal *cterm);
void cterm_end(struct cterminal *cterm);
void cterm_clearscreen(struct cterminal *cterm, char attr);
void cterm_start(struct cterminal *cterm);
#ifdef __cplusplus
}
#endif

#endif
