/* $Id: cterm.h,v 1.64 2020/06/27 00:04:45 deuce Exp $ */

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
#include <link_list.h>
#include <semwrap.h>
#include <stdbool.h>
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
	,CTERM_EMULATION_PRESTEL
	,CTERM_EMULATION_BEEB
	,CTERM_EMULATION_ATARIST_VT52
} cterm_emulation_t;

typedef enum {
	CTERM_MUSIC_SYNCTERM,
	CTERM_MUSIC_BANSI,
	CTERM_MUSIC_ENABLED
} cterm_music_t;

#define CTERM_LOG_MASK	0x7f
#define CTERM_LOG_PAUSED	0x80

#define CTERM_NO_SETFONT_REQUESTED	99

enum prestel_prog_states {
	PRESTEL_PROG_NONE = 0,
	PRESTEL_PROG_1,
	PRESTEL_PROG_1_ESC,
	PRESTEL_PROG_2,
	PRESTEL_PROG_2_ESC,
	PRESTEL_PROG_PROGRAM_BLOCK,
};

#define PRESTEL_MEM_SLOTS 7
#define PRESTEL_MEM_SLOT_SIZE 16

struct cterminal {
	/* conio stuff */
	int	x;		// X position of the left side on the screen
	int	y;		// Y position of the top pn the screen
	int setfont_result;
	int altfont[4];	// The font slots successfully assigned to the 4 alt-font styles/attributes

	/* emulation mode */
	cterm_emulation_t	emulation;
	int					height;			// Height of the terminal buffer
	int					width;			// Width of the terminal buffer
	int					top_margin;
	int					bottom_margin;
	int					left_margin;
	int					right_margin;
	int					quiet;			// No sounds are made
	struct vmem_cell	*scrollback;
	int					backfilled;		// Number of lines copied into scrollback
	int					backlines;		// Number of lines in scrollback
	int					backwidth;		// Number of columns in scrollback
	char				DA[1024];		// Device Attributes
#define	CTERM_SAVEMODE_AUTOWRAP			0x001
#define CTERM_SAVEMODE_CURSOR			0x002
#define	CTERM_SAVEMODE_ALTCHARS			0x004
#define CTERM_SAVEMODE_NOBRIGHT			0x008
#define CTERM_SAVEMODE_BGBRIGHT			0x010
#define CTERM_SAVEMODE_SIXEL_SCROLL		0x020
#define CTERM_SAVEMODE_ORIGIN			0x040
#define	CTERM_SAVEMODE_BLINKALTCHARS		0x080
#define CTERM_SAVEMODE_NOBLINK			0x100
#define CTERM_SAVEMODE_MOUSE_X10		0x200
#define CTERM_SAVEMODE_MOUSE_NORMAL		0x400
#define CTERM_SAVEMODE_MOUSE_HIGHLIGHT		0x500
#define CTERM_SAVEMODE_MOUSE_BUTTONTRACK	0x1000
#define CTERM_SAVEMODE_MOUSE_ANY		0x2000
#define CTERM_SAVEMODE_MOUSE_FOCUS		0x4000
#define CTERM_SAVEMODE_MOUSE_UTF8		0x8000
#define CTERM_SAVEMODE_MOUSE_SGR		0x10000
#define CTERM_SAVEMODE_MOUSE_ALTSCROLL		0x20000
#define CTERM_SAVEMODE_MOUSE_URXVT		0x40000
#define CTERM_SAVEMODE_DECLRMM			0x80000
#define CTERM_SAVEMODE_DECBKM                   0x100000
	int32_t				saved_mode;
	int32_t				saved_mode_mask;

	/* emulation state */
	int					started;		// Indicates that conio functions are being called
	bool					c64reversemode;	// Commodore 64 reverse mode state
	bool negative;
	unsigned char		attr;			// Current attribute
	uint32_t			fg_color;
	uint32_t			bg_color;
	unsigned int		extattr;		// Extended attributes
#define CTERM_EXTATTR_AUTOWRAP		0x0001
#define CTERM_EXTATTR_ORIGINMODE	0x0002
#define CTERM_EXTATTR_SXSCROLL		0x0004
#define CTERM_EXTATTR_DECLRMM		0x0008
#define CTERM_EXTATTR_BRACKETPASTE      0x0010
#define CTERM_EXTATTR_DECBKM            0x0020
#define CTERM_EXTATTR_PRESTEL_MOSAIC	0x0040
#define CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT 0x0080
#define CTERM_EXTATTR_PRESTEL_CONCEAL	0x0100
#define CTERM_EXTATTR_PRESTEL_SEPARATED	0x0200
#define CTERM_EXTATTR_PRESTEL_HOLD	0x0400
#define CTERM_EXTATTR_ALTERNATE_KEYPAD	0x0800
	int					save_xpos;		// Saved position (for later restore)
	int					save_ypos;
	int					sequence;		// An escape sequence is being parsed
	int					string;
#define CTERM_STRING_APC	1
#define CTERM_STRING_DCS	2
#define CTERM_STRING_OSC	3
#define CTERM_STRING_PM		4
#define CTERM_STRING_SOS	5
	char				*strbuf;
	size_t				strbuflen;
	size_t				strbufsize;
	size_t				escbufsz;
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
	int					backpos; // Position where new lines will be added
	int					backstart; // First line of scrollback
	int					xpos;
	int					ypos;
	cterm_log_t			log;
	FILE*				logfile;
	char				fontbuf[4097];	// Remote font
	int					font_read;		// Current position in fontbuf
	int					font_slot;
	int					font_size;		// Bytes
	int					doorway_mode;
	int					doorway_char;	// Indicates next char is a "doorway" mode char
	int					cursor;			// Current cursor mode (Normal or None)
	char				*fg_tc_str;
	char				*bg_tc_str;
	int					*tabs;
	int					tab_count;
	uint32_t last_column_flag;
#define CTERM_LCF_SET 1
#define CTERM_LCF_ENABLED 2
#define CTERM_LCF_FORCED 4

