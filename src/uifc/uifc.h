/* Text-mode User Interface Library (inspired by Novell SYSCON look & feel) */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _UIFC_H_
#define _UIFC_H_

#include "gen_defs.h"   // int64_t
#include "ciolib.h"     // enum text_modes

#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
/* OS Specific */
#if defined(_WIN32)
	#include <windows.h>
#endif
#if defined(__unix__)
	#include <sys/param.h>  /* PATH_MAX */
#endif

#ifdef _WIN32
		#if defined(UIFC_IMPORTS) || defined(UIFC_EXPORTS)
				#if defined(UIFC_IMPORTS)
						#define UIFCEXPORT __declspec(dllimport)
						#define UIFCEXPORTVAR __declspec(dllimport)
				#else
						#define UIFCEXPORT __declspec(dllexport)
						#define UIFCEXPORTVAR __declspec(dllexport)
				#endif
		#else   /* self-contained executable */
				#define UIFCEXPORT
				#define UIFCEXPORTVAR   extern
		#endif
#else
		#define UIFCEXPORT
		#define UIFCEXPORTVAR   extern
#endif

#if defined(__unix__) && !defined(stricmp)
	#define stricmp strcasecmp
	#define strnicmp strncasecmp
#endif

#if !defined(FREE_AND_NULL)
	#define FREE_AND_NULL(x)            if (x != NULL) { free(x); x = NULL; }
#endif

#if !defined(MAX_PATH)  /* maximum path length */
	#if defined MAXPATHLEN
		#define MAX_PATH MAXPATHLEN /* clib.h */
	#elif defined PATH_MAX
		#define MAX_PATH PATH_MAX
	#elif defined _MAX_PATH
		#define MAX_PATH _MAX_PATH
	#else
		#define MAX_PATH 260
	#endif
#endif

#define MAX_OPTS            100000
#define MSK_ON              0xf0000000
#define MSK_OFF             0x0fffffff
#define MSK_INS             0x10000000
#define MSK_DEL             0x20000000
#define MSK_COPY            0x30000000
#define MSK_CUT             0x40000000
#define MSK_PASTE           0x50000000  /* Overwrite selected item with previously copied item */
#define MSK_EDIT            0x60000000
#define MSK_TAG             0x70000000

#define MSK_TAGALL          (MSK_TAG | MSK_OFF)

/* Legacy terms (get/put instead of copy/paste) */
#define MSK_GET     MSK_COPY
#define MSK_PUT     MSK_PASTE

/* Don't forget, negative return values are used for extended keys (if WIN_EXTKEYS used)! */
#define MAX_OPLN    75      /* Maximum length of each option per menu call */
#define MAX_BUFS    7       /* Maximum number of screen buffers to save */
#define MIN_LINES   14      /* Minimum number of screen lines supported */
#define MAX_LINES   255     /* Maximum number of screen lines supported (ciolib screenheight is a uchar) */
#define MAX_COLS    255     /* Maximum number of screen columns supported (ciolib screenwidth is a uchar) */
#define MAX_BFLN    (MAX_COLS * MAX_LINES * 2)  /* Maximum size of screen buffers, char + attr */

#ifndef uint
#define uint unsigned int
#endif

/**************************/
/* Bits in uifcapi_t.mode */
/**************************/
#define UIFC_INMSG      (1 << 0)  /* Currently in Message Routine non-recursive */
#define UIFC_MOUSE      (1 << 1)  /* Mouse installed and available */
#define UIFC_MONO       (1 << 2)  /* Force monochrome mode */
#define UIFC_COLOR      (1 << 3)  /* Force color mode */
#define UIFC_IBM        (1 << 4)  /* Force use of IBM charset	*/
#define UIFC_NOCTRL     (1 << 5)  /* Don't allow usage of CTRL keys for movement
	                             * etc in menus (Still available in text boxes) */
#define UIFC_NHM        (1 << 6)  /* Don't hide the mouse pointer */
#define UIFC_NOMOUSE    (1 << 7)  /* Don't enable/use the mouse */

