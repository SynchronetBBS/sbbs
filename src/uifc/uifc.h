/* uifc.h */

/* Rob Swindell's Text-mode User Interface Library */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#if !defined(__unix__)
    #include <io.h>
	#include <malloc.h>		/* malloc prototype */
#endif
#if (defined(__unix__) || defined(_WIN32)) && !defined(__FLAT__)
    #define __FLAT__
#endif

#if defined(__FLAT__)
	#define far
#endif

#if !defined(__FLAT__)
    #include <bios.h>
#endif

#if defined(__unix__) && !defined(stricmp)
    #define stricmp strcasecmp
	#define strnicmp strncasecmp
#endif

/****************************************************************************/
/* MALLOC/FREE Macros for various compilers and environments				*/
/* MALLOC is used for allocations of 64k or less							*/
/* FREE is used to free buffers allocated with MALLOC						*/
/* LMALLOC is used for allocations of possibly larger than 64k				*/
/* LFREE is used to free buffers allocated with LMALLOC 					*/
/* REALLOC is used to re-size a previously MALLOCed or LMALLOCed buffer 	*/
/****************************************************************************/
#if defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
	#if defined(__TURBOC__)
		#define REALLOC(x,y) farrealloc(x,y)
		#define LMALLOC(x) farmalloc(x)
		#define MALLOC(x) farmalloc(x)
		#define LFREE(x) farfree(x)
		#define FREE(x) farfree(x)
	#elif defined(__WATCOMC__)
		#define REALLOC realloc
		#define LMALLOC(x) halloc(x,1)	/* far heap, but slow */
		#define MALLOC malloc			/* far heap, but 64k max */
		#define LFREE hfree
		#define FREE free
	#else	/* Other 16-bit Compiler */
		#define REALLOC realloc
		#define LMALLOC malloc
		#define MALLOC malloc
		#define LFREE free
		#define FREE free
	#endif
#else		/* 32-bit Compiler or Small Memory Model */
	#define REALLOC realloc
	#define LMALLOC malloc
	#define MALLOC malloc
	#define LFREE free
	#define FREE free
#endif

#ifdef __DPMI32__
	#define INT_86(i,j,k) int386(i,j,k)
#else
	#define INT_86(i,j,k) int86(i,j,k)
#endif

#ifdef __FLAT__
	#define MAX_OPTS	10000
	#define MSK_ON		0xf0000000
	#define MSK_OFF 	0x0fffffff
	#define MSK_INS 	0x10000000
	#define MSK_DEL 	0x20000000
	#define MSK_GET 	0x30000000
	#define MSK_PUT 	0x40000000
#else
	#define MAX_OPTS	500 	/* Maximum number of options per menu call */
	#define MSK_ON		0xf000
	#define MSK_OFF 	0x0fff
	#define MSK_INS 	0x1000
	#define MSK_DEL 	0x2000
	#define MSK_GET 	0x3000
	#define MSK_PUT 	0x4000
#endif
#define MAX_OPLN	75		/* Maximum length of each option per menu call */
#define MAX_BUFS	7		/* Maximum number of screen buffers to save */
#define MIN_LINES   20      /* Minimum number of screen lines supported */
#define MAX_LINES   60      /* Maximum number of screen rows supported */ 
#define MAX_BFLN	80*MAX_LINES*2	/* Maximum size of screen buffers */

#ifndef uint
#define uint unsigned int
#endif

                            /* Bits in uifcapi_t.mode */
#define UIFC_INMSG	(1<<0)	/* Currently in Message Routine non-recursive */
#define UIFC_MOUSE	(1<<1)	/* Mouse installed and available */
#define UIFC_MONO	(1<<2)	/* Force monochrome mode */
#define UIFC_COLOR	(1<<3)	/* Force color mode */
#define UIFC_IBM	(1<<4)	/* Force use of IBM charset	*/

                            /* Bits in uifcapi_t.list mode */
#define WIN_ORG 	(1<<0)	/* Original menu - destroy valid screen area */
#define WIN_SAV 	(1<<1)	/* Save existing text and replace when finished */
#define WIN_ACT 	(1<<2)	/* Menu remains active after a selection */
#define WIN_L2R 	(1<<3)	/* Center the window based on 'width'   */
#define WIN_T2B 	(1<<4)	/* Center the window based on 'height'  */
#define WIN_INS 	(1<<5)	/* Allows user to user insert key */
#define WIN_INSACT	(1<<6)	/* Remains active after insert key */
#define WIN_DEL 	(1<<7)	/* Allows user to use delete key */
#define WIN_DELACT	(1<<8)	/* Remains active after delete key */
#define WIN_ESC 	(1<<9)	/* Screen is active when escape is hit			 */
#define WIN_RHT 	(1<<10) /* Place window against right side of screen */
#define WIN_BOT 	(1<<11) /* Place window against botton of screen */
#define WIN_GET 	(1<<12) /* Allows F5 to Get a menu item */
#define WIN_PUT 	(1<<13) /* Allows F6 to Put a menu item */
#define WIN_CHE 	(1<<14) /* Stay active after escape if changes */
#define WIN_XTR 	(1<<15) /* Add extra line at end for inserting at end */

#define WIN_MID WIN_L2R|WIN_T2B  /* Place window in middle of screen */

#define SCRN_TOP	3
#define SCRN_LEFT	5
#define SCRN_RIGHT  76

								/* Bits in 'mode' for getkey and getstr     */
