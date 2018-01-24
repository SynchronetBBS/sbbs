/* $Id$ */

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

#ifndef _CTERM_H_
#define _CTERM_H_

#include <stdio.h>	/* FILE* */
#if !(defined __BORLANDC__ || defined _MSC_VER)
#include <stdbool.h>
#else
#define bool int
enum { false, true };
#endif
#include <link_list.h>
#include <semwrap.h>
#include "ciolib.h"

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
	int setfont_result;

	/* emulation mode */
	cterm_emulation_t	emulation;
	int					height;			// Height of the terminal buffer
	int					width;			// Width of the terminal buffer
	int					top_margin;
	int					bottom_margin;
	int					quiet;			// No sounds are made
	unsigned char				*scrollback;
	int					backlines;		// Number of lines in scrollback
	char				DA[1024];		// Device Attributes
	bool				autowrap;
	bool				origin_mode;
#define	CTERM_SAVEMODE_AUTOWRAP			0x001
#define CTERM_SAVEMODE_CURSOR			0x002
#define	CTERM_SAVEMODE_ALTCHARS			0x004
#define CTERM_SAVEMODE_NOBRIGHT			0x008
#define CTERM_SAVEMODE_BGBRIGHT			0x010
#define CTERM_SAVEMODE_DOORWAY			0x020
#define CTERM_SAVEMODE_ORIGIN			0x040
#define	CTERM_SAVEMODE_BLINKALTCHARS	0x080
#define CTERM_SAVEMODE_NOBLINK			0x100
	int32_t				saved_mode;
	int32_t				saved_mode_mask;

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
	int		(*ciolib_gettext)		(struct cterminal *,int,int,int,int,void *);
	void	(*ciolib_gettextinfo)	(struct cterminal *,struct text_info *);
	void	(*ciolib_textattr)		(struct cterminal *,int);
	void	(*ciolib_setcursortype)	(struct cterminal *,int);
	int		(*ciolib_movetext)		(struct cterminal *,int,int,int,int,int,int);
	void	(*ciolib_clreol)		(struct cterminal *);
	void	(*ciolib_clrscr)		(struct cterminal *);
	void	(*ciolib_setvideoflags)	(struct cterminal *,int flags);
	int		(*ciolib_getvideoflags)	(struct cterminal *);
	void	(*ciolib_setscaling)	(struct cterminal *,int new_value);
	int		(*ciolib_getscaling)	(struct cterminal *);
	int		(*ciolib_putch)			(struct cterminal *,int);
	int		(*ciolib_puttext)		(struct cterminal *,int,int,int,int,void *);
	void	(*ciolib_window)		(struct cterminal *,int,int,int,int);
	int		(*ciolib_cputs)			(struct cterminal *,char *);
	int		(*ciolib_setfont)		(struct cterminal *,int font, int force, int font_num);
#else
	void	CIOLIBCALL (*ciolib_gotoxy)		(int,int);
	int		CIOLIBCALL (*ciolib_wherex)		(void);
	int		CIOLIBCALL (*ciolib_wherey)		(void);
	int		CIOLIBCALL (*ciolib_gettext)		(int,int,int,int,void *);
	void	CIOLIBCALL (*ciolib_gettextinfo)	(struct text_info *);
	void	CIOLIBCALL (*ciolib_textattr)		(int);
	void	CIOLIBCALL (*ciolib_setcursortype)	(int);
	int		CIOLIBCALL (*ciolib_movetext)		(int,int,int,int,int,int);
	void	CIOLIBCALL (*ciolib_clreol)		(void);
	void	CIOLIBCALL (*ciolib_clrscr)		(void);
	void	CIOLIBCALL (*ciolib_setvideoflags)	(int flags);
	int		CIOLIBCALL (*ciolib_getvideoflags)	(void);
	void	CIOLIBCALL (*ciolib_setscaling)		(int new_value);
	int		CIOLIBCALL (*ciolib_getscaling)		(void);
	int		CIOLIBCALL (*ciolib_putch)			(int);
	int		CIOLIBCALL (*ciolib_puttext)		(int,int,int,int,void *);
	void	CIOLIBCALL (*ciolib_window)		(int,int,int,int);
	int		CIOLIBCALL (*ciolib_cputs)			(char *);
	int		CIOLIBCALL (*ciolib_setfont)		(int font, int force, int font_num);
#endif
	int 	*_wscroll;
	int		*puttext_can_move;
	int		*hold_update;
	void	*extra;
};

#ifdef __cplusplus
extern "C" {
#endif

CIOLIBEXPORT struct cterminal* CIOLIBCALL cterm_init(int height, int width, int xpos, int ypos, int backlines, unsigned char *scrollback, int emulation);
CIOLIBEXPORT char* CIOLIBCALL cterm_write(struct cterminal *cterm, const void *buf, int buflen, char *retbuf, size_t retsize, int *speed);
CIOLIBEXPORT int CIOLIBCALL cterm_openlog(struct cterminal *cterm, char *logfile, int logtype);
CIOLIBEXPORT void CIOLIBCALL cterm_closelog(struct cterminal *cterm);
CIOLIBEXPORT void CIOLIBCALL cterm_end(struct cterminal *cterm);
CIOLIBEXPORT void CIOLIBCALL cterm_clearscreen(struct cterminal *cterm, char attr);
CIOLIBEXPORT void CIOLIBCALL cterm_start(struct cterminal *cterm);
#ifdef __cplusplus
}
#endif

#endif
