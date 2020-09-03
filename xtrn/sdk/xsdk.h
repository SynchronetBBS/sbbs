/* xsdk.h */

/* Synchronet XSDK function prototypes */

/* $Id: xsdk.h,v 1.24 2009/01/01 12:45:59 deuce Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout xtrn	*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/


#ifndef _XSDK_H
#define _XSDK_H

/*********************************************/
/* Standard Borland/Turbo C/C++ Header Files */
/*********************************************/
#ifdef __unix__
 #ifdef __solaris__
	#include <sys/filio.h>	/* FIONREAD on SunOS */
 #endif
#else
	#include <io.h>
	#include <share.h>
	#include <conio.h>
#endif
#ifdef __16BIT__
	#include <dos.h>
#endif
#ifdef _WIN32
	#include <windows.h>
#endif
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#ifndef __unix__
	#include <malloc.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>

#ifdef USE_XPDEV
#include "semwrap.h"
#include "filewrap.h"
#define	BACKSLASH	PATH_DELIM
#define mswait(x)	SLEEP(x)
#define COMPILER_DESC(x)	DESCRIBE_COMPILER(x)
#endif

#define GLOBAL extern	/* turns XSDKVARS.C into a header file */
#include "xsdkinet.h"
#ifndef USE_XPDEV
#include "xsdkwrap.h"
#endif
#include "xsdkvars.c"