/*******************************/
/* Bits in uifcapi_t.list mode */
/*******************************/
#define WIN_ORG     (1LL << 0)    /* Original menu - destroy valid screen area */
#define WIN_SAV     (1LL << 1)    /* Save existing text and replace when finished */
#define WIN_ACT     (1LL << 2)    /* Menu remains active after a selection */
#define WIN_L2R     (1LL << 3)    /* Center the window based on 'width'   */
#define WIN_T2B     (1LL << 4)    /* Center the window based on 'height'  */
#define WIN_INS     (1LL << 5)    /* Allows user to use insert key */
#define WIN_INSACT  (1LL << 6)    /* Remains active after insert key */
#define WIN_DEL     (1LL << 7)    /* Allows user to use delete key */
#define WIN_DELACT  (1LL << 8)    /* Remains active after delete key */
#define WIN_ESC     (1LL << 9)    /* Screen is active when escape is hit			 */
#define WIN_RHT     (1LL << 10) /* Place window against right side of screen */
#define WIN_BOT     (1LL << 11) /* Place window against bottom of screen */
#define WIN_COPY    (1LL << 12) /* Allows F5 to Get (copy) a menu item */
#define WIN_PASTE   (1LL << 13) /* Allows F6 to Put (paste) a menu item */
#define WIN_CHE     (1LL << 14) /* Stay active after escape if changes */
#define WIN_XTR     (1LL << 15) /* Add extra line at end for inserting at end */
#define WIN_DYN     (1LL << 16) /* Dynamic window - return at least every second */
#define WIN_CUT     (1LL << 17)   /* Allow ^X (cut) a menu item */
#define WIN_HLP     (1LL << 17) /* Parse 'Help codes' - showbuf() */
#define WIN_TAG     (1LL << 18)   /* Allow space bar to toggle tagged options */
#define WIN_PACK    (1LL << 18) /* Pack text in window (No padding) - showbuf() */
#define WIN_IMM     (1LL << 19) /* Draw window and return immediately */
#define WIN_FAT     (1LL << 20)   /* Do not pad outside borders */
#define WIN_REDRAW  (1LL << 21) /* Force redraw on dynamic window */
#define WIN_NODRAW  (1LL << 22) /* Force not to redraw on dynamic window */
#define WIN_EXTKEYS (1LL << 23) /* Return on any keypress... if it's not handled internally
	                         * Return value is -2 - keyvalue */
#define WIN_NOBRDR  (1LL << 24)   /* Do not draw a border around the window */
#define WIN_FIXEDHEIGHT (1LL << 25)   /* Use list_height from uifc struct */
#define WIN_UNGETMOUSE  (1LL << 26) /* If the mouse is clicked outside the window, */
                                    /* Put the mouse event back into the event queue */
#define WIN_EDIT        (1LL << 27)   /* Allow F2 to edit a menu item */
#define WIN_PASTEXTR    (1LL << 28)   /* Allow paste into extra (blank) item */
#define WIN_INACT       (1LL << 29)   /* Draw window inactive... intended for use with WIN_IMM */
#define WIN_POP         (1LL << 30)   /* Exit the list. Act as though ESC was pressed. */
/* Intended for use after a WIN_EXTKEYS or WIN_DYN */
#define WIN_SEL         (1LL << 31)   /* Exit the list. Act as though ENTER was pressed. */
/* Intended for use after a WIN_EXTKEYS or WIN_DYN */
#define WIN_LEFTKEY     (1LL << 32)   /* Display left-arrow-key support indicator */
#define WIN_RIGHTKEY    (1LL << 33)   /* Display right-arrow-key support indicator */
#define WIN_BLANKOPTS   (1LL << 34)   /* Blank options are allowed (terminate list on NULL only) */
#define WIN_ATEXIT      (1LL << 35)   /* Do not exit this because UIFC_XF_QUIT is set - for non-exiting menus */

#define WIN_MID WIN_L2R | WIN_T2B  /* Place window in middle of screen */
#define WIN_GET WIN_COPY
#define WIN_PUT WIN_PASTE

typedef int64_t uifc_winmode_t;

#define SCRN_TOP    3
#define SCRN_LEFT   (api->scrn_width < 80 ? 1 : 5)
#define SCRN_RIGHT  ((int)api->scrn_width - (api->scrn_width < 80 ? 0 : 4))

