/* Synchronet User Interface constants */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/* This file must remain in-sync with src/uifc/uifc.h */

const MAX_OPTS	= 10000;
const MSK_ON	= 0xf0000000;
const MSK_OFF 	= 0x0fffffff;
const MSK_INS 	= 0x10000000;
const MSK_DEL 	= 0x20000000;
const MSK_COPY 	= 0x30000000;
const MSK_CUT 	= 0x40000000;
const MSK_PASTE = 0x50000000;
const MSK_EDIT 	= 0x60000000;
const MSK_TAG	= 0x70000000;

const MSK_TAGALL = MSK_TAG | MSK_OFF;

/* Dont forget, negative return values are used for extended keys (if WIN_EXTKEYS used)! */
const MAX_OPLN	= 75;	/* Maximum length of each option per menu call */
const MAX_BUFS	= 7;	/* Maximum number of screen buffers to save */
const MIN_LINES = 14;   /* Minimum number of screen lines supported */
const MAX_LINES = 60;   /* Maximum number of screen rows supported */ 
const MAX_BFLN	= 80*MAX_LINES*2;	/* Maximum size of screen buffers */

								/**************************/
								/* Bits in uifcapi_t.mode */
								/**************************/
const UIFC_INMSG	= (1<<0);	/* Currently in Message Routine non-recursive */
const UIFC_MOUSE	= (1<<1);	/* Mouse installed and available */
const UIFC_MONO		= (1<<2);	/* Force monochrome mode */
const UIFC_COLOR	= (1<<3);	/* Force color mode */
const UIFC_IBM		= (1<<4);	/* Force use of IBM charset	*/
const UIFC_NOCTRL	= (1<<5);	/* Don't allow useage of CTRL keys for movement 
								 *	 etc in menus (Still available in text boxes) */

								/*******************************/
								/* Bits in uifcapi_t.list mode */
								/*******************************/
const WIN_ORG 		= (1<<0);	/* Original menu - destroy valid screen area */
const WIN_SAV 		= (1<<1);	/* Save existing text and replace when finished */
const WIN_ACT 		= (1<<2);	/* Menu remains active after a selection */
const WIN_L2R 		= (1<<3);	/* Center the window based on 'width'   */
const WIN_T2B 		= (1<<4);	/* Center the window based on 'height'  */
const WIN_INS 		= (1<<5);	/* Allows user to user insert key */
const WIN_INSACT	= (1<<6);	/* Remains active after insert key */
const WIN_DEL 		= (1<<7);	/* Allows user to use delete key */
const WIN_DELACT	= (1<<8);	/* Remains active after delete key */
const WIN_ESC 		= (1<<9);	/* Screen is active when escape is hit			 */
const WIN_RHT 		= (1<<10);	/* Place window against right side of screen */
const WIN_BOT 		= (1<<11);	/* Place window against botton of screen */
const WIN_COPY 		= (1<<12);	/* Allows F5 to Get (copy) a menu item */
const WIN_PASTE		= (1<<13);	/* Allows F6 to Put (paste) a menu item */
const WIN_CHE 		= (1<<14);	/* Stay active after escape if changes */
const WIN_XTR 		= (1<<15);	/* Add extra line at end for inserting at end */
const WIN_DYN 		= (1<<16);	/* Dynamic window - return at least every second */
const WIN_CUT		= (1<<17);	/* Allow ^X (cut) a menu item */
const WIN_HLP		= (1<<17);	/* showbuf(): Parse help text codes */
const WIN_TAG		= (1<<18);	/* Allow space bar to toggle tagged options */
const WIN_PACK 		= (1<<18);	/* showbuf(): Pack text in window (No padding) */
const WIN_IMM 		= (1<<19);	/* Draw window and return immediately */
const WIN_FAT		= (1<<20);	/* Do not pad outside borders */
const WIN_REDRAW	= (1<<21);	/* Force redraw on dynamic window */
const WIN_NODRAW	= (1<<22);	/* Force not to redraw on dynamic window */
const WIN_EXTKEYS	= (1<<23);	/* Return on any keypress... if it's not handled internally
								 * Return value is -2 - keyvalue */
const WIN_NOBRDR	= (1<<24);	/* Do not draw a border around the window */
const WIN_FIXEDHEIGHT= (1<<25);	/* Use list_height from uifc struct */
const WIN_UNGETMOUSE= (1<<26);	/* If the mouse is clicked outside the window, */
								/* Put the mouse event back into the event queue */
const WIN_EDIT		= (1<<27);	/* Allow F2 to edit a menu item */
const WIN_PASTEXTR	= (1<<28);	/* Allow paste into extra (blank) item */
const WIN_INACT		= (1<<29);	/* Draw window inactive... intended for use with WIN_IMM */
const WIN_POP		= (1<<30);	/* Exit the list. Act as though ESC was pressed. */
								/* Intended for use after a WIN_EXTKEYS or WIN_DYN */
const WIN_SEL		= (1<<31);	/* Exit the list. Act as though ENTER was pressed. */
								/* Intended for use after a WIN_EXTKEYS or WIN_DYN */
const WIN_MID		= WIN_L2R|WIN_T2B;/* Place window in middle of screen */

const SCRN_TOP		= 3;
const SCRN_LEFT		= 5;

								/********************************************/
								/* Additional kmode constants				*/
								/* load('sbbsdefs.js') for the others		*/
								/********************************************/
const K_SCANNING	= (1<<11);	/* UPC Scanner is active... return on '%'	*/
const K_TABEXIT		= (1<<12);	/* Return on TAB or BACKTAB			    	*/
const K_DECIMAL		= (1<<13);	/* Allow floating point numbers only		*/
const K_DEUCEEXIT	= (1<<14);	/* Return whenever Deuce wants to exit		*/
								/* Returns on up/down/F2					*/
const K_MOUSEEXIT	= (1<<15);	/* Returns when mouse is clicked outside of */
								/* Input area (NOT outside of window!)		*/
								/* And ungets the mouse event.				*/
const K_PASSWORD	= (1<<16);	/* Does not display text while editing		*/
const K_FIND		= (1<<17);	/* Don't set the "changes" flag				*/
								/********************************************/