	/* Sixel state */
	int					sixel;			// Sixel status
#define SIXEL_INACTIVE	0
#define SIXEL_POSSIBLE	1
#define SIXEL_STARTED	2
	int					sx_iv;			// Vertical size
	int					sx_ih;			// Horizontal size
	int					sx_trans;		// "Transparent" background
	unsigned long		sx_repeat;		// Repeat count
	unsigned			sx_left;		// Left margin (0-based pixel offset)
	unsigned			sx_x, sx_y;		// Current position
	uint32_t			sx_fg, sx_bg;	// Current colour set
	int					sx_pixels_sent;	/* If any pixels have been sent... 
										   Raster Attributes are ignore if this is true. */
	int					sx_first_pass;	// First pass through a line
	int					sx_hold_update;	// hold_update value to restore on completion
	int					sx_start_x;		// Starting X position
	int					sx_start_y;		// Starting Y position
	int					sx_row_max_x;	// Max right size of this sixel line
	struct ciolib_pixels *sx_pixels;
	unsigned long		sx_width;		// Width from raster attributes
	unsigned long		sx_height;		// REMAINING heigh from raster attributes
	struct ciolib_mask	*sx_mask;
	int					sx_orig_cursor;	// Original value of cterm->cursor

	/* APC Handler */
	void				(*apc_handler)(char *strbuf, size_t strlen, char *retbuf, size_t retsize, void *cbdata);
	void				*apc_handler_data;

	/* Mouse state change callback */
	void (*mouse_state_change)(int parameter, int enable, void *cbdata);
	void *mouse_state_change_cbdata;
	int (*mouse_state_query)(int parameter, void *cbdata);
	void *mouse_state_query_cbdata;

	/* Macros */
	char *macros[64];
	size_t macro_lens[64];
	uint64_t in_macro;
	int macro;
#define MACRO_INACTIVE	0
#define MACRO_POSSIBLE	1
#define MACRO_STARTED	2
	int macro_num;
	int macro_del;
#define MACRO_DELETE_OLD	0
#define MACRO_DELETE_ALL	1
	int macro_encoding;
#define MACRO_ENCODING_ASCII	0
#define MACRO_ENCODING_HEX	1

	/* Alternate font renderer */
	void (*font_render)(char *str);
	int skypix;
	uint8_t prestel_last_mosaic;

	/* Prestel data */
	char prestel_data[PRESTEL_MEM_SLOTS][PRESTEL_MEM_SLOT_SIZE];
	enum prestel_prog_states prestel_prog_state;
	uint8_t prestel_mem;
};

#ifdef __cplusplus
extern "C" {
#endif

CIOLIBEXPORT struct cterminal* cterm_init(int height, int width, int xpos, int ypos, int backlines, int backcols, struct vmem_cell *scrollback, int emulation);
CIOLIBEXPORT size_t cterm_write(struct cterminal *cterm, const void *buf, int buflen, char *retbuf, size_t retsize, int *speed);
CIOLIBEXPORT int cterm_openlog(struct cterminal *cterm, char *logfile, int logtype);
CIOLIBEXPORT void cterm_closelog(struct cterminal *cterm);
CIOLIBEXPORT void cterm_end(struct cterminal *cterm, int free_fonts);
CIOLIBEXPORT void cterm_clearscreen(struct cterminal *cterm, char attr);
CIOLIBEXPORT void cterm_start(struct cterminal *cterm);
void cterm_gotoxy(struct cterminal *cterm, int x, int y);
void setwindow(struct cterminal *cterm);
void cterm_clreol(struct cterminal *cterm);
void cterm_scrollup(struct cterminal *cterm);

#ifdef __cplusplus
}
#endif

#endif