/* Bits in 'mode' for getkey and getstr     */
#define K_UPPER     (1 << 0)  /* Converts all letters to upper case		*/
#define K_UPRLWR    (1 << 1)  /* Upper/Lower case automatically			*/
#define K_NUMBER    (1 << 2)  /* Allow numbers only						*/
#define K_WORDWRAP  (1 << 3)  /* Allows word wrap 						*/
#define K_MSG       (1 << 4)  /* Allows ANSI, ^N ^A ^G					*/
#define K_SPIN      (1 << 5)  /* Spinning cursor (same as SPIN)			*/
#define K_LINE      (1 << 6)  /* Input line (inverse color)				*/
#define K_EDIT      (1 << 7)  /* Edit string passed						*/
#define K_CHAT      (1 << 8)  /* In chat multi-chat						*/
#define K_NOCRLF    (1 << 9)  /* Don't print CRLF after string input      */
#define K_ALPHA     (1 << 10) /* Only allow alphabetic characters 		*/
#define K_SCANNING  (1 << 11) /* UPC Scanner is active... return on '%'	*/
#define K_TABEXIT   (1 << 12) /* Return on TAB or BACKTAB			    	*/
#define K_DECIMAL   (1 << 13) /* Allow floating point numbers only		*/
#define K_DEUCEEXIT (1 << 14) /* Return whenever Deuce wants to exit		*/
                              /* Returns on up/down/F2					*/
#define K_MOUSEEXIT (1 << 15) /* Returns when mouse is clicked outside of */
                              /* Input area (NOT outside of window!)		*/
                              /* And ungets the mouse event.				*/
#define K_PASSWORD  (1 << 16) /* Does not display text while editing		*/
#define K_FIND      (1 << 17) /* Don't set the "changes" flag				*/
#define K_TRIM      (1 << 23) /* Don't allow leading or trailing wsp		*/
#define K_NOSPACE   (1 << 26) /* Don't allow any wsp chars				*/
#define K_SPACE     (1 << 27) // Allow wsp chars (defeat K_TRIM and K_NOSPACE)
#define K_CHANGED   (1 << 28) /* Return -1 if string was not changed (w/K_EDIT) */
#define K_NEGATIVE  (1 << 29) /* Allow negative K_NUMERIC and K_DECIMAL values */


/* Extra exit flags */
#define UIFC_XF_QUIT    (1 << 0)  /* Returned -1 due to CIO_KEY_QUIT */

#define HELPBUF_SIZE 4000

#ifndef _GEN_DEFS_H
/* Control characters */
#define STX     0x02                /* Start of text			^B	*/
#define ETX     0x03                /* End of text				^C	*/
#define BS      '\b'                /* Back space				^H	*/
#define TAB     '\t'                /* Horizontal tabulation	^I	*/
#define LF      '\n'                /* Line feed				^J	*/
#define FF      0x0c                /* Form feed				^L	*/
#define CR      '\r'                /* Carriage return			^M	*/
#define ESC     0x1b                /* Escape					^[	*/
#define DEL     0x7f                /* Delete                   ^BS */

enum {
	CTRL_A = 1
	, CTRL_B
	, CTRL_C
	, CTRL_D
	, CTRL_E
	, CTRL_F
	, CTRL_G
	, CTRL_H
	, CTRL_I
	, CTRL_J
	, CTRL_K
	, CTRL_L
	, CTRL_M
	, CTRL_N
	, CTRL_O
	, CTRL_P
	, CTRL_Q
	, CTRL_R
	, CTRL_S
	, CTRL_T
	, CTRL_U
	, CTRL_V
	, CTRL_W
	, CTRL_X
	, CTRL_Y
	, CTRL_Z
};

#endif

#ifndef uchar               /* Short-hand for unsigned data types */
#define uchar unsigned char
#endif
#ifndef uint
#define uint unsigned int
#endif

#ifndef BOOL
#define BOOL    int
#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif
#endif

typedef struct {
	int left, top, right, bot;
	int *cur, *bar;
	void* buf;
} win_t;