#ifdef __cplusplus
	extern "C" {
#endif

extern char *xsdk_ver;	/* XSDK version number */
extern ulong xsdk_mode;	/* XSDK mode bits */
extern int mswtyp;		/* MSwait type */

/***********************/
/* Function Prototypes */
/***********************/

/* Initialize Data
	- Reads XTRN.DAT and initializes variables */
void initdata(void);

/* Get Terminal Type
	- Detects RIP and WIP terminal compatibility */
void get_term(void);

/* BBS Print String
	- Displays a string locally and remotely (with Ctrl-A codes) */
void bputs(char *str);

/* Raw Print String
	- Oututs a string locally and remotely (verbatim) */
void rputs(char *str);

/* BBS Print Formatted
	- Displays a formatted string locally and remotely (with Ctrl-A codes)
	- printf() equivalent */
int  bprintf(char *fmt, ...);

/* Raw Print Formated
	- Displays a formatted string locally and remotely (verbatim)
	- printf() equivalent */
int  rprintf(char *fmt, ...);

/* BBS String Length
	- Returns length of string, excluding Ctrl-A codes */
int  bstrlen(char *str);

/* Output character
	- Displays a single character */
void outchar(char ch);

/* Mnemonics
	- Display a string expanding ~letter combinations to command keys
	- Automatically colorizes mnemonic letters or places them in parenthesis
	  for non-ANSI users */
void mnemonics(char *str);

/* Pause prompt
	- Displays [Hit a key] and waits for the user to hit a key */
void bpause(void);

/* Yes/no Question
	- Displays a string with (Y/n) ? appended and waits for the user to hit
	  'Y', 'N' or enter
	- Returns 1 if the user hit 'Y' or enter
	- Automatic colorization */
char yesno(char *str);

/* No/yes Question
	- Displays a string with (y/N) ? appended and waits for the user to hit
	  'Y', 'N' or enter
	- Returns 1 if the user hit 'N' or enter */
char noyes(char *str);

/* Inbound Keystroke
	- If the local or remote user has struck a key, this function returns the
	  key, otherwise it returns 0
	- Does not wait for a keystroke */
char inkey(long mode);

/* Key hit?
	- Returns non-zero if key hit, 0 otherwise */
int keyhit(void);

/* Get a Key
	- Waits for the local or remote user to hit a valid key
	- See K_* constants in XSDKDEFS.H for possible values of mode */
char getkey(long mode);

/* Get One of these Keys or a Number
	- Waits for the user to hit a valid key or if max is non-zero, then enter
	  a number not greater than max
	- Hot-keyed input, automatically displays the struck key (in upper case)
	  followed by CRLF
	- If the user entered a number, the number is ORed with 0x8000 and returned
	  you must XOR (or not) this bit to get the correct number */
int  getkeys(char *str, int max);

/* Get a Number
	- Waits for the user to enter a number from 0 to max, 'Q' or ENTER
	- Returns -1 if the user hit 'Q' or Ctrl-C
	- Returns 0 if the user just hit ENTER */
int  getnum(int max);

/* Change Attribute
	- Sends ANSI escape sequences (if user supports ANSI) to change color
	- Valid color bits are defined in INCLUDE\CONIO.H */
void attr(int atr);

/* Clear Screen
	- Clears local and remote screen (using ANSI if appropriate)
	- Automatically pauses before clearing if lncntr is >1 */
void cls(void);

/* Process Ctrl-A Code
	- Changes color or performs special Ctrl-A function */
void ctrl_a(char x);

/* Network Open
	- Opens a file in DENYNONE or DENYWRITE mode, automatically retrying */
int  nopen(const char *str, int access);

/* Truncate Space
	- Removes white space characters from the end of a string */
#ifdef USE_XPDEV
#include <genwrap.h>
#else
void truncsp(uchar *str);
#endif

/* Adds Backslash
	- Adds backslash to end of string if it doesn't exist */
#ifdef USE_XPDEV
#include <dirwrap.h>
#else
void backslash(char *str);
#endif

/* Check Time Left
	- Checks the amount of time the user has left and sets the timeleft
	  variable
	- Automatically exits if user runs out of time */
void checktimeleft(void);

/* Print File
	- Displays contents of file (expanding Ctrl-A characters if appropriate) */
void printfile(char *str);

/* Get String
	- Waits for the user to enter a string
	- maxlen is the maximum length of the string
	- See K_* constants in XSDKDEFS.H for possible values of mode */
int  getstr(char *str, size_t maxlen, long mode);

/* Redraw String
	- Redisplays a string, mainly called by getstr() */
void redrwstr(char *strin, int i, int l, long mode);

/* Strip Invalid Ctrl-A codes */
char stripattr(char *strin);

/* Returns the name of the user number passed */
char *xsdk_username(uint usernumber);
#define username(x)	xsdk_username(x)

/* Returns the number of the user name passed */
uint usernumber(char *username);

/* Convert unsigned long to an ASCII string with commas */
char *ultoac(ulong l, char *string);

/* Convert an ASCII string of hex digits into an unsigned long */
ulong ahtoul(char *str);

/* Display status of node */
void xsdk_printnodedat(int number, node_t node);
#define printnodedat(a,b)	xsdk_printnodedat(a,b)

/* Checks to see if this node has been interrupted or any messages waiting */
void nodesync(void);

/* Writes the node information to disk */
void xsdk_putnodedat(int number, node_t node);
#define putnodedat(a,b)	xsdk_putnodedat(a,b)

/* Reads the node information from disk */
void xsdk_getnodedat(int number, node_t *node, char lockit);
#define getnodedat(a,b,c)	xsdk_getnodedat(a,b,c)

/* Writes a short message (telegram) for specified user number */
void xsdk_putsmsg(int usernumber, char *strin);
#define putsmsg(a,b)	xsdk_putsmsg(a,b)

/* Reads and displays short message for specified user number */
void xsdk_getsmsg(int usernumber);
#define getsmsg(a)	xsdk_getsmsg(a)

/* Writes a node message for specified node */
void xsdk_putnmsg(int num, char *strin);
#define putnmsg(a,b)	xsdk_putnmsg(a,b)

/* Reads and displays node message for current node */
void xsdk_getnmsg(void);
#define getnmsg()	xsdk_getnmsg()

/* Displays users online */
int whos_online(char listself);

/* Send a node-message (ctrl-P) */
void nodemsg(void);

/* Put a key into the keyboard buffer */
void ungetkey(char ch);

/* Check to see if the user has hung-up */
int isconnected(void);
void checkline(void);

/* Display a line (with ctrl-A codes) centered on the screen */
void center(char *str);

#ifdef _MSC_VER
int lock(int file, long offset, long size);
int unlock(int file, long offset, long size);
#endif

#ifdef __cplusplus
	}
#endif


#endif	/* Don't add anything after this #endif */