#define K_UPPER 	(1L<<0) 	/* Converts all letters to upper case		*/
#define K_UPRLWR	(1L<<1) 	/* Upper/Lower case automatically			*/
#define K_NUMBER	(1L<<2) 	/* Allow numbers only						*/
#define K_WRAP		(1L<<3) 	/* Allows word wrap 						*/
#define K_MSG		(1L<<4) 	/* Allows ANSI, ^N ^A ^G					*/
#define K_SPIN		(1L<<5) 	/* Spinning cursor (same as SPIN)			*/
#define K_LINE		(1L<<6) 	/* Input line (inverse color)				*/
#define K_EDIT		(1L<<7) 	/* Edit string passed						*/
#define K_CHAT		(1L<<8) 	/* In chat multi-chat						*/
#define K_NOCRLF	(1L<<9) 	/* Don't print CRLF after string input      */
#define K_ALPHA 	(1L<<10)	/* Only allow alphabetic characters 		*/

#define HELPBUF_SIZE 4000

#ifndef TAB
									/* Control characters */
#define STX 	0x02				/* Start of text			^B	*/
#define ETX 	0x03				/* End of text				^C	*/
#define BS		'\b'				/* Back space				^H	*/
#define TAB 	'\t'				/* Horizontal tabulation	^I	*/
#define LF		'\n'				/* Line feed				^J	*/
#define FF		0x0c				/* Form feed				^L	*/
#define CR		'\r'				/* Carriage return			^M	*/
#define ESC 	0x1b				/* Escape					^[	*/
#define SP      ' '                 /* Space                        */

#endif

#define CLREOL 256

#ifndef uchar				/* Short-hand for unsigned data types */
#define uchar unsigned char
#endif
#ifndef uint
#define uint unsigned int
#endif
#ifndef ulong
#define ulong unsigned long
#endif

#ifndef BOOL
#define BOOL    int
#define TRUE    1
#define FALSE   0
#endif

typedef struct {
	uint    left,top,right,bot;
    uchar   *buf;
} win_t;

#if !defined(__FLAT__)
    /* LCLOLL.ASM */
    int lclini(int);
    void lclxy(int,int);
    int lclwx(void);
    int lclwy(void);
    int lclatr(int);
    void lputc(int);
    long lputs(char far *);
#endif    

#if defined(__OS2__) || !defined(__FLAT__)
void mswait(int msecs);
extern mswtyp;
#endif

typedef struct {
/****************************************************************************/
/* Size of the structure (for version compatibility verification).			*/
/****************************************************************************/
    size_t  size;
/****************************************************************************/
/* Controls general UIFC library behavior.									*/
/****************************************************************************/
    long    mode;
/****************************************************************************/
/* Set to TRUE when changes to data have been made by input function.		*/ 
/****************************************************************************/
    BOOL    changes;
/****************************************************************************/
/* The overlapped-window save buffer number.								*/
/****************************************************************************/
    uint    savnum;
/****************************************************************************/
/* The current overlapped-window save buffer depth.							*/
/****************************************************************************/
    uint    savdepth;
/****************************************************************************/
/* Screen length															*/
/****************************************************************************/
    uint    scrn_len;
/****************************************************************************/
/* ESC key delay for curses													*/
/****************************************************************************/
    uint    esc_delay;
/****************************************************************************/
/* Alternative method of setting current help text.							*/
/****************************************************************************/
    char*   helpbuf;
/****************************************************************************/
/* Location of the help data and index files.								*/
/****************************************************************************/
    char    helpdatfile[256];
    char    helpixbfile[256];
/****************************************************************************/
/* Exit/uninitialize function.												*/
/****************************************************************************/
    void    (*bail) (void);
/****************************************************************************/
/* Fill the screen with the appropriate background attribute.				*/
/* str is the title for the application banner.								*/
/* Returns 0 on success, non-zero on failure.								*/
/****************************************************************************/
    int     (*scrn) (char* str);
/****************************************************************************/
/* Popup a message, maybe wait for the user to hit a key or click button.	*/
/****************************************************************************/
    void    (*msg)  (char* str);
/****************************************************************************/
/* Popup/down a status message.												*/
/* str is the message to display on popup.									*/
/* if str==NULL, then the the status is to be cleared (popdown).			*/
/****************************************************************************/
    void    (*pop)  (char* str);
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
/* option number OR'd with MSK_INS, MSK_DEL, MSK_GET, or MSK_PUT.			*/
/****************************************************************************/
    int     (*list) (int mode, int left, int top, int width, int* dflt
                        ,int* bar, char *title, char** option);
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
    int     (*input)(int mode, int left, int top, char* prompt, char* str
            	        ,int len, int kmode);
/****************************************************************************/
/* Sets the current help index by source code file and line number.			*/
/****************************************************************************/
    void    (*sethelp)(int line, char* file);
} uifcapi_t;

/****************************************************************************/
/* Initialization routines for each UIFC implementation.					*/
/* Returns 0 on success, non-zero on failure.								*/
/****************************************************************************/
int uifcini(uifcapi_t*);	/* Original implementation based on conio		*/
int uifcinix(uifcapi_t*);	/* Standard I/O implementation					*/
int uifcinic(uifcapi_t*);	/* Unix curses implementation (by Deuce)		*/
int uifcinid(uifcapi_t*);	/* Unix libdialog implementation (by Deuce)		*/
/****************************************************************************/