typedef struct {
	char background;
	char help_char;
	char close_char;
	char up_arrow;
	char down_arrow;
	char left_arrow;
	char right_arrow;
	char button_left;
	char button_right;

	char list_top_left;
	char list_top;
	char list_top_right;
	char list_separator_left;
	char list_separator_right;
	char list_horizontal_separator;
	char list_left;
	char list_right;
	char list_bottom_left;
	char list_bottom_right;
	char list_bottom;
	char list_scrollbar_separator;

	char input_top_left;
	char input_top;
	char input_top_right;
	char input_left;
	char input_right;
	char input_bottom_left;
	char input_bottom_right;
	char input_bottom;

	char popup_top_left;
	char popup_top;
	char popup_top_right;
	char popup_left;
	char popup_right;
	char popup_bottom_left;
	char popup_bottom_right;
	char popup_bottom;

	char help_top_left;
	char help_top;
	char help_top_right;
	char help_left;
	char help_right;
	char help_bottom_left;
	char help_bottom_right;
	char help_bottom;
	char help_titlebreak_left;
	char help_titlebreak_right;
	char help_hitanykey_left;
	char help_hitanykey_right;
} uifc_graphics_t;

typedef struct {
/****************************************************************************/
/* Size of the structure (for version compatibility verification).			*/
/****************************************************************************/
	size_t size;
/****************************************************************************/
/* Controls general UIFC library behavior.									*/
/****************************************************************************/
	int mode;
/****************************************************************************/
/* Common input mode flags (K_*) to always set.								*/
/****************************************************************************/
	int input_mode;
/****************************************************************************/
/* Set to TRUE when changes to data have been made by input function.		*/
/****************************************************************************/
	BOOL changes;
/****************************************************************************/
/* Set to TRUE to enable insert mode by default (not overwrite)				*/
/****************************************************************************/
	BOOL insert_mode;
/****************************************************************************/
/* Set to TRUE to reverse the insert/overwrite cursor choice				*/
/****************************************************************************/
	BOOL reverse_cursor;
/****************************************************************************/
/* The overlapped-window save buffer number.								*/
/****************************************************************************/
	uint savnum;
/****************************************************************************/
/* Screen length															*/
/****************************************************************************/
	uint scrn_len;
/****************************************************************************/
/* Screen Width 															*/
/****************************************************************************/
	uint scrn_width;
/****************************************************************************/
/* ESC key delay for curses													*/
/****************************************************************************/
	uint esc_delay;
/****************************************************************************/
/* Alternative method of setting current help text.							*/
/****************************************************************************/
	char* helpbuf;
/****************************************************************************/
/* Location of the help data and index files.								*/
/****************************************************************************/
	char helpdatfile[MAX_PATH + 1];
	char helpixbfile[MAX_PATH + 1];
	BOOL help_available;
/****************************************************************************/
/* Help and exit button locations for current/last window					*/
/****************************************************************************/
	int buttony;
	int exitstart;
	int exitend;
	int helpstart;
	int helpend;
/****************************************************************************/
/* List height for WIN_FIXEDHEIGHT lists.									*/
/****************************************************************************/
	int list_height;

/****************************************************************************/
/* Colours for the various bits												*/
/****************************************************************************/
	uchar hclr, lclr, bclr, cclr, lbclr;

/****************************************************************************/
/* Have we initialized successfully?										*/
/****************************************************************************/
	BOOL initialized;

/****************************************************************************/
/* UIFC_XF_* bits for extra exit status										*/
/****************************************************************************/
	uint exit_flags;

/****************************************************************************/
/* Custom drawing characters												*/
/****************************************************************************/
	uifc_graphics_t *chars;

/****************************************************************************/
/* Allow application override												*/
/****************************************************************************/
	char** yesNoOpts;

/****************************************************************************/
/* Exit/uninitialize function.												*/
/****************************************************************************/
	void (*bail) (void);
/****************************************************************************/
/* Fill the screen with the appropriate background attribute.				*/
/* str is the title for the application banner.								*/
/* Returns 0 on success, non-zero on failure.								*/
/****************************************************************************/
	int (*scrn) (const char* str);
/****************************************************************************/
/* Popup a message, maybe wait for the user to hit a key or click button.	*/
/****************************************************************************/
	int (*msg)  (const char* str);
	int (*msgf) (const char* fmt, ...);
	BOOL (*deny) (const char* fmt, ...);
	BOOL (*confirm) (const char* fmt, ...);
/****************************************************************************/
/* Popup/down a status message.												*/
/* str is the message to display on popup.									*/
/* if str==NULL, then the the status is to be cleared (popdown).			*/
/****************************************************************************/
	void (*pop)  (const char* str, ...);
/****************************************************************************/
/* General menu function.													*/
/* mode contains WIN_* flags to control display and functionality.			*/
/* left, top and width specify desired screen locations and window size.	*/
/* cur is a pointer to the current (default) option.						*/
/* bar is a pointer to the current location of the lightbar (which used).	*/
/* title is the caption for the menu.										*/
/* Menus can centered left to right and top to bottom automatically.		*/
/* mode bits are set with macros WIN_*.										*/
/* option is an array of char arrays, first element of last char array		*/
/* must be NULL.															*/
/* Returns the 0-based selected option number, -1 for ESC, or the selected	*/
/* option number OR'd with MSK_INS, MSK_DEL, MSK_GET, MSK_PUT, or MSK_EDIT.	*/
/****************************************************************************/
	int (*list) (uifc_winmode_t, int left, int top, int width, int* dflt
	             , int* bar, const char *title, char** option);
/****************************************************************************/
/* Restore last saved window buffer (using WIN_SAV mode)					*/
/* Decrements save_num if non-zero.											*/
/* Can be used to return from a nested-list (with WIN_SAV) without ESC key.	*/
/****************************************************************************/
	BOOL (*restore)(void);
/****************************************************************************/
/* Windowed string input routine.											*/
/* mode contains WIN_* flags to control display and functionality.			*/
/* left and top specify desired screen location.							*/
/* prompt is displayed before the input is requested.						*/
/* str is the string to input or edit.										*/
/* len is the maximum length of the string.									*/
/* kmode contains flags that control the string input (K_* macros).			*/
/* This function sets uifcapi_t.changes to TRUE if the string is modified.	*/
/* Returns the length of the string or -1 on escape/abort.					*/
/****************************************************************************/
	int (*input)(uifc_winmode_t, int left, int top, const char* prompt, char* str
	             , int len, int kmode);
/****************************************************************************/
/* Sets the current help index by source code file and line number.			*/
/****************************************************************************/
	void (*sethelp)(int line, char* file);

/****************************************************************************/
/* Shows the current help text												*/
/****************************************************************************/
	void (*showhelp)(void);

/****************************************************************************/
/* Shows a scrollable text buffer - optionally parsing "help markup codes"	*/
/****************************************************************************/
	void (*showbuf)(uifc_winmode_t, int left, int top, int width, int height
	                , const char *title, const char *hbuf, int *curp, int *barp);

/****************************************************************************/
/* Updates time in upper left corner of screen with current time in ASCII/  */
/* Unix format																*/
/****************************************************************************/
	void (*timedisplay)(BOOL force);

/****************************************************************************/
/* Displays the bottom line using the WIN_* mode flags						*/
/****************************************************************************/
	void (*bottomline)(uifc_winmode_t);

/****************************************************************************/
/* String input/exit box at a specified position							*/
/****************************************************************************/
	int (*getstrxy)(int left, int top, int width, char *outstr, int max
	                , long mode, int *lastkey);

/****************************************************************************/
/* Formatted print with attribute											*/
/****************************************************************************/
	int (*printf)(int x, int y, unsigned attr, char *fmat, ...);

} uifcapi_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Return value from uifc_api.list() when uifcYesNoOpts is used */
enum {
	uifcYes = 0,
	uifcNo = 1
};

UIFCEXPORTVAR char* uifcYesNoOpts[];

/****************************************************************************/
/* Initialization routines for each UIFC implementation.					*/
/* Returns 0 on success, non-zero on failure.								*/
/****************************************************************************/
int uifcini(uifcapi_t*);    /* Original implementation based on conio		*/
UIFCEXPORT int uifcinix(uifcapi_t*);    /* Standard I/O implementation		*/
UIFCEXPORT int uifcini32(uifcapi_t*);   /* modern implementation			*/
/****************************************************************************/

void read_uifc_ini(const char* path, uifcapi_t*, int* ciolib_mode, enum text_modes*);

#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line! */
